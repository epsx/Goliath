#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace goliath {

enum class VisibleSelectionDecision {
    KeepCurrent,
    SelectVisibleReplacement,
    ClearSelection,
};

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

inline const std::string& libraryDetailsTitle(
    bool isCd, const std::string& fullName,
    const std::string& displayName) noexcept {
    return isCd && !fullName.empty() ? fullName : displayName;
}

} // namespace goliath
