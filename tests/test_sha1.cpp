#include "catch2/catch.hpp"
#include "common/sha1.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("SHA-1 helpers match standard vectors", "[sha1]") {
    REQUIRE(goliath::sha1_hex("") ==
            "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    REQUIRE(goliath::sha1_hex("abc") ==
            "a9993e364706816aba3e25717850c26c9cd0d89d");
    REQUIRE(goliath::sha1_hex("The quick brown fox jumps over the lazy dog") ==
            "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

    const fs::path path = fs::temp_directory_path() / "goliath_sha1_test.bin";
    {
        std::ofstream out(path, std::ios::binary);
        out << "abc";
    }
    const auto digest = goliath::sha1_file_hex(path);
    REQUIRE(digest.has_value());
    REQUIRE(*digest == "a9993e364706816aba3e25717850c26c9cd0d89d");
    fs::remove(path);
}
