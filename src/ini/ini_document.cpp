#include "ini_document.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace goliath {

namespace {
std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}
} // namespace

IniDocument::Section* IniDocument::find_section(const std::string& name) {
    for (auto& s : m_sections) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

const IniDocument::Section* IniDocument::find_section(const std::string& name) const {
    for (const auto& s : m_sections) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

void IniDocument::load(const fs::path& path) {
    m_sections.clear();
    if (!fs::is_regular_file(path)) return;

    std::ifstream in(path, std::ios::binary);
    std::string line;
    Section* current = nullptr;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(line);
        if (t.empty()) continue;
        if (t[0] == ';' || t[0] == '#') continue;

        if (t.front() == '[' && t.back() == ']') {
            std::string name = trim(t.substr(1, t.size() - 2));
            m_sections.push_back(Section{name, {}});
            current = &m_sections.back();
            continue;
        }

        auto eq = t.find('=');
        if (eq == std::string::npos || !current) continue;

        std::string key = trim(t.substr(0, eq));
        std::string value = trim(t.substr(eq + 1));
        current->entries.push_back(Entry{key, value});
    }
}

bool IniDocument::save(const fs::path& path, std::string* error) const {
    if (error) error->clear();

    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            if (error) *error = "Could not create INI directory: " + ec.message();
            return false;
        }
    }

    std::ostringstream serialized;
    for (const auto& section : m_sections) {
        serialized << "[" << section.name << "]\n";
        for (const auto& e : section.entries) {
            serialized << e.key << " = " << e.value << "\n";
        }
        serialized << "\n";
    }

    const QByteArray bytes = QByteArray::fromStdString(serialized.str());
    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = "Could not open INI file for writing: " + path.string() +
                     ": " + output.errorString().toStdString();
        }
        return false;
    }
    if (output.write(bytes) != bytes.size()) {
        if (error) {
            *error = "Could not write INI file: " + path.string() +
                     ": " + output.errorString().toStdString();
        }
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        if (error) {
            *error = "Could not replace INI file: " + path.string() +
                     ": " + output.errorString().toStdString();
        }
        return false;
    }
    return true;
}

bool IniDocument::has_section(const std::string& section) const {
    return find_section(section) != nullptr;
}

bool IniDocument::has_option(const std::string& section, const std::string& key) const {
    const Section* s = find_section(section);
    if (!s) return false;
    for (const auto& e : s->entries) {
        if (e.key == key) return true;
    }
    return false;
}

std::string IniDocument::get(const std::string& section, const std::string& key,
                              const std::string& fallback) const {
    const Section* s = find_section(section);
    if (!s) return fallback;
    for (const auto& e : s->entries) {
        if (e.key == key) return e.value;
    }
    return fallback;
}

void IniDocument::set(const std::string& section, const std::string& key, const std::string& value) {
    Section* s = find_section(section);
    if (!s) {
        m_sections.push_back(Section{section, {}});
        s = &m_sections.back();
    }
    for (auto& e : s->entries) {
        if (e.key == key) {
            e.value = value;
            return;
        }
    }
    s->entries.push_back(Entry{key, value});
}

void IniDocument::ensure_section(const std::string& section) {
    if (!find_section(section)) {
        m_sections.push_back(Section{section, {}});
    }
}

void IniDocument::remove_option(const std::string& section, const std::string& key) {
    Section* s = find_section(section);
    if (!s) return;
    s->entries.erase(std::remove_if(s->entries.begin(), s->entries.end(),
                                     [&](const Entry& e) { return e.key == key; }),
                      s->entries.end());
}

void IniDocument::remove_section(const std::string& section) {
    m_sections.erase(std::remove_if(m_sections.begin(), m_sections.end(),
                                     [&](const Section& s) { return s.name == section; }),
                      m_sections.end());
}

std::vector<std::string> IniDocument::sections() const {
    std::vector<std::string> result;
    result.reserve(m_sections.size());
    for (const auto& s : m_sections) result.push_back(s.name);
    return result;
}

std::vector<std::string> IniDocument::options(const std::string& section) const {
    std::vector<std::string> result;
    const Section* s = find_section(section);
    if (!s) return result;
    result.reserve(s->entries.size());
    for (const auto& e : s->entries) result.push_back(e.key);
    return result;
}

} // namespace goliath
