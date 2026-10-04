#include "catch2/catch.hpp"

#include "update/update_schedule.hpp"

using namespace goliath;

TEST_CASE("update check intervals use safe persisted keys", "[update]") {
    CHECK(parse_update_check_interval("disabled") ==
          UpdateCheckInterval::Disabled);
    CHECK(parse_update_check_interval("daily") == UpdateCheckInterval::Daily);
    CHECK(parse_update_check_interval("weekly") == UpdateCheckInterval::Weekly);
    CHECK(parse_update_check_interval("unexpected") ==
          UpdateCheckInterval::Disabled);

    CHECK(update_check_interval_key(UpdateCheckInterval::Disabled) ==
          "disabled");
    CHECK(update_check_interval_key(UpdateCheckInterval::Daily) == "daily");
    CHECK(update_check_interval_key(UpdateCheckInterval::Weekly) == "weekly");
}

TEST_CASE("update check timestamps reject malformed values", "[update]") {
    CHECK(parse_update_check_epoch("1728060000") == 1728060000);
    CHECK_FALSE(parse_update_check_epoch(""));
    CHECK_FALSE(parse_update_check_epoch("0"));
    CHECK_FALSE(parse_update_check_epoch("-1"));
    CHECK_FALSE(parse_update_check_epoch("tomorrow"));
    CHECK_FALSE(parse_update_check_epoch("1728060000x"));
}

TEST_CASE("disabled automatic update checks are never due", "[update]") {
    CHECK_FALSE(automatic_update_check_due(
        UpdateCheckInterval::Disabled, std::nullopt, 1728060000));
    CHECK_FALSE(automatic_update_check_due(
        UpdateCheckInterval::Disabled, 1, 1728060000));
}

TEST_CASE("daily and weekly update checks honor exact boundaries", "[update]") {
    constexpr std::int64_t now = 1728060000;
    constexpr std::int64_t day = 24 * 60 * 60;
    constexpr std::int64_t week = 7 * day;

    CHECK(automatic_update_check_due(
        UpdateCheckInterval::Daily, std::nullopt, now));
    CHECK_FALSE(automatic_update_check_due(
        UpdateCheckInterval::Daily, now - day + 1, now));
    CHECK(automatic_update_check_due(
        UpdateCheckInterval::Daily, now - day, now));

    CHECK_FALSE(automatic_update_check_due(
        UpdateCheckInterval::Weekly, now - week + 1, now));
    CHECK(automatic_update_check_due(
        UpdateCheckInterval::Weekly, now - week, now));
}

TEST_CASE("clock rollback causes a safe automatic recheck", "[update]") {
    CHECK(automatic_update_check_due(
        UpdateCheckInterval::Daily, 1728060001, 1728060000));
    CHECK(automatic_update_check_due(
        UpdateCheckInterval::Weekly, 1728060001, 1728060000));
}
