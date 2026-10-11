#include "ui/library_details_presentation.hpp"

#include <catch2/catch.hpp>

#include <QString>

using goliath::Game;
using goliath::GamePlaytimeRecord;
using goliath::GameProfileSummary;
using goliath::LibraryDetailsSelectionKind;
using goliath::Rom;
using goliath::buildLibraryDetailsPresentation;
using goliath::libraryBinarySizeText;
using goliath::libraryCdVerificationBadge;
using goliath::libraryDetailsRom;
using goliath::libraryVariantFullName;

TEST_CASE("library details presentation formats an MVS variant",
          "[library-view][details-presentation]") {
    Game game;
    game.name = "Metal Slug 2 - Super Vehicle-001/II (NGM-2410)";
    game.display = "Metal Slug 2";
    game.short_name = "mslug2";
    game.source = "mame";
    game.system = "neogeo";
    game.year = "1998";
    game.manufacturer = "SNK";
    game.genre = "Shooter";
    game.players = "2";
    game.history = "Mission start.";

    Rom rom;
    rom.file = "mslug2t.neo";
    rom.mame = "mslug2t";
    rom.name = "Metal Slug 2 Turbo";
    rom.alt_title = "Metal Slug 2 <Turbo>";
    rom.serial = "NGM-2410, NGH-2410";
    rom.release = "19980102, prototype";
    rom.part = "cart";
    rom.interface = "neo_cart";
    rom.program_width = 16;
    rom.program_endianness = "big";
    rom.program_size = 2 * 1024 * 1024;
    rom.fixed_size = 128 * 1024;
    rom.verification = "geolith-crc32";
    rom.crc32 = "e1a45894";
    rom.hashes.push_back({});
    game.roms.push_back(rom);
    game.main_rom = rom.file;

    const GameProfileSummary profile{
        true, "AES / Japan", "Vulkan / Nearest", "Gamepad 1"};
    const GamePlaytimeRecord playtime{
        3725, 7, 1699999700, 1700000000, 300};
    const auto presentation = buildLibraryDetailsPresentation(
        game, 0, rom.file, profile, &playtime);

    CHECK(presentation.title ==
          QStringLiteral("Metal Slug 2 - Super Vehicle-001/II"));
    CHECK(presentation.year == QStringLiteral("1998"));
    CHECK(presentation.manufacturer == QStringLiteral("SNK"));
    CHECK(presentation.alternate_title.visible);
    CHECK(presentation.alternate_title.text ==
          QStringLiteral("Metal Slug 2 <Turbo>"));
    CHECK(presentation.catalog_label == QStringLiteral("MAME ID"));
    CHECK(presentation.catalog_html == QStringLiteral("mslug2t"));
    CHECK(presentation.serial.text ==
          QStringLiteral("NGM-2410\nNGH-2410"));
    CHECK(presentation.release.text ==
          QStringLiteral("1998-01-02\nprototype"));
    CHECK(presentation.media_format.text ==
          QStringLiteral("Cartridge (neo_cart)"));
    CHECK(presentation.media_format.tooltip ==
          QStringLiteral("mslug2t.neo"));
    CHECK(presentation.program_rom.text ==
          QString::fromUtf8("P 2 MiB · S 128 KiB"));
    CHECK(presentation.program_rom.tooltip.contains(
          QStringLiteral("P attributes: 16-bit")));
    CHECK(presentation.verification_html.contains(
          QStringLiteral("e1a45894")));
    CHECK(presentation.verification_details_available);
    CHECK(presentation.profile_status == QStringLiteral("Custom overrides"));
    CHECK(presentation.profile_system == QStringLiteral("AES / Japan"));
    CHECK(presentation.playtime == QStringLiteral("1h 2m"));
    CHECK(presentation.sessions == QStringLiteral("7"));
    REQUIRE(presentation.last_started_epoch.has_value());
    CHECK(*presentation.last_started_epoch == 1699999700);
    REQUIRE(presentation.last_ended_epoch.has_value());
    CHECK(*presentation.last_ended_epoch == 1700000000);
    CHECK(presentation.last_session == QStringLiteral("5m"));
    CHECK(presentation.selection_kind ==
          LibraryDetailsSelectionKind::Variant);
    CHECK(presentation.variant_text ==
          QStringLiteral("Selected variant: Metal Slug 2 Turbo"));
    CHECK(presentation.snapshot_short_name == "mslug2t");
    CHECK(presentation.snapshot_parent_short_name == "mslug2");
    CHECK(presentation.recording_media.empty());
    CHECK(presentation.history == QStringLiteral("Mission start."));
}

TEST_CASE("library details preserves legacy playtime without inventing metadata",
          "[library-view][details-presentation][game_playtime]") {
    Game game;
    game.name = "Legacy Game";
    game.display = "Legacy Game";
    game.short_name = "legacy";
    game.system = "neogeo";

    Rom rom;
    rom.file = "legacy.neo";
    rom.main = true;
    game.roms.push_back(rom);
    game.main_rom = rom.file;

    const GameProfileSummary profile{
        false, "MVS / World", "OpenGL / Linear", "Keyboard"};
    const GamePlaytimeRecord playtime{
        120, 1, 0, 1700000000, 0};
    const auto presentation = buildLibraryDetailsPresentation(
        game, -1, rom.file, profile, &playtime);

    CHECK(presentation.playtime == QStringLiteral("2m"));
    CHECK(presentation.sessions == QStringLiteral("1"));
    CHECK_FALSE(presentation.last_started_epoch.has_value());
    REQUIRE(presentation.last_ended_epoch.has_value());
    CHECK(*presentation.last_ended_epoch == 1700000000);
    CHECK(presentation.last_session == QStringLiteral("-"));
}

TEST_CASE("library details presentation formats a Neo Geo CD parent",
          "[library-view][details-presentation]") {
    Game game;
    game.name = "Samurai Spirits RPG";
    game.display = "Samurai Spirits RPG";
    game.short_name = "ssrpg";
    game.source = "redump";
    game.system = "neogeocd";
    game.identified = true;
    game.verification = "redump-cue";
    game.redump_id = "69152";
    game.snapshot = "database/snaps/ssrpg.png";

    Rom rom;
    rom.file = "Samurai Spirits RPG.cue";
    rom.mame = "ssrpg";
    rom.main = true;
    game.roms.push_back(rom);
    game.main_rom = rom.file;

    const GameProfileSummary profile{
        false, "CD / Japan", "OpenGL / Linear", "Keyboard"};
    const auto presentation = buildLibraryDetailsPresentation(
        game, -1, rom.file, profile, nullptr);

    CHECK(presentation.catalog_label == QStringLiteral("Redump ID"));
    CHECK(presentation.catalog_url ==
          QStringLiteral("https://redump.info/disc/69152"));
    CHECK(presentation.catalog_html.contains(
          QStringLiteral("href=\"https://redump.info/disc/69152\"")));
    CHECK_FALSE(presentation.alternate_title.visible);
    CHECK_FALSE(presentation.serial.visible);
    CHECK_FALSE(presentation.release.visible);
    CHECK_FALSE(presentation.media_format.visible);
    CHECK_FALSE(presentation.program_rom.visible);
    CHECK(presentation.profile_status == QStringLiteral("Global defaults"));
    CHECK(presentation.playtime == QStringLiteral("Not played yet"));
    CHECK(presentation.sessions == QStringLiteral("0"));
    CHECK_FALSE(presentation.last_started_epoch.has_value());
    CHECK_FALSE(presentation.last_ended_epoch.has_value());
    CHECK(presentation.last_session == QStringLiteral("-"));
    CHECK(presentation.selection_kind ==
          LibraryDetailsSelectionKind::Game);
    CHECK(presentation.variant_text ==
          QString::fromUtf8("Verification: ✓ Redump set"));
    CHECK(presentation.snapshot_path ==
          QStringLiteral("database/snaps/ssrpg.png"));
    CHECK(presentation.snapshot_short_name == "ssrpg");
    CHECK(presentation.recording_media == rom.file);
}

TEST_CASE("library details helpers retain fallback behavior",
          "[library-view][details-presentation]") {
    CHECK(libraryBinarySizeText(0) == QStringLiteral("0 bytes"));
    CHECK(libraryBinarySizeText(128 * 1024) == QStringLiteral("128 KiB"));
    CHECK(libraryBinarySizeText(2 * 1024 * 1024) == QStringLiteral("2 MiB"));

    Game game;
    game.system = "neogeo";
    game.main_rom = "main.neo";

    Rom unnamed;
    unnamed.file = "fallback.neo";
    CHECK(libraryVariantFullName(unnamed) ==
          QStringLiteral("fallback.neo"));

    Rom main;
    main.file = "main.neo";
    main.mame = "main";
    main.main = true;
    game.roms = {unnamed, main};

    CHECK(libraryDetailsRom(game, 0) == &game.roms[0]);
    CHECK(libraryDetailsRom(game, -1) == &game.roms[1]);
    CHECK(libraryDetailsRom(game, 99) == &game.roms[1]);
    CHECK(libraryCdVerificationBadge(game).text.isEmpty());

    const GameProfileSummary profile{};
    const auto invalid = buildLibraryDetailsPresentation(
        game, 99, std::nullopt, profile, nullptr);
    CHECK(invalid.selection_kind ==
          LibraryDetailsSelectionKind::InvalidVariant);
}
