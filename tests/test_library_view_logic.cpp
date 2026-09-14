#include "ui/library_view_logic.hpp"

#include <catch2/catch.hpp>

using goliath::VisibleSelectionDecision;
using goliath::detailsScrollTarget;
using goliath::libraryDetailsTitle;
using goliath::libraryFavoriteGroupAllowed;
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

TEST_CASE("Favorites view retains the parent container for favorite variants",
          "[library-view][favorites]") {
    CHECK(libraryFavoriteGroupAllowed(false, false, false));
    CHECK(libraryFavoriteGroupAllowed(true, true, false));
    CHECK(libraryFavoriteGroupAllowed(true, false, true));
    CHECK_FALSE(libraryFavoriteGroupAllowed(true, false, false));
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
