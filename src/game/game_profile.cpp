#include "game/game_profile.hpp"

#include "input/input_maps.hpp"
#include "json.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;
using nlohmann::json;

namespace goliath {

namespace {

constexpr int kProfileSchemaVersion = 1;

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::optional<int> optional_int_in_range(const json& object,
                                         const char* key,
                                         int minimum,
                                         int maximum) {
    if (!object.contains(key) || object.at(key).is_null())
        return std::nullopt;
    if (!object.at(key).is_number_integer())
        return std::nullopt;

    const int value = object.at(key).get<int>();
    if (value < minimum || value > maximum)
        return std::nullopt;
    return value;
}

std::optional<bool> optional_bool(const json& object, const char* key) {
    if (!object.contains(key) || object.at(key).is_null())
        return std::nullopt;
    if (!object.at(key).is_boolean())
        return std::nullopt;
    return object.at(key).get<bool>();
}

void put_optional(json& object, const char* key, const std::optional<int>& value) {
    if (value.has_value()) object[key] = *value;
}

void put_optional(json& object, const char* key, const std::optional<bool>& value) {
    if (value.has_value()) object[key] = *value;
}

GameInputOverrides sanitized_input_overrides(
        const GameInputOverrides& overrides) {
    GameInputOverrides result;
    for (const auto& [section, keys] : overrides) {
        for (const auto& [key, value] : keys) {
            const std::string canonicalKey = canonical_input_key(section, key);
            const auto stored = normalize_input_binding_for_definition(
                section, canonicalKey, value);
            if (stored.has_value())
                result[section][canonicalKey] = *stored;
        }
    }
    return result;
}

GameInputOverrides input_overrides_from_json(const json& object) {
    GameInputOverrides raw;
    if (!object.contains("input_overrides") ||
        !object.at("input_overrides").is_object()) {
        return raw;
    }

    for (const auto& [section, keys] :
         object.at("input_overrides").items()) {
        if (!keys.is_object()) continue;
        for (const auto& [key, value] : keys.items()) {
            if (value.is_string())
                raw[section][key] = value.get<std::string>();
        }
    }
    return sanitized_input_overrides(raw);
}

void put_input_overrides(json& object,
                         const GameInputOverrides& overrides) {
    if (overrides.empty()) return;

    json sections = json::object();
    for (const auto& [section, keys] : overrides) {
        json keyValues = json::object();
        for (const auto& [key, value] : keys)
            keyValues[key] = value;
        if (!keyValues.empty())
            sections[section] = std::move(keyValues);
    }
    if (!sections.empty())
        object["input_overrides"] = std::move(sections);
}

VideoSettingValues video_values_from_json(
        const json& object,
        const char* key,
        const std::vector<VideoSettingSpec>& specs) {
    VideoSettingValues raw;
    if (!object.contains(key) || !object.at(key).is_object())
        return raw;

    for (const auto& [setting, value] : object.at(key).items()) {
        if (value.is_number_integer())
            raw[setting] = value.get<int>();
    }
    return sanitize_video_setting_values(raw, specs);
}

void put_video_values(json& object,
                      const char* key,
                      const VideoSettingValues& values) {
    if (values.empty()) return;
    json settings = json::object();
    for (const auto& [setting, value] : values)
        settings[setting] = value;
    object[key] = std::move(settings);
}

GameLaunchProfile profile_from_json(const json& object) {
    GameLaunchProfile profile;
    profile.cartridge_system = optional_int_in_range(object, "cartridge_system", 0, 2);
    profile.cd_system = optional_int_in_range(object, "cd_system", 0, 3);
    profile.universe_hardware = optional_int_in_range(object, "universe_hardware", 0, 1);
    profile.region = optional_int_in_range(object, "region", 0, 3);
    profile.input_device = optional_int_in_range(object, "input_device", 0, 3);
    profile.free_play = optional_bool(object, "free_play");
    profile.setting_mode = optional_bool(object, "setting_mode");
    profile.video_api = optional_int_in_range(object, "video_api", 0, 3);
    profile.shader = optional_int_in_range(object, "shader", 0, 6);
    profile.jgrf_video = video_values_from_json(
        object, "jgrf_video", jgrf_video_setting_specs());
    // API and shader use their established top-level representation.
    profile.jgrf_video.erase("api");
    profile.jgrf_video.erase("shader");
    profile.geolith_video = video_values_from_json(
        object, "geolith_video", geolith_video_setting_specs());
    profile.controller_port = optional_int_in_range(object, "controller_port", 0, 9);
    profile.input_overrides = input_overrides_from_json(object);
    return profile;
}

json profile_to_json(const GameLaunchProfile& profile) {
    json object = json::object();
    put_optional(object, "cartridge_system", profile.cartridge_system);
    put_optional(object, "cd_system", profile.cd_system);
    put_optional(object, "universe_hardware", profile.universe_hardware);
    put_optional(object, "region", profile.region);
    put_optional(object, "input_device", profile.input_device);
    put_optional(object, "free_play", profile.free_play);
    put_optional(object, "setting_mode", profile.setting_mode);
    put_optional(object, "video_api", profile.video_api);
    put_optional(object, "shader", profile.shader);
    put_video_values(object, "jgrf_video", profile.jgrf_video);
    put_video_values(object, "geolith_video", profile.geolith_video);
    put_optional(object, "controller_port", profile.controller_port);
    put_input_overrides(object, profile.input_overrides);
    return object;
}

} // namespace

bool GameLaunchProfile::empty() const {
    return !cartridge_system.has_value() &&
           !cd_system.has_value() &&
           !universe_hardware.has_value() &&
           !region.has_value() &&
           !input_device.has_value() &&
           !free_play.has_value() &&
           !setting_mode.has_value() &&
           !video_api.has_value() &&
           !shader.has_value() &&
           jgrf_video.empty() &&
           geolith_video.empty() &&
           !controller_port.has_value() &&
           input_overrides.empty();
}

std::string normalize_game_profile_media(std::string media) {
    for (char& ch : media) {
        if (ch == '\\') ch = '/';
    }

    fs::path normalized = fs::path(media).lexically_normal();
    std::string result = normalized.generic_string();
    while (result.starts_with("./")) result.erase(0, 2);
    return result == "." ? std::string() : result;
}

std::string make_game_profile_key(const std::string& system,
                                  const std::string& media) {
    return system + "\n" + normalize_game_profile_media(media);
}

std::string exact_media_storage_id(const std::string& system,
                                   const std::string& media) {
    const std::string key = make_game_profile_key(system, media);
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : key) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }

    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << hash;
    return output.str();
}

bool GameProfileStore::load(const fs::path& path, std::string* error) {
    m_records.clear();
    if (error) error->clear();

    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec) {
        if (error) *error = "Could not inspect profile file: " + ec.message();
        return false;
    }
    if (!exists) return true;

    const bool regular = fs::is_regular_file(path, ec);
    if (ec || !regular) {
        if (error) *error = "Profile path is not a regular file: " + path.string();
        return false;
    }

    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            if (error) *error = "Could not open profile file: " + path.string();
            return false;
        }

        json root;
        input >> root;
        if (!root.is_object() ||
            !root.contains("version") ||
            !root.at("version").is_number_integer() ||
            root.at("version").get<int>() != kProfileSchemaVersion ||
            !root.contains("profiles") ||
            !root.at("profiles").is_array()) {
            if (error) *error = "Unsupported or malformed game profile schema.";
            return false;
        }

        for (const json& item : root.at("profiles")) {
            if (!item.is_object() ||
                !item.contains("system") || !item.at("system").is_string() ||
                !item.contains("media") || !item.at("media").is_string() ||
                !item.contains("overrides") || !item.at("overrides").is_object()) {
                continue;
            }

            const std::string system = item.at("system").get<std::string>();
            const std::string media =
                normalize_game_profile_media(item.at("media").get<std::string>());
            if ((system != "neogeo" && system != "neogeocd") || media.empty())
                continue;

            GameLaunchProfile profile = profile_from_json(item.at("overrides"));
            if (profile.empty()) continue;

            GameProfileRecord record{system, media, std::move(profile)};
            m_records[make_game_profile_key(system, media)] = std::move(record);
        }
    } catch (const std::exception& ex) {
        m_records.clear();
        if (error) *error = std::string("Could not parse game profiles: ") + ex.what();
        return false;
    }

    return true;
}

bool GameProfileStore::save(const fs::path& path, std::string* error) const {
    if (error) error->clear();

    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            if (error) *error = "Could not create profile directory: " + ec.message();
            return false;
        }
    }

    json root;
    root["version"] = kProfileSchemaVersion;
    root["profiles"] = json::array();
    for (const auto& [key, record] : m_records) {
        (void)key;
        if (record.profile.empty()) continue;
        root["profiles"].push_back({
            {"system", record.system},
            {"media", record.media},
            {"overrides", profile_to_json(record.profile)},
        });
    }

    const QByteArray serialized =
        QByteArray::fromStdString(root.dump(2) + '\n');
    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = "Could not open profile file for writing: " +
                     path.string() + ": " + output.errorString().toStdString();
        }
        return false;
    }
    if (output.write(serialized) != serialized.size()) {
        if (error) {
            *error = "Could not write profile file: " + path.string() +
                     ": " + output.errorString().toStdString();
        }
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        if (error) {
            *error = "Could not replace profile file: " + path.string() +
                     ": " + output.errorString().toStdString();
        }
        return false;
    }
    return true;
}

const GameLaunchProfile* GameProfileStore::find(const std::string& system,
                                                const std::string& media) const {
    const auto it = m_records.find(make_game_profile_key(system, media));
    return it == m_records.end() ? nullptr : &it->second.profile;
}

void GameProfileStore::set(const std::string& system,
                           const std::string& media,
                           GameLaunchProfile profile) {
    const std::string normalized = normalize_game_profile_media(media);
    const std::string key = make_game_profile_key(system, normalized);
    profile.input_overrides =
        sanitized_input_overrides(profile.input_overrides);
    profile.jgrf_video = sanitize_video_setting_values(
        profile.jgrf_video, jgrf_video_setting_specs());
    profile.jgrf_video.erase("api");
    profile.jgrf_video.erase("shader");
    profile.geolith_video = sanitize_video_setting_values(
        profile.geolith_video, geolith_video_setting_specs());
    if (profile.empty() || normalized.empty() ||
        (system != "neogeo" && system != "neogeocd")) {
        m_records.erase(key);
        return;
    }
    m_records[key] = GameProfileRecord{system, normalized, std::move(profile)};
}

void GameProfileStore::remove(const std::string& system, const std::string& media) {
    m_records.erase(make_game_profile_key(system, media));
}

void GameProfileStore::clear() {
    m_records.clear();
}

std::size_t GameProfileStore::size() const {
    return m_records.size();
}

} // namespace goliath
