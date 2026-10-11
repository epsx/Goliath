#include "catch2/catch.hpp"

#include "common/paths.hpp"
#include "common/qt_path.hpp"
#include "game/jollygood_bios.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

using goliath::AppPaths;
using goliath::ensure_jollygood_bios;
using goliath::path_from_utf8;
using goliath::path_to_utf8;

namespace {

fs::path make_bios_test_root(const fs::path& name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path unique = "goliath_bios_";
    unique += name;
    unique += "_";
    unique += std::to_string(stamp);
    const fs::path root = fs::temp_directory_path() / unique;
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

TEST_CASE("BIOS marker preserves a Unicode source path as UTF-8",
          "[bios][filesystem][unicode]") {
    const fs::path root = make_bios_test_root(
        path_from_utf8("Știință_日本"));
    const AppPaths paths = make_bios_paths(root);
    const fs::path destination = expected_bios_destination(paths);
    const fs::path marker = paths.data_dir / "jollygood" / "bios_source.txt";

    write_text(paths.bios_dir / "000-lo.lo", "configured BIOS");
    ensure_jollygood_bios(paths);

    std::error_code ec;
    const fs::path canonicalBiosDir = fs::weakly_canonical(paths.bios_dir, ec);
    REQUIRE_FALSE(ec);
    REQUIRE(fs::is_directory(destination));
    REQUIRE(fs::is_regular_file(destination / "000-lo.lo"));
    REQUIRE(read_text(marker) == path_to_utf8(canonicalBiosDir));

    // Remove a junction/symlink before its target. On Windows, remove_all()
    // may otherwise delete configured-bios first and then report ENOENT while
    // inspecting the now-dangling JGRF BIOS junction.
    fs::remove(destination, ec);
    ec.clear();
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

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
