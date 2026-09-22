#include "game/command_dat.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

namespace goliath {
namespace {

constexpr std::uintmax_t kMaximumCommandDatBytes = 64u * 1024u * 1024u;

std::string trim_copy(const std::string& input) {
    auto first = input.begin();
    while (first != input.end() &&
           std::isspace(static_cast<unsigned char>(*first))) {
        ++first;
    }

    auto last = input.end();
    while (last != first &&
           std::isspace(static_cast<unsigned char>(*(last - 1)))) {
        --last;
    }
    return std::string(first, last);
}

std::string normalized_id(std::string id) {
    id = trim_copy(id);
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return id;
}

bool valid_id(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    return std::none_of(id.begin(), id.end(), [](unsigned char c) {
        return std::iscntrl(c) || std::isspace(c) || c == ',';
    });
}

std::vector<std::string> split_ids(const std::string& value,
                                   bool* valid) {
    std::vector<std::string> ids;
    std::unordered_set<std::string> seen;
    std::size_t start = 0;
    *valid = true;

    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        const std::size_t end = comma == std::string::npos
            ? value.size() : comma;
        std::string id = normalized_id(value.substr(start, end - start));
        if (!id.empty()) {
            if (!valid_id(id)) {
                *valid = false;
                return {};
            }
            if (seen.insert(id).second) ids.push_back(std::move(id));
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return ids;
}

std::string joined_body(std::vector<std::string> lines) {
    while (!lines.empty() && trim_copy(lines.front()).empty())
        lines.erase(lines.begin());
    while (!lines.empty() && trim_copy(lines.back()).empty())
        lines.pop_back();

    std::string result;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) result.push_back('\n');
        result += lines[i];
    }
    return result;
}

void add_lookup_id(std::vector<std::string>& ids,
                   const std::string& value) {
    const std::string id = normalized_id(value);
    if (!valid_id(id)) return;
    if (std::find(ids.begin(), ids.end(), id) == ids.end())
        ids.push_back(id);
}

} // namespace

void CommandDatCatalog::clear() {
    m_entries.clear();
    m_index.clear();
}

CommandDatLoadStatus CommandDatCatalog::load(const fs::path& path,
                                             std::string* error) {
    clear();
    if (error) error->clear();

    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec) {
        if (error) *error = "could not inspect command.dat: " + ec.message();
        return CommandDatLoadStatus::Error;
    }
    if (!exists) return CommandDatLoadStatus::Missing;
    if (!fs::is_regular_file(path, ec) || ec) {
        if (error) *error = "command.dat is not a regular file";
        return CommandDatLoadStatus::Error;
    }

    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec) {
        if (error) *error = "could not read command.dat size: " + ec.message();
        return CommandDatLoadStatus::Error;
    }
    if (size > kMaximumCommandDatBytes) {
        if (error) *error = "command.dat exceeds the 64 MiB safety limit";
        return CommandDatLoadStatus::Error;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "could not open command.dat";
        return CommandDatLoadStatus::Error;
    }
    std::string data((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    if (!input.eof() && input.fail()) {
        if (error) *error = "could not read command.dat";
        return CommandDatLoadStatus::Error;
    }

    if (data.size() >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
        data.erase(0, 3);
    }
    while (!data.empty() &&
           static_cast<unsigned char>(data.back()) == 0x1A) {
        data.pop_back();
    }

    enum class ParseState { Outside, AwaitingCommand, Body };
    ParseState state = ParseState::Outside;
    std::vector<std::string> currentIds;
    std::vector<std::string> bodyLines;
    std::size_t lineNumber = 0;

    auto fail = [&](const std::string& detail) {
        clear();
        if (error) {
            *error = "command.dat line " + std::to_string(lineNumber) +
                     ": " + detail;
        }
        return CommandDatLoadStatus::Error;
    };

    auto finish_entry = [&]() -> bool {
        const std::string body = joined_body(bodyLines);
        // The canonical database ends with an intentionally empty $info=
        // sentinel.  Consume but do not index it, even if it contains notes.
        if (currentIds.empty()) return true;
        if (body.empty()) return false;

        const std::size_t entryIndex = m_entries.size();
        for (const std::string& id : currentIds) {
            if (m_index.find(id) != m_index.end()) return false;
        }
        m_entries.push_back(CommandDatEntry{body});
        for (const std::string& id : currentIds)
            m_index.emplace(id, entryIndex);
        return true;
    };

    std::size_t offset = 0;
    while (offset <= data.size()) {
        const std::size_t newline = data.find('\n', offset);
        const std::size_t end = newline == std::string::npos
            ? data.size() : newline;
        std::string line = data.substr(offset, end - offset);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        ++lineNumber;
        const std::string trimmed = trim_copy(line);

        if (state == ParseState::Outside) {
            if (trimmed.rfind("$info=", 0) == 0) {
                bool idsValid = false;
                currentIds = split_ids(trimmed.substr(6), &idsValid);
                if (!idsValid) return fail("invalid MAME id in $info");
                bodyLines.clear();
                state = ParseState::AwaitingCommand;
            }
        } else if (state == ParseState::AwaitingCommand) {
            if (trimmed == "$cmd") {
                state = ParseState::Body;
            } else if (!trimmed.empty() && trimmed[0] != '#') {
                return fail("expected $cmd after $info");
            }
        } else {
            if (trimmed == "$end") {
                if (!finish_entry())
                    return fail("empty entry or duplicate MAME id");
                currentIds.clear();
                bodyLines.clear();
                state = ParseState::Outside;
            } else {
                bodyLines.push_back(std::move(line));
            }
        }

        if (newline == std::string::npos) break;
        offset = newline + 1;
    }

    if (state != ParseState::Outside)
        return fail("unterminated entry");
    return CommandDatLoadStatus::Loaded;
}

const CommandDatEntry* CommandDatCatalog::find(
    const std::string& mameId) const {
    const auto it = m_index.find(normalized_id(mameId));
    if (it == m_index.end() || it->second >= m_entries.size()) return nullptr;
    return &m_entries[it->second];
}

std::vector<std::string> command_dat_lookup_ids(const Game& game,
                                                int romIndex) {
    std::vector<std::string> result;

    if (romIndex >= 0 && romIndex < static_cast<int>(game.roms.size())) {
        const Rom& rom = game.roms[static_cast<std::size_t>(romIndex)];
        add_lookup_id(result, rom.mame);
        if (rom.cloneof.has_value()) add_lookup_id(result, *rom.cloneof);
    } else {
        const auto main = std::find_if(
            game.roms.begin(), game.roms.end(),
            [](const Rom& rom) { return rom.main; });
        if (main != game.roms.end()) {
            add_lookup_id(result, main->mame);
            if (main->cloneof.has_value()) add_lookup_id(result, *main->cloneof);
        }
    }
    add_lookup_id(result, game.short_name);
    return result;
}

} // namespace goliath
