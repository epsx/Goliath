#include "catch2/catch.hpp"

#include "common/paths.hpp"
#include "game/game_profile.hpp"
#include "ui/widgets/settings_field.hpp"
#include "game/jollygood_capabilities.hpp"
#include "game/jollygood_executable.hpp"
#include "game/jollygood_launch.hpp"
#include "game/geolith_capabilities.hpp"
#include "ui/tabs/core_tab_help.hpp"

#include <filesystem>
#include <fstream>

using namespace goliath;

TEST_CASE("settings field normalization follows JGRF default fallback semantics",
          "[settings][video][misc]") {
    const FieldSpec combo{"log", "Log", 1, FieldSpec::Kind::Combo,
                          {{0, "Debug"}, {1, "Info"}, {2, "Warning"}, {3, "Error"}}, 0, 0};
    REQUIRE(is_valid_field_value(combo, 0));
    REQUIRE(is_valid_field_value(combo, 3));
    REQUIRE_FALSE(is_valid_field_value(combo, 9));
    REQUIRE(normalize_field_value(combo, 0) == 0);
    REQUIRE(normalize_field_value(combo, 3) == 3);
    REQUIRE(normalize_field_value(combo, 9) == 1);

    const FieldSpec boolean{"fullscreen", "Fullscreen", 0, FieldSpec::Kind::Bool, {}, 0, 0};
    REQUIRE(normalize_field_value(boolean, 0) == 0);
    REQUIRE(normalize_field_value(boolean, 1) == 1);
    REQUIRE(normalize_field_value(boolean, 2) == 0);

    const FieldSpec spin{"scale", "Scale", 3, FieldSpec::Kind::Spin, {}, 1, 8};
    REQUIRE(normalize_field_value(spin, 1) == 1);
    REQUIRE(normalize_field_value(spin, 8) == 8);
    REQUIRE(normalize_field_value(spin, 0) == 3);
    REQUIRE(normalize_field_value(spin, 99) == 3);

    const FieldSpec deadzone{"axis_deadzone", "Axis Deadzone", 5,
                             FieldSpec::Kind::Spin, {}, 0, 9};
    REQUIRE(normalize_field_value(deadzone, 0) == 0);
    REQUIRE(normalize_field_value(deadzone, 5) == 5);
    REQUIRE(normalize_field_value(deadzone, 9) == 9);
    REQUIRE(normalize_field_value(deadzone, 10) == 5);
}

TEST_CASE("Universe BIOS help explains independent cartridge and CD settings",
          "[settings][core][universe-bios]") {
    const QString help = universe_bios_help_text();

    REQUIRE(help.contains("cartridge (.neo) games"));
    REQUIRE(help.contains("Used only for Neo Geo CD games"));
    REQUIRE(help.contains("does not depend on System Type"));
    REQUIRE(help.contains("Used only when System Type is Universe BIOS"));
    REQUIRE(help.contains("does not affect CD Universe BIOS"));
    REQUIRE(help.contains("US, JP, AS (Asia), or EU"));
    REQUIRE(help.contains("next game launch"));
}

TEST_CASE("JGRF executable resolution is shared by launch and capability probes",
          "[jgrf][path][resolver]") {
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "goliath_jgrf_resolver_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);

#if defined(_WIN32)
    const fs::path executable = root / "jollygood.exe";
#else
    const fs::path executable = root / "jollygood";
#endif
    {
        std::ofstream out(executable, std::ios::binary);
        out << "test";
    }

    REQUIRE(resolve_jollygood_executable(executable) == executable);
    REQUIRE(resolve_jollygood_executable(root / "jollygood") == executable);
    REQUIRE(resolve_jollygood_executable(root / "missing").empty());

    fs::remove_all(root, ec);
}

TEST_CASE("JGRF help capability probe detects optional Vulkan renderer",
          "[settings][video][vulkan]") {
    REQUIRE(jgrf_help_reports_vulkan(
        "                              3 = Vulkan\n"));
    REQUIRE(jgrf_help_reports_vulkan("3=VULKAN\r\n"));
    REQUIRE_FALSE(jgrf_help_reports_vulkan(
        "0 = OpenGL Core\n1 = OpenGL ES\n2 = OpenGL Compatibility\n"));
    REQUIRE_FALSE(jgrf_help_reports_vulkan(
        "This text mentions ENABLE_VULKAN but does not expose API 3."));
}

TEST_CASE("JGRF help probe extracts frontend version",
          "[settings][info][jgrf]") {
    const auto version = jgrf_version_from_help(
        "The Jolly Good Reference Frontend 2.0.1\r\nusage: jollygood [options] game\r\n");
    REQUIRE(version.has_value());
    REQUIRE(*version == "2.0.1");
    REQUIRE_FALSE(jgrf_version_from_help("usage: jollygood [options] game\n").has_value());
}

TEST_CASE("JG API numeric version formats as semantic version",
          "[settings][info][jg]") {
    REQUIRE(format_jg_api_version(20000) == "2.0.0");
    REQUIRE(format_jg_api_version(20603) == "2.6.3");
    REQUIRE(format_jg_api_version(10100) == "1.1.0");
}


TEST_CASE("Geolith JG system extensions expose Neo Geo CD CHD capability",
          "[settings][info][geolith][neogeocd]") {
    REQUIRE(parse_jg_extension_list("cue,chd") == std::vector<std::string>{"cue", "chd"});
    REQUIRE(parse_jg_extension_list(" CUE, .CHD , cue ") ==
            std::vector<std::string>{"cue", "chd"});
    REQUIRE(parse_jg_extension_list("").empty());

    GeolithCapabilities withChd;
    withChd.success = true;
    withChd.systems.push_back({"neogeo", "Neo Geo", {"neo"}});
    withChd.systems.push_back({"neogeocd", "Neo Geo CD", {"cue", "chd"}});

    REQUIRE(geolith_supports_extension(withChd, "neogeocd", "cue"));
    REQUIRE(geolith_supports_extension(withChd, "neogeocd", ".CHD"));
    REQUIRE_FALSE(geolith_supports_extension(withChd, "neogeo", "chd"));
    REQUIRE(geolith_extensions_display(withChd, "neogeocd") == "CUE, CHD");

    GeolithCapabilities cueOnly;
    cueOnly.success = true;
    cueOnly.systems.push_back({"neogeocd", "Neo Geo CD", {"cue"}});
    REQUIRE_FALSE(geolith_supports_extension(cueOnly, "neogeocd", "chd"));
    REQUIRE(geolith_extensions_display(cueOnly, "neogeocd") == "CUE");
}

TEST_CASE("Neo Geo CD CHD launch is gated on installed core capability",
          "[launch][neogeocd][chd]") {
    const std::vector<std::string> baseArgs = {"-c", "geolith", "-f"};

    REQUIRE(media_is_launchable("neogeo", "kof98.neo", false));
    REQUIRE(media_is_launchable("neogeocd", "Metal Slug.cue", false));
    REQUIRE(media_is_launchable("neogeocd", "METAL SLUG.CUE", false));
    REQUIRE_FALSE(media_is_launchable("neogeocd", "Metal Slug.chd", false));
    REQUIRE(media_is_launchable("neogeocd", "Metal Slug.chd", true));
    REQUIRE(media_is_launchable("neogeocd", "METAL SLUG.CHD", true));
    REQUIRE_FALSE(media_is_launchable("neogeocd", "random.iso", true));

    const QStringList cdArgs = build_jollygood_launch_args(
        "jollygood.exe", baseArgs, "neogeocd", "D:/Neo Geo CD/Metal Slug.chd");
    REQUIRE(cdArgs == QStringList{
        "jollygood.exe", "-c", "geolith", "-f", "-e", "neogeocd",
        "D:/Neo Geo CD/Metal Slug.chd"});

    const QStringList cartArgs = build_jollygood_launch_args(
        "jollygood.exe", baseArgs, "neogeo", "D:/Neo Geo/kof98.neo");
    REQUIRE(cartArgs == QStringList{
        "jollygood.exe", "-c", "geolith", "-f", "D:/Neo Geo/kof98.neo"});

    const std::vector<std::string> argsWithGlobalFrontend = {
        "-c", "geolith", "--video", "0", "--shader", "5"};
    const QStringList profiledArgs = build_jollygood_launch_args(
        "jollygood.exe", argsWithGlobalFrontend, "neogeo",
        "D:/Neo Geo/kof98.neo", 3, 0);
    REQUIRE(profiledArgs == QStringList{
        "jollygood.exe", "-c", "geolith", "--video", "0", "--shader", "5",
        "--video", "3", "--shader", "0", "D:/Neo Geo/kof98.neo"});
    REQUIRE(profiledArgs.last() == "D:/Neo Geo/kof98.neo");

    const QStringList invalidOverrides = build_jollygood_launch_args(
        "jollygood.exe", baseArgs, "neogeo", "D:/Neo Geo/kof98.neo", 9, 7);
    REQUIRE(invalidOverrides == QStringList{
        "jollygood.exe", "-c", "geolith", "-f",
        "D:/Neo Geo/kof98.neo"});

    const std::vector<std::string> conflictingWindowArgs = {
        "-c", "geolith", "--fullscreen", "--scale", "2"};
    const QStringList windowedProfile = build_jollygood_launch_args(
        "jollygood.exe", conflictingWindowArgs, "neogeo",
        "D:/Neo Geo/kof98.neo", std::nullopt, std::nullopt, 0, 8);
    REQUIRE(windowedProfile == QStringList{
        "jollygood.exe", "-c", "geolith", "--scale", "2",
        "--window", "--scale", "8", "D:/Neo Geo/kof98.neo"});

    const std::vector<std::string> conflictingFullscreenArgs = {
        "-c", "geolith", "--window"};
    const QStringList fullscreenProfile = build_jollygood_launch_args(
        "jollygood.exe", conflictingFullscreenArgs, "neogeo",
        "D:/Neo Geo/kof98.neo", std::nullopt, std::nullopt, 1, 3);
    REQUIRE(fullscreenProfile == QStringList{
        "jollygood.exe", "-c", "geolith", "--fullscreen",
        "--scale", "3", "D:/Neo Geo/kof98.neo"});
}

TEST_CASE("session verbose logging is additive and keeps media last",
          "[launch][logging][verbose]") {
    const std::vector<std::string> baseArgs = {"-c", "geolith"};
    REQUIRE_FALSE(jollygood_args_have_verbose_logging(baseArgs));
    REQUIRE(jollygood_args_have_verbose_logging({"-c", "geolith", "-v"}));
    REQUIRE(jollygood_args_have_verbose_logging(
        {"-c", "geolith", "--verbose"}));

    const QStringList cartridge = build_jollygood_launch_args(
        "jollygood.exe", baseArgs, "neogeo", "D:/Neo Geo/mslug.neo",
        std::nullopt, std::nullopt, std::nullopt, std::nullopt, true);
    REQUIRE(cartridge == QStringList{
        "jollygood.exe", "-c", "geolith", "--verbose",
        "D:/Neo Geo/mslug.neo"});
    REQUIRE(cartridge.at(cartridge.size() - 2) == "--verbose");
    REQUIRE(cartridge.last() == "D:/Neo Geo/mslug.neo");

    const QString cuePath = "D:/Neo Geo CD/mslug.cue";
    const QString preparedCuePath =
        prepare_media_path_for_launch("neogeocd", cuePath);
    const QStringList cd = build_jollygood_launch_args(
        "jollygood.exe", baseArgs, "neogeocd", cuePath,
        std::nullopt, 5, std::nullopt, std::nullopt, true);
    REQUIRE(cd == QStringList{
        "jollygood.exe", "-c", "geolith", "-e", "neogeocd",
        "--shader", "5", "--verbose", preparedCuePath});
    REQUIRE(cd.last() == preparedCuePath);

    const QStringList configured = build_jollygood_launch_args(
        "jollygood.exe", {"-c", "geolith", "--verbose"}, "neogeo",
        "D:/Neo Geo/kof98.neo", std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, true);
    REQUIRE(configured.count("--verbose") == 1);
    REQUIRE(configured.last() == "D:/Neo Geo/kof98.neo");
}

TEST_CASE("per-game launch uses isolated config and shared data",
          "[launch][profiles][environment]") {
    namespace fs = std::filesystem;

    const fs::path root =
        fs::temp_directory_path() / "gp_launch_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "config" / "jollygood", ec);
    REQUIRE_FALSE(ec);

#if defined(_WIN32)
    const fs::path executable = root / "jollygood.exe";
#else
    const fs::path executable = root / "jollygood";
#endif
    {
        std::ofstream output(executable, std::ios::binary);
        output << "test";
    }

    AppPaths paths;
    paths.base_dir = root;
    paths.config_dir = root / "config";
    paths.data_dir = root / "data";
    paths.jollygood_exe = executable;
    paths.jollygood_args = {"-c", "geolith"};
    paths.jollygood_settings_ini = paths.config_dir / "jollygood" / "settings.ini";
    paths.geolith_ini = paths.config_dir / "jollygood" / "geolith.ini";
    paths.input_config = paths.config_dir / "jollygood" / "geolith_input.ini";
    paths.profile_runtime_dir = paths.config_dir / "p";
    {
        std::ofstream output(paths.geolith_ini, std::ios::binary);
        output << "[geolith]\nsystem = 0\n";
    }

    GameLaunchProfile profile;
    profile.cartridge_system = 1;
    profile.video_api = 3;
    profile.shader = 0;
    profile.jgrf_video["fullscreen"] = 0;
    profile.jgrf_video["scale"] = 6;

    const auto preparation = prepare_jollygood_launch(
        paths, root / "roms", root / "neocd", "neogeo", "kof98.neo",
        &profile, true);
    REQUIRE(preparation.status == LaunchPreparationStatus::Ready);
    REQUIRE(preparation.profile_applied);
    REQUIRE_FALSE(preparation.profile_config_root.isEmpty());
    REQUIRE(preparation.environment.value("XDG_CONFIG_HOME") ==
            QString::fromStdString(
                fs::path(preparation.profile_config_root.toStdString())
                    .lexically_relative(paths.base_dir).string()));
    REQUIRE(preparation.environment.value("XDG_DATA_HOME") ==
            QString("data"));
    REQUIRE(preparation.environment.value("XDG_CONFIG_HOME") !=
            QString::fromStdString(paths.config_dir.string()));
    REQUIRE(preparation.args.contains("--video"));
    REQUIRE(preparation.args.contains("--shader"));
    REQUIRE(preparation.args.contains("--window"));
    REQUIRE(preparation.args.contains("--scale"));
    REQUIRE(preparation.args.contains("--verbose"));
    REQUIRE(preparation.args.at(preparation.args.indexOf("--video") + 1) == "3");
    REQUIRE(preparation.args.at(preparation.args.indexOf("--shader") + 1) == "0");
    REQUIRE(preparation.args.at(preparation.args.indexOf("--scale") + 1) == "6");
    REQUIRE(preparation.args.at(preparation.args.size() - 2) == "--verbose");
    REQUIRE(preparation.args.last().endsWith("kof98.neo"));

    AppPaths externalPaths = paths;
    externalPaths.data_dir = root.parent_path() / "goliath-external-data";
    const auto external = prepare_jollygood_launch(
        externalPaths, root / "roms", root / "neocd", "neogeo", "kof98.neo");
    REQUIRE(external.status == LaunchPreparationStatus::Ready);
    REQUIRE(external.environment.value("XDG_DATA_HOME") ==
            QString::fromStdString(externalPaths.data_dir.string()));

    fs::remove_all(root, ec);
}

TEST_CASE("per-game launch measures the XDG config path passed to JGRF",
          "[launch][profiles][environment][paths]") {
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() /
        ("gp_portable_" + std::string(100, 'p'));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "config" / "jollygood", ec);
    REQUIRE_FALSE(ec);

#if defined(_WIN32)
    const fs::path executable = root / "jollygood.exe";
#else
    const fs::path executable = root / "jollygood";
#endif
    {
        std::ofstream output(executable, std::ios::binary);
        output << "test";
    }

    AppPaths paths;
    paths.base_dir = root;
    paths.config_dir = root / "config";
    paths.data_dir = root / "data";
    paths.jollygood_exe = executable;
    paths.jollygood_args = {"-c", "geolith"};
    paths.jollygood_settings_ini =
        paths.config_dir / "jollygood" / "settings.ini";
    paths.geolith_ini = paths.config_dir / "jollygood" / "geolith.ini";
    paths.input_config =
        paths.config_dir / "jollygood" / "geolith_input.ini";
    paths.profile_runtime_dir = paths.config_dir / "p";

    GameLaunchProfile profile;
    profile.video_api = 1;

    const auto portable = prepare_jollygood_launch(
        paths, root / "roms", root / "neocd", "neogeocd",
        "fatal-fury-3.cue", &profile, false);
    REQUIRE(portable.status == LaunchPreparationStatus::Ready);
    REQUIRE(portable.profile_applied);
    REQUIRE(portable.environment.value("XDG_CONFIG_HOME") ==
            QString::fromStdString(
                fs::path(portable.profile_config_root.toStdString())
                    .lexically_relative(paths.base_dir).string()));
    REQUIRE((fs::path(portable.profile_config_root.toStdString()) /
             "jollygood").string().size() + 1 >= 128);
    REQUIRE((portable.environment.value("XDG_CONFIG_HOME") +
             "/jollygood").toUtf8().size() + 1 < 128);

    AppPaths external = paths;
    external.profile_runtime_dir = fs::temp_directory_path() /
        ("gp_external_" + std::string(110, 'x'));
    fs::remove_all(external.profile_runtime_dir, ec);

    const auto rejected = prepare_jollygood_launch(
        external, root / "roms", root / "neocd", "neogeocd",
        "fatal-fury-3.cue", &profile, false);
    REQUIRE(rejected.status ==
            LaunchPreparationStatus::ProfileConfigurationFailed);
    REQUIRE(rejected.detail.find("path is too long") != std::string::npos);
    REQUIRE_FALSE(fs::exists(external.profile_runtime_dir));

    fs::remove_all(root, ec);
}

TEST_CASE("Windows CUE launch covers derived paths beyond MAX_PATH",
          "[launch][neogeocd][longpath]") {
    const QString shortCue = "D:/Neo Geo CD/Metal Slug.cue";
    const QString shortChd = "D:/Neo Geo CD/Metal Slug.chd";

    // Reproduce the reported boundary: the selected CUE still fits below
    // MAX_PATH, but a relative track name derived by Geolith does not.
    const QString cueBelowLimit =
        QStringLiteral("D:/") +
        QString(120, QChar('a')) + "/" +
        QString(127, QChar('b')) + ".cue";
    const QString trackBeyondLimit =
        cueBelowLimit.left(cueBelowLimit.size() - 4) +
        QStringLiteral(" (Track 01).bin");
    REQUIRE(cueBelowLimit.size() == 255);
    REQUIRE(trackBeyondLimit.size() == 266);

    const QString longChd =
        QStringLiteral("D:/") +
        QString(120, QChar('a')) + "/" +
        QString(120, QChar('b')) + "/" +
        QString(40, QChar('c')) + ".chd";
    REQUIRE(longChd.size() >= 260);

#if defined(_WIN32)
    REQUIRE(prepare_media_path_for_launch("neogeocd", shortCue) ==
            QStringLiteral("\\\\?\\D:\\Neo Geo CD\\Metal Slug.cue"));
    REQUIRE(prepare_media_path_for_launch("neogeocd", shortChd) == shortChd);

    const QString preparedCue =
        prepare_media_path_for_launch("neogeocd", cueBelowLimit);
    REQUIRE(preparedCue.startsWith(QStringLiteral("\\\\?\\D:\\")));
    REQUIRE(preparedCue.endsWith(QStringLiteral(".cue")));

    const QString preparedChd =
        prepare_media_path_for_launch("neogeocd", longChd);
    REQUIRE(preparedChd.startsWith(QStringLiteral("\\\\?\\D:\\")));
    REQUIRE(preparedChd.endsWith(QStringLiteral(".chd")));
#else
    REQUIRE(prepare_media_path_for_launch("neogeocd", shortCue) == shortCue);
    REQUIRE(prepare_media_path_for_launch("neogeocd", shortChd) == shortChd);
    REQUIRE(prepare_media_path_for_launch("neogeocd", cueBelowLimit) == cueBelowLimit);
    REQUIRE(prepare_media_path_for_launch("neogeocd", longChd) == longChd);
#endif

    REQUIRE(prepare_media_path_for_launch("neogeo", cueBelowLimit) == cueBelowLimit);
}


TEST_CASE("launch media selection keeps parent and explicit variants distinct",
          "[launch][selection]") {
    Game game;
    game.main_rom = "shocktr2.neo";

    Rom parent;
    parent.file = "shocktr2.neo";
    parent.mame = "shocktr2";
    parent.main = true;

    Rom bootleg;
    bootleg.file = "lans2004.neo";
    bootleg.mame = "lans2004";
    bootleg.cloneof = "shocktr2";

    game.roms = {parent, bootleg};

    const auto parentMedia = selected_launch_media(game, -1);
    REQUIRE(parentMedia.has_value());
    REQUIRE(*parentMedia == "shocktr2.neo");

    const auto variantMedia = selected_launch_media(game, 1);
    REQUIRE(variantMedia.has_value());
    REQUIRE(*variantMedia == "lans2004.neo");

    const auto invalidVariant = selected_launch_media(game, 99);
    REQUIRE(invalidVariant.has_value());
    REQUIRE(*invalidVariant == "shocktr2.neo");

    game.main_rom.reset();
    REQUIRE_FALSE(selected_launch_media(game, -1).has_value());
    REQUIRE(selected_launch_media(game, 1) == std::optional<std::string>("lans2004.neo"));

    AppPaths paths;
    const auto unsupported = prepare_jollygood_launch(
        paths, std::filesystem::path("roms"), std::filesystem::path("neocd"),
        "neogeocd", "disc.iso");
    REQUIRE(unsupported.status == LaunchPreparationStatus::UnsupportedMedia);
}
