// Neo Geo game library scanner implementation.

#include "db_scanner.hpp"
#include "common/goliath_common.hpp"
#include "crc32_cache.hpp"
#include "geolith_verification.hpp"
#include "sha1_cache.hpp"
#include "neogeo_metadata.hpp"
#include "neocd_verification.hpp"
#include "filesystem_io.hpp"

#include "json.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

namespace goliath {

// ---------------------------------------------------------------------------
// Small string helpers
// ---------------------------------------------------------------------------

static std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

static QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

static std::string path_stem(const std::string& filename) {
    return fs::path(filename).stem().string();
}

static const std::regex g_paren_re(R"(\s*\(.*?\))");

// ---------------------------------------------------------------------------
// Config-derived locations
// ---------------------------------------------------------------------------

struct Paths {
    fs::path romdir, neocddir, icondir, snapdir, outdir, out_json;
    fs::path metadata, geolith_xml, neogeo_xml, neocd_xml;
};

// ---------------------------------------------------------------------------
// Utility / matching functions
// ---------------------------------------------------------------------------

static std::string clean_display(std::string s) {
    auto slash = s.find('/');
    if (slash != std::string::npos) s = s.substr(0, slash);

    s = std::regex_replace(s, g_paren_re, "");

    return trim(s);
}

static std::optional<std::string> make_variant_label(const std::string& short_name,
                                                     const SoftwareCatalog& catalog) {
    auto it = catalog.find(short_name);
    if (it == catalog.end()) {
        return short_name;
    }
    const SoftwareEntry& data = it->second;

    if (!data.cloneof.has_value()) {
        return std::nullopt;
    }

    static const std::regex product_code(
        R"(\((NG[MH]-[^)]*)\)(.*)$)", std::regex::icase);
    std::smatch product_match;
    if (std::regex_search(data.description, product_match, product_code)) {
        std::string label = product_match[1].str();
        const std::string qualifiers = trim(product_match[2].str());
        if (!qualifiers.empty()) label += " " + qualifiers;
        return label;
    }

    static const std::vector<std::regex> patterns = {
        std::regex(R"(\((prototype[^)]*)\))", std::regex::icase),
        std::regex(R"(\((bootleg[^)]*)\))", std::regex::icase),
        std::regex(R"(\((Korean[^)]*)\))", std::regex::icase),
        std::regex(R"(\((Japan[^)]*)\))", std::regex::icase),
        std::regex(R"(\((Export[^)]*)\))", std::regex::icase),
        std::regex(R"(\((World[^)]*)\))", std::regex::icase),
        std::regex(R"(\((set \d+)\))", std::regex::icase),
    };

    for (const auto& pattern : patterns) {
        std::smatch m;
        if (std::regex_search(data.description, m, pattern)) {
            return m[1].str();
        }
    }

    return data.description;
}

static bool is_main_rom(const std::string& short_name, const SoftwareCatalog& catalog) {
    auto it = catalog.find(short_name);
    if (it == catalog.end()) return true;
    return !it->second.cloneof.has_value();
}

static std::optional<std::string> find_game(
    const std::string& filename,
    const SoftwareCatalog& catalog,
    const std::unordered_map<std::string, std::string>& desc_to_short) {

    // Primary identifier for cartridge ROMs: the MAME/Geolith shortname in
    // the .neo filename (e.g. mslug.neo). This is deterministic and avoids
    // fuzzy matching for the normal Goliath setup.
    std::string stem = to_lower(path_stem(filename));
    if (catalog.find(stem) != catalog.end())
        return stem;

    // Compatibility fallback for canonical long filenames. Punctuation is
    // normalized so '/' and '-' naming differences do not matter.
    std::string clean = make_desc_key(path_stem(filename));
    auto it = desc_to_short.find(clean);
    if (it != desc_to_short.end())
        return it->second;

    return std::nullopt;
}

static std::optional<fs::path> find_icon(const fs::path& icondir,
                                         const std::string& short_name,
                                         const SoftwareCatalog& catalog) {
    std::error_code ec;
    fs::path icon = icondir / (short_name + ".ico");
    if (fs::exists(icon, ec)) return icon;

    auto it = catalog.find(short_name);
    if (it != catalog.end() && it->second.cloneof.has_value()) {
        fs::path parent_icon = icondir / (*it->second.cloneof + ".ico");
        if (fs::exists(parent_icon, ec)) return parent_icon;
    }
    return std::nullopt;
}

static std::optional<fs::path> find_snapshot(const fs::path& snapdir, const std::string& short_name) {
    std::error_code ec;
    fs::path snap = snapdir / (short_name + ".png");
    if (fs::exists(snap, ec)) return snap;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Neo Geo CD presentation and stable internal identifiers
// ---------------------------------------------------------------------------

static std::string make_unknown_cd_short(const std::string& relative_path);

static std::string make_redump_cd_short(const RedumpGameEntry& entry,
                                        const std::string& relative_path) {
    if (entry.id.has_value() && !entry.id->empty()) {
        std::string id;
        for (unsigned char c : *entry.id) {
            if (std::isalnum(c))
                id.push_back(static_cast<char>(std::tolower(c)));
            else
                id.push_back('_');
        }
        if (!id.empty())
            return "cd_redump_" + id;
    }
    return make_unknown_cd_short(relative_path);
}

static std::string clean_cd_display(const std::string& description) {
    std::string s = description;
    const std::size_t regional = s.find('~');
    if (regional != std::string::npos)
        s = s.substr(0, regional);
    s = std::regex_replace(s, g_paren_re, "");
    return trim(s);
}

static std::string make_unknown_cd_short(const std::string& relative_path) {
    // Stable, compact identifier for an image that is intentionally retained
    // even though neocd.xml could not identify it. FNV-1a is sufficient here;
    // this is an internal key, not a content hash or authenticity check.
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : relative_path) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    std::ostringstream out;
    out << "cd_unknown_" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

// ---------------------------------------------------------------------------
// Game record assembled during scanning
// ---------------------------------------------------------------------------

struct HashDetailEntry {
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

struct RomEntry {
    std::string file;
    std::optional<std::string> name;
    std::optional<std::string> label;
    std::string mame_short;
    std::optional<std::string> cloneof;
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
    std::optional<std::string> verification;
    std::optional<std::string> crc32;
    std::optional<std::string> expected_crc32;
    std::vector<HashDetailEntry> hashes;
    bool is_main;
};

struct GameEntry {
    std::string name;
    std::string display;
    std::string short_name;
    std::string source;
    std::string system = "neogeo";
    bool identified = true;
    std::optional<std::string> verification;
    std::optional<std::string> redump_id;
    std::optional<std::string> year;
    std::optional<std::string> manufacturer;
    std::optional<std::string> developer;
    std::optional<std::string> publisher;
    std::optional<std::string> genre_val;
    std::optional<std::string> players_val;
    std::optional<std::string> series_val;
    std::optional<std::string> history_val;
    std::optional<fs::path> icon;
    std::optional<fs::path> snapshot;
    std::vector<RomEntry> roms;
    std::optional<std::string> main_rom;
};

static json opt_to_json(const std::optional<std::string>& v) {
    if (v.has_value()) return json(*v);
    return json(nullptr);
}

static json opt_uint_to_json(const std::optional<std::uintmax_t>& value) {
    if (value.has_value()) return json(*value);
    return json(nullptr);
}

static void copy_catalog_metadata(RomEntry& rom,
                                  const SoftwareEntry& metadata) {
    rom.alt_title = metadata.alt_title;
    rom.serial = metadata.serial;
    rom.release = metadata.release;
    rom.part = metadata.part_name;
    rom.interface = metadata.interface;
    rom.program_width = metadata.maincpu_width;
    rom.program_endianness = metadata.maincpu_endianness;
    rom.program_size = metadata.maincpu_size;
    rom.fixed_size = metadata.fixed_size;
    rom.audio_cpu_size = metadata.audio_cpu_size;
    rom.audio_data_size = metadata.audio_data_size;
    rom.graphics_size = metadata.graphics_size;
}

static json opt_path_to_json(const std::optional<fs::path>& v) {
    if (v.has_value()) return json(v->generic_string());
    return json(nullptr);
}

static std::optional<std::string> map_get(const std::unordered_map<std::string, std::string>& m, const std::string& key) {
    auto it = m.find(key);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

static std::optional<std::string> non_empty_map_get(
    const std::unordered_map<std::string, std::string>& values,
    const std::string& key) {
    std::optional<std::string> value = map_get(values, key);
    if (!value.has_value() || value->empty())
        return std::nullopt;
    return value;
}

static std::optional<std::string> map_get_with_parent_fallback(
    const std::unordered_map<std::string, std::string>& values,
    const std::string& short_name,
    const std::optional<std::string>& parent_short_name) {
    if (std::optional<std::string> value =
            non_empty_map_get(values, short_name)) {
        return value;
    }
    if (!parent_short_name.has_value() || parent_short_name->empty() ||
        *parent_short_name == short_name) {
        return std::nullopt;
    }
    return non_empty_map_get(values, *parent_short_name);
}

static std::optional<std::string> supplemental_genre_for_id(
    const SupplementalMetadata& supplemental, const std::string& short_name) {
    if (std::optional<std::string> value =
            non_empty_map_get(supplemental.catver, short_name)) {
        return value;
    }
    if (std::optional<std::string> value =
            non_empty_map_get(supplemental.catlist, short_name)) {
        return value;
    }
    return non_empty_map_get(supplemental.genre, short_name);
}

static std::optional<std::string> supplemental_genre_with_parent_fallback(
    const SupplementalMetadata& supplemental,
    const std::string& short_name,
    const std::optional<std::string>& parent_short_name) {
    if (std::optional<std::string> value =
            supplemental_genre_for_id(supplemental, short_name)) {
        return value;
    }
    if (!parent_short_name.has_value() || parent_short_name->empty() ||
        *parent_short_name == short_name) {
        return std::nullopt;
    }
    return supplemental_genre_for_id(supplemental, *parent_short_name);
}

// Classification is independent from metadata availability.  A modern/homebrew
// title can legitimately exist in MAME's software list and still belong to the
// Homebrew bucket used by Goliath's existing UI/statistics.
static bool is_homebrew_release(const std::string& short_name) {
    static const std::unordered_set<std::string> homebrew_short_names = {
        "xenocris",
    };

    return homebrew_short_names.contains(short_name);
}

static std::unordered_set<std::string> build_supplemental_metadata_ids(
    const SoftwareCatalog& neogeo_metadata,
    const SoftwareCatalog& neocd_metadata,
    const std::vector<std::string>& rom_files) {
    std::unordered_set<std::string> ids;
    ids.reserve(neogeo_metadata.size() + neocd_metadata.size() +
                rom_files.size());

    for (const auto& entry : neogeo_metadata)
        ids.insert(entry.first);
    for (const auto& entry : neocd_metadata)
        ids.insert(entry.first);

    // Unknown/homebrew .neo files can still have MAME supplemental metadata
    // under their filename stem even when they are absent from neogeo.xml.
    for (const std::string& rom : rom_files)
        ids.insert(to_lower(path_stem(rom)));

    return ids;
}

// ---------------------------------------------------------------------------
// Main scan entry point
// ---------------------------------------------------------------------------

ScanResult scan_roms(const Config& config, ScanProgressCallback progress_callback,
                     const std::atomic<bool>* cancel) {
    ScanResult result;
    using ScanClock = std::chrono::steady_clock;
    const auto scan_started = ScanClock::now();
    const auto elapsed_ms = [](ScanClock::time_point begin,
                               ScanClock::time_point end) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end - begin).count();
    };

    // Stop promptly when cancellation is requested.
    auto is_canceled = [cancel]() { return cancel && cancel->load(std::memory_order_acquire); };

    Paths p;
    p.romdir = resolve_path(config, "roms");
    p.neocddir = resolve_path(config, "neocd");
    p.icondir = resolve_path(config, "icons");
    p.snapdir = resolve_path(config, "snaps");
    p.outdir = resolve_path(config, "database");
    p.out_json = p.outdir / "games.json";

    p.metadata = resolve_path(config, "metadata");
    p.geolith_xml = p.metadata / "geolith.xml";
    p.neogeo_xml = p.metadata / "neogeo.xml";
    p.neocd_xml = p.metadata / "neocd.xml";

    if (progress_callback) progress_callback("Loading Neo Geo metadata...\n");

    SoftwareCatalog neogeo_metadata;
    SoftwareCatalog neocd_metadata;
    load_software_list_xml(p.neogeo_xml, "neogeo", NeoGeoMedia::Cartridge,
                           neogeo_metadata, progress_callback);
    load_software_list_xml(p.neocd_xml, "neocd", NeoGeoMedia::CD,
                           neocd_metadata, progress_callback);
    const GeolithCrcCatalog geolith_crc_catalog =
        load_geolith_crc_catalog(p.geolith_xml, progress_callback);

    RedumpCatalog redump_metadata;
    if (const auto redump_dat = find_redump_neocd_dat(p.metadata);
        redump_dat.has_value()) {
        load_redump_neocd_dat(*redump_dat, redump_metadata, progress_callback);
    }

    Crc32Cache geolith_crc32_cache =
        Crc32Cache::load(p.outdir / "crc32_cache.json");
    Sha1Cache redump_sha1_cache = Sha1Cache::load(p.outdir / "hash_cache.json");

    apply_cartridge_compatibility(neogeo_metadata, progress_callback);

    std::unordered_map<std::string, std::string> desc_to_short;
    desc_to_short.reserve(neogeo_metadata.size());
    for (const auto& [short_name, data] : neogeo_metadata) {
        desc_to_short.try_emplace(make_desc_key(data.description), short_name);
    }

    const CdMatchIndex cd_match_index = build_cd_match_index(neocd_metadata);
    const MameChdHashCatalog mame_chd_hashes =
        build_mame_chd_hash_catalog(neocd_metadata);
    const RedumpLayoutIndex redump_layout_index(redump_metadata);
    const auto metadata_finished = ScanClock::now();

    if (is_canceled()) {
        result.error_message = "Scan canceled.";
        return result;
    }

    std::error_code ec;
    const bool rom_root_available = fs::is_directory(p.romdir, ec);
    std::vector<std::string> rom_files;
    if (rom_root_available) {
        for (const auto& entry : fs::directory_iterator(p.romdir, ec)) {
            ec.clear();
            if (!entry.is_regular_file(ec)) continue;
            std::string fname = entry.path().filename().string();
            if (fname.ends_with(".neo")) rom_files.push_back(fname);
        }
    }
    std::sort(rom_files.begin(), rom_files.end());

    const std::unordered_set<std::string> supplemental_ids =
        build_supplemental_metadata_ids(neogeo_metadata, neocd_metadata,
                                        rom_files);
    const SupplementalMetadata supplemental =
        load_supplemental_metadata(p.metadata, supplemental_ids,
                                   progress_callback);

    if (!rom_root_available) {
        std::string msg = "ROM directory not found: " + p.romdir.string() + "\n";
        if (progress_callback) progress_callback(msg);
        result.error_message = msg;
        return result;
    }

    // Neo Geo CD collections are commonly organized one game per directory,
    // but Goliath also supports images placed directly in the configured root.
    // Only launchable descriptor/image files become library entries; track
    // files such as .bin/.wav/.iso and archives are intentionally ignored.
    CdImageDiscoveryResult cd_discovery =
        discover_neocd_images(p.neocddir, cancel);
    if (cd_discovery.canceled) {
        result.error_message = "Scan canceled.";
        return result;
    }

    const bool cd_root_available = cd_discovery.root_available;
    std::vector<fs::path> cd_files = std::move(cd_discovery.files);
    if (!cd_root_available && progress_callback) {
        progress_callback("Neo Geo CD directory not found; skipping CD scan: " +
                          p.neocddir.string() + "\n");
    }

    std::sort(cd_files.begin(), cd_files.end(), [](const fs::path& a, const fs::path& b) {
        return to_lower(a.generic_string()) < to_lower(b.generic_string());
    });

    if (progress_callback)
        progress_callback("Neo Geo CD image files found: " + std::to_string(cd_files.size()) + "\n");
    const auto discovery_finished = ScanClock::now();

    std::unordered_map<std::string, GameEntry> games;
    games.reserve(rom_files.size());

    if (progress_callback)
        progress_callback("Verifying MVS/AES cartridge images...\n");

    for (const std::string& rom : rom_files) {
        if (is_canceled()) {
            result.error_message = "Scan canceled.";
            return result;
        }

        std::optional<std::string> short_opt = find_game(rom, neogeo_metadata, desc_to_short);

        std::string short_name;
        std::string game_short;
        std::string source;
        SoftwareEntry data;

        if (!short_opt.has_value()) {
            short_name = to_lower(path_stem(rom));
            game_short = short_name;
            source = "homebrew";

            data.description = path_stem(rom);
            data.year = std::nullopt;
            data.publisher = std::nullopt;
            data.cloneof = std::nullopt;
        } else {
            short_name = *short_opt;
            game_short = short_name;

            auto mit = neogeo_metadata.find(short_name);
            if (mit != neogeo_metadata.end() && mit->second.cloneof.has_value()) {
                game_short = *mit->second.cloneof;
            }

            if (mit == neogeo_metadata.end()) {
                continue;
            }
            data = mit->second;
            source = is_homebrew_release(short_name) ? "homebrew" : "mame";
        }

        const bool main_flag = is_main_rom(short_name, neogeo_metadata);

        auto game_it = games.find(game_short);
        if (game_it == games.end()) {
            GameEntry g;
            g.name = data.description;
            g.display = clean_display(data.description);
            g.short_name = game_short;
            g.source = source;
            g.year = data.year;
            // Keep the existing JSON/UI contract:
            // MAME software-list <publisher> is exposed through the historical
            // "manufacturer" field. A later schema cleanup can rename it.
            g.manufacturer = data.publisher;
            g.developer = std::nullopt;
            g.publisher = std::nullopt;

            std::optional<std::string> catver_v =
                map_get(supplemental.catver, game_short);
            std::optional<std::string> catlist_v =
                map_get(supplemental.catlist, game_short);
            std::optional<std::string> genre_v =
                map_get(supplemental.genre, game_short);
            if (catver_v.has_value() && !catver_v->empty())
                g.genre_val = catver_v;
            else if (catlist_v.has_value() && !catlist_v->empty())
                g.genre_val = catlist_v;
            else
                g.genre_val = genre_v;

            g.players_val = map_get(supplemental.players, game_short);
            g.series_val = map_get(supplemental.series, game_short);
            g.history_val = map_get(supplemental.history.neogeo, game_short);

            g.icon = find_icon(p.icondir, short_name, neogeo_metadata);
            g.snapshot = find_snapshot(p.snapdir, short_name);
            g.main_rom = std::nullopt;

            game_it = games.emplace(game_short, std::move(g)).first;
        }

        // ROM filenames are scanned alphabetically, so a clone can be seen
        // before its parent (for example lans2004 before shocktr2). Once the
        // actual parent ROM is present, the top-level group must use the
        // parent's metadata regardless of which file created the group first.
        if (main_flag) {
            GameEntry& group = game_it->second;
            group.name = data.description;
            group.display = clean_display(data.description);
            group.source = source;
            group.year = data.year;
            group.manufacturer = data.publisher;
            group.icon = find_icon(p.icondir, short_name, neogeo_metadata);
            group.snapshot = find_snapshot(p.snapdir, short_name);
        }

        RomEntry re;
        re.file = rom;
        re.name = data.description;
        re.label = make_variant_label(short_name, neogeo_metadata);
        re.mame_short = short_name;
        re.cloneof = data.cloneof;
        copy_catalog_metadata(re, data);
        re.is_main = main_flag;

        std::string geolith_catalog_file = to_lower(rom);
        auto expected_crc = geolith_crc_catalog.find(geolith_catalog_file);
        if (expected_crc == geolith_crc_catalog.end()) {
            // A collection may keep descriptive filenames while neogeo.xml
            // still resolves the exact MAME/Geolith software ID. CRC identity
            // does not depend on the local spelling, so use the canonical
            // shortname as a safe fallback.
            geolith_catalog_file = to_lower(short_name) + ".neo";
            expected_crc = geolith_crc_catalog.find(geolith_catalog_file);
        }
        if (expected_crc != geolith_crc_catalog.end()) {
            const std::optional<GeolithCrcVerification> crc_verification =
                verify_geolith_neo(
                    filesystem_io_path(p.romdir / fs::path(rom)),
                    geolith_catalog_file,
                    geolith_crc_catalog, cancel, &geolith_crc32_cache);
            if (is_canceled()) {
                result.error_message = "Scan canceled.";
                return result;
            }
            if (crc_verification.has_value()) {
                re.expected_crc32 = crc_verification->expected_crc32;
                re.crc32 = crc_verification->actual_crc32;
                re.verification = crc_verification->matches()
                    ? "geolith-crc32"
                    : "geolith-crc32-mismatch";

                HashDetailEntry hash;
                hash.file = rom;
                hash.catalog_file = geolith_catalog_file;
                hash.role = "Cartridge";
                hash.algorithm = "CRC-32";
                std::error_code size_ec;
                const std::uintmax_t size = fs::file_size(
                    filesystem_io_path(p.romdir / fs::path(rom)), size_ec);
                if (!size_ec)
                    hash.size = size;
                hash.actual = crc_verification->actual_crc32;
                hash.expected = crc_verification->expected_crc32;
                hash.matched = crc_verification->matches();
                re.hashes.push_back(std::move(hash));
            } else if (progress_callback) {
                progress_callback("Warning: could not calculate CRC-32 for " +
                                  (p.romdir / fs::path(rom)).string() +
                                  "\n");
            }
        }

        game_it->second.roms.push_back(std::move(re));
        if (main_flag) {
            game_it->second.main_rom = rom;
        }
    }

    if (!geolith_crc_catalog.empty() && !is_canceled())
        geolith_crc32_cache.prune_untouched();
    if (!geolith_crc32_cache.save() && progress_callback) {
        progress_callback("Warning: could not write Geolith CRC-32 cache: " +
                          geolith_crc32_cache.path().string() + "\n");
    }
    const auto cartridge_finished = ScanClock::now();

    std::vector<GameEntry> game_list;
    game_list.reserve(games.size() + cd_files.size());
    for (auto& [k, v] : games) game_list.push_back(std::move(v));

    // Each discovered CD image is a standalone top-level entry. Filename/title
    // metadata is provisional: verified CUE/CHD content identity may override
    // it or identify a Redump-only disc that is absent from neocd.xml.
    if (progress_callback)
        progress_callback("Verifying Neo Geo CD images...\n");

    for (const fs::path& rel_path : cd_files) {
        if (is_canceled()) {
            result.error_message = "Scan canceled.";
            return result;
        }

        const std::string rel = rel_path.generic_string();
        std::optional<std::string> short_opt =
            find_cd_game(rel_path, cd_match_index);

        const std::string cd_ext = to_lower(rel_path.extension().string());

        const fs::path cd_image_io_path = filesystem_io_path(p.neocddir / rel_path);
        std::optional<RedumpCueVerification> redump_verification;
        std::optional<std::string> redump_metadata_short;
        std::optional<std::string> mame_chd_short;
        std::optional<MameChdVerification> mame_chd_verification;

        if (cd_ext == ".cue" && !redump_metadata.empty()) {
            redump_verification = verify_redump_cue(cd_image_io_path,
                                                    redump_metadata,
                                                    redump_layout_index,
                                                    redump_sha1_cache,
                                                    make_cd_title_key,
                                                    cancel);
            if (is_canceled()) {
                result.error_message = "Scan canceled.";
                return result;
            }

            if (redump_verification.has_value() &&
                redump_verification->catalog_index < redump_metadata.size()) {
                // Track content identity wins over a filename hint even when
                // the small CUE descriptor was renamed or modified. Complete
                // Redump-set verification remains a separate, stricter state.
                const RedumpGameEntry& redump =
                    redump_metadata[redump_verification->catalog_index];
                redump_metadata_short = find_redump_cd_metadata(
                    redump, cd_match_index, neocd_metadata);
            }
        } else if (cd_ext == ".chd") {
            mame_chd_verification = verify_mame_chd(
                cd_image_io_path, mame_chd_hashes);
            if (mame_chd_verification.has_value())
                mame_chd_short =
                    mame_chd_verification->matched_short_name;
            if (mame_chd_short.has_value()) {
                // The CHD header hash is stronger than the filename hint. A
                // renamed official CHD must still recover the correct
                // software-list entry from its content identity.
                short_opt = mame_chd_short;
            }
        }

        GameEntry g;
        g.system = "neogeocd";
        g.main_rom = rel;
        if (redump_verification.has_value()) {
            g.verification =
                redump_verification->match == RedumpCueMatch::CompleteSet
                    ? "redump-cue"
                    : "redump-tracks-only";
        } else if (mame_chd_short.has_value()) {
            g.verification = "mame-chd";
        }

        RomEntry re;
        re.file = rel;
        re.name = std::nullopt;
        re.label = std::nullopt;
        re.cloneof = std::nullopt;
        re.is_main = true;

        if (redump_verification.has_value() &&
            redump_verification->catalog_index < redump_metadata.size()) {
            const RedumpGameEntry& redump =
                redump_metadata[redump_verification->catalog_index];
            g.identified = true;
            g.source = "redump";
            g.redump_id = redump.id;
            g.short_name = make_redump_cd_short(redump, rel);
            g.name = redump.description.empty() ? redump.name : redump.description;
            g.display = clean_cd_display(g.name);
            if (g.display.empty())
                g.display = strip_grouped_tags(g.name);
            if (g.display.empty())
                g.display = g.name;

            const SoftwareEntry* metadata_entry = nullptr;
            if (redump_metadata_short.has_value()) {
                const auto metadata_it =
                    neocd_metadata.find(*redump_metadata_short);
                if (metadata_it != neocd_metadata.end())
                    metadata_entry = &metadata_it->second;
            }

            if (metadata_entry) {
                g.year = metadata_entry->year;
                g.manufacturer = metadata_entry->publisher;
                g.genre_val = supplemental_genre_with_parent_fallback(
                    supplemental, *redump_metadata_short,
                    metadata_entry->cloneof);
                g.players_val = map_get_with_parent_fallback(
                    supplemental.players, *redump_metadata_short,
                    metadata_entry->cloneof);
                g.series_val = map_get_with_parent_fallback(
                    supplemental.series, *redump_metadata_short,
                    metadata_entry->cloneof);
                g.icon = find_icon(p.icondir, *redump_metadata_short,
                                   neocd_metadata);
                g.snapshot = find_snapshot(p.snapdir,
                                           *redump_metadata_short);
                re.mame_short = *redump_metadata_short;
                copy_catalog_metadata(re, *metadata_entry);
            } else {
                g.year = std::nullopt;
                g.manufacturer = std::nullopt;
                g.genre_val = std::nullopt;
                g.players_val = std::nullopt;
                g.series_val = std::nullopt;
                g.icon = std::nullopt;
                g.snapshot = std::nullopt;
                re.mame_short = g.short_name;
            }
            g.developer = std::nullopt;
            g.publisher = std::nullopt;

            // Keep Redump verification provenance in the description. MAME
            // metadata enriches the structured fields without replacing the
            // exact Redump disc identity or claiming revision-specific history.
            if (redump_verification->match == RedumpCueMatch::CompleteSet) {
                g.history_val =
                    "Verified as a complete set against the local Redump Neo "
                    "Geo CD DAT using CUE/BIN size + SHA-1.\n\nRedump: " +
                    redump.name + "\nImage: " + rel;
            } else {
                g.history_val =
                    "Identified against the local Redump Neo Geo CD DAT: all "
                    "BIN tracks match by size + SHA-1, but the CUE does not."
                    "\n\nRedump: " + redump.name + "\nImage: " + rel;
            }
        } else if (short_opt.has_value()) {
            const auto it = neocd_metadata.find(*short_opt);
            if (it == neocd_metadata.end())
                continue;

            const SoftwareEntry& data = it->second;
            g.identified = true;
            g.source = "mame";
            g.short_name = *short_opt;
            g.name = data.description;
            g.display = clean_cd_display(data.description);
            g.year = data.year;
            g.manufacturer = data.publisher;
            g.developer = std::nullopt;
            g.publisher = std::nullopt;

            // MAME's supplemental INIs often list only the parent software ID
            // for Neo Geo CD revisions. Prefer revision-specific values when
            // present, then inherit only missing descriptive fields.
            g.genre_val = supplemental_genre_with_parent_fallback(
                supplemental, *short_opt, data.cloneof);
            g.players_val = map_get_with_parent_fallback(
                supplemental.players, *short_opt, data.cloneof);
            g.series_val = map_get_with_parent_fallback(
                supplemental.series, *short_opt, data.cloneof);

            // History is revision-specific in history.xml. Do not replace a
            // short clone entry with the parent's different release history.
            g.history_val = map_get(supplemental.history.neocd, *short_opt);
            g.icon = find_icon(p.icondir, *short_opt, neocd_metadata);
            g.snapshot = find_snapshot(p.snapdir, *short_opt);

            re.mame_short = *short_opt;
            re.cloneof = data.cloneof;
            copy_catalog_metadata(re, data);
        } else {
            g.identified = false;
            g.source = "unknown";
            g.short_name = make_unknown_cd_short(rel);
            g.name = strip_grouped_tags(rel_path.stem().string());
            if (g.name.empty())
                g.name = rel_path.stem().string();
            g.display = g.name;
            g.year = std::nullopt;
            g.manufacturer = std::nullopt;
            g.developer = std::nullopt;
            g.publisher = std::nullopt;
            g.genre_val = std::nullopt;
            g.players_val = std::nullopt;
            g.series_val = std::nullopt;
            g.history_val = "Metadata not identified in neocd.xml.\n\nImage: " + rel;
            g.icon = std::nullopt;
            g.snapshot = std::nullopt;

            re.mame_short = g.short_name;
        }

        if (redump_verification.has_value()) {
            re.verification =
                redump_verification->match == RedumpCueMatch::CompleteSet
                    ? "redump-cue"
                    : "redump-tracks-only";
            for (const FileHashVerification& verified_file :
                 redump_verification->files) {
                HashDetailEntry hash;
                hash.file = verified_file.file;
                hash.catalog_file = verified_file.catalog_file;
                hash.role = verified_file.role;
                hash.algorithm = verified_file.algorithm;
                hash.size = verified_file.size;
                hash.expected_size = verified_file.expected_size;
                hash.actual = verified_file.actual_hash;
                hash.expected = verified_file.expected_hash;
                hash.matched = verified_file.matches;
                re.hashes.push_back(std::move(hash));
            }
        } else if (mame_chd_verification.has_value()) {
            HashDetailEntry hash;
            hash.file = rel;
            hash.role = "CHD";
            hash.algorithm = "SHA-1";
            std::error_code size_ec;
            const std::uintmax_t size =
                fs::file_size(cd_image_io_path, size_ec);
            if (!size_ec)
                hash.size = size;
            hash.actual = mame_chd_verification->actual_sha1;

            const auto metadata_it = neocd_metadata.find(re.mame_short);
            if (metadata_it != neocd_metadata.end()) {
                hash.catalog_file = metadata_it->second.disk_name;
                hash.expected = metadata_it->second.disk_sha1;
            }
            hash.matched =
                hash.expected.has_value() && hash.actual.has_value() &&
                to_lower(*hash.expected) == to_lower(*hash.actual);
            re.verification = hash.matched
                ? "mame-chd"
                : "mame-chd-mismatch";
            // Unlike metadata-only identification, both outcomes prove that
            // the CHD header was read and compared with neocd.xml. Preserve
            // the result at game level so summaries and badges do not report
            // a verified mismatch as unverified metadata.
            g.verification = re.verification;
            re.hashes.push_back(std::move(hash));
        }

        g.roms.push_back(std::move(re));
        game_list.push_back(std::move(g));
    }

    // Prune only after a complete scan of an available CD root and only when
    // the Redump catalog is loaded. If the external CD drive/path is offline,
    // preserve the cache so reconnecting it does not force a full re-hash.
    if (cd_root_available && !redump_metadata.empty() && !is_canceled())
        redump_sha1_cache.prune_untouched();

    if (!redump_sha1_cache.save() && progress_callback) {
        progress_callback("Warning: could not write Redump SHA-1 cache: " +
                          redump_sha1_cache.path().string() + "\n");
    }
    const auto cd_finished = ScanClock::now();

    std::sort(game_list.begin(), game_list.end(), [](const GameEntry& a, const GameEntry& b) {
        return to_lower(a.display) < to_lower(b.display);
    });

    for (auto& g : game_list) {
        std::sort(g.roms.begin(), g.roms.end(), [](const RomEntry& a, const RomEntry& b) {
            if (a.is_main != b.is_main) return a.is_main;
            std::string la = a.label.value_or("");
            std::string lb = b.label.value_or("");
            return la < lb;
        });
    }

    if (is_canceled()) {
        result.error_message = "Scan canceled.";
        return result;
    }

    json out = json::array();
    for (const auto& g : game_list) {
        json jg;
        jg["name"] = g.name;
        jg["display"] = g.display;
        jg["short"] = g.short_name;
        jg["source"] = g.source;
        jg["system"] = g.system;
        jg["identified"] = g.identified;
        jg["verification"] = opt_to_json(g.verification);
        jg["redump_id"] = opt_to_json(g.redump_id);
        jg["year"] = opt_to_json(g.year);
        jg["manufacturer"] = opt_to_json(g.manufacturer);
        jg["developer"] = opt_to_json(g.developer);
        jg["publisher"] = opt_to_json(g.publisher);
        jg["genre"] = opt_to_json(g.genre_val);
        jg["players"] = opt_to_json(g.players_val);
        jg["series"] = opt_to_json(g.series_val);
        jg["history"] = opt_to_json(g.history_val);
        jg["icon"] = opt_path_to_json(g.icon);
        jg["snapshot"] = opt_path_to_json(g.snapshot);

        json jroms = json::array();
        for (const auto& r : g.roms) {
            json jr;
            jr["file"] = r.file;
            jr["name"] = opt_to_json(r.name);
            jr["label"] = opt_to_json(r.label);
            jr["mame"] = r.mame_short;
            jr["cloneof"] = opt_to_json(r.cloneof);
            jr["alt_title"] = opt_to_json(r.alt_title);
            jr["serial"] = opt_to_json(r.serial);
            jr["release"] = opt_to_json(r.release);
            jr["part"] = opt_to_json(r.part);
            jr["interface"] = opt_to_json(r.interface);
            jr["program_width"] = opt_uint_to_json(r.program_width);
            jr["program_endianness"] =
                opt_to_json(r.program_endianness);
            jr["program_size"] = opt_uint_to_json(r.program_size);
            jr["fixed_size"] = opt_uint_to_json(r.fixed_size);
            jr["audio_cpu_size"] = opt_uint_to_json(r.audio_cpu_size);
            jr["audio_data_size"] = opt_uint_to_json(r.audio_data_size);
            jr["graphics_size"] = opt_uint_to_json(r.graphics_size);
            jr["verification"] = opt_to_json(r.verification);
            jr["crc32"] = opt_to_json(r.crc32);
            jr["expected_crc32"] = opt_to_json(r.expected_crc32);
            json jhashes = json::array();
            for (const HashDetailEntry& hash : r.hashes) {
                json jhash;
                jhash["file"] = hash.file;
                jhash["catalog_file"] = opt_to_json(hash.catalog_file);
                jhash["role"] = hash.role;
                jhash["algorithm"] = hash.algorithm;
                jhash["size"] = opt_uint_to_json(hash.size);
                jhash["expected_size"] =
                    opt_uint_to_json(hash.expected_size);
                jhash["actual"] = opt_to_json(hash.actual);
                jhash["expected"] = opt_to_json(hash.expected);
                jhash["matched"] = hash.matched;
                jhashes.push_back(std::move(jhash));
            }
            jr["hashes"] = std::move(jhashes);
            jr["main"] = r.is_main;
            jroms.push_back(std::move(jr));
        }
        jg["roms"] = std::move(jroms);
        jg["main_rom"] = opt_to_json(g.main_rom);

        out.push_back(std::move(jg));
    }

    fs::create_directories(p.outdir, ec);
    if (ec) {
        result.error_message =
            "Could not create database directory: " + ec.message();
        return result;
    }

    const QByteArray serialized = QByteArray::fromStdString(out.dump(2));
    QSaveFile output(path_to_qstring(p.out_json));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        result.error_message =
            "Could not open database file for writing: " +
            p.out_json.string() + ": " + output.errorString().toStdString();
        return result;
    }
    if (output.write(serialized) != serialized.size()) {
        result.error_message =
            "Could not write database file: " + p.out_json.string() +
            ": " + output.errorString().toStdString();
        output.cancelWriting();
        return result;
    }
    if (!output.commit()) {
        result.error_message =
            "Could not replace database file: " + p.out_json.string() +
            ": " + output.errorString().toStdString();
        return result;
    }
    const auto database_finished = ScanClock::now();

    std::string msg = "\nCreated: " + p.out_json.string() + "\n";
    if (progress_callback) progress_callback(msg);

    result.rom_file_count = 0;
    result.game_count = 0;
    result.parent_games = 0;
    result.variant_count = 0;
    result.homebrew_games = 0;
    result.neo_geolith_crc32_verified_files = 0;
    result.neo_geolith_crc32_mismatch_files = 0;
    result.neo_metadata_only_files = 0;
    result.neo_geolith_crc32_cache_hits = geolith_crc32_cache.hits();
    result.neo_geolith_crc32_calculated_files =
        geolith_crc32_cache.calculated();
    result.neo_geolith_crc32_cache_entries = geolith_crc32_cache.entries();
    result.neo_geolith_crc32_cache_pruned = geolith_crc32_cache.pruned();
    result.cd_image_count = 0;
    result.cd_game_count = 0;
    result.cd_identified_games = 0;
    result.cd_redump_cue_verified_games = 0;
    result.cd_redump_tracks_only_games = 0;
    result.cd_mame_chd_matched_games = 0;
    result.cd_mame_chd_mismatch_games = 0;
    result.cd_metadata_only_games = 0;
    result.cd_unknown_games = 0;
    result.cd_redump_sha1_cache_hits = redump_sha1_cache.hits();
    result.cd_redump_sha1_calculated_files = redump_sha1_cache.calculated();
    result.cd_redump_sha1_cache_entries = redump_sha1_cache.entries();
    result.cd_redump_sha1_cache_pruned = redump_sha1_cache.pruned();

    for (const auto& g : game_list) {
        if (g.system == "neogeocd") {
            result.cd_game_count++;
            result.cd_image_count += g.roms.size();
            if (g.identified) result.cd_identified_games++;
            else result.cd_unknown_games++;
            if (g.verification == std::optional<std::string>("redump-cue"))
                result.cd_redump_cue_verified_games++;
            else if (g.verification == std::optional<std::string>("redump-tracks-only"))
                result.cd_redump_tracks_only_games++;
            else if (g.verification == std::optional<std::string>("mame-chd"))
                result.cd_mame_chd_matched_games++;
            else if (g.verification ==
                     std::optional<std::string>("mame-chd-mismatch"))
                result.cd_mame_chd_mismatch_games++;
            else if (g.identified)
                result.cd_metadata_only_games++;
            continue;
        }

        result.game_count++;
        result.rom_file_count += g.roms.size();
        for (const RomEntry& rom : g.roms) {
            if (rom.verification ==
                std::optional<std::string>("geolith-crc32")) {
                result.neo_geolith_crc32_verified_files++;
            } else if (rom.verification ==
                       std::optional<std::string>(
                           "geolith-crc32-mismatch")) {
                result.neo_geolith_crc32_mismatch_files++;
            } else {
                result.neo_metadata_only_files++;
            }
        }
        if (g.source == "homebrew") result.homebrew_games++;
        if (g.main_rom.has_value()) result.parent_games++;
    }
    result.variant_count =
        result.rom_file_count >= result.game_count
            ? result.rom_file_count - result.game_count
            : 0;
    const auto scan_finished = ScanClock::now();

    std::string summary = "\n========== GOLIATH DATABASE ==========\n\n";
    summary += "Neo Geo MVS/AES\n\n";
    summary += "ROM files                : " + std::to_string(result.rom_file_count) + "\n\n";
    summary += "Parents                  : " + std::to_string(result.parent_games) + "\n";
    summary += "Variants (clones/hacks)  : " + std::to_string(result.variant_count) + "\n";
    summary += "Homebrew                 : " + std::to_string(result.homebrew_games) + "\n\n";
    summary += "Geolith CRC-32 verified  : " +
               std::to_string(result.neo_geolith_crc32_verified_files) +
               "\n";
    summary += "CRC-32 mismatches        : " +
               std::to_string(result.neo_geolith_crc32_mismatch_files) +
               "\n";
    summary += "Metadata only            : " +
               std::to_string(result.neo_metadata_only_files) + "\n\n";
    summary += "Geolith CRC-32 cache hits: " +
               std::to_string(result.neo_geolith_crc32_cache_hits) + "\n";
    summary += "Geolith CRC-32 calculated: " +
               std::to_string(result.neo_geolith_crc32_calculated_files) +
               "\n";
    summary += "Geolith CRC-32 cache rows: " +
               std::to_string(result.neo_geolith_crc32_cache_entries) +
               "\n";
    summary += "Geolith CRC-32 pruned    : " +
               std::to_string(result.neo_geolith_crc32_cache_pruned) +
               "\n\n";
    summary += "Neo Geo CD\n\n";
    summary += "Images                   : " + std::to_string(result.cd_image_count) + "\n";
    summary += "Games                    : " + std::to_string(result.cd_game_count) + "\n";
    summary += "Identified               : " + std::to_string(result.cd_identified_games) + "\n";
    summary += "Redump sets verified     : " + std::to_string(result.cd_redump_cue_verified_games) + "\n";
    summary += "Redump tracks only       : " + std::to_string(result.cd_redump_tracks_only_games) + "\n";
    summary += "MAME sets matched        : " + std::to_string(result.cd_mame_chd_matched_games) + "\n";
    summary += "CHD mismatches           : " + std::to_string(result.cd_mame_chd_mismatch_games) + "\n";
    summary += "Metadata only            : " + std::to_string(result.cd_metadata_only_games) + "\n";
    summary += "Unknown                  : " + std::to_string(result.cd_unknown_games) + "\n";
    summary += "Redump SHA-1 cache hits  : " + std::to_string(result.cd_redump_sha1_cache_hits) + "\n";
    summary += "Redump SHA-1 calculated  : " + std::to_string(result.cd_redump_sha1_calculated_files) + "\n";
    summary += "Redump SHA-1 cache rows  : " + std::to_string(result.cd_redump_sha1_cache_entries) + "\n";
    summary += "Redump SHA-1 pruned      : " + std::to_string(result.cd_redump_sha1_cache_pruned) + "\n\n";
    summary += "Scan timing (ms)\n\n";
    summary += "Metadata and indexes     : " +
               std::to_string(elapsed_ms(scan_started, metadata_finished)) +
               "\n";
    summary += "File discovery           : " +
               std::to_string(elapsed_ms(metadata_finished,
                                         discovery_finished)) +
               "\n";
    summary += "MVS/AES verification     : " +
               std::to_string(elapsed_ms(discovery_finished,
                                         cartridge_finished)) +
               "\n";
    summary += "Neo Geo CD verification  : " +
               std::to_string(elapsed_ms(cartridge_finished, cd_finished)) +
               "\n";
    summary += "Database build/write     : " +
               std::to_string(elapsed_ms(cd_finished, database_finished)) +
               "\n";
    summary += "Statistics               : " +
               std::to_string(elapsed_ms(database_finished, scan_finished)) +
               "\n";
    summary += "Total                    : " +
               std::to_string(elapsed_ms(scan_started, scan_finished)) +
               "\n\n";
    summary += "======================================\n";


    if (progress_callback) progress_callback(summary);

    result.success = true;
    return result;
}

} // namespace goliath
