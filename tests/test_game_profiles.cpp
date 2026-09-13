#include "catch2/catch.hpp"

#include "common/filesystem_safety.hpp"
#include "common/paths.hpp"
#include "game/game_profile.hpp"
#include "game/game_profile_runtime.hpp"
#include "ini/ini_document.hpp"

#if defined(_WIN32)
#include <QProcess>
#include <QString>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

using goliath::AppPaths;
using goliath::DirectPathKind;
using goliath::GameLaunchProfile;
using goliath::GameProfileGlobalSettings;
using goliath::GameProfileStore;
using goliath::IniDocument;
using goliath::effective_cartridge_system;
using goliath::game_profile_supports_four_player;
using goliath::game_profile_uses_mvs_hardware;
using goliath::make_game_profile_key;
using goliath::materialize_game_profile_runtime;
using goliath::normalize_game_profile_media;
using goliath::inspect_direct_path;

namespace {

fs::path make_profile_test_root(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("gp_" + name + "_" + std::to_string(stamp));
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

AppPaths make_profile_paths(const fs::path& root) {
    AppPaths paths;
    paths.base_dir = root;
    paths.config_dir = root / "config";
    paths.data_dir = root / "data";
    paths.jollygood_settings_ini = paths.config_dir / "jollygood" / "settings.ini";
    paths.geolith_ini = paths.config_dir / "jollygood" / "geolith.ini";
    paths.input_config = paths.config_dir / "jollygood" / "geolith_input.ini";
    paths.game_profiles_json = paths.config_dir / "game_profiles.json";
    paths.profile_runtime_dir = paths.config_dir / "p";
    return paths;
}

bool create_directory_link(const fs::path& link,
                           const fs::path& target) {
#if defined(_WIN32)
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(
        "cmd.exe",
        {"/d", "/c", "mklink", "/J",
         QString::fromStdWString(link.wstring()),
         QString::fromStdWString(target.wstring())});
    return process.waitForStarted(5000) && process.waitForFinished(5000) &&
           process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
#else
    std::error_code ec;
    fs::create_directory_symlink(target, link, ec);
    return !ec;
#endif
}

bool remove_directory_link(const fs::path& link) {
#if defined(_WIN32)
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(
        "cmd.exe",
        {"/d", "/c", "rmdir",
         QString::fromStdWString(link.wstring())});
    return process.waitForStarted(5000) && process.waitForFinished(5000) &&
           process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
#else
    std::error_code ec;
    return fs::remove(link, ec) && !ec;
#endif
}

} // namespace

TEST_CASE("per-game profile keys distinguish every launchable media",
          "[profiles][key]") {
    REQUIRE(normalize_game_profile_media("sets\\kof98.neo") == "sets/kof98.neo");
    REQUIRE(normalize_game_profile_media("./sets/../kof98.neo") == "kof98.neo");

    const std::string parent = make_game_profile_key("neogeo", "shocktr2.neo");
    const std::string variant = make_game_profile_key("neogeo", "lans2004.neo");
    const std::string cue = make_game_profile_key("neogeocd", "rbff2/game.cue");
    const std::string chd = make_game_profile_key("neogeocd", "rbff2/game.chd");

    REQUIRE(parent != variant);
    REQUIRE(cue != chd);
    REQUIRE(parent != cue);
}

TEST_CASE("shared video schema matches JGRF and Geolith contracts",
          "[profiles][video][schema]") {
    const auto& frontend = goliath::jgrf_video_setting_specs();
    const auto& core = goliath::geolith_video_setting_specs();
    REQUIRE(frontend.size() == 12);
    REQUIRE(core.size() == 6);

    const auto* scale = goliath::find_video_setting(frontend, "scale");
    const auto* crteaMode =
        goliath::find_video_setting(frontend, "crtea_mode");
    const auto* scanline =
        goliath::find_video_setting(frontend, "crtea_scanstr");
    const auto* overscan =
        goliath::find_video_setting(core, "overscan_r");
    REQUIRE(scale != nullptr);
    REQUIRE(crteaMode != nullptr);
    REQUIRE(scanline != nullptr);
    REQUIRE(overscan != nullptr);
    REQUIRE(scale->default_value == 3);
    REQUIRE(goliath::is_valid_video_setting_value(*scale, 1));
    REQUIRE(goliath::is_valid_video_setting_value(*scale, 8));
    REQUIRE_FALSE(goliath::is_valid_video_setting_value(*scale, 0));
    REQUIRE(goliath::is_valid_video_setting_value(*crteaMode, 4));
    REQUIRE_FALSE(goliath::is_valid_video_setting_value(*crteaMode, 5));
    REQUIRE(goliath::is_valid_video_setting_value(*scanline, 10));
    REQUIRE(goliath::is_valid_video_setting_value(*overscan, 4));
    REQUIRE_FALSE(goliath::is_valid_video_setting_value(*overscan, 5));
}

TEST_CASE("per-game profiles round-trip outside games.json",
          "[profiles][persistence]") {
    const fs::path root = make_profile_test_root("roundtrip");
    const fs::path path = root / "config" / "game_profiles.json";

    GameLaunchProfile parent;
    parent.cartridge_system = 1;
    parent.region = 1;
    parent.video_api = 3;
    parent.shader = 0;
    parent.jgrf_video = {
        {"fullscreen", 1},
        {"scale", 6},
        {"crtea_mode", 4},
        {"crtea_scanstr", 8},
    };
    parent.geolith_video = {
        {"aspect", 2},
        {"palette", 1},
        {"overscan_l", 1},
    };
    parent.free_play = true;
    parent.controller_port = 2;
    parent.input_overrides["neogeojs1"]["a"] = " space ";
    parent.input_overrides["neogeojs1"]["Up"] = "j2h00";
    parent.input_overrides["neogeojs1"]["Bogus"] = "j0b1";
    parent.input_overrides["neogeojs1"]["B"] = "j0b32";

    GameLaunchProfile variant;
    variant.cartridge_system = 2;
    variant.universe_hardware = 1;

    GameLaunchProfile cue;
    cue.cd_system = 0;

    GameLaunchProfile chd;
    chd.cd_system = 2;

    GameProfileStore store;
    store.set("neogeo", "shocktr2.neo", parent);
    store.set("neogeo", "lans2004.neo", variant);
    store.set("neogeocd", "rbff2/game.cue", cue);
    store.set("neogeocd", "rbff2/game.chd", chd);
    REQUIRE(store.size() == 4);

    const std::string previous = "old profile contents\n";
    write_text(path, previous);

    std::string error;
    REQUIRE(store.save(path, &error));
    REQUIRE(error.empty());
    CHECK(read_text(path) != previous);

    GameProfileStore loaded;
    REQUIRE(loaded.load(path, &error));
    REQUIRE(error.empty());
    REQUIRE(loaded.size() == 4);

    const GameLaunchProfile* loadedParent =
        loaded.find("neogeo", "shocktr2.neo");
    REQUIRE(loadedParent != nullptr);
    REQUIRE(loadedParent->cartridge_system == std::optional<int>(1));
    REQUIRE(loadedParent->region == std::optional<int>(1));
    REQUIRE(loadedParent->video_api == std::optional<int>(3));
    REQUIRE(loadedParent->shader == std::optional<int>(0));
    REQUIRE(loadedParent->jgrf_video.at("fullscreen") == 1);
    REQUIRE(loadedParent->jgrf_video.at("scale") == 6);
    REQUIRE(loadedParent->jgrf_video.at("crtea_mode") == 4);
    REQUIRE(loadedParent->jgrf_video.at("crtea_scanstr") == 8);
    REQUIRE(loadedParent->geolith_video.at("aspect") == 2);
    REQUIRE(loadedParent->geolith_video.at("palette") == 1);
    REQUIRE(loadedParent->geolith_video.at("overscan_l") == 1);
    REQUIRE(loadedParent->free_play == std::optional<bool>(true));
    REQUIRE(loadedParent->controller_port == std::optional<int>(2));
    REQUIRE(loadedParent->input_overrides.at("neogeojs1").at("A") == "44");
    REQUIRE(loadedParent->input_overrides.at("neogeojs1").at("Up") ==
            "j2h00");
    REQUIRE_FALSE(loadedParent->input_overrides.at("neogeojs1").contains(
        "Bogus"));
    REQUIRE_FALSE(loadedParent->input_overrides.at("neogeojs1").contains(
        "B"));

    REQUIRE(loaded.find("neogeo", "lans2004.neo") != nullptr);
    REQUIRE(loaded.find("neogeocd", "rbff2/game.cue")->cd_system ==
            std::optional<int>(0));
    REQUIRE(loaded.find("neogeocd", "rbff2/game.chd")->cd_system ==
            std::optional<int>(2));

    loaded.set("neogeo", "shocktr2.neo", GameLaunchProfile{});
    REQUIRE(loaded.size() == 3);
    REQUIRE(loaded.find("neogeo", "shocktr2.neo") == nullptr);

    fs::remove_all(root);
}

TEST_CASE("per-game profiles recover after a blocked save destination",
          "[profiles][persistence][recovery]") {
    const fs::path root = make_profile_test_root("save_recovery");
    const fs::path blocker = root / "not_a_directory";
    const fs::path path = blocker / "game_profiles.json";
    const std::string sentinel = "keep blocker bytes unchanged\n";
    write_text(blocker, sentinel);

    GameLaunchProfile profile;
    profile.shader = 4;
    GameProfileStore store;
    store.set("neogeo", "kof98.neo", profile);
    REQUIRE(store.size() == 1);

    std::string error;
    CHECK_FALSE(store.save(path, &error));
    CHECK(error.find("Could not create profile directory:") == 0);
    CHECK(read_text(blocker) == sentinel);

    REQUIRE(fs::remove(blocker));
    REQUIRE(store.save(path, &error));
    CHECK(error.empty());
    REQUIRE(fs::is_regular_file(path));

    GameProfileStore recovered;
    REQUIRE(recovered.load(path, &error));
    CHECK(error.empty());
    REQUIRE(recovered.size() == 1);
    const GameLaunchProfile* loaded =
        recovered.find("neogeo", "kof98.neo");
    REQUIRE(loaded != nullptr);
    CHECK(loaded->shader == std::optional<int>(4));

    fs::remove_all(root);
}

TEST_CASE("invalid per-game frontend values are ignored individually",
          "[profiles][persistence][video]") {
    const fs::path root = make_profile_test_root("invalid_video");
    const fs::path path = root / "game_profiles.json";
    write_text(path, R"json({
  "version": 1,
  "profiles": [
    {
      "system": "neogeo",
      "media": "kof98.neo",
      "overrides": {
        "video_api": 9,
        "shader": 0,
        "jgrf_video": {
          "api": 2,
          "fullscreen": 1,
          "scale": 0,
          "crtea_scanstr": 10,
          "unknown": 4
        },
        "geolith_video": {
          "aspect": 9,
          "palette": 1,
          "overscan_t": -1,
          "overscan_r": 4
        }
      }
    },
    {
      "system": "neogeocd",
      "media": "disc.chd",
      "overrides": {
        "video_api": 2
      }
    }
  ]
})json");

    GameProfileStore store;
    std::string error;
    REQUIRE(store.load(path, &error));
    REQUIRE(error.empty());

    const GameLaunchProfile* profile =
        store.find("neogeo", "kof98.neo");
    REQUIRE(profile != nullptr);
    REQUIRE_FALSE(profile->video_api.has_value());
    REQUIRE(profile->shader == std::optional<int>(0));
    REQUIRE((profile->jgrf_video == goliath::VideoSettingValues{
        {"crtea_scanstr", 10}, {"fullscreen", 1}}));
    REQUIRE((profile->geolith_video == goliath::VideoSettingValues{
        {"overscan_r", 4}, {"palette", 1}}));

    REQUIRE(store.size() == 2);
    const GameLaunchProfile* apiOnly =
        store.find("neogeocd", "disc.chd");
    REQUIRE(apiOnly != nullptr);
    REQUIRE(apiOnly->video_api == std::optional<int>(2));
    REQUIRE_FALSE(apiOnly->empty());

    fs::remove_all(root);
}

TEST_CASE("malformed profile files fail closed",
          "[profiles][persistence]") {
    const fs::path root = make_profile_test_root("malformed");
    const fs::path path = root / "game_profiles.json";
    write_text(path, R"json({"version":99,"profiles":[]})json");

    GameProfileStore store;
    GameLaunchProfile profile;
    profile.shader = 0;
    store.set("neogeo", "kof98.neo", profile);
    REQUIRE(store.size() == 1);

    std::string error;
    REQUIRE_FALSE(store.load(path, &error));
    REQUIRE_FALSE(error.empty());
    REQUIRE(store.size() == 0);

    fs::remove_all(root);
}

TEST_CASE("stored per-game input overrides discard unsafe entries",
          "[profiles][persistence][input]") {
    const fs::path root = make_profile_test_root("stored_input");
    const fs::path path = root / "game_profiles.json";
    write_text(path, R"json({
  "version": 1,
  "profiles": [
    {
      "system": "neogeo",
      "media": "unsafe.neo",
      "overrides": {
        "input_overrides": {
          "neogeojs1": {
            "A": "j0b31",
            "B": "j0b32",
            "Unknown": "4"
          },
          "neogeoirrmaze": {
            "XAxis": "j1a5",
            "YAxis": "j1a5+"
          },
          "unknown": {
            "A": "4"
          }
        }
      }
    }
  ]
})json");

    GameProfileStore store;
    std::string error;
    REQUIRE(store.load(path, &error));
    REQUIRE(error.empty());

    const GameLaunchProfile* profile =
        store.find("neogeo", "unsafe.neo");
    REQUIRE(profile != nullptr);
    REQUIRE(profile->input_overrides.at("neogeojs1").at("A") == "j0b31");
    REQUIRE(profile->input_overrides.at("neogeoirrmaze").at("XAxis") ==
            "j1a5");
    REQUIRE_FALSE(profile->input_overrides.at("neogeojs1").contains("B"));
    REQUIRE_FALSE(profile->input_overrides.at("neogeojs1").contains(
        "Unknown"));
    REQUIRE_FALSE(profile->input_overrides.at("neogeoirrmaze").contains(
        "YAxis"));
    REQUIRE_FALSE(profile->input_overrides.contains("unknown"));

    fs::remove_all(root);
}

TEST_CASE("global profile defaults follow JGRF override order",
          "[profiles][runtime]") {
    const fs::path root = make_profile_test_root("globals");
    const AppPaths paths = make_profile_paths(root);
    write_text(paths.jollygood_settings_ini,
               "[video]\napi = 1\nshader = 5\nfullscreen = 1\n"
               "scale = 7\ncrtea_mode = 4\ncrtea_scanstr = 8\n");
    write_text(paths.geolith_ini,
               "[video]\napi = 3\nshader = 4\nfullscreen = 0\n"
               "scale = 99\ncrtea_scanstr = 10\n\n"
               "[geolith]\nsystem = 1\nunihw = 0\nregion = 2\n"
               "input = 3\nfreeplay = 1\nsettingmode = 1\n"
               "aspect = 2\npalette = 1\noverscan_t = 4\n");

    const GameProfileGlobalSettings global =
        goliath::load_game_profile_global_settings(paths);
    REQUIRE(global.video_api == 3);
    REQUIRE(global.shader == 4);
    REQUIRE(global.jgrf_video.at("fullscreen") == 0);
    // An invalid core-specific override retains the settings.ini value.
    REQUIRE(global.jgrf_video.at("scale") == 7);
    REQUIRE(global.jgrf_video.at("crtea_mode") == 4);
    REQUIRE(global.jgrf_video.at("crtea_scanstr") == 10);
    REQUIRE(global.geolith_video.at("aspect") == 2);
    REQUIRE(global.geolith_video.at("palette") == 1);
    REQUIRE(global.geolith_video.at("overscan_t") == 4);
    REQUIRE(global.cartridge_system == 1);
    REQUIRE(global.universe_hardware == 0);
    REQUIRE(global.region == 2);
    REQUIRE(global.input_device == 3);
    REQUIRE(global.free_play == 1);
    REQUIRE(global.setting_mode == 1);

    fs::remove_all(root);
}

TEST_CASE("runtime profile inherits global files without modifying them",
          "[profiles][runtime][input]") {
    const fs::path root = make_profile_test_root("runtime");
    const AppPaths paths = make_profile_paths(root);

    const std::string settingsText =
        "; preserve this comment and CRLF\r\n[video]\r\nshader = 5\r\n";
    const std::string coreText =
        "[geolith]\n"
        "system = 0\ncdsystem = 2\nunihw = 1\nregion = 0\n"
        "input = 1\nfreeplay = 0\nsettingmode = 0\n"
        "memcard_inserted = 1\n";
    const std::string inputText =
        "[neogeojs1]\na = j0b1\nB = 13\nUp = j0h00\n"
        "[neogeojs2]\nA = j1b1\n"
        "[neogeosystem]\nCoin1 = j0b8\n"
        "[unrelated]\nKey = j0b9\n";
    write_text(paths.jollygood_settings_ini, settingsText);
    write_text(paths.geolith_ini, coreText);
    write_text(paths.input_config, inputText);

    GameLaunchProfile profile;
    profile.cartridge_system = 1;
    profile.region = 1;
    profile.input_device = 3;
    profile.free_play = true;
    profile.setting_mode = true;
    profile.video_api = 3;
    profile.shader = 0;
    profile.jgrf_video = {
        {"fullscreen", 1},
        {"scale", 6},
        {"crtea_mode", 4},
        {"crtea_masktype", 3},
        {"crtea_maskstr", 8},
        {"crtea_scanstr", 9},
        {"crtea_sharpness", 10},
        {"crtea_curve", 4},
        {"crtea_corner", 5},
        {"crtea_tcurve", 6},
        {"unknown", 7},
    };
    profile.geolith_video = {
        {"aspect", 2},
        {"palette", 1},
        {"overscan_t", 1},
        {"overscan_b", 2},
        {"overscan_l", 3},
        {"overscan_r", 4},
        {"unknown", 0},
    };
    profile.controller_port = 2;
    profile.input_overrides["neogeojs1"]["A"] = "j1b9";
    profile.input_overrides["neogeojs1"]["B"] = "44";
    profile.input_overrides["neogeojs1"]["C"] = "j0b32";
    profile.input_overrides["neogeosystem"]["Coin1"] = "35";

    const auto runtime = materialize_game_profile_runtime(
        paths, "neogeo", "sets/kof98.neo", profile);
    REQUIRE(runtime.success);
    REQUIRE(runtime.error.empty());
    REQUIRE(runtime.config_root.parent_path() == paths.profile_runtime_dir);

    const fs::path runtimeConfig = runtime.config_root / "jollygood";
    REQUIRE(read_text(runtimeConfig / "settings.ini") == settingsText);

    IniDocument core;
    core.load(runtimeConfig / "geolith.ini");
    REQUIRE(core.get("geolith", "system") == "1");
    REQUIRE(core.get("geolith", "region") == "1");
    REQUIRE(core.get("geolith", "input") == "3");
    REQUIRE(core.get("geolith", "freeplay") == "1");
    REQUIRE(core.get("geolith", "settingmode") == "1");
    REQUIRE(core.get("geolith", "memcard_inserted") == "1");
    REQUIRE(core.get("video", "fullscreen") == "1");
    REQUIRE(core.get("video", "scale") == "6");
    REQUIRE(core.get("video", "crtea_mode") == "4");
    REQUIRE(core.get("video", "crtea_masktype") == "3");
    REQUIRE(core.get("video", "crtea_maskstr") == "8");
    REQUIRE(core.get("video", "crtea_scanstr") == "9");
    REQUIRE(core.get("video", "crtea_sharpness") == "10");
    REQUIRE(core.get("video", "crtea_curve") == "4");
    REQUIRE(core.get("video", "crtea_corner") == "5");
    REQUIRE(core.get("video", "crtea_tcurve") == "6");
    REQUIRE_FALSE(core.has_option("video", "api"));
    REQUIRE_FALSE(core.has_option("video", "shader"));
    REQUIRE_FALSE(core.has_option("video", "unknown"));
    REQUIRE(core.get("geolith", "aspect") == "2");
    REQUIRE(core.get("geolith", "palette") == "1");
    REQUIRE(core.get("geolith", "overscan_t") == "1");
    REQUIRE(core.get("geolith", "overscan_b") == "2");
    REQUIRE(core.get("geolith", "overscan_l") == "3");
    REQUIRE(core.get("geolith", "overscan_r") == "4");
    REQUIRE_FALSE(core.has_option("geolith", "unknown"));

    IniDocument input;
    input.load(runtimeConfig / "geolith_input.ini");
    REQUIRE(input.get("neogeojs1", "A") == "j1b9");
    REQUIRE(input.get("neogeojs1", "B") == "44");
    REQUIRE(input.get("neogeojs2", "A") == "j1b1");
    REQUIRE(input.get("neogeosystem", "Coin1") == "35");
    REQUIRE(input.get("unrelated", "Key") == "j0b9");
    REQUIRE(input.get("neogeojs1", "Up") == "j2h00");
    REQUIRE_FALSE(input.has_option("neogeojs1", "a"));
    REQUIRE_FALSE(input.has_option("neogeojs1", "C"));

    REQUIRE(read_text(paths.jollygood_settings_ini) == settingsText);
    REQUIRE(read_text(paths.geolith_ini) == coreText);
    REQUIRE(read_text(paths.input_config) == inputText);

    fs::remove_all(root);
}

TEST_CASE("per-game input mappings inherit built-in defaults without a global INI",
          "[profiles][runtime][input]") {
    const fs::path root = make_profile_test_root("default_input");
    const AppPaths paths = make_profile_paths(root);

    GameLaunchProfile profile;
    profile.input_overrides["neogeojs1"]["A"] = "44";

    const auto runtime = materialize_game_profile_runtime(
        paths, "neogeo", "default.neo", profile);
    REQUIRE(runtime.success);
    REQUIRE(runtime.error.empty());

    IniDocument input;
    input.load(runtime.config_root / "jollygood" / "geolith_input.ini");
    REQUIRE(input.get("neogeojs1", "A") == "44");
    REQUIRE(input.get("neogeojs1", "B") == "14");
    REQUIRE(input.get("neogeosystem", "Coin1") == "34");
    REQUIRE_FALSE(fs::exists(paths.input_config));

    fs::remove_all(root);
}

TEST_CASE("MVS-only options and four-player constraints are enforced",
          "[profiles][runtime][mvs]") {
    const fs::path root = make_profile_test_root("mvs_rules");
    const AppPaths paths = make_profile_paths(root);
    write_text(paths.geolith_ini,
               "[geolith]\nsystem = 1\nunihw = 1\nregion = 1\n"
               "input = 3\nfreeplay = 1\nsettingmode = 1\n");

    const GameProfileGlobalSettings global =
        goliath::load_game_profile_global_settings(paths);

    GameLaunchProfile aes;
    aes.cartridge_system = 0;
    REQUIRE_FALSE(game_profile_uses_mvs_hardware("neogeo", aes, global));
    REQUIRE_FALSE(game_profile_supports_four_player("neogeo", aes, global));

    const auto aesRuntime = materialize_game_profile_runtime(
        paths, "neogeo", "aes.neo", aes);
    REQUIRE(aesRuntime.success);
    IniDocument aesCore;
    aesCore.load(aesRuntime.config_root / "jollygood" / "geolith.ini");
    REQUIRE(aesCore.get("geolith", "input") == "0");
    REQUIRE(aesCore.get("geolith", "freeplay") == "0");
    REQUIRE(aesCore.get("geolith", "settingmode") == "0");

    GameLaunchProfile universeMvs;
    universeMvs.cartridge_system = 2;
    universeMvs.universe_hardware = 1;
    universeMvs.free_play = true;
    REQUIRE(effective_cartridge_system(universeMvs, global) == 2);
    REQUIRE(game_profile_uses_mvs_hardware("neogeo", universeMvs, global));

    GameLaunchProfile fourPlayer;
    fourPlayer.cartridge_system = 1;
    fourPlayer.region = 1;
    fourPlayer.input_device = 3;
    REQUIRE(game_profile_supports_four_player("neogeo", fourPlayer, global));
    fourPlayer.region = 2;
    REQUIRE(game_profile_supports_four_player("neogeo", fourPlayer, global));
    fourPlayer.region = 0;
    REQUIRE_FALSE(game_profile_supports_four_player("neogeo", fourPlayer, global));

    GameLaunchProfile cd;
    cd.cd_system = 2;
    REQUIRE_FALSE(game_profile_uses_mvs_hardware("neogeocd", cd, global));
    const auto cdRuntime = materialize_game_profile_runtime(
        paths, "neogeocd", "disc.cue", cd);
    REQUIRE(cdRuntime.success);
    IniDocument cdCore;
    cdCore.load(cdRuntime.config_root / "jollygood" / "geolith.ini");
    REQUIRE(cdCore.get("geolith", "input") == "0");
    REQUIRE(cdCore.get("geolith", "freeplay") == "0");
    REQUIRE(cdCore.get("geolith", "settingmode") == "0");

    fs::remove_all(root);
}

TEST_CASE("runtime profile materialization accepts long storage paths",
          "[profiles][runtime][paths]") {
    const fs::path root = make_profile_test_root("long_path");
    AppPaths paths = make_profile_paths(root);
    paths.profile_runtime_dir =
        root / std::string(120, 'x');

    GameLaunchProfile profile;
    profile.shader = 0;
    const auto runtime = materialize_game_profile_runtime(
        paths, "neogeo", "kof98.neo", profile);
    REQUIRE(runtime.success);
    REQUIRE(runtime.error.empty());
    REQUIRE(fs::is_directory(runtime.config_root / "jollygood"));

    fs::remove_all(root);
}

TEST_CASE("runtime profile refuses a substituted exact-media directory",
          "[profiles][runtime][filesystem][safety]") {
    const fs::path root = make_profile_test_root("runtime_link");
    const AppPaths paths = make_profile_paths(root);
    const std::string media = "substituted.neo";
    const fs::path runtimeRoot =
        paths.profile_runtime_dir /
        goliath::exact_media_storage_id("neogeo", media);
    const fs::path outside = root / "outside-runtime";

    fs::create_directories(paths.profile_runtime_dir);
    write_text(outside / "user-owned.txt", "keep me");
    REQUIRE(fs::is_directory(outside));
    REQUIRE(create_directory_link(runtimeRoot, outside));
    REQUIRE(inspect_direct_path(runtimeRoot).kind == DirectPathKind::Other);
    REQUIRE(read_text(outside / "user-owned.txt") == "keep me");

    GameLaunchProfile profile;
    profile.shader = 0;
    const auto runtime = materialize_game_profile_runtime(
        paths, "neogeo", media, profile);

    REQUIRE_FALSE(runtime.success);
    REQUIRE_FALSE(runtime.error.empty());
    REQUIRE(runtime.error.find("direct directory") != std::string::npos);
    REQUIRE(read_text(outside / "user-owned.txt") == "keep me");
    REQUIRE_FALSE(fs::exists(outside / "jollygood"));

    REQUIRE(remove_directory_link(runtimeRoot));
    REQUIRE_FALSE(fs::exists(runtimeRoot));
    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("runtime profile refuses a non-regular INI destination",
          "[profiles][runtime][filesystem][safety]") {
    const fs::path root = make_profile_test_root("runtime_ini_blocker");
    const AppPaths paths = make_profile_paths(root);
    const std::string media = "blocked.neo";
    const fs::path runtimeSettings =
        paths.profile_runtime_dir /
        goliath::exact_media_storage_id("neogeo", media) /
        "jollygood" / "settings.ini";

    write_text(paths.jollygood_settings_ini,
               "[video]\nshader = 4\n");
    write_text(runtimeSettings / "user-owned.txt", "keep child");
    REQUIRE(fs::is_directory(runtimeSettings));
    REQUIRE(read_text(runtimeSettings / "user-owned.txt") == "keep child");

    GameLaunchProfile profile;
    profile.shader = 0;
    const auto runtime = materialize_game_profile_runtime(
        paths, "neogeo", media, profile);

    REQUIRE_FALSE(runtime.success);
    REQUIRE(runtime.error.find("direct regular file") != std::string::npos);
    REQUIRE(fs::is_directory(runtimeSettings));
    REQUIRE(read_text(runtimeSettings / "user-owned.txt") == "keep child");
    REQUIRE(read_text(paths.jollygood_settings_ini) ==
            "[video]\nshader = 4\n");

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}
