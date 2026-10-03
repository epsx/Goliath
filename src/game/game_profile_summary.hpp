#pragma once

#include <string>

namespace goliath {

struct GameLaunchProfile;
struct GameProfileGlobalSettings;

// Compact, presentation-ready view of the effective settings used for one
// launchable media item. Missing per-game values are resolved from the same
// global settings used by the launch pipeline.
struct GameProfileSummary {
    bool custom = false;
    std::string system;
    std::string video;
    std::string input;
};

GameProfileSummary summarize_game_profile(
    const std::string& system,
    const GameLaunchProfile* profile,
    const GameProfileGlobalSettings& global);

} // namespace goliath
