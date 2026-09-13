#include "paths.hpp"

#include "common/goliath_common.hpp"

#include <sstream>

namespace fs = std::filesystem;

namespace goliath {

namespace {
// Splits on any run of whitespace, discarding empty tokens (used to parse
// "jollygood_args" from goliath.ini into a plain argv-style list).
std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}
} // namespace

AppPaths compute_app_paths(const Config& config) {
    AppPaths p;
    p.base_dir = get_base_dir();
    p.config_dir = resolve_path(config, "config");

    p.json_file = resolve_path(config, "database") / "games.json";

    p.jollygood_exe = resolve_path(config, "jollygood");
    p.jollygood_args = split_whitespace(config.get("Paths", "jollygood_args", "-c geolith"));

    p.bios_dir = resolve_path(config, "bios");
    p.data_dir = p.base_dir / "data";

    const fs::path jollygoodConfigDir = p.config_dir / "jollygood";
    p.jollygood_settings_ini = jollygoodConfigDir / "settings.ini";
    p.geolith_ini = jollygoodConfigDir / "geolith.ini";
    p.input_config = jollygoodConfigDir / "geolith_input.ini";
    p.game_profiles_json = p.config_dir / "game_profiles.json";
    p.profile_runtime_dir = p.config_dir / "p";
    p.game_playtime_json = p.config_dir / "game_playtime.json";
    p.save_backup_dir = p.data_dir / "goliath" / "save_backups";
    p.audio_export_dir = p.data_dir / "goliath" / "audio_exports";

    return p;
}

} // namespace goliath
