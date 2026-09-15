#include "ui/main_window.hpp"
#include "audio/jollygood_audio.hpp"
#include "ui/about_dialog.hpp"
#include "ui/widgets/title_bar.hpp"
#include "ui/settings_dialog.hpp"
#include "ui/logging_dialog.hpp"
#include "common/paths.hpp"
#include "common/debug_logger.hpp"
#include "game/jollygood_process.hpp"
#include "game/jollygood_executable.hpp"
#include "game/jollygood_bios.hpp"
#include "game/jollygood_launch.hpp"
#include "game/bios_verify.hpp"
#include "game/rescan_worker.hpp"
#include "ui/rescan_dialog.hpp"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QResizeEvent>
#include <QDialog>
#include <QFont>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStringList>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <utility>


namespace fs = std::filesystem;

namespace goliath {


MainWindow::MainWindow(Config config, QWidget* parent)
    : QMainWindow(parent), m_config(std::move(config)) {
    setWindowTitle("Goliath - Neo Geo");
    setObjectName("frameless_window");
    setWindowFlags(Qt::FramelessWindowHint);
    setContentsMargins(1, 1, 1, 1);

    new ResizeFilter(this);

    refreshResolvedPaths();
    loadGameProfiles();
    loadGamePlaytime();
    loadGameLibraryState();

    DebugLogger::logInfo(QString("base_dir: %1").arg(QString::fromStdString(m_paths.base_dir.string())));
    DebugLogger::logInfo(QString("config_dir: %1").arg(QString::fromStdString(m_paths.config_dir.string())));
    DebugLogger::logInfo(QString("rom_dir: %1").arg(QString::fromStdString(m_romDir.string())));
    DebugLogger::logInfo(QString("neocd_dir: %1").arg(QString::fromStdString(m_neocdDir.string())));
    DebugLogger::logInfo(QString("snapshot_dir: %1").arg(QString::fromStdString(m_snapDir.string())));
    DebugLogger::logInfo(QString("database_path: %1").arg(QString::fromStdString(m_paths.json_file.string())));
    const fs::path resolvedJollygood =
        resolve_jollygood_executable(m_paths.jollygood_exe);
    DebugLogger::logInfo(
        QString("jollygood_exe: configured=%1 resolved=%2")
            .arg(QString::fromStdString(m_paths.jollygood_exe.string()))
            .arg(resolvedJollygood.empty()
                     ? QStringLiteral("<not found>")
                     : QString::fromStdString(resolvedJollygood.string())));

    int width = std::max(m_config.get_int("UI", "window_width", 1200), 800);
    int height = std::max(m_config.get_int("UI", "window_height", 800), 600);
    resize(width, height);
    centerWindow();

    loadGames();
    DebugLogger::logInfo(QString("loaded %1 game entries").arg((int)m_games.size()));

    m_sortKey = QString::fromStdString(m_config.get("UI", "sort_key", "display"));
    if (m_sortKey != "display" && m_sortKey != "display_desc" &&
        m_sortKey != "year" && m_sortKey != "year_desc") {
        m_sortKey = "display";
    }
    m_showVariants = m_config.get_bool("UI", "show_variants", true);
    m_favoritesOnly = m_config.get_bool("UI", "favorites_only", false);
    m_librarySystem = m_config.get("UI", "library_system", "neogeo");
    if (m_librarySystem != "neogeo" && m_librarySystem != "neogeocd") {
        m_librarySystem = "neogeo";
    }

    buildUi();
    m_playtimeTimer = new QTimer(this);
    m_playtimeTimer->setInterval(1000);
    connect(m_playtimeTimer, &QTimer::timeout,
            this, &MainWindow::pollTrackedGameProcesses);
    refreshLibraryView(false);
    applyTheme(QString::fromStdString(m_config.get("UI", "theme", "Dark Modern")));
}

void MainWindow::refreshResolvedPaths() {
    m_paths = compute_app_paths(m_config);
    m_romDir = resolve_path(m_config, "roms");
    m_neocdDir = resolve_path(m_config, "neocd");
    m_snapDir = resolve_path(m_config, "snaps");
}

void MainWindow::loadGames() {
    m_games = load_games(m_paths.json_file);
}

void MainWindow::loadGameProfiles() {
    std::string error;
    if (!m_gameProfiles.load(m_paths.game_profiles_json, &error)) {
        DebugLogger::logError(
            QString("could not load per-game profiles: %1")
                .arg(QString::fromStdString(error)));
        return;
    }
    DebugLogger::logInfo(
        QString("loaded %1 per-game launch profile(s)")
            .arg(static_cast<qulonglong>(m_gameProfiles.size())));
}

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

void MainWindow::loadGameLibraryState() {
    GameLibraryStateStore loaded;
    std::string error;
    if (!loaded.load(m_paths.game_library_state_json, &error)) {
        // Keep the last valid in-memory state and do not overwrite a malformed
        // file after a configured path change.
        m_gameLibraryStatePersistenceAvailable = false;
        DebugLogger::logError(
            QString("could not load personal game library state: %1")
                .arg(QString::fromStdString(error)));
        return;
    }

    m_gameLibraryState = std::move(loaded);
    m_gameLibraryStatePersistenceAvailable = true;
    DebugLogger::logInfo(
        QString("loaded %1 favorite exact media item(s)")
            .arg(static_cast<qulonglong>(
                m_gameLibraryState.favorite_count())));
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

void MainWindow::trackGameProcess(qint64 pid,
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
        return;
    }

    m_trackedGameProcesses.push_back(std::move(tracker));
    if (m_playtimeTimer && !m_playtimeTimer->isActive())
        m_playtimeTimer->start();
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

    if (changed) {
        saveGamePlaytime();
        updateSelection();
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
                             std::optional<fs::path> waveOutputPath) {
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
                .arg(QString::fromStdString(m_paths.jollygood_exe.string())));
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
#if defined(_WIN32)
        const QString output =
            QString::fromStdWString(waveOutputPath->wstring());
#else
        const QString output =
            QString::fromStdString(waveOutputPath->string());
#endif
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

    trackGameProcess(pid, system, media.toStdString());
    apply_jollygood_audio_volume_with_retry(
        this, pid, m_config.get_int("Audio", "volume", 100));
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        m_titleBar->updateMaxButton();
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::event(QEvent* e) {
    // Menu items emit (empty) StatusTip events on hover, which would clear
    // the permanent stats message in the status bar. Ignore them.
    if (e->type() == QEvent::StatusTip) {
        return true;
    }
    return QMainWindow::event(e);
}

void MainWindow::verifyBios() {
    auto resultOpt = verify_bios(m_paths.bios_dir);
    if (!resultOpt.has_value()) {
        QMessageBox::warning(this, "Verify BIOS",
            QString("BIOS folder not found:\n%1").arg(QString::fromStdString(m_paths.bios_dir.string())));
        return;
    }
    const BiosVerifyResult& result = *resultOpt;

    QString header;
    if (result.problems == 0) {
        header = QString::fromUtf8("\xE2\x9C\x85 All BIOS files are compatible with geolith.\n\n");
    } else {
        header = QString::fromUtf8("\xE2\x9A\xA0 %1 problem(s) found. Geolith needs BIOS files from a recent MAME set.\n\n")
                     .arg(result.problems);
    }

    QString body;
    for (const std::string& line : result.lines) body += QString::fromStdString(line) + "\n";

    QDialog dialog(this);
    dialog.resize(640, 480);
    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(&dialog, "Verify BIOS", &layout);
    auto* text = new QTextEdit();
    text->setReadOnly(true);
    text->setFont(QFont("Consolas", 9));
    text->setPlainText(header + body);
    layout->addWidget(text);
    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
    dialog.exec();
}

void MainWindow::warnIfBiosMissing() {
    std::error_code ec;

    if (!fs::is_directory(m_paths.bios_dir, ec)) {
        QMessageBox::warning(this, "BIOS folder not found",
            QString("The configured BIOS folder does not exist:\n%1\n\n"
                    "Set it in Settings > Path, or run Tools > Verify BIOS.")
                .arg(QString::fromStdString(m_paths.bios_dir.string())));
        return;
    }

    QStringList missing;
    for (const auto& set : bios_sets()) {
        fs::path zipPath = m_paths.bios_dir / set.zip_name;
        if (!fs::exists(zipPath, ec)) {
            missing << QString::fromStdString(set.zip_name + " - " + set.purpose +
                                              (set.required ? " (required)" : " (optional)"));
        }
    }

    if (!missing.isEmpty()) {
        QMessageBox::warning(this, "Missing BIOS files",
            QString("The following BIOS archives are missing from:\n%1\n\n%2\n\n"
                    "Games may fail to launch. Run Tools > Verify BIOS for details.")
                .arg(QString::fromStdString(m_paths.bios_dir.string()))
                .arg(missing.join("\n")));
        return;
    }
}

void MainWindow::openSettings() {
    SettingsDialog dialog(m_config, m_paths, this);
    dialog.exec();
    // Path tab edits (e.g. BIOS Folder) must apply to this session too,
    // without waiting for a rescan.
    refreshResolvedPaths();
    loadGameProfiles();
    loadGamePlaytime();
    loadGameLibraryState();
    refreshLibraryView(true);
}

void MainWindow::openLogging() {
    const bool configuredVerbose =
        jollygood_args_have_verbose_logging(m_paths.jollygood_args);
    LoggingDialog dialog(
        m_verboseJgrfLogging, configuredVerbose, m_paths,
        [this]() { return hasActiveTrackedGameProcess(); }, this);
    dialog.exec();

    const bool enabled = dialog.sessionVerboseLoggingEnabled();
    if (enabled != m_verboseJgrfLogging) {
        m_verboseJgrfLogging = enabled;
        DebugLogger::logInfo(
            QString("session verbose JGRF logging %1")
                .arg(enabled ? "enabled" : "disabled"));
    }
}

void MainWindow::openAbout() {
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::rescanRoms() {
    m_rescanAction->setEnabled(false);

    if (m_rescanWorker) {
        m_rescanWorker->stop();
        m_rescanWorker->wait();
        delete m_rescanWorker;
        m_rescanWorker = nullptr;
    }

    auto* worker = new RescanWorker(m_config, this);
    m_rescanWorker = worker;
    connect(worker, &RescanWorker::completed, this, &MainWindow::onRescanFinished);
    connect(worker, &RescanWorker::error, this, &MainWindow::onRescanError);

    RescanDialog dialog(worker, this);
    worker->start();
    dialog.exec();

    // If the user closed the dialog before the scan completed, stop the
    // scan and clean up the worker before returning.
    if (m_rescanWorker && m_rescanWorker->isRunning()) {
        m_rescanWorker->stop();
        m_rescanWorker->wait();
        m_rescanWorker->deleteLater();
        m_rescanWorker = nullptr;
        m_rescanAction->setEnabled(true);
    }
}

void MainWindow::onRescanFinished() {
    if (m_rescanWorker) {
        m_rescanWorker->wait(); // Ensure the thread has finished before deleting it.
        m_rescanWorker->deleteLater();
        m_rescanWorker = nullptr;
    }

    // Path tab changes (roms/icons/snaps/metadata) are only picked up by
    // the scanner at rescan time; refresh the same canonical path state used
    // by launch and Settings before reloading the library.
    refreshResolvedPaths();

    loadGames();
    refreshLibraryView(false);
    m_rescanAction->setEnabled(true);
}

void MainWindow::onRescanError(QString msg) {
    if (m_rescanWorker) {
        m_rescanWorker->wait(); // Ensure the thread has finished before deleting it.
        m_rescanWorker->deleteLater();
        m_rescanWorker = nullptr;
    }
    m_rescanAction->setEnabled(true);
    QMessageBox::critical(this, "Rescan Error", msg);
}


void MainWindow::centerWindow() {
    QScreen* screen = this->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect geo = screen->availableGeometry();
    int newWidth = std::min(width(), geo.width());
    int newHeight = std::min(height(), geo.height());
    if (newWidth != width() || newHeight != height()) {
        resize(newWidth, newHeight);
    }
    int x = geo.x() + (geo.width() - width()) / 2;
    int y = geo.y() + (geo.height() - height()) / 2;
    if (x < geo.x()) x = geo.x();
    if (y < geo.y()) y = geo.y();
    if (x + width() > geo.x() + geo.width()) x = geo.x() + geo.width() - width();
    if (y + height() > geo.y() + geo.height()) y = geo.y() + geo.height() - height();
    move(x, y);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Harvest processes which ended since the last timer tick before deciding
    // whether the frontend must remain as a background observer.
    pollTrackedGameProcesses();

    if (m_rescanWorker) {
        disconnect(m_rescanWorker, nullptr, this, nullptr);
        m_rescanWorker->stop();
        m_rescanWorker->wait();
        delete m_rescanWorker;
        m_rescanWorker = nullptr;
    }

    m_config.set("UI", "window_width", std::to_string(width()));
    m_config.set("UI", "window_height", std::to_string(height()));
    QString lastRom = selectedRomFile();
    if (!lastRom.isEmpty()) {
        m_config.set("UI", "last_rom", lastRom.toStdString());
    }
    save_config(m_config);

    if (!m_trackedGameProcesses.empty()) {
        m_exitWhenTrackedProcessesFinish = true;
        DebugLogger::logInfo(
            QString("Goliath window closed; monitoring %1 JGRF process(es) "
                    "in the background")
                .arg(static_cast<qlonglong>(
                    m_trackedGameProcesses.size())));
        hide();
        event->ignore();
        return;
    }

    finishTrackedGameSessions();
    QMainWindow::closeEvent(event);
    QCoreApplication::quit();
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    if (!m_splitter) {
        return;
    }

    constexpr int collapseWidth = 1000;
    constexpr int restoreWidth = 1150;

    const int width = event->size().width();

    if (!m_detailsCollapsed && width < collapseWidth) {
        const QList<int> sizes = m_splitter->sizes();

        if (sizes.size() == 2 && sizes[0] + sizes[1] > 0) {
            m_savedSplitterRatio =
                static_cast<double>(sizes[0]) /
                static_cast<double>(sizes[0] + sizes[1]);
        }

        m_splitter->setSizes({1, 0});
        m_detailsCollapsed = true;
        return;
    }

    if (m_detailsCollapsed && width >= restoreWidth) {
        const int totalWidth = m_splitter->width();

        if (totalWidth > 0 && m_savedSplitterRatio > 0.0) {
            const int leftWidth =
                static_cast<int>(totalWidth * m_savedSplitterRatio);
            m_splitter->setSizes({leftWidth, totalWidth - leftWidth});
        } else {
            m_splitter->setSizes({400, 760});
        }

        m_detailsCollapsed = false;
        m_savedSplitterRatio = 0.0;
    }
}


} // namespace goliath
