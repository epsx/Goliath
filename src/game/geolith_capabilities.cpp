#include "game/geolith_capabilities.hpp"

#include "game/jollygood_capabilities.hpp"
#include "game/jollygood_executable.hpp"

#include <QLibrary>
#include <QString>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {
namespace {

struct JgCoreInfoAbi {
    const char* name;
    const char* fname;
    const char* version;
    const char* sys;
    std::uint8_t numinputs;
    std::uint32_t hints;
};

struct JgSystemInfoAbi {
    const char* name;
    const char* fname;
    const char* ext;
};

std::string lower_trimmed(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }

    std::string out(value.substr(first, last - first));
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    while (!out.empty() && out.front() == '.')
        out.erase(out.begin());
    return out;
}

} // namespace

fs::path geolith_library_for_jgrf(const fs::path& jollygoodExe) {
#if defined(_WIN32)
    constexpr const char* extension = ".dll";
#elif defined(__APPLE__)
    constexpr const char* extension = ".dylib";
#else
    constexpr const char* extension = ".so";
#endif
    if (jollygoodExe.empty()) return {};
    return jollygoodExe.parent_path() / "cores" / "geolith" /
           (std::string("geolith") + extension);
}

std::vector<std::string> parse_jg_extension_list(std::string_view extensions) {
    std::vector<std::string> result;
    std::size_t start = 0;

    while (start <= extensions.size()) {
        const std::size_t comma = extensions.find(',', start);
        const std::size_t end = comma == std::string_view::npos ? extensions.size() : comma;
        std::string value = lower_trimmed(extensions.substr(start, end - start));
        if (!value.empty() && std::find(result.begin(), result.end(), value) == result.end())
            result.push_back(std::move(value));
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }

    return result;
}

GeolithCapabilities probe_geolith_capabilities(const fs::path& configuredJollygood) {
    GeolithCapabilities result;

    const fs::path exe = resolve_jollygood_executable(configuredJollygood);
    if (exe.empty()) {
        result.error = "Could not resolve the JGRF executable.";
        return result;
    }

    result.core_library = geolith_library_for_jgrf(exe);
    if (result.core_library.empty()) {
        result.error = "Could not resolve the Geolith core library path.";
        return result;
    }

    std::error_code ec;
    if (!fs::is_regular_file(result.core_library, ec)) {
        result.error = "Geolith core library was not found at JGRF's local core path.";
        return result;
    }

    QLibrary library(QString::fromStdString(result.core_library.string()));
    if (!library.load()) {
        result.error = library.errorString().toStdString();
        return result;
    }

    using GetCoreInfoFn = JgCoreInfoAbi* (*)(const char*);
    using GetSystemListFn = JgSystemInfoAbi* (*)(std::size_t*);
    using ApiVersionFn = unsigned (*)();

    const auto getCoreInfo = reinterpret_cast<GetCoreInfoFn>(library.resolve("jg_get_coreinfo"));
    const auto getSystemList = reinterpret_cast<GetSystemListFn>(library.resolve("jg_get_systemlist"));
    const auto getApiVersion = reinterpret_cast<ApiVersionFn>(library.resolve("jg_api_version"));

    if (!getCoreInfo || !getSystemList || !getApiVersion) {
        result.error = "Required Jolly Good API capability symbols were not found in the Geolith library.";
        library.unload();
        return result;
    }

    JgCoreInfoAbi* coreInfo = getCoreInfo("");
    if (!coreInfo || !coreInfo->version) {
        result.error = "Geolith did not return valid core information.";
        library.unload();
        return result;
    }

    result.version = coreInfo->version;
    result.api_version = format_jg_api_version(getApiVersion());

    std::size_t systemCount = 0;
    JgSystemInfoAbi* systems = getSystemList(&systemCount);
    if (!systems || systemCount == 0) {
        result.error = "Geolith did not advertise any JG systems/media extensions.";
        library.unload();
        return result;
    }

    result.systems.reserve(systemCount);
    for (std::size_t i = 0; i < systemCount; ++i) {
        const JgSystemInfoAbi& system = systems[i];
        if (!system.name || !*system.name) continue;

        GeolithSystemCapability capability;
        capability.name = system.name;
        if (system.fname) capability.full_name = system.fname;
        if (system.ext) capability.extensions = parse_jg_extension_list(system.ext);
        result.systems.push_back(std::move(capability));
    }

    result.success = !result.systems.empty();
    if (!result.success)
        result.error = "Geolith returned an empty JG system list.";

    library.unload();
    return result;
}

std::vector<std::string> geolith_extensions_for_system(
        const GeolithCapabilities& capabilities, std::string_view system) {
    const std::string wanted = lower_trimmed(system);
    for (const auto& entry : capabilities.systems) {
        if (lower_trimmed(entry.name) == wanted)
            return entry.extensions;
    }
    return {};
}

bool geolith_supports_extension(const GeolithCapabilities& capabilities,
                                std::string_view system,
                                std::string_view extension) {
    const std::string wanted = lower_trimmed(extension);
    if (wanted.empty()) return false;

    const auto extensions = geolith_extensions_for_system(capabilities, system);
    return std::find(extensions.begin(), extensions.end(), wanted) != extensions.end();
}

std::string geolith_extensions_display(const GeolithCapabilities& capabilities,
                                       std::string_view system) {
    const auto extensions = geolith_extensions_for_system(capabilities, system);
    if (extensions.empty()) return {};

    std::ostringstream out;
    for (std::size_t i = 0; i < extensions.size(); ++i) {
        if (i != 0) out << ", ";
        std::string upper = extensions[i];
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        out << upper;
    }
    return out.str();
}

} // namespace goliath
