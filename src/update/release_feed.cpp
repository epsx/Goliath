#include "update/release_feed.hpp"

#include "json.hpp"

#include <charconv>
#include <exception>
#include <tuple>
#include <utility>

using nlohmann::json;

namespace goliath {

namespace {

std::optional<int> parse_version_component(std::string_view component) {
    if (component.empty()) return std::nullopt;

    int value = 0;
    const char* begin = component.data();
    const char* end = begin + component.size();
    const auto [next, result] = std::from_chars(begin, end, value);
    if (result != std::errc{} || next != end || value < 0) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> required_string(const json& object,
                                           const char* key) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) return std::nullopt;
    return it->get<std::string>();
}

std::string optional_string(const json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_string()
        ? it->get<std::string>()
        : std::string{};
}

bool is_draft(const json& object) {
    const auto it = object.find("draft");
    return it == object.end() || !it->is_boolean() || it->get<bool>();
}

} // namespace

std::optional<ReleaseVersion> parse_release_version(std::string_view text) {
    if (!text.empty() && text.front() == 'v') text.remove_prefix(1);

    bool preview = false;
    constexpr std::string_view previewSuffix = "-preview";
    if (text.size() >= previewSuffix.size() &&
        text.substr(text.size() - previewSuffix.size()) == previewSuffix) {
        preview = true;
        text.remove_suffix(previewSuffix.size());
    }

    const std::size_t firstDot = text.find('.');
    if (firstDot == std::string_view::npos) return std::nullopt;
    const std::size_t secondDot = text.find('.', firstDot + 1);
    if (secondDot == std::string_view::npos ||
        text.find('.', secondDot + 1) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto major = parse_version_component(text.substr(0, firstDot));
    const auto minor = parse_version_component(
        text.substr(firstDot + 1, secondDot - firstDot - 1));
    const auto patch = parse_version_component(text.substr(secondDot + 1));
    if (!major || !minor || !patch) return std::nullopt;

    return ReleaseVersion{*major, *minor, *patch, preview};
}

int compare_release_versions(const ReleaseVersion& lhs,
                             const ReleaseVersion& rhs) noexcept {
    const auto lhsNumbers = std::tie(lhs.major, lhs.minor, lhs.patch);
    const auto rhsNumbers = std::tie(rhs.major, rhs.minor, rhs.patch);
    if (lhsNumbers < rhsNumbers) return -1;
    if (rhsNumbers < lhsNumbers) return 1;
    if (lhs.preview == rhs.preview) return 0;
    return lhs.preview ? -1 : 1;
}

std::optional<PublishedRelease> select_newest_published_release(
        std::string_view response,
        std::string* error) {
    if (error) error->clear();

    try {
        const json releases = json::parse(response.begin(), response.end());
        if (!releases.is_array()) {
            if (error) *error = "GitHub returned an unexpected release list.";
            return std::nullopt;
        }

        std::optional<PublishedRelease> newest;
        for (const json& item : releases) {
            if (!item.is_object() || is_draft(item)) continue;

            const auto tag = required_string(item, "tag_name");
            const auto pageUrl = required_string(item, "html_url");
            if (!tag || !pageUrl) continue;

            const auto version = parse_release_version(*tag);
            if (!version) continue;

            PublishedRelease candidate;
            candidate.version = *version;
            candidate.tag = *tag;
            candidate.name = optional_string(item, "name");
            if (candidate.name.empty()) candidate.name = candidate.tag;
            candidate.page_url = *pageUrl;
            candidate.published_at = optional_string(item, "published_at");

            if (!newest ||
                compare_release_versions(candidate.version,
                                         newest->version) > 0) {
                newest = std::move(candidate);
            }
        }

        if (!newest && error) {
            *error = "No compatible published Goliath release was found.";
        }
        return newest;
    } catch (const std::exception& ex) {
        if (error) {
            *error = std::string("Could not parse GitHub's release list: ") +
                     ex.what();
        }
        return std::nullopt;
    }
}

} // namespace goliath
