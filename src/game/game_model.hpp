// ---------------------------------------------------------------------------
// games.json schema
//
// Required Game fields:
//   name, display, short, source, roms
//
// Optional Game fields:
//   system (legacy databases default to "neogeo"), identified, verification,
//   year, manufacturer, developer, publisher,
//   genre, players, series, history, icon,
//   snapshot, main_rom
//
// Required Rom fields:
//   file, mame, main
//
// Optional Rom fields:
//   name, label, cloneof
//
// main_rom must identify the ROM whose Rom::main == true.
// ---------------------------------------------------------------------------

// Game and Rom mirror the database/games.json schema written by the scanner.
#pragma once

#include <optional>
#include <string>
#include <vector>
#include <filesystem>

namespace goliath {

struct Rom {
    std::string file;
    std::optional<std::string> name;
    std::optional<std::string> label;
    std::string mame;
    std::optional<std::string> cloneof;
    bool main = false;
};

struct Game {
    std::string name;
    std::string display;
    std::string short_name; // JSON key "short"
    std::string source;     // "mame", "homebrew", "redump" or "unknown"
    std::string system = "neogeo"; // "neogeo" or "neogeocd"
    bool identified = true; // true for metadata matches or trusted content identity
    // "redump-cue", "redump-tracks-only", "mame-chd", or null.
    std::optional<std::string> verification;
    std::optional<std::string> year;
    std::optional<std::string> manufacturer;
    std::optional<std::string> developer;
    std::optional<std::string> publisher;
    std::optional<std::string> genre;
    std::optional<std::string> players;
    std::optional<std::string> series;
    std::optional<std::string> history;
    std::optional<std::string> icon;     // path as stored in the JSON
    std::optional<std::string> snapshot; // path as stored in the JSON
    std::vector<Rom> roms;
    std::optional<std::string> main_rom;
};

// Loads database/games.json. Returns an empty vector (and logs to stderr)
// if the file is missing or malformed.
std::vector<Game> load_games(const std::filesystem::path& json_file);

} // namespace goliath
