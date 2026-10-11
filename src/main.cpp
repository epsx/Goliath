#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#if defined(GOLIATH_WAYLAND_CAPTURE)
#include <QGuiApplication>
#endif

#include "common/debug_logger.hpp"
#include "common/goliath_common.hpp"
#include "common/qt_path.hpp"
#include "ui/main_window.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
#if defined(GOLIATH_WAYLAND_CAPTURE)
    if (QGuiApplication::platformName() == "wayland")
        app.setDesktopFileName("io.github.epsx.Goliath");
#endif
    // MainWindow explicitly decides when the application may exit. If a game
    // is still active, closing the visible window leaves the event loop alive
    // until the detached-process observer has finalized the full session.
    app.setQuitOnLastWindowClosed(false);
    app.setStyle("Fusion");
    // Applies to every top-level window (MainWindow's custom title bar and
    // SettingsDialog's native title bar alike) so the same branded icon is
    // used everywhere instead of relying on Qt/Windows fallbacks.
    app.setWindowIcon(QIcon(":/icons/goliath-qt.ico"));

    const QString debugLogPath = goliath::path_to_qstring(
        goliath::get_base_dir() / "goliath-qt-debug.log");
    QString debugLogError;
    if (!goliath::DebugLogger::initialize(debugLogPath, &debugLogError)) {
        QMessageBox::warning(
            nullptr,
            "Frontend Log Unavailable",
            QString("Goliath refused to open its frontend log and will "
                    "continue without file logging.\n\n%1")
                .arg(debugLogError));
    }
    goliath::DebugLogger::logInfo("Goliath Qt started");

    goliath::Config config = goliath::load_config();
    goliath::DebugLogger::logInfo(
        QString("config path: %1")
            .arg(goliath::path_to_qstring(goliath::get_config_path())));

    goliath::MainWindow window(config);
    window.show();
    return app.exec();
}
