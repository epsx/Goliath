#include "game/game_profile_runtime.hpp"

#include "common/filesystem_safety.hpp"
#include "common/paths.hpp"
#include "game/game_profile.hpp"
#include "ini/ini_document.hpp"
#include "input/input_maps.hpp"

#include <array>
#include <cctype>

namespace fs = std::filesystem;

namespace goliath {

namespace {

int ini_int(const IniDocument& document,
            const std::string& section,
            const std::string& key,
            int fallback,
            int minimum,
            int maximum) {
    if (!document.has_option(section, key)) return fallback;
    try {
        const int value = std::stoi(document.get(section, key));
        return value >= minimum && value <= maximum ? value : fallback;
    } catch (...) {
        return fallback;
    }
}

int ini_video_value(const IniDocument& document,
                    const std::string& section,
                    const VideoSettingSpec& spec,
                    int fallback) {
    if (!document.has_option(section, spec.key)) return fallback;
    try {
        const int value = std::stoi(document.get(section, spec.key));
        return is_valid_video_setting_value(spec, value) ? value : fallback;
    } catch (...) {
        return fallback;
    }
}

bool copy_regular_file_if_present(const fs::path& source,
                                  const fs::path& destination,
                                  std::string* error) {
    const DirectPathInspection destinationInspection =
        inspect_direct_path(destination);
    if (destinationInspection.kind == DirectPathKind::Error) {
        if (error) {
            *error = "Could not inspect runtime configuration " +
                     destination.string() + ": " +
                     destinationInspection.error;
        }
        return false;
    }
    const bool destinationExists =
        destinationInspection.kind == DirectPathKind::RegularFile;
    if (destinationInspection.kind != DirectPathKind::Missing &&
        !destinationExists) {
        if (error) {
            *error = "Runtime configuration target is not a direct regular "
                     "file: " + destination.string();
        }
        return false;
    }

    std::error_code ec;
    const bool sourceExists = fs::exists(source, ec);
    if (ec) {
        if (error) *error = "Could not inspect " + source.string() + ": " + ec.message();
        return false;
    }
    if (!sourceExists) {
        // A profile directory is reused across launches. Do not let a file
        // inherited during an earlier launch survive after its global source
        // has been removed.
        if (destinationExists && !fs::remove(destination, ec)) {
            if (error) {
                *error = "Could not remove stale runtime configuration " +
                         destination.string();
                if (ec) *error += ": " + ec.message();
            }
            return false;
        }
        return true;
    }
    if (!fs::is_regular_file(source, ec)) {
        if (error) *error = "Configuration source is not a regular file: " + source.string();
        return false;
    }

    // Runtime files are disposable. Removing a confirmed regular file first
    // lets copy_file use its non-overwriting mode; a substituted object that
    // appears afterward causes failure instead of being followed.
    if (destinationExists && !fs::remove(destination, ec)) {
        if (error) {
            *error = "Could not replace runtime configuration " +
                     destination.string();
            if (ec) *error += ": " + ec.message();
        }
        return false;
    }
    ec.clear();
    fs::copy_file(source, destination, fs::copy_options::none, ec);
    if (ec) {
        if (error) {
            *error = "Could not copy " + source.string() + ": " + ec.message();
        }
        return false;
    }
    const DirectPathInspection copied = inspect_direct_path(destination);
    if (copied.kind != DirectPathKind::RegularFile) {
        if (error) {
            *error = "Copied runtime configuration is not a direct regular "
                     "file: " + destination.string();
        }
        return false;
    }
    return true;
}

bool save_and_verify(const IniDocument& document,
                     const fs::path& destination,
                     std::string* error) {
    // Runtime trees are disposable. Remove an earlier copy first so a failed
    // save can never look successful merely because a stale regular file is
    // still present from the previous launch.
    const DirectPathInspection inspection = inspect_direct_path(destination);
    if (inspection.kind == DirectPathKind::Error) {
        if (error) {
            *error = "Could not inspect runtime configuration " +
                     destination.string() + ": " + inspection.error;
        }
        return false;
    }
    if (inspection.kind != DirectPathKind::Missing &&
        inspection.kind != DirectPathKind::RegularFile) {
        if (error) {
            *error = "Runtime configuration target is not a direct regular "
                     "file: " + destination.string();
        }
        return false;
    }

    std::error_code ec;
    if (inspection.kind == DirectPathKind::RegularFile &&
        !fs::remove(destination, ec)) {
        if (error) {
            *error = "Could not replace runtime configuration " +
                     destination.string();
            if (ec) *error += ": " + ec.message();
        }
        return false;
    }

    if (!document.save(destination, error)) return false;
    const DirectPathInspection saved = inspect_direct_path(destination);
    if (saved.kind != DirectPathKind::RegularFile) {
        if (error) *error = "Could not write runtime configuration: " + destination.string();
        return false;
    }
    return true;
}

bool load_input_config_or_defaults(const fs::path& source,
                                   IniDocument* input,
                                   std::string* error) {
    std::error_code ec;
    const bool exists = fs::exists(source, ec);
    if (ec) {
        if (error) *error = "Could not inspect " + source.string() + ": " + ec.message();
        return false;
    }
    if (exists) {
        if (!fs::is_regular_file(source, ec) || ec) {
            if (error) *error = "Input configuration is not a regular file: " + source.string();
            return false;
        }
        input->load(source);
        return true;
    }

    // Match the global Input editor's fallback when no INI has been saved
    // yet, so adding one per-game override does not discard the built-in
    // keyboard mappings for every unchanged control.
    for (const auto& [section, keys] : default_input_config()) {
        input->ensure_section(section);
        for (const auto& [key, value] : keys)
            input->set(section, key, value);
    }
    return true;
}

void set_canonical_input_binding(IniDocument& input,
                                 const std::string& section,
                                 const std::string& key,
                                 const std::string& value) {
    input.ensure_section(section);
    for (const std::string& rawKey : input.options(section)) {
        if (canonical_input_key(section, rawKey) == key)
            input.remove_option(section, rawKey);
    }
    input.set(section, key, value);
}

bool has_runtime_ini_overrides(const GameLaunchProfile& profile) {
    return profile.cartridge_system.has_value() ||
           profile.cd_system.has_value() ||
           profile.universe_hardware.has_value() ||
           profile.region.has_value() ||
           profile.input_device.has_value() ||
           profile.free_play.has_value() ||
           profile.setting_mode.has_value() ||
           !profile.jgrf_video.empty() ||
           !profile.geolith_video.empty();
}

void apply_video_overrides(
        IniDocument& core,
        const std::string& section,
        const VideoSettingValues& values,
        const std::vector<VideoSettingSpec>& specs,
        bool skipCliSettings = false) {
    for (const auto& [key, value] : values) {
        if (skipCliSettings && (key == "api" || key == "shader"))
            continue;
        const VideoSettingSpec* spec = find_video_setting(specs, key);
        if (!spec || !is_valid_video_setting_value(*spec, value))
            continue;
        core.set(section, key, std::to_string(value));
    }
}

bool is_player_one_input_section(const std::string& section) {
    static constexpr std::array<const char*, 5> sections = {
        "neogeojs1",
        "neogeomahjong",
        "neogeovliner",
        "neogeoirrmaze",
        "neogeosystem",
    };
    for (const char* candidate : sections) {
        if (section == candidate) return true;
    }
    return false;
}

std::string retarget_joystick_binding(std::string value, int port) {
    // JGRF 2.0.1 parses the SDL joystick port from the single digit directly
    // after 'j' (j0b1, j0a1-, j0h00). Keyboard and mouse mappings are left
    // untouched.
    if (value.size() >= 3 && value[0] == 'j' &&
        std::isdigit(static_cast<unsigned char>(value[1]))) {
        value[1] = static_cast<char>('0' + port);
    }
    return value;
}

void retarget_player_one_bindings(IniDocument& input, int port) {
    for (const std::string& section : input.sections()) {
        if (!is_player_one_input_section(section)) continue;
        for (const std::string& key : input.options(section)) {
            const std::string current = input.get(section, key);
            input.set(section, key, retarget_joystick_binding(current, port));
        }
    }
}

} // namespace

GameProfileGlobalSettings load_game_profile_global_settings(const AppPaths& paths) {
    GameProfileGlobalSettings global;

    IniDocument core;
    core.load(paths.geolith_ini);
    global.cartridge_system = ini_int(core, "geolith", "system", 0, 0, 2);
    global.cd_system = ini_int(core, "geolith", "cdsystem", 2, 0, 3);
    global.universe_hardware = ini_int(core, "geolith", "unihw", 1, 0, 1);
    global.region = ini_int(core, "geolith", "region", 0, 0, 3);
    global.input_device = ini_int(core, "geolith", "input", 0, 0, 3);
    global.free_play = ini_int(core, "geolith", "freeplay", 0, 0, 1);
    global.setting_mode = ini_int(core, "geolith", "settingmode", 0, 0, 1);

    IniDocument frontend;
    frontend.load(paths.jollygood_settings_ini);
    for (const VideoSettingSpec& spec : jgrf_video_setting_specs()) {
        int value = ini_video_value(
            frontend, "video", spec, spec.default_value);
        // JGRF applies a valid frontend section in geolith.ini after
        // settings.ini and retains the prior value for an invalid override.
        value = ini_video_value(core, "video", spec, value);
        if (std::string_view(spec.key) == "api") {
            global.video_api = value;
        } else if (std::string_view(spec.key) == "shader") {
            global.shader = value;
        } else {
            global.jgrf_video.emplace(spec.key, value);
        }
    }

    for (const VideoSettingSpec& spec : geolith_video_setting_specs()) {
        global.geolith_video.emplace(
            spec.key,
            ini_video_value(core, "geolith", spec, spec.default_value));
    }

    return global;
}

int effective_cartridge_system(const GameLaunchProfile& profile,
                               const GameProfileGlobalSettings& global) {
    return profile.cartridge_system.value_or(global.cartridge_system);
}

int effective_universe_hardware(const GameLaunchProfile& profile,
                                const GameProfileGlobalSettings& global) {
    return profile.universe_hardware.value_or(global.universe_hardware);
}

int effective_region(const GameLaunchProfile& profile,
                     const GameProfileGlobalSettings& global) {
    return profile.region.value_or(global.region);
}

bool game_profile_uses_mvs_hardware(const std::string& system,
                                    const GameLaunchProfile& profile,
                                    const GameProfileGlobalSettings& global) {
    if (system != "neogeo") return false;
    const int cartridgeSystem = effective_cartridge_system(profile, global);
    return cartridgeSystem == 1 ||
           (cartridgeSystem == 2 && effective_universe_hardware(profile, global) == 1);
}

bool game_profile_supports_four_player(const std::string& system,
                                       const GameLaunchProfile& profile,
                                       const GameProfileGlobalSettings& global) {
    if (system != "neogeo" || effective_cartridge_system(profile, global) != 1)
        return false;
    const int region = effective_region(profile, global);
    return region == 1 || region == 2;
}

GameProfileRuntimeResult materialize_game_profile_runtime(
        const AppPaths& paths,
        const std::string& system,
        const std::string& media,
        const GameLaunchProfile& profile) {
    GameProfileRuntimeResult result;
    if ((system != "neogeo" && system != "neogeocd") || media.empty()) {
        result.error = "Invalid system or media for per-game profile.";
        return result;
    }

    result.config_root =
        paths.profile_runtime_dir / exact_media_storage_id(system, media);
    const fs::path runtimeJollygood = result.config_root / "jollygood";

    for (const fs::path& directory : {
             paths.profile_runtime_dir, result.config_root, runtimeJollygood}) {
        if (!ensure_direct_directory(directory, &result.error)) {
            result.error = "Could not prepare per-game configuration tree: " +
                           result.error;
            return result;
        }
    }

    const fs::path runtimeSettings = runtimeJollygood / "settings.ini";
    if (!copy_regular_file_if_present(paths.jollygood_settings_ini,
                                      runtimeSettings, &result.error)) {
        return result;
    }

    const fs::path runtimeCore = runtimeJollygood / "geolith.ini";
    if (has_runtime_ini_overrides(profile)) {
        IniDocument core;
        core.load(paths.geolith_ini);

        if (system == "neogeo" && profile.cartridge_system.has_value()) {
            core.set("geolith", "system", std::to_string(*profile.cartridge_system));
        }
        if (system == "neogeocd" && profile.cd_system.has_value()) {
            core.set("geolith", "cdsystem", std::to_string(*profile.cd_system));
        }
        if (system == "neogeo" && profile.universe_hardware.has_value()) {
            core.set("geolith", "unihw", std::to_string(*profile.universe_hardware));
        }
        if (profile.region.has_value()) {
            core.set("geolith", "region", std::to_string(*profile.region));
        }

        const GameProfileGlobalSettings global =
            load_game_profile_global_settings(paths);
        const int requestedInput =
            profile.input_device.value_or(global.input_device);
        int input = requestedInput;
        if (system == "neogeocd" && input > 1)
            input = 0;
        if (input == 3 &&
            !game_profile_supports_four_player(system, profile, global)) {
            input = 0;
        }
        if (profile.input_device.has_value() || input != requestedInput) {
            core.set("geolith", "input", std::to_string(input));
        }

        const bool mvsHardware =
            game_profile_uses_mvs_hardware(system, profile, global);
        if (profile.free_play.has_value() || !mvsHardware) {
            core.set("geolith", "freeplay",
                     profile.free_play.value_or(false) && mvsHardware ? "1" : "0");
        }
        if (profile.setting_mode.has_value() || !mvsHardware) {
            core.set("geolith", "settingmode",
                     profile.setting_mode.value_or(false) && mvsHardware ? "1" : "0");
        }

        // JGRF reads [video] from the core-specific file after settings.ini.
        // API and shader remain final command-line overrides so Feature 11's
        // launch ordering and Vulkan capability checks stay unchanged.
        // Fullscreen/scale are kept here for an inspectable complete runtime
        // profile and are also appended as CLI overrides to defeat conflicting
        // global launch arguments.
        apply_video_overrides(
            core, "video", profile.jgrf_video,
            jgrf_video_setting_specs(), true);
        apply_video_overrides(
            core, "geolith", profile.geolith_video,
            geolith_video_setting_specs());

        if (!save_and_verify(core, runtimeCore, &result.error)) return result;
    } else if (!copy_regular_file_if_present(paths.geolith_ini,
                                             runtimeCore, &result.error)) {
        return result;
    }

    const fs::path runtimeInput = runtimeJollygood / "geolith_input.ini";
    if (profile.controller_port.has_value() ||
        !profile.input_overrides.empty()) {
        IniDocument input;
        if (!load_input_config_or_defaults(
                paths.input_config, &input, &result.error)) {
            return result;
        }
        if (profile.controller_port.has_value())
            retarget_player_one_bindings(input, *profile.controller_port);

        // Explicit per-game mappings are applied last and retain their
        // literal captured jN port. controller_port retargets inherited
        // Player 1 bindings only.
        for (const auto& [section, keys] : profile.input_overrides) {
            for (const auto& [key, value] : keys) {
                const std::string canonicalKey =
                    canonical_input_key(section, key);
                const auto stored = normalize_input_binding_for_definition(
                    section, canonicalKey, value);
                if (!stored.has_value()) continue;
                set_canonical_input_binding(
                    input, section, canonicalKey, *stored);
            }
        }
        if (!save_and_verify(input, runtimeInput, &result.error)) return result;
    } else if (!copy_regular_file_if_present(paths.input_config,
                                             runtimeInput, &result.error)) {
        return result;
    }

    result.success = true;
    return result;
}

} // namespace goliath
