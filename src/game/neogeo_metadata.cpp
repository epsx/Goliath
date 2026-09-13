#include "neogeo_metadata.hpp"

#include "tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace fs = std::filesystem;
using TinyXmlDocument = tinyxml2::XMLDocument;
using TinyXmlElement = tinyxml2::XMLElement;

namespace goliath {
namespace {

std::string trim(const std::string& s) {
    const std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::optional<std::string> child_text(TinyXmlElement* parent, const char* name) {
    if (!parent)
        return std::nullopt;

    TinyXmlElement* child = parent->FirstChildElement(name);
    if (!child || !child->GetText())
        return std::nullopt;

    return std::string(child->GetText());
}

std::optional<std::uintmax_t> parse_uintmax(const char* text) {
    if (!text || !*text)
        return std::nullopt;

    try {
        std::size_t used = 0;
        const std::string value = text;
        const unsigned long long parsed = std::stoull(value, &used, 10);
        if (used != value.size())
            return std::nullopt;
        return static_cast<std::uintmax_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

bool is_redump_neocd_document(TinyXmlDocument& doc) {
    TinyXmlElement* root = doc.RootElement();
    if (!root || std::string(root->Name() ? root->Name() : "") != "datafile")
        return false;

    TinyXmlElement* header = root->FirstChildElement("header");
    const auto name = child_text(header, "name");
    return name.has_value() && trim(*name) == "SNK - Neo Geo CD";
}

std::string expand_squared_tokens(std::string input) {
    // Redump/TOSEC commonly writes "Bang^2 Busters" for "Bang Bang Busters".
    // Expand that notation before punctuation normalization so the title can
    // match the MAME software-list description without a game-specific hack.
    static const std::regex squared(R"(([A-Za-z0-9]+)\^2)");
    return std::regex_replace(input, squared, "$1 $1");
}

void append_bracketed_aliases(const std::string& input,
                              std::vector<std::string>& aliases) {
    std::size_t search_from = 0;
    while (search_from < input.size()) {
        const std::size_t open = input.find('[', search_from);
        if (open == std::string::npos)
            break;

        const std::size_t close = input.find(']', open + 1);
        if (close == std::string::npos)
            break;

        const std::string alias = trim(input.substr(open + 1,
                                                     close - open - 1));
        if (!alias.empty())
            aliases.push_back(alias);

        search_from = close + 1;
    }
}

std::vector<std::string> cd_aliases_for_entry(const std::string& short_name,
                                               const SoftwareEntry& entry) {
    std::vector<std::string> aliases;
    aliases.push_back(short_name);
    aliases.push_back(entry.description);

    // MAME descriptions often carry regional aliases separated by '~'.
    std::size_t start = 0;
    while (start <= entry.description.size()) {
        const std::size_t pos = entry.description.find('~', start);
        aliases.push_back(trim(entry.description.substr(
            start, pos == std::string::npos ? std::string::npos : pos - start)));
        if (pos == std::string::npos)
            break;
        start = pos + 1;
    }

    if (entry.disk_name.has_value()) {
        aliases.push_back(*entry.disk_name);
        // MAME CHD names frequently keep an alternate regional title inside
        // square brackets. Preserve that title as an independent alias before
        // make_cd_title_key() removes grouped tags from the full disk name.
        append_bracketed_aliases(*entry.disk_name, aliases);
    }

    return aliases;
}

std::unordered_map<std::string, std::string> load_ini(
    const fs::path& path,
    const std::optional<std::string>& section = std::nullopt,
    const std::unordered_set<std::string>* relevant_ids = nullptr) {
    std::unordered_map<std::string, std::string> data;
    std::error_code ec;
    if (path.empty() || !fs::is_regular_file(path, ec))
        return data;

    std::ifstream in(path, std::ios::binary);
    std::string line;
    std::string current_section;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#')
            continue;
        if (t.starts_with("RootFolder") || t.starts_with("SubFolder"))
            continue;

        if (t.front() == '[' && t.back() == ']') {
            current_section = trim(t.substr(1, t.size() - 2));
            continue;
        }

        if (section.has_value() && current_section != *section)
            continue;

        const std::size_t eq = t.find('=');
        if (eq == std::string::npos)
            continue;

        const std::string key = trim(t.substr(0, eq));
        const std::string value = trim(t.substr(eq + 1));
        if (!key.empty() &&
            (!relevant_ids || relevant_ids->contains(key))) {
            data[key] = value;
        }
    }

    return data;
}

std::unordered_map<std::string, std::string> load_folder_ini(
    const fs::path& path,
    const std::unordered_set<std::string>* relevant_ids = nullptr) {
    std::unordered_map<std::string, std::string> data;
    std::error_code ec;
    if (path.empty() || !fs::is_regular_file(path, ec))
        return data;

    std::ifstream in(path, std::ios::binary);
    std::string line;
    std::string current;
    bool have_current = false;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#')
            continue;
        if (t.starts_with("RootFolder") || t.starts_with("SubFolder"))
            continue;

        if (t == "[ROOT_FOLDER]") {
            have_current = false;
            continue;
        }

        if (t.front() == '[' && t.back() == ']') {
            current = trim(t.substr(1, t.size() - 2));
            have_current = !current.empty();
            continue;
        }

        if (have_current &&
            (!relevant_ids || relevant_ids->contains(t))) {
            data[t] = current;
        }
    }

    return data;
}

std::string collapse_blank_lines(const std::string& text) {
    static const std::regex blank_lines(R"(\n\s*\n+)");
    return std::regex_replace(trim(text), blank_lines, "\n\n");
}

bool is_machine_only_cartridge_history_id(const std::string& short_name) {
    static const std::unordered_set<std::string> ids = {
        "mslug2t", "vliner", "vliner53", "vliner54", "vliner6e", "vliner7e",
    };
    return ids.find(short_name) != ids.end();
}

HistoryLists load_history(const fs::path& history_path,
                          MetadataProgressCallback callback) {
    HistoryLists history;

    std::error_code ec;
    if (history_path.empty() || !fs::is_regular_file(history_path, ec))
        return history;

    if (callback)
        callback("Loading History...\n");

    TinyXmlDocument doc;
    if (doc.LoadFile(history_path.string().c_str()) != tinyxml2::XML_SUCCESS) {
        if (callback)
            callback("Warning: could not parse " + history_path.string() + "\n");
        return history;
    }

    TinyXmlElement* root = doc.RootElement();
    if (!root)
        return history;

    for (TinyXmlElement* entry = root->FirstChildElement("entry");
         entry;
         entry = entry->NextSiblingElement("entry")) {
        std::vector<std::string> neogeo_names;
        std::vector<std::string> neocd_names;

        // Keep cartridge and CD history separate even when the same MAME
        // software shortname exists in both lists (for example "mslug").
        if (TinyXmlElement* software = entry->FirstChildElement("software")) {
            for (TinyXmlElement* item = software->FirstChildElement("item");
                 item;
                 item = item->NextSiblingElement("item")) {
                const char* list = item->Attribute("list");
                const char* name = item->Attribute("name");
                if (!list || !name || !*name)
                    continue;

                if (std::string(list) == "neogeo")
                    neogeo_names.emplace_back(name);
                else if (std::string(list) == "neocd")
                    neocd_names.emplace_back(name);
            }
        }

        // History stores the few cartridge machine-only entries missing from
        // hash/neogeo.xml under <systems>. Whitelist only those known IDs.
        if (TinyXmlElement* systems = entry->FirstChildElement("systems")) {
            for (TinyXmlElement* system = systems->FirstChildElement("system");
                 system;
                 system = system->NextSiblingElement("system")) {
                const char* name = system->Attribute("name");
                if (name && *name && is_machine_only_cartridge_history_id(name))
                    neogeo_names.emplace_back(name);
            }
        }

        // history.xml contains entries for every MAME system. Normalize the
        // potentially large text only after the entry is known to affect one
        // of Goliath's two Neo Geo catalogs.
        if (neogeo_names.empty() && neocd_names.empty())
            continue;

        TinyXmlElement* text_el = entry->FirstChildElement("text");
        if (!text_el || !text_el->GetText())
            continue;

        const std::string text = collapse_blank_lines(text_el->GetText());
        if (text.empty())
            continue;

        for (const std::string& name : neogeo_names)
            history.neogeo.try_emplace(name, text);
        for (const std::string& name : neocd_names)
            history.neocd.try_emplace(name, text);
    }

    if (callback) {
        callback("History entries (Neo Geo): " +
                 std::to_string(history.neogeo.size()) + "\n");
        callback("History entries (Neo Geo CD): " +
                 std::to_string(history.neocd.size()) + "\n");
    }

    return history;
}

SupplementalMetadata load_supplemental_metadata_impl(
    const fs::path& metadata_dir,
    const std::unordered_set<std::string>* relevant_ids,
    MetadataProgressCallback callback) {
    SupplementalMetadata metadata;
    metadata.catver = load_ini(metadata_dir / "catver.ini",
                               std::string("Category"), relevant_ids);
    metadata.catlist = load_folder_ini(metadata_dir / "catlist.ini",
                                       relevant_ids);
    metadata.players = load_ini(metadata_dir / "nplayers.ini", std::nullopt,
                                relevant_ids);
    metadata.series = load_folder_ini(metadata_dir / "series.ini",
                                      relevant_ids);
    metadata.genre = load_folder_ini(metadata_dir / "genre.ini",
                                     relevant_ids);
    metadata.history = load_history(metadata_dir / "history.xml", callback);
    return metadata;
}

} // namespace

std::string make_desc_key(const std::string& input) {
    // Software-list descriptions and canonical ROM names often differ only in
    // separators, e.g. "Aero Fighters 2 / Sonic Wings 2" vs
    // "Aero Fighters 2 - Sonic Wings 2". Normalize punctuation to spaces,
    // but keep the text inside parentheses so clone/revision names remain
    // distinguishable when description matching is used as a fallback.
    const std::string lower = to_lower(input);
    std::string out;
    out.reserve(lower.size());
    bool previous_space = false;

    for (unsigned char c : lower) {
        const bool word = (c >= 0x80) || std::isalnum(c);
        if (word) {
            out.push_back(static_cast<char>(c));
            previous_space = false;
        } else if (!previous_space) {
            out.push_back(' ');
            previous_space = true;
        }
    }

    return trim(out);
}

void load_software_list_xml(const fs::path& xml_path,
                            const std::string& expected_list,
                            NeoGeoMedia media,
                            SoftwareCatalog& catalog,
                            MetadataProgressCallback callback) {
    std::error_code ec;
    if (xml_path.empty() || !fs::is_regular_file(xml_path, ec))
        return;

    TinyXmlDocument doc;
    if (doc.LoadFile(xml_path.string().c_str()) != tinyxml2::XML_SUCCESS) {
        if (callback)
            callback("Warning: could not parse " + xml_path.string() + "\n");
        return;
    }

    TinyXmlElement* root = doc.RootElement();
    if (!root || std::string(root->Name() ? root->Name() : "") != "softwarelist") {
        if (callback)
            callback("Warning: invalid MAME software list: " + xml_path.string() + "\n");
        return;
    }

    const char* list_name = root->Attribute("name");
    if (!list_name || expected_list != list_name) {
        if (callback)
            callback("Warning: expected software list '" + expected_list + "' in " +
                     xml_path.string() + "\n");
        return;
    }

    for (TinyXmlElement* software = root->FirstChildElement("software");
         software;
         software = software->NextSiblingElement("software")) {

        const char* name_attr = software->Attribute("name");
        auto description = child_text(software, "description");
        if (!name_attr || !*name_attr || !description.has_value())
            continue;

        SoftwareEntry entry;
        entry.media = media;
        entry.description = *description;
        entry.year = child_text(software, "year");
        entry.publisher = child_text(software, "publisher");

        if (const char* cloneof = software->Attribute("cloneof"); cloneof && *cloneof)
            entry.cloneof = cloneof;
        if (const char* supported = software->Attribute("supported"); supported && *supported)
            entry.supported = supported;

        for (TinyXmlElement* info = software->FirstChildElement("info");
             info;
             info = info->NextSiblingElement("info")) {
            const char* info_name = info->Attribute("name");
            const char* value = info->Attribute("value");
            if (!info_name || !value)
                continue;

            const std::string key = info_name;
            if (key == "serial") entry.serial = value;
            else if (key == "release") entry.release = value;
            else if (key == "alt_title") entry.alt_title = value;
        }

        for (TinyXmlElement* feat = software->FirstChildElement("sharedfeat");
             feat;
             feat = feat->NextSiblingElement("sharedfeat")) {
            const char* feat_name = feat->Attribute("name");
            const char* value = feat->Attribute("value");
            if (!feat_name || !value)
                continue;

            const std::string key = feat_name;
            if (key == "release") entry.release_types = value;
            else if (key == "compatibility") entry.compatibility = value;
        }

        if (media == NeoGeoMedia::CD) {
            if (TinyXmlElement* part = software->FirstChildElement("part")) {
                if (TinyXmlElement* diskarea = part->FirstChildElement("diskarea")) {
                    if (TinyXmlElement* disk = diskarea->FirstChildElement("disk")) {
                        if (const char* disk_name = disk->Attribute("name"); disk_name && *disk_name)
                            entry.disk_name = disk_name;
                        if (const char* sha1 = disk->Attribute("sha1"); sha1 && *sha1)
                            entry.disk_sha1 = sha1;
                    }
                }
            }
        }

        catalog[name_attr] = std::move(entry);
    }

    if (callback) {
        const std::string label =
            media == NeoGeoMedia::Cartridge ? "Neo Geo cartridge" : "Neo Geo CD";
        callback(label + " software entries: " + std::to_string(catalog.size()) + "\n");
    }
}

SupplementalMetadata load_supplemental_metadata(
    const fs::path& metadata_dir,
    MetadataProgressCallback callback) {
    return load_supplemental_metadata_impl(metadata_dir, nullptr,
                                           std::move(callback));
}

SupplementalMetadata load_supplemental_metadata(
    const fs::path& metadata_dir,
    const std::unordered_set<std::string>& relevant_ids,
    MetadataProgressCallback callback) {
    return load_supplemental_metadata_impl(metadata_dir, &relevant_ids,
                                           std::move(callback));
}

void apply_cartridge_compatibility(SoftwareCatalog& catalog,
                                   MetadataProgressCallback callback) {
    const std::size_t before = catalog.size();

    auto add_entry = [&catalog](const std::string& short_name,
                                const std::string& description,
                                const std::string& year,
                                const std::string& publisher,
                                const std::optional<std::string>& cloneof) {
        if (catalog.find(short_name) != catalog.end())
            return;

        SoftwareEntry entry;
        entry.media = NeoGeoMedia::Cartridge;
        entry.description = description;
        entry.year = year;
        entry.publisher = publisher;
        entry.cloneof = cloneof;
        catalog.emplace(short_name, std::move(entry));
    };

    // Present in MAME's Neo Geo machine set and in Geolith's supported .neo
    // set, but intentionally not represented as software-list items.
    add_entry("mslug2t", "Metal Slug 2 Turbo (NGM-9410) (hack)",
              "2015", "hack (trap15)", std::string("mslug2"));

    add_entry("vliner", "V-Liner (v0.7a)",
              "2001", "Dyna / BrezzaSoft", std::nullopt);
    add_entry("vliner53", "V-Liner (v0.53)",
              "2001", "Dyna / BrezzaSoft", std::string("vliner"));
    add_entry("vliner54", "V-Liner (v0.54)",
              "2001", "Dyna / BrezzaSoft", std::string("vliner"));
    add_entry("vliner6e", "V-Liner (v0.6e)",
              "2001", "Dyna / BrezzaSoft", std::string("vliner"));
    add_entry("vliner7e", "V-Liner (v0.7e)",
              "2001", "Dyna / BrezzaSoft", std::string("vliner"));

    // The software list keeps both Robo Army dumps as standalone software
    // items, while the MAME machine set (and Goliath's existing database)
    // groups roboarmya under roboarmy. Keep that established grouping.
    auto robo = catalog.find("roboarmya");
    if (robo != catalog.end())
        robo->second.cloneof = "roboarmy";

    if (callback && catalog.size() != before) {
        callback("Neo Geo cartridge compatibility entries: " +
                 std::to_string(catalog.size() - before) + "\n");
    }
}

std::optional<fs::path> find_redump_neocd_dat(const fs::path& metadata_dir) {
    std::error_code ec;
    if (!fs::is_directory(metadata_dir, ec))
        return std::nullopt;

    std::vector<fs::path> candidates;
    for (const auto& entry : fs::directory_iterator(metadata_dir, ec)) {
        if (ec)
            break;

        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec))
            continue;
        if (to_lower(entry.path().extension().string()) == ".dat")
            candidates.push_back(entry.path());
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const fs::path& a, const fs::path& b) {
                  return to_lower(a.filename().string()) <
                         to_lower(b.filename().string());
              });

    for (const fs::path& path : candidates) {
        TinyXmlDocument doc;
        if (doc.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS)
            continue;
        if (is_redump_neocd_document(doc))
            return path;
    }

    return std::nullopt;
}

void load_redump_neocd_dat(const fs::path& dat_path,
                           RedumpCatalog& catalog,
                           MetadataProgressCallback callback) {
    catalog.clear();

    std::error_code ec;
    if (dat_path.empty() || !fs::is_regular_file(dat_path, ec))
        return;

    TinyXmlDocument doc;
    if (doc.LoadFile(dat_path.string().c_str()) != tinyxml2::XML_SUCCESS) {
        if (callback)
            callback("Warning: could not parse Redump Neo Geo CD DAT: " +
                     dat_path.string() + "\\n");
        return;
    }

    if (!is_redump_neocd_document(doc)) {
        if (callback)
            callback("Warning: invalid Redump Neo Geo CD DAT: " +
                     dat_path.string() + "\\n");
        return;
    }

    TinyXmlElement* root = doc.RootElement();
    for (TinyXmlElement* game = root->FirstChildElement("game");
         game;
         game = game->NextSiblingElement("game")) {

        const char* game_name = game->Attribute("name");
        if (!game_name || !*game_name)
            continue;

        RedumpGameEntry entry;
        entry.name = game_name;
        if (const char* id = game->Attribute("id"); id && *id)
            entry.id = id;
        entry.category = child_text(game, "category");
        entry.description = child_text(game, "description").value_or(entry.name);

        for (TinyXmlElement* rom = game->FirstChildElement("rom");
             rom;
             rom = rom->NextSiblingElement("rom")) {

            const char* rom_name = rom->Attribute("name");
            const auto size = parse_uintmax(rom->Attribute("size"));
            const char* sha1 = rom->Attribute("sha1");

            // Track verification needs an exact byte length and SHA-1. Keep
            // incomplete DAT rows out of the catalog instead of weakening the
            // identity check.
            if (!rom_name || !*rom_name || !size.has_value() || !sha1 || !*sha1)
                continue;

            RedumpRomEntry file;
            file.name = rom_name;
            file.size = *size;
            if (const char* crc = rom->Attribute("crc"); crc && *crc)
                file.crc = to_lower(crc);
            if (const char* md5 = rom->Attribute("md5"); md5 && *md5)
                file.md5 = to_lower(md5);
            file.sha1 = to_lower(sha1);
            entry.roms.push_back(std::move(file));
        }

        if (!entry.roms.empty())
            catalog.push_back(std::move(entry));
    }

    if (callback) {
        callback("Redump Neo Geo CD entries: " +
                 std::to_string(catalog.size()) + "\\n");
    }
}

std::string strip_grouped_tags(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    int paren_depth = 0;
    int bracket_depth = 0;
    for (char c : input) {
        if (c == '(') { ++paren_depth; continue; }
        if (c == ')' && paren_depth > 0) { --paren_depth; continue; }
        if (c == '[') { ++bracket_depth; continue; }
        if (c == ']' && bracket_depth > 0) { --bracket_depth; continue; }
        if (paren_depth == 0 && bracket_depth == 0)
            out.push_back(c);
    }
    return trim(out);
}

std::string make_cd_title_key(const std::string& input) {
    std::string s = strip_grouped_tags(expand_squared_tokens(input));
    s = make_desc_key(s);

    // Normalize the common "The Foo" / "Foo, The" naming difference.
    if (s.starts_with("the "))
        s = trim(s.substr(4));
    if (s.size() > 4 && s.ends_with(" the"))
        s = trim(s.substr(0, s.size() - 4));

    return s;
}

CdMatchIndex build_cd_match_index(const SoftwareCatalog& catalog) {
    CdMatchIndex index;
    std::unordered_set<std::string> ambiguous;

    auto add_alias = [&](const std::string& raw, const std::string& short_name) {
        const std::string key = make_cd_title_key(raw);
        if (key.empty())
            return;

        auto it = index.exact.find(key);
        if (it == index.exact.end() && !ambiguous.contains(key)) {
            index.exact.emplace(key, short_name);
        } else if (it != index.exact.end() && it->second != short_name) {
            index.exact.erase(it);
            ambiguous.insert(key);
        }
    };

    for (const auto& [short_name, entry] : catalog) {
        for (const auto& alias : cd_aliases_for_entry(short_name, entry))
            add_alias(alias, short_name);
    }

    index.aliases.reserve(index.exact.size());
    for (const auto& [key, short_name] : index.exact) {
        // Very short aliases are poor containment candidates (for example
        // short MAME IDs), but are still valid for an exact filename match.
        if (key.size() >= 6)
            index.aliases.emplace_back(key, short_name);
    }

    std::sort(index.aliases.begin(), index.aliases.end(),
              [](const auto& a, const auto& b) {
                  return a.first.size() > b.first.size();
              });
    return index;
}

std::optional<std::string> find_cd_game(const fs::path& relative_path,
                                        const CdMatchIndex& index) {
    std::vector<std::string> candidates;
    candidates.push_back(make_cd_title_key(relative_path.stem().string()));

    const fs::path parent = relative_path.parent_path();
    if (!parent.empty() && parent != ".")
        candidates.push_back(make_cd_title_key(parent.filename().string()));

    // First try only deterministic exact aliases.
    for (const std::string& candidate : candidates) {
        if (candidate.empty())
            continue;
        auto it = index.exact.find(candidate);
        if (it != index.exact.end())
            return it->second;
    }

    // Conservative fallback: an alias may be embedded in a longer Redump or
    // TOSEC filename. Prefer the most specific (longest) matching alias; this
    // avoids a generic title such as "Super Sidekicks" defeating the more
    // precise "Super Sidekicks 2". Equal-length conflicting hits stay unknown.
    std::optional<std::string> matched;
    std::size_t best_alias_length = 0;
    for (const std::string& candidate : candidates) {
        if (candidate.empty())
            continue;
        for (const auto& [alias, short_name] : index.aliases) {
            if (candidate.find(alias) == std::string::npos)
                continue;

            if (alias.size() > best_alias_length) {
                best_alias_length = alias.size();
                matched = short_name;
            } else if (alias.size() == best_alias_length &&
                       matched.has_value() && *matched != short_name) {
                return std::nullopt;
            }
        }
    }
    return matched;
}

std::optional<std::string> find_redump_cd_metadata(
        const RedumpGameEntry& entry,
        const CdMatchIndex& index,
        const SoftwareCatalog& catalog) {
    // Redump IDs are stable and avoid guessing between region/revision discs
    // whose normalized titles collapse to the same MAME software-list family.
    // These entries are the complete set of verified titles in the current
    // Neo Geo CD DAT that cannot be resolved by the generic title index.
    static const std::unordered_map<std::string, std::string> id_overrides = {
        {"32460", "doubledr"},
        {"30908", "doubledr"},
        {"49698", "fatfury3"},
        {"32484", "fatfury3"},
        {"32481", "fatfury3"},
        {"49536", "fatfury3"},
        {"32226", "kof94"},
        {"24407", "kof94"},
        {"59077", "kof95"},
        {"24412", "kof95"},
        {"24413", "kof96"},
        {"63438", "mahretsu"},
        {"75662", "neodrift"},
        {"59256", "stakwin"},
    };

    if (entry.id.has_value()) {
        const auto override_it = id_overrides.find(*entry.id);
        if (override_it != id_overrides.end() &&
            catalog.contains(override_it->second)) {
            return override_it->second;
        }
    }

    const std::optional<std::string> matched =
        find_cd_game(fs::path(entry.name), index);
    if (matched.has_value() && catalog.contains(*matched))
        return matched;
    return std::nullopt;
}

MameChdHashCatalog build_mame_chd_hash_catalog(const SoftwareCatalog& catalog) {
    MameChdHashCatalog hashes;
    for (const auto& [short_name, entry] : catalog) {
        if (entry.disk_sha1.has_value())
            hashes.emplace(short_name, *entry.disk_sha1);
    }
    return hashes;
}

} // namespace goliath
