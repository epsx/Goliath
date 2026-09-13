#include "filesystem_io.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#ifdef _WIN32
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
namespace {

bool is_canceled(const std::atomic<bool>* cancel) {
    return cancel && cancel->load(std::memory_order_acquire);
}

#ifndef _WIN32
std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
#endif

#ifdef _WIN32
std::wstring windows_extended_absolute(const fs::path& path) {
    if (path.empty())
        return {};

    const std::wstring existing = path.wstring();
    if (existing.rfind(L"\\\\?\\", 0) == 0)
        return existing;

    std::error_code ec;
    fs::path absolute = path.is_absolute() ? path : fs::absolute(path, ec);
    if (ec)
        return {};
    absolute = absolute.lexically_normal();

    const std::wstring native = absolute.wstring();
    if (native.rfind(L"\\\\", 0) == 0)
        return std::wstring(L"\\\\?\\UNC\\") + native.substr(2);
    return std::wstring(L"\\\\?\\") + native;
}

CdImageDiscoveryResult discover_neocd_images_windows(
    const fs::path& logical_root,
    const std::atomic<bool>* cancel) {
    CdImageDiscoveryResult result;

    const std::wstring root = windows_extended_absolute(logical_root);
    if (root.empty())
        return result;

    const DWORD attributes = GetFileAttributesW(root.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return result;
    }
    result.root_available = true;

    struct PendingDirectory {
        std::wstring io_path;
        fs::path relative_path;
    };

    std::vector<PendingDirectory> pending;
    pending.push_back({root, fs::path{}});

    while (!pending.empty()) {
        if (is_canceled(cancel)) {
            result.canceled = true;
            return result;
        }

        PendingDirectory current = std::move(pending.back());
        pending.pop_back();

        std::wstring pattern = current.io_path;
        if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/')
            pattern.push_back(L'\\');
        pattern.push_back(L'*');

        WIN32_FIND_DATAW data{};
        HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
        if (handle == INVALID_HANDLE_VALUE) {
            // Same spirit as skip_permission_denied: a single unreadable
            // subtree must not abort the rest of the collection.
            continue;
        }

        do {
            if (is_canceled(cancel)) {
                result.canceled = true;
                FindClose(handle);
                return result;
            }

            const std::wstring name = data.cFileName;
            if (name == L"." || name == L"..")
                continue;

            const fs::path relative = current.relative_path / fs::path(name);

            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                // std::filesystem::recursive_directory_iterator does not follow
                // directory symlinks by default. Match that behavior and avoid
                // loops through junctions/reparse points.
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                    continue;

                std::wstring child = current.io_path;
                if (!child.empty() && child.back() != L'\\' && child.back() != L'/')
                    child.push_back(L'\\');
                child += name;
                pending.push_back({std::move(child), relative});
                continue;
            }

            std::wstring ext = fs::path(name).extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t ch) {
                if (ch >= L'A' && ch <= L'Z')
                    return static_cast<wchar_t>(ch - L'A' + L'a');
                return ch;
            });
            if (ext == L".cue" || ext == L".chd")
                result.files.push_back(relative);
        } while (FindNextFileW(handle, &data));

        FindClose(handle);
    }

    return result;
}
#endif

} // namespace

fs::path filesystem_io_path(const fs::path& path) {
#ifdef _WIN32
    if (path.empty())
        return path;

    const std::wstring existing = path.wstring();
    if (existing.rfind(L"\\\\?\\", 0) == 0)
        return path;

    std::error_code ec;
    fs::path absolute = path.is_absolute() ? path : fs::absolute(path, ec);
    if (ec)
        return path;
    absolute = absolute.lexically_normal();

    const std::wstring native = absolute.wstring();

    // Do not force ordinary short paths through the extended namespace.
    // MinGW/libstdc++ std::filesystem treats some \\?\ paths differently
    // from Win32 itself; keeping short paths untouched preserves the normal
    // scanner/test behavior. Only paths that actually need long-path I/O get
    // the prefix.
    if (native.size() < 248)
        return path;

    if (native.rfind(L"\\\\", 0) == 0)
        return fs::path(std::wstring(L"\\\\?\\UNC\\") + native.substr(2));
    return fs::path(std::wstring(L"\\\\?\\") + native);
#else
    return path;
#endif
}

CdImageDiscoveryResult discover_neocd_images(
    const fs::path& logical_root,
    const std::atomic<bool>* cancel) {
#ifdef _WIN32
    return discover_neocd_images_windows(logical_root, cancel);
#else
    CdImageDiscoveryResult result;
    std::error_code ec;
    if (!fs::is_directory(logical_root, ec))
        return result;

    result.root_available = true;
    fs::recursive_directory_iterator it(
        logical_root, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;

    while (it != end) {
        if (is_canceled(cancel)) {
            result.canceled = true;
            return result;
        }

        if (!ec) {
            std::error_code entry_ec;
            if (it->is_regular_file(entry_ec)) {
                const std::string ext = to_lower(it->path().extension().string());
                if (ext == ".cue" || ext == ".chd") {
                    fs::path rel = it->path().lexically_relative(logical_root);
                    if (!rel.empty())
                        result.files.push_back(rel);
                }
            }
        }

        ec.clear();
        it.increment(ec);
    }

    return result;
#endif
}

} // namespace goliath
