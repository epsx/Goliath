#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "common/diagnostic_report.hpp"
#include "common/file_transaction.hpp"
#include "common/qt_path.hpp"

#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QThread>

#include <exception>
#include <memory>
#include <string>
#include <utility>

#ifndef GOLIATH_VERSION
#define GOLIATH_VERSION "development"
#endif
#ifndef GOLIATH_BUILD_DATE
#define GOLIATH_BUILD_DATE "unknown"
#endif
#ifndef GOLIATH_BUILD_REVISION
#define GOLIATH_BUILD_REVISION "unknown"
#endif

namespace goliath {

void MainWindow::exportDiagnosticReport() {
    const QString timestamp =
        QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QString destinationDirectory = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (destinationDirectory.isEmpty())
        destinationDirectory = path_to_qstring(m_paths.base_dir);
    const QString suggested = destinationDirectory +
        "/Goliath-Diagnostic-" + timestamp + ".txt";

    QString destination = QFileDialog::getSaveFileName(
        this, "Export Diagnostic Report", suggested,
        "Text files (*.txt);;All files (*.*)");
    if (destination.isEmpty()) return;
    if (QFileInfo(destination).suffix().isEmpty()) destination += ".txt";

    DiagnosticReportRequest request;
    request.config = m_config;
    request.paths = m_paths;
    request.rom_dir = m_romDir;
    request.neocd_dir = m_neocdDir;
    request.snapshot_dir = m_snapDir;
    request.application_version = GOLIATH_VERSION;
    request.build_date = GOLIATH_BUILD_DATE;
    request.build_revision = GOLIATH_BUILD_REVISION;
    request.qt_platform = QGuiApplication::platformName().toStdString();
    request.session_verbose_logging = m_verboseJgrfLogging;

    auto generated = std::make_shared<DiagnosticReportResult>();
    auto generationError = std::make_shared<std::string>();
    QThread* worker = QThread::create(
        [request = std::move(request), generated, generationError]() {
            try {
                *generated = generate_diagnostic_report(request);
            } catch (const std::exception& error) {
                *generationError = error.what();
            } catch (...) {
                *generationError = "Unknown diagnostic collection error.";
            }
        });

    QProgressDialog progress(
        "Collecting versions, configuration, file checks, and recent logs...",
        QString(), 0, 0, this);
    progress.setWindowTitle("Export Diagnostic Report");
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    connect(worker, &QThread::finished, &progress, &QProgressDialog::accept);
    worker->start();
    progress.exec();
    worker->wait();
    delete worker;

    if (!generationError->empty()) {
        QMessageBox::critical(
            this, "Export Diagnostic Report",
            QStringLiteral("The diagnostic report could not be generated.\n\n") +
                QString::fromStdString(*generationError));
        return;
    }

    std::string saveError;
    if (!save_files_transactionally(
            {{qstring_to_path(destination), generated->text}}, &saveError)) {
        QMessageBox::critical(
            this, "Export Diagnostic Report",
            QString("The diagnostic report could not be saved.\n\n%1")
                .arg(QString::fromStdString(saveError)));
        return;
    }

    DebugLogger::logInfo("privacy-redacted diagnostic report exported");
    QMessageBox::information(
        this, "Diagnostic Report Exported",
        QString("The privacy-redacted diagnostic report was saved to:\n%1\n\n"
                "Required-file problems: %2\nBIOS problems: %3")
            .arg(destination)
            .arg(generated->required_file_problems)
            .arg(generated->bios_problems));
}

} // namespace goliath
