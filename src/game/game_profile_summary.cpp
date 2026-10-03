#include "game/game_profile_summary.hpp"

#include "game/game_profile.hpp"
#include "game/game_profile_runtime.hpp"
#include "game/video_settings.hpp"

#include <string>
#include <string_view>

namespace goliath {
namespace {

std::string cartridge_system_label(int value) {
    switch (value) {
    case 0: return "AES (Console)";
    case 1: return "MVS (Arcade)";
    case 2: return "Universe BIOS";
    default: return "Unknown";
    }
}

std::string cd_system_label(int value) {
    switch (value) {
    case 0: return "Neo Geo CD (Front Loader)";
    case 1: return "Neo Geo CD (Top Loader)";
    case 2: return "Neo Geo CDZ";
    case 3: return "Universe BIOS";
    default: return "Unknown";
    }
}

std::string universe_hardware_label(int value) {
    return value == 0 ? "AES" : value == 1 ? "MVS" : "Unknown";
}

std::string region_label(int value) {
    switch (value) {
    case 0: return "US";
    case 1: return "JP";
    case 2: return "AS";
    case 3: return "EU";
    default: return "Unknown";
    }
}

std::string input_device_label(int value) {
    switch (value) {
    case 0: return "Auto";
    case 1: return "Neo Geo Joysticks";
    case 2: return "Mahjong";
    case 3: return "4-Player";
    default: return "Unknown";
    }
}

std::string video_option_label(std::string_view key, int value) {
    const VideoSettingSpec* spec =
        find_video_setting(jgrf_video_setting_specs(), key);
    if (!spec) return "Unknown";
    for (const VideoSettingOption& option : spec->options) {
        if (option.value == value) return option.label;
    }
    return "Unknown";
}

} // namespace

GameProfileSummary summarize_game_profile(
    const std::string& system,
    const GameLaunchProfile* profile,
    const GameProfileGlobalSettings& global) {
    const GameLaunchProfile inherited;
    const GameLaunchProfile& effective = profile ? *profile : inherited;

    GameProfileSummary summary;
    summary.custom = profile && !profile->empty();

    const int region = effective.region.value_or(global.region);
    if (system == "neogeo") {
        const int cartridge =
            effective.cartridge_system.value_or(global.cartridge_system);
        summary.system = cartridge_system_label(cartridge);
        if (cartridge == 2) {
            const int hardware = effective.universe_hardware.value_or(
                global.universe_hardware);
            summary.system += " / " + universe_hardware_label(hardware);
        }
        summary.system += " / " + region_label(region);
    } else if (system == "neogeocd") {
        const int cd = effective.cd_system.value_or(global.cd_system);
        summary.system = cd_system_label(cd) + " / " + region_label(region);
    } else {
        summary.system = "Unknown";
    }

    const int videoApi = effective.video_api.value_or(global.video_api);
    const int shader = effective.shader.value_or(global.shader);
    // Keep API and shader on separate lines. The Launch Profile column is
    // intentionally compact, and a word-wrapped final token can otherwise be
    // painted outside the single-line form-row height and appear missing.
    summary.video = video_option_label("api", videoApi) + "\n" +
                    video_option_label("shader", shader);

    const int input = effective.input_device.value_or(global.input_device);
    summary.input = input_device_label(input);
    if (effective.controller_port.has_value()) {
        summary.input += " / Controller port " +
                         std::to_string(*effective.controller_port + 1);
    } else {
        summary.input += " / Global mappings";
    }
    if (!effective.input_overrides.empty())
        summary.input += " / Custom bindings";

    return summary;
}

} // namespace goliath
