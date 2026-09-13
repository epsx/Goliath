// audio_export_dialog.hpp — one-shot WAV destination picker and confirmation
// for stock JGRF. All filesystem policy remains in game/audio_export.
#pragma once

#include <QString>

#include <filesystem>
#include <optional>

class QWidget;

namespace goliath {

// Returns a new absolute .wav path only after validation, a same-directory
// write probe, and explicit Launch & Record confirmation. Cancellation and
// every failure return nullopt without creating the requested target.
std::optional<std::filesystem::path> request_audio_export_target(
    const QString& display_name,
    const std::filesystem::path& default_directory,
    QWidget* parent = nullptr);

} // namespace goliath
