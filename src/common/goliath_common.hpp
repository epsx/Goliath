// goliath_common.hpp — Config (an in-memory INI document: section -> key ->
// value) plus the app-wide config helpers built on top of it:
//
//   - get_base_dir() returns the folder containing the running executable
//     (via /proc/self/exe on Linux, GetModuleFileNameW on Windows) - NOT the
//     current working directory. This is what makes the whole app portable:
//     move the folder anywhere and every relative config path still resolves
//     correctly.
//   - load_config() reads "goliath.ini" next to the executable, creating it
//     from default_config() first if it doesn't exist yet, then fills in any
//     section/key missing from the file (in-memory only - not written back
//     to disk unless save_config() is called afterwards).
//   - resolve_path(cfg, key, base_dir) reads cfg["Paths"][key]: empty/missing
//     -> returns base_dir itself (not an empty path); relative -> joined onto
//     base_dir; absolute -> returned as-is. No "~" expansion.

#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace goliath {

class Config {
public:
    // section -> (key -> value)
    std::map<std::string, std::map<std::string, std::string>> sections;

    std::string get(const std::string& section, const std::string& key,
                     const std::string& fallback = "") const;
    // Typed accessors for C++ callers that need an int/bool instead of a
    // string. An unrecognized/empty value falls back instead of throwing.
    int get_int(const std::string& section, const std::string& key, int fallback) const;
    bool get_bool(const std::string& section, const std::string& key, bool fallback) const;
    void set(const std::string& section, const std::string& key, const std::string& value);
    bool has(const std::string& section, const std::string& key) const;
};

// Folder containing the running executable. All relative config paths are
// resolved against this, so the tool keeps working if moved anywhere.
std::filesystem::path get_base_dir();

// Path to goliath.ini inside get_base_dir().
std::filesystem::path get_config_path();

// The canonical default Config: every section/key/value goliath.ini is
// expected to have out of the box.
Config default_config();

// Creates goliath.ini with defaults if it doesn't exist yet. Returns its path.
std::filesystem::path ensure_config();

// Loads goliath.ini (creating it first if needed via ensure_config()), then
// fills in any keys/sections missing from the file using default_config()
// (in-memory only - not written back to disk).
Config load_config();

// Writes the config back to goliath.ini next to the executable.
void save_config(const Config& config);

// Resolves a key from the [Paths] section. Empty/missing value -> base_dir
// itself; relative value -> joined onto base_dir; absolute value -> used
// as-is. base_dir defaults to get_base_dir() when not given.
std::filesystem::path resolve_path(const Config& config, const std::string& key,
                                    const std::optional<std::filesystem::path>& base_dir = std::nullopt);

} // namespace goliath
