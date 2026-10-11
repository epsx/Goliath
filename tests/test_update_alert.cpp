#include "catch2/catch.hpp"

#include "update/update_alert.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

using namespace goliath;

namespace {

fs::path make_update_alert_test_root(const std::string& name) {
    const auto stamp = std::chrono::high_resolution_clock::now()
                           .time_since_epoch()
                           .count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_update_alert_" + name + "_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

PublishedRelease sample_release() {
    return PublishedRelease{
        ReleaseVersion{0, 40, 1, true},
        "v0.40.1-preview",
        "Goliath 0.40.1 Preview",
        "https://github.com/epsx/Goliath/releases/tag/v0.40.1-preview",
        "2026-10-08T12:52:26Z",
        "## Changes\n\n- A cached multiline changelog",
    };
}

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << text;
    REQUIRE(output.good());
}

} // namespace

TEST_CASE("missing update alert cache is a valid empty state", "[update]") {
    const fs::path root = make_update_alert_test_root("missing");
    std::string error;

    CHECK_FALSE(load_update_alert_cache(root / "alert.json", &error));
    CHECK(error.empty());

    std::error_code ec;
    fs::remove_all(root, ec);
    CHECK_FALSE(ec);
}

TEST_CASE("update alert cache preserves release details and Markdown",
          "[update][persistence]") {
    const fs::path root = make_update_alert_test_root("roundtrip");
    const fs::path path = root / "config/goliath_update_alert.json";
    const PublishedRelease release = sample_release();
    std::string error;

    REQUIRE(save_update_alert_cache(path, release, &error));
    REQUIRE(error.empty());

    const auto loaded = load_update_alert_cache(path, &error);
    REQUIRE(loaded);
    CHECK(error.empty());
    CHECK(loaded->version.major == 0);
    CHECK(loaded->version.minor == 40);
    CHECK(loaded->version.patch == 1);
    CHECK(loaded->version.preview);
    CHECK(loaded->tag == release.tag);
    CHECK(loaded->name == release.name);
    CHECK(loaded->page_url == release.page_url);
    CHECK(loaded->published_at == release.published_at);
    CHECK(loaded->body == release.body);

    std::error_code ec;
    fs::remove_all(root, ec);
    CHECK_FALSE(ec);
}

TEST_CASE("malformed update alert caches are rejected", "[update]") {
    const fs::path root = make_update_alert_test_root("malformed");
    const fs::path path = root / "goliath_update_alert.json";
    write_text(path, R"json({"schema_version":1,"release":{"tag":"bad"}})json");

    std::string error;
    CHECK_FALSE(load_update_alert_cache(path, &error));
    CHECK_FALSE(error.empty());

    std::error_code ec;
    fs::remove_all(root, ec);
    CHECK_FALSE(ec);
}

TEST_CASE("update alert cache clearing is idempotent", "[update][persistence]") {
    const fs::path root = make_update_alert_test_root("clear");
    const fs::path path = root / "goliath_update_alert.json";
    std::string error;

    REQUIRE(save_update_alert_cache(path, sample_release(), &error));
    REQUIRE(fs::is_regular_file(path));
    CHECK(clear_update_alert_cache(path, &error));
    CHECK(error.empty());
    CHECK_FALSE(fs::exists(path));
    CHECK(clear_update_alert_cache(path, &error));
    CHECK(error.empty());

    std::error_code ec;
    fs::remove_all(root, ec);
    CHECK_FALSE(ec);
}

TEST_CASE("clearing an update alert never removes a directory", "[update]") {
    const fs::path root = make_update_alert_test_root("directory");
    const fs::path path = root / "goliath_update_alert.json";
    REQUIRE(fs::create_directories(path));

    std::string error;
    CHECK_FALSE(clear_update_alert_cache(path, &error));
    CHECK_FALSE(error.empty());
    CHECK(fs::is_directory(path));

    std::error_code ec;
    fs::remove_all(root, ec);
    CHECK_FALSE(ec);
}

TEST_CASE("cached update alerts expire against the installed version",
          "[update]") {
    const PublishedRelease preview = sample_release();
    CHECK(release_is_newer_than_installed(preview, "0.40.0"));
    CHECK_FALSE(release_is_newer_than_installed(preview, "0.40.1-preview"));
    CHECK_FALSE(release_is_newer_than_installed(preview, "0.40.1"));

    PublishedRelease stable = preview;
    stable.tag = "v0.40.1";
    stable.version.preview = false;
    CHECK(release_is_newer_than_installed(stable, "0.40.1-preview"));
    CHECK_FALSE(release_is_newer_than_installed(stable, "0.40.1"));
}
