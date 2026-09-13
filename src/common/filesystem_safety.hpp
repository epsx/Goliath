// filesystem_safety.hpp — direct-object inspection for Goliath-owned paths.
// A "direct" object is the final directory entry itself, not the target of a
// symbolic link, Windows junction, or other reparse point.
#pragma once

#include <filesystem>
#include <string>

namespace goliath {

enum class DirectPathKind {
    Missing,
    RegularFile,
    Directory,
    Other,
    Error,
};

struct DirectPathInspection {
    DirectPathKind kind = DirectPathKind::Error;
    std::string error;
};

DirectPathInspection inspect_direct_path(
    const std::filesystem::path& path);

// Ensure that the final path component is an ordinary directory. Missing
// parents may be created, but the resulting final object is reinspected so a
// symbolic link, junction, or other substituted object is never accepted.
bool ensure_direct_directory(const std::filesystem::path& path,
                             std::string* error = nullptr);

} // namespace goliath
