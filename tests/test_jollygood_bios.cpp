#include "catch2/catch.hpp"

#include "common/paths.hpp"
#include "game/jollygood_bios.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

using goliath::AppPaths;
using goliath::ensure_jollygood_bios;

namespace {

fs::path make_bios_test_root(const std::string& name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_bios_" + name + "_" + std::to_string(stamp));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    return root;
}

void write_text(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

AppPaths make_bios_paths(const fs::path& root, bool dynamicCore = true) {
    AppPaths paths;
    paths.base_dir = root;
    paths.data_dir = root / "data";
    paths.bios_dir = root / "configured-bios";
#if defined(_WIN32)
    paths.jollygood_exe = root / "bin" / "jollygood.exe";
#else
    paths.jollygood_exe = root / "bin" / "jollygood";
#endif
    paths.jollygood_args = dynamicCore
        ? std::vector<std::string>{"-c", "geolith"}
        : std::vector<std::string>{};
    write_text(paths.jollygood_exe, "test executable");
    return paths;
}

fs::path expected_bios_destination(const AppPaths& paths) {
    return paths.data_dir / "jollygood" / "bios";
}

} // namespace

TEST_CASE("BIOS preparation preserves an existing plain destination directory",
          "[bios][filesystem][safety]") {
    const fs::path root = make_bios_test_root("plain_directory");
    const AppPaths paths = make_bios_paths(root);
    const fs::path destination = expected_bios_destination(paths);
    const fs::path marker = paths.data_dir / "jollygood" / "bios_source.txt";

    write_text(paths.bios_dir / "000-lo.lo", "configured BIOS");
    write_text(destination / "user-owned.txt", "keep me");

    REQUIRE(fs::is_directory(paths.bios_dir));
    REQUIRE(fs::is_directory(destination));
    REQUIRE(read_text(destination / "user-owned.txt") == "keep me");

    ensure_jollygood_bios(paths);

    REQUIRE(fs::is_regular_file(paths.bios_dir / "000-lo.lo"));
    REQUIRE(fs::is_directory(destination));
    REQUIRE(read_text(destination / "user-owned.txt") == "keep me");
    REQUIRE(read_text(destination / "000-lo.lo") == "configured BIOS");
    REQUIRE(fs::is_regular_file(marker));

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("BIOS preparation refuses a non-directory destination",
          "[bios][filesystem][safety]") {
    const fs::path root = make_bios_test_root("blocker");
    const AppPaths paths = make_bios_paths(root);
    const fs::path destination = expected_bios_destination(paths);
    const fs::path marker = paths.data_dir / "jollygood" / "bios_source.txt";

    write_text(paths.bios_dir / "000-lo.lo", "configured BIOS");
    write_text(destination, "user-owned blocker");

    REQUIRE(fs::is_regular_file(destination));
    REQUIRE(read_text(destination) == "user-owned blocker");

    ensure_jollygood_bios(paths);

    REQUIRE(fs::is_regular_file(destination));
    REQUIRE(read_text(destination) == "user-owned blocker");
    REQUIRE(fs::is_regular_file(paths.bios_dir / "000-lo.lo"));
    REQUIRE_FALSE(fs::exists(marker));

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("BIOS fallback refuses a non-regular destination child",
          "[bios][filesystem][safety]") {
    const fs::path root = make_bios_test_root("child_blocker");
    const AppPaths paths = make_bios_paths(root);
    const fs::path destination = expected_bios_destination(paths);
    const fs::path blockedChild = destination / "000-lo.lo";
    const fs::path marker = paths.data_dir / "jollygood" / "bios_source.txt";

    write_text(paths.bios_dir / "000-lo.lo", "configured BIOS");
    write_text(blockedChild / "user-owned.txt", "keep child");

    REQUIRE(fs::is_regular_file(paths.bios_dir / "000-lo.lo"));
    REQUIRE(fs::is_directory(blockedChild));
    REQUIRE(read_text(blockedChild / "user-owned.txt") == "keep child");

    ensure_jollygood_bios(paths);

    REQUIRE(fs::is_directory(blockedChild));
    REQUIRE(read_text(blockedChild / "user-owned.txt") == "keep child");
    REQUIRE_FALSE(fs::is_regular_file(blockedChild));
    REQUIRE_FALSE(fs::exists(marker));

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("static-core BIOS preparation uses the executable directory",
          "[bios][filesystem][layout]") {
    const fs::path root = make_bios_test_root("static_layout");
    const AppPaths paths = make_bios_paths(root, false);
    const fs::path destination = paths.jollygood_exe.parent_path() / "bios";

    write_text(paths.bios_dir / "000-lo.lo", "configured BIOS");
    ensure_jollygood_bios(paths);

    REQUIRE(fs::is_directory(destination));
    REQUIRE(read_text(destination / "000-lo.lo") == "configured BIOS");
    REQUIRE_FALSE(fs::exists(paths.data_dir / "jollygood" / "bios"));

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}
