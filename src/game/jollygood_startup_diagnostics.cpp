#include "game/jollygood_startup_diagnostics.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace goliath {
namespace {

std::string lower_ascii(std::string_view text) {
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return lowered;
}

} // namespace

std::optional<JollygoodStartupDiagnosis>
diagnose_jollygood_startup_failure(std::string_view output) {
    const std::string lowered = lower_ascii(output);

    std::size_t start = 0;
    while (start <= lowered.size()) {
        const std::size_t end = lowered.find('\n', start);
        const std::string_view line(lowered.data() + start,
            (end == std::string::npos ? lowered.size() : end) - start);

        if (line.find("failed to load ") != std::string_view::npos &&
            line.find(" from bios archive") != std::string_view::npos) {
            return JollygoodStartupDiagnosis{
                JollygoodStartupIssue::BiosArchive,
                "Required BIOS content could not be loaded",
                "Geolith could not load one or more required files from the "
                "selected BIOS archive.",
                "Run Tools > Verify BIOS. Repair or replace the archive "
                "reported in Recent JGRF output, then try the game again.",
                true,
            };
        }

        if (line.find(
                "compatible with the neo geo mvs and universe bios only") !=
                std::string_view::npos ||
            line.find("incompatible with aes systems") !=
                std::string_view::npos) {
            return JollygoodStartupDiagnosis{
                JollygoodStartupIssue::RequiresMvsOrUniverse,
                "This game cannot use AES mode",
                "Geolith reports that this exact media requires MVS hardware "
                "or Universe BIOS.",
                "Open Game settings > General for this exact media, choose "
                "MVS or Universe BIOS, then launch it again.",
                false,
            };
        }

        if (line.find("incompatible with us mvs systems") !=
            std::string_view::npos) {
            return JollygoodStartupDiagnosis{
                JollygoodStartupIssue::UnsupportedUsMvs,
                "This game cannot use US MVS",
                "Geolith reports that this exact media is incompatible with "
                "the US MVS combination.",
                "Open Game settings > General for this exact media and choose "
                "a different supported region or system type, then launch it "
                "again.",
                false,
            };
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }

    return std::nullopt;
}

} // namespace goliath
