#include "catch2/catch.hpp"

#include "game/game_model.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using goliath::Game;
using goliath::load_games;

namespace {

fs::path make_test_file(
    const std::string& name,
    const std::string& content
) {
    fs::path path =
        fs::temp_directory_path() /
        ("goliath_game_model_" + name + ".json");

    std::ofstream out(path, std::ios::binary);

    REQUIRE(out.good());

    out << content;
    out.close();

    return path;
}

} // namespace

TEST_CASE(
    "game model loads valid game",
    "[game_model]"
) {
    fs::path json_file = make_test_file(
        "valid",

        R"([
            {
                "name": "Metal Slug - Super Vehicle-001",
                "display": "Metal Slug",
                "short": "mslug",
                "source": "mame",

                "year": "1996",
                "manufacturer": "Nazca",
                "developer": null,
                "publisher": null,

                "genre": "Platform / Shooter Scrolling",
                "players": "2P sim",
                "series": "Metal Slug",
                "history": "Neo Geo history",

                "icon": null,
                "snapshot": null,

                "roms": [
                    {
                        "file": "mslug.neo",
                        "name": "Metal Slug - Super Vehicle-001",
                        "label": null,
                        "mame": "mslug",
                        "cloneof": null,
                        "main": true
                    }
                ],

                "main_rom": "mslug.neo"
            }
        ])"
    );

    auto games = load_games(json_file);

    REQUIRE(games.size() == 1);

    const Game& game = games.front();

    REQUIRE(game.name == "Metal Slug - Super Vehicle-001");
    REQUIRE(game.display == "Metal Slug");
    REQUIRE(game.short_name == "mslug");
    REQUIRE(game.source == "mame");

    REQUIRE(game.year.has_value());
    REQUIRE(*game.year == "1996");

    REQUIRE(game.manufacturer.has_value());
    REQUIRE(*game.manufacturer == "Nazca");

    REQUIRE_FALSE(game.developer.has_value());
    REQUIRE_FALSE(game.publisher.has_value());

    REQUIRE(game.genre.has_value());
    REQUIRE(*game.genre == "Platform / Shooter Scrolling");

    REQUIRE(game.players.has_value());
    REQUIRE(*game.players == "2P sim");

    REQUIRE(game.series.has_value());
    REQUIRE(*game.series == "Metal Slug");

    REQUIRE(game.roms.size() == 1);

    REQUIRE(game.roms[0].file == "mslug.neo");
    REQUIRE(game.roms[0].name.has_value());
    REQUIRE(*game.roms[0].name == "Metal Slug - Super Vehicle-001");
    REQUIRE(game.roms[0].mame == "mslug");
    REQUIRE(game.roms[0].main);

    REQUIRE(game.main_rom.has_value());
    REQUIRE(*game.main_rom == "mslug.neo");

    fs::remove(json_file);
}

TEST_CASE(
    "game model rejects game with missing required fields",
    "[game_model]"
) {
    const std::vector<std::string> json_documents = {

        R"([
            {
                "display": "Metal Slug",
                "short": "mslug",
                "source": "mame",
                "roms": []
            }
        ])",

        R"([
            {
                "name": "Metal Slug",
                "short": "mslug",
                "source": "mame",
                "roms": []
            }
        ])",

        R"([
            {
                "name": "Metal Slug",
                "display": "Metal Slug",
                "source": "mame",
                "roms": []
            }
        ])",

        R"([
            {
                "name": "Metal Slug",
                "display": "Metal Slug",
                "short": "mslug",
                "roms": []
            }
        ])"
    };

    for (std::size_t i = 0; i < json_documents.size(); ++i) {

        fs::path file = make_test_file(
            "missing_required_" + std::to_string(i),
            json_documents[i]
        );

        auto games = load_games(file);

        REQUIRE(games.empty());

        fs::remove(file);
    }
}

TEST_CASE(
    "game model rejects game without roms array",
    "[game_model]"
) {
    fs::path json_file = make_test_file(
        "missing_roms",

        R"([
            {
                "name": "Metal Slug",
                "display": "Metal Slug",
                "short": "mslug",
                "source": "mame"
            }
        ])"
    );

    auto games = load_games(json_file);

    REQUIRE(games.empty());

    fs::remove(json_file);
}

TEST_CASE(
    "game model skips invalid rom entries",
    "[game_model]"
) {
    fs::path json_file = make_test_file(
        "invalid_rom",

        R"([
            {
                "name": "Metal Slug",
                "display": "Metal Slug",
                "short": "mslug",
                "source": "mame",

                "roms": [
                    {
                        "file": "mslug.neo",
                        "label": null,
                        "mame": "mslug",
                        "cloneof": null,
                        "main": true
                    },

                    {
                        "file": "",
                        "label": null,
                        "mame": "broken",
                        "cloneof": null,
                        "main": false
                    }
                ],

                "main_rom": "mslug.neo"
            }
        ])"
    );

    auto games = load_games(json_file);

    REQUIRE(games.size() == 1);

    const Game& game = games.front();

    REQUIRE(game.roms.size() == 1);

    REQUIRE(game.roms[0].file == "mslug.neo");
    REQUIRE(game.roms[0].main);

    REQUIRE(game.main_rom.has_value());
    REQUIRE(*game.main_rom == "mslug.neo");

    fs::remove(json_file);
}

TEST_CASE(
    "game model clears invalid main_rom",
    "[game_model]"
) {
    fs::path json_file = make_test_file(
        "invalid_main_rom",

        R"([
            {
                "name": "Metal Slug",
                "display": "Metal Slug",
                "short": "mslug",
                "source": "mame",

                "roms": [
                    {
                        "file": "mslug.neo",
                        "label": null,
                        "mame": "mslug",
                        "cloneof": null,
                        "main": true
                    }
                ],

                "main_rom": "does-not-exist.neo"
            }
        ])"
    );

    auto games = load_games(json_file);

    REQUIRE(games.size() == 1);

    const Game& game = games.front();

    REQUIRE(game.roms.size() == 1);
    REQUIRE(game.roms[0].main);

    REQUIRE_FALSE(game.main_rom.has_value());

    fs::remove(json_file);
}

TEST_CASE(
    "game model clears main_rom pointing to non-main rom",
    "[game_model]"
) {
    fs::path json_file = make_test_file(
        "non_main_rom",

        R"([
            {
                "name": "Metal Slug",
                "display": "Metal Slug",
                "short": "mslug",
                "source": "mame",

                "roms": [
                    {
                        "file": "mslug.neo",
                        "label": null,
                        "mame": "mslug",
                        "cloneof": null,
                        "main": true
                    },

                    {
                        "file": "msluga.neo",
                        "label": "Prototype",
                        "mame": "msluga",
                        "cloneof": "mslug",
                        "main": false
                    }
                ],

                "main_rom": "msluga.neo"
            }
        ])"
    );

    auto games = load_games(json_file);

    REQUIRE(games.size() == 1);

    REQUIRE_FALSE(
        games.front().main_rom.has_value()
    );

    fs::remove(json_file);
}

TEST_CASE(
    "game model accepts only known source values",
    "[game_model]"
) {
    const std::vector<std::string> valid_sources = {
        "mame",
        "homebrew"
    };

    for (const auto& source : valid_sources) {

        fs::path file = make_test_file(
            "valid_source_" + source,

            R"([
                {
                    "name": "Metal Slug",
                    "display": "Metal Slug",
                    "short": "mslug",
                    "source": ")" + source + R"(",
                    "roms": []
                }
            ])"
        );

        auto games = load_games(file);

        REQUIRE(games.size() == 1);
        REQUIRE(games.front().source == source);

        fs::remove(file);
    }
}

TEST_CASE(
    "game model rejects unknown source value",
    "[game_model]"
) {
    fs::path file = make_test_file(
        "invalid_source",

        R"([
            {
                "name": "Metal Slug",
                "display": "Metal Slug",
                "short": "mslug",
                "source": "banana",
                "roms": []
            }
        ])"
    );

    auto games = load_games(file);

    REQUIRE(games.empty());

    fs::remove(file);
}
