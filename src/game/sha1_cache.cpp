#include "sha1_cache.hpp"

#include "common/sha1.hpp"

namespace fs = std::filesystem;

namespace goliath {

Sha1Cache Sha1Cache::load(const fs::path& path) {
    return Sha1Cache(HashCache::load(path, "sha1", "sha1", 40));
}

std::optional<std::string> Sha1Cache::file_sha1(
    const fs::path& file,
    std::uintmax_t size,
    const std::atomic<bool>* cancel) {
    return m_cache.file_hash(file, size, sha1_file_hex, cancel);
}

} // namespace goliath
