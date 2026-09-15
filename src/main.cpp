#include <QApplication>
#include <QIcon>
#include <QMessageBox>

#include "common/debug_logger.hpp"
#include "common/goliath_common.hpp"
#include "ui/main_window.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // MainWindow explicitly decides when the application may exit. If a game
    // is still active, closing the visible window leaves the event loop alive
    // until the detached-process observer has finalized the full session.
    app.setQuitOnLastWindowClosed(false);
    app.setStyle("Fusion");
    // Applies to every top-level window (MainWindow's custom title bar and
    // SettingsDialog's native title bar alike) so the same branded icon is
    // used everywhere instead of relying on Qt/Windows fallbacks.
    app.setWindowIcon(QIcon(":/icons/goliath-qt.ico"));

    const QString debugLogPath = QString::fromStdString((goliath::get_base_dir() / "goliath-qt-debug.log").string());
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
    goliath::DebugLogger::logInfo(QString("config path: %1").arg(QString::fromStdString(goliath::get_config_path().string())));

    goliath::MainWindow window(config);
    window.show();
    return app.exec();
}
