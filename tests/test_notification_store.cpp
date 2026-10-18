#include "catch2/catch.hpp"

#include "common/notification_store.hpp"
#include "test_temp_directory.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace fs = std::filesystem;
using namespace goliath;
using goliath::test::ScopedTestDirectory;

namespace {

AppNotification notification(std::string id, std::int64_t created) {
    return {
        std::move(id),
        NotificationCategory::Library,
        NotificationSeverity::Warning,
        "Library needs attention",
        "A useful detail",
        created,
        false,
        NotificationAction::RescanLibrary,
    };
}

const AppNotification* find_notification(const NotificationStore& store,
                                         const std::string& id) {
    for (const AppNotification& item : store.items()) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

} // namespace

TEST_CASE("notification store treats a missing file as empty",
          "[notifications]") {
    const ScopedTestDirectory temporary(
        "goliath_notification_store_missing");
    const fs::path& root = temporary.path();
    NotificationStore store;
    std::string error;
    CHECK(store.load(root / "notifications.json", &error));
    CHECK(error.empty());
    CHECK(store.size() == 0);
}

TEST_CASE("notification store round trips categories severities and actions",
          "[notifications]") {
    const ScopedTestDirectory temporary(
        "goliath_notification_store_roundtrip");
    const fs::path& root = temporary.path();
    const fs::path path = root / "nested" / "notifications.json";
    NotificationStore original;
    CHECK(original.upsert({
        "update:available", NotificationCategory::Update,
        NotificationSeverity::Information, "Update", "Version 1.2.3", 40,
        false, NotificationAction::OpenUpdate}));
    CHECK(original.upsert({
        "configuration:bios", NotificationCategory::Configuration,
        NotificationSeverity::Error, "BIOS", "Missing", 30,
        true, NotificationAction::OpenSettings}));
    CHECK(original.upsert({
        "integrity:problem", NotificationCategory::Integrity,
        NotificationSeverity::Warning, "Integrity", "Mismatch", 20,
        false, NotificationAction::OpenLogs,
        "Verification issues:\n• Game — disc.chd: CHD SHA-1 mismatch"}));

    std::string error;
    REQUIRE(original.save(path, &error));
    NotificationStore restored;
    REQUIRE(restored.load(path, &error));
    REQUIRE(restored.size() == 3);
    CHECK(restored.unread_count() == 2);
    const AppNotification* bios =
        find_notification(restored, "configuration:bios");
    REQUIRE(bios != nullptr);
    CHECK(bios->category == NotificationCategory::Configuration);
    CHECK(bios->severity == NotificationSeverity::Error);
    CHECK(bios->read);
    CHECK(bios->action == NotificationAction::OpenSettings);
    const AppNotification* integrity =
        find_notification(restored, "integrity:problem");
    REQUIRE(integrity != nullptr);
    CHECK(integrity->hover_details ==
          "Verification issues:\n• Game — disc.chd: CHD SHA-1 mismatch");
}

TEST_CASE("identical notification payload preserves read state and timestamp",
          "[notifications]") {
    NotificationStore store;
    REQUIRE(store.upsert(notification("library:rescan-error", 100)));
    REQUIRE(store.mark_read("library:rescan-error"));

    AppNotification repeated = notification("library:rescan-error", 200);
    CHECK_FALSE(store.upsert(std::move(repeated)));
    const AppNotification* item =
        find_notification(store, "library:rescan-error");
    REQUIRE(item != nullptr);
    CHECK(item->created_at_epoch == 100);
    CHECK(item->read);
}

TEST_CASE("changed notification payload becomes unread again",
          "[notifications]") {
    NotificationStore store;
    REQUIRE(store.upsert(notification("configuration:bios", 100)));
    REQUIRE(store.mark_read("configuration:bios"));

    AppNotification changed = notification("configuration:bios", 200);
    changed.message = "A different problem";
    REQUIRE(store.upsert(std::move(changed)));
    const AppNotification* item =
        find_notification(store, "configuration:bios");
    REQUIRE(item != nullptr);
    CHECK(item->created_at_epoch == 200);
    CHECK_FALSE(item->read);
}

TEST_CASE("changed hover details become unread again",
          "[notifications]") {
    NotificationStore store;
    REQUIRE(store.upsert(notification("library:verification", 100)));
    REQUIRE(store.mark_read("library:verification"));

    AppNotification changed = notification("library:verification", 200);
    changed.hover_details = "Game — disc.chd: CHD SHA-1 mismatch";
    REQUIRE(store.upsert(std::move(changed)));
    const AppNotification* item =
        find_notification(store, "library:verification");
    REQUIRE(item != nullptr);
    CHECK(item->created_at_epoch == 200);
    CHECK_FALSE(item->read);
}

TEST_CASE("notification store accepts history without hover details",
          "[notifications]") {
    const ScopedTestDirectory temporary(
        "goliath_notification_store_legacy_without_hover");
    const fs::path& root = temporary.path();
    const fs::path path = root / "notifications.json";
    {
        std::ofstream output(path, std::ios::binary);
        output << R"({"schema_version":1,"notifications":[{)"
               << R"("id":"library:legacy","category":"library",)"
               << R"("severity":"warning","title":"Legacy",)"
               << R"("message":"Existing M13 item",)"
               << R"("created_at_epoch":100,"read":false,)"
               << R"("action":"none"}]})";
    }

    NotificationStore store;
    std::string error;
    REQUIRE(store.load(path, &error));
    const AppNotification* item =
        find_notification(store, "library:legacy");
    REQUIRE(item != nullptr);
    CHECK(item->hover_details.empty());
}

TEST_CASE("notification store keeps only the newest bounded history",
          "[notifications]") {
    NotificationStore store;
    for (std::int64_t index = 0; index < 60; ++index) {
        REQUIRE(store.upsert(notification(
            "library:event:" + std::to_string(index), index)));
    }
    CHECK(store.size() == NotificationStore::kMaximumEntries);
    CHECK(store.items().front().id == "library:event:59");
    CHECK(store.items().back().id == "library:event:10");
    CHECK(find_notification(store, "library:event:9") == nullptr);
}

TEST_CASE("malformed notification JSON does not replace live history",
          "[notifications]") {
    const ScopedTestDirectory temporary(
        "goliath_notification_store_malformed");
    const fs::path& root = temporary.path();
    const fs::path path = root / "notifications.json";
    {
        std::ofstream output(path, std::ios::binary);
        output << "{ definitely not json";
    }

    NotificationStore store;
    REQUIRE(store.upsert(notification("library:existing", 100)));
    std::string error;
    CHECK_FALSE(store.load(path, &error));
    CHECK_FALSE(error.empty());
    CHECK(find_notification(store, "library:existing") != nullptr);
}

TEST_CASE("notification history supports read dismiss and clear operations",
          "[notifications]") {
    NotificationStore store;
    REQUIRE(store.upsert(notification("library:first", 100)));
    REQUIRE(store.upsert(notification("library:second", 200)));
    CHECK(store.unread_count() == 2);
    CHECK(store.mark_all_read());
    CHECK(store.unread_count() == 0);
    CHECK_FALSE(store.mark_all_read());
    CHECK(store.remove("library:first"));
    CHECK_FALSE(store.remove("library:missing"));
    CHECK(store.clear());
    CHECK_FALSE(store.clear());
}
