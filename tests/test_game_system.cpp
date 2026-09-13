#include "catch2/catch.hpp"

#include "common/goliath_common.hpp"
#include "game/game_model.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using goliath::default_config;
using goliath::load_games;

namespace {

fs::path make_system_test_file(const std::string& name, const std::string& content) {
    const fs::path path =
        fs::temp_directory_path() / ("goliath_game_system_" + name + ".json");

    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << content;
    out.close();
    return path;
}

} // namespace

TEST_CASE("legacy game entries default to Neo Geo cartridge system", "[game_model][system]") {
    const fs::path file = make_system_test_file(
        "legacy",
        R"json([{
            "name": "Metal Slug",
            "display": "Metal Slug",
            "short": "mslug",
            "source": "mame",
            "roms": []
        }])json"
    );

    const auto games = load_games(file);
    REQUIRE(games.size() == 1);
    REQUIRE(games.front().system == "neogeo");
    fs::remove(file);
}

TEST_CASE("game model accepts Neo Geo CD system", "[game_model][system]") {
    const fs::path file = make_system_test_file(
        "neogeocd",
        R"json([{
            "name": "Metal Slug (Neo Geo CD)",
            "display": "Metal Slug",
            "short": "mslug",
            "source": "mame",
            "system": "neogeocd",
            "roms": []
        }])json"
    );

    const auto games = load_games(file);
    REQUIRE(games.size() == 1);
    REQUIRE(games.front().system == "neogeocd");
    fs::remove(file);
}

TEST_CASE("game model rejects unknown system", "[game_model][system]") {
    const fs::path file = make_system_test_file(
        "invalid",
        R"json([{
            "name": "Metal Slug",
            "display": "Metal Slug",
            "short": "mslug",
            "source": "mame",
            "system": "banana",
            "roms": []
        }])json"
    );

    const auto games = load_games(file);
    REQUIRE(games.empty());
    fs::remove(file);
}

TEST_CASE("Neo Geo CD path and library selector have safe defaults", "[config][system]") {
    const auto cfg = default_config();
    REQUIRE(cfg.get("Paths", "neocd", "") == "neocd");
    REQUIRE(cfg.get("UI", "library_system", "") == "neogeo");
}

TEST_CASE("game model accepts unidentified Neo Geo CD entries", "[game_model][system][neocd]") {
    const fs::path file = make_system_test_file(
        "neogeocd_unknown",
        R"json([{
            "name": "Andro Dunos",
            "display": "Andro Dunos",
            "short": "cd_unknown_1234567890abcdef",
            "source": "unknown",
            "system": "neogeocd",
            "identified": false,
            "main_rom": "Andro Dunos (France) (Unl).cue",
            "roms": [{
                "file": "Andro Dunos (France) (Unl).cue",
                "mame": "cd_unknown_1234567890abcdef",
                "main": true
            }]
        }])json"
    );

    const auto games = load_games(file);
    REQUIRE(games.size() == 1);
    REQUIRE(games.front().system == "neogeocd");
    REQUIRE(games.front().source == "unknown");
    REQUIRE_FALSE(games.front().identified);
    REQUIRE(games.front().main_rom == "Andro Dunos (France) (Unl).cue");
    fs::remove(file);
}

TEST_CASE("game model rejects unknown source for cartridge entries", "[game_model][system]") {
    const fs::path file = make_system_test_file(
        "cartridge_unknown",
        R"json([{
            "name": "Mystery Cart",
            "display": "Mystery Cart",
            "short": "mystery",
            "source": "unknown",
            "system": "neogeo",
            "identified": false,
            "roms": []
        }])json"
    );

    const auto games = load_games(file);
    REQUIRE(games.empty());
    fs::remove(file);
}

TEST_CASE("game model accepts verified Redump-only Neo Geo CD entries", "[game_model][system][neocd][redump]") {
    const fs::path file = make_system_test_file(
        "neogeocd_redump_verified",
        R"json([{
            "name": "Andro Dunos (France) (Unl)",
            "display": "Andro Dunos",
            "short": "cd_redump_69152",
            "source": "redump",
            "system": "neogeocd",
            "identified": true,
            "verification": "redump-cue",
            "main_rom": "Mystery Disc.cue",
            "roms": [{
                "file": "Mystery Disc.cue",
                "mame": "cd_redump_69152",
                "main": true
            }]
        }])json"
    );

    const auto games = load_games(file);
    REQUIRE(games.size() == 1);
    REQUIRE(games.front().system == "neogeocd");
    REQUIRE(games.front().source == "redump");
    REQUIRE(games.front().identified);
    REQUIRE(games.front().verification == std::optional<std::string>("redump-cue"));
    fs::remove(file);
}

TEST_CASE("game model accepts Redump track-only Neo Geo CD identity", "[game_model][system][neocd][redump]") {
    const fs::path file = make_system_test_file(
        "neogeocd_redump_tracks_only",
        R"json([{
            "name": "Andro Dunos (France) (Unl)",
            "display": "Andro Dunos",
            "short": "cd_redump_69152",
            "source": "redump",
            "system": "neogeocd",
            "identified": true,
            "verification": "redump-tracks-only",
            "main_rom": "Modified Disc.cue",
            "roms": [{
                "file": "Modified Disc.cue",
                "mame": "cd_redump_69152",
                "main": true
            }]
        }])json"
    );

    const auto games = load_games(file);
    REQUIRE(games.size() == 1);
    REQUIRE(games.front().system == "neogeocd");
    REQUIRE(games.front().source == "redump");
    REQUIRE(games.front().identified);
    REQUIRE(games.front().verification ==
            std::optional<std::string>("redump-tracks-only"));
    fs::remove(file);
}
