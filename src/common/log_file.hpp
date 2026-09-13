// log_file.hpp — narrow, testable direct-file operations for runtime logs.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

class QFile;

namespace goliath {

struct LogFileOperationResult {
    bool success = false;
    bool existed = false;
    std::string error;
};

using LogFileClearResult = LogFileOperationResult;

// Open the final directory entry itself without following a symbolic link,
// Windows junction/reparse point, directory, or other special object. On
// success, file is open read/write with append semantics when the target
// exists or createIfMissing created it. A missing target with
// createIfMissing=false succeeds while leaving file closed and existed=false.
LogFileOperationResult open_direct_regular_log_file(
    QFile& file,
    const std::filesystem::path& path,
    bool createIfMissing);

// Create an empty direct regular file when missing, or validate an existing
// direct regular file without changing its bytes.
LogFileOperationResult prepare_regular_log_file(
    const std::filesystem::path& path);

// Append all bytes to a safely opened direct regular file, creating it when
// missing. Directories, links/reparse points, and special objects are refused.
LogFileOperationResult append_regular_log_file(
    const std::filesystem::path& path,
    std::string_view bytes);

// Truncate a direct regular file. A missing file is already clear and succeeds;
// directories, links/reparse points, and special filesystem objects are refused.
LogFileClearResult clear_regular_log_file(
    const std::filesystem::path& path);

} // namespace goliath
