// ini_document.hpp — a small order-preserving INI document. Used for
// jollygood's settings.ini, geolith.ini and geolith_input.ini, all of which
// have far more keys than this UI exposes - load()/save() round-trip
// whatever was already in the file besides the few keys actually touched.
//
// Unlike goliath::Config (used only for goliath.ini), keys here keep their
// original case, since jollygood/geolith's own ini files are case-sensitive.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace goliath {

class IniDocument {
public:
    void load(const std::filesystem::path& path); // missing file -> empty document, no error
    // Saves the complete document transactionally. A failed save leaves an
    // existing destination untouched and optionally describes the error.
    bool save(const std::filesystem::path& path,
              std::string* error = nullptr) const;

    bool has_section(const std::string& section) const;
    bool has_option(const std::string& section, const std::string& key) const;
    std::string get(const std::string& section, const std::string& key,
                     const std::string& fallback = "") const;
    void set(const std::string& section, const std::string& key, const std::string& value);
    void ensure_section(const std::string& section); // creates an empty section if missing (no-op if present)
    void remove_option(const std::string& section, const std::string& key);
    void remove_section(const std::string& section);

    std::vector<std::string> sections() const;
    // Raw keys as found in the file, in file order (or insertion order for
    // freshly-set keys).
    std::vector<std::string> options(const std::string& section) const;

private:
    struct Entry { std::string key, value; };
    struct Section { std::string name; std::vector<Entry> entries; };
    std::vector<Section> m_sections;

    Section* find_section(const std::string& name);
    const Section* find_section(const std::string& name) const;
};

} // namespace goliath
