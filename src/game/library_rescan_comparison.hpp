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

struct LibraryVerificationSummary {
    std::size_t verified = 0;
    std::size_t cue_mismatches = 0;
    std::size_t chd_mismatches = 0;
    std::size_t crc32_mismatches = 0;
    std::size_t metadata_only = 0;
    std::size_t unknown = 0;

    [[nodiscard]] std::size_t mismatch_count() const {
        return cue_mismatches + chd_mismatches + crc32_mismatches;
    }
    [[nodiscard]] std::size_t issue_count() const {
        return mismatch_count() + unknown;
    }
};

// Ordering changes alone are ignored. An entry is updated when its exact
// system/media identity remains present but scanner-owned metadata changes.
LibraryRescanComparison compare_library_snapshots(
    const std::vector<Game>& previous,
    const std::vector<Game>& current);

// Counts exact verification states across cartridge files and Neo Geo CD
// images. CUE track-only matches are deliberately classified as mismatches:
// their track hashes match Redump, but the CUE layout itself does not.
LibraryVerificationSummary summarize_library_verification(
    const std::vector<Game>& current);

// Returns at most maximum_entries concrete verification problems from the
// current snapshot. The order follows the scanner output so notification
// previews are stable and immediately recognizable in the library.
std::vector<LibraryVerificationIssue> collect_library_verification_issues(
    const std::vector<Game>& current, std::size_t maximum_entries);

} // namespace goliath
