#include "game/audio_export.hpp"

#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

namespace goliath {

namespace {

constexpr std::size_t kPortableStemByteLimit = 160;

bool invalid_filename_byte(unsigned char ch) {
    if (ch < 32 || ch == 127) return true;
    switch (ch) {
    case '<':
    case '>':
    case ':':
    case '"':
    case '/':
    case '\\':
    case '|':
    case '?':
    case '*':
        return true;
    default:
        return false;
    }
}

bool portable_space_byte(unsigned char ch) {
    return ch == ' ' || (ch >= '\t' && ch <= '\r');
}

void trim_portable_filename(std::string& value) {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '.' ||
            value.front() == '-')) {
        value.erase(value.begin());
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '.' ||
            value.back() == '-')) {
        value.pop_back();
    }
}

void truncate_portable_filename(std::string& value) {
    if (value.size() <= kPortableStemByteLimit) return;

    std::size_t cut = kPortableStemByteLimit;
    while (cut > 0 &&
           (static_cast<unsigned char>(value[cut]) & 0xC0U) == 0x80U) {
        --cut;
    }
    value.resize(cut);
}

std::string ascii_upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
                       return static_cast<char>(ch);
                   });
    return value;
}

bool windows_reserved_stem(const std::string& value) {
    const std::string upper = ascii_upper(value);
    if (upper == "CON" || upper == "PRN" || upper == "AUX" ||
        upper == "NUL") {
        return true;
    }
    if (upper.size() == 4 && upper.back() >= '1' && upper.back() <= '9') {
        return upper.rfind("COM", 0) == 0 || upper.rfind("LPT", 0) == 0;
    }
    return false;
}

bool path_status_occupied(const fs::path& path, std::error_code& ec) {
    ec.clear();
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) {
        ec.clear();
        return false;
    }
    if (ec) return true;
    return status.type() != fs::file_type::not_found;
}

fs::path numbered_audio_export_path(const fs::path& path, int number) {
    fs::path filename = path.stem();
    filename += fs::path(" - " + std::to_string(number)).native();
    filename += path.extension().native();
    return path.parent_path() / filename;
}

} // namespace

std::string sanitize_audio_export_stem(std::string_view displayName) {
    std::string result;
    result.reserve(displayName.size());

    bool pendingSpace = false;
    for (unsigned char ch : displayName) {
        if (portable_space_byte(ch)) {
            pendingSpace = !result.empty();
            continue;
        }

        if (invalid_filename_byte(ch)) {
            if (!result.empty() && result.back() != '-' && result.back() != ' ')
                result.push_back('-');
            pendingSpace = false;
            continue;
        }

        if (pendingSpace && !result.empty() && result.back() != '-')
            result.push_back(' ');
        pendingSpace = false;
        result.push_back(static_cast<char>(ch));
    }

    truncate_portable_filename(result);
    trim_portable_filename(result);
    if (result.empty()) result = "Goliath Audio";
    if (windows_reserved_stem(result)) result = "Goliath - " + result;
    return result;
}

bool audio_export_has_wav_extension(const fs::path& path) {
    const auto extension = path.extension().native();
    if (extension.size() != 4 || extension[0] != '.') return false;

    const auto lowerAscii = [](auto ch) {
        if (ch >= 'A' && ch <= 'Z')
            return static_cast<decltype(ch)>(ch - 'A' + 'a');
        return ch;
    };
    return lowerAscii(extension[1]) == 'w' &&
           lowerAscii(extension[2]) == 'a' &&
           lowerAscii(extension[3]) == 'v';
}

fs::path ensure_audio_export_wav_extension(fs::path path) {
    if (!audio_export_has_wav_extension(path)) path += fs::path(".wav").native();
    return path;
}

fs::path unique_audio_export_path(const fs::path& desiredPath) {
    const fs::path normalized = ensure_audio_export_wav_extension(desiredPath);
    std::error_code ec;
    const bool normalizedOccupied = path_status_occupied(normalized, ec);
    if (ec || !normalizedOccupied) return normalized;

    for (int number = 2; number <= 9999; ++number) {
        const fs::path candidate = numbered_audio_export_path(normalized, number);
        const bool candidateOccupied = path_status_occupied(candidate, ec);
        if (ec) return normalized;
        if (!candidateOccupied) return candidate;
    }
    return normalized;
}

AudioExportTargetValidation validate_audio_export_target(
        const fs::path& path) {
    AudioExportTargetValidation result;
    if (path.empty()) {
        result.error = "No WAV output file was selected.";
        return result;
    }
    if (!path.is_absolute()) {
        result.error = "The WAV output path must be absolute.";
        return result;
    }
    if (!audio_export_has_wav_extension(path)) {
        result.error = "The audio export target must use the .wav extension.";
        return result;
    }

    std::error_code ec;
    const fs::file_status targetStatus = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory)
        ec.clear();
    if (ec) {
        result.error = "The WAV output target could not be inspected.";
        return result;
    }
    if (targetStatus.type() != fs::file_type::not_found) {
        result.error =
            "The WAV output target already exists. JGRF never overwrites "
            "existing files; choose a new filename.";
        return result;
    }

    const fs::path parent = path.parent_path();
    if (parent.empty()) {
        result.error = "The WAV output directory is missing.";
        return result;
    }
    const fs::file_status parentStatus = fs::status(parent, ec);
    if (ec || !fs::is_directory(parentStatus)) {
        result.error = "The WAV output directory does not exist or is not a directory.";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace goliath
