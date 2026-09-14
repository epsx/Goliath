#include "game/game_library_state.hpp"

#include <catch2/catch.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

using goliath::GameLibraryStateStore;

namespace {

fs::path make_library_state_test_root(const std::string& name) {
    const auto stamp = std::chrono::high_resolution_clock::now()
                           .time_since_epoch()
                           .count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_library_state_" + name + "_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << text;
    REQUIRE(output.good());
}

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

} // namespace

TEST_CASE("missing personal library state is a valid empty store",
          "[library-state][favorites]") {
    const fs::path root = make_library_state_test_root("missing");
    GameLibraryStateStore store;
    std::string error;

    CHECK(store.load(root / "game_library_state.json", &error));
    CHECK(error.empty());
    CHECK(store.size() == 0);
    CHECK(store.favorite_count() == 0);
}

TEST_CASE("favorites use normalized exact-media identities",
          "[library-state][favorites][identity]") {
    GameLibraryStateStore store;

    store.set_favorite("neogeo", ".\\sets\\kof98.neo", true);
    store.set_favorite("neogeocd", "discs/../discs/KOF 98.cue", true);

    CHECK(store.is_favorite("neogeo", "sets/kof98.neo"));
    CHECK(store.is_favorite("neogeocd", "discs/KOF 98.cue"));
    CHECK_FALSE(store.is_favorite("neogeocd", "sets/kof98.neo"));
    CHECK(store.favorite_count("neogeo") == 1);
    CHECK(store.favorite_count("neogeocd") == 1);
    CHECK(store.favorite_count() == 2);
}

TEST_CASE("favorite state saves and reloads atomically",
          "[library-state][favorites][persistence]") {
    const fs::path root = make_library_state_test_root("roundtrip");
    const fs::path path = root / "config/game_library_state.json";

    GameLibraryStateStore saved;
    saved.set_favorite("neogeo", "sets\\garou.neo", true);
    saved.set_favorite("neogeocd", "Metal Slug/Metal Slug.chd", true);

    std::string error;
    REQUIRE(saved.save(path, &error));
    REQUIRE(error.empty());
    REQUIRE(fs::is_regular_file(path));

    const std::string serialized = read_text(path);
    CHECK(serialized.find("sets/garou.neo") != std::string::npos);
    CHECK(serialized.find("sets\\\\garou.neo") == std::string::npos);
    CHECK(serialized.find("\"version\": 1") != std::string::npos);

    GameLibraryStateStore loaded;
    REQUIRE(loaded.load(path, &error));
    CHECK(error.empty());
    CHECK(loaded.is_favorite("neogeo", "sets/garou.neo"));
    CHECK(loaded.is_favorite("neogeocd", "Metal Slug/Metal Slug.chd"));
    CHECK(loaded.size() == 2);
}

TEST_CASE("removing a favorite removes its otherwise empty record",
          "[library-state][favorites]") {
    GameLibraryStateStore store;
    store.set_favorite("neogeo", "aof.neo", true);
    REQUIRE(store.size() == 1);

    store.set_favorite("neogeo", "aof.neo", false);
    CHECK_FALSE(store.is_favorite("neogeo", "aof.neo"));
    CHECK(store.size() == 0);
}

TEST_CASE("invalid favorite records are ignored safely",
          "[library-state][favorites][validation]") {
    const fs::path root = make_library_state_test_root("validation");
    const fs::path path = root / "game_library_state.json";
    write_text(path, R"({
  "version": 1,
  "records": [
    {"system":"unknown", "media":"bad.neo", "favorite":true},
    {"system":"neogeo", "media":"", "favorite":true},
    {"system":"neogeo", "media":"bad-type.neo", "favorite":1},
    {"system":"neogeo", "media":"kof98.neo", "favorite":true},
    {"system":"neogeo", "media":"kof98.neo", "favorite":false},
    {"system":"neogeo", "media":"garou.neo", "favorite":true}
  ]
})");

    GameLibraryStateStore store;
    std::string error;
    REQUIRE(store.load(path, &error));
    CHECK(error.empty());
    CHECK_FALSE(store.is_favorite("neogeo", "kof98.neo"));
    CHECK(store.is_favorite("neogeo", "garou.neo"));
    CHECK(store.size() == 1);
}

TEST_CASE("malformed personal library state never keeps stale favorites",
          "[library-state][favorites][validation]") {
    const fs::path root = make_library_state_test_root("malformed");
    const fs::path path = root / "game_library_state.json";

    GameLibraryStateStore store;
    store.set_favorite("neogeo", "stale.neo", true);
    write_text(path, "{ not valid json");

    std::string error;
    CHECK_FALSE(store.load(path, &error));
    CHECK_FALSE(error.empty());
    CHECK(store.size() == 0);
}

TEST_CASE("favorites survive replacement of the generated game database",
          "[library-state][favorites][rescan]") {
    const fs::path root = make_library_state_test_root("rescan");
    const fs::path statePath = root / "config/game_library_state.json";
    const fs::path databasePath = root / "database/games.json";

    GameLibraryStateStore before;
    before.set_favorite("neogeo", "kof98.neo", true);
    REQUIRE(before.save(statePath));

    write_text(databasePath, "{\"games\":[]}");
    write_text(databasePath, "{\"games\":[{\"short\":\"kof98\"}]}");

    GameLibraryStateStore after;
    REQUIRE(after.load(statePath));
    CHECK(after.is_favorite("neogeo", "kof98.neo"));
    CHECK(after.size() == 1);
}
