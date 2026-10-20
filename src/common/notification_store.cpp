// notification_store.cpp - JSON persistence for the notification center.
#include "common/notification_store.hpp"

#include "common/goliath_common.hpp"
#include "common/qt_path.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace goliath {
namespace {

constexpr int kSchemaVersion = 1;
constexpr std::uintmax_t kMaximumStoreBytes = 1024 * 1024;
constexpr std::size_t kMaximumIdCharacters = 128;
constexpr std::size_t kMaximumTitleCharacters = 240;
constexpr std::size_t kMaximumMessageCharacters = 4096;
constexpr std::size_t kMaximumHoverDetailsCharacters = 4096;

using json = nlohmann::json;

void assign_error(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

bool valid_id(const std::string& id) {
    if (id.empty() || id.size() > kMaximumIdCharacters) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '.' || ch == '_' || ch == ':' ||
               ch == '-';
    });
}

bool valid_notification(const AppNotification& item) {
    return valid_id(item.id) && !item.title.empty() &&
           item.title.size() <= kMaximumTitleCharacters &&
           item.message.size() <= kMaximumMessageCharacters &&
           item.hover_details.size() <= kMaximumHoverDetailsCharacters &&
           item.created_at_epoch >= 0;
}

const char* category_key(NotificationCategory category) {
    switch (category) {
    case NotificationCategory::Update: return "update";
    case NotificationCategory::Library: return "library";
    case NotificationCategory::Configuration: return "configuration";
    case NotificationCategory::Integrity: return "integrity";
    }
    return "configuration";
}

const char* severity_key(NotificationSeverity severity) {
    switch (severity) {
    case NotificationSeverity::Information: return "information";
    case NotificationSeverity::Warning: return "warning";
    case NotificationSeverity::Error: return "error";
    }
    return "information";
}

const char* action_key(NotificationAction action) {
    switch (action) {
    case NotificationAction::None: return "none";
    case NotificationAction::OpenUpdate: return "open_update";
    case NotificationAction::OpenSettings: return "open_settings";
    case NotificationAction::RescanLibrary: return "rescan_library";
    case NotificationAction::OpenVerificationReport:
        return "open_verification_report";
    case NotificationAction::VerifyBios: return "verify_bios";
    case NotificationAction::OpenLogs: return "open_logs";
    }
    return "none";
}

bool parse_category(const std::string& key, NotificationCategory* out) {
    if (key == "update") *out = NotificationCategory::Update;
    else if (key == "library") *out = NotificationCategory::Library;
    else if (key == "configuration") *out = NotificationCategory::Configuration;
    else if (key == "integrity") *out = NotificationCategory::Integrity;
    else return false;
    return true;
}

bool parse_severity(const std::string& key, NotificationSeverity* out) {
    if (key == "information") *out = NotificationSeverity::Information;
    else if (key == "warning") *out = NotificationSeverity::Warning;
    else if (key == "error") *out = NotificationSeverity::Error;
    else return false;
    return true;
}

bool parse_action(const std::string& key, NotificationAction* out) {
    if (key == "none") *out = NotificationAction::None;
    else if (key == "open_update") *out = NotificationAction::OpenUpdate;
    else if (key == "open_settings") *out = NotificationAction::OpenSettings;
    else if (key == "rescan_library") *out = NotificationAction::RescanLibrary;
    else if (key == "open_verification_report")
        *out = NotificationAction::OpenVerificationReport;
    else if (key == "verify_bios") *out = NotificationAction::VerifyBios;
    else if (key == "open_logs") *out = NotificationAction::OpenLogs;
    else return false;
    return true;
}

bool parse_item(const json& value, AppNotification* item) {
    if (!value.is_object()) return false;
    const auto id = value.find("id");
    const auto category = value.find("category");
    const auto severity = value.find("severity");
    const auto title = value.find("title");
    const auto message = value.find("message");
    const auto created = value.find("created_at_epoch");
    const auto readValue = value.find("read");
    const auto action = value.find("action");
    const auto hoverDetails = value.find("hover_details");
    if (id == value.end() || !id->is_string() ||
        category == value.end() || !category->is_string() ||
        severity == value.end() || !severity->is_string() ||
        title == value.end() || !title->is_string() ||
        message == value.end() || !message->is_string() ||
        created == value.end() || !created->is_number_integer() ||
        readValue == value.end() || !readValue->is_boolean() ||
        action == value.end() || !action->is_string()) {
        return false;
    }
    if (hoverDetails != value.end() && !hoverDetails->is_string()) return false;

    item->id = id->get<std::string>();
    item->title = title->get<std::string>();
    item->message = message->get<std::string>();
    item->hover_details = hoverDetails == value.end()
        ? std::string() : hoverDetails->get<std::string>();
    item->created_at_epoch = created->get<std::int64_t>();
    item->read = readValue->get<bool>();
    return parse_category(category->get<std::string>(), &item->category) &&
           parse_severity(severity->get<std::string>(), &item->severity) &&
           parse_action(action->get<std::string>(), &item->action) &&
           valid_notification(*item);
}

bool same_payload(const AppNotification& left, const AppNotification& right) {
    return left.category == right.category && left.severity == right.severity &&
           left.title == right.title && left.message == right.message &&
           left.action == right.action &&
           left.hover_details == right.hover_details;
}

} // namespace

bool NotificationStore::load(const std::filesystem::path& path,
                             std::string* error) {
    if (error) error->clear();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (ec) {
            assign_error(error, "Could not inspect the notification store: " +
                                    ec.message());
            return false;
        }
        m_items.clear();
        return true;
    }
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        assign_error(error, "The notification store is not a regular file.");
        return false;
    }
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaximumStoreBytes) {
        assign_error(error, ec ? "Could not inspect the notification store size."
                               : "The notification store is too large.");
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        assign_error(error, "Could not open the notification store.");
        return false;
    }

    try {
        const json root = json::parse(input);
        const auto schema = root.find("schema_version");
        const auto items = root.find("notifications");
        if (!root.is_object() || schema == root.end() ||
            !schema->is_number_integer() || schema->get<int>() != kSchemaVersion ||
            items == root.end() || !items->is_array()) {
            assign_error(error, "The notification store has an unsupported format.");
            return false;
        }

        std::vector<AppNotification> loaded;
        loaded.reserve(items->size());
        std::unordered_set<std::string> ids;
        for (const auto& value : *items) {
            AppNotification item;
            if (!parse_item(value, &item) || !ids.insert(item.id).second) {
                assign_error(error, "The notification store contains invalid entries.");
                return false;
            }
            loaded.push_back(std::move(item));
        }
        m_items = std::move(loaded);
        sort_and_prune();
        return true;
    } catch (const std::exception&) {
        assign_error(error, "The notification store is not valid JSON.");
        return false;
    }
}

bool NotificationStore::save(const std::filesystem::path& path,
                             std::string* error) const {
    if (error) error->clear();
    std::error_code ec;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            assign_error(error, "Could not create the notification directory: " +
                                    ec.message());
            return false;
        }
    }

    json entries = json::array();
    for (const AppNotification& item : m_items) {
        entries.push_back({
            {"id", item.id},
            {"category", category_key(item.category)},
            {"severity", severity_key(item.severity)},
            {"title", item.title},
            {"message", item.message},
            {"created_at_epoch", item.created_at_epoch},
            {"read", item.read},
            {"action", action_key(item.action)},
            {"hover_details", item.hover_details},
        });
    }
    const json root = {
        {"schema_version", kSchemaVersion},
        {"notifications", std::move(entries)},
    };
    const QByteArray bytes = QByteArray::fromStdString(root.dump(2) + '\n');

    QSaveFile output(path_to_qstring(path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        assign_error(error, "Could not open the notification store for writing.");
        return false;
    }
    if (output.write(bytes) != bytes.size() || !output.commit()) {
        output.cancelWriting();
        assign_error(error, "Could not commit the notification store.");
        return false;
    }
    return true;
}

bool NotificationStore::upsert(AppNotification notification) {
    if (!valid_notification(notification)) return false;
    const auto existing = std::find_if(
        m_items.begin(), m_items.end(), [&](const AppNotification& item) {
            return item.id == notification.id;
        });
    if (existing != m_items.end()) {
        if (same_payload(*existing, notification)) return false;
        *existing = std::move(notification);
    } else {
        m_items.push_back(std::move(notification));
    }
    sort_and_prune();
    return true;
}

bool NotificationStore::mark_read(const std::string& id) {
    const auto item = std::find_if(
        m_items.begin(), m_items.end(), [&](const AppNotification& candidate) {
            return candidate.id == id;
        });
    if (item == m_items.end() || item->read) return false;
    item->read = true;
    return true;
}

bool NotificationStore::mark_all_read() {
    bool changed = false;
    for (AppNotification& item : m_items) {
        if (!item.read) {
            item.read = true;
            changed = true;
        }
    }
    return changed;
}

bool NotificationStore::remove(const std::string& id) {
    const auto oldSize = m_items.size();
    std::erase_if(m_items, [&](const AppNotification& item) {
        return item.id == id;
    });
    return oldSize != m_items.size();
}

bool NotificationStore::clear() {
    if (m_items.empty()) return false;
    m_items.clear();
    return true;
}

std::size_t NotificationStore::unread_count() const {
    return static_cast<std::size_t>(std::count_if(
        m_items.begin(), m_items.end(),
        [](const AppNotification& item) { return !item.read; }));
}

void NotificationStore::sort_and_prune() {
    std::stable_sort(m_items.begin(), m_items.end(),
                     [](const AppNotification& left,
                        const AppNotification& right) {
                         return left.created_at_epoch > right.created_at_epoch;
                     });
    if (m_items.size() > kMaximumEntries)
        m_items.resize(kMaximumEntries);
}

std::filesystem::path notification_store_path() {
    return get_config_path().parent_path() / "goliath_notifications.json";
}

} // namespace goliath
