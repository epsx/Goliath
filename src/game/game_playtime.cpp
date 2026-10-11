#include "game/game_playtime.hpp"

#include "common/qt_path.hpp"
#include "game/game_profile.hpp"
#include "json.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <exception>
#include <fstream>
#include <limits>
#include <utility>

namespace fs = std::filesystem;
using nlohmann::json;

namespace goliath {

namespace {

constexpr int kPlaytimeSchemaVersion = 1;

bool is_supported_system(const std::string& system) {
    return system == "neogeo" || system == "neogeocd";
}

bool read_nonnegative_int64(const json& object,
                            const char* key,
                            std::int64_t* value) {
    if (!value || !object.contains(key)) return false;

    const json& item = object.at(key);
    try {
        if (item.is_number_unsigned()) {
            const std::uint64_t unsignedValue = item.get<std::uint64_t>();
            if (unsignedValue >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max())) {
                return false;
            }
            *value = static_cast<std::int64_t>(unsignedValue);
            return true;
        }
        if (!item.is_number_integer()) return false;

        const std::int64_t signedValue = item.get<std::int64_t>();
        if (signedValue < 0) return false;
        *value = signedValue;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void read_last_session_metadata(const json& object,
                                std::int64_t legacyEndedEpoch,
                                GamePlaytimeRecord* record) {
    if (!record) return;

    // Version 1 originally persisted only last_played_epoch, whose actual
    // meaning was the end of the last session. Keep that field authoritative
    // for backward compatibility and treat the clearer metadata as optional.
    record->last_ended_epoch = legacyEndedEpoch;

    std::int64_t startedEpoch = 0;
    std::int64_t endedEpoch = 0;
    std::int64_t sessionSeconds = 0;
    if (!read_nonnegative_int64(
            object, "last_started_epoch", &startedEpoch) ||
        !read_nonnegative_int64(
            object, "last_ended_epoch", &endedEpoch) ||
        !read_nonnegative_int64(
            object, "last_session_seconds", &sessionSeconds) ||
        startedEpoch == 0 || endedEpoch != legacyEndedEpoch ||
        sessionSeconds < kMinimumTrackedSessionSeconds ||
        startedEpoch > endedEpoch ||
        endedEpoch - startedEpoch != sessionSeconds) {
        return;
    }

    record->last_started_epoch = startedEpoch;
    record->last_session_seconds = sessionSeconds;
}

std::int64_t saturating_add(std::int64_t current, std::int64_t increment) {
    const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    if (increment > maximum - current) return maximum;
    return current + increment;
}

} // namespace

std::string format_playtime_seconds(std::int64_t seconds) {
    if (seconds < 60) return "Less than a minute";

    const std::int64_t totalMinutes = seconds / 60;
    const std::int64_t hours = totalMinutes / 60;
    const std::int64_t minutes = totalMinutes % 60;

    if (hours == 0) return std::to_string(minutes) + "m";
    if (minutes == 0) return std::to_string(hours) + "h";
    return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
}

bool GamePlaytimeStore::load(const fs::path& path, std::string* error) {
    m_records.clear();
    if (error) error->clear();

    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec) {
        if (error) *error = "Could not inspect playtime file: " + ec.message();
        return false;
    }
    if (!exists) return true;

    const bool regular = fs::is_regular_file(path, ec);
    if (ec || !regular) {
        if (error) {
            *error = "Playtime path is not a regular file: " +
                     path_to_utf8(path);
        }
        return false;
    }

    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            if (error) {
                *error = "Could not open playtime file: " + path_to_utf8(path);
            }
            return false;
        }

        json root;
        input >> root;
        if (!root.is_object() ||
            !root.contains("version") ||
            !root.at("version").is_number_integer() ||
            root.at("version").get<int>() != kPlaytimeSchemaVersion ||
            !root.contains("records") ||
            !root.at("records").is_array()) {
            if (error) {
                *error = "Unsupported or malformed game playtime schema.";
            }
            return false;
        }

        for (const json& item : root.at("records")) {
            if (!item.is_object() ||
                !item.contains("system") || !item.at("system").is_string() ||
                !item.contains("media") || !item.at("media").is_string()) {
                continue;
            }

            const std::string system = item.at("system").get<std::string>();
            const std::string media = normalize_game_profile_media(
                item.at("media").get<std::string>());
            if (!is_supported_system(system) || media.empty()) continue;

            GamePlaytimeRecord record;
            std::int64_t legacyEndedEpoch = 0;
            if (!read_nonnegative_int64(
                    item, "total_seconds", &record.total_seconds) ||
                !read_nonnegative_int64(
                    item, "session_count", &record.session_count) ||
                !read_nonnegative_int64(
                    item, "last_played_epoch", &legacyEndedEpoch) ||
                record.total_seconds < kMinimumTrackedSessionSeconds ||
                record.session_count == 0 ||
                legacyEndedEpoch == 0) {
                continue;
            }
            read_last_session_metadata(item, legacyEndedEpoch, &record);

            m_records[make_game_profile_key(system, media)] =
                Entry{system, media, record};
        }
    } catch (const std::exception& ex) {
        m_records.clear();
        if (error) {
            *error = std::string("Could not parse game playtime: ") + ex.what();
        }
        return false;
    }

    return true;
}

bool GamePlaytimeStore::save(const fs::path& path, std::string* error) const {
    if (error) error->clear();

    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            if (error) {
                *error = "Could not create playtime directory: " + ec.message();
            }
            return false;
        }
    }

    json root;
    root["version"] = kPlaytimeSchemaVersion;
    root["records"] = json::array();
    for (const auto& [key, entry] : m_records) {
        (void)key;
        json item = {
            {"system", entry.system},
            {"media", entry.media},
            {"total_seconds", entry.record.total_seconds},
            {"session_count", entry.record.session_count},
            // Keep the original key so older Goliath builds can still read
            // files written by this version.
            {"last_played_epoch", entry.record.last_ended_epoch},
        };
        if (entry.record.last_started_epoch > 0 &&
            entry.record.last_ended_epoch > 0 &&
            entry.record.last_session_seconds >=
                kMinimumTrackedSessionSeconds) {
            item["last_started_epoch"] =
                entry.record.last_started_epoch;
            item["last_ended_epoch"] =
                entry.record.last_ended_epoch;
            item["last_session_seconds"] =
                entry.record.last_session_seconds;
        }
        root["records"].push_back(std::move(item));
    }

    const QByteArray serialized =
        QByteArray::fromStdString(root.dump(2) + "\n");
    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = "Could not open playtime file for writing: " +
                     path_to_utf8(path) + ": " +
                     output.errorString().toStdString();
        }
        return false;
    }

    if (output.write(serialized) != serialized.size()) {
        if (error) {
            *error = "Could not write playtime file: " + path_to_utf8(path) +
                     ": " + output.errorString().toStdString();
        }
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        if (error) {
            *error = "Could not replace playtime file: " + path_to_utf8(path) +
                     ": " + output.errorString().toStdString();
        }
        return false;
    }
    return true;
}

const GamePlaytimeRecord* GamePlaytimeStore::find(
        const std::string& system,
        const std::string& media) const {
    const auto it = m_records.find(make_game_profile_key(system, media));
    return it == m_records.end() ? nullptr : &it->second.record;
}

bool GamePlaytimeStore::add_session(const std::string& system,
                                    const std::string& media,
                                    std::int64_t session_seconds,
                                    std::int64_t ended_epoch) {
    if (session_seconds < kMinimumTrackedSessionSeconds ||
        ended_epoch <= session_seconds ||
        !is_supported_system(system)) {
        return false;
    }

    const std::string normalized = normalize_game_profile_media(media);
    if (normalized.empty()) return false;

    const std::string key = make_game_profile_key(system, normalized);
    auto [it, inserted] = m_records.try_emplace(
        key, Entry{system, normalized, GamePlaytimeRecord{}});
    (void)inserted;

    GamePlaytimeRecord& record = it->second.record;
    record.total_seconds =
        saturating_add(record.total_seconds, session_seconds);
    record.session_count = saturating_add(record.session_count, 1);
    if (ended_epoch >= record.last_ended_epoch) {
        record.last_started_epoch = ended_epoch - session_seconds;
        record.last_ended_epoch = ended_epoch;
        record.last_session_seconds = session_seconds;
    }
    return true;
}

void GamePlaytimeStore::clear() {
    m_records.clear();
}

std::size_t GamePlaytimeStore::size() const {
    return m_records.size();
}

} // namespace goliath
