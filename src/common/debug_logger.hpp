#pragma once

#include <QString>

namespace goliath {

// Simple thread-safe file logger for startup and runtime diagnostics.
// Open/truncate the log once with initialize(), then use logInfo/logWarn/logError.
class DebugLogger {
public:
    // Refuse directories, links/reparse points, and special objects before
    // opening or truncating the frontend log.
    static bool initialize(const QString& logPath,
                           QString* error = nullptr);
    // Truncate the currently open frontend log without invalidating the file
    // handle used by subsequent logger writes.
    static bool clear(QString* error = nullptr);
    static void logInfo(const QString& message);
    static void logWarn(const QString& message);
    static void logError(const QString& message);
};

} // namespace goliath
