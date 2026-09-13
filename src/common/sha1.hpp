#pragma once

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace goliath {

// Lower-case hexadecimal SHA-1 helpers used by media verification.
std::string sha1_hex(std::string_view data);
std::optional<std::string> sha1_file_hex(
    const std::filesystem::path& path,
    const std::atomic<bool>* cancel = nullptr);

} // namespace goliath
