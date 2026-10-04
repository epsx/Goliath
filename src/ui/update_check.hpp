// update_check.hpp - shared manual and silent automatic Goliath release checks.
#pragma once

#include <functional>

class QWidget;

namespace goliath {

class Config;

// Returns true only after a valid published release list was received.
bool checkForUpdates(QWidget* parent);

// Performs no progress/error/up-to-date UI. A dialog is shown only when a
// newer release exists. completion receives true after a valid release list.
using UpdateCheckCompletion = std::function<void(bool)>;
void checkForUpdatesAutomatically(
    QWidget* parent, UpdateCheckCompletion completion = {});

// Records only successful checks, so network failures never postpone the next
// scheduled attempt.
void recordSuccessfulUpdateCheck(Config& config);

} // namespace goliath
