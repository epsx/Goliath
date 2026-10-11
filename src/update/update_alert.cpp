#include "update/update_alert.hpp"

#include "common/goliath_common.hpp"
#include "common/qt_path.hpp"
#include "json.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <cstdint>
#include <exception>
#include <fstream>
#include <utility>

namespace fs = std::filesystem;
using nlohmann::json;

namespace goliath {

namespace {

constexpr int kUpdateAlertSchemaVersion = 1;
constexpr std::uintmax_t kMaximumUpdateAlertBytes = 1024 * 1024;

void set_error(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

std::optional<std::string> required_string(const json& object,
                                           const char* key) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) return std::nullopt;
    return it->get<std::string>();
}

} // namespace

fs::path update_alert_cache_path() {
    return get_config_path().parent_path() / "goliath_update_alert.json";
}

bool release_is_newer_than_installed(
        const PublishedRelease& release,
        std::string_view installedVersion) {
    const auto installed = parse_release_version(installedVersion);
    return !installed ||
           compare_release_versions(release.version, *installed) > 0;
}

std::optional<PublishedRelease> load_update_alert_cache(
        const fs::path& path, std::string* error) {
    if (error) error->clear();

    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec) {
        set_error(error, "Could not inspect update alert cache: " +
                         ec.message());
        return std::nullopt;
    }
    if (!exists) return std::nullopt;

    if (!fs::is_regular_file(path, ec) || ec) {
        set_error(error, "Update alert cache is not a regular file: " +
                         path_to_utf8(path));
        return std::nullopt;
    }

    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec || size == 0 || size > kMaximumUpdateAlertBytes) {
        set_error(error, "Update alert cache is empty or unexpectedly large.");
        return std::nullopt;
    }

    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            set_error(error, "Could not open update alert cache: " +
                             path_to_utf8(path));
            return std::nullopt;
        }

        json root;
        input >> root;
        if (!root.is_object() ||
            !root.contains("schema_version") ||
            !root.at("schema_version").is_number_integer() ||
            root.at("schema_version").get<int>() !=
                kUpdateAlertSchemaVersion ||
            !root.contains("release") ||
            !root.at("release").is_object()) {
            set_error(error, "Unsupported or malformed update alert cache.");
            return std::nullopt;
        }

        const json& object = root.at("release");
        const auto tag = required_string(object, "tag");
        const auto name = required_string(object, "name");
        const auto pageUrl = required_string(object, "page_url");
        const auto publishedAt = required_string(object, "published_at");
        const auto body = required_string(object, "body");
        if (!tag || !name || !pageUrl || !publishedAt || !body) {
            set_error(error, "Unsupported or malformed update alert cache.");
            return std::nullopt;
        }

        const auto version = parse_release_version(*tag);
        if (!version || pageUrl->empty()) {
            set_error(error, "Update alert cache contains an invalid release.");
            return std::nullopt;
        }

        return PublishedRelease{
            *version, *tag, *name, *pageUrl, *publishedAt, *body};
    } catch (const std::exception& ex) {
        set_error(error, std::string("Could not parse update alert cache: ") +
                         ex.what());
        return std::nullopt;
    }
}

bool save_update_alert_cache(const fs::path& path,
                             const PublishedRelease& release,
                             std::string* error) {
    if (error) error->clear();
    if (!parse_release_version(release.tag) || release.page_url.empty()) {
        set_error(error, "Cannot cache invalid update release metadata.");
        return false;
    }

    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            set_error(error, "Could not create update cache directory: " +
                             ec.message());
            return false;
        }
    }

    json root;
    root["schema_version"] = kUpdateAlertSchemaVersion;
    root["release"] = {
        {"tag", release.tag},
        {"name", release.name},
        {"page_url", release.page_url},
        {"published_at", release.published_at},
        {"body", release.body},
    };

    const QByteArray serialized =
        QByteArray::fromStdString(root.dump(2) + '\n');
    if (static_cast<std::uintmax_t>(serialized.size()) >
        kMaximumUpdateAlertBytes) {
        set_error(error, "Update alert cache would be unexpectedly large.");
        return false;
    }

    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        set_error(error, "Could not open update alert cache for writing: " +
                         path_to_utf8(path) + ": " +
                         output.errorString().toStdString());
        return false;
    }
    if (output.write(serialized) != serialized.size()) {
        set_error(error, "Could not write update alert cache: " +
                         path_to_utf8(path) + ": " +
                         output.errorString().toStdString());
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        set_error(error, "Could not replace update alert cache: " +
                         path_to_utf8(path) + ": " +
                         output.errorString().toStdString());
        return false;
    }
    return true;
}

bool clear_update_alert_cache(const fs::path& path, std::string* error) {
    if (error) error->clear();
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) return true;
    if (ec) {
        set_error(error, "Could not inspect update alert cache for removal: " +
                         ec.message());
        return false;
    }
    if (!fs::exists(status)) return true;
    if (!fs::is_regular_file(status) && !fs::is_symlink(status)) {
        set_error(error, "Update alert cache path is not a removable file: " +
                         path_to_utf8(path));
        return false;
    }

    fs::remove(path, ec);
    if (ec) {
        set_error(error, "Could not remove update alert cache: " +
                         ec.message());
        return false;
    }
    return true;
}

} // namespace goliath
