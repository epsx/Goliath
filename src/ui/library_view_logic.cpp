#include "ui/library_view_logic.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <unordered_map>
#include <utility>

namespace goliath {

LibraryCatalogIdPresentation libraryCatalogIdPresentation(
    const Game& game, const Rom* rom) {
    if (game.source == "redump") {
        const std::string id = game.redump_id.value_or("");
        bool numeric = !id.empty();
        for (const char rawCh : id) {
            const auto ch = static_cast<unsigned char>(rawCh);
            if (ch < '0' || ch > '9') {
                numeric = false;
                break;
            }
        }
        return {
            "Redump ID", id,
            numeric ? "https://redump.info/disc/" + id : "",
        };
    }

    if (rom && !rom->mame.empty())
        return {"MAME ID", rom->mame, ""};
    return {"MAME ID", game.short_name, ""};
}

LibraryVerificationPresentation libraryVerificationPresentation(
    const Game& game, const Rom* rom) {
    if (!game.identified) {
        return {
            "Unidentified", "",
            "No matching MAME software-list entry was identified.",
            VerificationTone::Neutral, false,
        };
    }

    if (rom && rom->verification ==
                   std::optional<std::string>("geolith-crc32")) {
        return {
            "Geolith / CRC-32", rom->crc32.value_or("Verified"),
            "The .neo file CRC-32 matches metadata/geolith.xml. "
            "Open details for the expected and calculated values.",
            VerificationTone::Success, !rom->hashes.empty(),
        };
    }
    if (rom && rom->verification ==
                   std::optional<std::string>(
                       "geolith-crc32-mismatch")) {
        return {
            "Geolith / CRC-32", rom->crc32.value_or("Mismatch"),
            "The .neo file does not match metadata/geolith.xml. "
            "Open details to compare the expected and calculated CRC-32.",
            VerificationTone::Error, !rom->hashes.empty(),
        };
    }

    if (game.verification ==
        std::optional<std::string>("redump-cue")) {
        return {
            "Redump / SHA-1", "Verified",
            "The CUE descriptor and every physical track match the local "
            "Redump DAT by size and SHA-1.",
            VerificationTone::Success, rom && !rom->hashes.empty(),
        };
    }
    if (game.verification ==
        std::optional<std::string>("redump-tracks-only")) {
        return {
            "Redump / SHA-1", "CUE mismatch",
            "Every physical track matches Redump by size and SHA-1, but the "
            "CUE descriptor differs.",
            VerificationTone::Error, rom && !rom->hashes.empty(),
        };
    }
    if (game.verification ==
        std::optional<std::string>("mame-chd")) {
        return {
            "MAME CHD / SHA-1", "Verified",
            "The CHD combined header SHA-1 matches the local MAME "
            "neocd.xml catalog.",
            VerificationTone::Success, rom && !rom->hashes.empty(),
        };
    }
    if (game.verification ==
            std::optional<std::string>("mame-chd-mismatch") ||
        (rom && rom->verification ==
                    std::optional<std::string>("mame-chd-mismatch"))) {
        return {
            "MAME CHD / SHA-1", "Mismatch",
            "The CHD combined header SHA-1 does not match the filename-"
            "identified MAME neocd.xml entry.",
            VerificationTone::Error, !rom->hashes.empty(),
        };
    }

    return {
        "Metadata only", "",
        "The media name was matched to catalog metadata, but its content "
        "hash has not been verified.",
        VerificationTone::Neutral, false,
    };
}

std::string libraryRememberedSelection(
        std::string_view preferredRom,
        std::string_view systemRom,
        std::string_view legacyRom) {
    if (!preferredRom.empty()) return std::string(preferredRom);
    if (!systemRom.empty()) return std::string(systemRom);
    return std::string(legacyRom);
}

void LibraryListViewportMemory::remember(
        std::string_view system, int verticalPosition) {
    m_positions[std::string(system)] = verticalPosition;
}

std::optional<int> LibraryListViewportMemory::recall(
        std::string_view system) const {
    const auto found = m_positions.find(std::string(system));
    if (found == m_positions.end()) return std::nullopt;
    return found->second;
}

std::string libraryMvsAesHumanTitle(
    const std::string& fullName, const std::string& displayName) {
    if (fullName.empty()) return displayName;

    std::string title = fullName;
    std::size_t searchFrom = 0;
    while (true) {
        const std::size_t open = title.find('(', searchFrom);
        if (open == std::string::npos) break;
        const std::size_t close = title.find(')', open + 1);
        if (close == std::string::npos) break;

        const std::string_view qualifier(title.data() + open + 1,
                                         close - open - 1);
        bool catalogCode = false;
        for (std::size_t index = 1; index + 1 < qualifier.size(); ++index) {
            if (qualifier[index] != '-' ||
                qualifier[index + 1] < '0' ||
                qualifier[index + 1] > '9') {
                continue;
            }

            std::size_t uppercaseLetters = 0;
            std::size_t cursor = index;
            while (cursor > 0 && qualifier[cursor - 1] >= 'A' &&
                   qualifier[cursor - 1] <= 'Z') {
                --cursor;
                ++uppercaseLetters;
            }
            if (uppercaseLetters >= 2) {
                catalogCode = true;
                break;
            }
        }
        if (!catalogCode) {
            searchFrom = close + 1;
            continue;
        }

        std::size_t eraseFrom = open;
        while (eraseFrom > 0 && title[eraseFrom - 1] == ' ')
            --eraseFrom;
        title.erase(eraseFrom, close - eraseFrom + 1);
        searchFrom = eraseFrom;
    }

    const std::size_t first = title.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return displayName;
    const std::size_t last = title.find_last_not_of(" \t\r\n");
    return title.substr(first, last - first + 1);
}

std::string libraryTileTitle(const Game& game, const Rom* rom) {
    if (!rom) return game.display;

    std::string title;
    if (rom->name.has_value() && !rom->name->empty())
        title = *rom->name;
    else if (rom->label.has_value() && !rom->label->empty())
        title = *rom->label;
    else if (!rom->mame.empty())
        title = rom->mame;
    else
        title = rom->file;

    if (game.system == "neogeocd")
        return title.empty() ? game.display : title;
    return libraryMvsAesHumanTitle(title, game.display);
}

std::string libraryListVariantTooltip(
        const Game& game, const Rom& rom) {
    if (game.system != "neogeo") return {};
    if (rom.name.has_value() && !rom.name->empty()) return *rom.name;
    if (rom.label.has_value() && !rom.label->empty()) return *rom.label;
    if (!rom.mame.empty()) return rom.mame;
    return rom.file;
}

std::string libraryDetailsTitle(
    bool isCd, const std::string& fullName,
    const std::string& displayName) {
    if (isCd) return fullName.empty() ? displayName : fullName;
    return libraryMvsAesHumanTitle(fullName, displayName);
}

namespace detail {

std::string cdListTrim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

std::string cdListLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return text;
}

std::vector<std::string> cdListCommaTokens(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t comma = text.find(',', begin);
        const std::size_t end = comma == std::string_view::npos
            ? text.size() : comma;
        std::string token = cdListTrim(text.substr(begin, end - begin));
        if (!token.empty()) tokens.push_back(std::move(token));
        if (comma == std::string_view::npos) break;
        begin = comma + 1;
    }
    return tokens;
}

std::string cdListCanonicalRegion(std::string_view token) {
    const std::string lower = cdListLower(cdListTrim(token));
    static constexpr std::array<std::pair<std::string_view, std::string_view>,
                                18> regions{{
        {"world", "World"}, {"export", "Export"},
        {"japan", "Japan"}, {"usa", "USA"},
        {"europe", "Europe"}, {"asia", "Asia"},
        {"france", "France"}, {"germany", "Germany"},
        {"italy", "Italy"}, {"spain", "Spain"},
        {"korea", "Korea"}, {"brazil", "Brazil"},
        {"australia", "Australia"}, {"canada", "Canada"},
        {"unl", "Unl"}, {"demo", "Demo"},
        {"prototype", "Prototype"}, {"beta", "Beta"},
    }};
    for (const auto& [key, value] : regions) {
        if (lower == key) return std::string(value);
    }
    return {};
}

std::string cdListCanonicalLanguage(std::string_view token) {
    const std::string lower = cdListLower(cdListTrim(token));
    static constexpr std::array<std::string_view, 20> languages{{
        "en", "ja", "es", "pt", "fr", "de", "it", "ko", "zh", "nl",
        "ru", "sv", "no", "da", "fi", "pl", "cs", "hu", "tr", "el",
    }};
    if (std::find(languages.begin(), languages.end(), lower) ==
        languages.end()) {
        return {};
    }
    std::string canonical = lower;
    canonical.front() = static_cast<char>(
        std::toupper(static_cast<unsigned char>(canonical.front())));
    return canonical;
}

std::string cdListCanonicalRevision(std::string_view token) {
    const std::string trimmed = cdListTrim(token);
    const std::string lower = cdListLower(trimmed);
    std::size_t valueStart = std::string::npos;
    if (lower.starts_with("revision ")) valueStart = 9;
    else if (lower.starts_with("rev ")) valueStart = 4;
    else if (lower.starts_with("rev.")) valueStart = 4;
    if (valueStart == std::string::npos) return {};

    const std::string value = cdListTrim(
        std::string_view(trimmed).substr(valueStart));
    return value.empty() ? std::string{} : "Rev " + value;
}

struct CdListQualifiers {
    std::vector<std::string> regions;
    std::string revision;
    std::vector<std::string> languages;
};

void cdListAppendUnique(std::vector<std::string>& values,
                        const std::string& value) {
    if (!value.empty() &&
        std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

CdListQualifiers cdListQualifiers(std::string_view fullName) {
    CdListQualifiers result;
    std::size_t searchFrom = 0;
    while (true) {
        const std::size_t open = fullName.find('(', searchFrom);
        if (open == std::string_view::npos) break;
        const std::size_t close = fullName.find(')', open + 1);
        if (close == std::string_view::npos) break;

        const std::vector<std::string> tokens = cdListCommaTokens(
            fullName.substr(open + 1, close - open - 1));
        for (const std::string& token : tokens) {
            const std::string revision = cdListCanonicalRevision(token);
            if (!revision.empty()) {
                result.revision = revision;
                continue;
            }
            const std::string region = cdListCanonicalRegion(token);
            if (!region.empty()) {
                cdListAppendUnique(result.regions, region);
                continue;
            }
            cdListAppendUnique(result.languages,
                               cdListCanonicalLanguage(token));
        }
        searchFrom = close + 1;
    }
    return result;
}

std::string cdListJoin(const std::vector<std::string>& values,
                       std::string_view separator) {
    std::string result;
    for (const std::string& value : values) {
        if (!result.empty()) result += separator;
        result += value;
    }
    return result;
}

std::string cdListQualifierText(const Game& game,
                                bool includeLanguages) {
    const CdListQualifiers qualifiers = cdListQualifiers(game.name);
    std::vector<std::string> parts = qualifiers.regions;
    if (!qualifiers.revision.empty())
        parts.push_back(qualifiers.revision);
    if (includeLanguages && !qualifiers.languages.empty())
        parts.push_back(cdListJoin(qualifiers.languages, ","));
    return cdListJoin(parts, " \xC2\xB7 ");
}

std::string cdListWithSuffix(const std::string& display,
                             const std::string& suffix) {
    return suffix.empty() ? display : display + " [" + suffix + "]";
}

std::string cdListFallbackIdentity(const Game& game) {
    if (game.redump_id.has_value() && !game.redump_id->empty())
        return "Redump " + *game.redump_id;
    if (!game.short_name.empty())
        return game.source == "mame"
            ? "MAME " + game.short_name : game.short_name;
    if (!game.main_rom.has_value() || game.main_rom->empty()) return {};

    std::string filename = *game.main_rom;
    const std::size_t slash = filename.find_last_of("/\\");
    if (slash != std::string::npos) filename.erase(0, slash + 1);
    const std::size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) filename.erase(dot);
    return filename;
}

} // namespace detail

std::vector<std::string> libraryGameListTitles(
        const std::vector<Game>& games) {
    std::vector<std::string> titles;
    titles.reserve(games.size());
    for (const Game& game : games) titles.push_back(game.display);

    std::unordered_map<std::string, std::size_t> baseCounts;
    for (const Game& game : games) {
        if (game.system == "neogeocd")
            ++baseCounts[detail::cdListLower(game.display)];
    }

    for (std::size_t index = 0; index < games.size(); ++index) {
        const Game& game = games[index];
        if (game.system != "neogeocd" ||
            baseCounts[detail::cdListLower(game.display)] < 2) {
            continue;
        }
        titles[index] = detail::cdListWithSuffix(
            game.display, detail::cdListQualifierText(game, false));
    }

    auto candidateCounts = [&](const std::vector<std::string>& values) {
        std::unordered_map<std::string, std::size_t> counts;
        for (std::size_t index = 0; index < games.size(); ++index) {
            if (games[index].system == "neogeocd" &&
                baseCounts[detail::cdListLower(games[index].display)] >= 2) {
                ++counts[detail::cdListLower(values[index])];
            }
        }
        return counts;
    };

    auto counts = candidateCounts(titles);
    for (std::size_t index = 0; index < games.size(); ++index) {
        const Game& game = games[index];
        if (game.system != "neogeocd" ||
            counts[detail::cdListLower(titles[index])] < 2) {
            continue;
        }
        titles[index] = detail::cdListWithSuffix(
            game.display, detail::cdListQualifierText(game, true));
    }

    counts = candidateCounts(titles);
    for (std::size_t index = 0; index < games.size(); ++index) {
        const Game& game = games[index];
        if (game.system != "neogeocd" ||
            counts[detail::cdListLower(titles[index])] < 2) {
            continue;
        }
        std::string suffix = detail::cdListQualifierText(game, true);
        const std::string identity = detail::cdListFallbackIdentity(game);
        if (!identity.empty()) {
            if (!suffix.empty()) suffix += " \xC2\xB7 ";
            suffix += identity;
        }
        titles[index] = detail::cdListWithSuffix(game.display, suffix);
    }
    return titles;
}

} // namespace goliath
