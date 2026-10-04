#include "update/update_schedule.hpp"

#include <charconv>

namespace goliath {

namespace {

constexpr std::int64_t kSecondsPerDay = 24 * 60 * 60;
constexpr std::int64_t kSecondsPerWeek = 7 * kSecondsPerDay;

} // namespace

UpdateCheckInterval parse_update_check_interval(
        std::string_view value) noexcept {
    if (value == "daily") return UpdateCheckInterval::Daily;
    if (value == "weekly") return UpdateCheckInterval::Weekly;
    return UpdateCheckInterval::Disabled;
}

std::string_view update_check_interval_key(
        UpdateCheckInterval interval) noexcept {
    switch (interval) {
    case UpdateCheckInterval::Daily:
        return "daily";
    case UpdateCheckInterval::Weekly:
        return "weekly";
    case UpdateCheckInterval::Disabled:
        return "disabled";
    }
    return "disabled";
}

std::optional<std::int64_t> parse_update_check_epoch(
        std::string_view value) noexcept {
    if (value.empty()) return std::nullopt;

    std::int64_t epoch = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto [next, result] = std::from_chars(begin, end, epoch);
    if (result != std::errc{} || next != end || epoch <= 0) {
        return std::nullopt;
    }
    return epoch;
}

bool automatic_update_check_due(
        UpdateCheckInterval interval,
        std::optional<std::int64_t> lastSuccessfulCheckEpoch,
        std::int64_t nowEpoch) noexcept {
    if (interval == UpdateCheckInterval::Disabled) return false;
    if (!lastSuccessfulCheckEpoch || nowEpoch <= 0 ||
        *lastSuccessfulCheckEpoch > nowEpoch) {
        return true;
    }

    const std::int64_t requiredAge = interval == UpdateCheckInterval::Daily
        ? kSecondsPerDay
        : kSecondsPerWeek;
    return nowEpoch - *lastSuccessfulCheckEpoch >= requiredAge;
}

} // namespace goliath
