#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>

namespace goliath {

// Persistent SHA-1 cache used by Neo Geo CD Redump BIN-track verification.
// A cache hit is valid only when logical absolute path, byte size, and file
// modification time all match the stored fingerprint.
class Sha1Cache {
public:
    static Sha1Cache load(const std::filesystem::path& path);

    std::optional<std::string> file_sha1(
        const std::filesystem::path& file,
        std::uintmax_t size,
        const std::atomic<bool>* cancel = nullptr);

    // Remove rows that were not touched during the current complete scan.
    // Callers decide when pruning is safe (for example, not while a configured
    // removable Neo Geo CD root is offline).
    void prune_untouched();

    // Persist changes. A clean cache is a successful no-op.
    bool save();

    const std::filesystem::path& path() const { return m_path; }
    std::size_t hits() const { return m_hits; }
    std::size_t calculated() const { return m_calculated; }
    std::size_t entries() const { return m_entries.size(); }
    std::size_t pruned() const { return m_pruned; }

private:
    struct Entry {
        std::uintmax_t size = 0;
        std::int64_t mtime_ticks = 0;
        std::string sha1;
    };

    explicit Sha1Cache(std::filesystem::path path);

    std::filesystem::path m_path;
    std::map<std::string, Entry> m_entries;
    std::unordered_set<std::string> m_touched_keys;
    bool m_dirty = false;
    std::size_t m_hits = 0;
    std::size_t m_calculated = 0;
    std::size_t m_pruned = 0;
};

} // namespace goliath
