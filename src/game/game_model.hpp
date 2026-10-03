// ---------------------------------------------------------------------------
// games.json schema
//
// Required Game fields:
//   name, display, short, source, roms
//
// Optional Game fields:
//   system (legacy databases default to "neogeo"), identified, verification,
//   redump_id,
//   year, manufacturer, developer, publisher,
//   genre, players, series, history, icon,
//   snapshot, main_rom
//
// Required Rom fields:
//   file, mame, main
//
// Optional Rom fields:
//   name, label, cloneof, alt_title, serial, release, part, interface,
//   program_width, program_endianness, program_size, fixed_size,
//   audio_cpu_size, audio_data_size, graphics_size, verification, crc32,
//   expected_crc32, hashes
//
// main_rom must identify the ROM whose Rom::main == true.
// ---------------------------------------------------------------------------

// Game and Rom mirror the database/games.json schema written by the scanner.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace goliath {

struct MediaHashDetail {
    std::string file;
    std::optional<std::string> catalog_file;
    std::string role;
    std::string algorithm;
    std::optional<std::uintmax_t> size;
    std::optional<std::uintmax_t> expected_size;
    std::optional<std::string> actual;
    std::optional<std::string> expected;
    bool matched = false;
};

struct Rom {
    std::string file;
    std::optional<std::string> name;
    std::optional<std::string> label;
    std::string mame;
    std::optional<std::string> cloneof;
    // Keep this before appended optional metadata so legacy aggregate
    // initializers ending in the main flag remain source-compatible.
    bool main = false;
    std::optional<std::string> alt_title;
    std::optional<std::string> serial;
    std::optional<std::string> release;
    std::optional<std::string> part;
    std::optional<std::string> interface;
    std::optional<std::uintmax_t> program_width;
    std::optional<std::string> program_endianness;
    std::optional<std::uintmax_t> program_size;
    std::optional<std::uintmax_t> fixed_size;
    std::optional<std::uintmax_t> audio_cpu_size;
    std::optional<std::uintmax_t> audio_data_size;
    std::optional<std::uintmax_t> graphics_size;
    // Cartridge-only values: "geolith-crc32" or
    // "geolith-crc32-mismatch", plus calculated/catalog CRC-32 values.
    std::optional<std::string> verification;
    std::optional<std::string> crc32;
    std::optional<std::string> expected_crc32;
    std::vector<MediaHashDetail> hashes;
};

struct Game {
    std::string name;
    std::string display;
    std::string short_name; // JSON key "short"
    std::string source;     // "mame", "homebrew", "redump" or "unknown"
    std::string system = "neogeo"; // "neogeo" or "neogeocd"
    bool identified = true; // true for metadata matches or trusted content identity
    // "redump-cue", "redump-tracks-only", "mame-chd",
    // "mame-chd-mismatch", or null.
    std::optional<std::string> verification;
    // Exact Redump DAT <game id="..."> value for verified Redump CD media.
    std::optional<std::string> redump_id;
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
