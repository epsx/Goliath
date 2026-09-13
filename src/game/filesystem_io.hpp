#pragma once

#include <atomic>
#include <filesystem>
#include <vector>

namespace goliath {

struct CdImageDiscoveryResult {
    bool root_available = false;
    bool canceled = false;
    std::vector<std::filesystem::path> files;
};

// Return a path suitable for filesystem I/O. On Windows this opts only paths
// that need it into the extended-length namespace (\\?\ / \\?\UNC\), while
// preserving the logical/configured spelling everywhere else.
std::filesystem::path filesystem_io_path(const std::filesystem::path& path);

// Recursively discover launchable Neo Geo CD image descriptors below the
// configured logical root. Returned paths are relative logical paths so JSON,
// cache keys and UI strings never leak Windows extended-length prefixes.
CdImageDiscoveryResult discover_neocd_images(
    const std::filesystem::path& logical_root,
    const std::atomic<bool>* cancel = nullptr);

} // namespace goliath
