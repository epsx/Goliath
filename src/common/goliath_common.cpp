#include "goliath_common.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <climits>
#else
    #include <unistd.h>
    #include <climits>
#endif

namespace fs = std::filesystem;

namespace goliath {

static std::string to_lower_local(std::string s);

static QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

// ---------- Config ----------

std::string Config::get(const std::string& section, const std::string& key,
                         const std::string& fallback) const {
    auto sec_it = sections.find(section);
    if (sec_it == sections.end()) return fallback;
    auto key_it = sec_it->second.find(key);
    if (key_it == sec_it->second.end()) return fallback;
    return key_it->second;
}

int Config::get_int(const std::string& section, const std::string& key, int fallback) const {
    std::string v = get(section, key, "");
    if (v.empty()) return fallback;
    try {
        return std::stoi(v);
    } catch (...) {
        return fallback;
    }
}

bool Config::get_bool(const std::string& section, const std::string& key, bool fallback) const {
    std::string v = to_lower_local(get(section, key, ""));
    if (v.empty()) return fallback;
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return fallback;
}

void Config::set(const std::string& section, const std::string& key, const std::string& value) {
    sections[section][key] = value;
}

bool Config::has(const std::string& section, const std::string& key) const {
    auto sec_it = sections.find(section);
    if (sec_it == sections.end()) return false;
    return sec_it->second.find(key) != sec_it->second.end();
}

// ---------- base dir ----------

fs::path get_base_dir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0) return fs::current_path();
    fs::path exe_path(std::wstring(buf, len));
    return exe_path.parent_path();
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    std::uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return fs::current_path();
    std::error_code ec;
    fs::path exe_path = fs::canonical(fs::path(buf), ec);
    if (ec) exe_path = fs::path(buf);
    return exe_path.parent_path();
#else
    std::error_code ec;
    fs::path exe_path = fs::canonical(fs::path("/proc/self/exe"), ec);
    if (ec) return fs::current_path();
    return exe_path.parent_path();
#endif
}

fs::path get_config_path() {
    return get_base_dir() / "goliath.ini";
}

// ---------- minimal INI parsing (configparser-like: sections + key=value) ----------

static std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string to_lower_local(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static Config parse_ini_text(const std::string& text) {
    Config cfg;
    std::istringstream stream(text);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == ';' || trimmed[0] == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));
        if (!current_section.empty()) {
            cfg.sections[current_section][key] = value;
        }
    }
    return cfg;
}

// ---------- defaults ----------

Config default_config() {
    Config cfg;
    cfg.sections["Audio"] = {
        {"volume", "100"},
    };
    cfg.sections["Paths"] = {
        {"roms", "roms"},
        {"neocd", "neocd"},
        {"icons", "icons"},
        {"snaps", "snaps"},
        {"metadata", "metadata"},
        {"bios", "bios"},
        {"database", "database"},
        {"jollygood", "jollygood"},
        {"jollygood_args", "-c geolith"},
        {"config", "config"},
    };
    cfg.sections["UI"] = {
        {"window_width", "1200"},
        {"window_height", "800"},
        {"last_rom", ""},
        {"library_system", "neogeo"},
    };
    return cfg;
}

static std::string serialize_ini(const Config& cfg) {
    std::ostringstream out;
    // Sections/keys are written in std::map order (alphabetical).
    for (const auto& [section, kv] : cfg.sections) {
        out << "[" << section << "]\n";
        for (const auto& [key, value] : kv) {
            out << key << " = " << value << "\n";
        }
        out << "\n";
    }
    return out.str();
}

static bool write_config_file(const fs::path& path, const Config& config) {
    const QByteArray serialized =
        QByteArray::fromStdString(serialize_ini(config));
    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        std::cerr << "[goliath] Warning: could not open " << path.string()
                  << " for writing: " << output.errorString().toStdString()
                  << "\n";
        return false;
    }
    if (output.write(serialized) != serialized.size()) {
        std::cerr << "[goliath] Warning: could not write " << path.string()
                  << ": " << output.errorString().toStdString() << "\n";
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        std::cerr << "[goliath] Warning: could not replace " << path.string()
                  << ": " << output.errorString().toStdString() << "\n";
        return false;
    }
    return true;
}

fs::path ensure_config() {
    fs::path ini_path = get_config_path();
    if (!fs::exists(ini_path))
        write_config_file(ini_path, default_config());
    return ini_path;
}

Config load_config() {
    fs::path ini_path = ensure_config();

    std::ifstream in(ini_path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    Config cfg = parse_ini_text(buf.str());

    // Fill in any section/key missing from the file (in-memory only).
    Config defaults = default_config();
    for (const auto& [section, kv] : defaults.sections) {
        for (const auto& [key, value] : kv) {
            if (!cfg.has(section, key)) {
                cfg.sections[section][key] = value;
            }
        }
    }
    return cfg;
}

void save_config(const Config& config) {
    write_config_file(get_config_path(), config);
}

// ---------- path resolution ----------

fs::path resolve_path(const Config& config, const std::string& key,
                       const std::optional<fs::path>& base_dir_opt) {
    fs::path base_dir = base_dir_opt.has_value() ? *base_dir_opt : get_base_dir();

    std::string value = config.get("Paths", key, "");
    if (value.empty()) {
        return base_dir;
    }

    fs::path p(value);
    if (p.is_absolute()) {
        return p;
    }
    return base_dir / p;
}

} // namespace goliath
