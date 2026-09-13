#include "catch2/catch.hpp"

#include "game/db_scanner.hpp"
#include "game/game_model.hpp"
#include "game/neocd_verification.hpp"
#include "game/neogeo_metadata.hpp"
#include "common/goliath_common.hpp"

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

using goliath::Config;
using goliath::CdMatchIndex;
using goliath::RedumpGameEntry;
using goliath::ScanResult;
using goliath::SoftwareCatalog;
using goliath::SoftwareEntry;
using goliath::build_cd_match_index;
using goliath::find_redump_cd_metadata;
using goliath::scan_roms;

namespace {

fs::path make_test_root(const std::string& name) {
    fs::path root =
        fs::temp_directory_path() /
        ("goliath_db_test_" + name);

    std::error_code ec;

    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    return root;
}

void write_file(
    const fs::path& path,
    const std::string& text
) {
    fs::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());

    out << text;
}

// Test-only equivalent of the scanner's Windows extended-length I/O path.
// It lets the regression fixture itself create a >260-character tree before
// asking scan_roms() to discover and verify it.
fs::path test_io_path(const fs::path& path) {
#ifdef _WIN32
    if (path.empty())
        return path;

    const std::wstring existing = path.wstring();
    if (existing.rfind(L"\\\\?\\", 0) == 0)
        return path;

    std::error_code ec;
    fs::path absolute = path.is_absolute() ? path : fs::absolute(path, ec);
    if (ec)
        return path;
    absolute = absolute.lexically_normal();

    const std::wstring native = absolute.wstring();
    if (native.rfind(L"\\\\", 0) == 0)
        return fs::path(std::wstring(L"\\\\?\\UNC\\") + native.substr(2));
    return fs::path(std::wstring(L"\\\\?\\") + native);
#else
    return path;
#endif
}

void write_long_path_file(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(test_io_path(path.parent_path()), ec);
    REQUIRE_FALSE(ec);

    std::ofstream out(test_io_path(path), std::ios::binary);
    REQUIRE(out.good());
    out << text;
}

void write_chd_v5_header(const fs::path& path,
                         const std::string& sha1_hex,
                         const std::string& data_sha1_hex = std::string(40, '0')) {
    REQUIRE(sha1_hex.size() == 40);
    REQUIRE(data_sha1_hex.size() == 40);

    std::vector<unsigned char> header(124, 0);
    const char magic[] = "MComprHD";
    std::copy(magic, magic + 8, header.begin());

    auto put_be32 = [&](std::size_t offset, std::uint32_t value) {
        header[offset + 0] = static_cast<unsigned char>((value >> 24) & 0xff);
        header[offset + 1] = static_cast<unsigned char>((value >> 16) & 0xff);
        header[offset + 2] = static_cast<unsigned char>((value >> 8) & 0xff);
        header[offset + 3] = static_cast<unsigned char>(value & 0xff);
    };
    put_be32(8, 124);
    put_be32(12, 5);

    auto put_sha1 = [&](std::size_t offset, const std::string& hex) {
        auto nibble = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(c - 'A' + 10);
            return 0xff;
        };
        for (std::size_t i = 0; i < 20; ++i) {
            const unsigned char hi = nibble(hex[i * 2]);
            const unsigned char lo = nibble(hex[i * 2 + 1]);
            REQUIRE(hi <= 0x0f);
            REQUIRE(lo <= 0x0f);
            header[offset + i] = static_cast<unsigned char>((hi << 4) | lo);
        }
    };

    // v5: Data SHA1/rawsha1 at 64, chdman SHA1/combined sha1 at 84.
    put_sha1(64, data_sha1_hex);
    put_sha1(84, sha1_hex);

    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char*>(header.data()),
              static_cast<std::streamsize>(header.size()));
}

Config make_config(const fs::path& root) {

    Config cfg;

    cfg.set("Paths", "roms",     (root / "roms").string());
    cfg.set("Paths", "neocd",    (root / "neocd").string());
    cfg.set("Paths", "icons",    (root / "icons").string());
    cfg.set("Paths", "snaps",    (root / "snaps").string());
    cfg.set("Paths", "metadata", (root / "metadata").string());
    cfg.set("Paths", "database", (root / "database").string());
    cfg.set("Paths", "bios",     (root / "bios").string());
    cfg.set("Paths", "config",   (root / "config").string());
    cfg.set("Paths", "jollygood",(root / "jollygood").string());

    return cfg;
}

void create_common_metadata(const fs::path& root) {

    fs::create_directories(root / "roms");
    fs::create_directories(root / "neocd");
    fs::create_directories(root / "metadata");
    fs::create_directories(root / "database");
    fs::create_directories(root / "icons");
    fs::create_directories(root / "snaps");

    write_file(
        root / "metadata/neogeo.xml",
        R"xml(<?xml version="1.0"?>
<softwarelist name="neogeo" description="SNK Neo-Geo cartridges">
    <software name="mslug">
        <description>Metal Slug - Super Vehicle-001</description>
        <year>1996</year>
        <publisher>Nazca</publisher>
        <info name="serial" value="NGM-201 (MVS), NGH-201 (AES)"/>
        <info name="release" value="19960419 (MVS), 19960524 (AES)"/>
        <sharedfeat name="release" value="MVS,AES"/>
        <sharedfeat name="compatibility" value="MVS,AES"/>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="msluga" cloneof="mslug">
        <description>Metal Slug (prototype)</description>
        <year>1996</year>
        <publisher>Nazca</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="mslug2">
        <description>Metal Slug 2 - Super Vehicle-001/II (NGM-2410 ~ NGH-2410)</description>
        <year>1998</year>
        <publisher>SNK</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="kof98">
        <description>The King of Fighters '98</description>
        <year>1998</year>
        <publisher>SNK</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="sonicwi2">
        <description>Aero Fighters 2 / Sonic Wings 2</description>
        <year>1994</year>
        <publisher>Video System Co.</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="roboarmy">
        <description>Robo Army</description>
        <year>1991</year>
        <publisher>SNK</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="roboarmya">
        <description>Robo Army (NGM-032 ~ NGH-032)</description>
        <year>1991</year>
        <publisher>SNK</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="xenocris">
        <description>Xeno Crisis</description>
        <year>2021</year>
        <publisher>Bitmap Bureau</publisher>
        <info name="serial" value="NGM-BB01 (MVS), NGH-BB01 (AES)"/>
        <sharedfeat name="release" value="MVS,AES"/>
        <sharedfeat name="compatibility" value="MVS,AES"/>
        <part name="cart" interface="neo_cart"/>
    </software>
</softwarelist>
)xml"
    );

    // Same shortname as the cartridge entry on purpose: catalogs must stay separate.
    write_file(
        root / "metadata/neocd.xml",
        R"xml(<?xml version="1.0"?>
<softwarelist name="neocd" description="SNK NeoGeo CD CD-ROMs">
    <software name="mslug">
        <description>Metal Slug (Japan, USA)</description>
        <year>1996</year>
        <publisher>SNK</publisher>
        <info name="serial" value="NGCD-201 (JPN)"/>
        <part name="cdrom" interface="cdrom">
            <diskarea name="cdrom">
                <disk name="metal slug (1996)(snk)(jp-us)" sha1="b4f83b0b7046e9445f9cc16c40e57fd84b575ef9"/>
            </diskarea>
        </part>
    </software>
    <software name="bbbuster">
        <description>Bang Bang Busters</description>
        <year>2011</year>
        <publisher>Neo Conception International</publisher>
        <part name="cdrom" interface="cdrom">
            <diskarea name="cdrom">
                <disk name="Bang Bang Busters (France) (En,Ja) (Unl)" sha1="2112eca62613c4ebaff3a62334b6395009c05c37"/>
            </diskarea>
        </part>
    </software>
    <software name="ssideki2">
        <description>Super Sidekicks 2 (USA) ~ Tokuten Ou 2 (Japan)</description>
        <year>1994</year>
        <publisher>SNK</publisher>
        <part name="cdrom" interface="cdrom">
            <diskarea name="cdrom">
                <disk name="super sidekicks 2 (1994)(snk)(jp-us)[tokuten ou 2]" sha1="ef2a5fee5502561d25922aad1656319de18c72a0"/>
            </diskarea>
        </part>
    </software>
</softwarelist>
)xml"
    );

write_file(
    root / "metadata/history.xml",

    R"(<?xml version="1.0"?>
<history>

    <entry>
        <systems>
            <system name="mslug" />
        </systems>

        <software>
            <item list="saturn" name="mslug" />
        </software>

        <text>
Saturn version - WRONG
        </text>
    </entry>

    <entry>
        <systems>
            <system name="something_else" />
        </systems>

        <software>
            <item list="neogeo" name="mslug" />
        </software>

        <text>
Neo Geo version - CORRECT
        </text>
    </entry>

    <entry>
        <software>
            <item list="neocd" name="mslug" />
        </software>

        <text>
Neo Geo CD version - WRONG FOR CARTRIDGE
        </text>
    </entry>

    <entry>
        <systems>
            <system name="mslug2t" />
        </systems>
        <text>
Metal Slug 2 Turbo machine-only history
        </text>
    </entry>

    <entry>
        <systems>
            <system name="vliner" />
            <system name="vliner53" />
        </systems>
        <text>
V-Liner machine-only history
        </text>
    </entry>

</history>
)"
);


    write_file(
        root / "metadata/catver.ini",
        R"([Category]
mslug=Platform / Shooter Scrolling
msluga=Platform / Shooter Scrolling
kof98=Fighter / Versus
)"
    );

    write_file(
        root / "metadata/catlist.ini",
        R"([Arcade: Platform / Shooter Scrolling]
mslug
msluga

[Arcade: Fighter / Versus]
kof98
)"
    );

    write_file(
        root / "metadata/nplayers.ini",
        R"(mslug=2P sim
msluga=2P sim
kof98=2P sim
)"
    );

    write_file(
        root / "metadata/series.ini",
        R"([Metal Slug]
mslug
msluga

[King of Fighters]
kof98
)"
    );

    write_file(
        root / "metadata/genre.ini",
        R"([Shooter]
mslug
msluga

[Fighter]
kof98
)"
    );
 }

void create_redump_neocd_dat(const fs::path& root) {
    write_file(
        root / "metadata/SNK - Neo Geo CD - Datfile (test).dat",
        R"xml(<?xml version="1.0"?>
<datafile>
    <header>
        <name>SNK - Neo Geo CD</name>
        <description>SNK - Neo Geo CD - Datfile (test)</description>
        <version>test</version>
        <author>redump.info</author>
    </header>
    <game name="Andro Dunos (France) (Unl)" id="69152">
        <category>Games</category>
        <description>Andro Dunos (France) (Unl)</description>
        <rom name="Andro Dunos (France) (Unl).cue" size="67"
             crc="ce6ee568" md5="10f6d88259cbead63a08a845db6e682e"
             sha1="71c9c43aede9cb375e70813f187c2dfc8652403d"/>
        <rom name="Andro Dunos (France) (Unl).bin" size="3"
             crc="352441c2" md5="900150983cd24fb0d6963f7d28e17f72"
             sha1="a9993e364706816aba3e25717850c26c9cd0d89d"/>
    </game>
    <game name="Metal Slug (Japan) (En,Ja)" id="test-mslug">
        <category>Games</category>
        <description>Metal Slug (Japan) (En,Ja)</description>
        <rom name="Metal Slug (Japan) (En,Ja).cue" size="78"
             crc="00000000" md5="00000000000000000000000000000000"
             sha1="0c4f96e76bff6abc620257cf005ea22b3c0c9892"/>
        <rom name="Metal Slug (Japan) (En,Ja) (Track 01).bin" size="11"
             crc="11111111" md5="11111111111111111111111111111111"
             sha1="f508d4f5a7d9793e3390d95f40ecac984691f31b"/>
    </game>
</datafile>
)xml"
    );
}

} // namespace

TEST_CASE("Redump layout index preserves ordered track layouts and catalog order",
          "[db][neocd][redump][index]") {
    const auto make_game = [](const std::string& name,
                              std::initializer_list<std::uintmax_t> sizes) {
        goliath::RedumpGameEntry game;
        game.name = name;
        game.description = name;

        goliath::RedumpRomEntry cue;
        cue.name = name + ".CUE";
        cue.size = 999;
        game.roms.push_back(std::move(cue));

        std::size_t track = 0;
        for (const std::uintmax_t size : sizes) {
            goliath::RedumpRomEntry rom;
            rom.name = name + " track " + std::to_string(++track) + ".bin";
            rom.size = size;
            game.roms.push_back(std::move(rom));
        }
        return game;
    };

    goliath::RedumpCatalog catalog;
    catalog.push_back(make_game("first", {10, 20}));
    catalog.push_back(make_game("reversed", {20, 10}));
    catalog.push_back(make_game("duplicate", {10, 20}));
    catalog.push_back(make_game("cue-only", {}));

    const goliath::RedumpLayoutIndex index(catalog);
    using TrackLayout = goliath::RedumpLayoutIndex::TrackLayout;

    const auto* shared = index.candidates_for(TrackLayout{10, 20});
    REQUIRE(shared != nullptr);
    const std::vector<std::size_t> shared_expected{0, 2};
    REQUIRE(*shared == shared_expected);

    const auto* reversed = index.candidates_for(TrackLayout{20, 10});
    REQUIRE(reversed != nullptr);
    const std::vector<std::size_t> reversed_expected{1};
    REQUIRE(*reversed == reversed_expected);

    REQUIRE(index.candidates_for(TrackLayout{10}) == nullptr);
    REQUIRE(index.candidates_for(TrackLayout{}) == nullptr);
}

TEST_CASE("db scanner groups parent and clone", "[db][scanner]") {

    fs::path root =
        make_test_root("parent_clone");

    create_common_metadata(root);

    write_file(root / "roms/mslug.neo", "");
    write_file(root / "roms/msluga.neo", "");

    Config cfg = make_config(root);

    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 2);
    REQUIRE(result.game_count == 1);
    REQUIRE(result.parent_games == 1);
    REQUIRE(result.variant_count == 1);
    REQUIRE(result.homebrew_games == 0);

    fs::remove_all(root);
}

TEST_CASE("db scanner cancellation preserves the existing database",
          "[db][scanner][cancel]") {
    const fs::path root = make_test_root("cancel_preserves_database");
    create_common_metadata(root);
    write_file(root / "roms/mslug.neo", "");

    const fs::path database = root / "database/games.json";
    const std::string existing = "existing database must remain untouched\n";
    write_file(database, existing);

    std::atomic<bool> cancel{false};
    const ScanResult result = scan_roms(
        make_config(root),
        [&cancel](const std::string& line) {
            if (line.find("Neo Geo CD image files found:") != std::string::npos)
                cancel.store(true, std::memory_order_release);
        },
        &cancel);

    CHECK_FALSE(result.success);
    CHECK(result.error_message == "Scan canceled.");

    std::ifstream in(database, std::ios::binary);
    const std::string current(std::istreambuf_iterator<char>(in), {});
    CHECK(current == existing);
    in.close();

    fs::remove_all(root);
}

TEST_CASE("db scanner reports an unusable database destination",
          "[db][scanner][persistence]") {
    const fs::path root = make_test_root("database_destination_failure");
    create_common_metadata(root);
    write_file(root / "roms/mslug.neo", "");

    const fs::path blocker = root / "database-blocker";
    const std::string existing = "ordinary file, not a database directory\n";
    write_file(blocker, existing);

    Config config = make_config(root);
    config.set("Paths", "database", blocker.string());
    const ScanResult result = scan_roms(config);

    CHECK_FALSE(result.success);
    CHECK(result.error_message.find("Could not create database directory:") == 0);

    std::ifstream in(blocker, std::ios::binary);
    REQUIRE(in.good());
    const std::string current(std::istreambuf_iterator<char>(in), {});
    CHECK(current == existing);
    in.close();

    REQUIRE(fs::remove(blocker));
    const ScanResult retry = scan_roms(config);
    REQUIRE(retry.success);
    CHECK(retry.error_message.empty());
    CHECK(retry.rom_file_count == 1);
    CHECK(retry.game_count == 1);

    const fs::path database = blocker / "games.json";
    REQUIRE(fs::is_regular_file(database));
    std::ifstream recovered(database, std::ios::binary);
    const std::string recoveredText(
        std::istreambuf_iterator<char>(recovered), {});
    CHECK(recoveredText.find("\"file\": \"mslug.neo\"") !=
          std::string::npos);
    recovered.close();

    fs::remove_all(root);
}

TEST_CASE("db scanner selects parent as main_rom", "[db][scanner]") {

    fs::path root =
        make_test_root("main_rom");

    create_common_metadata(root);

    write_file(root / "roms/mslug.neo", "");
    write_file(root / "roms/msluga.neo", "");

    Config cfg = make_config(root);

    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);

    fs::path json_path =
        root / "database/games.json";

    REQUIRE(fs::exists(json_path));

    std::ifstream in(json_path);
    std::string json_text(
        std::istreambuf_iterator<char>(in),
        {}
    );

    REQUIRE(
        json_text.find("\"main_rom\": \"mslug.neo\"")
        != std::string::npos
    );
    in.close();
    fs::remove_all(root);
}

TEST_CASE("db scanner uses parent metadata when clone sorts first", "[db][scanner][parent][clone]") {
    fs::path root = make_test_root("clone_before_parent_metadata");
    create_common_metadata(root);

    write_file(
        root / "metadata/neogeo.xml",
        R"xml(<?xml version="1.0"?>
<softwarelist name="neogeo" description="SNK Neo-Geo cartridges">
    <software name="zparent">
        <description>Canonical Parent</description>
        <year>1999</year>
        <publisher>Parent Corp</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
    <software name="aclone" cloneof="zparent">
        <description>Clone Seen First (bootleg of Canonical Parent)</description>
        <year>2004</year>
        <publisher>bootleg</publisher>
        <part name="cart" interface="neo_cart"/>
    </software>
</softwarelist>
)xml"
    );

    // Alphabetical discovery sees aclone.neo before zparent.neo.
    write_file(root / "roms/aclone.neo", "");
    write_file(root / "roms/zparent.neo", "");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.game_count == 1);
    REQUIRE(result.parent_games == 1);
    REQUIRE(result.variant_count == 1);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});

    REQUIRE(text.find("\"name\": \"Canonical Parent\"") != std::string::npos);
    REQUIRE(text.find("\"display\": \"Canonical Parent\"") != std::string::npos);
    REQUIRE(text.find("\"short\": \"zparent\"") != std::string::npos);
    REQUIRE(text.find("\"manufacturer\": \"Parent Corp\"") != std::string::npos);
    REQUIRE(text.find("\"main_rom\": \"zparent.neo\"") != std::string::npos);
    REQUIRE(text.find("\"file\": \"aclone.neo\"") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("db scanner detects homebrew", "[db][scanner]") {

    fs::path root =
        make_test_root("homebrew");

    create_common_metadata(root);

    write_file(
        root / "roms/xenocrisis.neo",
        ""
    );
    write_file(
        root / "metadata/catver.ini",
        R"([Category]
mslug=Platform / Shooter Scrolling
xenocrisis=Homebrew / Local metadata
)"
    );

    Config cfg = make_config(root);

    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 1);
    REQUIRE(result.game_count == 1);
    REQUIRE(result.homebrew_games == 1);
    REQUIRE(result.parent_games == 1);

    std::ifstream in(root / "database/games.json");
    const std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"genre\": \"Homebrew / Local metadata\"") !=
            std::string::npos);
    in.close();

    fs::remove_all(root);
}

TEST_CASE("db scanner keeps known homebrew classification while using software-list metadata", "[db][metadata][homebrew]") {
    fs::path root = make_test_root("known_homebrew_metadata");
    create_common_metadata(root);

    write_file(root / "roms/xenocris.neo", "");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 1);
    REQUIRE(result.game_count == 1);
    REQUIRE(result.homebrew_games == 1);
    REQUIRE(result.parent_games == 1);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});

    REQUIRE(text.find("\"short\": \"xenocris\"") != std::string::npos);
    REQUIRE(text.find("\"source\": \"homebrew\"") != std::string::npos);
    REQUIRE(text.find("\"name\": \"Xeno Crisis\"") != std::string::npos);
    REQUIRE(text.find("\"year\": \"2021\"") != std::string::npos);
    REQUIRE(text.find("\"manufacturer\": \"Bitmap Bureau\"") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("db scanner imports metadata", "[db][scanner]") {

    fs::path root =
        make_test_root("metadata");

    create_common_metadata(root);

    write_file(root / "roms/mslug.neo", "");

    Config cfg = make_config(root);

    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);

    std::ifstream in(
        root / "database/games.json"
    );

    std::string text(
        std::istreambuf_iterator<char>(in),
        {}
    );

    REQUIRE(
        text.find(
            "\"genre\": \"Platform / Shooter Scrolling\""
        ) != std::string::npos
    );

    REQUIRE(
        text.find(
            "\"players\": \"2P sim\""
        ) != std::string::npos
    );

    REQUIRE(
        text.find(
            "\"series\": \"Metal Slug\""
        ) != std::string::npos
    );
    in.close();
    fs::remove_all(root);
}

TEST_CASE("Neo Geo CD revisions inherit only missing parent metadata",
          "[db][neocd][metadata][clone]") {
    const fs::path root = make_test_root("neocd_clone_metadata");
    create_common_metadata(root);

    write_file(
        root / "metadata/neocd.xml",
        R"xml(<?xml version="1.0"?>
<softwarelist name="neocd" description="SNK NeoGeo CD CD-ROMs">
    <software name="cdparent">
        <description>Parent CD Release (Japan)</description>
        <year>1995</year>
        <publisher>Parent Publisher</publisher>
        <part name="cdrom" interface="cdrom">
            <diskarea name="cdrom">
                <disk name="parent cd release" sha1="1111111111111111111111111111111111111111"/>
            </diskarea>
        </part>
    </software>
    <software name="cdrevision" cloneof="cdparent">
        <description>Complete Revision Name (USA, Rev 2)</description>
        <year>1996</year>
        <publisher>Revision Publisher</publisher>
        <part name="cdrom" interface="cdrom">
            <diskarea name="cdrom">
                <disk name="revision cd image" sha1="2222222222222222222222222222222222222222"/>
            </diskarea>
        </part>
    </software>
</softwarelist>
)xml");

    write_file(
        root / "metadata/catver.ini",
        R"([Category]
cdparent=Parent Fighter Category
)");
    write_file(
        root / "metadata/nplayers.ini",
        R"(cdparent=2P sim
cdrevision=1P
)");
    write_file(
        root / "metadata/series.ini",
        R"([Parent Series]
cdparent
)");
    write_file(
        root / "metadata/history.xml",
        R"xml(<?xml version="1.0"?>
<history>
    <entry>
        <software>
            <item list="neocd" name="cdparent" />
        </software>
        <text>Parent CD history - must not replace revision history</text>
    </entry>
    <entry>
        <software>
            <item list="neocd" name="cdrevision" />
        </software>
        <text>Revision-specific CD history</text>
    </entry>
</history>
)xml");
    write_file(root / "neocd/revision cd image.chd", "fake chd");

    const Config cfg = make_config(root);
    const ScanResult result = scan_roms(cfg);
    REQUIRE(result.success);

    const std::vector<goliath::Game> games =
        goliath::load_games(root / "database/games.json");
    REQUIRE(games.size() == 1);
    const goliath::Game& revision = games.front();

    CHECK(revision.short_name == "cdrevision");
    CHECK(revision.name == "Complete Revision Name (USA, Rev 2)");
    CHECK(revision.year == std::optional<std::string>("1996"));
    CHECK(revision.manufacturer ==
          std::optional<std::string>("Revision Publisher"));
    CHECK(revision.genre ==
          std::optional<std::string>("Parent Fighter Category"));
    CHECK(revision.players == std::optional<std::string>("1P"));
    CHECK(revision.series == std::optional<std::string>("Parent Series"));
    CHECK(revision.history ==
          std::optional<std::string>("Revision-specific CD history"));
    REQUIRE(revision.roms.size() == 1);
    CHECK(revision.roms.front().cloneof ==
          std::optional<std::string>("cdparent"));

    fs::remove_all(root);
}

TEST_CASE("history uses only Neo Geo entries", "[db][history]") {

    fs::path root =
        make_test_root("history");

    create_common_metadata(root);

    write_file(root / "roms/mslug.neo", "");

    Config cfg = make_config(root);

    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);

    std::ifstream in(
        root / "database/games.json"
    );

    std::string text(
        std::istreambuf_iterator<char>(in),
        {}
    );

    REQUIRE(
        text.find("Neo Geo version - CORRECT")
        != std::string::npos
    );

    REQUIRE(
        text.find("Saturn version - WRONG")
        == std::string::npos
    );

    REQUIRE(
        text.find("Neo Geo CD version - WRONG FOR CARTRIDGE")
        == std::string::npos
    );
    REQUIRE(
        text.find("\"manufacturer\": \"Nazca\"")
        != std::string::npos
    );

    REQUIRE(
        text.find("\"developer\": null")
        != std::string::npos
    );

    REQUIRE(
        text.find("\"publisher\": null")
        != std::string::npos
    );
    in.close();
    fs::remove_all(root);
}

TEST_CASE("selective history parsing preserves Neo Geo catalog semantics",
          "[db][history][metadata]") {
    const fs::path root = make_test_root("history_selective");

    write_file(
        root / "metadata/history.xml",
        R"xml(<?xml version="1.0"?>
<history>
    <entry>
        <software>
            <item list="saturn" name="irrelevant" />
        </software>
        <text>
Irrelevant non-Neo-Geo history text
        </text>
    </entry>
    <entry>
        <systems>
            <system name="vliner" />
            <system name="not_whitelisted" />
        </systems>
        <software>
            <item list="neogeo" name="shared" />
            <item list="neocd" name="shared" />
        </software>
        <text>
First relevant text


Second paragraph
        </text>
    </entry>
    <entry>
        <software>
            <item list="neogeo" name="shared" />
        </software>
        <text>
Later duplicate must not replace the first entry
        </text>
    </entry>
    <entry>
        <software>
            <item list="neogeo" name="missing_text" />
        </software>
    </entry>
</history>
)xml");

    std::string progress;
    const goliath::SupplementalMetadata metadata =
        goliath::load_supplemental_metadata(
            root / "metadata", [&](const std::string& line) {
                progress += line;
            });

    const std::string expected =
        "First relevant text\n\nSecond paragraph";
    REQUIRE(metadata.history.neogeo.size() == 2);
    REQUIRE(metadata.history.neocd.size() == 1);
    REQUIRE(metadata.history.neogeo.at("shared") == expected);
    REQUIRE(metadata.history.neogeo.at("vliner") == expected);
    REQUIRE(metadata.history.neocd.at("shared") == expected);
    REQUIRE(metadata.history.neogeo.count("not_whitelisted") == 0);
    REQUIRE(metadata.history.neogeo.count("missing_text") == 0);
    REQUIRE(progress.find("History entries (Neo Geo): 2") !=
            std::string::npos);
    REQUIRE(progress.find("History entries (Neo Geo CD): 1") !=
            std::string::npos);

    fs::remove_all(root);
}

TEST_CASE("selective supplemental INI parsing preserves relevant game semantics",
          "[db][metadata][ini]") {
    const fs::path root = make_test_root("supplemental_ini_selective");

    write_file(
        root / "metadata/catver.ini",
        R"([Other]
catalog=Wrong section
[Category]
catalog=First category
irrelevant=Unused category
homebrew=Local category
catalog=Final category
)"
    );
    write_file(
        root / "metadata/catlist.ini",
        R"(RootFolder = Category
[ROOT_FOLDER]
ignored_root
[Arcade]
catalog
irrelevant
[Local]
homebrew
[Override]
catalog
)"
    );
    write_file(
        root / "metadata/nplayers.ini",
        R"([Players]
catalog=1
irrelevant=4
[Other]
homebrew=2
catalog=3
)"
    );
    write_file(
        root / "metadata/series.ini",
        R"(RootFolder = Series
[ROOT_FOLDER]
ignored_root
[Series A]
catalog
irrelevant
[Local Series]
homebrew
[Final Series]
catalog
)"
    );
    write_file(
        root / "metadata/genre.ini",
        R"(RootFolder = Genre
[ROOT_FOLDER]
ignored_root
[Action]
catalog
irrelevant
[Local Genre]
homebrew
[Final Genre]
catalog
)"
    );
    write_file(
        root / "metadata/history.xml",
        R"xml(<?xml version="1.0"?>
<history>
    <entry>
        <software>
            <item list="neogeo" name="catalog" />
            <item list="neogeo" name="irrelevant" />
        </software>
        <text>History remains independently selective.</text>
    </entry>
</history>
)xml"
    );

    const goliath::SupplementalMetadata full =
        goliath::load_supplemental_metadata(root / "metadata");
    const std::unordered_set<std::string> relevant_ids = {
        "catalog", "homebrew",
    };
    const goliath::SupplementalMetadata selective =
        goliath::load_supplemental_metadata(root / "metadata", relevant_ids);

    REQUIRE(full.catver.size() == 3);
    REQUIRE(full.catlist.size() == 3);
    REQUIRE(full.players.size() == 3);
    REQUIRE(full.series.size() == 3);
    REQUIRE(full.genre.size() == 3);

    REQUIRE(selective.catver.size() == 2);
    REQUIRE(selective.catlist.size() == 2);
    REQUIRE(selective.players.size() == 2);
    REQUIRE(selective.series.size() == 2);
    REQUIRE(selective.genre.size() == 2);

    REQUIRE(selective.catver.count("irrelevant") == 0);
    REQUIRE(selective.catlist.count("irrelevant") == 0);
    REQUIRE(selective.players.count("irrelevant") == 0);
    REQUIRE(selective.series.count("irrelevant") == 0);
    REQUIRE(selective.genre.count("irrelevant") == 0);

    REQUIRE(selective.catver.at("catalog") == "Final category");
    REQUIRE(selective.catver.at("homebrew") == "Local category");
    REQUIRE(selective.catlist.at("catalog") == "Override");
    REQUIRE(selective.catlist.at("homebrew") == "Local");
    REQUIRE(selective.players.at("catalog") == "3");
    REQUIRE(selective.players.at("homebrew") == "2");
    REQUIRE(selective.series.at("catalog") == "Final Series");
    REQUIRE(selective.series.at("homebrew") == "Local Series");
    REQUIRE(selective.genre.at("catalog") == "Final Genre");
    REQUIRE(selective.genre.at("homebrew") == "Local Genre");

    REQUIRE(selective.history.neogeo == full.history.neogeo);
    REQUIRE(selective.history.neocd == full.history.neocd);
    REQUIRE(selective.history.neogeo.size() == 2);

    fs::remove_all(root);
}

TEST_CASE("db scanner loads dedicated Neo Geo metadata catalogs", "[db][metadata]") {
    fs::path root = make_test_root("software_catalogs");
    create_common_metadata(root);
    write_file(root / "roms/mslug.neo", "");

    std::string progress;
    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg, [&](const std::string& line) {
        progress += line;
    });

    REQUIRE(result.success);
    REQUIRE(progress.find("Neo Geo cartridge software entries: 8") != std::string::npos);
    REQUIRE(progress.find("Neo Geo CD software entries: 3") != std::string::npos);
    REQUIRE(progress.find("History entries (Neo Geo): 4") != std::string::npos);
    REQUIRE(progress.find("History entries (Neo Geo CD): 1") != std::string::npos);

    fs::remove_all(root);
}

TEST_CASE("Neo Geo CD matching retains bracketed MAME title aliases",
          "[db][neocd][metadata][alias]") {
    goliath::SoftwareCatalog catalog;

    goliath::SoftwareEntry last_blade;
    last_blade.media = goliath::NeoGeoMedia::CD;
    last_blade.description = "Bakumatsu Roman - Gekka no Kenshi (Japan)";
    last_blade.disk_name =
        "last blade, the (1998)(snk)(jp)[bakumatsu roman - gekka no kenshi]";
    catalog.emplace("lastblad", std::move(last_blade));

    goliath::SoftwareEntry last_blade_2;
    last_blade_2.media = goliath::NeoGeoMedia::CD;
    last_blade_2.description =
        "Bakumatsu Roman Daini Maku - Gekka no Kenshi - Tsuki ni Saku Hana, "
        "Chiri Yuku Hana (Japan)";
    last_blade_2.disk_name =
        "bakumatsu roman daini maku - gekka no kenshi - tsuki ni saku hana, "
        "chiri yuku hana (1999)(snk)(jp)[last blade 2, the]";
    catalog.emplace("lastbld2", std::move(last_blade_2));

    const goliath::CdMatchIndex index = goliath::build_cd_match_index(catalog);
    const auto match = goliath::find_cd_game(
        fs::path("Bakumatsu Roman Dainimaku - Gekka no Kenshi - Tsuki ni Saku "
                 "Hana, Chiri Yuku Hana ~ The Last Blade 2 (Japan) "
                 "(En,Ja,Es,Pt).cue"),
        index);

    REQUIRE(match.has_value());
    REQUIRE(*match == "lastbld2");

    const auto original_match = goliath::find_cd_game(
        fs::path("Bakumatsu Roman - Gekka no Kenshi ~ The Last Blade "
                 "(Japan) (En,Ja,Es,Pt).cue"),
        index);
    REQUIRE(original_match.has_value());
    REQUIRE(*original_match == "lastblad");
}

TEST_CASE("Redump metadata bridge resolves stable IDs and title matches",
          "[db][neocd][redump][metadata]") {
    SoftwareCatalog catalog;
    for (const std::string short_name : {
             "doubledr", "fatfury3", "kof94", "kof95", "kof96",
             "mahretsu", "neodrift", "stakwin"}) {
        SoftwareEntry entry;
        entry.description = short_name;
        catalog.emplace(short_name, std::move(entry));
    }

    SoftwareEntry metal_slug;
    metal_slug.description = "Metal Slug (Japan, USA)";
    catalog.emplace("mslug", std::move(metal_slug));
    const CdMatchIndex index = build_cd_match_index(catalog);

    const std::vector<std::pair<std::string, std::string>> overrides = {
        {"32460", "doubledr"}, {"30908", "doubledr"},
        {"49698", "fatfury3"}, {"32484", "fatfury3"},
        {"32481", "fatfury3"}, {"49536", "fatfury3"},
        {"32226", "kof94"}, {"24407", "kof94"},
        {"59077", "kof95"}, {"24412", "kof95"},
        {"24413", "kof96"}, {"63438", "mahretsu"},
        {"75662", "neodrift"}, {"59256", "stakwin"},
    };

    for (const auto& [redump_id, expected_short] : overrides) {
        RedumpGameEntry redump;
        redump.id = redump_id;
        redump.name = "Title deliberately unrelated to MAME";
        CHECK(find_redump_cd_metadata(redump, index, catalog) ==
              std::optional<std::string>(expected_short));
    }

    RedumpGameEntry automatic;
    automatic.id = "future-id";
    automatic.name = "Metal Slug (Japan) (En,Ja)";
    CHECK(find_redump_cd_metadata(automatic, index, catalog) ==
          std::optional<std::string>("mslug"));

    RedumpGameEntry unmatched;
    unmatched.id = "69152";
    unmatched.name = "Andro Dunos (France) (Unl)";
    CHECK_FALSE(find_redump_cd_metadata(unmatched, index, catalog).has_value());
}

TEST_CASE("db scanner uses catlist as category fallback", "[db][metadata]") {
    fs::path root = make_test_root("catlist_fallback");
    create_common_metadata(root);

    write_file(
        root / "metadata/catver.ini",
        R"([Category]
kof98=Fighter / Versus
)"
    );
    write_file(root / "roms/mslug.neo", "");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);
    REQUIRE(result.success);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"genre\": \"Arcade: Platform / Shooter Scrolling\"") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("db scanner no longer requires mame.xml", "[db][metadata][migration]") {
    fs::path root = make_test_root("no_mame_xml");
    create_common_metadata(root);
    REQUIRE_FALSE(fs::exists(root / "metadata/mame.xml"));

    write_file(root / "roms/mslug.neo", "");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 1);
    REQUIRE(result.game_count == 1);
    REQUIRE(result.homebrew_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"short\": \"mslug\"") != std::string::npos);
    REQUIRE(text.find("\"system\": \"neogeo\"") != std::string::npos);
    REQUIRE(text.find("Metal Slug - Super Vehicle-001") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("db scanner normalizes software-list description separators", "[db][metadata][migration]") {
    fs::path root = make_test_root("software_description_separator");
    create_common_metadata(root);

    write_file(root / "roms/Aero Fighters 2 - Sonic Wings 2.neo", "");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 1);
    REQUIRE(result.game_count == 1);
    REQUIRE(result.homebrew_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"short\": \"sonicwi2\"") != std::string::npos);
    REQUIRE(text.find("\"mame\": \"sonicwi2\"") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("db scanner preserves cartridge compatibility grouping without mame.xml", "[db][metadata][migration]") {
    fs::path root = make_test_root("compatibility_grouping");
    create_common_metadata(root);

    write_file(root / "roms/mslug2.neo", "");
    write_file(root / "roms/mslug2t.neo", "");
    write_file(root / "roms/roboarmy.neo", "");
    write_file(root / "roms/roboarmya.neo", "");
    write_file(root / "roms/vliner.neo", "");
    write_file(root / "roms/vliner53.neo", "");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 6);
    REQUIRE(result.game_count == 3);
    REQUIRE(result.parent_games == 3);
    REQUIRE(result.variant_count == 3);
    REQUIRE(result.homebrew_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"mame\": \"mslug2t\"") != std::string::npos);
    REQUIRE(text.find(
        "\"name\": \"Metal Slug 2 Turbo (NGM-9410) (hack)\"") !=
        std::string::npos);
    REQUIRE(text.find("\"label\": \"NGM-9410 (hack)\"") !=
            std::string::npos);
    REQUIRE(text.find("\"mame\": \"roboarmya\"") != std::string::npos);
    REQUIRE(text.find("\"mame\": \"vliner53\"") != std::string::npos);
    REQUIRE(text.find("V-Liner machine-only history") != std::string::npos);

    in.close();
    fs::remove_all(root);
}


TEST_CASE("db scanner loads the local Redump Neo Geo CD DAT without changing Stage 3B identity", "[db][neocd][redump]") {
    fs::path root = make_test_root("redump_dat_foundation");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    // Stage 3C foundation only: the DAT is loaded, but Stage 3B matching is
    // still authoritative until content verification is added separately.
    write_file(root / "neocd/Andro Dunos (France) (Unl).cue", "FILE Andro.bin BINARY\n");
    write_file(root / "neocd/Andro Dunos (France) (Unl).bin", "ignored");

    std::string progress;
    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg, [&](const std::string& line) {
        progress += line;
    });

    REQUIRE(result.success);
    REQUIRE(progress.find("Redump Neo Geo CD entries: 2") != std::string::npos);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 0);
    REQUIRE(result.cd_unknown_games == 1);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"source\": \"unknown\"") != std::string::npos);
    REQUIRE(text.find("Andro Dunos") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("complete Redump set verification requires exact CUE and tracks", "[db][neocd][redump][cue]") {
    fs::path root = make_test_root("redump_complete_set");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    write_file(root / "neocd/Andro Dunos (France) (Unl).cue",
               "FILE \"Andro Dunos (France) (Unl).bin\" BINARY\n"
               "  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/Andro Dunos (France) (Unl).bin", "abc");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_redump_cue_verified_games == 1);
    REQUIRE(result.cd_redump_tracks_only_games == 0);
    REQUIRE(result.cd_metadata_only_games == 0);
    REQUIRE(result.cd_unknown_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"source\": \"redump\"") != std::string::npos);
    REQUIRE(text.find("\"verification\": \"redump-cue\"") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("Redump track identity preserves metadata without complete-set status",
          "[db][neocd][redump][cue]") {
    fs::path root = make_test_root("redump_cue_content_only");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    // Local names are deliberately unrelated to Redump. The physical track
    // content alone must identify the disc.
    write_file(root / "neocd/Mystery Disc.cue",
               "FILE \"renamed.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/renamed.bin", "abc");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_redump_cue_verified_games == 0);
    REQUIRE(result.cd_redump_tracks_only_games == 1);
    REQUIRE(result.cd_metadata_only_games == 0);
    REQUIRE(result.cd_unknown_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"source\": \"redump\"") != std::string::npos);
    REQUIRE(text.find("\"verification\": \"redump-tracks-only\"") != std::string::npos);
    REQUIRE(text.find("Andro Dunos (France) (Unl)") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("Redump CUE badge is never granted by filename alone", "[db][neocd][redump][cue]") {
    fs::path root = make_test_root("redump_cue_wrong_hash");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    // The title matches MAME/Redump and the byte size matches the DAT, but the
    // SHA-1 does not. Stage 3B metadata identity remains valid; verification
    // must stay empty.
    write_file(root / "neocd/Metal Slug (Japan) (En,Ja).cue",
               "FILE \"Metal Slug (Japan) (En,Ja) (Track 01).bin\" BINARY\n"
               "  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/Metal Slug (Japan) (En,Ja) (Track 01).bin", "wrong-track");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_redump_cue_verified_games == 0);
    REQUIRE(result.cd_redump_tracks_only_games == 0);
    REQUIRE(result.cd_metadata_only_games == 1);
    REQUIRE(result.cd_unknown_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"short\": \"mslug\"") != std::string::npos);
    REQUIRE(text.find("\"source\": \"mame\"") != std::string::npos);
    REQUIRE(text.find("\"verification\": null") != std::string::npos);
    REQUIRE(text.find("redump-cue") == std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("Redump track identity retains Redump identity and imports MAME metadata",
          "[db][neocd][redump][cue][metadata]") {
    fs::path root = make_test_root("redump_cue_renamed_mame");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    write_file(root / "neocd/Completely Renamed.cue",
               "FILE \"track.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/track.bin", "metal-track");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_redump_cue_verified_games == 0);
    REQUIRE(result.cd_redump_tracks_only_games == 1);
    REQUIRE(result.cd_metadata_only_games == 0);
    REQUIRE(result.cd_unknown_games == 0);

    const std::vector<goliath::Game> games =
        goliath::load_games(root / "database/games.json");
    REQUIRE(games.size() == 1);
    const goliath::Game& game = games.front();
    CHECK(game.short_name == "cd_redump_test_mslug");
    CHECK(game.source == "redump");
    CHECK(game.name == "Metal Slug (Japan) (En,Ja)");
    CHECK(game.verification ==
          std::optional<std::string>("redump-tracks-only"));
    CHECK(game.year == std::optional<std::string>("1996"));
    CHECK(game.manufacturer == std::optional<std::string>("SNK"));
    CHECK(game.genre ==
          std::optional<std::string>("Platform / Shooter Scrolling"));
    CHECK(game.players == std::optional<std::string>("2P sim"));
    CHECK(game.series == std::optional<std::string>("Metal Slug"));
    REQUIRE(game.history.has_value());
    CHECK(game.history->find("Identified against the local Redump") !=
          std::string::npos);
    REQUIRE(game.roms.size() == 1);
    CHECK(game.roms.front().mame == "mslug");

    fs::remove_all(root);
}


TEST_CASE("MAME CHD verification uses the CHD combined SHA-1, not Data SHA1", "[db][neocd][mame][chd]") {
    fs::path root = make_test_root("mame_chd_combined_sha1");
    create_common_metadata(root);

    // This mirrors chdman v5 semantics: the MAME software-list hash for
    // Super Sidekicks 2 is the displayed SHA1/combined hash at offset 84.
    write_chd_v5_header(
        root / "neocd/Tokuten-ou 2 Super Sidekicks 2 - The World Championship.chd",
        "ef2a5fee5502561d25922aad1656319de18c72a0",
        "1198aac4b91e06c96f960a74ac51d6d030ecf773");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_mame_chd_matched_games == 1);
    REQUIRE(result.cd_metadata_only_games == 0);
    REQUIRE(result.cd_unknown_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"short\": \"ssideki2\"") != std::string::npos);
    REQUIRE(text.find("\"verification\": \"mame-chd\"") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("MAME CHD badge is never granted by filename alone", "[db][neocd][mame][chd]") {
    fs::path root = make_test_root("mame_chd_wrong_hash");
    create_common_metadata(root);

    // Same title as the real game, but the CHD combined SHA-1 is deliberately
    // different. Stage 3B may still identify metadata by filename; Stage 3C3
    // must not award the MAME CHD badge.
    write_chd_v5_header(
        root / "neocd/Tokuten-ou 2 Super Sidekicks 2 - The World Championship.chd",
        "0d18059c40c2ff4b36c0cf94c5451d8811f9fd0f",
        "a766c0b7b81b7a7cd53db68dae5cf301cbb00a75");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_mame_chd_matched_games == 0);
    REQUIRE(result.cd_metadata_only_games == 1);
    REQUIRE(result.cd_unknown_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"short\": \"ssideki2\"") != std::string::npos);
    REQUIRE(text.find("\"verification\": null") != std::string::npos);
    REQUIRE(text.find("mame-chd") == std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("verified MAME CHD hash can identify a renamed image", "[db][neocd][mame][chd]") {
    fs::path root = make_test_root("mame_chd_renamed");
    create_common_metadata(root);

    write_chd_v5_header(
        root / "neocd/completely unrelated filename.chd",
        "ef2a5fee5502561d25922aad1656319de18c72a0",
        "1198aac4b91e06c96f960a74ac51d6d030ecf773");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_mame_chd_matched_games == 1);
    REQUIRE(result.cd_metadata_only_games == 0);
    REQUIRE(result.cd_unknown_games == 0);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});
    REQUIRE(text.find("\"short\": \"ssideki2\"") != std::string::npos);
    REQUIRE(text.find("Super Sidekicks 2") != std::string::npos);
    REQUIRE(text.find("\"verification\": \"mame-chd\"") != std::string::npos);

    in.close();
    fs::remove_all(root);
}


TEST_CASE("Redump CUE verification resolves track filename case without weakening path safety", "[db][neocd][redump][cue][paths]") {
    fs::path root = make_test_root("redump_cue_case_path");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    // The DAT fixture maps the three-byte payload "abc" to the Redump-only
    // Andro Dunos entry.  The CUE intentionally uses different filename case.
    write_file(root / "neocd/case-test.cue",
               "FILE \"TRACK.BIN\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/track.bin", "abc");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_redump_cue_verified_games == 0);
    REQUIRE(result.cd_redump_tracks_only_games == 1);
    REQUIRE(result.cd_metadata_only_games == 0);
    REQUIRE(result.cd_unknown_games == 0);

    fs::remove_all(root);
}

TEST_CASE("Redump CUE verification rejects parent-directory track traversal", "[db][neocd][redump][cue][paths]") {
    fs::path root = make_test_root("redump_cue_parent_traversal");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    // Even if an outside file has exactly the trusted Redump content, a CUE
    // inside the library must never escape its own directory to reach it.
    write_file(root / "outside.bin", "abc");
    write_file(root / "neocd/Metal Slug (Japan) (En,Ja).cue",
               "FILE \"../outside.bin\" BINARY\n  TRACK 01 MODE1/2352\n");

    std::string progress;
    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg, [&](const std::string& line) {
        progress += line;
    });

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1); // Stage 3B metadata/title only.
    REQUIRE(result.cd_redump_cue_verified_games == 0);
    REQUIRE(result.cd_redump_tracks_only_games == 0);
    REQUIRE(result.cd_metadata_only_games == 1);
    REQUIRE(result.cd_unknown_games == 0);
    REQUIRE(progress.find("Metadata only            : 1") != std::string::npos);

    fs::remove_all(root);
}

TEST_CASE("MAME CHD verification rejects an all-zero identity hash", "[db][neocd][mame][chd][header]") {
    fs::path root = make_test_root("mame_chd_zero_sha1");
    create_common_metadata(root);

    write_chd_v5_header(
        root / "neocd/Tokuten-ou 2 Super Sidekicks 2 - The World Championship.chd",
        std::string(40, '0'),
        "1198aac4b91e06c96f960a74ac51d6d030ecf773");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1); // Filename metadata still identifies it.
    REQUIRE(result.cd_mame_chd_matched_games == 0);
    REQUIRE(result.cd_metadata_only_games == 1);
    REQUIRE(result.cd_unknown_games == 0);

    fs::remove_all(root);
}


TEST_CASE("Redump CUE SHA-1 cache reuses unchanged track hashes", "[db][neocd][redump][cue][cache]") {
    fs::path root = make_test_root("redump_cue_cache_hit");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    write_file(root / "neocd/Mystery Disc.cue",
               "FILE \"renamed.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/renamed.bin", "abc");

    Config cfg = make_config(root);
    ScanResult first = scan_roms(cfg);

    REQUIRE(first.success);
    REQUIRE(first.cd_redump_cue_verified_games == 0);
    REQUIRE(first.cd_redump_tracks_only_games == 1);
    REQUIRE(first.cd_redump_sha1_cache_hits == 0);
    REQUIRE(first.cd_redump_sha1_calculated_files == 1);
    REQUIRE(first.cd_redump_sha1_cache_entries == 1);
    REQUIRE(first.cd_redump_sha1_cache_pruned == 0);
    REQUIRE(fs::is_regular_file(root / "database/hash_cache.json"));

    {
        std::ifstream cache_in(root / "database/hash_cache.json");
        std::string cache_text(std::istreambuf_iterator<char>(cache_in), {});
        REQUIRE(cache_text.find("a9993e364706816aba3e25717850c26c9cd0d89d") != std::string::npos);
    }

    // A second scan is a fresh scan_roms() call, so this verifies persistence
    // to disk rather than an in-memory shortcut.
    ScanResult second = scan_roms(cfg);
    REQUIRE(second.success);
    REQUIRE(second.cd_redump_cue_verified_games == 0);
    REQUIRE(second.cd_redump_tracks_only_games == 1);
    REQUIRE(second.cd_redump_sha1_cache_hits == 1);
    REQUIRE(second.cd_redump_sha1_calculated_files == 0);
    REQUIRE(second.cd_redump_sha1_cache_entries == 1);
    REQUIRE(second.cd_redump_sha1_cache_pruned == 0);

    fs::remove_all(root);
}

TEST_CASE("Redump CUE SHA-1 cache invalidates a same-size modified track", "[db][neocd][redump][cue][cache]") {
    fs::path root = make_test_root("redump_cue_cache_invalidate");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    const fs::path track = root / "neocd/renamed.bin";
    write_file(root / "neocd/Mystery Disc.cue",
               "FILE \"renamed.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(track, "abc");

    Config cfg = make_config(root);
    ScanResult first = scan_roms(cfg);
    REQUIRE(first.success);
    REQUIRE(first.cd_redump_cue_verified_games == 0);
    REQUIRE(first.cd_redump_tracks_only_games == 1);
    REQUIRE(first.cd_redump_sha1_calculated_files == 1);

    std::error_code ec;
    const fs::file_time_type old_mtime = fs::last_write_time(track, ec);
    REQUIRE_FALSE(ec);

    // Same byte length as the trusted payload, but different content.  Force a
    // clearly different timestamp so the cache fingerprint must be rejected.
    write_file(track, "xyz");
    fs::last_write_time(track, old_mtime + std::chrono::seconds(2), ec);
    REQUIRE_FALSE(ec);

    ScanResult second = scan_roms(cfg);
    REQUIRE(second.success);
    REQUIRE(second.cd_redump_cue_verified_games == 0);
    REQUIRE(second.cd_redump_tracks_only_games == 0);
    REQUIRE(second.cd_redump_sha1_cache_hits == 0);
    REQUIRE(second.cd_redump_sha1_calculated_files == 1);
    REQUIRE(second.cd_identified_games == 0);
    REQUIRE(second.cd_unknown_games == 1);

    fs::remove_all(root);
}

TEST_CASE("Redump CUE SHA-1 cache ignores malformed rows and rebuilds safely", "[db][neocd][redump][cue][cache]") {
    fs::path root = make_test_root("redump_cue_cache_malformed");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    write_file(root / "neocd/Mystery Disc.cue",
               "FILE \"renamed.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/renamed.bin", "abc");

    write_file(root / "database/hash_cache.json",
               R"json({"version":1,"files":{"broken":{"size":3,"mtime_ns":0,"sha1":"not-a-sha1"}}})json");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);
    REQUIRE(result.success);
    REQUIRE(result.cd_redump_cue_verified_games == 0);
    REQUIRE(result.cd_redump_tracks_only_games == 1);
    REQUIRE(result.cd_redump_sha1_cache_hits == 0);
    REQUIRE(result.cd_redump_sha1_calculated_files == 1);

    {
        std::ifstream cache_in(root / "database/hash_cache.json");
        std::string cache_text(std::istreambuf_iterator<char>(cache_in), {});
        REQUIRE(cache_text.find("not-a-sha1") == std::string::npos);
        REQUIRE(cache_text.find("a9993e364706816aba3e25717850c26c9cd0d89d") != std::string::npos);
    }

    fs::remove_all(root);
}

TEST_CASE("Redump CUE SHA-1 cache prunes tracks no longer referenced by the collection", "[db][neocd][redump][cue][cache]") {
    fs::path root = make_test_root("redump_cue_cache_prune");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    const fs::path cue = root / "neocd/Mystery Disc.cue";
    const fs::path old_track = root / "neocd/old-name.bin";
    const fs::path new_track = root / "neocd/new-name.bin";

    write_file(cue, "FILE \"old-name.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(old_track, "abc");

    Config cfg = make_config(root);
    ScanResult first = scan_roms(cfg);
    REQUIRE(first.success);
    REQUIRE(first.cd_redump_cue_verified_games == 0);
    REQUIRE(first.cd_redump_tracks_only_games == 1);
    REQUIRE(first.cd_redump_sha1_calculated_files == 1);
    REQUIRE(first.cd_redump_sha1_cache_entries == 1);
    REQUIRE(first.cd_redump_sha1_cache_pruned == 0);

    // Rename the physical track and update the CUE, but deliberately leave the
    // old file in place. The old cache row is stale because no current CUE uses
    // it, even though the old path still exists on disk.
    write_file(new_track, "abc");
    write_file(cue, "FILE \"new-name.bin\" BINARY\n  TRACK 01 MODE1/2352\n");

    ScanResult second = scan_roms(cfg);
    REQUIRE(second.success);
    REQUIRE(second.cd_redump_cue_verified_games == 0);
    REQUIRE(second.cd_redump_tracks_only_games == 1);
    REQUIRE(second.cd_redump_sha1_cache_hits == 0);
    REQUIRE(second.cd_redump_sha1_calculated_files == 1);
    REQUIRE(second.cd_redump_sha1_cache_entries == 1);
    REQUIRE(second.cd_redump_sha1_cache_pruned == 1);

    {
        std::ifstream cache_in(root / "database/hash_cache.json");
        std::string cache_text(std::istreambuf_iterator<char>(cache_in), {});
        REQUIRE(cache_text.find("new-name.bin") != std::string::npos);
        REQUIRE(cache_text.find("old-name.bin") == std::string::npos);
    }

    fs::remove_all(root);
}

TEST_CASE("Redump CUE SHA-1 cache is preserved while the configured CD root is offline", "[db][neocd][redump][cue][cache]") {
    fs::path root = make_test_root("redump_cue_cache_offline_root");
    create_common_metadata(root);
    create_redump_neocd_dat(root);

    write_file(root / "neocd/Mystery Disc.cue",
               "FILE \"renamed.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_file(root / "neocd/renamed.bin", "abc");

    Config cfg = make_config(root);
    ScanResult first = scan_roms(cfg);
    REQUIRE(first.success);
    REQUIRE(first.cd_redump_cue_verified_games == 0);
    REQUIRE(first.cd_redump_tracks_only_games == 1);
    REQUIRE(first.cd_redump_sha1_cache_entries == 1);

    std::error_code ec;
    fs::rename(root / "neocd", root / "neocd-offline", ec);
    REQUIRE_FALSE(ec);

    // A temporarily missing external drive/path must not erase a FULL-set
    // cache. No track is touched, but the existing row remains on disk.
    ScanResult offline = scan_roms(cfg);
    REQUIRE(offline.success);
    REQUIRE(offline.cd_game_count == 0);
    REQUIRE(offline.cd_redump_sha1_cache_hits == 0);
    REQUIRE(offline.cd_redump_sha1_calculated_files == 0);
    REQUIRE(offline.cd_redump_sha1_cache_entries == 1);
    REQUIRE(offline.cd_redump_sha1_cache_pruned == 0);

    {
        std::ifstream cache_in(root / "database/hash_cache.json");
        std::string cache_text(std::istreambuf_iterator<char>(cache_in), {});
        REQUIRE(cache_text.find("a9993e364706816aba3e25717850c26c9cd0d89d") != std::string::npos);
    }

    fs::remove_all(root);
}

TEST_CASE("Neo Geo CD scanner handles Windows paths beyond MAX_PATH", "[db][neocd][scan][longpath]") {
    // Use a unique root for this fixture. If a previous long-path run is
    // interrupted, MinGW std::filesystem::remove_all() can hang while trying
    // to clean the stale >MAX_PATH subtree on the next run. A unique root
    // keeps reruns independent; the explicit cleanup below removes the long
    // leaves before handing the short remainder back to remove_all().
    const auto unique_stamp =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() /
        ("goliath_db_test_neocd_long_path_" + std::to_string(unique_stamp));
    fs::create_directories(root);

    create_common_metadata(root);
    create_redump_neocd_dat(root);

    // One legal NTFS component can be up to 255 characters. A 220-character
    // game directory is enough to push both the CUE and its track beyond the
    // classic Windows 260-character full-path limit in the MSYS2 test root.
    const std::string long_folder(220, 'L');
    const fs::path game_dir = root / "neocd" / long_folder;
    const fs::path cue = game_dir / "Mystery Disc.cue";
    const fs::path track = game_dir / "renamed.bin";

#ifdef _WIN32
    REQUIRE(cue.wstring().size() > 260);
    REQUIRE(track.wstring().size() > 260);
#else
    REQUIRE(cue.string().size() > 260);
    REQUIRE(track.string().size() > 260);
#endif

    write_long_path_file(cue,
                         "FILE \"renamed.bin\" BINARY\n  TRACK 01 MODE1/2352\n");
    write_long_path_file(track, "abc");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.cd_image_count == 1);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_redump_cue_verified_games == 0);
    REQUIRE(result.cd_redump_tracks_only_games == 1);
    REQUIRE(result.cd_redump_sha1_cache_hits == 0);
    REQUIRE(result.cd_redump_sha1_calculated_files == 1);
    REQUIRE(result.cd_unknown_games == 0);

    // The persisted cache key must use the ordinary logical path, not the
    // Windows \\?\ spelling. A fresh scan must therefore reuse the long-path
    // track without hashing it again.
    ScanResult second = scan_roms(cfg);
    REQUIRE(second.success);
    REQUIRE(second.cd_redump_cue_verified_games == 0);
    REQUIRE(second.cd_redump_tracks_only_games == 1);
    REQUIRE(second.cd_redump_sha1_cache_hits == 1);
    REQUIRE(second.cd_redump_sha1_calculated_files == 0);

    std::ifstream in(root / "database/games.json");
    std::string json_text(std::istreambuf_iterator<char>(in), {});
    in.close();
    REQUIRE(json_text.find(long_folder + "/Mystery Disc.cue") != std::string::npos);
    REQUIRE(json_text.find("redump-tracks-only") != std::string::npos);
    REQUIRE(json_text.find("\\\\?\\") == std::string::npos);

    std::error_code ec;
#ifdef _WIN32
    // MinGW's recursive remove_all() can loop on an extended-length tree even
    // though ordinary single-file operations on the same \\?\ paths work.
    // Remove the two long leaves and their long directory explicitly first;
    // the remaining test tree is short and safe for normal remove_all().
    fs::remove(test_io_path(track), ec);
    REQUIRE_FALSE(ec);
    fs::remove(test_io_path(cue), ec);
    REQUIRE_FALSE(ec);
    fs::remove(test_io_path(game_dir), ec);
    REQUIRE_FALSE(ec);
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
#else
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
#endif
}

TEST_CASE("db scanner recursively discovers Neo Geo CD CUE and CHD images", "[db][neocd][scan]") {
    fs::path root = make_test_root("neocd_recursive");
    create_common_metadata(root);

    write_file(root / "neocd/Metal Slug (Japan) (En,Ja)/Metal Slug (Japan) (En,Ja).cue", "FILE track.bin BINARY\n");
    write_file(root / "neocd/Metal Slug (Japan) (En,Ja)/track.bin", "ignored");
    write_file(root / "neocd/Bang^2 Busters (France) (En,Ja) (Unl)/Bang^2 Busters (France) (En,Ja) (Unl).CUE", "FILE track.bin BINARY\n");
    write_file(root / "neocd/CHD/Tokuten-ou 2 Super Sidekicks 2 - The World Championship.CHD", "fake chd");
    write_file(root / "neocd/Andro Dunos (France) (Unl).cue", "FILE Andro.bin BINARY\n");
    write_file(root / "neocd/Andro Dunos (France) (Unl).bin", "ignored");
    write_file(root / "neocd/Metal Slug (Japan) (En,Ja).zip", "ignored");
    write_file(root / "neocd/random.iso", "ignored");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 0);
    REQUIRE(result.game_count == 0);
    REQUIRE(result.cd_image_count == 4);
    REQUIRE(result.cd_game_count == 4);
    REQUIRE(result.cd_identified_games == 3);
    REQUIRE(result.cd_unknown_games == 1);

    std::ifstream in(root / "database/games.json");
    std::string text(std::istreambuf_iterator<char>(in), {});

    REQUIRE(text.find("\"system\": \"neogeocd\"") != std::string::npos);
    REQUIRE(text.find("\"short\": \"mslug\"") != std::string::npos);
    REQUIRE(text.find("\"short\": \"bbbuster\"") != std::string::npos);
    REQUIRE(text.find("\"short\": \"ssideki2\"") != std::string::npos);
    REQUIRE(text.find("\"source\": \"unknown\"") != std::string::npos);
    REQUIRE(text.find("\"identified\": false") != std::string::npos);
    REQUIRE(text.find("Metal Slug (Japan) (En,Ja)/Metal Slug (Japan) (En,Ja).cue") != std::string::npos);
    REQUIRE(text.find("CHD/Tokuten-ou 2 Super Sidekicks 2 - The World Championship.CHD") != std::string::npos);
    REQUIRE(text.find("track.bin") == std::string::npos);
    REQUIRE(text.find("Metal Slug (Japan) (En,Ja).zip") == std::string::npos);
    REQUIRE(text.find("random.iso") == std::string::npos);
    REQUIRE(text.find("Neo Geo CD version - WRONG FOR CARTRIDGE") != std::string::npos);

    in.close();
    fs::remove_all(root);
}

TEST_CASE("db scanner keeps cartridge and CD statistics independent", "[db][neocd][stats]") {
    fs::path root = make_test_root("neocd_stats");
    create_common_metadata(root);

    write_file(root / "roms/mslug.neo", "");
    write_file(root / "roms/msluga.neo", "");
    write_file(root / "neocd/Metal Slug (Japan) (En,Ja).chd", "fake chd");

    Config cfg = make_config(root);
    ScanResult result = scan_roms(cfg);

    REQUIRE(result.success);
    REQUIRE(result.rom_file_count == 2);
    REQUIRE(result.game_count == 1);
    REQUIRE(result.parent_games == 1);
    REQUIRE(result.variant_count == 1);
    REQUIRE(result.homebrew_games == 0);
    REQUIRE(result.cd_image_count == 1);
    REQUIRE(result.cd_game_count == 1);
    REQUIRE(result.cd_identified_games == 1);
    REQUIRE(result.cd_unknown_games == 0);

    fs::remove_all(root);
}
