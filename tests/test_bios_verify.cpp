#include "catch2/catch.hpp"

#include "common/native_file.hpp"
#include "common/qt_path.hpp"
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

static const goliath::BiosArchiveVerification* find_archive(
    const goliath::BiosVerifyResult& result, const std::string& name) {
    const auto it = std::find_if(
        result.archives.begin(), result.archives.end(),
        [&name](const goliath::BiosArchiveVerification& archive) {
            return archive.zip_name == name;
        });
    return it == result.archives.end() ? nullptr : &*it;
}

static void clean_dir(const fs::path& dir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        fs::remove_all(entry.path(), ec);
    }
}

static void write_zip_with_file(const fs::path& zip_path, const std::string& member, const void* data, size_t size) {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    goliath::NativeFile output = goliath::open_native_file(
        zip_path, goliath::NativeFileMode::WriteBinary);
    if (output && mz_zip_writer_init_cfile(&zip, output.get(), 0)) {
        mz_zip_writer_add_mem(&zip, member.c_str(), data, size, 0);
        mz_zip_writer_finalize_archive(&zip);
        mz_zip_writer_end(&zip);
    }
}

TEST_CASE("bios_sets is populated", "[bios]") {
    REQUIRE(!bios_sets().empty());
    REQUIRE(!bios_sets()[0].zip_name.empty());

    const auto neocd = std::find_if(
        bios_sets().begin(), bios_sets().end(),
        [](const goliath::BiosSet& set) { return set.zip_name == "neocd.zip"; });
    REQUIRE(neocd != bios_sets().end());
    REQUIRE(neocd->files.size() == 2);
    CHECK(neocd->files[0].expected_crc == std::string("cac62307"));
    CHECK(neocd->files[1].expected_crc == std::string("c36a47c0"));

    const auto neocdz = std::find_if(
        bios_sets().begin(), bios_sets().end(),
        [](const goliath::BiosSet& set) { return set.zip_name == "neocdz.zip"; });
    REQUIRE(neocdz != bios_sets().end());
    REQUIRE(neocdz->files.size() == 3);
    CHECK(neocdz->files[0].expected_crc == std::string("df9de490"));
    CHECK(neocdz->files[2].member == "uni-bioscd33.rom");
    CHECK(neocdz->files[2].expected_crc == std::string("ff3abc59"));
    CHECK_FALSE(neocdz->files[2].required);
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
    CHECK(result->archives.size() == bios_sets().size());
    CHECK(result->problems == 2);
    CHECK(result->ready_archives == 0);
    CHECK(result->optional_missing_archives == 3);
    CHECK(result->checked_members == 0);

    const auto* neogeo = find_archive(*result, "neogeo.zip");
    REQUIRE(neogeo != nullptr);
    CHECK(neogeo->required);
    CHECK(neogeo->status == goliath::BiosArchiveStatus::MissingRequired);
    CHECK(neogeo->problems == 1);

    const auto* neocd = find_archive(*result, "neocd.zip");
    REQUIRE(neocd != nullptr);
    CHECK_FALSE(neocd->required);
    CHECK(neocd->status == goliath::BiosArchiveStatus::MissingOptional);
    CHECK(neocd->problems == 0);

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

    const auto* archive = find_archive(*result, "neogeo.zip");
    REQUIRE(archive != nullptr);
    CHECK(archive->status == goliath::BiosArchiveStatus::MemberProblems);
    CHECK(archive->problems ==
          static_cast<int>(bios_sets().front().files.size()));
    REQUIRE(archive->members.size() == bios_sets().front().files.size());
    CHECK(std::all_of(
        archive->members.begin(), archive->members.end(),
        [](const goliath::BiosMemberVerification& member) {
            return member.status == goliath::BiosMemberStatus::Missing &&
                   !member.actual_crc.has_value();
        }));

    clean_dir(dir);
    fs::remove(dir, ec);
}

TEST_CASE("verify_bios validates optional Neo Geo CD CRC values", "[bios]") {
    fs::path dir = fs::temp_directory_path() / "goliath_test_bios_optional";
    std::error_code ec;
    fs::create_directories(dir, ec);
    clean_dir(dir);

    const char data[] = "abc";
    write_zip_with_file(dir / "neocd.zip", "front-sp1.bin", data, sizeof(data));

    auto result = verify_bios(dir);
    REQUIRE(result.has_value());

    bool found_mismatch = false;
    for (const auto& line : result->lines) {
        if (line.find("front-sp1.bin") != std::string::npos &&
            line.find("BAD CRC") != std::string::npos) {
            found_mismatch = true;
            break;
        }
    }
    REQUIRE(found_mismatch);

    const auto* archive = find_archive(*result, "neocd.zip");
    REQUIRE(archive != nullptr);
    CHECK(archive->status == goliath::BiosArchiveStatus::MemberProblems);
    CHECK(archive->problems == 2);
    REQUIRE(archive->members.size() == 2);
    CHECK(archive->members[0].status ==
          goliath::BiosMemberStatus::CrcMismatch);
    CHECK(archive->members[0].expected_crc == std::string("cac62307"));
    CHECK(archive->members[0].actual_crc.has_value());
    CHECK(archive->members[1].status == goliath::BiosMemberStatus::Missing);
    CHECK(result->checked_members == 1);

    clean_dir(dir);
    fs::remove(dir, ec);
}

TEST_CASE("verify_bios reads archives below a Unicode directory",
          "[bios][paths][unicode]") {
    const fs::path dir = fs::temp_directory_path() /
        goliath::path_from_utf8("goliath_bios_Știință_日本");
    std::error_code ec;
    fs::remove_all(dir, ec);
    ec.clear();
    fs::create_directories(dir, ec);
    REQUIRE_FALSE(ec);

    const char data[] = "abc";
    write_zip_with_file(
        dir / "neocd.zip", "front-sp1.bin", data, sizeof(data));

    const auto result = verify_bios(dir);
    REQUIRE(result.has_value());

    const auto* archive = find_archive(*result, "neocd.zip");
    REQUIRE(archive != nullptr);
    REQUIRE_FALSE(archive->members.empty());
    CHECK(archive->members[0].member == "front-sp1.bin");
    CHECK(archive->members[0].actual_crc.has_value());

    fs::remove_all(dir, ec);
    CHECK_FALSE(ec);
}

TEST_CASE("missing CD Universe BIOS remains an optional capability",
          "[bios]") {
    const fs::path dir =
        fs::temp_directory_path() / "goliath_test_bios_cd_universe";
    std::error_code ec;
    fs::create_directories(dir, ec);
    clean_dir(dir);

    const char data[] = "cdz placeholder";
    write_zip_with_file(
        dir / "neocdz.zip", "neocd.bin", data, sizeof(data));

    const auto result = verify_bios(dir);
    REQUIRE(result.has_value());
    const auto* archive = find_archive(*result, "neocdz.zip");
    REQUIRE(archive != nullptr);
    REQUIRE(archive->members.size() == 3);
    CHECK(archive->problems == 2);
    CHECK(archive->optional_missing_members == 1);
    CHECK(result->optional_missing_members == 1);
    CHECK(archive->members[2].member == "uni-bioscd33.rom");
    CHECK_FALSE(archive->members[2].required);
    CHECK(archive->members[2].status ==
          goliath::BiosMemberStatus::MissingOptional);

    const bool reported_optional = std::any_of(
        result->lines.begin(), result->lines.end(),
        [](const std::string& line) {
            return line.find("uni-bioscd33.rom") != std::string::npos &&
                   line.find("optional component not found") !=
                       std::string::npos;
        });
    CHECK(reported_optional);

    fs::remove_all(dir, ec);
    CHECK_FALSE(ec);
}

TEST_CASE("verify_bios exposes an invalid archive status", "[bios]") {
    const fs::path dir =
        fs::temp_directory_path() / "goliath_test_bios_invalid_zip";
    std::error_code ec;
    fs::create_directories(dir, ec);
    clean_dir(dir);

    {
        std::ofstream output(dir / "aes.zip", std::ios::binary);
        output << "not a zip archive";
    }

    const auto result = verify_bios(dir);
    REQUIRE(result.has_value());
    const auto* archive = find_archive(*result, "aes.zip");
    REQUIRE(archive != nullptr);
    CHECK(archive->status == goliath::BiosArchiveStatus::InvalidArchive);
    CHECK(archive->problems == 1);
    CHECK(archive->members.empty());

    fs::remove_all(dir, ec);
    CHECK_FALSE(ec);
}
