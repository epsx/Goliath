#pragma once

#include "game/game_model.hpp"

#include <cstdint>
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace goliath {

enum class VisibleSelectionDecision {
    KeepCurrent,
    SelectVisibleReplacement,
    ClearSelection,
};

enum class VerificationTone {
    Neutral,
    Success,
    Error,
};

struct LibraryVerificationPresentation {
    std::string label;
    std::string value;
    std::string tooltip;
    VerificationTone tone = VerificationTone::Neutral;
    bool details_available = false;
};

struct LibraryCatalogIdPresentation {
    std::string label;
    std::string value;
    std::string url;
};

inline LibraryCatalogIdPresentation libraryCatalogIdPresentation(
    const Game& game, const Rom* rom) {
    if (game.source == "redump") {
        const std::string id = game.redump_id.value_or("");
        bool numeric = !id.empty();
        for (const unsigned char ch : id) {
            if (ch < '0' || ch > '9') {
                numeric = false;
                break;
            }
        }
        return {
            "Redump ID", id,
            numeric ? "https://redump.info/disc/" + id : "",
        };
    }

    if (rom && !rom->mame.empty())
        return {"MAME ID", rom->mame, ""};
    return {"MAME ID", game.short_name, ""};
}

inline LibraryVerificationPresentation libraryVerificationPresentation(
    const Game& game, const Rom* rom) {
    if (!game.identified) {
        return {
            "Unidentified", "",
            "No matching MAME software-list entry was identified.",
            VerificationTone::Neutral, false,
        };
    }

    if (rom && rom->verification ==
                   std::optional<std::string>("geolith-crc32")) {
        return {
            "Geolith / CRC-32", rom->crc32.value_or("Verified"),
            "The .neo file CRC-32 matches metadata/geolith.xml. "
            "Open details for the expected and calculated values.",
            VerificationTone::Success, !rom->hashes.empty(),
        };
    }
    if (rom && rom->verification ==
                   std::optional<std::string>(
                       "geolith-crc32-mismatch")) {
        return {
            "Geolith / CRC-32", rom->crc32.value_or("Mismatch"),
            "The .neo file does not match metadata/geolith.xml. "
            "Open details to compare the expected and calculated CRC-32.",
            VerificationTone::Error, !rom->hashes.empty(),
        };
    }

    if (game.verification ==
        std::optional<std::string>("redump-cue")) {
        return {
            "Redump / SHA-1", "Verified",
            "The CUE descriptor and every physical track match the local "
            "Redump DAT by size and SHA-1.",
            VerificationTone::Success, rom && !rom->hashes.empty(),
        };
    }
    if (game.verification ==
        std::optional<std::string>("redump-tracks-only")) {
        return {
            "Redump / SHA-1", "CUE mismatch",
            "Every physical track matches Redump by size and SHA-1, but the "
            "CUE descriptor differs.",
            VerificationTone::Error, rom && !rom->hashes.empty(),
        };
    }
    if (game.verification ==
        std::optional<std::string>("mame-chd")) {
        return {
            "MAME CHD / SHA-1", "Verified",
            "The CHD combined header SHA-1 matches the local MAME "
            "neocd.xml catalog.",
            VerificationTone::Success, rom && !rom->hashes.empty(),
        };
    }
    if (game.verification ==
            std::optional<std::string>("mame-chd-mismatch") ||
        (rom && rom->verification ==
                    std::optional<std::string>("mame-chd-mismatch"))) {
        return {
            "MAME CHD / SHA-1", "Mismatch",
            "The CHD combined header SHA-1 does not match the filename-"
            "identified MAME neocd.xml entry.",
            VerificationTone::Error, !rom->hashes.empty(),
        };
    }

    return {
        "Metadata only", "",
        "The media name was matched to catalog metadata, but its content "
        "hash has not been verified.",
        VerificationTone::Neutral, false,
    };
}

enum class LibraryRatingFilter {
    Any,
    Rated,
    Unrated,
    AtLeast1,
    AtLeast2,
    AtLeast3,
    AtLeast4,
    AtLeast5,
};

enum class LibraryPlaytimeFilter {
    Any,
    Played,
    NotPlayed,
};

constexpr int libraryListRatingBadge(int rating) noexcept {
    return rating >= 1 && rating <= 5 ? rating : 0;
}

constexpr std::string_view libraryRatingFilterKey(
        LibraryRatingFilter filter) noexcept {
    switch (filter) {
    case LibraryRatingFilter::Rated: return "rated";
    case LibraryRatingFilter::Unrated: return "unrated";
    case LibraryRatingFilter::AtLeast1: return "at_least_1";
    case LibraryRatingFilter::AtLeast2: return "at_least_2";
    case LibraryRatingFilter::AtLeast3: return "at_least_3";
    case LibraryRatingFilter::AtLeast4: return "at_least_4";
    case LibraryRatingFilter::AtLeast5: return "at_least_5";
    case LibraryRatingFilter::Any: return "any";
    }
    return "any";
}

constexpr LibraryRatingFilter libraryRatingFilterFromKey(
        std::string_view key) noexcept {
    if (key == "rated") return LibraryRatingFilter::Rated;
    if (key == "unrated") return LibraryRatingFilter::Unrated;
    if (key == "at_least_1") return LibraryRatingFilter::AtLeast1;
    if (key == "at_least_2") return LibraryRatingFilter::AtLeast2;
    if (key == "at_least_3") return LibraryRatingFilter::AtLeast3;
    if (key == "at_least_4") return LibraryRatingFilter::AtLeast4;
    if (key == "at_least_5") return LibraryRatingFilter::AtLeast5;
    return LibraryRatingFilter::Any;
}

constexpr std::string_view libraryPlaytimeFilterKey(
        LibraryPlaytimeFilter filter) noexcept {
    switch (filter) {
    case LibraryPlaytimeFilter::Played: return "played";
    case LibraryPlaytimeFilter::NotPlayed: return "not_played";
    case LibraryPlaytimeFilter::Any: return "any";
    }
    return "any";
}

constexpr LibraryPlaytimeFilter libraryPlaytimeFilterFromKey(
        std::string_view key) noexcept {
    if (key == "played") return LibraryPlaytimeFilter::Played;
    if (key == "not_played") return LibraryPlaytimeFilter::NotPlayed;
    return LibraryPlaytimeFilter::Any;
}

constexpr bool libraryRatingFilterAllows(
        LibraryRatingFilter filter, int rating) noexcept {
    switch (filter) {
    case LibraryRatingFilter::Rated: return rating > 0;
    case LibraryRatingFilter::Unrated: return rating == 0;
    case LibraryRatingFilter::AtLeast1: return rating >= 1;
    case LibraryRatingFilter::AtLeast2: return rating >= 2;
    case LibraryRatingFilter::AtLeast3: return rating >= 3;
    case LibraryRatingFilter::AtLeast4: return rating >= 4;
    case LibraryRatingFilter::AtLeast5: return rating >= 5;
    case LibraryRatingFilter::Any: return true;
    }
    return true;
}

constexpr bool libraryPlaytimeFilterAllows(
        LibraryPlaytimeFilter filter, std::int64_t totalSeconds) noexcept {
    switch (filter) {
    case LibraryPlaytimeFilter::Played: return totalSeconds > 0;
    case LibraryPlaytimeFilter::NotPlayed: return totalSeconds <= 0;
    case LibraryPlaytimeFilter::Any: return true;
    }
    return true;
}

constexpr bool libraryPersonalFiltersAllow(
        LibraryRatingFilter ratingFilter,
        LibraryPlaytimeFilter playtimeFilter,
        int rating,
        std::int64_t totalSeconds) noexcept {
    return libraryRatingFilterAllows(ratingFilter, rating) &&
           libraryPlaytimeFilterAllows(playtimeFilter, totalSeconds);
}

constexpr bool libraryPersonalFiltersActive(
        LibraryRatingFilter ratingFilter,
        LibraryPlaytimeFilter playtimeFilter) noexcept {
    return ratingFilter != LibraryRatingFilter::Any ||
           playtimeFilter != LibraryPlaytimeFilter::Any;
}

constexpr bool librarySystemChangeRequired(
        std::string_view currentSystem,
        std::string_view requestedSystem) noexcept {
    return currentSystem != requestedSystem;
}

constexpr bool librarySelectionAllowed(bool effectivelyVisible,
                                       bool filteredView,
                                       bool exactMatch) noexcept {
    return effectivelyVisible && (!filteredView || exactMatch);
}

// A filter may temporarily expand a parent solely to reveal an exact matching
// variant. That expansion is presentation state, not a user expansion to carry
// into the next rebuilt view.
constexpr bool libraryExpansionShouldBePreserved(
        bool expanded, bool filterAutoExpanded) noexcept {
    return expanded && !filterAutoExpanded;
}

// Zero represents a missing rating/playtime value. Missing values stay last in
// both directions; equal values are left for the caller's stable name tie-break.
constexpr bool libraryMetricPrecedes(std::int64_t left,
                                     std::int64_t right,
                                     bool descending) noexcept {
    const bool leftPresent = left > 0;
    const bool rightPresent = right > 0;
    if (leftPresent != rightPresent) return leftPresent;
    if (!leftPresent || left == right) return false;
    return descending ? left > right : left < right;
}

constexpr VisibleSelectionDecision visibleSelectionDecision(
    bool currentSelectionVisible, bool hasVisibleItems) noexcept {
    if (currentSelectionVisible)
        return VisibleSelectionDecision::KeepCurrent;
    return hasVisibleItems
               ? VisibleSelectionDecision::SelectVisibleReplacement
               : VisibleSelectionDecision::ClearSelection;
}

constexpr bool visibleSelectionShouldBeRevealed(
    VisibleSelectionDecision decision) noexcept {
    return decision != VisibleSelectionDecision::ClearSelection;
}

constexpr bool treeItemHasExpandableChildren(int childCount) noexcept {
    return childCount > 0;
}

constexpr bool libraryVariantAllowed(bool showVariants,
                                     bool favoritesOnly,
                                     bool variantFavorite,
                                     bool personalFiltersActive = false,
                                     bool variantFilterMatch = false) noexcept {
    return showVariants || (favoritesOnly && variantFavorite) ||
           (personalFiltersActive && variantFilterMatch);
}

constexpr int detailsScrollTarget(bool selectionChanged,
                                  int currentValue) noexcept {
    return selectionChanged ? 0 : currentValue;
}

inline std::string libraryMvsAesHumanTitle(
    const std::string& fullName, const std::string& displayName) {
    if (fullName.empty()) return displayName;

    std::string title = fullName;
    std::size_t searchFrom = 0;
    while (true) {
        const std::size_t open = title.find('(', searchFrom);
        if (open == std::string::npos) break;
        const std::size_t close = title.find(')', open + 1);
        if (close == std::string::npos) break;

        const std::string_view qualifier(title.data() + open + 1,
                                         close - open - 1);
        bool catalogCode = false;
        for (std::size_t index = 1; index + 1 < qualifier.size(); ++index) {
            if (qualifier[index] != '-' ||
                qualifier[index + 1] < '0' ||
                qualifier[index + 1] > '9') {
                continue;
            }

            std::size_t uppercaseLetters = 0;
            std::size_t cursor = index;
            while (cursor > 0 && qualifier[cursor - 1] >= 'A' &&
                   qualifier[cursor - 1] <= 'Z') {
                --cursor;
                ++uppercaseLetters;
            }
            if (uppercaseLetters >= 2) {
                catalogCode = true;
                break;
            }
        }
        if (!catalogCode) {
            searchFrom = close + 1;
            continue;
        }

        std::size_t eraseFrom = open;
        while (eraseFrom > 0 && title[eraseFrom - 1] == ' ')
            --eraseFrom;
        title.erase(eraseFrom, close - eraseFrom + 1);
        searchFrom = eraseFrom;
    }

    const std::size_t first = title.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return displayName;
    const std::size_t last = title.find_last_not_of(" \t\r\n");
    return title.substr(first, last - first + 1);
}

inline std::string libraryDetailsTitle(
    bool isCd, const std::string& fullName,
    const std::string& displayName) {
    if (isCd) return fullName.empty() ? displayName : fullName;
    return libraryMvsAesHumanTitle(fullName, displayName);
}

namespace detail {

inline std::string cdListTrim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

inline std::string cdListLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return text;
}

inline std::vector<std::string> cdListCommaTokens(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t comma = text.find(',', begin);
        const std::size_t end = comma == std::string_view::npos
            ? text.size() : comma;
        std::string token = cdListTrim(text.substr(begin, end - begin));
        if (!token.empty()) tokens.push_back(std::move(token));
        if (comma == std::string_view::npos) break;
        begin = comma + 1;
    }
    return tokens;
}

inline std::string cdListCanonicalRegion(std::string_view token) {
    const std::string lower = cdListLower(cdListTrim(token));
    static constexpr std::array<std::pair<std::string_view, std::string_view>,
                                18> regions{{
        {"world", "World"}, {"export", "Export"},
        {"japan", "Japan"}, {"usa", "USA"},
        {"europe", "Europe"}, {"asia", "Asia"},
        {"france", "France"}, {"germany", "Germany"},
        {"italy", "Italy"}, {"spain", "Spain"},
        {"korea", "Korea"}, {"brazil", "Brazil"},
        {"australia", "Australia"}, {"canada", "Canada"},
        {"unl", "Unl"}, {"demo", "Demo"},
        {"prototype", "Prototype"}, {"beta", "Beta"},
    }};
    for (const auto& [key, value] : regions) {
        if (lower == key) return std::string(value);
    }
    return {};
}

inline std::string cdListCanonicalLanguage(std::string_view token) {
    const std::string lower = cdListLower(cdListTrim(token));
    static constexpr std::array<std::string_view, 20> languages{{
        "en", "ja", "es", "pt", "fr", "de", "it", "ko", "zh", "nl",
        "ru", "sv", "no", "da", "fi", "pl", "cs", "hu", "tr", "el",
    }};
    if (std::find(languages.begin(), languages.end(), lower) ==
        languages.end()) {
        return {};
    }
    std::string canonical = lower;
    canonical.front() = static_cast<char>(
        std::toupper(static_cast<unsigned char>(canonical.front())));
    return canonical;
}

inline std::string cdListCanonicalRevision(std::string_view token) {
    const std::string trimmed = cdListTrim(token);
    const std::string lower = cdListLower(trimmed);
    std::size_t valueStart = std::string::npos;
    if (lower.starts_with("revision ")) valueStart = 9;
    else if (lower.starts_with("rev ")) valueStart = 4;
    else if (lower.starts_with("rev.")) valueStart = 4;
    if (valueStart == std::string::npos) return {};

    const std::string value = cdListTrim(
        std::string_view(trimmed).substr(valueStart));
    return value.empty() ? std::string{} : "Rev " + value;
}

struct CdListQualifiers {
    std::vector<std::string> regions;
    std::string revision;
    std::vector<std::string> languages;
};

inline void cdListAppendUnique(std::vector<std::string>& values,
                               const std::string& value) {
    if (!value.empty() &&
        std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

inline CdListQualifiers cdListQualifiers(std::string_view fullName) {
    CdListQualifiers result;
    std::size_t searchFrom = 0;
    while (true) {
        const std::size_t open = fullName.find('(', searchFrom);
        if (open == std::string_view::npos) break;
        const std::size_t close = fullName.find(')', open + 1);
        if (close == std::string_view::npos) break;

        const std::vector<std::string> tokens = cdListCommaTokens(
            fullName.substr(open + 1, close - open - 1));
        for (const std::string& token : tokens) {
            const std::string revision = cdListCanonicalRevision(token);
            if (!revision.empty()) {
                result.revision = revision;
                continue;
            }
            const std::string region = cdListCanonicalRegion(token);
            if (!region.empty()) {
                cdListAppendUnique(result.regions, region);
                continue;
            }
            cdListAppendUnique(result.languages,
                               cdListCanonicalLanguage(token));
        }
        searchFrom = close + 1;
    }
    return result;
}

inline std::string cdListJoin(const std::vector<std::string>& values,
                              std::string_view separator) {
    std::string result;
    for (const std::string& value : values) {
        if (!result.empty()) result += separator;
        result += value;
    }
    return result;
}

inline std::string cdListQualifierText(const Game& game,
                                       bool includeLanguages) {
    const CdListQualifiers qualifiers = cdListQualifiers(game.name);
    std::vector<std::string> parts = qualifiers.regions;
    if (!qualifiers.revision.empty())
        parts.push_back(qualifiers.revision);
    if (includeLanguages && !qualifiers.languages.empty())
        parts.push_back(cdListJoin(qualifiers.languages, ","));
    return cdListJoin(parts, " \xC2\xB7 ");
}

inline std::string cdListWithSuffix(const std::string& display,
                                    const std::string& suffix) {
    return suffix.empty() ? display : display + " [" + suffix + "]";
}

inline std::string cdListFallbackIdentity(const Game& game) {
    if (game.redump_id.has_value() && !game.redump_id->empty())
        return "Redump " + *game.redump_id;
    if (!game.short_name.empty())
        return game.source == "mame"
            ? "MAME " + game.short_name : game.short_name;
    if (!game.main_rom.has_value() || game.main_rom->empty()) return {};

    std::string filename = *game.main_rom;
    const std::size_t slash = filename.find_last_of("/\\");
    if (slash != std::string::npos) filename.erase(0, slash + 1);
    const std::size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) filename.erase(dot);
    return filename;
}

} // namespace detail

// Returns the exact strings used by the game tree. MVS/AES remains unchanged.
// Neo Geo CD receives a compact region/revision suffix only when its cleaned
// display title collides with another CD entry. Languages and then a stable
// catalog identity are progressively added only when still needed.
inline std::vector<std::string> libraryGameListTitles(
        const std::vector<Game>& games) {
    std::vector<std::string> titles;
    titles.reserve(games.size());
    for (const Game& game : games) titles.push_back(game.display);

    std::unordered_map<std::string, std::size_t> baseCounts;
    for (const Game& game : games) {
        if (game.system == "neogeocd")
            ++baseCounts[detail::cdListLower(game.display)];
    }

    for (std::size_t index = 0; index < games.size(); ++index) {
        const Game& game = games[index];
        if (game.system != "neogeocd" ||
            baseCounts[detail::cdListLower(game.display)] < 2) {
            continue;
        }
        titles[index] = detail::cdListWithSuffix(
            game.display, detail::cdListQualifierText(game, false));
    }

    auto candidateCounts = [&](const std::vector<std::string>& values) {
        std::unordered_map<std::string, std::size_t> counts;
        for (std::size_t index = 0; index < games.size(); ++index) {
            if (games[index].system == "neogeocd" &&
                baseCounts[detail::cdListLower(games[index].display)] >= 2) {
                ++counts[detail::cdListLower(values[index])];
            }
        }
        return counts;
    };

    auto counts = candidateCounts(titles);
    for (std::size_t index = 0; index < games.size(); ++index) {
        const Game& game = games[index];
        if (game.system != "neogeocd" ||
            counts[detail::cdListLower(titles[index])] < 2) {
            continue;
        }
        titles[index] = detail::cdListWithSuffix(
            game.display, detail::cdListQualifierText(game, true));
    }

    counts = candidateCounts(titles);
    for (std::size_t index = 0; index < games.size(); ++index) {
        const Game& game = games[index];
        if (game.system != "neogeocd" ||
            counts[detail::cdListLower(titles[index])] < 2) {
            continue;
        }
        std::string suffix = detail::cdListQualifierText(game, true);
        const std::string identity = detail::cdListFallbackIdentity(game);
        if (!identity.empty()) {
            if (!suffix.empty()) suffix += " \xC2\xB7 ";
            suffix += identity;
        }
        titles[index] = detail::cdListWithSuffix(game.display, suffix);
    }
    return titles;
}

} // namespace goliath
