// Compares two validated games.json snapshots by exact system/media identity.
#pragma once

#include "game/game_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace goliath {

struct LibraryRescanChange {
    std::string display;
    std::string system;
    std::string media;
};

struct LibraryRescanComparison {
    std::size_t previous_count = 0;
    std::size_t current_count = 0;
    std::size_t unchanged_count = 0;
    std::vector<LibraryRescanChange> added;
    std::vector<LibraryRescanChange> removed;
    std::vector<LibraryRescanChange> updated;
};

struct LibraryVerificationIssue {
    std::string display;
    std::string media;
    std::string problem;
};

// Ordering changes alone are ignored. An entry is updated when its exact
// system/media identity remains present but scanner-owned metadata changes.
LibraryRescanComparison compare_library_snapshots(
    const std::vector<Game>& previous,
    const std::vector<Game>& current);

// Returns at most maximum_entries concrete verification problems from the
// current snapshot. The order follows the scanner output so notification
// previews are stable and immediately recognizable in the library.
std::vector<LibraryVerificationIssue> collect_library_verification_issues(
    const std::vector<Game>& current, std::size_t maximum_entries);

} // namespace goliath
