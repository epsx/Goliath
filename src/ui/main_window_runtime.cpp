#include "ui/main_window.hpp"

#include "audio/jollygood_audio.hpp"
#include "common/debug_logger.hpp"
#include "common/diagnostic_report.hpp"
#include "common/log_file.hpp"
#include "common/qt_path.hpp"
#include "game/jollygood_bios.hpp"
#include "game/jollygood_launch.hpp"
#include "game/jollygood_process.hpp"
#include "game/jollygood_startup_diagnostics.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace goliath {
namespace {

constexpr qint64 kLaunchLogExcerptBytes = 32 * 1024;
constexpr std::size_t kLaunchLogExcerptLines = 10;

struct TrackedLaunchFailurePresentation {
    QString title = QStringLiteral("Game Stopped During Startup");
    QString message;
    NotificationAction action = NotificationAction::OpenLogs;
};

NotificationAction launch_notification_action(
        LaunchPreparationStatus status) {
    switch (status) {
    case LaunchPreparationStatus::MediaMissing:
        return NotificationAction::RescanLibrary;
    case LaunchPreparationStatus::ExecutableNotFound:
    case LaunchPreparationStatus::CapabilityProbeFailed:
    case LaunchPreparationStatus::ChdUnsupported:
    case LaunchPreparationStatus::ProfileConfigurationFailed:
        return NotificationAction::OpenSettings;
    default:
        return NotificationAction::None;
    }
}

std::uintmax_t launch_log_offset(const fs::path& path) {
    QFile file;
    const LogFileOperationResult opened =
        open_direct_regular_log_file(file, path, false);
    if (!opened.success || !opened.existed || !file.isOpen()) return 0;
    const qint64 size = file.size();
    return size > 0 ? static_cast<std::uintmax_t>(size) : 0;
}

QString launch_log_excerpt(const fs::path& path, std::uintmax_t offset) {
    QFile file;
    const LogFileOperationResult opened =
        open_direct_regular_log_file(file, path, false);
    if (!opened.success || !opened.existed || !file.isOpen()) return {};

    const qint64 size = file.size();
    if (size <= 0) return {};
    qint64 start = offset <= static_cast<std::uintmax_t>(size)
        ? static_cast<qint64>(offset) : 0;
    if (size - start > kLaunchLogExcerptBytes)
        start = size - kLaunchLogExcerptBytes;
    if (!file.seek(start)) return {};

    QByteArray bytes = file.read(kLaunchLogExcerptBytes);
    if (start > 0 && start != static_cast<qint64>(offset)) {
        const qsizetype newline = bytes.indexOf('\n');
        bytes = newline < 0 ? QByteArray{} : bytes.mid(newline + 1);
    }

    const std::string text(bytes.constData(),
                           static_cast<std::size_t>(bytes.size()));
    const std::vector<std::string> lines =
        diagnostic_tail_lines(text, kLaunchLogExcerptLines);
    QStringList visible;
    for (const std::string& line : lines) {
        if (line.empty()) continue;
        visible.push_back(QString::fromUtf8(
            line.data(), static_cast<qsizetype>(line.size())));
    }
    return visible.join('\n');
}

QString launch_exit_code_text(std::optional<std::uint32_t> exitCode) {
    if (!exitCode.has_value()) return QStringLiteral("unavailable");
    return QStringLiteral("0x%1").arg(
        QString::number(*exitCode, 16).rightJustified(8, '0'));
}

TrackedLaunchFailurePresentation tracked_launch_failure_presentation(
        const DetachedProcessTracker& tracker,
        std::int64_t seconds,
        std::optional<std::uint32_t> exitCode,
        const fs::path& logPath) {
    TrackedLaunchFailurePresentation presentation;
    QString reason;
    if (exitCode.has_value()) {
        const std::string_view loaderReason =
            windows_loader_failure_reason(*exitCode);
        if (!loaderReason.empty()) {
            reason = QString::fromUtf8(
                loaderReason.data(),
                static_cast<qsizetype>(loaderReason.size()));
        } else if (*exitCode != 0) {
            reason = QStringLiteral("JGRF exited with code %1.")
                .arg(launch_exit_code_text(exitCode));
        }
    }
    if (reason.isEmpty()) {
        reason = QStringLiteral(
            "JGRF stopped after %1 second(s), before a playable session "
            "was established.")
            .arg(static_cast<qlonglong>(seconds));
    }

    const QString display = tracker.display_name().empty()
        ? QString::fromStdString(tracker.media())
        : QString::fromStdString(tracker.display_name());
    QString message = QStringLiteral(
        "Game: %1\nMedia: %2\nReason: %3\nExit code: %4")
        .arg(display)
        .arg(QString::fromStdString(tracker.media()))
        .arg(reason)
        .arg(launch_exit_code_text(exitCode));
    if (!tracker.effective_system().empty()) {
        message += QStringLiteral("\nEffective system: %1")
            .arg(QString::fromStdString(tracker.effective_system()));
    }

    const QString excerpt =
        launch_log_excerpt(logPath, tracker.log_offset());
    std::optional<JollygoodStartupDiagnosis> diagnosis;
    if (!excerpt.isEmpty()) {
        const QByteArray excerptBytes = excerpt.toUtf8();
        diagnosis = diagnose_jollygood_startup_failure(std::string_view(
            excerptBytes.constData(),
            static_cast<std::size_t>(excerptBytes.size())));
        message += QStringLiteral("\n\nRecent JGRF output:\n%1").arg(excerpt);
    }

    if (diagnosis.has_value()) {
        presentation.title = QString::fromStdString(diagnosis->title);
        presentation.action = diagnosis->offer_bios_verification
            ? NotificationAction::VerifyBios
            : NotificationAction::OpenLogs;
        message = QStringLiteral("%1\n\nWhat to do: %2\n\n%3")
            .arg(QString::fromStdString(diagnosis->explanation))
            .arg(QString::fromStdString(diagnosis->guidance))
            .arg(message);
    } else if (tracker.mvs_hardware()) {
        message += QStringLiteral(
            "\n\nCompatibility hint: this launch used MVS hardware. "
            "Some cartridges or homebrew require AES. In Game settings > "
            "General, try System Type: AES for this exact media. This is a "
            "suggestion, not an automatic diagnosis.");
    }
    presentation.message = std::move(message);
    return presentation;
}

QString unobserved_launch_text(
        qint64 pid,
        const DetachedProcessTracker& tracker,
        const fs::path& logPath) {
    QString message = QStringLiteral(
        "JGRF reported a successful detached start (PID %1), but Goliath "
        "could not attach its process observer. The process may have stopped "
        "before observation began, or the operating system may have denied "
        "access.\n\nMedia: %2")
        .arg(pid)
        .arg(QString::fromStdString(tracker.media()));
    if (!tracker.effective_system().empty()) {
        message += QStringLiteral("\nEffective system: %1")
            .arg(QString::fromStdString(tracker.effective_system()));
    }

    const QString excerpt =
        launch_log_excerpt(logPath, tracker.log_offset());
    if (!excerpt.isEmpty())
        message += QStringLiteral("\n\nRecent JGRF output:\n%1").arg(excerpt);
    else
        message += QStringLiteral(
            "\n\nNo new JGRF log output was available. Open Tools > "
            "Diagnostics & Logs for the complete logs.");

    if (tracker.mvs_hardware()) {
        message += QStringLiteral(
            "\n\nCompatibility hint: this launch used MVS hardware. "
            "Some cartridges or homebrew require AES. In Game settings > "
            "General, try System Type: AES for this exact media. This is a "
            "suggestion, not an automatic diagnosis.");
    }
    return message;
}

} // namespace

void MainWindow::loadGamePlaytime() {
    GamePlaytimeStore loaded;
    std::string error;
    if (!loaded.load(m_paths.game_playtime_json, &error)) {
        // Keep the last valid in-memory data if a changed config directory
        // contains a malformed file, and never overwrite that file blindly.
        m_gamePlaytimePersistenceAvailable = false;
        DebugLogger::logError(
            QString("could not load game playtime statistics: %1")
                .arg(QString::fromStdString(error)));
        return;
    }

    m_gamePlaytime = std::move(loaded);
    m_gamePlaytimePersistenceAvailable = true;
    DebugLogger::logInfo(
        QString("loaded playtime statistics for %1 exact media item(s)")
            .arg(static_cast<qulonglong>(m_gamePlaytime.size())));
}

void MainWindow::saveGamePlaytime() {
    if (!m_gamePlaytimePersistenceAvailable) return;

    std::string error;
    if (!m_gamePlaytime.save(m_paths.game_playtime_json, &error)) {
        DebugLogger::logError(
            QString("could not save game playtime statistics: %1")
                .arg(QString::fromStdString(error)));
    }
}

bool MainWindow::trackGameProcess(qint64 pid,
                                  const std::string& system,
                                  const std::string& media,
                                  const QString& displayName,
                                  const JollygoodLaunchPreparation& preparation,
                                  std::uintmax_t logOffset) {
    auto tracker = std::make_unique<DetachedProcessTracker>(
        static_cast<std::int64_t>(pid), system, media,
        displayName.toStdString(), preparation.effective_system,
        preparation.mvs_hardware, logOffset);
    if (!tracker->valid()) {
        const QString failure = unobserved_launch_text(
            pid, *tracker, m_paths.base_dir / "jollygood.log");
        DebugLogger::logError(
            QString("could not observe detached JGRF process: %1")
                .arg(failure));
        if (!m_closeRequested && isVisible()) {
            addNotification({
                "launch:observer",
                NotificationCategory::Configuration,
                NotificationSeverity::Warning,
                "Game launch status is unavailable",
                failure.left(1000).toStdString(),
                QDateTime::currentSecsSinceEpoch(),
                false,
                NotificationAction::OpenLogs,
            });
            showThemedMessageDialog(
                this, "JGRF Launch Status Unavailable", failure,
                ThemedMessageTone::Warning);
        }
        return false;
    }

    m_trackedGameProcesses.push_back(std::move(tracker));
    updateBareCaptureHotkeys();
    updateScreenshotAction();
    if (m_playtimeTimer && !m_playtimeTimer->isActive())
        m_playtimeTimer->start();
    return true;
}

void MainWindow::refreshCommandCatalog() {
    const fs::path path = m_paths.metadata_dir / "command.dat";
    std::error_code ec;
    const bool present = fs::is_regular_file(path, ec);
    const auto writeTime = present && !ec
        ? std::optional<fs::file_time_type>(fs::last_write_time(path, ec))
        : std::nullopt;
    const auto bytes = present && !ec
        ? std::optional<std::uintmax_t>(fs::file_size(path, ec))
        : std::nullopt;
    if (path == m_commandCatalogPath && writeTime == m_commandCatalogWriteTime &&
        bytes == m_commandCatalogSize && !ec) return;

    m_commandCatalogPath = path;
    m_commandCatalogWriteTime = writeTime;
    m_commandCatalogSize = bytes;
    std::string error;
    const CommandDatLoadStatus status = m_commandCatalog.load(path, &error);
    m_commandCatalogLoaded = status == CommandDatLoadStatus::Loaded;
    if (status == CommandDatLoadStatus::Error) {
        DebugLogger::logError(QString("could not load optional command.dat: %1")
                                  .arg(QString::fromStdString(error)));
    }
}

bool MainWindow::hasCommands(const Game& game, int romIndex) const {
    if (!m_commandCatalogLoaded) return false;
    for (const std::string& id : command_dat_lookup_ids(game, romIndex)) {
        if (m_commandCatalog.find(id)) return true;
    }
    return false;
}

void MainWindow::openCommandCompanion(
        qint64 pid, const QString& gameTitle,
        const std::vector<std::string>& lookupIds) {
    if (pid <= 0 || lookupIds.empty()) return;

    refreshCommandCatalog();
    if (!m_commandCatalogLoaded) return;

    const CommandDatEntry* entry = nullptr;
    std::string matchedId;
    for (const std::string& id : lookupIds) {
        entry = m_commandCatalog.find(id);
        if (entry) {
            matchedId = id;
            break;
        }
    }
    if (!entry) return;

    QString exactMediaKey = QString::fromStdString(matchedId);
    for (const auto& tracker : m_trackedGameProcesses) {
        if (tracker->pid() != pid) continue;
        exactMediaKey = QString::fromStdString(tracker->system()) + '/' +
                        QString::fromStdString(tracker->media());
        break;
    }
    const fs::path positionPath = m_paths.config_dir / "command_positions.ini";
    const QString positionFile = path_to_qstring(positionPath);
    auto dialog = std::make_unique<CommandDialog>(
        gameTitle.isEmpty() ? QString::fromStdString(matchedId) : gameTitle,
        QString::fromStdString(matchedId),
        QString::fromUtf8(entry->text.data(),
                          static_cast<qsizetype>(entry->text.size())),
        styleSheet(),
        positionFile,
        exactMediaKey);
    dialog->placeBeside(
        this, static_cast<int>(m_commandDialogs.size()));
    dialog->show();
    m_commandDialogs[static_cast<std::int64_t>(pid)] = std::move(dialog);

    DebugLogger::logInfo(
        QString("opened command.dat companion for MAME id %1 (PID %2)")
            .arg(QString::fromStdString(matchedId))
            .arg(pid));
}

void MainWindow::closeCommandCompanion(std::int64_t pid) {
    const auto it = m_commandDialogs.find(pid);
    if (it == m_commandDialogs.end()) return;
    it->second->close();
    m_commandDialogs.erase(it);
}

void MainWindow::pollTrackedGameProcesses() {
    bool changed = false;
    std::vector<TrackedLaunchFailurePresentation> startupFailures;
    auto it = m_trackedGameProcesses.begin();
    while (it != m_trackedGameProcesses.end()) {
        DetachedProcessTracker& tracker = **it;
        if (tracker.state() != DetachedProcessState::Exited) {
            ++it;
            continue;
        }

        const std::int64_t endedEpoch =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        const std::int64_t seconds = tracker.elapsed_seconds();
        const std::optional<std::uint32_t> exitCode = tracker.exit_code();
        closeCommandCompanion(tracker.pid());
        if (should_record_tracked_session(seconds, exitCode)) {
            changed |= m_gamePlaytime.add_session(
                tracker.system(), tracker.media(), seconds, endedEpoch);
            DebugLogger::logInfo(
                QString("JGRF play session ended: %1 second(s), %2")
                    .arg(static_cast<qlonglong>(seconds))
                    .arg(QString::fromStdString(tracker.media())));
        } else {
            const QString codeText = launch_exit_code_text(exitCode);
            DebugLogger::logInfo(
                QString("JGRF launch did not become a countable play "
                        "session: %1 second(s), exit=%2, %3")
                    .arg(static_cast<qlonglong>(seconds))
                    .arg(codeText)
                    .arg(QString::fromStdString(tracker.media())));
            if (is_likely_tracked_launch_failure(seconds, exitCode)) {
                TrackedLaunchFailurePresentation failure =
                    tracked_launch_failure_presentation(
                        tracker, seconds, exitCode,
                        m_paths.base_dir / "jollygood.log");
                DebugLogger::logError(
                    QString("JGRF startup failure: %1")
                        .arg(failure.message));
                if (!m_closeRequested &&
                    !m_exitWhenTrackedProcessesFinish && isVisible()) {
                    startupFailures.push_back(std::move(failure));
                }
            }
        }
        it = m_trackedGameProcesses.erase(it);
    }
    updateBareCaptureHotkeys();
    updateScreenshotAction();

    // Remove every exited tracker before entering a modal dialog. The nested
    // event loop may deliver another timer tick; no failure may be reported
    // twice merely because its first message is still open.
    for (const TrackedLaunchFailurePresentation& failure : startupFailures) {
        addNotification({
            "launch:startup",
            NotificationCategory::Configuration,
            NotificationSeverity::Error,
            failure.title.toStdString(),
            failure.message.left(1000).toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            failure.action,
        });
        showThemedMessageDialog(
            this, failure.title, failure.message,
            ThemedMessageTone::Warning);
    }

    if (changed) {
        saveGamePlaytime();
        const bool playtimeAffectsView =
            m_sortKey.startsWith("playtime") ||
            m_playtimeFilter != LibraryPlaytimeFilter::Any;
        if (playtimeAffectsView) {
            refreshLibraryView(true);
        } else {
            updateSelection();
        }
    }
    if (m_trackedGameProcesses.empty()) {
        if (m_playtimeTimer) m_playtimeTimer->stop();
        if (m_exitWhenTrackedProcessesFinish) {
            DebugLogger::logInfo(
                "all background JGRF sessions finished; exiting Goliath");
            QCoreApplication::quit();
        }
    }
}

void MainWindow::finishTrackedGameSessions() {
    if (m_playtimeTimer) m_playtimeTimer->stop();

    const std::int64_t endedEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    bool changed = false;
    for (const auto& tracker : m_trackedGameProcesses) {
        // Detached games intentionally survive Goliath. This is a fallback
        // for application-wide shutdown paths other than the normal window
        // close, which now remains in the background until tracking finishes.
        const std::int64_t seconds = tracker->elapsed_seconds();
        const std::optional<std::uint32_t> exitCode = tracker->exit_code();
        if (should_record_tracked_session(seconds, exitCode)) {
            changed |= m_gamePlaytime.add_session(
                tracker->system(), tracker->media(), seconds, endedEpoch);
        }
    }
    m_trackedGameProcesses.clear();
    updateBareCaptureHotkeys();
    updateScreenshotAction();
    for (auto& [pid, dialog] : m_commandDialogs) {
        (void)pid;
        dialog->close();
    }
    m_commandDialogs.clear();

    if (changed) saveGamePlaytime();
}

bool MainWindow::hasActiveTrackedGameProcess() const {
    for (const auto& tracker : m_trackedGameProcesses) {
        // Unknown is intentionally treated as active. Destructive save-data
        // work is unsafe unless process termination is positively known.
        if (tracker->state() != DetachedProcessState::Exited) return true;
    }
    return false;
}

void MainWindow::launchMedia(const QString& media, const std::string& system,
                             const GameLaunchProfile* profile,
                             std::optional<fs::path> waveOutputPath,
                             std::vector<std::string> commandLookupIds,
                             QString commandTitle) {
    const JollygoodLaunchPreparation preparation = prepare_jollygood_launch(
        m_paths, m_romDir, m_neocdDir, system, media.toStdString(), profile,
        m_verboseJgrfLogging, waveOutputPath);
    const bool chdLaunch =
        system == "neogeocd" && launch_extension_lower(media.toStdString()) == ".chd";

    if (chdLaunch &&
        (preparation.status == LaunchPreparationStatus::Ready ||
         preparation.status == LaunchPreparationStatus::ChdUnsupported)) {
        const QString formatsText = preparation.advertised_formats.empty()
            ? QString("not advertised")
            : QString::fromStdString(preparation.advertised_formats);
        DebugLogger::logInfo(
            QString("Geolith Neo Geo CD formats: %1; CHD support=%2")
                .arg(formatsText)
                .arg(preparation.chd_supported ? "yes" : "no"));
    }

    if (preparation.status != LaunchPreparationStatus::Ready) {
        const QString failure = QString::fromStdString(
            launch_preparation_failure_text(preparation));
        DebugLogger::logError(
            QString("launch preflight blocked %1: %2")
                .arg(media, failure));
        addNotification({
            QString("launch:preflight:%1")
                .arg(static_cast<int>(preparation.status)).toStdString(),
            NotificationCategory::Configuration,
            NotificationSeverity::Warning,
            waveOutputPath.has_value()
                ? "Audio export cannot start"
                : "Game cannot start",
            failure.left(1000).toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            launch_notification_action(preparation.status),
        });
        showThemedMessageDialog(
            this,
            waveOutputPath.has_value()
                ? QStringLiteral("Audio WAV Export Cannot Start")
                : QStringLiteral("Game Cannot Start"),
            failure,
            ThemedMessageTone::Warning);
        return;
    }

    if (!confirm_jollygood_audio_before_launch(this)) return;

    warnIfBiosMissing();
    ensure_jollygood_bios(m_paths);

    DebugLogger::logInfo(QString("launching %1 media: %2")
                             .arg(QString::fromStdString(system))
                             .arg(media));
    DebugLogger::logInfo(QString("jollygood args: %1").arg(preparation.args.join(" ")));
    DebugLogger::logInfo(QString("jollygood env: XDG_CONFIG_HOME=%1 XDG_DATA_HOME=%2")
                             .arg(preparation.environment.value("XDG_CONFIG_HOME"))
                             .arg(preparation.environment.value("XDG_DATA_HOME")));
    if (preparation.profile_applied) {
        DebugLogger::logInfo(
            QString("per-game launch profile active: %1")
                .arg(preparation.profile_config_root));
    }
    if (m_verboseJgrfLogging) {
        DebugLogger::logInfo(
            "session verbose JGRF logging requested for this launch");
    }
    if (waveOutputPath.has_value()) {
        const QString output = path_to_qstring(*waveOutputPath);
        DebugLogger::logInfo(
            QString("one-shot JGRF WAV export requested: %1").arg(output));
    }

    qint64 pid = 0;
    QString launchError;
    const std::uintmax_t logOffset =
        launch_log_offset(m_paths.base_dir / "jollygood.log");
    if (!launch_jollygood_detached(
            preparation.args,
            preparation.working_directory,
            preparation.environment,
            preparation.log_path,
            &pid,
            &launchError)) {
        DebugLogger::logError(
            QString("failed to launch jollygood for %1 media: %2: %3")
                .arg(QString::fromStdString(system), media, launchError));
        addNotification({
            "launch:process",
            NotificationCategory::Configuration,
            NotificationSeverity::Error,
            "JGRF process could not start",
            QString("%1\nMedia: %2")
                .arg(launchError, media).left(1000).toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            NotificationAction::OpenLogs,
        });
        showThemedMessageDialog(
            this,
            "JGRF Launch Failed",
            QString("Could not start the Jollygood process.\n\n"
                    "Reason: %1\n\nMedia: %2\nEffective system: %3")
                .arg(launchError)
                .arg(media)
                .arg(QString::fromStdString(
                    preparation.effective_system)),
            ThemedMessageTone::Critical);
        return;
    }

    if (trackGameProcess(
            pid, system, media.toStdString(), commandTitle,
            preparation, logOffset) &&
        !waveOutputPath.has_value() && m_commandOverlayEnabled) {
        openCommandCompanion(pid, commandTitle, commandLookupIds);
    }
    apply_jollygood_audio_volume_with_retry(
        this, pid, m_config.get_int("Audio", "volume", 100));
}

} // namespace goliath
