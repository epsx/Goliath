// jollygood_launch.hpp — shared launch policy and non-interactive JGRF
// preparation used by the UI and tests. Neo Geo CD CHD launch is enabled
// only when the installed Geolith core advertises CHD for the neogeocd system.
#pragma once

#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/qt_path.hpp"
#include "game/game_model.hpp"

namespace goliath {

struct AppPaths;
struct GameLaunchProfile;

inline std::string launch_extension_lower(const std::string& file) {
    std::string ext = path_to_utf8(path_from_utf8(file).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

inline bool jollygood_args_have_verbose_logging(
        const std::vector<std::string>& args) {
    return std::find(args.begin(), args.end(), "-v") != args.end() ||
           std::find(args.begin(), args.end(), "--verbose") != args.end();
}

// JGRF accepts both separated and attached getopt forms. An explicit Goliath
// export must not be combined with a configured writer because JGRF's global
// waveout state makes multiple -o/--wave options ambiguous.
inline bool jollygood_args_have_wave_output(
        const std::vector<std::string>& args) {
    for (const std::string& arg : args) {
        if (arg == "--wave" || arg.rfind("--wave=", 0) == 0)
            return true;

        if (arg.size() < 2 || arg[0] != '-' || arg[1] == '-')
            continue;

        // musl_getopt accepts short-option clusters. Stop scanning when an
        // option that consumes the rest of this argv item is reached; this
        // avoids mistaking the 'o' inside values such as -cgeolith for -o.
        for (std::size_t index = 1; index < arg.size(); ++index) {
            const char option = arg[index];
            if (option == 'o') return true;
            if (std::string_view("abcegrsx").find(option) !=
                std::string_view::npos) {
                break;
            }
        }
    }
    return false;
}

// Cartridge launch accepts .neo only. Neo Geo CD CUE is always launchable;
// CHD is gated by the exact installed Geolith capability probe.
inline bool media_is_launchable(std::string_view system,
                                const std::string& file,
                                bool chd_supported) {
    const std::string ext = launch_extension_lower(file);
    if (system == "neogeo") return ext == ".neo";
    if (system != "neogeocd") return false;
    if (ext == ".cue") return true;
    if (ext == ".chd") return chd_supported;
    return false;
}

// JGRF/Geolith can open Neo Geo CD media beyond legacy MAX_PATH when Windows
// receives an extended-length absolute path. CUE track filenames are resolved
// inside Geolith and may cross MAX_PATH even when the selected CUE is shorter,
// so prefix every absolute CUE. CHD only needs the prefix when its own path is
// long. Keep the logical path elsewhere and transform only the final argument.
inline QString prepare_media_path_for_launch(std::string_view system,
                                              const QString& mediaPath) {
#if defined(_WIN32)
    if (system != "neogeocd")
        return mediaPath;

    const bool cue =
        mediaPath.endsWith(QStringLiteral(".cue"), Qt::CaseInsensitive);
    if (!cue && mediaPath.size() < 260)
        return mediaPath;

    QString native = mediaPath;
    native.replace('/', '\\');

    // Already extended.
    if (native.startsWith(QStringLiteral("\\\\?\\")))
        return native;

    // UNC: \\server\share\... -> \\?\UNC\server\share\...
    if (native.startsWith(QStringLiteral("\\\\")))
        return QStringLiteral("\\\\?\\UNC\\") + native.mid(2);

    // Extended-length syntax requires an absolute drive path.
    const bool driveAbsolute =
        native.size() >= 3 &&
        native.at(1) == QChar(':') &&
        native.at(2) == QChar('\\');
    if (!driveAbsolute)
        return mediaPath;

    return QStringLiteral("\\\\?\\") + native;
#else
    (void)system;
    return mediaPath;
#endif
}

// QFileDialog returns an absolute native path. Preserve normal paths and add
// Windows extended-length syntax only when the WAV destination itself reaches
// MAX_PATH. JGRF receives the result as one argv item, even when it has spaces.
inline QString prepare_wave_output_path_for_launch(const QString& outputPath) {
#if defined(_WIN32)
    if (outputPath.size() < 260)
        return outputPath;

    QString native = outputPath;
    native.replace('/', '\\');
    if (native.startsWith(QStringLiteral("\\\\?\\")))
        return native;
    if (native.startsWith(QStringLiteral("\\\\")))
        return QStringLiteral("\\\\?\\UNC\\") + native.mid(2);

    const bool driveAbsolute =
        native.size() >= 3 && native.at(1) == QChar(':') &&
        native.at(2) == QChar('\\');
    if (driveAbsolute)
        return QStringLiteral("\\\\?\\") + native;
#endif
    return outputPath;
}

// Preserve the user's configured JGRF/core arguments. Neo Geo CD gets an
// explicit system selector so a CUE is never routed through cartridge logic.
inline QStringList build_jollygood_launch_args(
        const QString& executable,
        const std::vector<std::string>& baseArgs,
        std::string_view system,
        const QString& mediaPath,
        std::optional<int> videoApiOverride = std::nullopt,
        std::optional<int> shaderOverride = std::nullopt,
        std::optional<int> fullscreenOverride = std::nullopt,
        std::optional<int> scaleOverride = std::nullopt,
        bool verboseLogging = false,
        const QString& waveOutputPath = {}) {
    QStringList args;
    args << executable;
    const bool validFullscreen = fullscreenOverride.has_value() &&
        (*fullscreenOverride == 0 || *fullscreenOverride == 1);
    for (const std::string& arg : baseArgs) {
        // JGRF records --fullscreen and --window independently, then always
        // applies --window last. Remove either global form when this profile
        // has an explicit choice so the exact-media value is deterministic.
        if (validFullscreen &&
            (arg == "-f" || arg == "--fullscreen" ||
             arg == "-w" || arg == "--window")) {
            continue;
        }
        args << QString::fromStdString(arg);
    }

    if (system == "neogeocd")
        args << "-e" << "neogeocd";

    // JGRF applies command-line frontend settings after settings.ini and the
    // core-specific override file. Appending these after the user's base args
    // guarantees that the per-game choices win for this launch only.
    if (videoApiOverride.has_value() &&
        *videoApiOverride >= 0 && *videoApiOverride <= 3) {
        args << "--video" << QString::number(*videoApiOverride);
    }
    if (validFullscreen) {
        args << (*fullscreenOverride == 1 ? "--fullscreen" : "--window");
    }
    if (scaleOverride.has_value() &&
        *scaleOverride >= 1 && *scaleOverride <= 8) {
        args << "--scale" << QString::number(*scaleOverride);
    }
    if (shaderOverride.has_value() &&
        *shaderOverride >= 0 && *shaderOverride <= 6) {
        args << "--shader" << QString::number(*shaderOverride);
    }

    // Session diagnostics are additive and never rewrite the configured JGRF
    // arguments. Avoid a duplicate when the user already supplied -v there,
    // and preserve the selected media as the final command-line argument.
    if (verboseLogging && !jollygood_args_have_verbose_logging(baseArgs))
        args << "--verbose";

    // The explicit one-shot writer belongs immediately before the selected
    // media. Preparation rejects a configured writer; keep this lower-level
    // guard too so direct callers never add a duplicate output option.
    if (!waveOutputPath.isEmpty() &&
        !jollygood_args_have_wave_output(baseArgs)) {
        args << "--wave" << prepare_wave_output_path_for_launch(waveOutputPath);
    }

    args << prepare_media_path_for_launch(system, mediaPath);
    return args;
}

// Resolve which file should be launched from a selected tree item. Explicit
// variants win; otherwise the game's canonical main ROM/media is used.
std::optional<std::string> selected_launch_media(const Game& game, int romIndex);

enum class LaunchPreparationStatus {
    Ready,
    InvalidSystem,
    MediaMissing,
    MediaNotRegularFile,
    MediaInspectionFailed,
    ExecutableNotFound,
    CapabilityProbeFailed,
    ChdUnsupported,
    UnsupportedMedia,
    ProfileConfigurationFailed,
    WaveOutputConflict,
    WaveOutputInvalid
};

// Non-interactive launch preflight. This centralizes executable resolution,
// Neo Geo CD capability validation, environment construction, JGRF arguments,
// working directory and log path. UI prompts remain in MainWindow.
struct JollygoodLaunchPreparation {
    LaunchPreparationStatus status = LaunchPreparationStatus::Ready;
    std::string detail;
    std::string advertised_formats;
    std::string effective_system;
    std::filesystem::path media_path;
    bool chd_supported = false;
    bool mvs_hardware = false;
    bool profile_applied = false;
    QStringList args;
    QProcessEnvironment environment;
    QString profile_config_root;
    QString working_directory;
    QString log_path;
};

// Presentation-ready failure text shared by normal launch, Benchmark, and
// one-shot WAV export. Every non-ready preparation carries a concrete reason;
// the media path and effective BIOS/hardware mode provide diagnostic context.
std::string launch_preparation_failure_text(
    const JollygoodLaunchPreparation& preparation);

JollygoodLaunchPreparation prepare_jollygood_launch(
    const AppPaths& paths,
    const std::filesystem::path& romDir,
    const std::filesystem::path& neocdDir,
    const std::string& system,
    const std::string& media,
    const GameLaunchProfile* profile = nullptr,
    bool verboseLogging = false,
    std::optional<std::filesystem::path> waveOutputPath = std::nullopt);

} // namespace goliath
