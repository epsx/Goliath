// command_dat.hpp — optional command.dat loading and MAME-id lookup.
//
// Goliath never ships this third-party database.  When a user places a copy
// in the configured metadata directory, these helpers parse it for the
// companion command-list window shown alongside a launched game.
#pragma once

#include "game/game_model.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace goliath {

enum class CommandDatLoadStatus {
    Missing,
    Loaded,
    Error,
};

struct CommandDatEntry {
    std::string text;
};

class CommandDatCatalog {
public:
    CommandDatLoadStatus load(const std::filesystem::path& path,
                              std::string* error = nullptr);

    const CommandDatEntry* find(const std::string& mameId) const;
    std::size_t entry_count() const { return m_entries.size(); }
    std::size_t id_count() const { return m_index.size(); }
    void clear();

private:
    std::vector<CommandDatEntry> m_entries;
    std::unordered_map<std::string, std::size_t> m_index;
};

// Candidate ids are ordered from the exact selected variant to increasingly
// broad fallbacks.  The first command.dat match wins.
std::vector<std::string> command_dat_lookup_ids(const Game& game,
                                                int romIndex);

} // namespace goliath
