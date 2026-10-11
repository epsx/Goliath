#include "ui/main_window.hpp"
#include "ui/widgets/title_bar.hpp"
#include "common/paths.hpp"
#include "common/debug_logger.hpp"
#include "common/qt_path.hpp"
#include "game/jollygood_executable.hpp"

#include <QGuiApplication>
#include <QTimer>

#include <algorithm>
#include <utility>


namespace fs = std::filesystem;

namespace goliath {


MainWindow::MainWindow(Config config, QWidget* parent)
    : QMainWindow(parent), m_config(std::move(config)) {
    setWindowTitle("Goliath - Neo Geo");
    setObjectName("frameless_window");
    setWindowFlags(Qt::FramelessWindowHint);
    setContentsMargins(0, 0, 0, 0);

    new ResizeFilter(this);

    refreshResolvedPaths();
    loadGameProfiles();
    loadGamePlaytime();
    loadGameLibraryState();

    DebugLogger::logInfo(
        QString("base_dir: %1").arg(path_to_qstring(m_paths.base_dir)));
    DebugLogger::logInfo(
        QString("config_dir: %1").arg(path_to_qstring(m_paths.config_dir)));
    DebugLogger::logInfo(
        QString("rom_dir: %1").arg(path_to_qstring(m_romDir)));
    DebugLogger::logInfo(
        QString("neocd_dir: %1").arg(path_to_qstring(m_neocdDir)));
    DebugLogger::logInfo(
        QString("snapshot_dir: %1").arg(path_to_qstring(m_snapDir)));
    DebugLogger::logInfo(
        QString("database_path: %1").arg(path_to_qstring(m_paths.json_file)));
    const fs::path resolvedJollygood =
        resolve_jollygood_executable(m_paths.jollygood_exe);
    DebugLogger::logInfo(
        QString("jollygood_exe: configured=%1 resolved=%2")
            .arg(path_to_qstring(m_paths.jollygood_exe))
            .arg(resolvedJollygood.empty()
                     ? QStringLiteral("<not found>")
                     : path_to_qstring(resolvedJollygood)));

    int width = std::max(m_config.get_int("UI", "window_width", 1200), 800);
    int height = std::max(m_config.get_int("UI", "window_height", 800), 600);
    resize(width, height);
    installFramelessWindowShadow(this, "main_window_shadow_surface");
    centerWindow();

    loadGames();
    DebugLogger::logInfo(QString("loaded %1 game entries").arg((int)m_games.size()));

    m_sortKey = QString::fromStdString(m_config.get("UI", "sort_key", "display"));
    if (m_sortKey != "display" && m_sortKey != "display_desc" &&
        m_sortKey != "year" && m_sortKey != "year_desc" &&
        m_sortKey != "rating" && m_sortKey != "rating_desc" &&
        m_sortKey != "playtime" && m_sortKey != "playtime_desc") {
        m_sortKey = "display";
    }
    m_showVariants = m_config.get_bool("UI", "show_variants", true);
    m_commandOverlayEnabled =
        m_config.get_bool("UI", "command_overlay", true);
    m_favoritesOnly = m_config.get_bool("UI", "favorites_only", false);
    m_ratingFilter = libraryRatingFilterFromKey(
        m_config.get("UI", "rating_filter", "any"));
    m_playtimeFilter = libraryPlaytimeFilterFromKey(
        m_config.get("UI", "playtime_filter", "any"));
    m_libraryDisplayMode = libraryDisplayModeFromKey(
        m_config.get("UI", "library_view", "list"));
    m_librarySystem = m_config.get("UI", "library_system", "neogeo");
    if (m_librarySystem != "neogeo" && m_librarySystem != "neogeocd") {
        m_librarySystem = "neogeo";
    }

    buildUi();
    reloadCachedUpdateAlert();
    // Keep the documented compact window range authoritative. Without an
    // explicit limit, Qt can promote the expanded card layout's size hint to
    // an implicit minimum and prevent resizeEvent() from ever reaching the
    // width at which the details pane collapses into Classic View.
    setMinimumSize(800, 600);

#if defined(GOLIATH_WAYLAND_CAPTURE)
    // Let the main window show before the desktop asks for shortcut approval.
    if (QGuiApplication::platformName() == "wayland")
        QTimer::singleShot(0, this, [this]() { registerScreenshotHotkey(); });
    else
#endif
    registerScreenshotHotkey();
    updateScreenshotAction();
    m_playtimeTimer = new QTimer(this);
    m_playtimeTimer->setInterval(1000);
    connect(m_playtimeTimer, &QTimer::timeout,
            this, &MainWindow::pollTrackedGameProcesses);
    refreshLibraryView(false);
    applyTheme(QString::fromStdString(m_config.get("UI", "theme", "Dark Modern")));
    // Automatic checks are opt-in and never block the first paint. Results are
    // reflected by the header indicator without opening a startup dialog.
    QTimer::singleShot(0, this, [this]() {
        startAutomaticUpdateCheckIfDue();
    });
}

} // namespace goliath
