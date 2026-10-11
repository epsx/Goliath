// update_alert.hpp - persistent metadata for the newest Goliath release that
// is newer than the installed build. The cache is deliberately separate from
// goliath.ini because release notes are multiline Markdown.
#pragma once

#include "update/release_feed.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace goliath {

// Stored beside goliath.ini so portable installations keep the alert across
// restarts and upgrades without depending on a user-selected content path.
std::filesystem::path update_alert_cache_path();

bool release_is_newer_than_installed(
    const PublishedRelease& release,
    std::string_view installedVersion);

// A missing cache is a valid empty state. Malformed or inaccessible files
// return no release and describe the problem through error.
std::optional<PublishedRelease> load_update_alert_cache(
    const std::filesystem::path& path,
    std::string* error = nullptr);

// Cache writes are atomic. Only the selected release is stored, never the
// complete GitHub feed.
bool save_update_alert_cache(
    const std::filesystem::path& path,
    const PublishedRelease& release,
    std::string* error = nullptr);

// Missing files already represent a cleared alert and are treated as success.
bool clear_update_alert_cache(
    const std::filesystem::path& path,
    std::string* error = nullptr);

} // namespace goliath
