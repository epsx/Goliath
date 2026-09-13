#pragma once

#include "neogeo_metadata.hpp"
#include "sha1_cache.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace goliath {

using RedumpTitleKeyFunction = std::string (*)(const std::string&);

enum class RedumpCueMatch {
    CompleteSet,
    TracksOnly
};

struct RedumpCueVerification {
    std::size_t catalog_index = 0;
    RedumpCueMatch match = RedumpCueMatch::TracksOnly;
};

// Groups Redump catalog rows by their ordered physical-track sizes. Building
// this once per scan avoids walking the complete DAT for every local CUE while
// retaining catalog order for duplicate layouts and ambiguity handling.
class RedumpLayoutIndex {
public:
    using TrackLayout = std::vector<std::uintmax_t>;

    explicit RedumpLayoutIndex(const RedumpCatalog& catalog);

    const std::vector<std::size_t>* candidates_for(
        const TrackLayout& layout) const;

private:
    struct TrackLayoutHash {
        std::size_t operator()(const TrackLayout& layout) const noexcept;
    };

    std::unordered_map<TrackLayout, std::vector<std::size_t>, TrackLayoutHash>
        candidates_;
};

std::optional<RedumpCueVerification> verify_redump_cue(
    const std::filesystem::path& cue_path,
    const RedumpCatalog& catalog,
    const RedumpLayoutIndex& layout_index,
    Sha1Cache& sha1_cache,
    RedumpTitleKeyFunction title_key,
    const std::atomic<bool>* cancel = nullptr);

std::optional<std::string> verify_mame_chd(
    const std::filesystem::path& chd_path,
    const MameChdHashCatalog& catalog);

} // namespace goliath
