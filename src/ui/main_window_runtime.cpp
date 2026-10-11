#include "ui/main_window.hpp"

#include "audio/jollygood_audio.hpp"
#include "common/debug_logger.hpp"
#include "common/qt_path.hpp"
#include "game/jollygood_bios.hpp"
#include "game/jollygood_launch.hpp"
#include "game/jollygood_process.hpp"

#include <QCoreApplication>
#include <QMessageBox>
#include <QString>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace goliath {

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
                                  const std::string& media) {
    auto tracker = std::make_unique<DetachedProcessTracker>(
        static_cast<std::int64_t>(pid), system, media);
    if (!tracker->valid()) {
        DebugLogger::logError(
            QString("could not observe detached JGRF process %1; "
                    "playtime will not be recorded for %2")
                .arg(pid)
                .arg(QString::fromStdString(media)));
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
            const QString codeText = exitCode.has_value()
                ? QString("0x%1").arg(
                      QString::number(*exitCode, 16).rightJustified(8, '0'))
                : QStringLiteral("unavailable");
            DebugLogger::logInfo(
                QString("JGRF launch did not become a countable play "
                        "session: %1 second(s), exit=%2, %3")
                    .arg(static_cast<qlonglong>(seconds))
                    .arg(codeText)
                    .arg(QString::fromStdString(tracker.media())));
        }
        it = m_trackedGameProcesses.erase(it);
    }
    updateBareCaptureHotkeys();
    updateScreenshotAction();

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

    switch (preparation.status) {
    case LaunchPreparationStatus::ExecutableNotFound:
        QMessageBox::critical(
            this,
            chdLaunch ? "Neo Geo CD CHD Launch" : "Error",
            QString("Jollygood executable not found: %1")
                .arg(path_to_qstring(m_paths.jollygood_exe)));
        return;
    case LaunchPreparationStatus::CapabilityProbeFailed:
        DebugLogger::logError(
            QString("could not probe Geolith CHD capability: %1")
                .arg(QString::fromStdString(preparation.detail)));
        QMessageBox::warning(
            this,
            "Neo Geo CD CHD Launch",
            QString("Goliath could not verify CHD support in the installed Geolith core.\n\n%1\n\n"
                    "Open Settings > Info and use Refresh Information for details.")
                .arg(QString::fromStdString(preparation.detail)));
        return;
    case LaunchPreparationStatus::ChdUnsupported:
        QMessageBox::information(
            this,
            "Neo Geo CD CHD Launch",
            QString("The installed Geolith core does not advertise CHD support for Neo Geo CD.\n\n"
                    "Advertised formats: %1\n\n"
                    "You can confirm this in Settings > Info.")
                .arg(preparation.advertised_formats.empty()
                         ? QString("None")
                         : QString::fromStdString(preparation.advertised_formats)));
        return;
    case LaunchPreparationStatus::UnsupportedMedia:
        QMessageBox::warning(
            this,
            "Neo Geo CD Launch",
            QString("Unsupported Neo Geo CD image type: %1").arg(media));
        return;
    case LaunchPreparationStatus::ProfileConfigurationFailed:
        DebugLogger::logError(
            QString("could not prepare per-game launch profile: %1")
                .arg(QString::fromStdString(preparation.detail)));
        QMessageBox::warning(
            this,
            "Per-game Settings",
            QString("Goliath could not prepare the isolated configuration for this game.\n\n%1\n\n"
                    "The global JGRF configuration was not modified.")
                .arg(QString::fromStdString(preparation.detail)));
        return;
    case LaunchPreparationStatus::WaveOutputConflict:
        QMessageBox::warning(
            this,
            "Audio WAV Export",
            QString("Goliath could not start the one-shot WAV export.\n\n%1")
                .arg(QString::fromStdString(preparation.detail)));
        return;
    case LaunchPreparationStatus::WaveOutputInvalid:
        QMessageBox::warning(
            this,
            "Audio WAV Export",
            QString("The selected WAV destination is no longer safe to use.\n\n%1")
                .arg(QString::fromStdString(preparation.detail)));
        return;
    case LaunchPreparationStatus::Ready:
        break;
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
        QMessageBox::critical(
            this,
            "JGRF Launch Failed",
            QString("Could not start the Jollygood process.\n\n%1")
                .arg(launchError));
        return;
    }

    if (trackGameProcess(pid, system, media.toStdString()) &&
        !waveOutputPath.has_value() && m_commandOverlayEnabled) {
        openCommandCompanion(pid, commandTitle, commandLookupIds);
    }
    apply_jollygood_audio_volume_with_retry(
        this, pid, m_config.get_int("Audio", "volume", 100));
}

} // namespace goliath
