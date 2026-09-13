#include "catch2/catch.hpp"

#include "common/project_legal.hpp"

using namespace goliath;

TEST_CASE("project legal metadata identifies the published license",
          "[legal][release]") {
    REQUIRE(kProjectCopyright == "Copyright (C) 2026 epsx");
    REQUIRE(kProjectLicenseName == "GNU GPL v3 or later");
    REQUIRE(kProjectLicenseSpdx == "GPL-3.0-or-later");
    REQUIRE(kProjectUrl == "https://github.com/epsx/Goliath");
    REQUIRE(kProjectLicenseUrl ==
            "https://www.gnu.org/licenses/gpl-3.0.html");
    REQUIRE(kThirdPartyNoticesFile == "THIRD-PARTY-NOTICES.md");
}
