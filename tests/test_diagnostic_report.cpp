#include "catch2/catch.hpp"

#include "common/diagnostic_report.hpp"
#include "common/qt_path.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using namespace goliath;

TEST_CASE("diagnostic redaction replaces private roots and credentials",
          "[diagnostics][privacy]") {
    const std::string input =
        "path=C:/Users/Eugen/Goliath/config/settings.ini\n"
        "native=C:\\Users\\Eugen\\Goliath\\config\\settings.ini\n"
        "home=c:/users/eugen/Desktop/report.txt\n"
        "token = abc123\n"
        "password: \"hunter2\"\n"
        "remote=https://person:private@example.test/file\n";

    const std::string redacted = redact_diagnostic_text(input, {
        {"C:/Users/Eugen/Goliath", "<GOLIATH_DIR>", true},
        {"C:/Users/Eugen", "<HOME>", true},
    });

    CHECK(redacted.find("Eugen") == std::string::npos);
    CHECK(redacted.find("abc123") == std::string::npos);
    CHECK(redacted.find("hunter2") == std::string::npos);
    CHECK(redacted.find("person:private") == std::string::npos);
    CHECK(redacted.find("<GOLIATH_DIR>/config/settings.ini") !=
          std::string::npos);
    CHECK(redacted.find("<GOLIATH_DIR>\\config\\settings.ini") !=
          std::string::npos);
    CHECK(redacted.find("<HOME>/Desktop/report.txt") != std::string::npos);
    CHECK(redacted.find("token = <REDACTED>") != std::string::npos);
    CHECK(redacted.find("https://<REDACTED>@example.test/file") !=
          std::string::npos);
}

TEST_CASE("diagnostic log tails normalize line endings and stay bounded",
          "[diagnostics][logs]") {
    const std::vector<std::string> lines =
        diagnostic_tail_lines(
            "one\r\n\x1b[0mtwo\x1b[0m\nthree\r\nfour\n", 3);

    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "two");
    CHECK(lines[1] == "three");
    CHECK(lines[2] == "four");
    CHECK(diagnostic_tail_lines("one\n", 0).empty());
}

TEST_CASE("diagnostic video backend labels follow the shared schema",
          "[diagnostics][video]") {
    CHECK(diagnostic_video_api_name(0) == "OpenGL Core");
    CHECK(diagnostic_video_api_name(1) == "OpenGL ES");
    CHECK(diagnostic_video_api_name(2) == "OpenGL Compatibility");
    CHECK(diagnostic_video_api_name(3) == "Vulkan (Experimental)");
    CHECK(diagnostic_video_api_name(99) == "Unknown (99)");
}

TEST_CASE("generated diagnostic report uses an allowlist and redacts paths",
          "[diagnostics][privacy][report]") {
    const fs::path privateRoot =
        fs::temp_directory_path() / "private-user" / "Goliath";

    DiagnosticReportRequest request;
    request.application_version = "9.8.7-test";
    request.build_date = "2026-10-11";
    request.build_revision = "deadbeef";
    request.qt_platform = "test-platform";
    request.paths.base_dir = privateRoot;
    request.paths.config_dir = privateRoot / "config";
    request.paths.json_file = privateRoot / "database" / "games.json";
    request.paths.metadata_dir = privateRoot / "metadata";
    request.paths.jollygood_exe = privateRoot / "jollygood";
    request.paths.bios_dir = privateRoot / "bios";
    request.paths.jollygood_settings_ini =
        request.paths.config_dir / "jollygood" / "settings.ini";
    request.paths.geolith_ini =
        request.paths.config_dir / "jollygood" / "geolith.ini";
    request.paths.input_config =
        request.paths.config_dir / "jollygood" / "geolith_input.ini";
    request.rom_dir = privateRoot / "roms";
    request.neocd_dir = privateRoot / "neocd";
    request.snapshot_dir = privateRoot / "snaps";
    request.config.set("Paths", "jollygood_args",
                       "-c geolith --token=top-secret");
    request.config.set("Paths", "icons",
                       path_to_utf8(
                           privateRoot.parent_path() / "private-icons"));
    request.config.set("UI", "last_rom", "private-last-game.neo");

    const DiagnosticReportResult generated =
        generate_diagnostic_report(request);

    CHECK(generated.text.find("GOLIATH DIAGNOSTIC REPORT") !=
          std::string::npos);
    CHECK(generated.text.find("Goliath version: 9.8.7-test") !=
          std::string::npos);
    CHECK(generated.text.find("<GOLIATH_DIR>") != std::string::npos);
    CHECK(generated.text.find(privateRoot.string()) == std::string::npos);
    CHECK(generated.text.find("top-secret") == std::string::npos);
    CHECK(generated.text.find("private-icons") == std::string::npos);
    CHECK(generated.text.find("<ICON_DIR>") != std::string::npos);
    CHECK(generated.text.find("private-last-game.neo") == std::string::npos);
    CHECK(generated.required_file_problems > 0);
    CHECK(generated.bios_problems == 1);
}
