#pragma once

#include "game/game_model.hpp"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace goliath {

enum class GameIntegrityStatus {
    Passed,
    ProblemsFound,
    AvailableWithoutReference,
    Canceled,
};

struct GameIntegrityResult {
    GameIntegrityStatus status = GameIntegrityStatus::ProblemsFound;
    std::filesystem::path media_path;
    std::vector<MediaHashDetail> hashes;
    std::size_t matched_files = 0;
    std::size_t problem_files = 0;
    std::string detail;
};

// Rechecks the selected media against the exact sizes and digests preserved
// by the most recent library scan. CRC-32/SHA-1 cache entries are reused only
// while path, size, and modification time still match; CHD identity is read
// directly from its header. The library model is never mutated by this check.
GameIntegrityResult verify_game_integrity(
    const std::filesystem::path& media_root,
    const std::string& media,
    const Rom* rom,
    const std::filesystem::path& cache_directory,
    const std::atomic<bool>* cancel = nullptr);

} // namespace goliath
