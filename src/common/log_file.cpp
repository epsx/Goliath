#include "common/log_file.hpp"

#include "common/filesystem_safety.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QIODevice>
#include <QString>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace goliath {

namespace {

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::string path_error(const char* action,
                       const fs::path& path,
                       const std::string& detail = {}) {
    std::string message = action;
    message += ": ";
    message += path_to_qstring(path).toUtf8().toStdString();
    if (!detail.empty()) {
        message += ": ";
        message += detail;
    }
    return message;
}

bool open_native_direct_file(QFile& file,
                             const fs::path& path,
                             bool create,
                             std::string* error) {
#if defined(_WIN32)
    const DWORD disposition = create ? CREATE_NEW : OPEN_EXISTING;
    HANDLE handle = CreateFileW(
        path.wstring().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        disposition,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const std::error_code ec(
            static_cast<int>(GetLastError()), std::system_category());
        if (error) {
            *error = path_error(
                "Could not open the direct log file", path, ec.message());
        }
        return false;
    }

    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(handle, &information)) {
        const std::error_code ec(
            static_cast<int>(GetLastError()), std::system_category());
        CloseHandle(handle);
        if (error) {
            *error = path_error(
                "Could not verify the opened log file", path, ec.message());
        }
        return false;
    }
    if ((information.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        CloseHandle(handle);
        if (error) {
            *error = path_error(
                "The log path is not a direct regular file", path);
        }
        return false;
    }

    const int descriptor = _open_osfhandle(
        reinterpret_cast<std::intptr_t>(handle),
        _O_RDWR | _O_BINARY | _O_APPEND);
    if (descriptor == -1) {
        const std::error_code ec(errno, std::generic_category());
        CloseHandle(handle);
        if (error) {
            *error = path_error(
                "Could not adopt the direct log handle", path, ec.message());
        }
        return false;
    }
#else
    int flags = O_RDWR | O_APPEND;
#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
    flags |= O_NOFOLLOW;
#endif
    if (create) flags |= O_CREAT | O_EXCL;

    const int descriptor = ::open(path.c_str(), flags, 0666);
    if (descriptor == -1) {
        const std::error_code ec(errno, std::generic_category());
        if (error) {
            *error = path_error(
                "Could not open the direct log file", path, ec.message());
        }
        return false;
    }

    struct stat information{};
    if (::fstat(descriptor, &information) != 0) {
        const std::error_code ec(errno, std::generic_category());
        ::close(descriptor);
        if (error) {
            *error = path_error(
                "Could not verify the opened log file", path, ec.message());
        }
        return false;
    }
    if (!S_ISREG(information.st_mode)) {
        ::close(descriptor);
        if (error) {
            *error = path_error(
                "The opened log path is not a direct regular file", path);
        }
        return false;
    }
#endif

    file.setFileName(path_to_qstring(path));
    if (!file.open(
            descriptor,
            QIODevice::ReadWrite | QIODevice::Append,
            QFileDevice::AutoCloseHandle)) {
        const std::string detail = file.errorString().toStdString();
#if defined(_WIN32)
        _close(descriptor);
#else
        ::close(descriptor);
#endif
        if (error) {
            *error = path_error(
                "Could not attach the direct log file", path, detail);
        }
        return false;
    }
    return true;
}

} // namespace

LogFileOperationResult open_direct_regular_log_file(
        QFile& file,
        const fs::path& path,
        bool createIfMissing) {
    LogFileOperationResult result;
    if (file.isOpen()) file.close();
    if (path.empty()) {
        result.error = "The log path is empty.";
        return result;
    }

    const DirectPathInspection inspection = inspect_direct_path(path);
    if (inspection.kind == DirectPathKind::Error) {
        result.error = path_error(
            "Could not inspect the log path", path, inspection.error);
        return result;
    }
    result.existed = inspection.kind != DirectPathKind::Missing;
    if (inspection.kind == DirectPathKind::Missing && !createIfMissing) {
        result.success = true;
        return result;
    }
    if (inspection.kind != DirectPathKind::Missing &&
        inspection.kind != DirectPathKind::RegularFile) {
        result.error = path_error(
            "The log path is not a direct regular file", path);
        return result;
    }

    if (!open_native_direct_file(
            file, path, inspection.kind == DirectPathKind::Missing,
            &result.error)) {
        return result;
    }

    const DirectPathInspection opened = inspect_direct_path(path);
    if (opened.kind != DirectPathKind::RegularFile) {
        file.close();
        result.error = path_error(
            "The opened log path is not a direct regular file", path,
            opened.kind == DirectPathKind::Error ? opened.error : std::string{});
        return result;
    }

    result.success = true;
    return result;
}

LogFileOperationResult prepare_regular_log_file(const fs::path& path) {
    QFile file;
    LogFileOperationResult result =
        open_direct_regular_log_file(file, path, true);
    if (file.isOpen()) file.close();
    return result;
}

LogFileOperationResult append_regular_log_file(
        const fs::path& path,
        std::string_view bytes) {
    QFile file;
    LogFileOperationResult result =
        open_direct_regular_log_file(file, path, true);
    if (!result.success) return result;

    if (bytes.size() >
        static_cast<std::size_t>(std::numeric_limits<qint64>::max())) {
        result.success = false;
        result.error = "The log append payload is too large.";
        return result;
    }

    qint64 written = 0;
    const qint64 total = static_cast<qint64>(bytes.size());
    while (written < total) {
        const qint64 count =
            file.write(bytes.data() + written, total - written);
        if (count <= 0) {
            result.success = false;
            result.error = path_error(
                "Could not append to the log file", path,
                file.errorString().toStdString());
            return result;
        }
        written += count;
    }
    if (!file.flush()) {
        result.success = false;
        result.error = path_error(
            "Could not flush the log file", path,
            file.errorString().toStdString());
        return result;
    }
    return result;
}

LogFileClearResult clear_regular_log_file(const fs::path& path) {
    QFile file;
    LogFileClearResult result =
        open_direct_regular_log_file(file, path, false);
    if (!result.success || !result.existed) return result;

    // Resize only after a direct handle has been opened and verified.
    if (!file.resize(0) || !file.flush()) {
        result.success = false;
        result.error = path_error(
            "Could not truncate the log file", path,
            file.errorString().toStdString());
        return result;
    }
    return result;
}

} // namespace goliath
