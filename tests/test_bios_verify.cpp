#include "catch2/catch.hpp"

#include "game/bios_verify.hpp"
#include "miniz.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using goliath::bios_sets;
using goliath::verify_bios;

static void clean_dir(const fs::path& dir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        fs::remove_all(entry.path(), ec);
    }
}

static void write_zip_with_file(const fs::path& zip_path, const std::string& member, const void* data, size_t size) {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (mz_zip_writer_init_file(&zip, zip_path.string().c_str(), 0)) {
        mz_zip_writer_add_mem(&zip, member.c_str(), data, size, 0);
        mz_zip_writer_finalize_archive(&zip);
        mz_zip_writer_end(&zip);
    }
}

TEST_CASE("bios_sets is populated", "[bios]") {
    REQUIRE(!bios_sets().empty());
    REQUIRE(!bios_sets()[0].zip_name.empty());
}

TEST_CASE("verify_bios returns nullopt when directory missing", "[bios]") {
    auto result = verify_bios(fs::temp_directory_path() / "goliath_nonexistent_bios_dir");
    REQUIRE(!result.has_value());
}

TEST_CASE("verify_bios reports missing required BIOS archives", "[bios]") {
    fs::path dir = fs::temp_directory_path() / "goliath_test_bios_empty";
    std::error_code ec;
    fs::create_directories(dir, ec);
    clean_dir(dir);

    auto result = verify_bios(dir);
    REQUIRE(result.has_value());
    REQUIRE(result->problems > 0);

    clean_dir(dir);
    fs::remove(dir, ec);
}

TEST_CASE("verify_bios reports missing file inside a required zip", "[bios]") {
    fs::path dir = fs::temp_directory_path() / "goliath_test_bios_bad";
    std::error_code ec;
    fs::create_directories(dir, ec);
    clean_dir(dir);

    // neogeo.zip exists but contains a wrong member -> every expected entry is MISSING
    const char data[] = "wrong content";
    write_zip_with_file(dir / "neogeo.zip", "not-a-real-rom.bin", data, sizeof(data));

    auto result = verify_bios(dir);
    REQUIRE(result.has_value());
    REQUIRE(result->problems > 0);

    bool found_missing = false;
    for (const auto& line : result->lines) {
        if (line.find("MISSING") != std::string::npos) {
            found_missing = true;
            break;
        }
    }
    REQUIRE(found_missing);

    clean_dir(dir);
    fs::remove(dir, ec);
}

TEST_CASE("verify_bios accepts optional neocd presence check", "[bios]") {
    fs::path dir = fs::temp_directory_path() / "goliath_test_bios_optional";
    std::error_code ec;
    fs::create_directories(dir, ec);
    clean_dir(dir);

    const char data[] = "abc";
    write_zip_with_file(dir / "neocd.zip", "front-sp1.bin", data, sizeof(data));

    auto result = verify_bios(dir);
    REQUIRE(result.has_value());

    bool found = false;
    for (const auto& line : result->lines) {
        if (line.find("front-sp1.bin") != std::string::npos && line.find(" found") != std::string::npos) {
            found = true;
            break;
        }
    }
    REQUIRE(found);

    clean_dir(dir);
    fs::remove(dir, ec);
}
