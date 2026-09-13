#include "debug_logger.hpp"

#include "common/log_file.hpp"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>

#include <filesystem>

namespace goliath {

namespace {

QMutex g_mutex;
QFile g_file;
bool g_open = false;

enum class Level { Info, Warn, Error };

const char* levelName(Level level) {
    switch (level) {
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "INFO";
}

void writeLine(Level level, const QString& message) {
    QMutexLocker lock(&g_mutex);
    if (!g_open || !g_file.isOpen()) return;

    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString line = QString("[%1] [%2] %3\n").arg(timestamp, levelName(level), message);

    g_file.write(line.toUtf8());
    g_file.flush();
}

} // namespace

bool DebugLogger::initialize(const QString& logPath, QString* error) {
    QMutexLocker lock(&g_mutex);
    if (error) error->clear();
    if (g_file.isOpen()) g_file.close();
    g_open = false;

#if defined(_WIN32)
    const std::filesystem::path path(logPath.toStdWString());
#else
    const std::filesystem::path path(logPath.toStdString());
#endif
    const LogFileOperationResult opened =
        open_direct_regular_log_file(g_file, path, true);
    if (!opened.success) {
        if (error) *error = QString::fromStdString(opened.error);
        return false;
    }
    if (!g_file.resize(0) || !g_file.seek(0)) {
        if (error) *error = g_file.errorString();
        g_file.close();
        return false;
    }

    g_open = true;
    return true;
}

bool DebugLogger::clear(QString* error) {
    QMutexLocker lock(&g_mutex);
    if (error) error->clear();
    if (!g_open || !g_file.isOpen()) {
        if (error) *error = "The frontend log is not open.";
        return false;
    }
    if (!g_file.flush()) {
        if (error) *error = g_file.errorString();
        return false;
    }
    if (!g_file.resize(0) || !g_file.seek(0)) {
        if (error) *error = g_file.errorString();
        return false;
    }
    return true;
}

void DebugLogger::logInfo(const QString& message) { writeLine(Level::Info, message); }
void DebugLogger::logWarn(const QString& message) { writeLine(Level::Warn, message); }
void DebugLogger::logError(const QString& message) { writeLine(Level::Error, message); }

} // namespace goliath
