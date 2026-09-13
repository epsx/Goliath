#include "neocd_verification.hpp"

#include "common/sha1.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

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

struct CueTrackFile {
    fs::path path;
    std::uintmax_t size = 0;
};

std::optional<fs::path> resolve_cue_track_path(const fs::path& cue_dir,
                                               std::string referenced) {
    std::replace(referenced.begin(), referenced.end(), '\\', '/');
    fs::path rel = fs::path(referenced).lexically_normal();
    if (rel.empty() || rel.is_absolute())
        return std::nullopt;
    for (const auto& part : rel) {
        if (part == "..")
            return std::nullopt;
    }

    std::error_code ec;
    fs::path exact = cue_dir / rel;
    if (fs::is_regular_file(exact, ec))
        return exact;

    // CUE files are often authored on a case-insensitive filesystem. Resolve
    // only the final component case-insensitively so the same set can be
    // verified on Linux without weakening path traversal rules.
    const fs::path parent = exact.parent_path();
    if (!fs::is_directory(parent, ec))
        return std::nullopt;
    const std::string wanted = to_lower(exact.filename().string());
    for (const auto& entry : fs::directory_iterator(parent, ec)) {
        if (ec)
            break;
        std::error_code entry_ec;
        if (entry.is_regular_file(entry_ec) &&
            to_lower(entry.path().filename().string()) == wanted) {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::optional<std::vector<CueTrackFile>> cue_track_files(
    const fs::path& cue_path) {
    std::ifstream in(cue_path, std::ios::binary);
    if (!in)
        return std::nullopt;

    std::vector<CueTrackFile> files;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::string t = trim(line);
        const std::string lower = to_lower(t);
        if (!lower.starts_with("file "))
            continue;

        std::string rest = trim(t.substr(5));
        std::string referenced;
        if (!rest.empty() && rest.front() == '"') {
            const std::size_t end = rest.find('"', 1);
            if (end == std::string::npos)
                return std::nullopt;
            referenced = rest.substr(1, end - 1);
        } else {
            const std::size_t end = rest.find_first_of(" \t");
            referenced = rest.substr(0, end);
        }
        if (referenced.empty())
            return std::nullopt;

        const auto resolved =
            resolve_cue_track_path(cue_path.parent_path(), referenced);
        if (!resolved.has_value())
            return std::nullopt;

        std::error_code ec;
        const std::uintmax_t size = fs::file_size(*resolved, ec);
        if (ec)
            return std::nullopt;

        files.push_back({*resolved, size});
    }

    if (files.empty())
        return std::nullopt;
    return files;
}

std::vector<const RedumpRomEntry*> redump_track_rows(
    const RedumpGameEntry& game) {
    std::vector<const RedumpRomEntry*> rows;
    rows.reserve(game.roms.size());
    for (const RedumpRomEntry& rom : game.roms) {
        if (to_lower(fs::path(rom.name).extension().string()) == ".cue")
            continue;
        rows.push_back(&rom);
    }
    return rows;
}

const RedumpRomEntry* redump_cue_row(const RedumpGameEntry& game) {
    const RedumpRomEntry* cue = nullptr;
    for (const RedumpRomEntry& rom : game.roms) {
        if (to_lower(fs::path(rom.name).extension().string()) != ".cue")
            continue;
        if (cue != nullptr)
            return nullptr;
        cue = &rom;
    }
    return cue;
}

std::optional<std::size_t> select_catalog_match(
    const fs::path& cue_path,
    const RedumpCatalog& catalog,
    const std::vector<std::size_t>& matches,
    RedumpTitleKeyFunction title_key) {
    if (matches.size() == 1)
        return matches.front();
    if (matches.empty() || title_key == nullptr)
        return std::nullopt;

    const std::string cue_key = title_key(cue_path.stem().string());
    std::optional<std::size_t> named;
    for (std::size_t index : matches) {
        if (title_key(catalog[index].name) != cue_key &&
            title_key(catalog[index].description) != cue_key) {
            continue;
        }
        if (named.has_value())
            return std::nullopt;
        named = index;
    }
    return named;
}

std::uint32_t read_be32(const unsigned char* p) {
    return (std::uint32_t(p[0]) << 24) |
           (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) |
           std::uint32_t(p[3]);
}

std::string bytes_to_hex_lower(const unsigned char* data, std::size_t size) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.resize(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out[i * 2] = hex[data[i] >> 4];
        out[i * 2 + 1] = hex[data[i] & 0x0f];
    }
    return out;
}

std::optional<std::string> read_chd_combined_sha1(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;

    unsigned char header[124]{};
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    const std::streamsize got = in.gcount();
    if (got < 16)
        return std::nullopt;

    static constexpr unsigned char magic[8] = {
        'M', 'C', 'o', 'm', 'p', 'r', 'H', 'D'
    };
    if (!std::equal(std::begin(magic), std::end(magic), header))
        return std::nullopt;

    const std::uint32_t header_length = read_be32(header + 8);
    const std::uint32_t version = read_be32(header + 12);

    std::size_t sha1_offset = 0;
    std::size_t minimum_header = 0;
    if (version == 5) {
        sha1_offset = 84;
        minimum_header = 124;
    } else if (version == 4) {
        sha1_offset = 48;
        minimum_header = 108;
    } else {
        return std::nullopt;
    }

    if (header_length < minimum_header ||
        got < static_cast<std::streamsize>(sha1_offset + 20)) {
        return std::nullopt;
    }

    // A zero digest is not a meaningful CHD identity. Treat malformed or
    // placeholder headers conservatively instead of ever allowing a future
    // software-list entry to match an all-zero value by accident.
    bool any_nonzero = false;
    for (std::size_t i = 0; i < 20; ++i) {
        if (header[sha1_offset + i] != 0) {
            any_nonzero = true;
            break;
        }
    }
    if (!any_nonzero)
        return std::nullopt;

    return bytes_to_hex_lower(header + sha1_offset, 20);
}

} // namespace

std::size_t RedumpLayoutIndex::TrackLayoutHash::operator()(
    const TrackLayout& layout) const noexcept {
    std::size_t seed = layout.size();
    for (const std::uintmax_t size : layout) {
        const std::size_t value = std::hash<std::uintmax_t>{}(size);
        seed ^= value + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                (seed << 6U) + (seed >> 2U);
    }
    return seed;
}

RedumpLayoutIndex::RedumpLayoutIndex(const RedumpCatalog& catalog) {
    candidates_.reserve(catalog.size());
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const auto rows = redump_track_rows(catalog[index]);
        TrackLayout layout;
        layout.reserve(rows.size());
        for (const RedumpRomEntry* row : rows)
            layout.push_back(row->size);

        if (!layout.empty())
            candidates_[std::move(layout)].push_back(index);
    }
}

const std::vector<std::size_t>* RedumpLayoutIndex::candidates_for(
    const TrackLayout& layout) const {
    const auto found = candidates_.find(layout);
    return found == candidates_.end() ? nullptr : &found->second;
}

std::optional<RedumpCueVerification> verify_redump_cue(
    const fs::path& cue_path,
    const RedumpCatalog& catalog,
    const RedumpLayoutIndex& layout_index,
    Sha1Cache& sha1_cache,
    RedumpTitleKeyFunction title_key,
    const std::atomic<bool>* cancel) {
    const auto local_files = cue_track_files(cue_path);
    if (!local_files.has_value())
        return std::nullopt;

    // Byte sizes remain the cheap pre-filter. The catalog-wide work was done
    // once when the index was constructed; SHA-1 is calculated only when at
    // least one Redump entry has this exact ordered physical-track layout.
    RedumpLayoutIndex::TrackLayout layout;
    layout.reserve(local_files->size());
    for (const CueTrackFile& file : *local_files)
        layout.push_back(file.size);

    const std::vector<std::size_t>* candidates =
        layout_index.candidates_for(layout);
    if (candidates == nullptr)
        return std::nullopt;

    std::vector<std::string> local_sha1;
    local_sha1.reserve(local_files->size());
    for (const CueTrackFile& file : *local_files) {
        const auto digest = sha1_cache.file_sha1(file.path, file.size, cancel);
        if (!digest.has_value())
            return std::nullopt;
        local_sha1.push_back(*digest);
    }

    std::vector<std::size_t> matches;
    for (std::size_t index : *candidates) {
        const auto rows = redump_track_rows(catalog[index]);
        bool same_hashes = true;
        for (std::size_t j = 0; j < rows.size(); ++j) {
            if (!rows[j]->sha1.has_value() ||
                *rows[j]->sha1 != local_sha1[j]) {
                same_hashes = false;
                break;
            }
        }
        if (same_hashes)
            matches.push_back(index);
    }

    if (matches.empty())
        return std::nullopt;

    // Track identity and complete-set verification are separate. A modified
    // CUE may still describe byte-perfect Redump tracks, but only an exact CUE
    // size/SHA-1 match is allowed to receive complete Redump-set status.
    std::error_code ec;
    const std::uintmax_t cue_size = fs::file_size(cue_path, ec);
    std::optional<std::string> cue_sha1;
    if (!ec) {
        const bool size_candidate = std::any_of(
            matches.begin(), matches.end(), [&](std::size_t index) {
                const RedumpRomEntry* row = redump_cue_row(catalog[index]);
                return row != nullptr && row->sha1.has_value() &&
                       row->size == cue_size;
            });
        if (size_candidate)
            cue_sha1 = sha1_file_hex(cue_path, cancel);
    }

    if (cancel && cancel->load(std::memory_order_acquire))
        return std::nullopt;

    std::vector<std::size_t> complete_matches;
    if (cue_sha1.has_value()) {
        for (std::size_t index : matches) {
            const RedumpRomEntry* row = redump_cue_row(catalog[index]);
            if (row != nullptr && row->sha1.has_value() &&
                row->size == cue_size && *row->sha1 == *cue_sha1) {
                complete_matches.push_back(index);
            }
        }
    }

    if (!complete_matches.empty()) {
        const auto selected = select_catalog_match(
            cue_path, catalog, complete_matches, title_key);
        if (!selected.has_value())
            return std::nullopt;
        return RedumpCueVerification{*selected, RedumpCueMatch::CompleteSet};
    }

    // Identical track data can theoretically occur in more than one DAT row.
    // When the CUE itself did not resolve that ambiguity, use its title only
    // to select metadata. If that is still ambiguous, return no identity.
    const auto selected = select_catalog_match(
        cue_path, catalog, matches, title_key);
    if (!selected.has_value())
        return std::nullopt;
    return RedumpCueVerification{*selected, RedumpCueMatch::TracksOnly};
}

std::optional<std::string> verify_mame_chd(
    const fs::path& chd_path,
    const MameChdHashCatalog& catalog) {
    const auto chd_sha1 = read_chd_combined_sha1(chd_path);
    if (!chd_sha1.has_value())
        return std::nullopt;

    std::optional<std::string> matched_short;
    for (const auto& [short_name, disk_sha1] : catalog) {
        if (to_lower(disk_sha1) != *chd_sha1)
            continue;

        // A duplicated hash mapped to different software entries is ambiguous;
        // never award a verification badge in that case.
        if (matched_short.has_value() && *matched_short != short_name)
            return std::nullopt;
        matched_short = short_name;
    }
    return matched_short;
}

} // namespace goliath
