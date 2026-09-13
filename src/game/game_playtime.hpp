// game_playtime.hpp — persistent playtime/session statistics keyed by the
// exact system and normalized media path used by per-game launch profiles.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

namespace goliath {

// Very short processes are normally launch failures, not useful play
// sessions. Keeping the threshold public makes the policy explicit and
// deterministic for the UI and regression tests.
inline constexpr std::int64_t kMinimumTrackedSessionSeconds = 5;

struct GamePlaytimeRecord {
    std::int64_t total_seconds = 0;
    std::int64_t session_count = 0;
    std::int64_t last_played_epoch = 0;
};

// Compact display text such as "Less than a minute", "45m", or "2h 14m".
std::string format_playtime_seconds(std::int64_t seconds);

class GamePlaytimeStore {
public:
    // A missing file is a valid empty store. A malformed or unsupported file
    // returns false and leaves this store empty.
    bool load(const std::filesystem::path& path, std::string* error = nullptr);

    // Replaces the destination atomically, so interruption during a write
    // cannot truncate the previously valid statistics file.
    bool save(const std::filesystem::path& path,
              std::string* error = nullptr) const;

    const GamePlaytimeRecord* find(const std::string& system,
                                   const std::string& media) const;

    // Returns true only when a valid session was added. Totals saturate at
    // int64_t limits instead of overflowing corruptly.
    bool add_session(const std::string& system,
                     const std::string& media,
                     std::int64_t session_seconds,
                     std::int64_t ended_epoch);

    void clear();
    std::size_t size() const;

private:
    struct Entry {
        std::string system;
        std::string media;
        GamePlaytimeRecord record;
    };

    std::map<std::string, Entry> m_records;
};

} // namespace goliath
