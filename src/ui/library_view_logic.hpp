#pragma once

#include <string>

namespace goliath {

enum class VisibleSelectionDecision {
    KeepCurrent,
    SelectVisibleReplacement,
    ClearSelection,
};

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
