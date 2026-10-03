#include "ui/library_view_logic.hpp"
#include "ui/recording_id.hpp"

#include <catch2/catch.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using goliath::VisibleSelectionDecision;
using goliath::LibraryPlaytimeFilter;
using goliath::LibraryRatingFilter;
using goliath::detailsScrollTarget;
using goliath::libraryCatalogIdPresentation;
using goliath::libraryDetailsTitle;
using goliath::libraryExpansionShouldBePreserved;
using goliath::libraryGameListTitles;
using goliath::libraryMetricPrecedes;
using goliath::libraryMvsAesHumanTitle;
using goliath::libraryPersonalFiltersActive;
using goliath::libraryPersonalFiltersAllow;
using goliath::libraryPlaytimeFilterFromKey;
using goliath::libraryPlaytimeFilterKey;
using goliath::libraryRatingFilterFromKey;
using goliath::libraryRatingFilterKey;
using goliath::librarySelectionAllowed;
using goliath::librarySystemChangeRequired;
using goliath::libraryVariantAllowed;
using goliath::libraryVerificationPresentation;
using goliath::VerificationTone;
using goliath::treeItemHasExpandableChildren;
using goliath::visibleSelectionDecision;
using goliath::visibleSelectionShouldBeRevealed;

TEST_CASE("recording folders follow exact media filenames for Neo Geo CD",
          "[library-view][gallery]") {
    CHECK(goliath::recordingIdForMedia(
        "2020 Super Baseball (Japan) (En,Ja).cue") ==
          "2020_Super_Baseball__Japan___En_Ja_");
    CHECK(goliath::recordingIdForMedia(
        "3 count bout (1995) (snk) (jp-us) (fire suplex).chd") ==
          "3_count_bout__1995___snk___jp-us___fire_suplex_");
    CHECK(goliath::recordingIdForMedia("rbff1.neo") == "rbff1");
    CHECK(goliath::recordingIdForMedia("") == "game");
}

TEST_CASE("GIF folder action targets only games with recordings",
          "[library-view][gallery]") {
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString id = goliath::recordingIdForMedia(
        "2020 Super Baseball (Japan) (En,Ja).cue");
    const QString gameFolder = QDir(temp.path()).filePath("recordings/" + id);
    const QString dateFolder = QDir(gameFolder).filePath("2026-09-26");
    REQUIRE(QDir().mkpath(dateFolder));
    CHECK(goliath::recordingFolderWithGifs(temp.path(), id).isEmpty());
    QFile gif(QDir(dateFolder).filePath("16-30-42.gif"));
    REQUIRE(gif.open(QIODevice::WriteOnly));
    gif.write("GIF89a", 6);
    gif.close();
    CHECK(goliath::recordingFolderWithGifs(temp.path(), id) ==
          QDir(gameFolder).absolutePath());
    CHECK(goliath::recordingFolderWithGifs(temp.path(), "other-game").isEmpty());

    const QString legacy = QDir(temp.path()).filePath("screenshots");
    REQUIRE(QDir().mkpath(legacy));
    QFile oldGif(QDir(legacy).filePath("rbff1-20260926.gif"));
    REQUIRE(oldGif.open(QIODevice::WriteOnly));
    oldGif.write("GIF89a", 6);
    oldGif.close();
    CHECK(goliath::legacyGifFolder(legacy, "rbff1") == QDir(legacy).absolutePath());
    CHECK(goliath::legacyGifFolder(legacy, "rbff2").isEmpty());
}

TEST_CASE("visible library selections remain stable", "[library-view]") {
    CHECK(visibleSelectionDecision(true, true) ==
          VisibleSelectionDecision::KeepCurrent);
    CHECK(visibleSelectionDecision(true, false) ==
          VisibleSelectionDecision::KeepCurrent);
}

TEST_CASE("reselecting the active library system is a no-op",
          "[library-view][system]") {
    CHECK_FALSE(librarySystemChangeRequired("neogeo", "neogeo"));
    CHECK_FALSE(librarySystemChangeRequired("neogeocd", "neogeocd"));
    CHECK(librarySystemChangeRequired("neogeo", "neogeocd"));
    CHECK(librarySystemChangeRequired("neogeocd", "neogeo"));
}

TEST_CASE("hidden library selections choose a visible replacement",
          "[library-view]") {
    CHECK(visibleSelectionDecision(false, true) ==
          VisibleSelectionDecision::SelectVisibleReplacement);
    CHECK(visibleSelectionDecision(false, false) ==
          VisibleSelectionDecision::ClearSelection);
}

TEST_CASE("retained and replacement selections are revealed",
          "[library-view]") {
    CHECK(visibleSelectionShouldBeRevealed(
        VisibleSelectionDecision::KeepCurrent));
    CHECK(visibleSelectionShouldBeRevealed(
        VisibleSelectionDecision::SelectVisibleReplacement));
    CHECK_FALSE(visibleSelectionShouldBeRevealed(
        VisibleSelectionDecision::ClearSelection));
}

TEST_CASE("per-item expansion requires children", "[library-view]") {
    CHECK_FALSE(treeItemHasExpandableChildren(0));
    CHECK(treeItemHasExpandableChildren(1));
}

TEST_CASE("Favorites view keeps exact favorite variants visible",
          "[library-view][favorites]") {
    CHECK(libraryVariantAllowed(true, false, false));
    CHECK_FALSE(libraryVariantAllowed(false, false, true));
    CHECK(libraryVariantAllowed(false, true, true));
    CHECK_FALSE(libraryVariantAllowed(false, true, false));
}

TEST_CASE("personal filters keep exact matching variants visible",
          "[library-view][filters]") {
    CHECK(libraryVariantAllowed(false, false, false, true, true));
    CHECK_FALSE(libraryVariantAllowed(false, false, false, true, false));
    CHECK(libraryVariantAllowed(false, true, true, true, false));
}

TEST_CASE("rating filter keys are stable and unknown keys are safe",
          "[library-view][filters]") {
    CHECK(libraryRatingFilterKey(LibraryRatingFilter::Any) == "any");
    CHECK(libraryRatingFilterKey(LibraryRatingFilter::Rated) == "rated");
    CHECK(libraryRatingFilterKey(LibraryRatingFilter::AtLeast4) ==
          "at_least_4");
    CHECK(libraryRatingFilterFromKey("unrated") ==
          LibraryRatingFilter::Unrated);
    CHECK(libraryRatingFilterFromKey("at_least_5") ==
          LibraryRatingFilter::AtLeast5);
    CHECK(libraryRatingFilterFromKey("unsupported") ==
          LibraryRatingFilter::Any);
}

TEST_CASE("playtime filter keys are stable and unknown keys are safe",
          "[library-view][filters]") {
    CHECK(libraryPlaytimeFilterKey(LibraryPlaytimeFilter::Any) == "any");
    CHECK(libraryPlaytimeFilterKey(LibraryPlaytimeFilter::Played) ==
          "played");
    CHECK(libraryPlaytimeFilterFromKey("not_played") ==
          LibraryPlaytimeFilter::NotPlayed);
    CHECK(libraryPlaytimeFilterFromKey("unsupported") ==
          LibraryPlaytimeFilter::Any);
}

TEST_CASE("rating filters distinguish rated, unrated, and minimum stars",
          "[library-view][filters]") {
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Any, 0, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Rated, LibraryPlaytimeFilter::Any, 1, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Rated, LibraryPlaytimeFilter::Any, 0, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Unrated, LibraryPlaytimeFilter::Any, 0, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Unrated, LibraryPlaytimeFilter::Any, 2, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast4, LibraryPlaytimeFilter::Any, 4, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast4, LibraryPlaytimeFilter::Any, 5, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast4, LibraryPlaytimeFilter::Any, 3, 0));
}

TEST_CASE("playtime filters distinguish played and not played media",
          "[library-view][filters]") {
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Played, 0, 1));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Played, 0, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::NotPlayed, 0, 0));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::NotPlayed, 0, 1));
    CHECK_FALSE(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast3,
        LibraryPlaytimeFilter::Played, 3, 0));
    CHECK(libraryPersonalFiltersAllow(
        LibraryRatingFilter::AtLeast3,
        LibraryPlaytimeFilter::Played, 3, 60));
}

TEST_CASE("personal filter activity reflects either active dimension",
          "[library-view][filters]") {
    CHECK_FALSE(libraryPersonalFiltersActive(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::Any));
    CHECK(libraryPersonalFiltersActive(
        LibraryRatingFilter::Rated, LibraryPlaytimeFilter::Any));
    CHECK(libraryPersonalFiltersActive(
        LibraryRatingFilter::Any, LibraryPlaytimeFilter::NotPlayed));
}

TEST_CASE("filtered selections require an exact visible media match",
          "[library-view][filters]") {
    CHECK(librarySelectionAllowed(true, false, false));
    CHECK(librarySelectionAllowed(true, true, true));
    CHECK_FALSE(librarySelectionAllowed(true, true, false));
    CHECK_FALSE(librarySelectionAllowed(false, true, true));
}

TEST_CASE("filter-only expansion is not preserved as manual tree state",
          "[library-view][filters]") {
    CHECK(libraryExpansionShouldBePreserved(true, false));
    CHECK_FALSE(libraryExpansionShouldBePreserved(true, true));
    CHECK_FALSE(libraryExpansionShouldBePreserved(false, false));
    CHECK_FALSE(libraryExpansionShouldBePreserved(false, true));
}

TEST_CASE("missing sort metrics stay last in both directions",
          "[library-view][sort]") {
    CHECK(libraryMetricPrecedes(5, 0, true));
    CHECK(libraryMetricPrecedes(1, 0, false));
    CHECK_FALSE(libraryMetricPrecedes(0, 5, true));
    CHECK_FALSE(libraryMetricPrecedes(0, 1, false));
    CHECK(libraryMetricPrecedes(5, 3, true));
    CHECK(libraryMetricPrecedes(3, 5, false));
    CHECK_FALSE(libraryMetricPrecedes(3, 3, true));
}

TEST_CASE("library rating badges accept only persisted rating values",
          "[library-view][personal-badges]") {
    CHECK(goliath::libraryListRatingBadge(-1) == 0);
    CHECK(goliath::libraryListRatingBadge(0) == 0);
    CHECK(goliath::libraryListRatingBadge(1) == 1);
    CHECK(goliath::libraryListRatingBadge(3) == 3);
    CHECK(goliath::libraryListRatingBadge(5) == 5);
    CHECK(goliath::libraryListRatingBadge(6) == 0);
}

TEST_CASE("details scroll resets only for a true selection change",
          "[library-view]") {
    CHECK(detailsScrollTarget(true, 173) == 0);
    CHECK(detailsScrollTarget(false, 173) == 173);
}

TEST_CASE("Neo Geo CD details use the full catalog title",
          "[library-view]") {
    const std::string fullName =
        "Fatal Fury 3 - Road to the Final Victory (USA)";
    const std::string displayName =
        "Fatal Fury 3 - Road to the Final Victory";
    const std::string emptyName;

    CHECK(libraryDetailsTitle(true, fullName, displayName) == fullName);
    CHECK(libraryDetailsTitle(true, emptyName, displayName) == displayName);
}

TEST_CASE("Neo Geo CD duplicate list titles gain compact variant qualifiers",
          "[library-view][neocd][titles]") {
    const std::string display =
        "Garou Densetsu 3 - Road to the Final Victory";
    const auto cd = [&](const std::string& name, const std::string& id) {
        goliath::Game game;
        game.system = "neogeocd";
        game.source = "redump";
        game.name = name;
        game.display = display;
        game.redump_id = id;
        return game;
    };

    std::vector<goliath::Game> games{
        cd(display +
           " ~ Fatal Fury 3 - Road to the Final Victory "
           "(Export) (En,Ja,Es,Pt) (Rev 2)", "100"),
        cd(display +
           " ~ Fatal Fury 3 - Road to the Final Victory "
           "(World) (En,Ja,Es,Pt) (Rev 1)", "101"),
        cd(display +
           " ~ Fatal Fury 3 - Road to the Final Victory "
           "(Japan) (En,Ja,Es,Pt)", "102"),
        cd(display +
           " ~ Fatal Fury 3 - Road to the Final Victory "
           "(Export) (En,Ja,Es,Pt) (Rev 3)", "103"),
    };

    const std::vector<std::string> titles = libraryGameListTitles(games);
    REQUIRE(titles.size() == 4);
    CHECK(titles[0] == display + " [Export \xC2\xB7 Rev 2]");
    CHECK(titles[1] == display + " [World \xC2\xB7 Rev 1]");
    CHECK(titles[2] == display + " [Japan]");
    CHECK(titles[3] == display + " [Export \xC2\xB7 Rev 3]");
}

TEST_CASE("Neo Geo CD list titles add languages and IDs only when needed",
          "[library-view][neocd][titles]") {
    const auto cd = [](const std::string& name, const std::string& id) {
        goliath::Game game;
        game.system = "neogeocd";
        game.source = "redump";
        game.name = name;
        game.display = "Example Game";
        game.redump_id = id;
        return game;
    };

    std::vector<goliath::Game> languageGames{
        cd("Example Game (Japan) (En)", "200"),
        cd("Example Game (Japan) (Ja)", "201"),
    };
    CHECK(libraryGameListTitles(languageGames) ==
          std::vector<std::string>{
              "Example Game [Japan \xC2\xB7 En]",
              "Example Game [Japan \xC2\xB7 Ja]"});

    std::vector<goliath::Game> identicalQualifiers{
        cd("Example Game (Export) (En) (Rev 1)", "300"),
        cd("Example Game (Export) (En) (Rev 1)", "301"),
    };
    CHECK(libraryGameListTitles(identicalQualifiers) ==
          std::vector<std::string>{
              "Example Game [Export \xC2\xB7 Rev 1 \xC2\xB7 En \xC2\xB7 Redump 300]",
              "Example Game [Export \xC2\xB7 Rev 1 \xC2\xB7 En \xC2\xB7 Redump 301]"});
}

TEST_CASE("MVS AES list titles remain untouched even when names repeat",
          "[library-view][neogeo][titles]") {
    goliath::Game first;
    first.system = "neogeo";
    first.name = "Metal Slug (NGM-2010)";
    first.display = "Metal Slug";
    goliath::Game second = first;
    second.short_name = "mslugx";

    CHECK(libraryGameListTitles({first, second}) ==
          std::vector<std::string>{"Metal Slug", "Metal Slug"});
}

TEST_CASE("MVS AES titles retain human names without catalog codes",
          "[library-view]") {
    CHECK(libraryMvsAesHumanTitle(
              "Art of Fighting 2 / Ryuuko no Ken 2 (NGM-056)",
              "Art of Fighting 2") ==
          "Art of Fighting 2 / Ryuuko no Ken 2");
    CHECK(libraryMvsAesHumanTitle(
              "Metal Slug 2 - Super Vehicle-001/II "
              "(NGM-2410 ~ NGH-2410)",
              "Metal Slug 2 - Super Vehicle-001") ==
          "Metal Slug 2 - Super Vehicle-001/II");
    CHECK(libraryMvsAesHumanTitle(
              "Metal Slug (prototype)", "Metal Slug") ==
          "Metal Slug (prototype)");
    CHECK(libraryMvsAesHumanTitle(
              "Aggressors of Dark Kombat / Tsuukai GANGAN Koushinkyoku "
              "(ADM-008 ~ ADH-008)",
              "Aggressors of Dark Kombat") ==
          "Aggressors of Dark Kombat / Tsuukai GANGAN Koushinkyoku");
    CHECK(libraryMvsAesHumanTitle("", "Fallback") == "Fallback");
}

TEST_CASE("MVS AES details use the complete human title",
          "[library-view]") {
    const std::string fullName =
        "Art of Fighting 2 / Ryuuko no Ken 2 (NGM-056)";
    const std::string displayName = "Art of Fighting 2";

    CHECK(libraryDetailsTitle(false, fullName, displayName) ==
          "Art of Fighting 2 / Ryuuko no Ken 2");
}

TEST_CASE("catalog identity presentation distinguishes Redump from MAME",
          "[library-view][catalog-id]") {
    goliath::Game game;
    game.source = "redump";
    game.short_name = "cd_redump_69152";
    game.redump_id = "69152";

    goliath::Rom rom;
    rom.mame = "androdun";

    auto presentation = libraryCatalogIdPresentation(game, &rom);
    CHECK(presentation.label == "Redump ID");
    CHECK(presentation.value == "69152");
    CHECK(presentation.url == "https://redump.info/disc/69152");

    game.redump_id.reset();
    presentation = libraryCatalogIdPresentation(game, &rom);
    CHECK(presentation.label == "Redump ID");
    CHECK(presentation.value.empty());
    CHECK(presentation.url.empty());

    game.redump_id = "test-mslug";
    presentation = libraryCatalogIdPresentation(game, &rom);
    CHECK(presentation.value == "test-mslug");
    CHECK(presentation.url.empty());

    game.source = "mame";
    game.short_name = "androdun";
    presentation = libraryCatalogIdPresentation(game, &rom);
    CHECK(presentation.label == "MAME ID");
    CHECK(presentation.value == "androdun");
    CHECK(presentation.url.empty());

    presentation = libraryCatalogIdPresentation(game, nullptr);
    CHECK(presentation.label == "MAME ID");
    CHECK(presentation.value == "androdun");
}

TEST_CASE("verification presentation distinguishes matches and mismatches",
          "[library-view][verification]") {
    goliath::Game game;
    game.identified = true;
    game.system = "neogeo";

    goliath::Rom rom;
    rom.verification = "geolith-crc32";
    rom.crc32 = "e1a45894";
    rom.hashes.push_back({});

    auto presentation = libraryVerificationPresentation(game, &rom);
    CHECK(presentation.label == "Geolith / CRC-32");
    CHECK(presentation.value == "e1a45894");
    CHECK(presentation.tone == VerificationTone::Success);
    CHECK(presentation.details_available);

    rom.verification = "geolith-crc32-mismatch";
    presentation = libraryVerificationPresentation(game, &rom);
    CHECK(presentation.value == "e1a45894");
    CHECK(presentation.tone == VerificationTone::Error);

    game.system = "neogeocd";
    game.verification = "redump-tracks-only";
    rom.verification = "redump-tracks-only";
    presentation = libraryVerificationPresentation(game, &rom);
    CHECK(presentation.label == "Redump / SHA-1");
    CHECK(presentation.value == "CUE mismatch");
    CHECK(presentation.tone == VerificationTone::Error);

    game.verification = "mame-chd";
    rom.verification = "mame-chd";
    presentation = libraryVerificationPresentation(game, &rom);
    CHECK(presentation.label == "MAME CHD / SHA-1");
    CHECK(presentation.value == "Verified");
    CHECK(presentation.tone == VerificationTone::Success);

    game.verification = "mame-chd-mismatch";
    rom.verification = "mame-chd-mismatch";
    presentation = libraryVerificationPresentation(game, &rom);
    CHECK(presentation.label == "MAME CHD / SHA-1");
    CHECK(presentation.value == "Mismatch");
    CHECK(presentation.tone == VerificationTone::Error);
    CHECK(presentation.details_available);
}
