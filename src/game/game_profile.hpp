// game_profile.hpp — Goliath-owned per-launchable-media profiles. Profiles
// are intentionally stored outside games.json so a ROM rescan cannot erase
// user choices. Every cartridge parent/variant and every CD image is keyed by
// its system plus normalized relative media path.
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "game/video_settings.hpp"

namespace goliath {

using GameInputOverrides =
    std::map<std::string, std::map<std::string, std::string>>;

struct GameLaunchProfile {
    // Geolith core settings. A missing value means "inherit global".
    std::optional<int> cartridge_system;   // 0 AES, 1 MVS, 2 Universe BIOS
    std::optional<int> cd_system;          // 0 Front, 1 Top, 2 CDZ, 3 Universe BIOS
    std::optional<int> universe_hardware;  // 0 AES, 1 MVS
    std::optional<int> region;             // 0 US, 1 JP, 2 AS, 3 EU
    std::optional<int> input_device;       // 0 Auto, 1 Joysticks, 2 Mahjong, 3 4-Player
    std::optional<bool> free_play;
    std::optional<bool> setting_mode;

    // JGRF frontend settings. Missing values inherit the effective global
    // settings.ini/geolith.ini value. Video API 3 requires a Vulkan-enabled
    // JGRF build; shader 0 is Nearest Neighbour (effectively no filter).
    std::optional<int> video_api;           // 0..3
    std::optional<int> shader;              // 0..6

    // Remaining integer-valued settings are data-driven by video_settings.*.
    // API and shader retain their established top-level members/JSON keys for
    // backward compatibility and because JGRF applies them as final CLI
    // overrides. These maps are materialized into the isolated geolith.ini.
    VideoSettingValues jgrf_video;          // excludes api and shader
    VideoSettingValues geolith_video;

    // Retarget Player 1/single-player joystick bindings in the inherited
    // geolith_input.ini to this JGRF SDL port. JGRF identifies physical
    // devices by connection order (j0, j1, ...), not by persistent GUID.
    std::optional<int> controller_port;     // 0..9

    // Exact-media input bindings layered last onto the inherited global
    // geolith_input.ini. Missing keys continue to follow the global mapping.
    GameInputOverrides input_overrides;

    bool empty() const;
};

struct GameProfileRecord {
    std::string system;
    std::string media;
    GameLaunchProfile profile;
};

std::string normalize_game_profile_media(std::string media);
std::string make_game_profile_key(const std::string& system,
                                  const std::string& media);

// Stable, short identifier for storage owned by one exact system/media pair.
// The readable identity remains in the corresponding JSON/manifest; callers
// use this only for bounded directory names.
std::string exact_media_storage_id(const std::string& system,
                                   const std::string& media);

class GameProfileStore {
public:
    // A missing file is a valid empty store. Malformed/unsupported files
    // return false, leave the store empty and optionally describe the error.
    bool load(const std::filesystem::path& path, std::string* error = nullptr);
    bool save(const std::filesystem::path& path, std::string* error = nullptr) const;

    const GameLaunchProfile* find(const std::string& system,
                                  const std::string& media) const;
    void set(const std::string& system, const std::string& media,
             GameLaunchProfile profile);
    void remove(const std::string& system, const std::string& media);
    void clear();
    std::size_t size() const;

private:
    std::map<std::string, GameProfileRecord> m_records;
};

} // namespace goliath
