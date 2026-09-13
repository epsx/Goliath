#include "common/filesystem_safety.hpp"

#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace goliath {

DirectPathInspection inspect_direct_path(const fs::path& path) {
    if (path.empty())
        return {DirectPathKind::Error, "The filesystem path is empty."};

#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.wstring().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
            return {DirectPathKind::Missing, {}};
        return {
            DirectPathKind::Error,
            std::error_code(static_cast<int>(code),
                            std::system_category()).message(),
        };
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        return {DirectPathKind::Other, {}};
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        return {DirectPathKind::Directory, {}};

    std::error_code ec;
    if (fs::is_regular_file(path, ec))
        return {DirectPathKind::RegularFile, {}};
    if (ec)
        return {DirectPathKind::Error, ec.message()};
    return {DirectPathKind::Other, {}};
#else
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory ||
        (!ec && !fs::exists(status))) {
        return {DirectPathKind::Missing, {}};
    }
    if (ec)
        return {DirectPathKind::Error, ec.message()};
    if (fs::is_regular_file(status))
        return {DirectPathKind::RegularFile, {}};
    if (fs::is_directory(status))
        return {DirectPathKind::Directory, {}};
    return {DirectPathKind::Other, {}};
#endif
}

bool ensure_direct_directory(const fs::path& path, std::string* error) {
    if (error) error->clear();

    DirectPathInspection inspection = inspect_direct_path(path);
    if (inspection.kind == DirectPathKind::Directory)
        return true;
    if (inspection.kind == DirectPathKind::Error) {
        if (error) {
            *error = "Could not inspect directory " + path.string() +
                     ": " + inspection.error;
        }
        return false;
    }
    if (inspection.kind != DirectPathKind::Missing) {
        if (error) {
            *error = "Path is not a direct directory: " + path.string();
        }
        return false;
    }

    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec) {
        if (error) {
            *error = "Could not create directory " + path.string() +
                     ": " + ec.message();
        }
        return false;
    }

    inspection = inspect_direct_path(path);
    if (inspection.kind == DirectPathKind::Directory)
        return true;
    if (error) {
        *error = "Created path is not a direct directory: " + path.string();
        if (inspection.kind == DirectPathKind::Error &&
            !inspection.error.empty()) {
            *error += ": " + inspection.error;
        }
    }
    return false;
}

} // namespace goliath
