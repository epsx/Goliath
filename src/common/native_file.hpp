// native_file.hpp — Unicode-safe stdio handles for C library boundaries.
#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>

namespace goliath {

enum class NativeFileMode {
    ReadBinary,
    WriteBinary,
};

struct NativeFileCloser {
    void operator()(std::FILE* file) const noexcept {
        if (file) std::fclose(file);
    }
};

using NativeFile = std::unique_ptr<std::FILE, NativeFileCloser>;

inline NativeFile open_native_file(
    const std::filesystem::path& path,
    NativeFileMode mode = NativeFileMode::ReadBinary) {
#if defined(_WIN32)
    const wchar_t* native_mode = mode == NativeFileMode::ReadBinary
        ? L"rb"
        : L"wb";
    return NativeFile(::_wfopen(path.c_str(), native_mode));
#else
    const char* native_mode = mode == NativeFileMode::ReadBinary
        ? "rb"
        : "wb";
    return NativeFile(std::fopen(path.c_str(), native_mode));
#endif
}

} // namespace goliath
