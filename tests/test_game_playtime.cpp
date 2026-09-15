#include "catch2/catch.hpp"

#include "game/game_playtime.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace fs = std::filesystem;

using goliath::format_playtime_seconds;
using goliath::GamePlaytimeRecord;
using goliath::GamePlaytimeStore;
using goliath::kMinimumTrackedSessionSeconds;
using goliath::kTrackedLaunchValidationSeconds;
using goliath::is_windows_loader_failure_exit_code;
using goliath::should_record_tracked_session;

namespace {

fs::path make_playtime_test_root(const std::string& name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("gpt_" + name + "_" + std::to_string(stamp));
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void write_text(const fs::path& path, const std::string& text) {
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("playtime duration formatting is compact and stable",
          "[game_playtime][format]") {
    CHECK(format_playtime_seconds(-1) == "Less than a minute");
    CHECK(format_playtime_seconds(0) == "Less than a minute");
    CHECK(format_playtime_seconds(59) == "Less than a minute");
    CHECK(format_playtime_seconds(60) == "1m");
    CHECK(format_playtime_seconds(125) == "2m");
    CHECK(format_playtime_seconds(3600) == "1h");
    CHECK(format_playtime_seconds(3725) == "1h 2m");
}

TEST_CASE("playtime store rejects launch failures and invalid identity",
          "[game_playtime][validation]") {
    GamePlaytimeStore store;

    CHECK_FALSE(store.add_session(
        "neogeo", "kof98.neo", kMinimumTrackedSessionSeconds - 1, 1000));
    CHECK_FALSE(store.add_session(
        "other", "kof98.neo", 30, 1000));
    CHECK_FALSE(store.add_session(
        "neogeo", "./", 30, 1000));
    CHECK_FALSE(store.add_session(
        "neogeo", "kof98.neo", 30, 0));
    CHECK(store.size() == 0);

    CHECK(store.add_session(
        "neogeo", "kof98.neo", kMinimumTrackedSessionSeconds, 1000));
    CHECK(store.size() == 1);
}

TEST_CASE("playtime validation rejects Windows loader failures even after a long dialog",
          "[game_playtime][launch-validation][windows]") {
    const std::uint32_t loaderFailures[] = {
        0xC000007Bu, // invalid image format
        0xC0000135u, // DLL not found
        0xC0000138u, // ordinal not found
        0xC0000139u, // entry point not found
        0xC0000142u, // DLL initialization failed
    };

    for (const std::uint32_t code : loaderFailures) {
        CHECK(is_windows_loader_failure_exit_code(code));
        CHECK_FALSE(should_record_tracked_session(10 * 60, code));
    }
    CHECK_FALSE(is_windows_loader_failure_exit_code(0));
    CHECK_FALSE(is_windows_loader_failure_exit_code(1));
}

TEST_CASE("playtime validation preserves real sessions and rejects early abnormal exits",
          "[game_playtime][launch-validation]") {
    CHECK_FALSE(should_record_tracked_session(
        kMinimumTrackedSessionSeconds - 1));
    CHECK(should_record_tracked_session(
        kMinimumTrackedSessionSeconds));
    CHECK(should_record_tracked_session(
        kMinimumTrackedSessionSeconds, 0));
    CHECK_FALSE(should_record_tracked_session(
        kTrackedLaunchValidationSeconds - 1, 1));
    CHECK(should_record_tracked_session(
        kTrackedLaunchValidationSeconds, 1));
}

TEST_CASE("playtime sessions accumulate on exact normalized media keys",
          "[game_playtime][key]") {
    GamePlaytimeStore store;
    REQUIRE(store.add_session("neogeo", "sets\\kof98.neo", 120, 2000));
    REQUIRE(store.add_session("neogeo", "sets/kof98.neo", 30, 1500));
    REQUIRE(store.add_session("neogeocd", "sets/kof98.neo", 60, 3000));

    const GamePlaytimeRecord* cartridge =
        store.find("neogeo", "sets/kof98.neo");
    REQUIRE(cartridge != nullptr);
    CHECK(cartridge->total_seconds == 150);
    CHECK(cartridge->session_count == 2);
    CHECK(cartridge->last_played_epoch == 2000);

    const GamePlaytimeRecord* cd =
        store.find("neogeocd", "sets\\kof98.neo");
    REQUIRE(cd != nullptr);
    CHECK(cd->total_seconds == 60);
    CHECK(cd->session_count == 1);
    CHECK(store.size() == 2);
}

TEST_CASE("playtime store round-trips through atomic persistence",
          "[game_playtime][persistence]") {
    const fs::path root = make_playtime_test_root("roundtrip");
    const fs::path path = root / "config" / "game_playtime.json";

    GamePlaytimeStore store;
    REQUIRE(store.add_session("neogeo", "mslug.neo", 120, 1000));
    REQUIRE(store.add_session(
        "neogeocd", "lastblade/disc.cue", 60, 2000));

    std::string error;
    REQUIRE(store.save(path, &error));
    CHECK(error.empty());
    CHECK(read_text(path).find("\"version\": 1") != std::string::npos);

    GamePlaytimeStore loaded;
    REQUIRE(loaded.load(path, &error));
    CHECK(error.empty());
    CHECK(loaded.size() == 2);

    const GamePlaytimeRecord* cartridge =
        loaded.find("neogeo", "mslug.neo");
    REQUIRE(cartridge != nullptr);
    CHECK(cartridge->total_seconds == 120);
    CHECK(cartridge->session_count == 1);
    CHECK(cartridge->last_played_epoch == 1000);

    const GamePlaytimeRecord* cd =
        loaded.find("neogeocd", "lastblade/disc.cue");
    REQUIRE(cd != nullptr);
    CHECK(cd->total_seconds == 60);

    fs::remove_all(root);
}

TEST_CASE("missing playtime file loads as an empty store",
          "[game_playtime][persistence]") {
    const fs::path root = make_playtime_test_root("missing");

    GamePlaytimeStore store;
    REQUIRE(store.add_session("neogeo", "kof98.neo", 60, 1000));
    std::string error;
    REQUIRE(store.load(root / "missing.json", &error));
    CHECK(error.empty());
    CHECK(store.size() == 0);

    fs::remove_all(root);
}

TEST_CASE("malformed playtime schema fails closed",
          "[game_playtime][persistence]") {
    const fs::path root = make_playtime_test_root("malformed");
    const fs::path path = root / "game_playtime.json";
    write_text(path, R"json({"version":99,"records":[]})json");

    GamePlaytimeStore store;
    REQUIRE(store.add_session("neogeo", "kof98.neo", 60, 1000));
    std::string error;
    CHECK_FALSE(store.load(path, &error));
    CHECK_FALSE(error.empty());
    CHECK(store.size() == 0);

    fs::remove_all(root);
}

TEST_CASE("invalid stored playtime records are skipped individually",
          "[game_playtime][persistence][validation]") {
    const fs::path root = make_playtime_test_root("invalid_records");
    const fs::path path = root / "game_playtime.json";
    write_text(path, R"json({
  "version": 1,
  "records": [
    {
      "system": "neogeo",
      "media": "valid\\game.neo",
      "total_seconds": 120,
      "session_count": 2,
      "last_played_epoch": 3000
    },
    {
      "system": "neogeo",
      "media": "negative.neo",
      "total_seconds": -1,
      "session_count": 1,
      "last_played_epoch": 3000
    },
    {
      "system": "neogeo",
      "media": "no_sessions.neo",
      "total_seconds": 120,
      "session_count": 0,
      "last_played_epoch": 3000
    },
    {
      "system": "neogeocd",
      "media": "no_date.cue",
      "total_seconds": 120,
      "session_count": 1,
      "last_played_epoch": 0
    },
    {
      "system": "other",
      "media": "foreign.rom",
      "total_seconds": 120,
      "session_count": 1,
      "last_played_epoch": 3000
    }
  ]
})json");

    GamePlaytimeStore store;
    std::string error;
    REQUIRE(store.load(path, &error));
    CHECK(error.empty());
    CHECK(store.size() == 1);
    CHECK(store.find("neogeo", "valid/game.neo") != nullptr);
    CHECK(store.find("neogeo", "negative.neo") == nullptr);
    CHECK(store.find("neogeo", "no_sessions.neo") == nullptr);

    fs::remove_all(root);
}

TEST_CASE("playtime arithmetic saturates instead of overflowing",
          "[game_playtime][persistence][overflow]") {
    const fs::path root = make_playtime_test_root("saturation");
    const fs::path path = root / "game_playtime.json";
    write_text(path, R"json({
  "version": 1,
  "records": [
    {
      "system": "neogeo",
      "media": "kof98.neo",
      "total_seconds": 9223372036854775805,
      "session_count": 9223372036854775807,
      "last_played_epoch": 1000
    }
  ]
})json");

    GamePlaytimeStore store;
    std::string error;
    REQUIRE(store.load(path, &error));
    REQUIRE(store.add_session("neogeo", "kof98.neo", 5, 2000));

    const GamePlaytimeRecord* record =
        store.find("neogeo", "kof98.neo");
    REQUIRE(record != nullptr);
    CHECK(record->total_seconds == std::numeric_limits<std::int64_t>::max());
    CHECK(record->session_count == std::numeric_limits<std::int64_t>::max());
    CHECK(record->last_played_epoch == 2000);

    fs::remove_all(root);
}

TEST_CASE("playtime save reports an unusable destination",
          "[game_playtime][persistence]") {
    const fs::path root = make_playtime_test_root("save_failure");
    const fs::path blocker = root / "not_a_directory";
    const fs::path path = blocker / "game_playtime.json";
    const std::string sentinel = "keep blocker bytes unchanged\n";
    write_text(blocker, sentinel);

    GamePlaytimeStore store;
    REQUIRE(store.add_session("neogeo", "kof98.neo", 60, 1000));
    std::string error;
    CHECK_FALSE(store.save(path, &error));
    CHECK_FALSE(error.empty());
    CHECK(read_text(blocker) == sentinel);

    REQUIRE(fs::remove(blocker));
    REQUIRE(store.save(path, &error));
    CHECK(error.empty());

    GamePlaytimeStore recovered;
    REQUIRE(recovered.load(path, &error));
    CHECK(error.empty());
    const GamePlaytimeRecord* record =
        recovered.find("neogeo", "kof98.neo");
    REQUIRE(record != nullptr);
    CHECK(record->total_seconds == 60);
    CHECK(record->session_count == 1);
    CHECK(record->last_played_epoch == 1000);

    fs::remove_all(root);
}
