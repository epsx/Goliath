#include "game_model.hpp"
#include "json.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace goliath {

static std::optional<std::string> opt_str(const json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) return std::nullopt;
    const json& v = j.at(key);
    if (v.is_string()) return v.get<std::string>();
    // some fields (e.g. year) could theoretically be numeric; stringify defensively
    return v.dump();
}

static std::optional<std::uintmax_t> opt_uintmax(
    const json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) return std::nullopt;
    const json& value = j.at(key);
    if (value.is_number_unsigned())
        return value.get<std::uintmax_t>();
    if (value.is_number_integer()) {
        const std::int64_t signedValue = value.get<std::int64_t>();
        if (signedValue >= 0)
            return static_cast<std::uintmax_t>(signedValue);
    }
    return std::nullopt;
}

static bool has_required_string(
    const json& j,
    const char* key
) {
    if (!j.contains(key))
        return false;

    const json& v = j.at(key);

    if (!v.is_string())
        return false;

    return !v.get<std::string>().empty();
}

std::vector<Game> load_games(const fs::path& json_file) {
    std::vector<Game> games;

    if (!fs::exists(json_file)) {
        return games;
    }

    std::ifstream in(json_file, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();

    json root;
    try {
        root = json::parse(buf.str());
    } catch (const std::exception& e) {
        std::cerr << "[goliath] Could not parse " << json_file.string() << ": " << e.what() << "\n";
        return games;
    }

    if (!root.is_array()) return games;

    games.reserve(root.size());
    for (const auto& jg : root) {
        Game g;

        if (!has_required_string(jg, "name") ||
            !has_required_string(jg, "display") ||
            !has_required_string(jg, "short") ||
            !has_required_string(jg, "source")) {
            std::cerr
                << "[goliath] Skipping invalid game entry\n";
            continue;
        }

        std::string system = "neogeo";
        if (jg.contains("system")) {
            if (!jg.at("system").is_string() || jg.at("system").get<std::string>().empty()) {
                std::cerr << "[goliath] Skipping game with invalid system\n";
                continue;
            }
            system = jg.at("system").get<std::string>();
        }

        if (system != "neogeo" && system != "neogeocd") {
            std::cerr
                << "[goliath] Skipping game with invalid system: "
                << system
                << "\n";
            continue;
        }

        const std::string source =
            jg.at("source").get<std::string>();

        if (source != "mame" &&
            source != "homebrew" &&
            source != "redump" &&
            source != "unknown") {
            std::cerr
                << "[goliath] Skipping game with invalid source: "
                << source
                << "\n";
            continue;
        }

        // CD-only sources must never leak into the cartridge library.
        if ((source == "unknown" || source == "redump") && system != "neogeocd") {
            std::cerr << "[goliath] Skipping non-CD game with CD-only source\n";
            continue;
        }

        bool identified = (source != "unknown");
        if (jg.contains("identified")) {
            if (!jg.at("identified").is_boolean()) {
                std::cerr << "[goliath] Skipping game with invalid identified flag\n";
                continue;
            }
            identified = jg.at("identified").get<bool>();
        }

        if (source == "unknown" && identified) {
            std::cerr << "[goliath] Skipping inconsistent unknown CD entry\n";
            continue;
        }

        std::optional<std::string> verification;
        if (jg.contains("verification") && !jg.at("verification").is_null()) {
            if (!jg.at("verification").is_string()) {
                std::cerr << "[goliath] Skipping game with invalid verification\n";
                continue;
            }
            verification = jg.at("verification").get<std::string>();
            if (*verification != "redump-cue" &&
                *verification != "redump-tracks-only" &&
                *verification != "mame-chd" &&
                *verification != "mame-chd-mismatch") {
                std::cerr << "[goliath] Skipping game with unsupported verification: "
                          << *verification << "\n";
                continue;
            }
            if (system != "neogeocd") {
                std::cerr << "[goliath] Skipping non-CD game with media verification\n";
                continue;
            }
        }

        const bool has_redump_identity =
            verification == std::optional<std::string>("redump-cue") ||
            verification == std::optional<std::string>("redump-tracks-only");
        if (source == "redump" && !has_redump_identity) {
            std::cerr << "[goliath] Skipping Redump entry without trusted Redump identity\n";
            continue;
        }

        g.name = jg.at("name").get<std::string>();
        g.display = jg.at("display").get<std::string>();
        g.short_name = jg.at("short").get<std::string>();
        g.source = source;
        g.system = system;
        g.identified = identified;
        g.verification = verification;
        g.redump_id = source == "redump"
            ? opt_str(jg, "redump_id")
            : std::nullopt;
        if (g.redump_id.has_value() && g.redump_id->empty())
            g.redump_id.reset();
        g.year = opt_str(jg, "year");
        g.manufacturer = opt_str(jg, "manufacturer");
        g.developer = opt_str(jg, "developer");
        g.publisher = opt_str(jg, "publisher");
        g.genre = opt_str(jg, "genre");
        g.players = opt_str(jg, "players");
        g.series = opt_str(jg, "series");
        g.history = opt_str(jg, "history");
        g.icon = opt_str(jg, "icon");
        g.snapshot = opt_str(jg, "snapshot");
        g.main_rom = opt_str(jg, "main_rom");

        if (!jg.contains("roms") ||
            !jg.at("roms").is_array()) {
            std::cerr
                << "[goliath] Skipping game without valid roms array: "
                << g.short_name
                << "\n";
            continue;
        }

        for (const auto& jr : jg.at("roms")) {
            if (!has_required_string(jr, "file") ||
                !has_required_string(jr, "mame") ||
                !jr.contains("main") ||
                !jr.at("main").is_boolean()) {
                std::cerr
                    << "[goliath] Skipping invalid ROM entry in game: "
                    << g.short_name
                    << "\n";
                continue;
            }

            Rom r;
            r.file = jr.at("file").get<std::string>();
            r.name = opt_str(jr, "name");
            r.label = opt_str(jr, "label");
            r.mame = jr.at("mame").get<std::string>();
            r.cloneof = opt_str(jr, "cloneof");
            r.alt_title = opt_str(jr, "alt_title");
            r.serial = opt_str(jr, "serial");
            r.release = opt_str(jr, "release");
            r.part = opt_str(jr, "part");
            r.interface = opt_str(jr, "interface");
            r.program_width = opt_uintmax(jr, "program_width");
            r.program_endianness = opt_str(jr, "program_endianness");
            r.program_size = opt_uintmax(jr, "program_size");
            r.fixed_size = opt_uintmax(jr, "fixed_size");
            r.audio_cpu_size = opt_uintmax(jr, "audio_cpu_size");
            r.audio_data_size = opt_uintmax(jr, "audio_data_size");
            r.graphics_size = opt_uintmax(jr, "graphics_size");
            r.verification = opt_str(jr, "verification");
            r.crc32 = opt_str(jr, "crc32");
            r.expected_crc32 = opt_str(jr, "expected_crc32");
            if (jr.contains("hashes") && jr.at("hashes").is_array()) {
                for (const auto& jhash : jr.at("hashes")) {
                    if (!has_required_string(jhash, "file") ||
                        !has_required_string(jhash, "role") ||
                        !has_required_string(jhash, "algorithm") ||
                        !jhash.contains("matched") ||
                        !jhash.at("matched").is_boolean()) {
                        continue;
                    }
                    MediaHashDetail hash;
                    hash.file = jhash.at("file").get<std::string>();
                    hash.catalog_file = opt_str(jhash, "catalog_file");
                    hash.role = jhash.at("role").get<std::string>();
                    hash.algorithm =
                        jhash.at("algorithm").get<std::string>();
                    hash.size = opt_uintmax(jhash, "size");
                    hash.expected_size =
                        opt_uintmax(jhash, "expected_size");
                    hash.actual = opt_str(jhash, "actual");
                    hash.expected = opt_str(jhash, "expected");
                    hash.matched = jhash.at("matched").get<bool>();
                    r.hashes.push_back(std::move(hash));
                }
            }
            r.main = jr.at("main").get<bool>();

            g.roms.push_back(std::move(r));
        }

        if (g.main_rom.has_value()) {
            bool main_found = false;
            for (const auto& rom : g.roms) {
                if (rom.file == *g.main_rom) {
                    main_found = rom.main;
                    break;
                }
            }

            if (!main_found) {
                std::cerr
                    << "[goliath] Invalid main_rom for game: "
                    << g.short_name
                    << "\n";
                g.main_rom = std::nullopt;
            }
        }

        games.push_back(std::move(g));
    }

    return games;
}

} // namespace goliath
