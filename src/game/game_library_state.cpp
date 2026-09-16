#include "game/game_library_state.hpp"

#include "game/game_profile.hpp"
#include "json.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <exception>
#include <fstream>
#include <utility>

namespace fs = std::filesystem;
using nlohmann::json;

namespace goliath {

namespace {

constexpr int kLibraryStateSchemaVersion = 2;
constexpr int kOldestSupportedLibraryStateSchemaVersion = 1;

bool is_supported_system(const std::string& system) {
    return system == "neogeo" || system == "neogeocd";
}

QString filesystem_path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

} // namespace

bool GameLibraryState::empty() const noexcept {
    return !favorite && rating == 0;
}

bool GameLibraryStateStore::load(const fs::path& path, std::string* error) {
    m_records.clear();
    if (error) error->clear();

    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec) {
        if (error) *error = "Could not inspect library state file: " + ec.message();
        return false;
    }
    if (!exists) return true;

    const bool regular = fs::is_regular_file(path, ec);
    if (ec || !regular) {
        if (error) {
            *error = "Library state path is not a regular file: " + path.string();
        }
        return false;
    }

    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            if (error) {
                *error = "Could not open library state file: " + path.string();
            }
            return false;
        }

        json root;
        input >> root;
        if (!root.is_object() ||
            !root.contains("version") ||
            !root.at("version").is_number_integer() ||
            !root.contains("records") ||
            !root.at("records").is_array()) {
            if (error) *error = "Unsupported or malformed game library state schema.";
            return false;
        }

        const int version = root.at("version").get<int>();
        if (version < kOldestSupportedLibraryStateSchemaVersion ||
            version > kLibraryStateSchemaVersion) {
            if (error) *error = "Unsupported or malformed game library state schema.";
            return false;
        }

        for (const json& item : root.at("records")) {
            if (!item.is_object() ||
                !item.contains("system") || !item.at("system").is_string() ||
                !item.contains("media") || !item.at("media").is_string() ||
                !item.contains("favorite") || !item.at("favorite").is_boolean()) {
                continue;
            }

            int rating = 0;
            if (version >= 2) {
                if (!item.contains("rating") ||
                    !item.at("rating").is_number_integer()) {
                    continue;
                }
                rating = item.at("rating").get<int>();
                if (rating < 0 || rating > 5) continue;
            }

            const std::string system = item.at("system").get<std::string>();
            const std::string media = normalize_game_profile_media(
                item.at("media").get<std::string>());
            if (!is_supported_system(system) || media.empty()) continue;

            const std::string key = make_game_profile_key(system, media);
            const bool favorite = item.at("favorite").get<bool>();
            const GameLibraryState state{favorite, rating};
            if (state.empty()) {
                m_records.erase(key);
                continue;
            }

            m_records[key] = GameLibraryStateRecord{
                system, media, state};
        }
    } catch (const std::exception& ex) {
        m_records.clear();
        if (error) {
            *error = std::string("Could not parse game library state: ") + ex.what();
        }
        return false;
    }

    return true;
}

bool GameLibraryStateStore::save(const fs::path& path,
                                 std::string* error) const {
    if (error) error->clear();

    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            if (error) {
                *error = "Could not create library state directory: " + ec.message();
            }
            return false;
        }
    }

    json root;
    root["version"] = kLibraryStateSchemaVersion;
    root["records"] = json::array();
    for (const auto& [key, record] : m_records) {
        (void)key;
        if (record.state.empty()) continue;
        root["records"].push_back({
            {"system", record.system},
            {"media", record.media},
            {"favorite", record.state.favorite},
            {"rating", record.state.rating},
        });
    }

    const QByteArray serialized = QByteArray::fromStdString(root.dump(2) + '\n');
    QSaveFile output(filesystem_path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = "Could not open library state file for writing: " +
                     path.string() + ": " + output.errorString().toStdString();
        }
        return false;
    }
    if (output.write(serialized) != serialized.size()) {
        if (error) {
            *error = "Could not write library state file: " + path.string() +
                     ": " + output.errorString().toStdString();
        }
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        if (error) {
            *error = "Could not replace library state file: " + path.string() +
                     ": " + output.errorString().toStdString();
        }
        return false;
    }
    return true;
}

const GameLibraryState* GameLibraryStateStore::find(
        const std::string& system,
        const std::string& media) const {
    const auto it = m_records.find(make_game_profile_key(system, media));
    return it == m_records.end() ? nullptr : &it->second.state;
}

bool GameLibraryStateStore::is_favorite(const std::string& system,
                                        const std::string& media) const {
    const GameLibraryState* state = find(system, media);
    return state && state->favorite;
}

void GameLibraryStateStore::set_favorite(const std::string& system,
                                         const std::string& media,
                                         bool favorite) {
    const std::string normalized = normalize_game_profile_media(media);
    const std::string key = make_game_profile_key(system, normalized);
    if (!is_supported_system(system) || normalized.empty()) {
        return;
    }

    auto it = m_records.find(key);
    if (it == m_records.end()) {
        if (!favorite) return;
        m_records[key] = GameLibraryStateRecord{
            system, normalized, GameLibraryState{true, 0}};
        return;
    }

    it->second.state.favorite = favorite;
    if (it->second.state.empty()) m_records.erase(it);
}

int GameLibraryStateStore::rating(const std::string& system,
                                  const std::string& media) const {
    const GameLibraryState* state = find(system, media);
    return state ? state->rating : 0;
}

void GameLibraryStateStore::set_rating(const std::string& system,
                                       const std::string& media,
                                       int rating) {
    if (rating < 0 || rating > 5) return;

    const std::string normalized = normalize_game_profile_media(media);
    const std::string key = make_game_profile_key(system, normalized);
    if (!is_supported_system(system) || normalized.empty()) return;

    auto it = m_records.find(key);
    if (it == m_records.end()) {
        if (rating == 0) return;
        m_records[key] = GameLibraryStateRecord{
            system, normalized, GameLibraryState{false, rating}};
        return;
    }

    it->second.state.rating = rating;
    if (it->second.state.empty()) m_records.erase(it);
}

std::size_t GameLibraryStateStore::favorite_count(
        const std::string& system) const {
    std::size_t count = 0;
    for (const auto& [key, record] : m_records) {
        (void)key;
        if (record.state.favorite &&
            (system.empty() || record.system == system)) {
            ++count;
        }
    }
    return count;
}

std::size_t GameLibraryStateStore::rating_count(
        const std::string& system) const {
    std::size_t count = 0;
    for (const auto& [key, record] : m_records) {
        (void)key;
        if (record.state.rating > 0 &&
            (system.empty() || record.system == system)) {
            ++count;
        }
    }
    return count;
}

void GameLibraryStateStore::clear() {
    m_records.clear();
}

std::size_t GameLibraryStateStore::size() const {
    return m_records.size();
}

} // namespace goliath
