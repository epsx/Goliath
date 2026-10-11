#include "catch2/catch.hpp"

#include "common/sha1.hpp"
#include "game/game_integrity.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path make_integrity_root(const std::string& name) {
    const fs::path root =
        fs::temp_directory_path() / ("goliath_integrity_" + name);
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "media/disc", ec);
    fs::create_directories(root / "cache", ec);
    return root;
}

void write_file(const fs::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.good());
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    REQUIRE(output.good());
}

goliath::MediaHashDetail reference(
        std::string file,
        std::string role,
        std::string algorithm,
        std::uintmax_t size,
        std::string digest) {
    goliath::MediaHashDetail hash;
    hash.file = std::move(file);
    hash.role = std::move(role);
    hash.algorithm = std::move(algorithm);
    hash.size = size;
    hash.expected_size = size;
    hash.actual = digest;
    hash.expected = std::move(digest);
    hash.matched = true;
    return hash;
}

void put_be32(std::vector<unsigned char>& bytes,
              std::size_t offset,
              std::uint32_t value) {
    bytes[offset] = static_cast<unsigned char>((value >> 24U) & 0xffU);
    bytes[offset + 1] =
        static_cast<unsigned char>((value >> 16U) & 0xffU);
    bytes[offset + 2] =
        static_cast<unsigned char>((value >> 8U) & 0xffU);
    bytes[offset + 3] = static_cast<unsigned char>(value & 0xffU);
}

} // namespace

TEST_CASE("game integrity rechecks cartridge CRC-32 and size",
          "[game_integrity]") {
    const fs::path root = make_integrity_root("crc32");
    const fs::path mediaRoot = root / "media";
    write_file(mediaRoot / "mslug.neo", "abc");

    goliath::Rom rom;
    rom.file = "mslug.neo";
    rom.hashes.push_back(reference(
        "mslug.neo", "Cartridge", "CRC-32", 3, "352441c2"));

    const auto passed = goliath::verify_game_integrity(
        mediaRoot, rom.file, &rom, root / "cache");
    REQUIRE(passed.status == goliath::GameIntegrityStatus::Passed);
    REQUIRE(passed.hashes.size() == 1);
    CHECK(passed.matched_files == 1);
    CHECK(passed.problem_files == 0);
    CHECK(passed.hashes[0].actual ==
          std::optional<std::string>("352441c2"));
    CHECK(passed.hashes[0].matched);

    write_file(mediaRoot / "mslug.neo", "changed");
    const auto changed = goliath::verify_game_integrity(
        mediaRoot, rom.file, &rom, root / "cache");
    REQUIRE(changed.status ==
            goliath::GameIntegrityStatus::ProblemsFound);
    REQUIRE(changed.hashes.size() == 1);
    CHECK(changed.matched_files == 0);
    CHECK(changed.problem_files == 1);
    CHECK(changed.hashes[0].size == std::optional<std::uintmax_t>(7));
    CHECK_FALSE(changed.hashes[0].matched);

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST_CASE("game integrity resolves CUE descriptor and track files",
          "[game_integrity]") {
    const fs::path root = make_integrity_root("cue");
    const fs::path mediaRoot = root / "media";
    std::error_code ec;
    fs::create_directories(mediaRoot / "disc/tracks", ec);
    const std::string cue =
        "FILE \"tracks/TRACK01.BIN\" BINARY\n"
        "  TRACK 01 MODE1/2352\n";
    write_file(mediaRoot / "disc/game.cue", cue);
    write_file(mediaRoot / "disc/tracks/track01.bin", "track-data");

    goliath::Rom rom;
    rom.file = "disc/game.cue";
    rom.hashes.push_back(reference(
        "game.cue", "CUE descriptor", "SHA-1", cue.size(),
        goliath::sha1_hex(cue)));
    rom.hashes.push_back(reference(
        "track01.bin", "Track 01", "SHA-1", 10,
        goliath::sha1_hex("track-data")));

    const auto passed = goliath::verify_game_integrity(
        mediaRoot, rom.file, &rom, root / "cache");
    REQUIRE(passed.status == goliath::GameIntegrityStatus::Passed);
    CHECK(passed.matched_files == 2);
    CHECK(passed.problem_files == 0);

    fs::remove(mediaRoot / "disc/tracks/track01.bin", ec);
    const auto missing = goliath::verify_game_integrity(
        mediaRoot, rom.file, &rom, root / "cache");
    REQUIRE(missing.status ==
            goliath::GameIntegrityStatus::ProblemsFound);
    REQUIRE(missing.hashes.size() == 2);
    CHECK(missing.matched_files == 1);
    CHECK(missing.problem_files == 1);
    CHECK(missing.hashes[1].actual ==
          std::optional<std::string>("Missing"));
    CHECK_FALSE(missing.hashes[1].matched);

    fs::remove_all(root, ec);
}

TEST_CASE("game integrity reads the current CHD combined SHA-1",
          "[game_integrity]") {
    const fs::path root = make_integrity_root("chd");
    const fs::path mediaRoot = root / "media";
    const fs::path chdPath = mediaRoot / "disc/game.chd";

    std::vector<unsigned char> header(124, 0);
    const std::string magic = "MComprHD";
    std::copy(magic.begin(), magic.end(), header.begin());
    put_be32(header, 8, 124);
    put_be32(header, 12, 5);
    std::string expected;
    static constexpr char hex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < 20; ++i) {
        const unsigned char value = static_cast<unsigned char>(i + 1);
        header[84 + i] = value;
        expected.push_back(hex[value >> 4U]);
        expected.push_back(hex[value & 0x0fU]);
    }
    {
        std::ofstream output(chdPath, std::ios::binary);
        REQUIRE(output.good());
        output.write(reinterpret_cast<const char*>(header.data()),
                     static_cast<std::streamsize>(header.size()));
        REQUIRE(output.good());
    }

    goliath::Rom rom;
    rom.file = "disc/game.chd";
    rom.hashes.push_back(reference(
        rom.file, "CHD", "SHA-1", header.size(), expected));

    const auto result = goliath::verify_game_integrity(
        mediaRoot, rom.file, &rom, root / "cache");
    REQUIRE(result.status == goliath::GameIntegrityStatus::Passed);
    REQUIRE(result.hashes.size() == 1);
    CHECK(result.hashes[0].actual ==
          std::optional<std::string>(expected));
    CHECK(result.hashes[0].matched);

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST_CASE("game integrity reports limited, unsafe, missing, and canceled checks",
          "[game_integrity]") {
    const fs::path root = make_integrity_root("states");
    const fs::path mediaRoot = root / "media";
    write_file(mediaRoot / "plain.neo", "abc");

    goliath::Rom noReference;
    noReference.file = "plain.neo";
    const auto limited = goliath::verify_game_integrity(
        mediaRoot, noReference.file, &noReference, root / "cache");
    CHECK(limited.status ==
          goliath::GameIntegrityStatus::AvailableWithoutReference);
    CHECK(limited.hashes.empty());

    goliath::Rom unsafe;
    unsafe.file = "plain.neo";
    unsafe.hashes.push_back(reference(
        "../outside.neo", "Cartridge", "CRC-32", 3, "352441c2"));
    const auto rejected = goliath::verify_game_integrity(
        mediaRoot, unsafe.file, &unsafe, root / "cache");
    REQUIRE(rejected.status ==
            goliath::GameIntegrityStatus::ProblemsFound);
    REQUIRE(rejected.hashes.size() == 1);
    CHECK(rejected.hashes[0].actual ==
          std::optional<std::string>("Invalid relative path"));

    const auto missing = goliath::verify_game_integrity(
        mediaRoot, "missing.neo", nullptr, root / "cache");
    CHECK(missing.status ==
          goliath::GameIntegrityStatus::ProblemsFound);
    CHECK(missing.problem_files == 1);

    std::atomic<bool> cancel{true};
    const auto canceled = goliath::verify_game_integrity(
        mediaRoot, noReference.file, &noReference, root / "cache", &cancel);
    CHECK(canceled.status == goliath::GameIntegrityStatus::Canceled);

    std::error_code ec;
    fs::remove_all(root, ec);
}
