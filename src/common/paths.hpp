// paths.hpp — all filesystem locations derived from the [Paths] section of
// goliath.ini, computed once at startup via compute_app_paths() and reused
// throughout the app (games.json location, jollygood executable/args,
// assets/bios folders, Goliath profile/playtime/audio-export state, and the
// jollygood/geolith config file paths).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace goliath {

class Config;

struct AppPaths {
    std::filesystem::path base_dir;
    std::filesystem::path config_dir; // resolve_path(config, "config") - used as XDG_CONFIG_HOME
    std::filesystem::path json_file; // <database>/games.json

    std::filesystem::path jollygood_exe;
    std::vector<std::string> jollygood_args;

    std::filesystem::path bios_dir;   // resolve_path(config, "bios") - user-configurable
    std::filesystem::path data_dir;   // <base_dir>/data - used as XDG_DATA_HOME
                                      // and for Goliath's own data (junction markers,
                                      // jollygood config/save). The actual BIOS location
                                      // depends on the launch layout: dynamic cores read
                                      // from <data>/jollygood/bios; static cores use
                                      // <jollygood_exe_dir>/bios.

    std::filesystem::path jollygood_settings_ini; // .../settings.ini
    std::filesystem::path geolith_ini;             // .../geolith.ini
    std::filesystem::path input_config;            // .../geolith_input.ini

    // Goliath-owned per-game profile state. These stay outside games.json so
    // rescans cannot remove them. Runtime folders use short hashes because
    // JGRF 2.0.1 has a fixed 128-byte configuration-path buffer.
    std::filesystem::path game_profiles_json;       // <config>/game_profiles.json
    std::filesystem::path profile_runtime_dir;      // <config>/p/<short hash>/...

    // Goliath-owned exact-media playtime statistics. This remains outside
    // scanner-generated games.json so a rescan cannot erase it.
    std::filesystem::path game_playtime_json;       // <config>/game_playtime.json

    // Goliath-owned manual save-data snapshots. Each exact media identity has
    // a short subdirectory containing validated ZIP archives and manifests.
    std::filesystem::path save_backup_dir;          // <data>/goliath/save_backups

    // Suggested destination for one-shot JGRF WAV captures. Users can choose
    // another directory in the save dialog; the selection is never persisted.
    std::filesystem::path audio_export_dir;          // <data>/goliath/audio_exports
};

AppPaths compute_app_paths(const Config& config);

} // namespace goliath
