#include "game/jollygood_bios.hpp"

#include "common/debug_logger.hpp"
#include "common/paths.hpp"
#include "game/jollygood_executable.hpp"

#include <QProcess>

#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace goliath {
namespace {

enum class BiosDestinationKind {
    Missing,
    Link,
    Directory,
    Other,
    Error,
};

struct BiosDestinationInspection {
    BiosDestinationKind kind = BiosDestinationKind::Error;
    std::string error;
};

BiosDestinationInspection inspectBiosDestination(const fs::path& path) {
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.wstring().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
            return {BiosDestinationKind::Missing, {}};
        return {
            BiosDestinationKind::Error,
            std::error_code(static_cast<int>(code), std::system_category()).message(),
        };
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        return {BiosDestinationKind::Link, {}};
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        return {BiosDestinationKind::Directory, {}};
    return {BiosDestinationKind::Other, {}};
#else
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory)
        return {BiosDestinationKind::Missing, {}};
    if (ec)
        return {BiosDestinationKind::Error, ec.message()};
    if (fs::is_symlink(status))
        return {BiosDestinationKind::Link, {}};
    if (fs::is_directory(status))
        return {BiosDestinationKind::Directory, {}};
    return {BiosDestinationKind::Other, {}};
#endif
}

bool equivalentExistingPaths(const fs::path& left, const fs::path& right) {
    std::error_code ec;
    const bool equivalent = fs::equivalent(left, right, ec);
    return !ec && equivalent;
}

// Returns the target a Windows junction / directory symlink points to.
// For Linux it reads the regular symlink. Returns std::nullopt on failure,
// which means "we cannot confirm it is correct, so recreate it".
std::optional<fs::path> readLinkTarget(const fs::path& p) {
    std::error_code ec;
    if (fs::is_symlink(p, ec)) {
        fs::path target = fs::read_symlink(p, ec);
        if (!ec) return target;
    }
#if defined(_WIN32)
    DWORD attr = GetFileAttributesW(p.wstring().c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
        HANDLE h = CreateFileW(p.wstring().c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            // Extended Windows paths can be much longer than MAX_PATH. Keep a
            // full Win32-sized buffer so a valid long junction target is not
            // mistaken for an unreadable link and needlessly retargeted.
            std::vector<WCHAR> buffer(32768);
            DWORD len = GetFinalPathNameByHandleW(
                h, buffer.data(), static_cast<DWORD>(buffer.size()),
                FILE_NAME_OPENED);
            CloseHandle(h);
            if (len != 0 && len < buffer.size()) {
                std::wstring s(buffer.data(), len);
                const std::wstring prefix = L"\\\\?\\";
                const std::wstring uncPrefix = L"\\\\?\\UNC\\";
                if (s.starts_with(uncPrefix)) {
                    s = L"\\\\" + s.substr(uncPrefix.size());
                } else if (s.starts_with(prefix)) {
                    s = s.substr(prefix.size());
                }
                return fs::path(s);
            }
        }
    }
#else
    (void)p;
#endif
    return std::nullopt;
}

fs::path resolvedLinkTarget(const fs::path& link,
                            const fs::path& target) {
    return target.is_absolute() ? target : link.parent_path() / target;
}

// Determine the directory jgrf will use as "binpath" for the configured
// jollygood executable. On Windows JGRF_STATIC builds the BIOS path becomes
// <binpath>/bios, so the junction/symlink must be created here.
fs::path jollygoodBinDir(const AppPaths& paths) {
    const fs::path executable = resolve_jollygood_executable(paths.jollygood_exe);
    return executable.empty() ? paths.base_dir : executable.parent_path();
}

bool hasDynamicCoreArgument(const std::vector<std::string>& args) {
    for (const std::string& arg : args) {
        if (arg == "-c" || arg == "--core" ||
            (arg.size() > 2 && arg.starts_with("-c")) ||
            arg.starts_with("--core=")) {
            return true;
        }
    }
    return false;
}

// Remove only a link/reparse point. A plain directory or file is never owned
// merely because it occupies the path JGRF calls "bios".
bool removeBiosLinkOnly(const fs::path& path, std::string* error) {
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.wstring().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        if (error) *error = "BIOS destination is no longer a link.";
        return false;
    }

    const BOOL removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
        ? RemoveDirectoryW(path.wstring().c_str())
        : DeleteFileW(path.wstring().c_str());
    if (!removed) {
        if (error) {
            *error = std::error_code(
                static_cast<int>(GetLastError()),
                std::system_category()).message();
        }
        return false;
    }
    return true;
#else
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec || !fs::is_symlink(status)) {
        if (error) {
            *error = ec ? ec.message()
                        : "BIOS destination is no longer a symbolic link.";
        }
        return false;
    }
    if (!fs::remove(path, ec) || ec) {
        if (error) *error = ec.message();
        return false;
    }
    return true;
#endif
}

} // namespace

void ensure_jollygood_bios(const AppPaths& paths) {
    fs::path biosDir = paths.bios_dir;
    fs::path jollygoodDir = paths.data_dir / "jollygood";
    // A -c/--core argument selects JGRF's dynamic-core layout, which resolves
    // BIOS files below XDG_DATA_HOME/jollygood. Static-core builds use the
    // executable directory instead. This distinction applies on every OS.
    const fs::path jollygoodBios = hasDynamicCoreArgument(paths.jollygood_args)
        ? jollygoodDir / "bios"
        : jollygoodBinDir(paths) / "bios";
    fs::path marker = jollygoodDir / "bios_source.txt";

    std::error_code ec;

    // One-time migration: older versions used <base_dir> directly as
    // XDG_DATA_HOME, so the emulator's data dir was <base_dir>/jollygood.
    // That breaks on Linux, where the jgrf executable itself is named
    // "jollygood" (a folder and a file cannot share a name). Move any old
    // data dir under <base_dir>/data/.
    fs::path legacyDir = paths.base_dir / "jollygood";
    if (fs::is_directory(legacyDir, ec) && !fs::exists(jollygoodDir, ec)) {
        fs::create_directories(paths.data_dir, ec);
        fs::rename(legacyDir, jollygoodDir, ec);
        if (ec) DebugLogger::logError(QString("could not migrate legacy jollygood data dir: %1")
                                          .arg(QString::fromStdString(ec.message())));
    }

    if (!fs::is_directory(biosDir, ec)) {
        if (ec) {
            DebugLogger::logError(
                QString("could not inspect configured BIOS directory %1: %2")
                    .arg(QString::fromStdString(biosDir.string()))
                    .arg(QString::fromStdString(ec.message())));
        }
        return;
    }

    // No need for a junction if the configured BIOS folder is the exact
    // path jgrf will already inspect (common when users leave [Paths] bios
    // as the default "bios" and jollygood is in the same directory).
    if (equivalentExistingPaths(biosDir, jollygoodBios)) {
        const fs::path canonicalBiosDir = fs::weakly_canonical(biosDir, ec);
        std::ofstream out(marker, std::ios::trunc);
        out << canonicalBiosDir.string();
        return;
    }

    fs::create_directories(jollygoodDir, ec);
    if (ec) {
        DebugLogger::logError(
            QString("could not create JGRF data directory %1: %2")
                .arg(QString::fromStdString(jollygoodDir.string()))
                .arg(QString::fromStdString(ec.message())));
        return;
    }

    // If the configured BIOS folder changed since the last launch, retarget
    // only a confirmed link. A plain folder is not owned by Goliath merely
    // because it occupies the path JGRF calls "bios".
    // Reading the actual link target is more robust than trusting a marker,
    // especially when the marker was updated but the link was not.
    const fs::path canonicalBiosDir = fs::weakly_canonical(biosDir, ec);
    if (ec) {
        DebugLogger::logError(
            QString("could not resolve configured BIOS directory %1: %2")
                .arg(QString::fromStdString(biosDir.string()))
                .arg(QString::fromStdString(ec.message())));
        return;
    }
    const std::string current = canonicalBiosDir.string();

    BiosDestinationInspection destination =
        inspectBiosDestination(jollygoodBios);
    if (destination.kind == BiosDestinationKind::Error) {
        DebugLogger::logError(
            QString("could not inspect JGRF BIOS destination %1: %2")
                .arg(QString::fromStdString(jollygoodBios.string()))
                .arg(QString::fromStdString(destination.error)));
        return;
    }
    if (destination.kind == BiosDestinationKind::Other) {
        DebugLogger::logError(
            QString("refusing to replace non-directory JGRF BIOS destination: %1")
                .arg(QString::fromStdString(jollygoodBios.string())));
        return;
    }

    if (destination.kind == BiosDestinationKind::Link) {
        bool correctTarget = false;
        if (const auto target = readLinkTarget(jollygoodBios)) {
            correctTarget = equivalentExistingPaths(
                biosDir, resolvedLinkTarget(jollygoodBios, *target));
        }
        if (correctTarget) {
            std::ofstream out(marker, std::ios::trunc);
            out << current;
            return;
        }

        std::string removeError;
        if (!removeBiosLinkOnly(jollygoodBios, &removeError)) {
            DebugLogger::logError(
                QString("could not remove stale JGRF BIOS link %1: %2")
                    .arg(QString::fromStdString(jollygoodBios.string()))
                    .arg(QString::fromStdString(removeError)));
            return;
        }
        destination.kind = BiosDestinationKind::Missing;
    }

    if (destination.kind == BiosDestinationKind::Missing) {
#if defined(_WIN32)
        {
            QProcess proc;
            proc.setWorkingDirectory(QString::fromStdString(paths.base_dir.string()));
            proc.start("cmd", {"/c", "mklink", "/J", QString::fromStdString(jollygoodBios.string()),
                                QString::fromStdString(biosDir.string())});
            bool ok = proc.waitForStarted(5000) && proc.waitForFinished(5000) && proc.exitCode() == 0;
            if (ok) {
                std::ofstream out(marker, std::ios::trunc);
                out << current;
                return;
            }
            DebugLogger::logError(QString("mklink /J failed for %1 -> %2")
                                      .arg(QString::fromStdString(jollygoodBios.string()))
                                      .arg(QString::fromStdString(biosDir.string())));
        }
#else
        std::error_code linkEc;
        fs::create_directory_symlink(biosDir, jollygoodBios, linkEc);
        if (!linkEc) {
            std::ofstream out(marker, std::ios::trunc);
            out << current;
            return;
        }
        DebugLogger::logError(QString("symlink failed for %1 -> %2: %3")
                                  .arg(QString::fromStdString(jollygoodBios.string()))
                                  .arg(QString::fromStdString(biosDir.string()))
                                  .arg(QString::fromStdString(linkEc.message())));
#endif
        fs::create_directories(jollygoodBios, ec);
        if (ec) {
            DebugLogger::logError(
                QString("could not create JGRF BIOS fallback directory %1: %2")
                    .arg(QString::fromStdString(jollygoodBios.string()))
                    .arg(QString::fromStdString(ec.message())));
            return;
        }
    }

    // Update marker after verifying jollygoodBios is in place (link, copy, or
    // existing directory).
    if (!fs::is_directory(jollygoodBios, ec) || ec) {
        DebugLogger::logError(
            QString("failed to prepare jollygood BIOS path %1%2")
                .arg(QString::fromStdString(jollygoodBios.string()))
                .arg(ec ? QString(": %1").arg(
                              QString::fromStdString(ec.message()))
                        : QString()));
        return;
    }

    bool copySucceeded = true;
    fs::directory_iterator iterator(biosDir, ec);
    const fs::directory_iterator end;
    while (!ec && iterator != end) {
        const fs::directory_entry entry = *iterator;
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc)) {
            if (entryEc) {
                copySucceeded = false;
                DebugLogger::logError(
                    QString("could not inspect configured BIOS entry %1: %2")
                        .arg(QString::fromStdString(entry.path().string()))
                        .arg(QString::fromStdString(entryEc.message())));
            }
            iterator.increment(ec);
            continue;
        }
        fs::path src = entry.path();
        fs::path dst = jollygoodBios / src.filename();

        bool shouldCopy = true;
        std::error_code statusEc;
        const fs::file_status destinationStatus =
            fs::symlink_status(dst, statusEc);
        if (statusEc == std::errc::no_such_file_or_directory) {
            statusEc.clear();
        } else if (statusEc ||
                   (fs::exists(destinationStatus) &&
                    !fs::is_regular_file(destinationStatus))) {
            copySucceeded = false;
            DebugLogger::logError(
                QString("refusing to replace non-regular BIOS fallback file: %1")
                    .arg(QString::fromStdString(dst.string())));
            iterator.increment(ec);
            continue;
        } else if (fs::is_regular_file(destinationStatus)) {
            std::error_code sourceTimeEc;
            std::error_code destinationTimeEc;
            const auto srcTime = fs::last_write_time(src, sourceTimeEc);
            const auto dstTime = fs::last_write_time(dst, destinationTimeEc);
            shouldCopy = sourceTimeEc || destinationTimeEc || srcTime > dstTime;
        }
        if (shouldCopy) {
            std::error_code copyEc;
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, copyEc);
            if (copyEc) {
                copySucceeded = false;
                DebugLogger::logError(
                    QString("could not copy BIOS fallback file %1: %2")
                        .arg(QString::fromStdString(dst.string()))
                        .arg(QString::fromStdString(copyEc.message())));
            }
        }
        iterator.increment(ec);
    }
    if (ec) {
        copySucceeded = false;
        DebugLogger::logError(
            QString("could not enumerate configured BIOS directory %1: %2")
                .arg(QString::fromStdString(biosDir.string()))
                .arg(QString::fromStdString(ec.message())));
    }

    if (copySucceeded) {
        std::ofstream out(marker, std::ios::trunc);
        out << current;
    }
}

} // namespace goliath
