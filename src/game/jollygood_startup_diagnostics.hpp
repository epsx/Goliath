// jollygood_startup_diagnostics.hpp — recognizes stable JGRF/Geolith startup
// failures and turns them into actionable frontend guidance without changing
// the user's BIOS, hardware, region, or per-game profile automatically.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace goliath {

enum class JollygoodStartupIssue {
    BiosArchive,
    RequiresMvsOrUniverse,
    UnsupportedUsMvs,
};

struct JollygoodStartupDiagnosis {
    JollygoodStartupIssue issue = JollygoodStartupIssue::BiosArchive;
    std::string title;
    std::string explanation;
    std::string guidance;
    bool offer_bios_verification = false;
};

// Examines only the output produced by the current launch. Unknown or
// ambiguous messages deliberately return nullopt so the existing generic
// failure path remains authoritative.
std::optional<JollygoodStartupDiagnosis>
diagnose_jollygood_startup_failure(std::string_view output);

} // namespace goliath
