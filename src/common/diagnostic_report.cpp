#include "common/diagnostic_report.hpp"

#include "common/filesystem_safety.hpp"
#include "common/log_file.hpp"
#include "common/qt_path.hpp"
#include "game/bios_verify.hpp"
#include "game/geolith_capabilities.hpp"
#include "game/jollygood_capabilities.hpp"
#include "game/jollygood_executable.hpp"
#include "game/video_settings.hpp"
#include "ini/ini_document.hpp"
#include "input/input_maps.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSysInfo>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace goliath {
namespace {

constexpr std::size_t kLogTailLines = 100;
constexpr qint64 kLogTailBytes = 128 * 1024;

struct JgrfDiagnosticProbe {
    fs::path executable;
    std::string version;
    bool vulkan_available = false;
    bool success = false;
    std::string error;
};

struct LogTailResult {
    std::vector<std::string> lines;
    std::string status;
};

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

void replace_all(std::string& text,
                 const std::string& needle,
                 const std::string& replacement,
                 bool caseInsensitive) {
    if (needle.empty()) return;

    if (!caseInsensitive) {
        std::size_t position = 0;
        while ((position = text.find(needle, position)) != std::string::npos) {
            text.replace(position, needle.size(), replacement);
            position += replacement.size();
        }
        return;
    }

    const std::string lowerNeedle = lower_ascii(needle);
    std::size_t position = 0;
    while (position <= text.size()) {
        const std::string lowerText = lower_ascii(text.substr(position));
        const std::size_t relative = lowerText.find(lowerNeedle);
        if (relative == std::string::npos) break;
        position += relative;
        text.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

std::string one_line(std::string value) {
    for (char& ch : value) {
        if (ch == '\r' || ch == '\n' || ch == '\t' || ch == '\0') ch = ' ';
    }
    return value;
}

std::string strip_ansi_csi_sequences(std::string_view text) {
    std::string result;
    result.reserve(text.size());

    std::size_t position = 0;
    while (position < text.size()) {
        if (static_cast<unsigned char>(text[position]) == 0x1b &&
            position + 1 < text.size() && text[position + 1] == '[') {
            std::size_t cursor = position + 2;
            while (cursor < text.size()) {
                const unsigned char ch =
                    static_cast<unsigned char>(text[cursor]);
                if (ch < 0x20 || ch > 0x3f) break;
                ++cursor;
            }
            if (cursor < text.size()) {
                const unsigned char finalByte =
                    static_cast<unsigned char>(text[cursor]);
                if (finalByte >= 0x40 && finalByte <= 0x7e) {
                    position = cursor + 1;
                    continue;
                }
            }
        }
        result.push_back(text[position]);
        ++position;
    }
    return result;
}

std::string yes_no(bool value) {
    return value ? "yes" : "no";
}

std::optional<int> ini_integer(const IniDocument& document,
                               const std::string& section,
                               const std::string& key) {
    if (!document.has_option(section, key)) return std::nullopt;
    try {
        std::size_t consumed = 0;
        const std::string stored = document.get(section, key);
        const int value = std::stoi(stored, &consumed);
        if (consumed != stored.size()) return std::nullopt;
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

int effective_video_api(const AppPaths& paths) {
    const VideoSettingSpec* api =
        find_video_setting(jgrf_video_setting_specs(), "api");
    const int fallback = api ? api->default_value : 0;

    IniDocument global;
    global.load(paths.jollygood_settings_ini);
    int value = ini_integer(global, "video", "api").value_or(fallback);
    if (api) value = normalize_video_setting_value(*api, value);

    IniDocument core;
    core.load(paths.geolith_ini);
    const std::optional<int> overrideValue = ini_integer(core, "video", "api");
    if (overrideValue && (!api || is_valid_video_setting_value(*api, *overrideValue)))
        value = *overrideValue;
    return value;
}

JgrfDiagnosticProbe probe_jgrf(const fs::path& configured) {
    JgrfDiagnosticProbe result;
    result.executable = resolve_jollygood_executable(configured);
    if (result.executable.empty()) {
        result.error = "Configured JGRF executable was not found.";
        return result;
    }

    QProcess process;
    process.setProgram(path_to_qstring(result.executable));
    process.setArguments({"--help"});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(1000)) {
        result.error = process.errorString().toStdString();
        return result;
    }
    if (!process.waitForFinished(kJgrfHelpProbeTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        result.error = "JGRF --help did not finish within 3 seconds.";
        return result;
    }

    const QByteArray output = process.readAll().left(1024 * 1024);
    const std::string help(output.constData(),
                           static_cast<std::size_t>(output.size()));
    const std::optional<std::string> version = jgrf_version_from_help(help);
    result.vulkan_available = jgrf_help_reports_vulkan(help);
    if (!version) {
        result.error =
            "JGRF --help did not report a recognizable frontend version.";
        return result;
    }

    result.version = *version;
    result.success = true;
    return result;
}

void append_key_value(std::ostringstream& report,
                      std::string_view key,
                      const std::string& value) {
    report << key << ": " << (value.empty() ? "<not available>" : value)
           << '\n';
}

void append_section(std::ostringstream& report, std::string_view title) {
    report << "\n[" << title << "]\n";
}

std::string direct_path_status(const DirectPathInspection& inspection,
                               DirectPathKind expectedKind) {
    if (inspection.kind == expectedKind) return "OK";
    switch (inspection.kind) {
    case DirectPathKind::Missing:
        return "MISSING";
    case DirectPathKind::RegularFile:
        return "INVALID (regular file; expected directory)";
    case DirectPathKind::Directory:
        return "INVALID (directory; expected regular file)";
    case DirectPathKind::Other:
        return "INVALID (link/reparse point or special object)";
    case DirectPathKind::Error:
        return "ERROR (" + inspection.error + ")";
    }
    return "ERROR";
}

void append_file_check(std::ostringstream& report,
                       int& requiredProblems,
                       std::string_view label,
                       const fs::path& path,
                       DirectPathKind expectedKind,
                       bool required) {
    const DirectPathInspection inspection = inspect_direct_path(path);
    const std::string status = direct_path_status(inspection, expectedKind);
    const bool ok = inspection.kind == expectedKind;
    report << (ok ? "[OK] " : required ? "[PROBLEM] " : "[OPTIONAL] ")
           << label << ": " << status << " | " << path_to_utf8(path) << '\n';
    if (required && !ok) ++requiredProblems;
}

std::size_t shader_file_count(const fs::path& directory) {
    std::error_code ec;
    if (!fs::is_directory(directory, ec)) return 0;

    std::size_t count = 0;
    for (fs::directory_iterator iterator(directory, ec), end;
         !ec && iterator != end; iterator.increment(ec)) {
        if (!iterator->is_regular_file(ec)) {
            ec.clear();
            continue;
        }
        if (lower_ascii(path_to_utf8(iterator->path().extension())) == ".spv")
            ++count;
    }
    return count;
}

LogTailResult read_log_tail(const fs::path& path) {
    LogTailResult result;
    QFile file;
    const LogFileOperationResult opened =
        open_direct_regular_log_file(file, path, false);
    if (!opened.success) {
        result.status = "unavailable: " + opened.error;
        return result;
    }
    if (!opened.existed || !file.isOpen()) {
        result.status = "not created";
        return result;
    }

    const qint64 size = file.size();
    const qint64 offset = std::max<qint64>(0, size - kLogTailBytes);
    if (!file.seek(offset)) {
        result.status = "unavailable: " + file.errorString().toStdString();
        return result;
    }

    QByteArray bytes = file.read(kLogTailBytes);
    if (offset > 0) {
        const qsizetype firstNewline = bytes.indexOf('\n');
        bytes = firstNewline < 0 ? QByteArray{} : bytes.mid(firstNewline + 1);
    }
    const std::string text(bytes.constData(),
                           static_cast<std::size_t>(bytes.size()));
    result.lines = diagnostic_tail_lines(text, kLogTailLines);
    result.status = result.lines.empty() ? "empty" : "available";
    return result;
}

void append_log(std::ostringstream& report,
                std::string_view title,
                const LogTailResult& tail) {
    append_section(report, title);
    if (tail.lines.empty()) {
        report << '<' << tail.status << ">\n";
        return;
    }
    for (const std::string& line : tail.lines) report << line << '\n';
}

void append_goliath_configuration(std::ostringstream& report,
                                  const Config& config,
                                  bool sessionVerbose) {
    struct Entry {
        const char* section;
        const char* key;
        const char* fallback;
    };
    static constexpr std::array<Entry, 25> entries = {{
        {"Paths", "roms", "roms"},
        {"Paths", "neocd", "neocd"},
        {"Paths", "icons", "icons"},
        {"Paths", "snaps", "snaps"},
        {"Paths", "metadata", "metadata"},
        {"Paths", "bios", "bios"},
        {"Paths", "database", "database"},
        {"Paths", "jollygood", "jollygood"},
        {"Paths", "jollygood_args", "-c geolith"},
        {"Paths", "config", "config"},
        {"Audio", "volume", "100"},
        {"Hotkeys", "gif", ""},
        {"Hotkeys", "png", ""},
        {"Hotkeys", "gif_duration_seconds", "5"},
        {"Tools", "lithogen_executable", ""},
        {"UI", "theme", "Dark Modern"},
        {"UI", "input_device", "0"},
        {"UI", "library_system", "neogeo"},
        {"UI", "library_view", "list"},
        {"UI", "sort_key", "display"},
        {"UI", "show_variants", "true"},
        {"UI", "command_overlay", "true"},
        {"UI", "favorites_only", "false"},
        {"UI", "rating_filter", "any"},
        {"UI", "playtime_filter", "any"},
    }};

    report << "goliath.ini allowlist:\n";
    for (const Entry& entry : entries) {
        report << "  [" << entry.section << "] " << entry.key << " = "
               << one_line(config.get(entry.section, entry.key,
                                      entry.fallback))
               << '\n';
    }
    report << "  [Updates] check_interval = "
           << one_line(config.get("Updates", "check_interval", "disabled"))
           << '\n';
    report << "  session_verbose_jgrf = " << yes_no(sessionVerbose) << '\n';
}

void append_known_ini_option(std::ostringstream& report,
                             const IniDocument& document,
                             std::string_view source,
                             const char* section,
                             const char* key) {
    if (!document.has_option(section, key)) return;
    report << "  " << source << " [" << section << "] " << key << " = "
           << one_line(document.get(section, key)) << '\n';
}

void append_jgrf_configuration(std::ostringstream& report,
                               const AppPaths& paths) {
    IniDocument settings;
    settings.load(paths.jollygood_settings_ini);
    IniDocument core;
    core.load(paths.geolith_ini);
    IniDocument input;
    input.load(paths.input_config);

    report << "JGRF/Geolith allowlist:\n";
    for (const VideoSettingSpec& spec : jgrf_video_setting_specs()) {
        append_known_ini_option(
            report, settings, "settings.ini", "video", spec.key);
        append_known_ini_option(
            report, core, "geolith.ini", "video", spec.key);
    }

    static constexpr std::array<const char*, 1> audioKeys = {{"rsqual"}};
    static constexpr std::array<const char*, 5> miscKeys = {{
        "cheatauto", "corelog", "frontendlog", "bgfx", "font",
    }};
    static constexpr std::array<const char*, 11> coreKeys = {{
        "system", "cdsystem", "unihw", "region", "memcard_inserted",
        "memcard_wp", "freeplay", "settingmode", "adpcm_wrap",
        "cd_dma_len_limit", "input",
    }};

    for (const char* key : audioKeys) {
        append_known_ini_option(report, settings, "settings.ini", "audio", key);
        append_known_ini_option(report, core, "geolith.ini", "audio", key);
    }
    for (const char* key : miscKeys) {
        append_known_ini_option(report, settings, "settings.ini", "misc", key);
        append_known_ini_option(report, core, "geolith.ini", "misc", key);
    }
    append_known_ini_option(
        report, settings, "settings.ini", "input", "axis_deadzone");
    append_known_ini_option(
        report, core, "geolith.ini", "input", "axis_deadzone");
    for (const char* key : coreKeys)
        append_known_ini_option(report, core, "geolith.ini", "geolith", key);
    for (const VideoSettingSpec& spec : geolith_video_setting_specs())
        append_known_ini_option(
            report, core, "geolith.ini", "geolith", spec.key);

    std::vector<InputBindingAssignment> assignments;
    for (const auto& [section, keys] : input_defs()) {
        for (const std::string& key : keys) {
            if (!input.has_option(section, key)) continue;
            const std::string value = input.get(section, key);
            report << "  geolith_input.ini [" << section << "] " << key
                   << " = " << one_line(value) << '\n';
            assignments.push_back({section, key, value, false});
        }
    }
    const InputBindingConflicts conflicts =
        find_input_binding_conflicts(assignments);
    std::size_t conflictingActions = 0;
    for (const auto& [binding, group] : conflicts) {
        static_cast<void>(binding);
        conflictingActions += group.size();
    }
    report << "  input_conflict_groups = " << conflicts.size() << '\n';
    report << "  input_conflicting_actions = " << conflictingActions << '\n';
}

std::vector<DiagnosticRedactionRule> redaction_rules(
        const DiagnosticReportRequest& request) {
#if defined(_WIN32)
    constexpr bool pathCaseInsensitive = true;
#else
    constexpr bool pathCaseInsensitive = false;
#endif
    std::vector<DiagnosticRedactionRule> rules;
    const auto addValue = [&](const std::string& value,
                              const char* replacement) {
        if (value.empty()) return;
        const std::string comparable = pathCaseInsensitive
            ? lower_ascii(value) : value;
        for (const DiagnosticRedactionRule& rule : rules) {
            const std::string existing = pathCaseInsensitive
                ? lower_ascii(rule.value) : rule.value;
            if (existing == comparable) return;
        }
        rules.push_back({value, replacement, pathCaseInsensitive});
    };
    const auto addPath = [&](const fs::path& path, const char* replacement) {
        if (path.empty() || path == path.root_path()) return;
        const std::string value = path_to_utf8(path.lexically_normal());
        if (!value.empty()) {
            addValue(value, replacement);
            std::string alternate = value;
            std::replace(alternate.begin(), alternate.end(), '\\', '/');
            if (alternate != value) addValue(alternate, replacement);
        }
    };
    const fs::path normalizedBase = request.paths.base_dir.lexically_normal();
    const auto addSpecificPath = [&](const fs::path& path,
                                     const char* replacement) {
        if (path.empty() || path.lexically_normal() == normalizedBase) return;
        addPath(path, replacement);
    };

    addSpecificPath(request.rom_dir, "<ROM_DIR>");
    addSpecificPath(request.neocd_dir, "<NEOCD_DIR>");
    addSpecificPath(request.snapshot_dir, "<SNAPSHOT_DIR>");
    addSpecificPath(request.paths.bios_dir, "<BIOS_DIR>");
    addSpecificPath(request.paths.config_dir, "<CONFIG_DIR>");
    addSpecificPath(request.paths.metadata_dir, "<METADATA_DIR>");
    addSpecificPath(request.paths.json_file.parent_path(), "<DATABASE_DIR>");
    const fs::path iconDirectory =
        resolve_path(request.config, "icons", request.paths.base_dir);
    if (!iconDirectory.empty() &&
        iconDirectory.lexically_normal() != normalizedBase) {
        addPath(iconDirectory, "<ICON_DIR>");
    }

    const fs::path jgrfDirectory = request.paths.jollygood_exe.parent_path();
    if (!jgrfDirectory.empty() &&
        jgrfDirectory.lexically_normal() != normalizedBase) {
        addPath(jgrfDirectory, "<JGRF_DIR>");
    }

    const std::string lithogenConfigured =
        request.config.get("Tools", "lithogen_executable", "");
    if (!lithogenConfigured.empty()) {
        fs::path lithogenPath = path_from_utf8(lithogenConfigured);
        if (lithogenPath.is_relative())
            lithogenPath = request.paths.base_dir / lithogenPath;
        const fs::path lithogenDirectory = lithogenPath.parent_path();
        if (!lithogenDirectory.empty() &&
            lithogenDirectory.lexically_normal() != normalizedBase) {
            addPath(lithogenDirectory, "<LITHOGEN_DIR>");
        }
    }
    addPath(request.paths.base_dir, "<GOLIATH_DIR>");
    addPath(qstring_to_path(QDir::homePath()), "<HOME>");
    return rules;
}

} // namespace

std::string redact_diagnostic_text(
        std::string_view text,
        std::vector<DiagnosticRedactionRule> rules) {
    std::sort(rules.begin(), rules.end(),
              [](const DiagnosticRedactionRule& left,
                 const DiagnosticRedactionRule& right) {
                  return left.value.size() > right.value.size();
              });

    std::string result(text);
    for (const DiagnosticRedactionRule& rule : rules) {
        replace_all(result, rule.value, rule.replacement,
                    rule.case_insensitive);

        std::string forwardSlashes = rule.value;
        std::replace(forwardSlashes.begin(), forwardSlashes.end(), '\\', '/');
        if (forwardSlashes != rule.value) {
            replace_all(result, forwardSlashes, rule.replacement,
                        rule.case_insensitive);
        }

        std::string backslashes = rule.value;
        std::replace(backslashes.begin(), backslashes.end(), '/', '\\');
        if (backslashes != rule.value) {
            replace_all(result, backslashes, rule.replacement,
                        rule.case_insensitive);
        }
    }

    static const std::regex credentialUrl(
        R"(([A-Za-z][A-Za-z0-9+.-]*://)[^/@\s]+@)");
    result = std::regex_replace(result, credentialUrl,
                                "$1<REDACTED>@");

    static const std::regex windowsUserDirectory(
        R"(([A-Za-z]:[\\/](?:Users|Documents and Settings)[\\/])[^\\/\s]+)",
        std::regex_constants::icase);
    result = std::regex_replace(result, windowsUserDirectory,
                                "$1<USER>");

    static const std::regex unixUserDirectory(
        R"((/(?:home|Users)/)[^/\s]+)");
    result = std::regex_replace(result, unixUserDirectory,
                                "$1<USER>");

    static const std::regex secretAssignment(
        R"((\b(?:token|password|passwd|secret|api[_-]?key|authorization|cookie)\b\s*[:=]\s*)("[^"]*"|'[^']*'|[^\s,;]+))",
        std::regex_constants::icase);
    result = std::regex_replace(result, secretAssignment,
                                "$1<REDACTED>");
    return result;
}

std::vector<std::string> diagnostic_tail_lines(
        std::string_view text,
        std::size_t maxLines) {
    if (maxLines == 0) return {};

    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t newline = text.find('\n', start);
        const std::size_t end = newline == std::string_view::npos
            ? text.size() : newline;
        std::string line(text.substr(start, end - start));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = strip_ansi_csi_sequences(line);
        if (newline == std::string_view::npos && line.empty()) break;
        lines.push_back(std::move(line));
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }

    if (lines.size() > maxLines) {
        lines.erase(lines.begin(),
                    lines.end() - static_cast<std::ptrdiff_t>(maxLines));
    }
    return lines;
}

std::string diagnostic_video_api_name(int value) {
    const VideoSettingSpec* api =
        find_video_setting(jgrf_video_setting_specs(), "api");
    if (!api) return "Unknown";
    for (const VideoSettingOption& option : api->options) {
        if (option.value == value) return option.label;
    }
    return "Unknown (" + std::to_string(value) + ")";
}

DiagnosticReportResult generate_diagnostic_report(
        const DiagnosticReportRequest& request) {
    DiagnosticReportResult result;
    std::ostringstream report;

    const int videoApi = effective_video_api(request.paths);
    const JgrfDiagnosticProbe jgrf = probe_jgrf(request.paths.jollygood_exe);
    const GeolithCapabilities geolith =
        probe_geolith_capabilities(request.paths.jollygood_exe);

    report << "GOLIATH DIAGNOSTIC REPORT\n"
           << "Generated: "
           << QDateTime::currentDateTime().toString(Qt::ISODate).toStdString()
           << '\n'
           << "Privacy: known personal paths and common credential fields "
              "are redacted. Only approved configuration keys are included.\n";

    append_section(report, "Application");
    append_key_value(report, "Goliath version", request.application_version);
    append_key_value(report, "Build date", request.build_date);
    append_key_value(report, "Build revision", request.build_revision);
    append_key_value(report, "Qt runtime", qVersion());

    append_section(report, "Operating system");
    append_key_value(report, "Product",
                     QSysInfo::prettyProductName().toStdString());
    append_key_value(report, "Kernel",
                     (QSysInfo::kernelType() + " " +
                      QSysInfo::kernelVersion()).toStdString());
    append_key_value(report, "CPU architecture",
                     QSysInfo::currentCpuArchitecture().toStdString());
    append_key_value(report, "Build ABI",
                     QSysInfo::buildAbi().toStdString());
    append_key_value(report, "Qt platform", request.qt_platform);

    append_section(report, "Runtime paths");
    append_key_value(report, "Goliath directory",
                     path_to_utf8(request.paths.base_dir));
    append_key_value(report, "Configuration directory",
                     path_to_utf8(request.paths.config_dir));
    append_key_value(report, "ROM directory", path_to_utf8(request.rom_dir));
    append_key_value(report, "Neo Geo CD directory",
                     path_to_utf8(request.neocd_dir));
    append_key_value(report, "Snapshot directory",
                     path_to_utf8(request.snapshot_dir));
    append_key_value(report, "BIOS directory",
                     path_to_utf8(request.paths.bios_dir));
    append_key_value(report, "Metadata directory",
                     path_to_utf8(request.paths.metadata_dir));
    append_key_value(report, "Library database",
                     path_to_utf8(request.paths.json_file));

    append_section(report, "JGRF and Geolith");
    append_key_value(report, "Configured JGRF executable",
                     path_to_utf8(request.paths.jollygood_exe));
    append_key_value(report, "Resolved JGRF executable",
                     path_to_utf8(jgrf.executable));
    append_key_value(report, "JGRF version",
                     jgrf.success ? jgrf.version : "Unavailable");
    append_key_value(report, "JGRF probe",
                     jgrf.success ? "OK" : jgrf.error);
    append_key_value(report, "JGRF Vulkan renderer",
                     jgrf.success ? (jgrf.vulkan_available
                                         ? "Available (experimental)"
                                         : "Not compiled")
                                  : "Unknown");
    const fs::path corePath = !geolith.core_library.empty()
        ? geolith.core_library
        : geolith_library_for_jgrf(
              jgrf.executable.empty() ? request.paths.jollygood_exe
                                      : jgrf.executable);
    append_key_value(report, "Geolith core path", path_to_utf8(corePath));
    append_key_value(report, "Geolith version",
                     geolith.success ? geolith.version : "Unavailable");
    append_key_value(report, "JG API version",
                     geolith.success ? geolith.api_version : "Unavailable");
    append_key_value(report, "Geolith probe",
                     geolith.success ? "OK" : geolith.error);
    append_key_value(report, "Configured global video backend",
                     diagnostic_video_api_name(videoApi) + " (" +
                         std::to_string(videoApi) + ")");

    append_section(report, "Relevant configuration");
    append_goliath_configuration(
        report, request.config, request.session_verbose_logging);
    append_jgrf_configuration(report, request.paths);

    append_section(report, "Required file checks");
    append_file_check(report, result.required_file_problems,
                      "Goliath configuration",
                      request.paths.base_dir / "goliath.ini",
                      DirectPathKind::RegularFile, true);
    append_file_check(report, result.required_file_problems,
                      "Library database", request.paths.json_file,
                      DirectPathKind::RegularFile, true);
    append_file_check(report, result.required_file_problems,
                      "JGRF executable",
                      jgrf.executable.empty() ? request.paths.jollygood_exe
                                             : jgrf.executable,
                      DirectPathKind::RegularFile, true);
    append_file_check(report, result.required_file_problems,
                      "Geolith core", corePath,
                      DirectPathKind::RegularFile, true);
    append_file_check(report, result.required_file_problems,
                      "JGRF settings.ini", request.paths.jollygood_settings_ini,
                      DirectPathKind::RegularFile, false);
    append_file_check(report, result.required_file_problems,
                      "Geolith geolith.ini", request.paths.geolith_ini,
                      DirectPathKind::RegularFile, false);
    append_file_check(report, result.required_file_problems,
                      "Geolith input mappings", request.paths.input_config,
                      DirectPathKind::RegularFile, false);
    append_file_check(report, result.required_file_problems,
                      "Metadata directory", request.paths.metadata_dir,
                      DirectPathKind::Directory, true);

    static constexpr std::array<const char*, 4> metadataFiles = {{
        "geolith.xml", "neogeo.xml", "neocd.xml", "softwarelist.dtd",
    }};
    for (const char* filename : metadataFiles) {
        append_file_check(report, result.required_file_problems,
                          std::string("Metadata ") + filename,
                          request.paths.metadata_dir / filename,
                          DirectPathKind::RegularFile, true);
    }
    append_file_check(report, result.required_file_problems,
                      "LICENSE", request.paths.base_dir / "LICENSE",
                      DirectPathKind::RegularFile, true);
    append_file_check(report, result.required_file_problems,
                      "THIRD-PARTY-NOTICES.md",
                      request.paths.base_dir / "THIRD-PARTY-NOTICES.md",
                      DirectPathKind::RegularFile, true);
#if defined(_WIN32)
    append_file_check(report, result.required_file_problems,
                      "SDL3 runtime", request.paths.base_dir / "SDL3.dll",
                      DirectPathKind::RegularFile, true);
    append_file_check(report, result.required_file_problems,
                      "Qt Windows platform plugin",
                      request.paths.base_dir / "platforms" / "qwindows.dll",
                      DirectPathKind::RegularFile, true);
    const fs::path jgrfDirectory = jgrf.executable.empty()
        ? request.paths.jollygood_exe.parent_path()
        : jgrf.executable.parent_path();
    append_file_check(report, result.required_file_problems,
                      "JGRF libepoxy runtime",
                      jgrfDirectory / "libepoxy-0.dll",
                      DirectPathKind::RegularFile, true);
#endif

    if (videoApi == 3) {
        const fs::path shaderDirectory =
            (jgrf.executable.empty() ? request.paths.jollygood_exe.parent_path()
                                     : jgrf.executable.parent_path()) /
            "shaders";
        append_file_check(report, result.required_file_problems,
                          "Vulkan shader directory", shaderDirectory,
                          DirectPathKind::Directory, true);
        const std::size_t shaderCount = shader_file_count(shaderDirectory);
        report << (shaderCount == 0 ? "[PROBLEM] " : "[OK] ")
               << "Vulkan SPIR-V shader files: " << shaderCount << '\n';
        if (shaderCount == 0) ++result.required_file_problems;
    } else {
        report << "[NOT REQUIRED] Vulkan shader files: backend is "
               << diagnostic_video_api_name(videoApi) << '\n';
    }
    report << "Required-file problem count: "
           << result.required_file_problems << '\n';

    append_section(report, "BIOS verification");
    const std::optional<BiosVerifyResult> bios =
        verify_bios(request.paths.bios_dir);
    if (!bios) {
        result.bios_problems = 1;
        report << "[PROBLEM] BIOS directory is missing or unavailable.\n";
    } else {
        result.bios_problems = bios->problems;
        report << "BIOS problem count: " << bios->problems << '\n';
        for (const std::string& line : bios->lines) report << line << '\n';
    }

    append_log(report, "Recent Goliath frontend log (last 100 lines)",
               read_log_tail(request.paths.base_dir /
                             "goliath-qt-debug.log"));
    append_log(report, "Recent JGRF/Geolith log (last 100 lines)",
               read_log_tail(request.paths.base_dir / "jollygood.log"));

    append_section(report, "Summary");
    report << "Required-file problems: " << result.required_file_problems
           << '\n';
    report << "BIOS problems: " << result.bios_problems << '\n';
    report << "End of report.\n";

    result.text = redact_diagnostic_text(
        report.str(), redaction_rules(request));
    return result;
}

} // namespace goliath
