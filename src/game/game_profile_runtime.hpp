// game_profile_runtime.hpp — composes an isolated JGRF configuration tree
// from the current global INIs plus one Goliath per-game profile. XDG data is
// deliberately not isolated, so BIOS, saves, states and cheats stay shared.
#pragma once

#include <filesystem>
#include <string>

#include "game/video_settings.hpp"

namespace goliath {

struct AppPaths;
struct GameLaunchProfile;

struct GameProfileGlobalSettings {
    int video_api = 0;
    int cartridge_system = 0;
    int cd_system = 2;
    int universe_hardware = 1;
    int region = 0;
    int input_device = 0;
    int free_play = 0;
    int setting_mode = 0;
    int shader = 2;

    // Effective values after JGRF's settings.ini -> geolith.ini precedence.
    // API/shader keep their established named members above; the JGRF map
    // contains every remaining frontend video setting.
    VideoSettingValues jgrf_video;
    VideoSettingValues geolith_video;
};

GameProfileGlobalSettings load_game_profile_global_settings(const AppPaths& paths);

int effective_cartridge_system(const GameLaunchProfile& profile,
                               const GameProfileGlobalSettings& global);
int effective_universe_hardware(const GameLaunchProfile& profile,
                                const GameProfileGlobalSettings& global);
int effective_region(const GameLaunchProfile& profile,
                     const GameProfileGlobalSettings& global);

// Free Play and Setting Mode are meaningful only on MVS hardware. Universe
// BIOS configured for MVS hardware is treated as MVS; CD is never MVS here.
bool game_profile_uses_mvs_hardware(const std::string& system,
                                    const GameLaunchProfile& profile,
                                    const GameProfileGlobalSettings& global);

// Geolith's 4-player board is stricter: direct MVS plus JP or AS region.
bool game_profile_supports_four_player(const std::string& system,
                                       const GameLaunchProfile& profile,
                                       const GameProfileGlobalSettings& global);

struct GameProfileRuntimeResult {
    bool success = false;
    std::filesystem::path config_root; // value assigned to XDG_CONFIG_HOME
    std::string error;
};

GameProfileRuntimeResult materialize_game_profile_runtime(
    const AppPaths& paths,
    const std::string& system,
    const std::string& media,
    const GameLaunchProfile& profile);

} // namespace goliath
