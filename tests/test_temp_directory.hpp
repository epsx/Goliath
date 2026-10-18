// test_temp_directory.hpp — owns one unique test directory and removes it
// automatically when the test scope ends, including after failed assertions.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace goliath::test {

class ScopedTestDirectory {
public:
    explicit ScopedTestDirectory(std::string_view prefix) {
        static std::atomic<std::uint64_t> sequence{0};
        const auto stamp = std::chrono::high_resolution_clock::now()
                               .time_since_epoch()
                               .count();
        m_path = std::filesystem::temp_directory_path() /
            (std::string(prefix) + "_" + std::to_string(stamp) + "_" +
             std::to_string(sequence.fetch_add(1)));

        std::error_code ec;
        if (!std::filesystem::create_directories(m_path, ec) || ec) {
            throw std::runtime_error(
                "Could not create test directory: " + m_path.string());
        }
    }

    ~ScopedTestDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    ScopedTestDirectory(const ScopedTestDirectory&) = delete;
    ScopedTestDirectory& operator=(const ScopedTestDirectory&) = delete;
    ScopedTestDirectory(ScopedTestDirectory&&) = delete;
    ScopedTestDirectory& operator=(ScopedTestDirectory&&) = delete;

    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

} // namespace goliath::test
