#include "input/controller_profiles.hpp"

#include "common/qt_path.hpp"
#include "input/input_maps.hpp"
#include "json.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <exception>
#include <fstream>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;
using nlohmann::json;

namespace goliath {

namespace {

constexpr int kControllerProfileSchemaVersion = 1;
constexpr std::size_t kMaximumProfileNameLength = 64;

std::string trim_name(std::string value) {
    const auto first = std::find_if_not(
        value.begin(), value.end(), [](char ch) {
            return std::isspace(static_cast<unsigned char>(ch)) != 0;
        });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(), [](char ch) {
            return std::isspace(static_cast<unsigned char>(ch)) != 0;
        }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string folded_name(const std::string& value) {
    std::string folded;
    folded.reserve(value.size());
    for (const char ch : value) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        folded.push_back(static_cast<char>(std::tolower(byte)));
    }
    return folded;
}

bool valid_name(const std::string& name) {
    if (name.empty() || name.size() > kMaximumProfileNameLength) return false;
    return std::none_of(name.begin(), name.end(), [](char ch) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        return byte < 0x20U || byte == 0x7fU;
    });
}

bool valid_device(int device) {
    return device >= 1 && device <= 5;
}

std::string make_profile_id(
        const std::vector<ControllerProfile>& profiles) {
    const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for (std::uint64_t suffix = 0; suffix < 10000; ++suffix) {
        std::ostringstream stream;
        stream << std::hex << static_cast<std::uint64_t>(ticks);
        if (suffix != 0) stream << '-' << suffix;
        const std::string candidate = stream.str();
        const bool exists = std::any_of(
            profiles.begin(), profiles.end(), [&](const ControllerProfile& profile) {
                return profile.id == candidate;
            });
        if (!exists) return candidate;
    }
    return {};
}

json bindings_to_json(const ControllerProfileBindings& bindings) {
    json object = json::object();
    for (const auto& [section, keys] : bindings) {
        json values = json::object();
        for (const auto& [key, value] : keys) values[key] = value;
        object[section] = std::move(values);
    }
    return object;
}

ControllerProfileBindings bindings_from_json(const json& object) {
    ControllerProfileBindings bindings;
    if (!object.is_object()) return bindings;
    for (auto section = object.begin(); section != object.end(); ++section) {
        if (!section.value().is_object()) continue;
        auto& destination = bindings[section.key()];
        for (auto key = section.value().begin();
             key != section.value().end(); ++key) {
            if (key.value().is_string()) {
                destination[key.key()] = key.value().get<std::string>();
            }
        }
    }
    return bindings;
}

bool fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

} // namespace

std::string controller_profile_device_name(int device) {
    switch (device) {
    case 1: return "Neo Geo Joysticks";
    case 2: return "Mahjong";
    case 3: return "4-Player (AS/JP MVS)";
    case 4: return "V-Liner";
    case 5: return "Irritating Maze";
    default: return "Unknown input layout";
    }
}

std::vector<std::string> controller_profile_sections(int device) {
    switch (device) {
    case 1: return {"neogeosystem", "neogeojs1", "neogeojs2"};
    case 2: return {"neogeosystem", "neogeomahjong"};
    case 3:
        return {"neogeosystem", "neogeojs1", "neogeojs2", "neogeojs3",
                "neogeojs4"};
    case 4: return {"neogeosystem", "neogeovliner"};
    case 5: return {"neogeosystem", "neogeoirrmaze"};
    default: return {};
    }
}

std::size_t controller_profile_binding_count(
        const ControllerProfileBindings& bindings) {
    std::size_t count = 0;
    for (const auto& [section, keys] : bindings) {
        (void)section;
        count += static_cast<std::size_t>(std::count_if(
            keys.begin(), keys.end(), [](const auto& entry) {
                return !entry.second.empty();
            }));
    }
    return count;
}

ControllerProfileBindings sanitize_controller_profile_bindings(
        int device,
        const ControllerProfileBindings& bindings) {
    ControllerProfileBindings sanitized;
    for (const std::string& section : controller_profile_sections(device)) {
        const auto definitions = input_defs().find(section);
        if (definitions == input_defs().end()) continue;
        auto& destination = sanitized[section];
        const auto sourceSection = bindings.find(section);
        for (const std::string& key : definitions->second) {
            std::string value;
            if (sourceSection != bindings.end()) {
                const auto sourceValue = sourceSection->second.find(key);
                if (sourceValue != sourceSection->second.end()) {
                    value = sourceValue->second;
                }
            }
            if (value.empty()) {
                destination[key] = {};
                continue;
            }
            const auto normalized = normalize_input_binding_for_definition(
                section, key, value);
            destination[key] = normalized.value_or(std::string());
        }
    }
    return sanitized;
}

ControllerProfileBindings controller_profile_assigned_bindings(
        int device,
        const ControllerProfileBindings& bindings) {
    ControllerProfileBindings assigned;
    for (const auto& [section, keys] :
         sanitize_controller_profile_bindings(device, bindings)) {
        for (const auto& [key, value] : keys) {
            if (!value.empty()) assigned[section][key] = value;
        }
    }
    return assigned;
}

bool ControllerProfileStore::load(const fs::path& path,
                                  std::string* error) {
    m_profiles.clear();
    if (error) error->clear();

    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec) return fail(error, "Could not inspect controller profiles: " + ec.message());
    if (!exists) return true;
    if (!fs::is_regular_file(path, ec) || ec) {
        return fail(error, "Controller profile path is not a regular file.");
    }

    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) return fail(error, "Could not open controller profiles.");
        json root;
        input >> root;
        if (!root.is_object() || !root.contains("version") ||
            !root.at("version").is_number_integer() ||
            root.at("version").get<int>() != kControllerProfileSchemaVersion ||
            !root.contains("profiles") || !root.at("profiles").is_array()) {
            return fail(error, "Unsupported or malformed controller profile schema.");
        }

        for (const json& item : root.at("profiles")) {
            if (!item.is_object() || !item.contains("id") ||
                !item.at("id").is_string() || !item.contains("name") ||
                !item.at("name").is_string() || !item.contains("device") ||
                !item.at("device").is_number_integer() ||
                !item.contains("bindings")) {
                continue;
            }
            ControllerProfile profile;
            profile.id = item.at("id").get<std::string>();
            profile.name = trim_name(item.at("name").get<std::string>());
            profile.device = item.at("device").get<int>();
            if (profile.id.empty() || !valid_name(profile.name) ||
                !valid_device(profile.device) || find(profile.id) ||
                !nameAvailable(profile.name)) {
                continue;
            }
            profile.bindings = sanitize_controller_profile_bindings(
                profile.device, bindings_from_json(item.at("bindings")));
            m_profiles.push_back(std::move(profile));
        }
    } catch (const std::exception& ex) {
        m_profiles.clear();
        return fail(error, std::string("Could not parse controller profiles: ") + ex.what());
    }
    return true;
}

bool ControllerProfileStore::save(const fs::path& path,
                                  std::string* error) const {
    if (error) error->clear();
    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) return fail(error, "Could not create controller profile directory: " + ec.message());
    }

    json root;
    root["version"] = kControllerProfileSchemaVersion;
    root["profiles"] = json::array();
    for (const ControllerProfile& profile : m_profiles) {
        root["profiles"].push_back({
            {"id", profile.id},
            {"name", profile.name},
            {"device", profile.device},
            {"bindings", bindings_to_json(profile.bindings)},
        });
    }

    const QByteArray serialized = QByteArray::fromStdString(root.dump(2) + '\n');
    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        return fail(error, "Could not open controller profile file for writing: " +
                           output.errorString().toStdString());
    }
    if (output.write(serialized) != serialized.size()) {
        const std::string message = "Could not write controller profiles: " +
                                    output.errorString().toStdString();
        output.cancelWriting();
        return fail(error, message);
    }
    if (!output.commit()) {
        return fail(error, "Could not replace controller profile file: " +
                           output.errorString().toStdString());
    }
    return true;
}

const std::vector<ControllerProfile>& ControllerProfileStore::profiles() const noexcept {
    return m_profiles;
}

const ControllerProfile* ControllerProfileStore::find(
        const std::string& id) const {
    const auto it = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&](const ControllerProfile& profile) {
            return profile.id == id;
        });
    return it == m_profiles.end() ? nullptr : &*it;
}

bool ControllerProfileStore::add(std::string name,
                                 int device,
                                 ControllerProfileBindings bindings,
                                 std::string* newId,
                                 std::string* error) {
    if (error) error->clear();
    name = trim_name(std::move(name));
    if (!valid_name(name)) {
        return fail(error, "Enter a profile name between 1 and 64 characters.");
    }
    if (!valid_device(device)) return fail(error, "Unknown input layout.");
    if (!nameAvailable(name)) return fail(error, "A profile with this name already exists.");
    const std::string id = make_profile_id(m_profiles);
    if (id.empty()) return fail(error, "Could not allocate a controller profile identifier.");
    m_profiles.push_back(ControllerProfile{
        id, std::move(name), device,
        sanitize_controller_profile_bindings(device, bindings)});
    if (newId) *newId = id;
    return true;
}

bool ControllerProfileStore::replace(const std::string& id,
                                     int device,
                                     ControllerProfileBindings bindings,
                                     std::string* error) {
    if (error) error->clear();
    auto it = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&](const ControllerProfile& profile) {
            return profile.id == id;
        });
    if (it == m_profiles.end()) return fail(error, "Controller profile was not found.");
    if (it->device != device) {
        return fail(error, "The current input layout does not match this profile.");
    }
    it->bindings = sanitize_controller_profile_bindings(device, bindings);
    return true;
}

bool ControllerProfileStore::rename(const std::string& id,
                                    std::string name,
                                    std::string* error) {
    if (error) error->clear();
    name = trim_name(std::move(name));
    if (!valid_name(name)) {
        return fail(error, "Enter a profile name between 1 and 64 characters.");
    }
    auto it = std::find_if(
        m_profiles.begin(), m_profiles.end(), [&](const ControllerProfile& profile) {
            return profile.id == id;
        });
    if (it == m_profiles.end()) return fail(error, "Controller profile was not found.");
    if (!nameAvailable(name, id)) return fail(error, "A profile with this name already exists.");
    it->name = std::move(name);
    return true;
}

bool ControllerProfileStore::erase(const std::string& id) {
    const auto oldSize = m_profiles.size();
    std::erase_if(m_profiles, [&](const ControllerProfile& profile) {
        return profile.id == id;
    });
    return m_profiles.size() != oldSize;
}

bool ControllerProfileStore::nameAvailable(
        const std::string& name,
        const std::string& ignoredId) const {
    const std::string folded = folded_name(name);
    return std::none_of(
        m_profiles.begin(), m_profiles.end(), [&](const ControllerProfile& profile) {
            return profile.id != ignoredId && folded_name(profile.name) == folded;
        });
}

} // namespace goliath
