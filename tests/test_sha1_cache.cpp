#include "catch2/catch.hpp"

#include "game/hash_cache.hpp"
#include "game/sha1_cache.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

using goliath::Sha1Cache;
using goliath::HashCache;

TEST_CASE("SHA-1 cache persistence never opens its legacy temporary name",
          "[sha1_cache][persistence][filesystem][safety]") {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_sha1_cache_target_" + std::to_string(stamp));
    const fs::path track = root / "track.bin";
    const fs::path cachePath = root / "hash_cache.json";
    const fs::path legacyTemp = root / "hash_cache.json.tmp";
    const fs::path sentinel = root / "outside" / "user-owned.txt";
    const std::string sentinelBytes = "keep external bytes\n";

    std::error_code ec;
    fs::create_directories(sentinel.parent_path(), ec);
    REQUIRE_FALSE(ec);

    {
        std::ofstream output(track, std::ios::binary | std::ios::trunc);
        REQUIRE(output.good());
        output << "abc";
        REQUIRE(output.good());
    }
    {
        std::ofstream output(sentinel, std::ios::binary | std::ios::trunc);
        REQUIRE(output.good());
        output << sentinelBytes;
        REQUIRE(output.good());
    }

    // The old writer opened this predictable name with truncation. A hard
    // link proves that doing so can modify an unrelated file even without
    // symbolic-link privileges on Windows.
    fs::create_hard_link(sentinel, legacyTemp, ec);
    REQUIRE_FALSE(ec);
    REQUIRE(fs::equivalent(sentinel, legacyTemp));

    Sha1Cache cache = Sha1Cache::load(cachePath);
    const auto digest = cache.file_sha1(track, fs::file_size(track));
    REQUIRE(digest.has_value());
    REQUIRE(*digest == "a9993e364706816aba3e25717850c26c9cd0d89d");
    REQUIRE(cache.save());

    const auto read_text = [](const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    REQUIRE(read_text(sentinel) == sentinelBytes);
    REQUIRE(read_text(legacyTemp) == sentinelBytes);
    REQUIRE(fs::equivalent(sentinel, legacyTemp));
    REQUIRE(fs::is_regular_file(cachePath));
    REQUIRE(read_text(cachePath).find(*digest) != std::string::npos);

    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("SHA-1 cache retries a failed save without losing dirty entries",
          "[sha1_cache][persistence][recovery]") {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_sha1_cache_recovery_" + std::to_string(stamp));
    const fs::path track = root / "track.bin";
    const fs::path blocker = root / "cache-blocker";
    const fs::path cachePath = blocker / "hash_cache.json";
    const std::string sentinelBytes = "keep blocker bytes unchanged\n";

    std::error_code ec;
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);
    {
        std::ofstream output(track, std::ios::binary | std::ios::trunc);
        REQUIRE(output.good());
        output << "abc";
        REQUIRE(output.good());
    }
    {
        std::ofstream output(blocker, std::ios::binary | std::ios::trunc);
        REQUIRE(output.good());
        output << sentinelBytes;
        REQUIRE(output.good());
    }

    Sha1Cache cache = Sha1Cache::load(cachePath);
    const auto digest = cache.file_sha1(track, fs::file_size(track));
    REQUIRE(digest.has_value());
    REQUIRE(*digest == "a9993e364706816aba3e25717850c26c9cd0d89d");
    REQUIRE_FALSE(cache.save());

    const auto read_text = [](const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    REQUIRE(read_text(blocker) == sentinelBytes);

    REQUIRE(fs::remove(blocker));
    REQUIRE(cache.save());
    REQUIRE(fs::is_regular_file(cachePath));

    Sha1Cache recovered = Sha1Cache::load(cachePath);
    const auto recoveredDigest =
        recovered.file_sha1(track, fs::file_size(track));
    REQUIRE(recoveredDigest.has_value());
    REQUIRE(*recoveredDigest == *digest);
    REQUIRE(recovered.hits() == 1);
    REQUIRE(recovered.calculated() == 0);

    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("hash cache rejects a file changed during digest calculation",
          "[hash_cache][fingerprint][safety]") {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_hash_cache_changed_during_read_" +
         std::to_string(stamp));
    const fs::path file = root / "game.neo";
    const fs::path cachePath = root / "hash_cache.json";

    std::error_code ec;
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);
    {
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        REQUIRE(output.good());
        output << "abc";
        REQUIRE(output.good());
    }
    const fs::file_time_type oldMtime = fs::last_write_time(file, ec);
    REQUIRE_FALSE(ec);

    HashCache cache = HashCache::load(cachePath, "crc32", "crc32", 8);
    const auto digest = cache.file_hash(
        file, fs::file_size(file),
        [&](const fs::path&, const std::atomic<bool>*)
            -> std::optional<std::string> {
            std::ofstream output(file, std::ios::binary | std::ios::trunc);
            if (!output) return std::nullopt;
            output << "xyz";
            output.close();
            fs::last_write_time(file, oldMtime + std::chrono::seconds(2), ec);
            if (ec) return std::nullopt;
            return "352441c2";
        });

    REQUIRE_FALSE(digest.has_value());
    CHECK(cache.calculated() == 1);
    CHECK(cache.entries() == 0);
    CHECK(cache.save());
    CHECK_FALSE(fs::exists(cachePath));

    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}
