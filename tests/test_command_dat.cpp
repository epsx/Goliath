#include "catch2/catch.hpp"

#include "game/command_dat.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path command_dat_test_path(const std::string& name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
        ("goliath_command_dat_" + name + "_" + std::to_string(stamp));
}

void write_command_dat(const fs::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.good());
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

} // namespace

TEST_CASE("command.dat accepts BOM CRLF aliases and DOS EOF",
          "[command_dat][parser]") {
    const fs::path path = command_dat_test_path("valid");
    const std::string contents =
        "\xEF\xBB\xBF# header\r\n"
        "$info=MSLUG, mslugr1\r\n"
        "$cmd\r\n"
        "Metal Slug\r\n\r\n"
        "Fire  _A\r\n"
        "$end\r\n"
        "$info=\r\n"
        "$cmd\r\n"
        "Don't remove this sentinel.\r\n"
        "$end\x1A";
    write_command_dat(path, contents);

    goliath::CommandDatCatalog catalog;
    std::string error;
    CHECK(catalog.load(path, &error) ==
          goliath::CommandDatLoadStatus::Loaded);
    CHECK(error.empty());
    CHECK(catalog.entry_count() == 1);
    CHECK(catalog.id_count() == 2);

    const goliath::CommandDatEntry* entry = catalog.find("MSLUGR1");
    REQUIRE(entry != nullptr);
    CHECK(entry->text == "Metal Slug\n\nFire  _A");
    CHECK(catalog.find("missing") == nullptr);
    fs::remove(path);
}

TEST_CASE("missing command.dat is an optional clean state",
          "[command_dat][parser]") {
    const fs::path path = command_dat_test_path("missing");
    fs::remove(path);
    goliath::CommandDatCatalog catalog;
    std::string error;
    CHECK(catalog.load(path, &error) ==
          goliath::CommandDatLoadStatus::Missing);
    CHECK(error.empty());
    CHECK(catalog.entry_count() == 0);
}

TEST_CASE("malformed command.dat fails closed and clears prior data",
          "[command_dat][parser]") {
    const fs::path path = command_dat_test_path("malformed");
    write_command_dat(path,
        "$info=mslug\n$cmd\nCommands\n$end\n");

    goliath::CommandDatCatalog catalog;
    REQUIRE(catalog.load(path) == goliath::CommandDatLoadStatus::Loaded);
    write_command_dat(path, "$info=mslug\n$cmd\nunterminated\n");

    std::string error;
    CHECK(catalog.load(path, &error) ==
          goliath::CommandDatLoadStatus::Error);
    CHECK_FALSE(error.empty());
    CHECK(catalog.entry_count() == 0);
    CHECK(catalog.find("mslug") == nullptr);
    fs::remove(path);
}

TEST_CASE("command.dat lookup prioritizes exact variant then parent",
          "[command_dat][lookup]") {
    goliath::Game game;
    game.short_name = "kof98";
    game.roms.push_back(goliath::Rom{
        "kof98.neo", {}, {}, "kof98", {}, true});
    game.roms.push_back(goliath::Rom{
        "kof98h.neo", {}, {}, "KOF98H", std::string("kof98"), false});

    CHECK(goliath::command_dat_lookup_ids(game, 1) ==
          std::vector<std::string>{"kof98h", "kof98"});
    CHECK(goliath::command_dat_lookup_ids(game, -1) ==
          std::vector<std::string>{"kof98"});
}
