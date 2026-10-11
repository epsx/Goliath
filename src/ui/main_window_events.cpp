#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "game/rescan_worker.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>
#include <QSplitter>
#include <QTimer>

#include <algorithm>
#include <string>

namespace goliath {

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
    m_closeRequested = true;
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

    const int shadowMargin = framelessWindowShadowMargin(this);
    m_config.set("UI", "window_width",
                 std::to_string(std::max(800, width() - shadowMargin * 2)));
    m_config.set("UI", "window_height",
                 std::to_string(std::max(600, height() - shadowMargin * 2)));
    rememberCurrentLibrarySelection();
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
    updateAvailableIndicator();
    // Child layouts and Classic View splitter changes settle after this event.
    // Recalculate only the icon-mode grid once the new viewport width exists.
    QTimer::singleShot(0, this, [this]() {
        updateIconViewGridSize();
    });

    if (!m_splitter) {
        return;
    }

    constexpr int collapseWidth = 1000;
    constexpr int restoreWidth = 1150;

    const int width = contentsRect().width();

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
