// game_library_state.hpp — Goliath-owned personal library state. This data is
// intentionally stored outside scanner-generated games.json so rescans cannot
// erase a user's favorites. Every selectable parent, variant, CUE, or CHD is
// keyed by the same exact system/media identity used by profiles and playtime.
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>

namespace goliath {

struct GameLibraryState {
    bool favorite = false;

    bool empty() const noexcept;
};

struct GameLibraryStateRecord {
    std::string system;
    std::string media;
    GameLibraryState state;
};

class GameLibraryStateStore {
public:
    // A missing file is a valid empty store. Malformed or unsupported files
    // return false, leave the store empty, and optionally describe the error.
    bool load(const std::filesystem::path& path, std::string* error = nullptr);

    // Replaces the destination atomically so an interrupted write cannot
    // truncate the previously valid personal-library state.
    bool save(const std::filesystem::path& path,
              std::string* error = nullptr) const;

    const GameLibraryState* find(const std::string& system,
                                 const std::string& media) const;
    bool is_favorite(const std::string& system,
                     const std::string& media) const;
    void set_favorite(const std::string& system,
                      const std::string& media,
                      bool favorite);

    std::size_t favorite_count(const std::string& system = {}) const;
    void clear();
    std::size_t size() const;

private:
    std::map<std::string, GameLibraryStateRecord> m_records;
};

} // namespace goliath
