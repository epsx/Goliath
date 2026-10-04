#include "catch2/catch.hpp"

#include "update/release_feed.hpp"

using namespace goliath;

TEST_CASE("Goliath release versions accept stable and preview tags",
          "[update]") {
    const auto stable = parse_release_version("v0.41.2");
    REQUIRE(stable);
    CHECK(stable->major == 0);
    CHECK(stable->minor == 41);
    CHECK(stable->patch == 2);
    CHECK_FALSE(stable->preview);

    const auto preview = parse_release_version("0.42.0-preview");
    REQUIRE(preview);
    CHECK(preview->major == 0);
    CHECK(preview->minor == 42);
    CHECK(preview->patch == 0);
    CHECK(preview->preview);

    CHECK_FALSE(parse_release_version("v0.42"));
    CHECK_FALSE(parse_release_version("v0.42.0-beta"));
    CHECK_FALSE(parse_release_version("release-0.42.0"));
    CHECK_FALSE(parse_release_version("v0.x.0"));
}

TEST_CASE("Goliath release comparison is numeric and stable-aware",
          "[update]") {
    const ReleaseVersion v0409{0, 40, 9, false};
    const ReleaseVersion v0410Preview{0, 41, 0, true};
    const ReleaseVersion v0410Stable{0, 41, 0, false};

    CHECK(compare_release_versions(v0410Preview, v0409) > 0);
    CHECK(compare_release_versions(v0409, v0410Preview) < 0);
    CHECK(compare_release_versions(v0410Stable, v0410Preview) > 0);
    CHECK(compare_release_versions(v0410Stable, v0410Stable) == 0);
}

TEST_CASE("release feed selects the highest non-draft Goliath version",
          "[update]") {
    const std::string response = R"json(
[
  {
    "draft": false,
    "prerelease": true,
    "tag_name": "v0.41.0-preview",
    "name": "Goliath 0.41.0 Preview",
    "html_url": "https://github.com/epsx/Goliath/releases/tag/v0.41.0-preview",
    "published_at": "2026-10-08T12:00:00Z",
    "body": "## Changes\n\n- Faster startup"
  },
  {
    "draft": true,
    "prerelease": true,
    "tag_name": "v9.0.0-preview",
    "name": "Unpublished",
    "html_url": "https://github.com/epsx/Goliath/releases/tag/v9.0.0-preview"
  },
  {
    "draft": false,
    "prerelease": false,
    "tag_name": "v0.40.1",
    "name": "Goliath 0.40.1",
    "html_url": "https://github.com/epsx/Goliath/releases/tag/v0.40.1",
    "published_at": "2026-10-09T12:00:00Z"
  },
  {
    "draft": false,
    "prerelease": true,
    "tag_name": "unrelated-tag",
    "name": "Ignored",
    "html_url": "https://github.com/epsx/Goliath/releases/tag/unrelated-tag"
  }
]
)json";

    std::string error;
    const auto release = select_newest_published_release(response, &error);
    REQUIRE(release);
    CHECK(error.empty());
    CHECK(release->tag == "v0.41.0-preview");
    CHECK(release->name == "Goliath 0.41.0 Preview");
    CHECK(release->published_at == "2026-10-08T12:00:00Z");
    CHECK(release->body == "## Changes\n\n- Faster startup");
}

TEST_CASE("stable release wins over preview with the same version",
          "[update]") {
    const std::string response = R"json(
[
  {"draft": false, "tag_name": "v0.41.0-preview",
   "html_url": "https://github.com/epsx/Goliath/releases/tag/v0.41.0-preview"},
  {"draft": false, "tag_name": "v0.41.0",
   "html_url": "https://github.com/epsx/Goliath/releases/tag/v0.41.0"}
]
)json";

    const auto release = select_newest_published_release(response);
    REQUIRE(release);
    CHECK(release->tag == "v0.41.0");
}

TEST_CASE("malformed or incompatible release feeds report an error",
          "[update]") {
    std::string error;
    CHECK_FALSE(select_newest_published_release("{}", &error));
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(select_newest_published_release("not-json", &error));
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(select_newest_published_release(
        R"json([{"draft": false, "tag_name": "bad"}])json", &error));
    CHECK_FALSE(error.empty());
}
