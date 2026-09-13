// geolith_capabilities.hpp — probe the exact Geolith core installed next to
// JGRF and expose its advertised JG systems/media extensions.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace goliath {

struct GeolithSystemCapability {
    std::string name;
    std::string full_name;
    std::vector<std::string> extensions;
};

struct GeolithCapabilities {
    bool success = false;
    std::filesystem::path core_library;
    std::string version;
    std::string api_version;
    std::vector<GeolithSystemCapability> systems;
    std::string error;
};

// Stock JGRF local-core layout used by this Goliath installation.
std::filesystem::path geolith_library_for_jgrf(const std::filesystem::path& jollygood_exe);

// Load the installed Geolith shared library and query JG core/system metadata.
GeolithCapabilities probe_geolith_capabilities(const std::filesystem::path& configured_jollygood);

// Pure helpers used by the Info UI, launch policy, and unit tests.
std::vector<std::string> parse_jg_extension_list(std::string_view extensions);
std::vector<std::string> geolith_extensions_for_system(
    const GeolithCapabilities& capabilities, std::string_view system);
bool geolith_supports_extension(const GeolithCapabilities& capabilities,
                                std::string_view system,
                                std::string_view extension);
std::string geolith_extensions_display(const GeolithCapabilities& capabilities,
                                       std::string_view system);

} // namespace goliath
