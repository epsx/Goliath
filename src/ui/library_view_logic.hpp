#pragma once

#include "game/game_model.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace goliath {

enum class LibraryDisplayMode {
    List,
    Grid,
    BigIcons,
};

constexpr std::string_view libraryDisplayModeKey(LibraryDisplayMode mode) {
    switch (mode) {
    case LibraryDisplayMode::List: return "list";
    case LibraryDisplayMode::Grid: return "grid";
    case LibraryDisplayMode::BigIcons: return "big_icons";
    }
    return "list";
}

constexpr LibraryDisplayMode libraryDisplayModeFromKey(
        std::string_view key) {
    if (key == "grid") return LibraryDisplayMode::Grid;
    if (key == "big_icons") return LibraryDisplayMode::BigIcons;
    return LibraryDisplayMode::List;
}

constexpr std::string_view libraryDisplayModeLabel(
        LibraryDisplayMode mode) noexcept {
    switch (mode) {
    case LibraryDisplayMode::List: return "List";
    case LibraryDisplayMode::Grid: return "Grid";
    case LibraryDisplayMode::BigIcons: return "Big Grid Icons";
    }
    return "List";
}

constexpr int libraryDisplayModeIconPixels(LibraryDisplayMode mode) {
    switch (mode) {
    case LibraryDisplayMode::List: return 32;
    case LibraryDisplayMode::Grid: return 64;
    case LibraryDisplayMode::BigIcons: return 96;
    }
    return 32;
}

// The delegate paints explicit theme backgrounds, so use the flattened visible
// row position to keep parent and child stripes continuous across group edges.
constexpr bool libraryListVisibleRowUsesAlternateBackground(
        int visibleRow) noexcept {
    return visibleRow % 2 != 0;
}

struct LibraryTileMetrics {
    int width;
    int height;
    int artwork_width;
    int artwork_height;
};

constexpr LibraryTileMetrics libraryTileMetrics(
        LibraryDisplayMode mode) noexcept {
    switch (mode) {
    case LibraryDisplayMode::Grid:
        return {132, 148, 108, 82};
    case LibraryDisplayMode::BigIcons:
        return {164, 190, 136, 124};
    case LibraryDisplayMode::List:
        return {0, 0, 32, 32};
    }
    return {0, 0, 32, 32};
}

constexpr int libraryResponsiveGridCellWidth(
        int viewportWidth, int baseWidth, int spacing) noexcept {
    if (baseWidth <= 0) return 0;
    if (viewportWidth <= 0) return baseWidth;

    const int safeSpacing = spacing > 0 ? spacing : 0;
    const int outerSpacing = safeSpacing * 2;
    const int contentWidth = viewportWidth > outerSpacing
        ? viewportWidth - outerSpacing
        : viewportWidth;
    const int columnStride = baseWidth + safeSpacing;
    const int fittedColumns = columnStride > 0
        ? (contentWidth + safeSpacing) / columnStride
        : 1;
    const int columns = fittedColumns > 0 ? fittedColumns : 1;
    const int distributedWidth =
        (contentWidth - (columns - 1) * safeSpacing) / columns;
    return distributedWidth > baseWidth ? distributedWidth : baseWidth;
}

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

struct LibraryMediaCounts {
    std::size_t groups = 0;
    std::size_t media = 0;
    std::size_t parents = 0;
    std::size_t variants = 0;
};

LibraryMediaCounts libraryMediaCounts(
    const std::vector<Game>& games, std::string_view system);

LibraryCatalogIdPresentation libraryCatalogIdPresentation(
    const Game& game, const Rom* rom);

LibraryVerificationPresentation libraryVerificationPresentation(
    const Game& game, const Rom* rom);

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

class LibraryListViewportMemory {
public:
    void remember(std::string_view system, int verticalPosition);
    std::optional<int> recall(std::string_view system) const;

private:
    std::map<std::string, int> m_positions;
};

constexpr std::string_view librarySelectionConfigKey(
        std::string_view system) noexcept {
    if (system == "neogeo") return "last_rom_neogeo";
    if (system == "neogeocd") return "last_rom_neogeocd";
    return "last_rom";
}

std::string libraryRememberedSelection(
        std::string_view preferredRom,
        std::string_view systemRom,
        std::string_view legacyRom);

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

std::string libraryMvsAesHumanTitle(
    const std::string& fullName, const std::string& displayName);

std::string libraryTileTitle(const Game& game, const Rom* rom);

std::string libraryListVariantTooltip(
        const Game& game, const Rom& rom);

std::string libraryDetailsTitle(
    bool isCd, const std::string& fullName,
    const std::string& displayName);

// Returns the exact strings used by the game tree. MVS/AES remains unchanged.
// Neo Geo CD receives a compact region/revision suffix only when its cleaned
// display title collides with another CD entry. Languages and then a stable
// catalog identity are progressively added only when still needed.
std::vector<std::string> libraryGameListTitles(
        const std::vector<Game>& games);

} // namespace goliath
