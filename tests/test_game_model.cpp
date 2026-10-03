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

        R"json([
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
                        "alt_title": "メタルスラッグ",
                        "serial": "NGM-201 (MVS), NGH-201 (AES)",
                        "release": "19960419 (MVS), 19960524 (AES)",
                        "part": "cart",
                        "interface": "neo_cart",
                        "program_width": 16,
                        "program_endianness": "big",
                        "program_size": 1048576,
                        "fixed_size": 262144,
                        "audio_cpu_size": 262144,
                        "audio_data_size": 2097152,
                        "graphics_size": 3145728,
                        "verification": "geolith-crc32",
                        "crc32": "352441c2",
                        "expected_crc32": "352441c2",
                        "hashes": [{
                            "file": "mslug.neo",
                            "catalog_file": "mslug.neo",
                            "role": "Cartridge",
                            "algorithm": "CRC-32",
                            "size": 1048576,
                            "expected_size": null,
                            "actual": "352441c2",
                            "expected": "352441c2",
                            "matched": true
                        }],
                        "main": true
                    }
                ],

                "main_rom": "mslug.neo"
            }
        ])json"
    );

    auto games = load_games(json_file);

    REQUIRE(games.size() == 1);

    const Game& game = games.front();

    REQUIRE(game.name == "Metal Slug - Super Vehicle-001");
    REQUIRE(game.display == "Metal Slug");
    REQUIRE(game.short_name == "mslug");
    REQUIRE(game.source == "mame");
    REQUIRE_FALSE(game.redump_id.has_value());

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
    REQUIRE(game.roms[0].alt_title ==
            std::optional<std::string>("メタルスラッグ"));
    REQUIRE(game.roms[0].serial == std::optional<std::string>(
        "NGM-201 (MVS), NGH-201 (AES)"));
    REQUIRE(game.roms[0].release == std::optional<std::string>(
        "19960419 (MVS), 19960524 (AES)"));
    REQUIRE(game.roms[0].part ==
            std::optional<std::string>("cart"));
    REQUIRE(game.roms[0].interface ==
            std::optional<std::string>("neo_cart"));
    REQUIRE(game.roms[0].program_width ==
            std::optional<std::uintmax_t>(16));
    REQUIRE(game.roms[0].program_endianness ==
            std::optional<std::string>("big"));
    REQUIRE(game.roms[0].program_size ==
            std::optional<std::uintmax_t>(1048576));
    REQUIRE(game.roms[0].fixed_size ==
            std::optional<std::uintmax_t>(262144));
    REQUIRE(game.roms[0].audio_cpu_size ==
            std::optional<std::uintmax_t>(262144));
    REQUIRE(game.roms[0].audio_data_size ==
            std::optional<std::uintmax_t>(2097152));
    REQUIRE(game.roms[0].graphics_size ==
            std::optional<std::uintmax_t>(3145728));
    REQUIRE(game.roms[0].verification ==
            std::optional<std::string>("geolith-crc32"));
    REQUIRE(game.roms[0].crc32 ==
            std::optional<std::string>("352441c2"));
    REQUIRE(game.roms[0].expected_crc32 ==
            std::optional<std::string>("352441c2"));
    REQUIRE(game.roms[0].hashes.size() == 1);
    CHECK(game.roms[0].hashes[0].file == "mslug.neo");
    CHECK(game.roms[0].hashes[0].role == "Cartridge");
    CHECK(game.roms[0].hashes[0].algorithm == "CRC-32");
    CHECK(game.roms[0].hashes[0].size ==
          std::optional<std::uintmax_t>(1048576));
    CHECK(game.roms[0].hashes[0].actual ==
          std::optional<std::string>("352441c2"));
    CHECK(game.roms[0].hashes[0].matched);
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
