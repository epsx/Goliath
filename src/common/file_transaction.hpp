#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace goliath {

struct FileTransactionRequest {
    std::filesystem::path path;
    std::string contents;
};

// Stages every requested file before replacing any destination. If a later
// commit fails, files already replaced during this call are restored to their
// original contents (or removed when they did not exist before the call).
bool save_files_transactionally(
    const std::vector<FileTransactionRequest>& requests,
    std::string* error = nullptr);

} // namespace goliath
