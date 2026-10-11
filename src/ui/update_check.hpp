// update_check.hpp - shared manual and silent automatic Goliath release checks.
#pragma once

#include "update/release_feed.hpp"

#include <functional>
#include <optional>

class QWidget;

namespace goliath {

class Config;

struct UpdateCheckResult {
    bool successful = false;
    std::optional<PublishedRelease> available_release;
};

// The explicit/manual path keeps its progress, result, and error dialogs.
// successful is true only after a valid published release list was received.
UpdateCheckResult checkForUpdates(QWidget* parent);

// Performs no progress, error, up-to-date, or release-result UI. MainWindow
// decides how an automatically discovered release is presented.
using UpdateCheckCompletion = std::function<void(UpdateCheckResult)>;
void checkForUpdatesAutomatically(
    QWidget* parent, UpdateCheckCompletion completion = {});

// Opens the existing release-details dialog from a cached or freshly fetched
// release. Closing it never clears the persistent update alert.
void showUpdateRelease(QWidget* parent, const PublishedRelease& release);

// Records only successful checks, so network failures never postpone the next
// scheduled attempt. The same transaction also updates or clears the cached
// alert according to the successful result.
void recordSuccessfulUpdateCheck(Config& config,
                                 const UpdateCheckResult& result);

} // namespace goliath
