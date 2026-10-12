#include "game/game_profile.hpp"

#include "common/qt_path.hpp"
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
#include <iterator>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;
using nlohmann::json;

namespace goliath {

namespace {

constexpr int kProfileSchemaVersion = 1;
constexpr int kTransferSchemaVersion = 1;
constexpr std::string_view kTransferFormat = "goliath-game-settings";
constexpr std::uintmax_t kMaximumTransferBytes = 1024U * 1024U;

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

bool fail_transfer(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

bool validate_optional_integer(const json& object,
                               const char* key,
                               int minimum,
                               int maximum,
                               std::string* error) {
    if (!object.contains(key)) return true;
    const json& value = object.at(key);
    if (!value.is_number_integer()) {
        return fail_transfer(
            error, std::string("Override '") + key + "' must be an integer.");
    }
    const int parsed = value.get<int>();
    if (parsed < minimum || parsed > maximum) {
        return fail_transfer(
            error, std::string("Override '") + key + "' is outside its "
                   "supported range.");
    }
    return true;
}

bool validate_optional_boolean(const json& object,
                               const char* key,
                               std::string* error) {
    if (!object.contains(key)) return true;
    if (!object.at(key).is_boolean()) {
        return fail_transfer(
            error, std::string("Override '") + key + "' must be true or false.");
    }
    return true;
}

bool validate_video_transfer_values(
        const json& object,
        const char* key,
        const std::vector<VideoSettingSpec>& specs,
        std::string* error) {
    if (!object.contains(key)) return true;
    const json& values = object.at(key);
    if (!values.is_object()) {
        return fail_transfer(
            error, std::string("Override group '") + key + "' must be an object.");
    }

    for (const auto& [name, value] : values.items()) {
        const VideoSettingSpec* spec = find_video_setting(specs, name);
        if (!spec) {
            return fail_transfer(
                error, "Unknown video override in transfer file: " + name);
        }
        if (!value.is_number_integer() ||
            !is_valid_video_setting_value(*spec, value.get<int>())) {
            return fail_transfer(
                error, "Invalid value for video override: " + name);
        }
    }
    return true;
}

bool validate_input_transfer_values(const json& object,
                                    std::string* error) {
    if (!object.contains("input_overrides")) return true;
    const json& sections = object.at("input_overrides");
    if (!sections.is_object()) {
        return fail_transfer(
            error, "Override group 'input_overrides' must be an object.");
    }

    const auto& definitions = input_defs();
    for (const auto& [section, keys] : sections.items()) {
        if (!definitions.contains(section) || !keys.is_object()) {
            return fail_transfer(
                error, "Unknown or malformed input section: " + section);
        }

        std::set<std::string> canonicalKeys;
        for (const auto& [key, value] : keys.items()) {
            if (!value.is_string()) {
                return fail_transfer(
                    error, "Input override values must be strings.");
            }
            const std::string canonical = canonical_input_key(section, key);
            const auto normalized = normalize_input_binding_for_definition(
                section, canonical, value.get<std::string>());
            if (canonical.empty() || !normalized.has_value()) {
                return fail_transfer(
                    error, "Invalid input override: " + section + "/" + key);
            }
            if (!canonicalKeys.insert(canonical).second) {
                return fail_transfer(
                    error, "Duplicate input override: " + section + "/" +
                               canonical);
            }
        }
    }
    return true;
}

bool validate_transfer_overrides(const json& object,
                                 const std::string& system,
                                 std::string* error) {
    static const std::set<std::string> allowed = {
        "cartridge_system", "cd_system", "universe_hardware", "region",
        "input_device", "free_play", "setting_mode", "video_api",
        "shader", "jgrf_video", "geolith_video", "controller_port",
        "input_overrides",
    };
    for (const auto& [key, value] : object.items()) {
        (void)value;
        if (!allowed.contains(key)) {
            return fail_transfer(
                error, "Unknown per-game override in transfer file: " + key);
        }
    }

    if (system == "neogeo" && object.contains("cd_system")) {
        return fail_transfer(
            error, "A Neo Geo cartridge transfer cannot contain CD settings.");
    }
    if (system == "neogeocd" &&
        (object.contains("cartridge_system") ||
         object.contains("universe_hardware") ||
         object.contains("free_play") ||
         object.contains("setting_mode"))) {
        return fail_transfer(
            error, "A Neo Geo CD transfer contains cartridge-only settings.");
    }

    const int maximumInputDevice = system == "neogeocd" ? 1 : 3;
    return validate_optional_integer(
               object, "cartridge_system", 0, 2, error) &&
           validate_optional_integer(object, "cd_system", 0, 3, error) &&
           validate_optional_integer(
               object, "universe_hardware", 0, 1, error) &&
           validate_optional_integer(object, "region", 0, 3, error) &&
           validate_optional_integer(
               object, "input_device", 0, maximumInputDevice, error) &&
           validate_optional_boolean(object, "free_play", error) &&
           validate_optional_boolean(object, "setting_mode", error) &&
           validate_optional_integer(object, "video_api", 0, 3, error) &&
           validate_optional_integer(object, "shader", 0, 6, error) &&
           validate_optional_integer(object, "controller_port", 0, 9, error) &&
           validate_video_transfer_values(
               object, "jgrf_video", jgrf_video_setting_specs(), error) &&
           validate_video_transfer_values(
               object, "geolith_video", geolith_video_setting_specs(), error) &&
           validate_input_transfer_values(object, error);
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

std::size_t GameProfileTransferSummary::total() const {
    return system_overrides + video_overrides + input_overrides;
}

GameProfileTransferSummary summarize_game_profile_transfer(
        const GameLaunchProfile& profile) {
    GameProfileTransferSummary summary;
    summary.system_overrides += profile.cartridge_system.has_value() ? 1U : 0U;
    summary.system_overrides += profile.cd_system.has_value() ? 1U : 0U;
    summary.system_overrides += profile.universe_hardware.has_value() ? 1U : 0U;
    summary.system_overrides += profile.region.has_value() ? 1U : 0U;
    summary.system_overrides += profile.free_play.has_value() ? 1U : 0U;
    summary.system_overrides += profile.setting_mode.has_value() ? 1U : 0U;

    summary.video_overrides += profile.video_api.has_value() ? 1U : 0U;
    summary.video_overrides += profile.shader.has_value() ? 1U : 0U;
    summary.video_overrides += profile.jgrf_video.size();
    summary.video_overrides += profile.geolith_video.size();

    summary.input_overrides += profile.input_device.has_value() ? 1U : 0U;
    summary.input_overrides += profile.controller_port.has_value() ? 1U : 0U;
    for (const auto& [section, keys] : profile.input_overrides) {
        (void)section;
        summary.input_overrides += keys.size();
    }
    return summary;
}

std::string serialize_game_profile_transfer(
        const std::string& system,
        const GameLaunchProfile& profile) {
    if (system != "neogeo" && system != "neogeocd") return {};

    const json overrides = profile_to_json(profile);
    if (!validate_transfer_overrides(overrides, system, nullptr)) return {};

    json root = {
        {"format", std::string(kTransferFormat)},
        {"version", kTransferSchemaVersion},
        {"system", system},
        {"overrides", overrides},
    };
    return root.dump(2) + '\n';
}

bool deserialize_game_profile_transfer(
        std::string_view document,
        const std::string& expected_system,
        GameLaunchProfile* profile,
        std::string* error) {
    if (error) error->clear();
    if (!profile) {
        return fail_transfer(error, "No destination profile was provided.");
    }
    if (expected_system != "neogeo" && expected_system != "neogeocd") {
        return fail_transfer(error, "The selected game has an unsupported system.");
    }
    if (document.size() > kMaximumTransferBytes) {
        return fail_transfer(error, "The game-settings file is unexpectedly large.");
    }

    try {
        const json root = json::parse(document.begin(), document.end());
        if (!root.is_object() || root.size() != 4U ||
            !root.contains("format") || !root.at("format").is_string() ||
            root.at("format").get<std::string>() != kTransferFormat ||
            !root.contains("version") ||
            !root.at("version").is_number_integer() ||
            root.at("version").get<int>() != kTransferSchemaVersion ||
            !root.contains("system") || !root.at("system").is_string() ||
            !root.contains("overrides") ||
            !root.at("overrides").is_object()) {
            return fail_transfer(
                error, "Unsupported or malformed Goliath game-settings file.");
        }

        const std::string sourceSystem = root.at("system").get<std::string>();
        if (sourceSystem != "neogeo" && sourceSystem != "neogeocd") {
            return fail_transfer(error, "The transfer file has an unknown system.");
        }
        if (sourceSystem != expected_system) {
            return fail_transfer(
                error,
                expected_system == "neogeo"
                    ? "Neo Geo CD settings cannot be imported into a cartridge game."
                    : "Neo Geo cartridge settings cannot be imported into a CD game.");
        }

        const json& overrides = root.at("overrides");
        if (!validate_transfer_overrides(overrides, sourceSystem, error))
            return false;

        GameLaunchProfile parsed = profile_from_json(overrides);
        *profile = std::move(parsed);
        return true;
    } catch (const std::exception& ex) {
        return fail_transfer(
            error, std::string("Could not parse game-settings file: ") + ex.what());
    }
}

bool save_game_profile_transfer(const fs::path& path,
                                const std::string& system,
                                const GameLaunchProfile& profile,
                                std::string* error) {
    if (error) error->clear();
    const std::string serialized =
        serialize_game_profile_transfer(system, profile);
    GameLaunchProfile validated;
    if (serialized.empty() ||
        !deserialize_game_profile_transfer(
            serialized, system, &validated, error)) {
        if (error && error->empty())
            *error = "The game profile cannot be exported.";
        return false;
    }

    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            return fail_transfer(
                error, "Could not create export directory: " + ec.message());
        }
    }

    const QByteArray bytes = QByteArray::fromStdString(serialized);
    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        return fail_transfer(
            error, "Could not open game-settings file for writing: " +
                       output.errorString().toStdString());
    }
    if (output.write(bytes) != bytes.size()) {
        const std::string message =
            "Could not write game-settings file: " +
            output.errorString().toStdString();
        output.cancelWriting();
        return fail_transfer(error, message);
    }
    if (!output.commit()) {
        return fail_transfer(
            error, "Could not replace game-settings file: " +
                       output.errorString().toStdString());
    }
    return true;
}

bool load_game_profile_transfer(const fs::path& path,
                                const std::string& expected_system,
                                GameLaunchProfile* profile,
                                std::string* error) {
    if (error) error->clear();

    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) {
        return fail_transfer(error, "The selected game-settings path is not a file.");
    }
    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec) {
        return fail_transfer(error, "Could not inspect the game-settings file.");
    }
    if (size > kMaximumTransferBytes) {
        return fail_transfer(error, "The game-settings file is unexpectedly large.");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return fail_transfer(error, "Could not open the game-settings file.");
    }
    const std::string document{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (!input.good() && !input.eof()) {
        return fail_transfer(error, "Could not read the game-settings file.");
    }
    return deserialize_game_profile_transfer(
        document, expected_system, profile, error);
}

std::string normalize_game_profile_media(std::string media) {
    for (char& ch : media) {
        if (ch == '\\') ch = '/';
    }

    fs::path normalized = path_from_utf8(media).lexically_normal();
    std::string result = path_to_utf8(normalized);
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
    for (const char rawCh : key) {
        const auto ch = static_cast<unsigned char>(rawCh);
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
        if (error) {
            *error = "Profile path is not a regular file: " +
                     path_to_utf8(path);
        }
        return false;
    }

    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            if (error) {
                *error = "Could not open profile file: " + path_to_utf8(path);
            }
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
                     path_to_utf8(path) + ": " +
                     output.errorString().toStdString();
        }
        return false;
    }
    if (output.write(serialized) != serialized.size()) {
        if (error) {
            *error = "Could not write profile file: " + path_to_utf8(path) +
                     ": " + output.errorString().toStdString();
        }
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        if (error) {
            *error = "Could not replace profile file: " + path_to_utf8(path) +
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
