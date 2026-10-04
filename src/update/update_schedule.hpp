// update_schedule.hpp - validated update-check preferences and deterministic
// interval decisions. Persistence and wall-clock access remain with the UI.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace goliath {

enum class UpdateCheckInterval {
    Disabled,
    Daily,
    Weekly,
};

UpdateCheckInterval parse_update_check_interval(std::string_view value) noexcept;
std::string_view update_check_interval_key(UpdateCheckInterval interval) noexcept;
std::optional<std::int64_t> parse_update_check_epoch(
    std::string_view value) noexcept;

bool automatic_update_check_due(
    UpdateCheckInterval interval,
    std::optional<std::int64_t> lastSuccessfulCheckEpoch,
    std::int64_t nowEpoch) noexcept;

} // namespace goliath
