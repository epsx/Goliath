#include "catch2/catch.hpp"

#include "ini/ini_document.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
using goliath::IniDocument;

TEST_CASE("IniDocument loads sections and keys", "[ini]") {
    fs::path p = fs::temp_directory_path() / "goliath_test_ini.ini";
    fs::remove(p);

    {
        std::ofstream out(p);
        out << "[Paths]\n"
            << "roms = C:\\\\roms\n"
            << "snaps=../snaps\n"
            << "\n"
            << "[UI]\n"
            << "theme = Dark Modern\n"
            << "# comment\n"
            << "; another comment\n";
    }

    IniDocument doc;
    doc.load(p);

    REQUIRE(doc.has_section("Paths"));
    REQUIRE(doc.has_section("UI"));
    REQUIRE(!doc.has_section("Missing"));

    REQUIRE(doc.has_option("Paths", "roms"));
    REQUIRE(doc.has_option("Paths", "snaps"));
    REQUIRE(doc.get("Paths", "roms") == "C:\\\\roms");
    REQUIRE(doc.get("Paths", "snaps") == "../snaps");
    REQUIRE(doc.get("UI", "theme") == "Dark Modern");
    REQUIRE(doc.get("Missing", "key", "fallback") == "fallback");

    auto sections = doc.sections();
    REQUIRE(sections.size() == 2);

    fs::remove(p);
}

TEST_CASE("IniDocument set, remove and save roundtrip", "[ini]") {
    fs::path p = fs::temp_directory_path() / "goliath_test_ini_save.ini";
    fs::remove(p);

    IniDocument doc;
    doc.set("Paths", "roms", "roms");
    doc.set("Paths", "snaps", "snaps");
    doc.set("UI", "theme", "Light");
    {
        std::ofstream previous(p);
        REQUIRE(previous.good());
        previous << "old contents\n";
    }
    REQUIRE(doc.save(p));

    IniDocument doc2;
    doc2.load(p);
    REQUIRE(doc2.get("Paths", "roms") == "roms");
    REQUIRE(doc2.get("Paths", "snaps") == "snaps");
    REQUIRE(doc2.get("UI", "theme") == "Light");

    doc2.remove_option("Paths", "snaps");
    REQUIRE(!doc2.has_option("Paths", "snaps"));

    doc2.remove_section("UI");
    REQUIRE(!doc2.has_section("UI"));

    fs::remove(p);
}

TEST_CASE("IniDocument preserves a blocker when its destination is unusable",
          "[ini][persistence]") {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_ini_blocker_" + std::to_string(stamp));
    fs::remove_all(root);
    fs::create_directories(root);

    const fs::path blocker = root / "not-a-directory";
    const std::string existing = "keep this file unchanged\n";
    {
        std::ofstream output(blocker, std::ios::binary);
        REQUIRE(output.good());
        output << existing;
    }

    IniDocument document;
    document.set("video", "shader", "4");

    std::string error;
    CHECK_FALSE(document.save(blocker / "settings.ini", &error));
    CHECK(error.find("Could not create INI directory:") == 0);

    std::ifstream input(blocker, std::ios::binary);
    REQUIRE(input.good());
    const std::string current(std::istreambuf_iterator<char>(input), {});
    CHECK(current == existing);
    input.close();

    REQUIRE(fs::remove(blocker));
    REQUIRE(document.save(blocker / "settings.ini", &error));
    CHECK(error.empty());
    REQUIRE(fs::is_regular_file(blocker / "settings.ini"));

    IniDocument recovered;
    recovered.load(blocker / "settings.ini");
    CHECK(recovered.get("video", "shader") == "4");

    fs::remove_all(root);
}
