#include "catch2/catch.hpp"

#include "game/library_rescan_comparison.hpp"

#include <string>
#include <utility>
#include <vector>

namespace {

goliath::Game make_game(std::string display, std::string media,
                        std::string system = "neogeo") {
    goliath::Game game;
    game.name = display;
    game.display = std::move(display);
    game.short_name = game.name;
    game.source = system == "neogeocd" ? "redump" : "mame";
    game.system = std::move(system);
    game.main_rom = media;

    goliath::Rom rom;
    rom.file = std::move(media);
    rom.mame = game.short_name;
    rom.main = true;
    game.roms.push_back(std::move(rom));
    return game;
}

} // namespace

TEST_CASE("rescan comparison classifies exact-media changes",
          "[rescan][comparison]") {
    auto unchanged = make_game("Metal Slug", "mslug.neo");
    auto updatedBefore = make_game("Last Resort", "lresort.neo");
    auto updatedAfter = updatedBefore;
    updatedAfter.year = "1992";

    const std::vector<goliath::Game> before = {
        unchanged,
        updatedBefore,
        make_game("Removed Game", "removed.neo")};
    const std::vector<goliath::Game> after = {
        make_game("Added Game", "added.neo"),
        updatedAfter,
        unchanged};

    const auto result = goliath::compare_library_snapshots(before, after);
    CHECK(result.previous_count == 3);
    CHECK(result.current_count == 3);
    CHECK(result.unchanged_count == 1);
    REQUIRE(result.added.size() == 1);
    REQUIRE(result.removed.size() == 1);
    REQUIRE(result.updated.size() == 1);
    CHECK(result.added.front().display == "Added Game");
    CHECK(result.removed.front().display == "Removed Game");
    CHECK(result.updated.front().display == "Last Resort");
}

TEST_CASE("rescan comparison ignores library ordering",
          "[rescan][comparison]") {
    const auto first = make_game("Alpha", "alpha.neo");
    const auto second = make_game("Beta", "beta.neo");

    const auto result = goliath::compare_library_snapshots(
        {first, second}, {second, first});
    CHECK(result.previous_count == 2);
    CHECK(result.current_count == 2);
    CHECK(result.unchanged_count == 2);
    CHECK(result.added.empty());
    CHECK(result.removed.empty());
    CHECK(result.updated.empty());
}

TEST_CASE("rescan comparison treats media replacement as remove and add",
          "[rescan][comparison]") {
    const auto result = goliath::compare_library_snapshots(
        {make_game("Samurai Shodown", "samsho.cue", "neogeocd")},
        {make_game("Samurai Shodown", "samsho.chd", "neogeocd")});

    CHECK(result.unchanged_count == 0);
    CHECK(result.updated.empty());
    REQUIRE(result.added.size() == 1);
    REQUIRE(result.removed.size() == 1);
    CHECK(result.added.front().media == "samsho.chd");
    CHECK(result.removed.front().media == "samsho.cue");
}

TEST_CASE("rescan comparison counts a new cartridge variant as added media",
          "[rescan][comparison]") {
    auto before = make_game("Shock Troopers", "shocktr2.neo");
    auto after = before;
    goliath::Rom variant;
    variant.file = "lans2004.neo";
    variant.name = "Lansquenet 2004";
    variant.label = "Lansquenet 2004";
    variant.mame = "lans2004";
    variant.main = false;
    after.roms.push_back(std::move(variant));

    const auto result =
        goliath::compare_library_snapshots({before}, {after});
    CHECK(result.previous_count == 1);
    CHECK(result.current_count == 2);
    CHECK(result.unchanged_count == 1);
    CHECK(result.updated.empty());
    REQUIRE(result.added.size() == 1);
    CHECK(result.added.front().media == "lans2004.neo");
}

TEST_CASE("rescan comparison keeps a surviving clone unchanged when parent disappears",
          "[rescan][comparison]") {
    auto before = make_game("Art of Fighting 3", "aof3.neo");
    goliath::Rom clone;
    clone.file = "aof3k.neo";
    clone.name = "Art of Fighting 3 (Korean release)";
    clone.label = "Korean release";
    clone.mame = "aof3k";
    clone.cloneof = "aof3";
    clone.main = false;
    before.roms.push_back(clone);

    auto after = before;
    after.main_rom.reset();
    after.roms.erase(after.roms.begin());

    const auto result =
        goliath::compare_library_snapshots({before}, {after});
    CHECK(result.previous_count == 2);
    CHECK(result.current_count == 1);
    CHECK(result.unchanged_count == 1);
    CHECK(result.added.empty());
    REQUIRE(result.removed.size() == 1);
    CHECK(result.removed.front().media == "aof3.neo");
    CHECK(result.updated.empty());
}

TEST_CASE("first rescan reports every discovered game as added",
          "[rescan][comparison]") {
    const auto result = goliath::compare_library_snapshots(
        {}, {make_game("Zed Blade", "zedblade.neo")});
    CHECK(result.previous_count == 0);
    CHECK(result.current_count == 1);
    CHECK(result.unchanged_count == 0);
    REQUIRE(result.added.size() == 1);
    CHECK(result.removed.empty());
    CHECK(result.updated.empty());
}

TEST_CASE("rescan verification preview identifies exact media and problem",
          "[rescan][verification]") {
    auto cartridge = make_game("Bad Cartridge", "bad.neo");
    cartridge.roms.front().verification = "geolith-crc32-mismatch";

    auto chd = make_game("Bad CHD", "bad.chd", "neogeocd");
    chd.verification = "mame-chd-mismatch";

    auto cue = make_game("Bad CUE", "bad.cue", "neogeocd");
    cue.verification = "redump-tracks-only";

    auto unknown = make_game("Mystery Disc", "mystery.cue", "neogeocd");
    unknown.identified = false;
    unknown.verification.reset();

    const auto issues = goliath::collect_library_verification_issues(
        {cartridge, chd, cue, unknown}, 4);
    REQUIRE(issues.size() == 4);
    CHECK(issues[0].display == "Bad Cartridge");
    CHECK(issues[0].media == "bad.neo");
    CHECK(issues[0].problem == "CRC-32 mismatch");
    CHECK(issues[1].display == "Bad CHD");
    CHECK(issues[1].media == "bad.chd");
    CHECK(issues[1].problem == "CHD SHA-1 mismatch");
    CHECK(issues[2].display == "Bad CUE");
    CHECK(issues[2].media == "bad.cue");
    CHECK(issues[2].problem.find("CUE layout mismatch") != std::string::npos);
    CHECK(issues[3].display == "Mystery Disc");
    CHECK(issues[3].media == "mystery.cue");
    CHECK(issues[3].problem == "Unknown Neo Geo CD image");
}

TEST_CASE("library verification summary counts CUE mismatches as issues",
          "[rescan][verification]") {
    auto verifiedCue = make_game("Good CUE", "good.cue", "neogeocd");
    verifiedCue.verification = "redump-cue";

    auto cueMismatch = make_game("Bad CUE", "bad.cue", "neogeocd");
    cueMismatch.verification = "redump-tracks-only";

    auto chdMismatch = make_game("Bad CHD", "bad.chd", "neogeocd");
    chdMismatch.verification = "mame-chd-mismatch";

    auto crcMismatch = make_game("Bad Cart", "bad.neo");
    crcMismatch.roms.front().verification = "geolith-crc32-mismatch";

    auto unknown = make_game("Unknown", "unknown.cue", "neogeocd");
    unknown.identified = false;

    const auto summary = goliath::summarize_library_verification(
        {verifiedCue, cueMismatch, chdMismatch, crcMismatch, unknown});
    CHECK(summary.verified == 1);
    CHECK(summary.cue_mismatches == 1);
    CHECK(summary.chd_mismatches == 1);
    CHECK(summary.crc32_mismatches == 1);
    CHECK(summary.mismatch_count() == 3);
    CHECK(summary.unknown == 1);
    CHECK(summary.issue_count() == 4);
}

TEST_CASE("rescan verification preview is capped without hiding total data",
          "[rescan][verification]") {
    auto first = make_game("First", "first.neo");
    first.roms.front().verification = "geolith-crc32-mismatch";
    auto second = make_game("Second", "second.neo");
    second.roms.front().verification = "geolith-crc32-mismatch";

    const auto issues = goliath::collect_library_verification_issues(
        {first, second}, 1);
    REQUIRE(issues.size() == 1);
    CHECK(issues.front().media == "first.neo");
}
