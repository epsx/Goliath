#include "ui/library_view_logic.hpp"

#include <catch2/catch.hpp>

using goliath::VisibleSelectionDecision;
using goliath::LibraryPlaytimeFilter;
using goliath::LibraryRatingFilter;
using goliath::detailsScrollTarget;
using goliath::libraryDetailsTitle;
using goliath::libraryExpansionShouldBePreserved;
using goliath::libraryMetricPrecedes;
using goliath::libraryPersonalFiltersActive;
using goliath::libraryPersonalFiltersAllow;
using goliath::libraryPlaytimeFilterFromKey;
using goliath::libraryPlaytimeFilterKey;
using goliath::libraryRatingFilterFromKey;
using goliath::libraryRatingFilterKey;
using goliath::librarySelectionAllowed;
using goliath::libraryVariantAllowed;
using goliath::treeItemHasExpandableChildren;
using goliath::visibleSelectionDecision;
using goliath::visibleSelectionShouldBeRevealed;

TEST_CASE("visible library selections remain stable", "[library-view]") {
    CHECK(visibleSelectionDecision(true, true) ==
          VisibleSelectionDecision::KeepCurrent);
    CHECK(visibleSelectionDecision(true, false) ==
          VisibleSelectionDecision::KeepCurrent);
}

TEST_CASE("hidden library selections choose a visible replacement",
          "[library-view]") {
    CHECK(visibleSelectionDecision(false, true) ==
          VisibleSelectionDecision::SelectVisibleReplacement);
    CHECK(visibleSelectionDecision(false, false) ==
          VisibleSelectionDecision::ClearSelection);
}

TEST_CASE("retained and replacement selections are revealed",
          "[library-view]") {
    CHECK(visibleSelectionShouldBeRevealed(
        VisibleSelectionDecision::KeepCurrent));
    CHECK(visibleSelectionShouldBeRevealed(
        VisibleSelectionDecision::SelectVisibleReplacement));
    CHECK_FALSE(visibleSelectionShouldBeRevealed(
        VisibleSelectionDecision::ClearSelection));
}

TEST_CASE("per-item expansion requires children", "[library-view]") {
    CHECK_FALSE(treeItemHasExpandableChildren(0));
    CHECK(treeItemHasExpandableChildren(1));
}

TEST_CASE("Favorites view keeps exact favorite variants visible",
          "[library-view][favorites]") {
    CHECK(libraryVariantAllowed(true, false, false));
    CHECK_FALSE(libraryVariantAllowed(false, false, true));
    CHECK(libraryVariantAllowed(false, true, true));
    CHECK_FALSE(libraryVariantAllowed(false, true, false));
}

TEST_CASE("personal filters keep exact matching variants visible",
          "[library-view][filters]") {
    CHECK(libraryVariantAllowed(false, false, false, true, true));
    CHECK_FALSE(libraryVariantAllowed(false, false, false, true, false));
    CHECK(libraryVariantAllowed(false, true, true, true, false));
}

TEST_CASE("rating filter keys are stable and unknown keys are safe",
          "[library-view][filters]") {
    CHECK(libraryRatingFilterKey(LibraryRatingFilter::Any) == "any");
    CHECK(libraryRatingFilterKey(LibraryRatingFilter::Rated) == "rated");
    CHECK(libraryRatingFilterKey(LibraryRatingFilter::AtLeast4) ==
          "at_least_4");
    CHECK(libraryRatingFilterFromKey("unrated") ==
          LibraryRatingFilter::Unrated);
    CHECK(libraryRatingFilterFromKey("at_least_5") ==
          LibraryRatingFilter::AtLeast5);
    CHECK(libraryRatingFilterFromKey("unsupported") ==
          LibraryRatingFilter::Any);
}

TEST_CASE("playtime filter keys are stable and unknown keys are safe",
          "[library-view][filters]") {
    CHECK(libraryPlaytimeFilterKey(LibraryPlaytimeFilter::Any) == "any");
    CHECK(libraryPlaytimeFilterKey(LibraryPlaytimeFilter::Played) ==
          "played");
    CHECK(libraryPlaytimeFilterFromKey("not_played") ==
          LibraryPlaytimeFilter::NotPlayed);
    CHECK(libraryPlaytimeFilterFromKey("unsupported") ==
          LibraryPlaytimeFilter::Any);
}

TEST_CASE("rating filters distinguish rated, unrated, and minimum stars",
          "[library-view][filters]") {
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Any, 0, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Rated, LibraryPlaytimeFilter::Any, 1, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Rated, LibraryPlaytimeFilter::Any, 0, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Unrated, LibraryPlaytimeFilter::Any, 0, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Unrated, LibraryPlaytimeFilter::Any, 2, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast4, LibraryPlaytimeFilter::Any, 4, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast4, LibraryPlaytimeFilter::Any, 5, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast4, LibraryPlaytimeFilter::Any, 3, 0));
}

TEST_CASE("playtime filters distinguish played and not played media",
          "[library-view][filters]") {
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Played, 0, 1));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Played, 0, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::NotPlayed, 0, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::NotPlayed, 0, 1));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast3,
        LibraryPlaytimeFilter::Played, 3, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast3,
        LibraryPlaytimeFilter::Played, 3, 60));
}

TEST_CASE("personal filter activity reflects either active dimension",
          "[library-view][filters]") {
    CHECK_FALSE(libraryPersonalFiltersActive(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Any));
    CHECK(libraryPersonalFiltersActive(
        LibraryRatingFilter::Rated, LibraryPlaytimeFilter::Any));
    CHECK(libraryPersonalFiltersActive(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::NotPlayed));
}

TEST_CASE("filtered selections require an exact visible media match",
          "[library-view][filters]") {
    CHECK(librarySelectionAllowed(true, false, false));
    CHECK(librarySelectionAllowed(true, true, true));
    CHECK_FALSE(librarySelectionAllowed(true, true, false));
    CHECK_FALSE(librarySelectionAllowed(false, true, true));
}

TEST_CASE("filter-only expansion is not preserved as manual tree state",
          "[library-view][filters]") {
    CHECK(libraryExpansionShouldBePreserved(true, false));
    CHECK_FALSE(libraryExpansionShouldBePreserved(true, true));
    CHECK_FALSE(libraryExpansionShouldBePreserved(false, false));
    CHECK_FALSE(libraryExpansionShouldBePreserved(false, true));
}

TEST_CASE("missing sort metrics stay last in both directions",
          "[library-view][sort]") {
    CHECK(libraryMetricPrecedes(5, 0, true));
    CHECK(libraryMetricPrecedes(1, 0, false));
    CHECK_FALSE(libraryMetricPrecedes(0, 5, true));
    CHECK_FALSE(libraryMetricPrecedes(0, 1, false));
    CHECK(libraryMetricPrecedes(5, 3, true));
    CHECK(libraryMetricPrecedes(3, 5, false));
    CHECK_FALSE(libraryMetricPrecedes(3, 3, true));
}

TEST_CASE("details scroll resets only for a true selection change",
          "[library-view]") {
    CHECK(detailsScrollTarget(true, 173) == 0);
    CHECK(detailsScrollTarget(false, 173) == 173);
}

TEST_CASE("Neo Geo CD details use the full catalog title",
          "[library-view]") {
    const std::string fullName =
        "Fatal Fury 3 - Road to the Final Victory (USA)";
    const std::string displayName =
        "Fatal Fury 3 - Road to the Final Victory";
    const std::string emptyName;

    CHECK(libraryDetailsTitle(true, fullName, displayName) == fullName);
    CHECK(libraryDetailsTitle(false, fullName, displayName) == displayName);
    CHECK(libraryDetailsTitle(true, emptyName, displayName) == displayName);
}
