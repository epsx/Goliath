#include "catch2/catch.hpp"

#include "common/file_transaction.hpp"
#include "common/goliath_common.hpp"
#include "ini/ini_document.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using goliath::Config;
using goliath::IniDocument;
using goliath::default_config;
using goliath::save_config_file;
using goliath::save_files_transactionally;

namespace {

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void write_text(const fs::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << contents;
    output.close();
    REQUIRE(output.good());
}

fs::path fresh_test_root(const std::string& name) {
    const fs::path root =
        fs::temp_directory_path() / ("goliath_config_" + name);
    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    REQUIRE(fs::create_directories(root, ec));
    REQUIRE_FALSE(ec);
    return root;
}

} // namespace

TEST_CASE("Goliath configuration saves transactionally to an explicit path",
          "[config][persistence]") {
    const fs::path root = fresh_test_root("roundtrip");
    const fs::path path = root / "goliath.ini";

    Config config = default_config();
    config.set("Paths", "roms", "custom-roms");
    config.set("UI", "theme", "Dark");

    std::string error = "stale";
    REQUIRE(save_config_file(path, config, &error));
    CHECK(error.empty());

    const std::string stored = read_text(path);
    CHECK(stored.find("roms = custom-roms") != std::string::npos);
    CHECK(stored.find("theme = Dark") != std::string::npos);

    fs::remove_all(root);
}

TEST_CASE("Goliath configuration preserves a blocker and retries after failure",
          "[config][persistence][recovery]") {
    const fs::path root = fresh_test_root("blocked");
    const fs::path blocker = root / "config";
    const fs::path path = blocker / "goliath.ini";
    const std::string sentinel = "do not replace\n";
    write_text(blocker, sentinel);

    Config config = default_config();
    config.set("Audio", "volume", "37");

    std::string error;
    CHECK_FALSE(save_config_file(path, config, &error));
    CHECK_FALSE(error.empty());
    CHECK(read_text(blocker) == sentinel);

    REQUIRE(fs::remove(blocker));
    REQUIRE(fs::create_directories(blocker));

    error = "stale";
    REQUIRE(save_config_file(path, config, &error));
    CHECK(error.empty());
    CHECK(read_text(path).find("volume = 37") != std::string::npos);

    fs::remove_all(root);
}

TEST_CASE("Mixed settings transaction preserves every file when one is blocked",
          "[config][ini][persistence][transaction]") {
    const fs::path root = fresh_test_root("mixed_transaction");
    const fs::path settingsPath = root / "settings.ini";
    const fs::path corePath = root / "geolith.ini";
    const fs::path blocker = root / "blocked";
    const fs::path configPath = blocker / "goliath.ini";

    const std::string originalSettings =
        "[audio]\nrsqual = 2\nkeep = settings\n";
    const std::string originalCore =
        "[geolith]\nregion = 1\nkeep = core\n";
    const std::string originalBlocker = "keep this blocker\n";
    write_text(settingsPath, originalSettings);
    write_text(corePath, originalCore);
    write_text(blocker, originalBlocker);

    IniDocument settings;
    settings.set("audio", "rsqual", "4");
    IniDocument core;
    core.set("geolith", "region", "2");
    Config config = default_config();
    config.set("Audio", "volume", "37");

    std::string error;
    CHECK_FALSE(save_files_transactionally({
        {settingsPath, settings.serialized_text()},
        {corePath, core.serialized_text()},
        {configPath, config.serialized_text()},
    }, &error));
    CHECK_FALSE(error.empty());
    CHECK(read_text(settingsPath) == originalSettings);
    CHECK(read_text(corePath) == originalCore);
    CHECK(read_text(blocker) == originalBlocker);

    REQUIRE(fs::remove(blocker));
    REQUIRE(save_files_transactionally({
        {settingsPath, settings.serialized_text()},
        {corePath, core.serialized_text()},
        {configPath, config.serialized_text()},
    }, &error));
    CHECK(error.empty());

    IniDocument savedSettings;
    savedSettings.load(settingsPath);
    CHECK(savedSettings.get("audio", "rsqual") == "4");

    IniDocument savedCore;
    savedCore.load(corePath);
    CHECK(savedCore.get("geolith", "region") == "2");
    CHECK(read_text(configPath).find("volume = 37") != std::string::npos);

    fs::remove_all(root);
}
