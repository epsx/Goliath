#include "crc32_cache.hpp"

#include "miniz.h"

#include <array>
#include <cstdio>
#include <fstream>

namespace fs = std::filesystem;

namespace goliath {

std::optional<std::string> Crc32Cache::calculate_file_crc32(
    const fs::path& path,
    const std::atomic<bool>* cancel) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::nullopt;

    mz_ulong crc = MZ_CRC32_INIT;
    std::array<char, 1024 * 1024> buffer{};
    while (input) {
        if (cancel && cancel->load(std::memory_order_acquire))
            return std::nullopt;
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            crc = mz_crc32(
                crc, reinterpret_cast<const mz_uint8*>(buffer.data()),
                static_cast<std::size_t>(count));
        }
    }
    if (input.bad())
        return std::nullopt;

    char value[9]{};
    std::snprintf(value, sizeof(value), "%08x",
                  static_cast<unsigned int>(crc));
    return std::string(value);
}

Crc32Cache Crc32Cache::load(const fs::path& path) {
    return Crc32Cache(HashCache::load(path, "crc32", "crc32", 8));
}

std::optional<std::string> Crc32Cache::file_crc32(
    const fs::path& file,
    std::uintmax_t size,
    const std::atomic<bool>* cancel) {
    return m_cache.file_hash(file, size, calculate_file_crc32, cancel);
}

} // namespace goliath
