// notification_store.hpp - persistent, bounded application notifications.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace goliath {

enum class NotificationCategory {
    Update,
    Library,
    Configuration,
    Integrity,
};

enum class NotificationSeverity {
    Information,
    Warning,
    Error,
};

enum class NotificationAction {
    None,
    OpenUpdate,
    OpenSettings,
    RescanLibrary,
    VerifyBios,
    OpenLogs,
};

struct AppNotification {
    std::string id;
    NotificationCategory category = NotificationCategory::Configuration;
    NotificationSeverity severity = NotificationSeverity::Information;
    std::string title;
    std::string message;
    std::int64_t created_at_epoch = 0;
    bool read = false;
    NotificationAction action = NotificationAction::None;
    // Optional compact detail shown on hover. Kept separate from message so
    // notification cards remain short while still explaining exact failures.
    std::string hover_details;

    AppNotification() = default;
    AppNotification(std::string id_value,
                    NotificationCategory category_value,
                    NotificationSeverity severity_value,
                    std::string title_value, std::string message_value,
                    std::int64_t created_at_epoch_value, bool read_value,
                    NotificationAction action_value,
                    std::string hover_details_value = {})
        : id(std::move(id_value)),
          category(category_value),
          severity(severity_value),
          title(std::move(title_value)),
          message(std::move(message_value)),
          created_at_epoch(created_at_epoch_value),
          read(read_value),
          action(action_value),
          hover_details(std::move(hover_details_value)) {}
};

class NotificationStore {
public:
    static constexpr std::size_t kMaximumEntries = 50;

    bool load(const std::filesystem::path& path, std::string* error = nullptr);
    bool save(const std::filesystem::path& path, std::string* error = nullptr) const;

    // Returns true only when the store changed. An identical payload keeps the
    // existing timestamp and read state, so repeated checks do not create noise.
    bool upsert(AppNotification notification);
    bool mark_read(const std::string& id);
    bool mark_all_read();
    bool remove(const std::string& id);
    bool clear();

    [[nodiscard]] std::size_t size() const { return m_items.size(); }
    [[nodiscard]] std::size_t unread_count() const;
    [[nodiscard]] const std::vector<AppNotification>& items() const {
        return m_items;
    }

private:
    void sort_and_prune();

    std::vector<AppNotification> m_items;
};

std::filesystem::path notification_store_path();

} // namespace goliath
