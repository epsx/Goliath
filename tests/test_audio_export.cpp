#include "catch2/catch.hpp"

#include "common/paths.hpp"
#include "game/audio_export.hpp"
#include "game/jollygood_launch.hpp"

#include <filesystem>
#include <fstream>

using namespace goliath;
namespace fs = std::filesystem;

namespace {

void write_test_file(const fs::path& path) {
    std::ofstream output(path, std::ios::binary);
    output << "test";
}

fs::path test_root(const char* name) {
    return fs::temp_directory_path() / name;
}

} // namespace

TEST_CASE("audio export filenames are portable and readable",
          "[audio_export][filename]") {
    REQUIRE(sanitize_audio_export_stem("Metal Slug 3") == "Metal Slug 3");
    REQUIRE(sanitize_audio_export_stem("  Metal\t  Slug  3  ") ==
            "Metal Slug 3");
    REQUIRE(sanitize_audio_export_stem("A/B:C*D?E\"F<G>H|I") ==
            "A-B-C-D-E-F-G-H-I");
    REQUIRE(sanitize_audio_export_stem(" ... ") == "Goliath Audio");
    REQUIRE(sanitize_audio_export_stem("CON") == "Goliath - CON");
    REQUIRE(sanitize_audio_export_stem("Karnov's Revenge") ==
            "Karnov's Revenge");
    REQUIRE(sanitize_audio_export_stem("Karnov’s Revenge") ==
            "Karnov’s Revenge");
    std::string longUtf8Name;
    for (int index = 0; index < 100; ++index) longUtf8Name += "’";
    REQUIRE(sanitize_audio_export_stem(longUtf8Name).size() == 159);
}

TEST_CASE("audio export extension and collision policy never overwrites",
          "[audio_export][path]") {
    const fs::path root = test_root("goliath_audio_export_unique_test");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);

    REQUIRE(ensure_audio_export_wav_extension(root / "capture") ==
            root / "capture.wav");
    REQUIRE(ensure_audio_export_wav_extension(root / "capture.WaV") ==
            root / "capture.WaV");
    REQUIRE(ensure_audio_export_wav_extension(root / "capture.raw") ==
            root / "capture.raw.wav");
    REQUIRE(audio_export_has_wav_extension(root / "capture.wav"));
    REQUIRE_FALSE(audio_export_has_wav_extension(root / "capture.flac"));
    REQUIRE(unique_audio_export_path(root / "capture.wav") ==
            root / "capture.wav");

    write_test_file(root / "capture.wav");
    REQUIRE(unique_audio_export_path(root / "capture.wav") ==
            root / "capture - 2.wav");
    write_test_file(root / "capture - 2.wav");
    REQUIRE(unique_audio_export_path(root / "capture.wav") ==
            root / "capture - 3.wav");

    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("audio export target validation is non-destructive",
          "[audio_export][validation]") {
    const fs::path root = test_root("goliath_audio_export_validation_test");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);

    REQUIRE_FALSE(validate_audio_export_target({}).success);
    REQUIRE_FALSE(validate_audio_export_target("relative.wav").success);
    REQUIRE_FALSE(validate_audio_export_target(root / "capture.flac").success);
    REQUIRE_FALSE(validate_audio_export_target(
        root / "missing" / "capture.wav").success);

    const fs::path existing = root / "existing.wav";
    write_test_file(existing);
    const AudioExportTargetValidation existingResult =
        validate_audio_export_target(existing);
    REQUIRE_FALSE(existingResult.success);
    REQUIRE(existingResult.error.find("never overwrites") != std::string::npos);

    fs::create_directories(root / "directory.wav", ec);
    REQUIRE_FALSE(ec);
    REQUIRE_FALSE(validate_audio_export_target(root / "directory.wav").success);

    const fs::path available = root / "available.wav";
    const AudioExportTargetValidation availableResult =
        validate_audio_export_target(available);
    REQUIRE(availableResult.success);
    REQUIRE_FALSE(fs::exists(available));

    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("one-shot JGRF WAV output is conflict-safe and keeps media last",
          "[audio_export][launch]") {
    REQUIRE_FALSE(jollygood_args_have_wave_output({"-cgeolith"}));
    REQUIRE(jollygood_args_have_wave_output({"-c", "geolith", "-o", "a.wav"}));
    REQUIRE(jollygood_args_have_wave_output({"--wave", "a.wav"}));
    REQUIRE(jollygood_args_have_wave_output({"--wave=a.wav"}));
    REQUIRE(jollygood_args_have_wave_output({"-voa.wav"}));

    const QString output = "H:/Captures/Metal Slug.wav";
    const QStringList cartridge = build_jollygood_launch_args(
        "jollygood.exe", {"-c", "geolith"}, "neogeo",
        "D:/Neo Geo/mslug.neo", 3, 5,
        std::nullopt, std::nullopt, true, output);
    REQUIRE(cartridge == QStringList{
        "jollygood.exe", "-c", "geolith", "--video", "3",
        "--shader", "5", "--verbose",
        "--wave", prepare_wave_output_path_for_launch(output),
        "D:/Neo Geo/mslug.neo"});
    REQUIRE(cartridge.at(cartridge.size() - 3) == "--wave");
    REQUIRE(cartridge.last() == "D:/Neo Geo/mslug.neo");

    const QStringList configured = build_jollygood_launch_args(
        "jollygood.exe", {"-c", "geolith", "--wave", "configured.wav"},
        "neogeo", "D:/Neo Geo/kof98.neo", std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, false, output);
    REQUIRE(configured.count("--wave") == 1);
    REQUIRE(configured.contains("configured.wav"));
    REQUIRE_FALSE(configured.contains(prepare_wave_output_path_for_launch(output)));
    REQUIRE(configured.last() == "D:/Neo Geo/kof98.neo");

    const QString longOutput = QString::fromStdString(
        "D:/" + std::string(260, 'a') + ".wav");
#if defined(_WIN32)
    REQUIRE(prepare_wave_output_path_for_launch(longOutput).startsWith(
        QStringLiteral("\\\\?\\")));
#else
    REQUIRE(prepare_wave_output_path_for_launch(longOutput) == longOutput);
#endif

    const fs::path root = test_root("goliath_audio_export_launch_test");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "captures", ec);
    REQUIRE_FALSE(ec);
#if defined(_WIN32)
    const fs::path executable = root / "jollygood.exe";
#else
    const fs::path executable = root / "jollygood";
#endif
    write_test_file(executable);

    AppPaths paths;
    paths.base_dir = root;
    paths.config_dir = root / "config";
    paths.data_dir = root / "data";
    paths.jollygood_exe = executable;
    paths.jollygood_args = {"-c", "geolith"};
    const fs::path capture = root / "captures" / "capture.wav";

    const JollygoodLaunchPreparation ready = prepare_jollygood_launch(
        paths, root / "roms", root / "cd", "neogeo", "mslug.neo",
        nullptr, false, capture);
    REQUIRE(ready.status == LaunchPreparationStatus::Ready);
    REQUIRE(ready.args.at(ready.args.size() - 3) == "--wave");
    REQUIRE(ready.args.last().endsWith("mslug.neo"));

    paths.jollygood_args = {"-c", "geolith", "-o", "configured.wav"};
    const JollygoodLaunchPreparation conflict = prepare_jollygood_launch(
        paths, root / "roms", root / "cd", "neogeo", "mslug.neo",
        nullptr, false, capture);
    REQUIRE(conflict.status == LaunchPreparationStatus::WaveOutputConflict);

    write_test_file(capture);
    paths.jollygood_args = {"-c", "geolith"};
    const JollygoodLaunchPreparation occupied = prepare_jollygood_launch(
        paths, root / "roms", root / "cd", "neogeo", "mslug.neo",
        nullptr, false, capture);
    REQUIRE(occupied.status == LaunchPreparationStatus::WaveOutputInvalid);

    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}
