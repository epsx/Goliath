#include "game/jollygood_launch.hpp"

#include "common/paths.hpp"
#include "common/qt_path.hpp"
#include "game/audio_export.hpp"
#include "game/game_profile.hpp"
#include "game/game_profile_summary.hpp"
#include "game/geolith_capabilities.hpp"
#include "game/jollygood_executable.hpp"
#include "game/game_profile_runtime.hpp"

#include <QByteArray>

#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace goliath {

namespace {

constexpr qsizetype kJgrfConfigPathCapacity = 128;

// JGRF 2.0.1 stores its resolved data path in a fixed 64-byte buffer. Keep
// portable paths short by passing locations below the process working
// directory as relative paths. External configured locations remain absolute.
QString environmentPathForWorkingDirectory(const fs::path& path,
                                           const fs::path& workingDirectory) {
    const fs::path relative = path.lexically_normal().lexically_relative(
        workingDirectory.lexically_normal());
    if (!relative.empty() && !relative.is_absolute() &&
        *relative.begin() != fs::path("..")) {
        return path_to_qstring(relative);
    }
    return path_to_qstring(path);
}

std::optional<std::string> perGameConfigPathError(
        const QString& environmentRoot) {
    QString jgrfConfigPath = environmentRoot;
    if (!jgrfConfigPath.endsWith('/') && !jgrfConfigPath.endsWith('\\'))
        jgrfConfigPath += '/';
    jgrfConfigPath += "jollygood";

    // JGRF appends a trailing separator and stores the result in
    // char configpath[128]. Measure the path it actually receives through
    // XDG_CONFIG_HOME, not the absolute location of Goliath's portable files.
    const qsizetype finalConfigPathBytes =
        jgrfConfigPath.toUtf8().size() + 1;
    if (finalConfigPathBytes < kJgrfConfigPathCapacity)
        return std::nullopt;

    return "Per-game JGRF configuration path is too long (" +
           std::to_string(finalConfigPathBytes) +
           " bytes; maximum is 127): " + jgrfConfigPath.toStdString();
}

std::optional<int> profile_frontend_video_override(
        const GameLaunchProfile* profile,
        std::string_view key) {
    if (!profile) return std::nullopt;
    const VideoSettingSpec* spec = find_video_setting(
        jgrf_video_setting_specs(), key);
    return spec ? video_setting_override(profile->jgrf_video, *spec)
                : std::nullopt;
}

} // namespace

std::optional<std::string> selected_launch_media(const Game& game, int romIndex) {
    if (romIndex >= 0 && romIndex < static_cast<int>(game.roms.size())) {
        const std::string& file =
            game.roms[static_cast<std::size_t>(romIndex)].file;
        if (file.empty()) return std::nullopt;
        return file;
    }

    if (game.main_rom.has_value() && !game.main_rom->empty())
        return *game.main_rom;

    return std::nullopt;
}

JollygoodLaunchPreparation prepare_jollygood_launch(
        const AppPaths& paths,
        const fs::path& romDir,
        const fs::path& neocdDir,
        const std::string& system,
        const std::string& media,
        const GameLaunchProfile* profile,
        bool verboseLogging,
        std::optional<fs::path> waveOutputPath) {
    JollygoodLaunchPreparation preparation;

    if (system != "neogeo" && system != "neogeocd") {
        preparation.status = LaunchPreparationStatus::InvalidSystem;
        preparation.detail = "The selected library item has an unsupported "
                             "system identifier: " + system;
        return preparation;
    }

    const GameProfileGlobalSettings global =
        load_game_profile_global_settings(paths);
    const GameLaunchProfile inherited;
    const GameLaunchProfile& effective = profile ? *profile : inherited;
    preparation.effective_system =
        summarize_game_profile(system, profile, global).system;
    preparation.mvs_hardware =
        game_profile_uses_mvs_hardware(system, effective, global);

    if (media.empty()) {
        preparation.status = LaunchPreparationStatus::MediaMissing;
        preparation.detail =
            "The selected library item has no launchable media path.";
        return preparation;
    }

    const fs::path& mediaDir =
        system == "neogeocd" ? neocdDir : romDir;
    preparation.media_path = mediaDir / path_from_utf8(media);

    const std::string extension = launch_extension_lower(media);
    if ((system == "neogeo" && extension != ".neo") ||
        (system == "neogeocd" && extension != ".cue" &&
         extension != ".chd")) {
        preparation.status = LaunchPreparationStatus::UnsupportedMedia;
        preparation.detail = "The selected " +
            std::string(system == "neogeo" ? "cartridge" : "Neo Geo CD") +
            " media type is unsupported: " + media;
        return preparation;
    }

    std::error_code mediaError;
    const fs::file_status mediaStatus =
        fs::status(preparation.media_path, mediaError);
    if (mediaError == std::errc::no_such_file_or_directory ||
        (!mediaError && !fs::exists(mediaStatus))) {
        preparation.status = LaunchPreparationStatus::MediaMissing;
        preparation.detail = "The selected game file no longer exists. "
                             "Rescan the library or reconnect its storage.";
        return preparation;
    }
    if (mediaError) {
        preparation.status = LaunchPreparationStatus::MediaInspectionFailed;
        preparation.detail = "Goliath could not inspect the selected game "
                             "file: " + mediaError.message();
        return preparation;
    }
    if (!fs::is_regular_file(mediaStatus)) {
        preparation.status = LaunchPreparationStatus::MediaNotRegularFile;
        preparation.detail =
            "The selected game path is not a regular file.";
        return preparation;
    }

    if (waveOutputPath.has_value()) {
        const AudioExportTargetValidation validation =
            validate_audio_export_target(*waveOutputPath);
        if (!validation.success) {
            preparation.status = LaunchPreparationStatus::WaveOutputInvalid;
            preparation.detail = validation.error;
            return preparation;
        }
        if (jollygood_args_have_wave_output(paths.jollygood_args)) {
            preparation.status = LaunchPreparationStatus::WaveOutputConflict;
            preparation.detail =
                "JGRF Arguments already contain -o/--wave. Remove that "
                "configured writer before starting an explicit Goliath WAV export.";
            return preparation;
        }
    }

    fs::path resolvedExecutable;
    if (system == "neogeocd") {
        bool chdSupported = false;

        if (extension == ".chd") {
            resolvedExecutable = resolve_jollygood_executable(paths.jollygood_exe);
            if (resolvedExecutable.empty()) {
                preparation.status = LaunchPreparationStatus::ExecutableNotFound;
                preparation.detail =
                    "The configured Jollygood executable was not found: " +
                    path_to_utf8(paths.jollygood_exe);
                return preparation;
            }

            const GeolithCapabilities capabilities =
                probe_geolith_capabilities(resolvedExecutable);
            if (!capabilities.success) {
                preparation.status = LaunchPreparationStatus::CapabilityProbeFailed;
                preparation.detail = capabilities.error;
                return preparation;
            }

            preparation.advertised_formats =
                geolith_extensions_display(capabilities, "neogeocd");
            chdSupported =
                geolith_supports_extension(capabilities, "neogeocd", "chd");
            preparation.chd_supported = chdSupported;

            if (!chdSupported) {
                preparation.status = LaunchPreparationStatus::ChdUnsupported;
                preparation.detail =
                    "The installed Geolith core does not advertise CHD "
                    "support for Neo Geo CD. Advertised formats: " +
                    (preparation.advertised_formats.empty()
                         ? std::string("none")
                         : preparation.advertised_formats);
                return preparation;
            }
        }

        if (!media_is_launchable(system, media, chdSupported)) {
            preparation.status = LaunchPreparationStatus::UnsupportedMedia;
            preparation.detail =
                "The selected Neo Geo CD media type is unsupported: " + media;
            return preparation;
        }
    }

    if (resolvedExecutable.empty())
        resolvedExecutable = resolve_jollygood_executable(paths.jollygood_exe);
    if (resolvedExecutable.empty()) {
        preparation.status = LaunchPreparationStatus::ExecutableNotFound;
        preparation.detail =
            "The configured Jollygood executable was not found: " +
            path_to_utf8(paths.jollygood_exe);
        return preparation;
    }

    const QString executable = path_to_qstring(resolvedExecutable);

    QString launchConfigEnvironment =
        environmentPathForWorkingDirectory(paths.config_dir, paths.base_dir);
    const bool hasProfile = profile && !profile->empty();
    if (hasProfile) {
        const fs::path profileConfigRoot =
            paths.profile_runtime_dir / exact_media_storage_id(system, media);
        const QString profileConfigEnvironment =
            environmentPathForWorkingDirectory(profileConfigRoot,
                                               paths.base_dir);
        if (const auto pathError =
                perGameConfigPathError(profileConfigEnvironment)) {
            preparation.status =
                LaunchPreparationStatus::ProfileConfigurationFailed;
            preparation.detail = *pathError;
            return preparation;
        }

        const GameProfileRuntimeResult runtime =
            materialize_game_profile_runtime(paths, system, media, *profile);
        if (!runtime.success) {
            preparation.status = LaunchPreparationStatus::ProfileConfigurationFailed;
            preparation.detail = runtime.error;
            return preparation;
        }
        launchConfigEnvironment = profileConfigEnvironment;
        preparation.profile_applied = true;
        preparation.profile_config_root = path_to_qstring(runtime.config_root);
    }

    preparation.environment = QProcessEnvironment::systemEnvironment();
    preparation.environment.insert("XDG_CONFIG_HOME",
                                   launchConfigEnvironment);
    preparation.environment.insert(
        "XDG_DATA_HOME",
        environmentPathForWorkingDirectory(paths.data_dir, paths.base_dir));

    const QString mediaPath = path_to_qstring(preparation.media_path);
    QString wavePath;
    if (waveOutputPath.has_value())
        wavePath = path_to_qstring(*waveOutputPath);
    preparation.args = build_jollygood_launch_args(
        executable, paths.jollygood_args, system, mediaPath,
        hasProfile ? profile->video_api : std::nullopt,
        hasProfile ? profile->shader : std::nullopt,
        hasProfile
            ? profile_frontend_video_override(profile, "fullscreen")
            : std::nullopt,
        hasProfile
            ? profile_frontend_video_override(profile, "scale")
            : std::nullopt,
        verboseLogging,
        wavePath);
    preparation.working_directory = path_to_qstring(paths.base_dir);
    preparation.log_path = path_to_qstring(paths.base_dir / "jollygood.log");
    preparation.status = LaunchPreparationStatus::Ready;
    return preparation;
}

std::string launch_preparation_failure_text(
        const JollygoodLaunchPreparation& preparation) {
    if (preparation.status == LaunchPreparationStatus::Ready)
        return {};

    std::ostringstream message;
    message << "Reason: "
            << (preparation.detail.empty()
                    ? "Unknown launch preparation failure."
                    : preparation.detail);
    if (!preparation.media_path.empty()) {
        message << "\n\nMedia: " << path_to_utf8(preparation.media_path);
    }
    if (!preparation.effective_system.empty()) {
        message << "\nEffective system: " << preparation.effective_system;
    }
    if (preparation.status ==
        LaunchPreparationStatus::ProfileConfigurationFailed) {
        message << "\n\nThe global JGRF configuration was not modified.";
    } else if (preparation.status ==
               LaunchPreparationStatus::CapabilityProbeFailed) {
        message << "\n\nOpen Settings > Info and refresh the installed "
                   "JGRF/Geolith information.";
    }
    return message.str();
}

} // namespace goliath
