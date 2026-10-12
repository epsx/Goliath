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

// Ordering changes alone are ignored. An entry is updated when its exact
// system/media identity remains present but scanner-owned metadata changes.
LibraryRescanComparison compare_library_snapshots(
    const std::vector<Game>& previous,
    const std::vector<Game>& current);

} // namespace goliath
