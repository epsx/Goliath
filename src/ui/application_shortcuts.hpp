#pragma once

#include <array>

namespace goliath {

struct ApplicationShortcutInfo {
    const char* action;
    const char* sequence;
};

inline constexpr char kRandomGameShortcut[] = "Ctrl+R";
inline constexpr char kFocusSearchShortcut[] = "Ctrl+F";
inline constexpr char kLaunchSelectedShortcut[] = "Return";
inline constexpr char kRescanRomsShortcut[] = "F5";
inline constexpr char kExpandAllShortcut[] = "Ctrl+Shift+E";
inline constexpr char kCollapseAllShortcut[] = "Ctrl+Shift+C";
inline constexpr char kClearSearchShortcut[] = "Escape";

inline constexpr std::array<ApplicationShortcutInfo, 7> kApplicationShortcuts{{
    {"Random game", kRandomGameShortcut},
    {"Focus search", kFocusSearchShortcut},
    {"Launch selected game", kLaunchSelectedShortcut},
    {"Rescan ROMs", kRescanRomsShortcut},
    {"Expand all variants", kExpandAllShortcut},
    {"Collapse all variants", kCollapseAllShortcut},
    {"Clear search (while focused)", kClearSearchShortcut},
}};

} // namespace goliath
