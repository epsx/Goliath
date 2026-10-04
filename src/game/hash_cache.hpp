#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>

namespace goliath {

// Shared persistent cache for file digests. Cache identity is the logical
// absolute path plus byte size and modification time; the digest algorithm,
// JSON field, and expected hexadecimal length are supplied by a thin wrapper.
class HashCache {
public:
    using Calculator = std::function<std::optional<std::string>(
        const std::filesystem::path&, const std::atomic<bool>*)>;

    static HashCache load(const std::filesystem::path& path,
                          std::string algorithm,
                          std::string digest_field,
                          std::size_t digest_hex_length);

    std::optional<std::string> file_hash(
        const std::filesystem::path& file,
        std::uintmax_t size,
        const Calculator& calculator,
        const std::atomic<bool>* cancel = nullptr);

    void prune_untouched();
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
        std::string digest;
    };

    HashCache(std::filesystem::path path,
              std::string algorithm,
              std::string digest_field,
              std::size_t digest_hex_length);

    bool valid_digest(const std::string& value) const;

    std::filesystem::path m_path;
    std::string m_algorithm;
    std::string m_digest_field;
    std::size_t m_digest_hex_length = 0;
    std::map<std::string, Entry> m_entries;
    std::unordered_set<std::string> m_touched_keys;
    bool m_dirty = false;
    std::size_t m_hits = 0;
    std::size_t m_calculated = 0;
    std::size_t m_pruned = 0;
};

} // namespace goliath
