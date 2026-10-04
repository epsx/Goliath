// release_feed.hpp - parsing and version selection for Goliath's published
// GitHub releases. Network access and presentation remain in the UI layer.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace goliath {

struct ReleaseVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool preview = false;
};

struct PublishedRelease {
    ReleaseVersion version;
    std::string tag;
    std::string name;
    std::string page_url;
    std::string published_at;
};

std::optional<ReleaseVersion> parse_release_version(std::string_view text);
int compare_release_versions(const ReleaseVersion& lhs,
                             const ReleaseVersion& rhs) noexcept;

std::optional<PublishedRelease> select_newest_published_release(
    std::string_view response,
    std::string* error = nullptr);

} // namespace goliath
