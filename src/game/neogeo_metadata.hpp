#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace goliath {

using MetadataProgressCallback = std::function<void(const std::string&)>;

enum class NeoGeoMedia {
    Cartridge,
    CD,
};

struct SoftwareEntry {
    NeoGeoMedia media = NeoGeoMedia::Cartridge;
    std::string description;
    std::optional<std::string> year;
    std::optional<std::string> publisher;
    std::optional<std::string> cloneof;
    std::optional<std::string> serial;
    std::optional<std::string> release;
    std::optional<std::string> alt_title;
    std::optional<std::string> release_types;
    std::optional<std::string> compatibility;
    std::optional<std::string> disk_name;
    std::optional<std::string> disk_sha1;
    std::string supported = "yes";
};

using SoftwareCatalog = std::map<std::string, SoftwareEntry>;

struct RedumpRomEntry {
    std::string name;
    std::uintmax_t size = 0;
    std::optional<std::string> crc;
    std::optional<std::string> md5;
    std::optional<std::string> sha1;
};

struct RedumpGameEntry {
    std::string name;
    std::optional<std::string> id;
    std::optional<std::string> category;
    std::string description;
    std::vector<RedumpRomEntry> roms;
};

using RedumpCatalog = std::vector<RedumpGameEntry>;
using MameChdHashCatalog = std::map<std::string, std::string>;

struct CdMatchIndex {
    std::unordered_map<std::string, std::string> exact;
    std::vector<std::pair<std::string, std::string>> aliases;
};

struct HistoryLists {
    std::unordered_map<std::string, std::string> neogeo;
    std::unordered_map<std::string, std::string> neocd;
};

struct SupplementalMetadata {
    std::unordered_map<std::string, std::string> catver;
    std::unordered_map<std::string, std::string> catlist;
    std::unordered_map<std::string, std::string> players;
    std::unordered_map<std::string, std::string> series;
    std::unordered_map<std::string, std::string> genre;
    HistoryLists history;
};

// Shared software-list loader used by both Neo Geo cartridge metadata and the
// Neo Geo CD catalog. Keeping one parser avoids drift between neogeo.xml and
// neocd.xml while leaving scan orchestration in db_scanner.cpp.
void load_software_list_xml(const std::filesystem::path& xml_path,
                            const std::string& expected_list,
                            NeoGeoMedia media,
                            SoftwareCatalog& catalog,
                            MetadataProgressCallback callback = nullptr);

// Supplementary MAME metadata lives beside the software-list XML files. Keep
// INI/history parsing here so db_scanner.cpp only coordinates already-loaded
// metadata. The relevant-ID overload avoids retaining unrelated MAME INI rows;
// History remains independently filtered by its Neo Geo list identifiers.
SupplementalMetadata load_supplemental_metadata(
    const std::filesystem::path& metadata_dir,
    MetadataProgressCallback callback = nullptr);
SupplementalMetadata load_supplemental_metadata(
    const std::filesystem::path& metadata_dir,
    const std::unordered_set<std::string>& relevant_ids,
    MetadataProgressCallback callback = nullptr);

// Preserve Goliath's established grouping for a small set of cartridge
// entries that are absent or represented differently in MAME's software list.
void apply_cartridge_compatibility(SoftwareCatalog& catalog,
                                   MetadataProgressCallback callback = nullptr);

// Redump DAT discovery/parsing stays metadata-only. Verification of the parsed
// size/SHA-1 rows is handled separately by neocd_verification.cpp.
std::optional<std::filesystem::path> find_redump_neocd_dat(
    const std::filesystem::path& metadata_dir);
void load_redump_neocd_dat(const std::filesystem::path& dat_path,
                           RedumpCatalog& catalog,
                           MetadataProgressCallback callback = nullptr);

// Normalization/matching helpers used to connect filenames and verified Redump
// titles to MAME's neocd.xml software entries.
std::string make_desc_key(const std::string& input);
std::string strip_grouped_tags(const std::string& input);
std::string make_cd_title_key(const std::string& input);
CdMatchIndex build_cd_match_index(const SoftwareCatalog& catalog);
std::optional<std::string> find_cd_game(const std::filesystem::path& relative_path,
                                        const CdMatchIndex& index);
std::optional<std::string> find_redump_cd_metadata(
    const RedumpGameEntry& entry,
    const CdMatchIndex& index,
    const SoftwareCatalog& catalog);
MameChdHashCatalog build_mame_chd_hash_catalog(const SoftwareCatalog& catalog);

} // namespace goliath
