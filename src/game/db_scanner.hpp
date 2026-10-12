// Neo Geo MVS/AES and Neo Geo CD library scanner.
//
// Discovers configured media, resolves metadata, verifies supported CD images,
// and writes database/games.json.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace goliath {

class Config;

// Receives one progress or status line at a time during scanning.
using ScanProgressCallback = std::function<void(const std::string&)>;

// Aggregate scan status and statistics.
struct ScanResult {
    bool success = false;
    std::string error_message;
    std::size_t rom_file_count = 0;
    std::size_t game_count = 0;
    std::size_t parent_games = 0;
    std::size_t variant_count = 0;
    std::size_t homebrew_games = 0;
    std::size_t neo_geolith_crc32_verified_files = 0;
    std::size_t neo_geolith_crc32_mismatch_files = 0;
    std::size_t neo_metadata_only_files = 0;
    std::size_t neo_geolith_crc32_cache_hits = 0;
    std::size_t neo_geolith_crc32_calculated_files = 0;
    std::size_t neo_geolith_crc32_cache_entries = 0;
    std::size_t neo_geolith_crc32_cache_pruned = 0;

    // Neo Geo CD statistics are kept separate from the legacy cartridge
    // counters above so existing callers/tests retain their established
    // MVS/AES semantics.
    std::size_t cd_image_count = 0;
    std::size_t cd_game_count = 0;
    std::size_t cd_identified_games = 0;
    std::size_t cd_redump_cue_verified_games = 0;
    std::size_t cd_redump_tracks_only_games = 0;
    std::size_t cd_mame_chd_matched_games = 0;
    std::size_t cd_mame_chd_mismatch_games = 0;
    std::size_t cd_metadata_only_games = 0;
    std::size_t cd_unknown_games = 0;

    // Number of physical CUE track files whose SHA-1 came from the persistent
    // cache vs. files that had to be read and hashed during this scan.
    std::size_t cd_redump_sha1_cache_hits = 0;
    std::size_t cd_redump_sha1_calculated_files = 0;
    std::size_t cd_redump_sha1_cache_entries = 0;
    std::size_t cd_redump_sha1_cache_pruned = 0;

    // Individual stages are exposed so the GUI can render a compact summary
    // without parsing the human-readable progress transcript.
    std::int64_t metadata_and_indexes_ms = 0;
    std::int64_t file_discovery_ms = 0;
    std::int64_t cartridge_verification_ms = 0;
    std::int64_t cd_verification_ms = 0;
    std::int64_t database_build_write_ms = 0;
    std::int64_t statistics_ms = 0;
    std::int64_t total_ms = 0;
};

// Scans the configured MVS/AES and Neo Geo CD roots and writes
// database/games.json.
//
// Parameters:
//   - config: application configuration containing the relevant content and
//             output paths.
//   - progress_callback: optional callback for live scanner output.
//   - cancel: optional cancellation flag. When set, scanning stops as soon as
//             practical and returns success=false.
//
// Returns the aggregate scan status and statistics.
ScanResult scan_roms(const Config& config, ScanProgressCallback progress_callback = nullptr,
                     const std::atomic<bool>* cancel = nullptr);

} // namespace goliath
