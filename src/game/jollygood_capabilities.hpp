// jollygood_capabilities.hpp — lightweight helpers for detecting optional
// capabilities exposed by the exact JGRF executable configured in Goliath.
#pragma once

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace goliath {

// Capability probes must never keep a Settings or exact-media dialog waiting
// indefinitely when a configured executable starts but fails to finish.
inline constexpr int kJgrfHelpProbeTimeoutMs = 3000;

// JGRF conditionally prints "3 = Vulkan" in --help only when it was built
// with JGRF_VULKAN / ENABLE_VULKAN=1. Normalize whitespace/case so the probe
// is not tied to the help text's indentation.
inline bool jgrf_help_reports_vulkan(std::string_view helpText) {
    std::string normalized;
    normalized.reserve(helpText.size());
    for (const char ch : helpText) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch)) continue;
        normalized.push_back(static_cast<char>(std::tolower(uch)));
    }
    return normalized.find("3=vulkan") != std::string::npos;
}

// Extract the version from the first line printed by a normal dynamic JGRF
// build, e.g. "The Jolly Good Reference Frontend 2.0.1".
inline std::optional<std::string> jgrf_version_from_help(std::string_view helpText) {
    constexpr std::string_view prefix = "The Jolly Good Reference Frontend ";

    std::size_t pos = 0;
    while (pos <= helpText.size()) {
        const std::size_t end = helpText.find('\n', pos);
        std::string_view line = helpText.substr(
            pos, end == std::string_view::npos ? helpText.size() - pos : end - pos);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        if (line.starts_with(prefix)) {
            line.remove_prefix(prefix.size());
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())))
                line.remove_prefix(1);
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
                line.remove_suffix(1);
            if (!line.empty()) return std::string(line);
        }

        if (end == std::string_view::npos) break;
        pos = end + 1;
    }
    return std::nullopt;
}

// JG_VERSION_NUMBER = patch + minor*100 + major*10000.
inline std::string format_jg_api_version(unsigned version) {
    const unsigned major = version / 10000U;
    const unsigned minor = (version / 100U) % 100U;
    const unsigned patch = version % 100U;
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
}

} // namespace goliath
