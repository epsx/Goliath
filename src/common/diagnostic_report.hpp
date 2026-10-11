// diagnostic_report.hpp — privacy-aware, exportable support report.
//
// Collection may probe the configured JGRF executable, load the installed
// Geolith core, verify BIOS archives, and read bounded log tails. Call it from
// a worker thread. The returned text is already redacted and safe to present
// in a save dialog.
#pragma once

#include "common/goliath_common.hpp"
#include "common/paths.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace goliath {

struct DiagnosticRedactionRule {
    std::string value;
    std::string replacement;
    bool case_insensitive = false;
};

struct DiagnosticReportRequest {
    Config config;
    AppPaths paths;
    std::filesystem::path rom_dir;
    std::filesystem::path neocd_dir;
    std::filesystem::path snapshot_dir;
    std::string application_version;
    std::string build_date;
    std::string build_revision;
    std::string qt_platform;
    bool session_verbose_logging = false;
};

struct DiagnosticReportResult {
    std::string text;
    int required_file_problems = 0;
    int bios_problems = 0;
};

// Replaces known private roots, credential-bearing URLs, and common secret
// assignments. Rules are applied longest-first so nested paths receive the
// most useful placeholder.
std::string redact_diagnostic_text(
    std::string_view text,
    std::vector<DiagnosticRedactionRule> rules);

// Normalizes CRLF and returns at most max_lines from the end of the text.
std::vector<std::string> diagnostic_tail_lines(
    std::string_view text,
    std::size_t max_lines);

// Stable display label for JGRF's [video] api value.
std::string diagnostic_video_api_name(int value);

// Potentially slow collection entry point. The report always succeeds as a
// text artifact; unavailable components are described inside it.
DiagnosticReportResult generate_diagnostic_report(
    const DiagnosticReportRequest& request);

} // namespace goliath
