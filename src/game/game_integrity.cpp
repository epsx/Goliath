#include "game/game_integrity.hpp"

#include "common/qt_path.hpp"
#include "game/crc32_cache.hpp"
#include "game/filesystem_io.hpp"
#include "game/neocd_verification.hpp"
#include "game/sha1_cache.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace goliath {
namespace {

bool canceled(const std::atomic<bool>* cancel) {
    return cancel && cancel->load(std::memory_order_acquire);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

bool safe_relative_path(const fs::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_path())
        return false;
    return std::none_of(path.begin(), path.end(), [](const fs::path& part) {
        return part == fs::path("..");
    });
}

std::optional<fs::path> integrity_file_path(
        const fs::path& mediaRoot,
        const fs::path& mediaPath,
        const fs::path& mediaRelative,
        const MediaHashDetail& hash) {
    fs::path relative = path_from_utf8(hash.file).lexically_normal();
    if (!safe_relative_path(relative))
        return std::nullopt;

    if (hash.role == "CUE descriptor" || relative == mediaRelative)
        return mediaPath;

    if ((hash.role == "Cartridge" || hash.role == "CHD") &&
        relative.filename() == mediaPath.filename()) {
        return mediaPath;
    }

    if (hash.role.starts_with("Track ") || !relative.has_parent_path())
        return mediaPath.parent_path() / relative;

    return mediaRoot / relative;
}

std::optional<std::string> current_digest(
        const MediaHashDetail& hash,
        const fs::path& path,
        std::uintmax_t size,
        Crc32Cache& crc32Cache,
        Sha1Cache& sha1Cache,
        const std::atomic<bool>* cancel) {
    const std::string algorithm = lower(hash.algorithm);
    if (algorithm == "crc-32" || algorithm == "crc32")
        return crc32Cache.file_crc32(path, size, cancel);

    if (algorithm == "sha-1" || algorithm == "sha1") {
        if (hash.role == "CHD") {
            const MameChdHashCatalog emptyCatalog;
            const auto verification = verify_mame_chd(path, emptyCatalog);
            if (!verification.has_value()) return std::nullopt;
            return verification->actual_sha1;
        }
        return sha1Cache.file_sha1(path, size, cancel);
    }

    return std::nullopt;
}

GameIntegrityResult primary_media_problem(
        const fs::path& mediaPath,
        const std::string& detail) {
    GameIntegrityResult result;
    result.status = GameIntegrityStatus::ProblemsFound;
    result.media_path = mediaPath;
    result.problem_files = 1;
    result.detail = detail;
    return result;
}

} // namespace

GameIntegrityResult verify_game_integrity(
        const fs::path& mediaRoot,
        const std::string& media,
        const Rom* rom,
        const fs::path& cacheDirectory,
        const std::atomic<bool>* cancel) {
    GameIntegrityResult result;
    const fs::path mediaRelative = path_from_utf8(media).lexically_normal();
    result.media_path = mediaRoot / mediaRelative;

    if (canceled(cancel)) {
        result.status = GameIntegrityStatus::Canceled;
        result.detail = "Integrity verification was canceled.";
        return result;
    }
    if (!safe_relative_path(mediaRelative)) {
        return primary_media_problem(
            result.media_path,
            "The selected library media path is not a safe relative path.");
    }

    const fs::path mediaIoPath = filesystem_io_path(result.media_path);
    std::error_code mediaError;
    const fs::file_status mediaStatus = fs::status(mediaIoPath, mediaError);
    if (mediaError == std::errc::no_such_file_or_directory ||
        (!mediaError && !fs::exists(mediaStatus))) {
        return primary_media_problem(
            result.media_path,
            "The selected game file is missing. Rescan the library or "
            "reconnect its storage.");
    }
    if (mediaError) {
        return primary_media_problem(
            result.media_path,
            "Goliath could not inspect the selected game file: " +
                mediaError.message());
    }
    if (!fs::is_regular_file(mediaStatus)) {
        return primary_media_problem(
            result.media_path,
            "The selected game path is not a regular file.");
    }

    if (!rom || rom->hashes.empty()) {
        std::ifstream input(mediaIoPath, std::ios::binary);
        if (!input) {
            return primary_media_problem(
                result.media_path,
                "The selected game file exists but cannot be opened for "
                "reading.");
        }
        result.status = GameIntegrityStatus::AvailableWithoutReference;
        result.detail =
            "The selected game file exists and is readable, but the latest "
            "library scan contains no catalog size/hash reference for it. "
            "A rescan with the matching metadata can enable full integrity "
            "verification.";
        return result;
    }

    Crc32Cache crc32Cache =
        Crc32Cache::load(cacheDirectory / "crc32_cache.json");
    Sha1Cache sha1Cache =
        Sha1Cache::load(cacheDirectory / "hash_cache.json");

    std::optional<std::vector<fs::path>> cueTracks;
    if (lower(path_to_utf8(mediaRelative.extension())) == ".cue")
        cueTracks = resolve_cue_track_paths(mediaIoPath);
    std::size_t cueTrackIndex = 0;

    result.hashes.reserve(rom->hashes.size());
    for (const MediaHashDetail& stored : rom->hashes) {
        if (canceled(cancel)) {
            result.status = GameIntegrityStatus::Canceled;
            result.detail = "Integrity verification was canceled.";
            return result;
        }

        MediaHashDetail checked = stored;
        checked.size.reset();
        checked.actual.reset();
        checked.matched = false;
        if (!checked.expected_size.has_value())
            checked.expected_size = stored.size;
        if (!checked.expected.has_value())
            checked.expected = stored.actual;

        std::optional<fs::path> logicalPath;
        if (stored.role.starts_with("Track ") && cueTracks.has_value() &&
            cueTrackIndex < cueTracks->size()) {
            logicalPath = (*cueTracks)[cueTrackIndex];
        } else {
            logicalPath = integrity_file_path(
                mediaRoot, result.media_path, mediaRelative, stored);
        }
        if (stored.role.starts_with("Track ")) ++cueTrackIndex;
        if (!logicalPath.has_value()) {
            checked.actual = "Invalid relative path";
            ++result.problem_files;
            result.hashes.push_back(std::move(checked));
            continue;
        }

        const fs::path ioPath = filesystem_io_path(*logicalPath);
        std::error_code statusError;
        const fs::file_status status = fs::status(ioPath, statusError);
        if (statusError == std::errc::no_such_file_or_directory ||
            (!statusError && !fs::exists(status))) {
            checked.actual = "Missing";
            ++result.problem_files;
            result.hashes.push_back(std::move(checked));
            continue;
        }
        if (statusError || !fs::is_regular_file(status)) {
            checked.actual = statusError
                ? "Inspection failed: " + statusError.message()
                : "Not a regular file";
            ++result.problem_files;
            result.hashes.push_back(std::move(checked));
            continue;
        }

        std::error_code sizeError;
        const std::uintmax_t size = fs::file_size(ioPath, sizeError);
        if (sizeError) {
            checked.actual = "Size check failed: " + sizeError.message();
            ++result.problem_files;
            result.hashes.push_back(std::move(checked));
            continue;
        }
        checked.size = size;

        const std::string algorithm = lower(checked.algorithm);
        if (algorithm != "crc-32" && algorithm != "crc32" &&
            algorithm != "sha-1" && algorithm != "sha1") {
            checked.actual = "Unsupported algorithm";
            ++result.problem_files;
            result.hashes.push_back(std::move(checked));
            continue;
        }

        const auto digest = current_digest(
            checked, ioPath, size, crc32Cache, sha1Cache, cancel);
        if (!digest.has_value()) {
            if (canceled(cancel)) {
                result.status = GameIntegrityStatus::Canceled;
                result.detail = "Integrity verification was canceled.";
                return result;
            }
            checked.actual = "Unreadable or invalid";
            ++result.problem_files;
            result.hashes.push_back(std::move(checked));
            continue;
        }

        checked.actual = lower(*digest);
        const bool sizeMatches =
            !checked.expected_size.has_value() ||
            *checked.expected_size == size;
        const bool digestMatches =
            checked.expected.has_value() &&
            lower(*checked.expected) == *checked.actual;
        checked.matched = sizeMatches && digestMatches;
        if (checked.matched)
            ++result.matched_files;
        else
            ++result.problem_files;
        result.hashes.push_back(std::move(checked));
    }

    // These saves only add or refresh entries that were actually checked;
    // unlike a complete rescan, an individual verification never prunes the
    // cache for other games.
    (void)crc32Cache.save();
    (void)sha1Cache.save();

    if (result.problem_files == 0 && !result.hashes.empty()) {
        result.status = GameIntegrityStatus::Passed;
        result.detail = "Every referenced file matches its expected size "
                        "and content digest.";
    } else {
        result.status = GameIntegrityStatus::ProblemsFound;
        std::ostringstream detail;
        detail << result.problem_files << " of " << result.hashes.size()
               << " referenced file(s) are missing, unreadable, or do not "
                  "match. Rescan after correcting the files to refresh the "
                  "library verification badge.";
        result.detail = detail.str();
    }
    return result;
}

} // namespace goliath
