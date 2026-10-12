#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "common/paths.hpp"
#include "game/rescan_worker.hpp"
#include "ui/rescan_dialog.hpp"

#include <QAction>
#include <QMessageBox>
#include <QString>

#include <string>
#include <utility>

namespace goliath {

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
        QString("loaded personal library state for %1 favorite and %2 rated "
                "exact media item(s)")
            .arg(static_cast<qulonglong>(
                m_gameLibraryState.favorite_count()))
            .arg(static_cast<qulonglong>(
                m_gameLibraryState.rating_count())));
}

void MainWindow::rescanRoms() {
    m_rescanAction->setEnabled(false);

    if (m_rescanWorker) {
        m_rescanWorker->stop();
        m_rescanWorker->wait();
        delete m_rescanWorker;
        m_rescanWorker = nullptr;
    }

    auto* worker = new RescanWorker(m_config, m_games, this);
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
    const QString preferredRom = selectedRomFile();

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
    refreshLibraryView(false, preferredRom);
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

} // namespace goliath
