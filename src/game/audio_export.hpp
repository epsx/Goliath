// audio_export.hpp — deterministic, non-destructive path policy for JGRF's
// one-shot WAV writer. The UI chooses a destination; these helpers sanitize a
// suggested filename and reject targets that JGRF would refuse to overwrite.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace goliath {

struct AudioExportTargetValidation {
    bool success = false;
    std::string error;
};

// Produces a bounded portable filename stem while preserving readable UTF-8
// bytes. Windows-forbidden punctuation and control characters become
// separators.
std::string sanitize_audio_export_stem(std::string_view display_name);

bool audio_export_has_wav_extension(const std::filesystem::path& path);

// Adds .wav only when the selected path does not already end in .wav,
// case-insensitively. An unrelated suffix is preserved (song.raw ->
// song.raw.wav) so Goliath never silently changes the user's typed name.
std::filesystem::path ensure_audio_export_wav_extension(
    std::filesystem::path path);

// Returns the desired path when free, otherwise adds " - 2", " - 3", ...
// before .wav. Existing files, directories, and symlinks are all occupied.
std::filesystem::path unique_audio_export_path(
    const std::filesystem::path& desired_path);

// JGRF deliberately refuses to overwrite an existing WAV. Validate the same
// rule before launch and require a direct absolute target in an existing
// directory. This function never creates, truncates, or removes anything.
AudioExportTargetValidation validate_audio_export_target(
    const std::filesystem::path& path);

} // namespace goliath
