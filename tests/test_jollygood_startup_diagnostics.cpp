#include "catch2/catch.hpp"

#include "game/jollygood_startup_diagnostics.hpp"

using goliath::JollygoodStartupIssue;
using goliath::diagnose_jollygood_startup_failure;

TEST_CASE("Geolith BIOS archive failures receive verification guidance",
          "[launch][diagnostics][bios]") {
    const auto diagnosis = diagnose_jollygood_startup_failure(
        "[ERROR] Failed to load uni-bios_4_0.rom from BIOS archive!\r\n"
        "[ERROR] Failed to load game. Exiting...\r\n");

    REQUIRE(diagnosis.has_value());
    CHECK(diagnosis->issue == JollygoodStartupIssue::BiosArchive);
    CHECK(diagnosis->offer_bios_verification);
    CHECK(diagnosis->title.find("BIOS") != std::string::npos);
    CHECK(diagnosis->guidance.find("Verify BIOS") != std::string::npos);
}

TEST_CASE("Geolith AES incompatibilities receive exact-media MVS guidance",
          "[launch][diagnostics][system]") {
    const auto mvsOnly = diagnose_jollygood_startup_failure(
        "This title is compatible with the Neo Geo MVS and Universe BIOS only\n"
        "Failed to load game. Exiting...\n");
    REQUIRE(mvsOnly.has_value());
    CHECK(mvsOnly->issue ==
          JollygoodStartupIssue::RequiresMvsOrUniverse);
    CHECK_FALSE(mvsOnly->offer_bios_verification);
    CHECK(mvsOnly->guidance.find("Game settings > General") !=
          std::string::npos);

    const auto aesRejected = diagnose_jollygood_startup_failure(
        "[error] THIS TITLE IS INCOMPATIBLE WITH AES SYSTEMS\n");
    REQUIRE(aesRejected.has_value());
    CHECK(aesRejected->issue ==
          JollygoodStartupIssue::RequiresMvsOrUniverse);
}

TEST_CASE("Geolith US MVS incompatibility receives region guidance",
          "[launch][diagnostics][system]") {
    const auto diagnosis = diagnose_jollygood_startup_failure(
        "This title is incompatible with US MVS systems\n");

    REQUIRE(diagnosis.has_value());
    CHECK(diagnosis->issue == JollygoodStartupIssue::UnsupportedUsMvs);
    CHECK(diagnosis->title.find("US MVS") != std::string::npos);
    CHECK(diagnosis->guidance.find("region or system type") !=
          std::string::npos);
}

TEST_CASE("Unknown or informational JGRF output keeps generic diagnostics",
          "[launch][diagnostics]") {
    CHECK_FALSE(diagnose_jollygood_startup_failure(
        "Failed to load game. Exiting...\n").has_value());
    CHECK_FALSE(diagnose_jollygood_startup_failure(
        "BIOS: neogeo.zip - MVS BIOS OK\n").has_value());
    CHECK_FALSE(diagnose_jollygood_startup_failure(
        "Failed to load shader from cache\n").has_value());
    CHECK_FALSE(diagnose_jollygood_startup_failure(
        "Failed to load shader\n"
        "Nothing was read from BIOS archive diagnostics\n").has_value());
    CHECK_FALSE(diagnose_jollygood_startup_failure({}).has_value());
}
