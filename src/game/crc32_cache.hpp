#pragma once

#include "hash_cache.hpp"

#include <utility>

namespace goliath {

class Crc32Cache {
public:
    static Crc32Cache load(const std::filesystem::path& path);
    static std::optional<std::string> calculate_file_crc32(
        const std::filesystem::path& file,
        const std::atomic<bool>* cancel = nullptr);

    std::optional<std::string> file_crc32(
        const std::filesystem::path& file,
        std::uintmax_t size,
        const std::atomic<bool>* cancel = nullptr);

    void prune_untouched() { m_cache.prune_untouched(); }
    bool save() { return m_cache.save(); }

    const std::filesystem::path& path() const { return m_cache.path(); }
    std::size_t hits() const { return m_cache.hits(); }
    std::size_t calculated() const { return m_cache.calculated(); }
    std::size_t entries() const { return m_cache.entries(); }
    std::size_t pruned() const { return m_cache.pruned(); }

private:
    explicit Crc32Cache(HashCache cache) : m_cache(std::move(cache)) {}
    HashCache m_cache;
};

} // namespace goliath
