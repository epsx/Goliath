#include "game/save_data_manager.hpp"

#include "common/filesystem_safety.hpp"
#include "common/paths.hpp"
#include "game/filesystem_io.hpp"
#include "game/game_profile.hpp"
#include "game/jollygood_executable.hpp"
#include "json.hpp"
#include "miniz.h"

#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;

namespace goliath {

namespace {

constexpr int kBackupSchemaVersion = 1;
constexpr std::uintmax_t kMaximumManagedFileBytes = 16U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumSnapshotBytes = 64U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumArchiveBytes = 64U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumManifestBytes = 64U * 1024U;
constexpr std::size_t kMaximumArchiveMembers = 8;

struct SaveFileSpec {
    ManagedSaveKind kind;
    const char* suffix;
    const char* manifest_id;
};

constexpr std::array<SaveFileSpec, 7> kSaveFileSpecs = {{
    {ManagedSaveKind::StateSlot0, ".st0", "state-slot-0"},
    {ManagedSaveKind::StateSlot1, ".st1", "state-slot-1"},
    {ManagedSaveKind::Nvram, ".nv", "nvram"},
    {ManagedSaveKind::CartridgeRam, ".srm", "cartridge-ram"},
    {ManagedSaveKind::MemoryCard, ".mcr", "memory-card"},
    {ManagedSaveKind::CdBackupRam, ".brm", "cd-backup-ram"},
    {ManagedSaveKind::Dips, ".dip", "dips"},
}};

using PayloadMap = std::map<std::string, std::vector<unsigned char>>;

struct ArchiveSnapshot {
    bool success = false;
    std::string error;
    PayloadMap payloads;
    std::size_t file_count = 0;
};

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

bool supported_system(const std::string& system) {
    return system == "neogeo" || system == "neogeocd";
}

const SaveFileSpec* spec_for_kind(ManagedSaveKind kind) {
    for (const SaveFileSpec& spec : kSaveFileSpecs) {
        if (spec.kind == kind) return &spec;
    }
    return nullptr;
}

std::string expected_filename(std::string_view game_name,
                              const SaveFileSpec& spec) {
    return std::string(game_name) + spec.suffix;
}

const SaveFileSpec* spec_for_filename(std::string_view game_name,
                                      std::string_view filename) {
    for (const SaveFileSpec& spec : kSaveFileSpecs) {
        if (filename == expected_filename(game_name, spec)) return &spec;
    }
    return nullptr;
}

bool is_state_kind(ManagedSaveKind kind) {
    return kind == ManagedSaveKind::StateSlot0 ||
           kind == ManagedSaveKind::StateSlot1;
}

bool has_dynamic_core_argument(const std::vector<std::string>& args) {
    for (const std::string& arg : args) {
        if (arg == "-c" || arg == "--core" ||
            (arg.size() > 2 && arg.starts_with("-c")) ||
            arg.starts_with("--core=")) {
            return true;
        }
    }
    return false;
}

bool directory_exists(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(filesystem_io_path(path), ec) && !ec;
}

#if defined(_WIN32)
fs::path configured_executable_directory(const AppPaths& paths) {
    fs::path executable = resolve_jollygood_executable(paths.jollygood_exe);
    if (executable.empty()) executable = paths.jollygood_exe;
    if (!executable.empty() && !executable.parent_path().empty())
        return executable.parent_path();
    return paths.base_dir;
}
#endif

fs::path static_data_root(const AppPaths& paths) {
#if defined(_WIN32)
    // Upstream JGRF_STATIC intentionally ignores XDG paths on Windows and
    // keeps its user data beside the executable.
    return configured_executable_directory(paths);
#else
    // On Unix-like systems JGRF_STATIC still honors XDG_DATA_HOME and uses
    // the statically linked core name as its application directory.
    return paths.data_dir / "geolith";
#endif
}

fs::path effective_backup_root(const AppPaths& paths) {
    if (!paths.save_backup_dir.empty()) return paths.save_backup_dir;
    return paths.data_dir / "goliath" / "save_backups";
}

bool inspect_plain_directory(const fs::path& logical_path,
                             bool* exists,
                             std::string* error) {
    if (exists) *exists = false;
    const fs::path ioPath = filesystem_io_path(logical_path);
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(ioPath, ec);
    if (ec == std::errc::no_such_file_or_directory) return true;
    if (ec) {
        if (error) {
            *error = "Could not inspect backup directory " +
                     logical_path.string() + ": " + ec.message();
        }
        return false;
    }
    if (!fs::exists(status)) return true;
    if (!fs::is_directory(status)) {
        if (error) {
            *error = "Backup path is not a plain directory: " +
                     logical_path.string();
        }
        return false;
    }
    if (exists) *exists = true;
    return true;
}

bool inspect_regular_file(const fs::path& logical_path,
                          bool* exists,
                          std::uintmax_t* size,
                          fs::file_time_type* modified,
                          std::string* error) {
    if (exists) *exists = false;
    const fs::path ioPath = filesystem_io_path(logical_path);
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(ioPath, ec);
    if (ec == std::errc::no_such_file_or_directory) return true;
    if (ec) {
        if (error) {
            *error = "Could not inspect " + logical_path.string() + ": " +
                     ec.message();
        }
        return false;
    }
    if (!fs::exists(status)) return true;
    if (!fs::is_regular_file(status)) {
        if (error) {
            *error = "Managed save path is not a regular file: " +
                     logical_path.string();
        }
        return false;
    }

    const std::uintmax_t fileSize = fs::file_size(ioPath, ec);
    if (ec) {
        if (error) {
            *error = "Could not read the size of " + logical_path.string() +
                     ": " + ec.message();
        }
        return false;
    }
    const fs::file_time_type fileTime = fs::last_write_time(ioPath, ec);
    if (ec) {
        if (error) {
            *error = "Could not read the timestamp of " +
                     logical_path.string() + ": " + ec.message();
        }
        return false;
    }

    if (exists) *exists = true;
    if (size) *size = fileSize;
    if (modified) *modified = fileTime;
    return true;
}

bool read_file_bytes(const fs::path& logical_path,
                     std::uintmax_t maximum_size,
                     std::vector<unsigned char>* bytes,
                     std::string* error) {
    if (!bytes) return false;
    bytes->clear();

    bool exists = false;
    std::uintmax_t size = 0;
    if (!inspect_regular_file(logical_path, &exists, &size, nullptr, error))
        return false;
    if (!exists) {
        if (error) *error = "File no longer exists: " + logical_path.string();
        return false;
    }
    if (size > maximum_size) {
        if (error) {
            *error = "File exceeds the safe size limit: " +
                     logical_path.string();
        }
        return false;
    }

    std::ifstream input(filesystem_io_path(logical_path), std::ios::binary);
    if (!input) {
        if (error) *error = "Could not open file: " + logical_path.string();
        return false;
    }

    bytes->resize(static_cast<std::size_t>(size));
    if (size != 0) {
        input.read(reinterpret_cast<char*>(bytes->data()),
                   static_cast<std::streamsize>(size));
    }
    if (!input || input.peek() != std::ifstream::traits_type::eof()) {
        if (error) *error = "Could not read file completely: " + logical_path.string();
        bytes->clear();
        return false;
    }
    return true;
}

bool ensure_directory(const fs::path& logical_path, std::string* error) {
    std::error_code ec;
    fs::create_directories(filesystem_io_path(logical_path), ec);
    if (!ec) return true;
    if (error) {
        *error = "Could not create directory " + logical_path.string() +
                 ": " + ec.message();
    }
    return false;
}

bool prepare_direct_save_branch(const JgrfSavePaths& paths,
                                ManagedSaveKind kind,
                                bool create,
                                std::string* error) {
    const fs::path& directory = is_state_kind(kind)
        ? paths.state_directory : paths.save_directory;

    std::vector<fs::path> candidates;
    if (paths.layout == JgrfSaveLayout::DynamicCore) {
        // <data>/jollygood and the intermediate state/save directory are
        // JGRF-owned. Inspect them separately so a parent junction cannot be
        // hidden by an ordinary geolith directory inside its external target.
        candidates.push_back(paths.root);
        candidates.push_back(directory.parent_path());
#if !defined(_WIN32)
    } else {
        // On Unix-like static builds the core-specific root lives below the
        // configured data directory and is owned by JGRF. On Windows the
        // static root is the configured executable directory, which remains
        // the caller's trust anchor rather than a directory Goliath owns.
        candidates.push_back(paths.root);
#endif
    }
    candidates.push_back(directory);

    fs::path previous;
    for (const fs::path& candidate : candidates) {
        if (candidate.empty()) {
            if (error) *error = "JGRF save-data directory is empty.";
            return false;
        }
        if (!previous.empty() && candidate == previous) continue;
        previous = candidate;

        const fs::path ioCandidate = filesystem_io_path(candidate);
        DirectPathInspection inspection = inspect_direct_path(ioCandidate);
        if (inspection.kind == DirectPathKind::Directory) continue;
        if (inspection.kind == DirectPathKind::Missing && !create)
            return true;
        if (inspection.kind == DirectPathKind::Missing && create) {
            std::string createError;
            if (ensure_direct_directory(ioCandidate, &createError)) continue;
            if (error) {
                *error = "Could not prepare direct JGRF save-data directory " +
                         candidate.string() + ": " + createError;
            }
            return false;
        }

        if (error) {
            *error = "JGRF save-data path is not a direct directory: " +
                     candidate.string();
            if (inspection.kind == DirectPathKind::Error &&
                !inspection.error.empty()) {
                *error += ": " + inspection.error;
            }
        }
        return false;
    }
    return true;
}

bool validate_direct_save_tree(const JgrfSavePaths& paths,
                               std::string* error) {
    return prepare_direct_save_branch(
               paths, ManagedSaveKind::StateSlot0, false, error) &&
           prepare_direct_save_branch(
               paths, ManagedSaveKind::MemoryCard, false, error);
}

bool resolve_safe_backup_directory(const AppPaths& paths,
                                   const std::string& system,
                                   const std::string& media,
                                   bool create,
                                   fs::path* directory,
                                   bool* exists,
                                   std::string* error) {
    if (exists) *exists = false;
    const fs::path root = effective_backup_root(paths);
    const fs::path exact = root / exact_media_storage_id(system, media);

    // Validate every Goliath-owned directory component. In particular, do not
    // follow a substituted <data>/goliath or save_backups symlink into an
    // unrelated location before creating or deleting an archive.
    for (const fs::path& candidate : {root.parent_path(), root, exact}) {
        if (candidate.empty()) {
            if (error) *error = "Backup directory has no usable parent.";
            return false;
        }
        bool present = false;
        if (!inspect_plain_directory(candidate, &present, error)) return false;
        if (!present && create) {
            if (!ensure_directory(candidate, error) ||
                !inspect_plain_directory(candidate, &present, error)) {
                return false;
            }
        }
        if (!present) {
            if (directory) *directory = exact;
            return true;
        }
    }

    if (directory) *directory = exact;
    if (exists) *exists = true;
    return true;
}

bool atomic_write_bytes(const fs::path& logical_path,
                        const std::vector<unsigned char>& bytes,
                        std::string* error) {
    if (!ensure_direct_directory(
            filesystem_io_path(logical_path.parent_path()), error)) {
        return false;
    }

    QSaveFile output(path_to_qstring(filesystem_io_path(logical_path)));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = "Could not open " + logical_path.string() +
                     " for writing: " + output.errorString().toStdString();
        }
        return false;
    }

    const qint64 size = static_cast<qint64>(bytes.size());
    if (size != 0 &&
        output.write(reinterpret_cast<const char*>(bytes.data()), size) != size) {
        if (error) {
            *error = "Could not write " + logical_path.string() + ": " +
                     output.errorString().toStdString();
        }
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        if (error) {
            *error = "Could not replace " + logical_path.string() + ": " +
                     output.errorString().toStdString();
        }
        return false;
    }
    return true;
}

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

    std::ostringstream output;
    output << std::put_time(&utc, "%Y%m%d_%H%M%S") << '_'
           << std::setw(3) << std::setfill('0') << milliseconds.count();
    return output.str();
}

std::string layout_manifest_id(JgrfSaveLayout layout) {
    return layout == JgrfSaveLayout::DynamicCore
        ? "dynamic-core" : "static-core";
}

bool validate_identity(const std::string& system,
                       const std::string& media,
                       std::string* normalized_media,
                       std::string* game_name,
                       std::string* error) {
    if (!supported_system(system)) {
        if (error) *error = "Unsupported system for save-data management.";
        return false;
    }
    const std::string normalized = normalize_game_profile_media(media);
    const std::string resolvedName = jgrf_game_name(media);
    if (normalized.empty() || resolvedName.empty()) {
        if (error) *error = "Invalid media identity for save-data management.";
        return false;
    }
    if (normalized_media) *normalized_media = normalized;
    if (game_name) *game_name = resolvedName;
    return true;
}

bool collect_current_payloads(const AppPaths& paths,
                              const std::string& media,
                              PayloadMap* payloads,
                              std::string* error) {
    if (!payloads) return false;
    payloads->clear();

    const std::string gameName = jgrf_game_name(media);
    if (gameName.empty()) {
        if (error) *error = "JGRF game name is empty.";
        return false;
    }
    const JgrfSavePaths savePaths = resolve_jgrf_save_paths(paths);
    if (!validate_direct_save_tree(savePaths, error)) return false;
    std::uintmax_t total = 0;
    for (const SaveFileSpec& spec : kSaveFileSpecs) {
        const fs::path path = managed_save_file_path(savePaths, gameName, spec.kind);
        bool exists = false;
        std::uintmax_t size = 0;
        if (!inspect_regular_file(path, &exists, &size, nullptr, error))
            return false;
        if (!exists) continue;
        if (size > kMaximumManagedFileBytes ||
            total > kMaximumSnapshotBytes - size) {
            if (error) {
                *error = "Managed save data exceeds the safe backup size limit.";
            }
            return false;
        }

        std::vector<unsigned char> bytes;
        if (!read_file_bytes(path, kMaximumManagedFileBytes, &bytes, error))
            return false;
        total += bytes.size();
        payloads->emplace(expected_filename(gameName, spec), std::move(bytes));
    }
    return true;
}

fs::path next_backup_path(const fs::path& directory, std::string* error) {
    const std::string timestamp = utc_timestamp();
    for (int suffix = 0; suffix < 1000; ++suffix) {
        std::string name = timestamp;
        if (suffix != 0) name += '_' + std::to_string(suffix);
        const fs::path candidate = directory / (name + ".zip");
        std::error_code ec;
        if (!fs::exists(filesystem_io_path(candidate), ec) && !ec)
            return candidate;
        if (ec) {
            if (error) {
                *error = "Could not inspect backup destination: " + ec.message();
            }
            return {};
        }
    }
    if (error) *error = "Could not allocate a unique backup filename.";
    return {};
}

SaveDataOperationResult write_backup_from_payloads(
        const AppPaths& paths,
        const std::string& system,
        const std::string& media,
        const PayloadMap& payloads) {
    SaveDataOperationResult result;
    std::string normalizedMedia;
    std::string gameName;
    if (!validate_identity(system, media, &normalizedMedia, &gameName,
                           &result.error)) {
        return result;
    }
    if (payloads.empty()) {
        result.error = "No save states or persistent save data exist for this game.";
        return result;
    }

    std::uintmax_t total = 0;
    json files = json::array();
    for (const auto& [name, bytes] : payloads) {
        const SaveFileSpec* spec = spec_for_filename(gameName, name);
        if (!spec || bytes.size() > kMaximumManagedFileBytes ||
            total > kMaximumSnapshotBytes - bytes.size()) {
            result.error = "Backup payload contains an invalid managed file.";
            return result;
        }
        total += bytes.size();
        files.push_back({
            {"name", name},
            {"kind", spec->manifest_id},
            {"size", bytes.size()},
        });
    }

    const JgrfSavePaths savePaths = resolve_jgrf_save_paths(paths);
    json manifest = {
        {"version", kBackupSchemaVersion},
        {"system", system},
        {"media", normalizedMedia},
        {"jgrf_game_name", gameName},
        {"jgrf_layout", layout_manifest_id(savePaths.layout)},
        {"created_utc", utc_timestamp()},
        {"files", std::move(files)},
    };
    const std::string manifestText = manifest.dump(2) + "\n";

    mz_zip_archive archive{};
    mz_zip_zero_struct(&archive);
    if (!mz_zip_writer_init_heap(&archive, 0, 128U * 1024U)) {
        result.error = "Could not initialize the ZIP backup writer.";
        return result;
    }

    auto writer_failure = [&](const std::string& prefix) {
        const mz_zip_error code = mz_zip_peek_last_error(&archive);
        result.error = prefix + ": " + mz_zip_get_error_string(code);
        mz_zip_writer_end(&archive);
    };

    if (!mz_zip_writer_add_mem(&archive, "manifest.json",
                               manifestText.data(), manifestText.size(),
                               MZ_BEST_SPEED)) {
        writer_failure("Could not add the backup manifest");
        return result;
    }
    for (const auto& [name, bytes] : payloads) {
        const void* data = bytes.empty() ? nullptr : bytes.data();
        if (!mz_zip_writer_add_mem(&archive, name.c_str(), data, bytes.size(),
                                   MZ_BEST_SPEED)) {
            writer_failure("Could not add " + name + " to the backup");
            return result;
        }
    }

    void* archiveMemory = nullptr;
    std::size_t archiveSize = 0;
    if (!mz_zip_writer_finalize_heap_archive(
            &archive, &archiveMemory, &archiveSize)) {
        writer_failure("Could not finalize the ZIP backup");
        return result;
    }
    const bool ended = mz_zip_writer_end(&archive) == MZ_TRUE;
    if (!ended || !archiveMemory || archiveSize == 0 ||
        archiveSize > kMaximumArchiveBytes) {
        if (archiveMemory) mz_free(archiveMemory);
        result.error = "The completed ZIP backup was invalid or too large.";
        return result;
    }

    fs::path directory;
    bool directoryExists = false;
    if (!resolve_safe_backup_directory(
            paths, system, normalizedMedia, true, &directory,
            &directoryExists, &result.error) || !directoryExists) {
        mz_free(archiveMemory);
        if (result.error.empty()) {
            result.error = "Could not prepare the exact-media backup directory.";
        }
        return result;
    }
    const fs::path destination = next_backup_path(directory, &result.error);
    if (destination.empty()) {
        mz_free(archiveMemory);
        return result;
    }

    std::vector<unsigned char> archiveBytes(
        static_cast<unsigned char*>(archiveMemory),
        static_cast<unsigned char*>(archiveMemory) + archiveSize);
    mz_free(archiveMemory);
    if (!atomic_write_bytes(destination, archiveBytes, &result.error))
        return result;

    result.success = true;
    result.backup_path = destination;
    result.file_count = payloads.size();
    return result;
}

bool plain_archive_member(std::string_view name) {
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string_view::npos &&
           name.find('\\') == std::string_view::npos;
}

bool json_unsigned(const json& value, std::uint64_t* output) {
    if (!output) return false;
    try {
        if (value.is_number_unsigned()) {
            *output = value.get<std::uint64_t>();
            return true;
        }
        if (!value.is_number_integer()) return false;
        const std::int64_t signedValue = value.get<std::int64_t>();
        if (signedValue < 0) return false;
        *output = static_cast<std::uint64_t>(signedValue);
        return true;
    } catch (...) {
        return false;
    }
}

ArchiveSnapshot read_backup_archive(const fs::path& path,
                                    const std::string& system,
                                    const std::string& media,
                                    bool extract_payloads) {
    ArchiveSnapshot result;
    std::string normalizedMedia;
    std::string gameName;
    if (!validate_identity(system, media, &normalizedMedia, &gameName,
                           &result.error)) {
        return result;
    }

    std::vector<unsigned char> archiveBytes;
    if (!read_file_bytes(path, kMaximumArchiveBytes, &archiveBytes,
                         &result.error)) {
        return result;
    }
    if (archiveBytes.empty()) {
        result.error = "Backup archive is empty.";
        return result;
    }

    mz_zip_archive archive{};
    mz_zip_zero_struct(&archive);
    if (!mz_zip_reader_init_mem(&archive, archiveBytes.data(),
                                archiveBytes.size(), 0)) {
        result.error = "Backup is not a readable ZIP archive.";
        return result;
    }

    auto fail = [&](std::string message) {
        result.error = std::move(message);
        mz_zip_reader_end(&archive);
    };

    const mz_uint memberCount = mz_zip_reader_get_num_files(&archive);
    if (memberCount < 2 || memberCount > kMaximumArchiveMembers) {
        fail("Backup contains an invalid number of archive members.");
        return result;
    }

    struct Member {
        mz_uint index = 0;
        std::uint64_t size = 0;
    };
    std::map<std::string, Member> members;
    std::uintmax_t totalPayload = 0;
    for (mz_uint index = 0; index < memberCount; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
            fail("Could not inspect a ZIP member.");
            return result;
        }
        const mz_uint required =
            mz_zip_reader_get_filename(&archive, index, nullptr, 0);
        if (required == 0 || required > MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE) {
            fail("Backup contains an invalid member name.");
            return result;
        }
        std::vector<char> filename(required);
        if (mz_zip_reader_get_filename(
                &archive, index, filename.data(), required) == 0) {
            fail("Could not read a ZIP member name.");
            return result;
        }
        if (filename.back() != '\0' ||
            std::find(filename.begin(), filename.end() - 1, '\0') !=
                filename.end() - 1) {
            fail("Backup contains an invalid member name.");
            return result;
        }
        const std::string name(filename.data());
        if (!plain_archive_member(name) || stat.m_is_directory ||
            stat.m_is_encrypted || !stat.m_is_supported ||
            !members.emplace(name, Member{index, stat.m_uncomp_size}).second) {
            fail("Backup contains an unsafe or duplicate member.");
            return result;
        }

        if (name == "manifest.json") {
            if (stat.m_uncomp_size > kMaximumManifestBytes) {
                fail("Backup manifest exceeds the safe size limit.");
                return result;
            }
        } else {
            if (!spec_for_filename(gameName, name) ||
                stat.m_uncomp_size > kMaximumManagedFileBytes ||
                totalPayload > kMaximumSnapshotBytes - stat.m_uncomp_size) {
                fail("Backup contains an unexpected or oversized save file.");
                return result;
            }
            totalPayload += stat.m_uncomp_size;
        }
    }

    const auto manifestIt = members.find("manifest.json");
    if (manifestIt == members.end()) {
        fail("Backup manifest is missing.");
        return result;
    }

    std::vector<unsigned char> manifestBytes(
        static_cast<std::size_t>(manifestIt->second.size));
    unsigned char emptyByte = 0;
    void* manifestDestination = manifestBytes.empty()
        ? static_cast<void*>(&emptyByte)
        : static_cast<void*>(manifestBytes.data());
    if (!mz_zip_reader_extract_to_mem(
            &archive, manifestIt->second.index, manifestDestination,
            manifestBytes.size(), 0)) {
        fail("Backup manifest could not be extracted or failed its CRC check.");
        return result;
    }

    json manifest;
    try {
        manifest = json::parse(manifestBytes.begin(), manifestBytes.end());
    } catch (const std::exception& ex) {
        fail(std::string("Backup manifest is malformed: ") + ex.what());
        return result;
    }

    if (!manifest.is_object() ||
        !manifest.contains("version") ||
        !manifest.at("version").is_number_integer() ||
        manifest.at("version").get<int>() != kBackupSchemaVersion ||
        !manifest.contains("system") || !manifest.at("system").is_string() ||
        !manifest.contains("media") || !manifest.at("media").is_string() ||
        !manifest.contains("jgrf_game_name") ||
        !manifest.at("jgrf_game_name").is_string() ||
        !manifest.contains("files") || !manifest.at("files").is_array()) {
        fail("Backup manifest has an unsupported or malformed schema.");
        return result;
    }
    if (manifest.at("system").get<std::string>() != system ||
        normalize_game_profile_media(
            manifest.at("media").get<std::string>()) != normalizedMedia ||
        manifest.at("jgrf_game_name").get<std::string>() != gameName) {
        fail("Backup belongs to a different exact media item.");
        return result;
    }

    std::set<std::string> manifestNames;
    for (const json& item : manifest.at("files")) {
        if (!item.is_object() ||
            !item.contains("name") || !item.at("name").is_string() ||
            !item.contains("kind") || !item.at("kind").is_string() ||
            !item.contains("size")) {
            fail("Backup manifest contains an invalid file record.");
            return result;
        }
        const std::string name = item.at("name").get<std::string>();
        const SaveFileSpec* spec = spec_for_filename(gameName, name);
        std::uint64_t declaredSize = 0;
        const auto memberIt = members.find(name);
        if (!spec || item.at("kind").get<std::string>() != spec->manifest_id ||
            !json_unsigned(item.at("size"), &declaredSize) ||
            memberIt == members.end() || memberIt->second.size != declaredSize ||
            !manifestNames.insert(name).second) {
            fail("Backup manifest does not match its save-data members.");
            return result;
        }
    }
    if (manifestNames.empty() || manifestNames.size() + 1 != members.size()) {
        fail("Backup manifest does not enumerate every archive member.");
        return result;
    }

    if (extract_payloads) {
        for (const std::string& name : manifestNames) {
            const Member& member = members.at(name);
            std::vector<unsigned char> bytes(
                static_cast<std::size_t>(member.size));
            void* destination = bytes.empty()
                ? static_cast<void*>(&emptyByte)
                : static_cast<void*>(bytes.data());
            if (!mz_zip_reader_extract_to_mem(
                    &archive, member.index, destination, bytes.size(), 0)) {
                fail("Save-data member " + name +
                     " could not be extracted or failed its CRC check.");
                return result;
            }
            result.payloads.emplace(name, std::move(bytes));
        }
    }

    if (!mz_zip_reader_end(&archive)) {
        result.error = "Could not close the ZIP backup reader cleanly.";
        return result;
    }
    result.success = true;
    result.file_count = manifestNames.size();
    return result;
}

bool direct_backup_member(const AppPaths& paths,
                          const std::string& system,
                          const std::string& media,
                          const fs::path& candidate) {
    if (candidate.empty() || candidate.filename().empty() ||
        lower_ascii(candidate.extension().string()) != ".zip") {
        return false;
    }

    std::error_code ec;
    const fs::path expected = fs::absolute(
        game_save_backup_directory(paths, system, media), ec).lexically_normal();
    if (ec) return false;
    const fs::path actual =
        fs::absolute(candidate.parent_path(), ec).lexically_normal();
    return !ec && actual == expected;
}

bool remove_regular_file(const fs::path& logical_path, std::string* error) {
    const fs::path ioPath = filesystem_io_path(logical_path);
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(ioPath, ec);
    if (ec) {
        if (error) *error = "Could not inspect " + logical_path.string() +
                            ": " + ec.message();
        return false;
    }
    if (!fs::is_regular_file(status)) {
        if (error) *error = "Refusing to delete a non-regular file.";
        return false;
    }
    if (!fs::remove(ioPath, ec) || ec) {
        if (error) *error = "Could not delete " + logical_path.string() +
                            ": " + ec.message();
        return false;
    }
    return true;
}

bool apply_payload_snapshot(const JgrfSavePaths& save_paths,
                            std::string_view game_name,
                            const PayloadMap& payloads,
                            std::string* error) {
    // Validate both branches before the first write or deletion. This keeps a
    // substituted unused branch from causing a partially applied snapshot.
    if (!validate_direct_save_tree(save_paths, error)) return false;

    for (const SaveFileSpec& spec : kSaveFileSpecs) {
        const std::string name = expected_filename(game_name, spec);
        const fs::path path = managed_save_file_path(
            save_paths, game_name, spec.kind);
        const auto payload = payloads.find(name);
        if (payload != payloads.end()) {
            if (!prepare_direct_save_branch(
                    save_paths, spec.kind, true, error)) {
                return false;
            }
            if (!atomic_write_bytes(path, payload->second, error)) return false;
            continue;
        }

        const fs::path ioPath = filesystem_io_path(path);
        std::error_code ec;
        const fs::file_status status = fs::symlink_status(ioPath, ec);
        if (ec == std::errc::no_such_file_or_directory) continue;
        if (ec) {
            if (error) *error = "Could not inspect " + path.string() +
                                ": " + ec.message();
            return false;
        }
        if (!fs::exists(status)) continue;
        if (!fs::is_regular_file(status)) {
            if (error) *error = "Refusing to replace a non-regular save path.";
            return false;
        }
        if (!fs::remove(ioPath, ec) || ec) {
            if (error) *error = "Could not remove stale save data " +
                                path.string() + ": " + ec.message();
            return false;
        }
    }
    return true;
}

} // namespace

const char* jgrf_save_layout_name(JgrfSaveLayout layout) {
    return layout == JgrfSaveLayout::DynamicCore
        ? "Dynamic JGRF core"
        : "Static JGRF core";
}

const char* managed_save_kind_name(ManagedSaveKind kind) {
    switch (kind) {
    case ManagedSaveKind::StateSlot0: return "Save State Slot 0";
    case ManagedSaveKind::StateSlot1: return "Save State Slot 1";
    case ManagedSaveKind::Nvram: return "NVRAM";
    case ManagedSaveKind::CartridgeRam: return "Cartridge RAM";
    case ManagedSaveKind::MemoryCard: return "Memory Card";
    case ManagedSaveKind::CdBackupRam: return "CD Backup RAM";
    case ManagedSaveKind::Dips: return "DIP Switches";
    }
    return "Unknown";
}

std::string jgrf_game_name(std::string_view media) {
    const std::size_t separator = media.find_last_of("/\\");
    std::string name(media.substr(
        separator == std::string_view::npos ? 0 : separator + 1));
    if (name.size() > 127) name.resize(127);

    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name.resize(dot);
    return name;
}

JgrfSavePaths resolve_jgrf_save_paths(const AppPaths& paths) {
    JgrfSavePaths dynamic;
    dynamic.layout = JgrfSaveLayout::DynamicCore;
    dynamic.root = paths.data_dir / "jollygood";
    dynamic.state_directory = dynamic.root / "state" / "geolith";
    dynamic.save_directory = dynamic.root / "save" / "geolith";

    JgrfSavePaths staticCore;
    staticCore.layout = JgrfSaveLayout::StaticCore;
    staticCore.root = static_data_root(paths);
    staticCore.state_directory = staticCore.root / "state";
    staticCore.save_directory = staticCore.root / "save";

    if (has_dynamic_core_argument(paths.jollygood_args)) return dynamic;

    const bool staticExists = directory_exists(staticCore.state_directory) ||
                              directory_exists(staticCore.save_directory);
    const bool dynamicExists = directory_exists(dynamic.state_directory) ||
                               directory_exists(dynamic.save_directory);
    if (staticExists) return staticCore;
    if (dynamicExists) return dynamic;

    return staticCore;
}

fs::path managed_save_file_path(const JgrfSavePaths& paths,
                                std::string_view game_name,
                                ManagedSaveKind kind) {
    const SaveFileSpec* spec = spec_for_kind(kind);
    if (!spec || game_name.empty()) return {};
    const fs::path& directory = is_state_kind(kind)
        ? paths.state_directory : paths.save_directory;
    return directory / expected_filename(game_name, *spec);
}

fs::path game_save_backup_directory(const AppPaths& paths,
                                    const std::string& system,
                                    const std::string& media) {
    return effective_backup_root(paths) /
           exact_media_storage_id(system, media);
}

std::vector<ManagedSaveFile> list_game_save_data(
        const AppPaths& paths,
        const std::string& media,
        std::string* error) {
    if (error) error->clear();
    std::vector<ManagedSaveFile> files;
    const std::string gameName = jgrf_game_name(media);
    if (gameName.empty()) {
        if (error) *error = "JGRF game name is empty.";
        return files;
    }

    const JgrfSavePaths savePaths = resolve_jgrf_save_paths(paths);
    if (!validate_direct_save_tree(savePaths, error)) return files;
    for (const SaveFileSpec& spec : kSaveFileSpecs) {
        ManagedSaveFile file;
        file.kind = spec.kind;
        file.path = managed_save_file_path(savePaths, gameName, spec.kind);
        bool exists = false;
        if (!inspect_regular_file(file.path, &exists, &file.size,
                                  &file.modified, error)) {
            return {};
        }
        if (exists) files.push_back(std::move(file));
    }
    return files;
}

std::vector<SaveBackupRecord> list_game_save_backups(
        const AppPaths& paths,
        const std::string& system,
        const std::string& media,
        std::string* error) {
    if (error) error->clear();
    std::vector<SaveBackupRecord> backups;
    std::string normalizedMedia;
    std::string gameName;
    if (!validate_identity(system, media, &normalizedMedia, &gameName, error))
        return backups;

    fs::path directory;
    bool directoryExists = false;
    if (!resolve_safe_backup_directory(
            paths, system, normalizedMedia, false, &directory,
            &directoryExists, error)) {
        return backups;
    }
    if (!directoryExists) return backups;

    const fs::path ioDirectory = filesystem_io_path(directory);
    std::error_code ec;
    fs::directory_iterator iterator(ioDirectory, ec);
    const fs::directory_iterator end;
    while (!ec && iterator != end) {
        const fs::directory_entry& entry = *iterator;
        std::error_code entryError;
        const fs::file_status status = entry.symlink_status(entryError);
        if (!entryError && fs::is_regular_file(status) &&
            lower_ascii(entry.path().extension().string()) == ".zip") {
            SaveBackupRecord backup;
            backup.path = directory / entry.path().filename();
            backup.size = entry.file_size(entryError);
            if (!entryError)
                backup.modified = entry.last_write_time(entryError);
            if (entryError) {
                backup.problem = "Could not inspect backup metadata: " +
                                 entryError.message();
            } else {
                const ArchiveSnapshot inspection = read_backup_archive(
                    backup.path, system, normalizedMedia, false);
                backup.compatible = inspection.success;
                backup.problem = inspection.error;
                backup.file_count = inspection.file_count;
            }
            backups.push_back(std::move(backup));
        }
        iterator.increment(ec);
    }
    if (ec && error) *error = "Could not enumerate backups: " + ec.message();

    std::sort(backups.begin(), backups.end(),
              [](const SaveBackupRecord& left,
                 const SaveBackupRecord& right) {
                  return left.path.filename().string() >
                         right.path.filename().string();
              });
    return backups;
}

SaveDataOperationResult create_game_save_backup(
        const AppPaths& paths,
        const std::string& system,
        const std::string& media) {
    SaveDataOperationResult result;
    std::string normalizedMedia;
    std::string gameName;
    if (!validate_identity(system, media, &normalizedMedia, &gameName,
                           &result.error)) {
        return result;
    }

    PayloadMap payloads;
    if (!collect_current_payloads(paths, media, &payloads, &result.error))
        return result;
    return write_backup_from_payloads(
        paths, system, normalizedMedia, payloads);
}

SaveDataOperationResult restore_game_save_backup(
        const AppPaths& paths,
        const std::string& system,
        const std::string& media,
        const fs::path& backup_path) {
    SaveDataOperationResult result;
    std::string normalizedMedia;
    std::string gameName;
    if (!validate_identity(system, media, &normalizedMedia, &gameName,
                           &result.error)) {
        return result;
    }
    if (!direct_backup_member(
            paths, system, normalizedMedia, backup_path)) {
        result.error = "Backup is outside this exact-media backup directory.";
        return result;
    }
    fs::path directory;
    bool directoryExists = false;
    if (!resolve_safe_backup_directory(
            paths, system, normalizedMedia, false, &directory,
            &directoryExists, &result.error) || !directoryExists) {
        if (result.error.empty()) result.error = "Backup directory is missing.";
        return result;
    }

    const ArchiveSnapshot archive = read_backup_archive(
        backup_path, system, normalizedMedia, true);
    if (!archive.success) {
        result.error = archive.error;
        return result;
    }

    PayloadMap originals;
    if (!collect_current_payloads(paths, media, &originals, &result.error))
        return result;
    if (!originals.empty()) {
        SaveDataOperationResult protection = write_backup_from_payloads(
            paths, system, normalizedMedia, originals);
        if (!protection.success) {
            result.error = "Could not create the protective backup: " +
                           protection.error;
            return result;
        }
        result.protective_backup = protection.backup_path;
    }

    const JgrfSavePaths savePaths = resolve_jgrf_save_paths(paths);
    std::string applyError;
    if (!apply_payload_snapshot(
            savePaths, gameName, archive.payloads, &applyError)) {
        std::string rollbackError;
        const bool rolledBack = apply_payload_snapshot(
            savePaths, gameName, originals, &rollbackError);
        result.error = "Restore failed: " + applyError;
        if (!rolledBack) {
            result.error += " Rollback also failed: " + rollbackError;
        }
        return result;
    }

    result.success = true;
    result.file_count = archive.file_count;
    return result;
}

SaveDataOperationResult delete_game_save_data(
        const AppPaths& paths,
        const std::string& system,
        const std::string& media,
        ManagedSaveKind kind) {
    SaveDataOperationResult result;
    std::string normalizedMedia;
    std::string gameName;
    if (!validate_identity(system, media, &normalizedMedia, &gameName,
                           &result.error)) {
        return result;
    }
    const SaveFileSpec* spec = spec_for_kind(kind);
    if (!spec) {
        result.error = "Unknown managed save-data type.";
        return result;
    }

    const JgrfSavePaths savePaths = resolve_jgrf_save_paths(paths);
    if (!validate_direct_save_tree(savePaths, &result.error)) return result;
    const fs::path target = managed_save_file_path(savePaths, gameName, kind);
    bool exists = false;
    if (!inspect_regular_file(target, &exists, nullptr, nullptr, &result.error))
        return result;
    if (!exists) {
        result.error = "The selected save-data file no longer exists.";
        return result;
    }

    PayloadMap originals;
    if (!collect_current_payloads(paths, media, &originals, &result.error))
        return result;
    SaveDataOperationResult protection = write_backup_from_payloads(
        paths, system, normalizedMedia, originals);
    if (!protection.success) {
        result.error = "Could not create the protective backup: " +
                       protection.error;
        return result;
    }
    result.protective_backup = protection.backup_path;

    if (!remove_regular_file(target, &result.error)) return result;
    result.success = true;
    result.file_count = 1;
    return result;
}

SaveDataOperationResult delete_game_save_backup(
        const AppPaths& paths,
        const std::string& system,
        const std::string& media,
        const fs::path& backup_path) {
    SaveDataOperationResult result;
    std::string normalizedMedia;
    std::string gameName;
    if (!validate_identity(system, media, &normalizedMedia, &gameName,
                           &result.error)) {
        return result;
    }
    if (!direct_backup_member(
            paths, system, normalizedMedia, backup_path)) {
        result.error = "Backup is outside this exact-media backup directory.";
        return result;
    }
    fs::path directory;
    bool directoryExists = false;
    if (!resolve_safe_backup_directory(
            paths, system, normalizedMedia, false, &directory,
            &directoryExists, &result.error) || !directoryExists) {
        if (result.error.empty()) result.error = "Backup directory is missing.";
        return result;
    }
    if (!remove_regular_file(backup_path, &result.error)) return result;
    result.success = true;
    result.file_count = 1;
    return result;
}

} // namespace goliath
