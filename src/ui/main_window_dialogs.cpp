#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "common/qt_path.hpp"
#include "game/bios_verify.hpp"
#include "game/jollygood_launch.hpp"
#include "ui/about_dialog.hpp"
#include "ui/logging_dialog.hpp"
#include "ui/settings_dialog.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QDialog>
#include <QFont>
#include <QKeySequence>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace goliath {

void MainWindow::verifyBios() {
    auto resultOpt = verify_bios(m_paths.bios_dir);
    if (!resultOpt.has_value()) {
        QMessageBox::warning(this, "Verify BIOS",
            QString("BIOS folder not found:\n%1").arg(path_to_qstring(m_paths.bios_dir)));
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
                .arg(path_to_qstring(m_paths.bios_dir)));
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
                .arg(path_to_qstring(m_paths.bios_dir))
                .arg(missing.join("\n")));
        return;
    }
}

void MainWindow::openSettings() {
    const int treeVertical = m_tree
        ? m_tree->verticalScrollBar()->value() : 0;
    const int treeHorizontal = m_tree
        ? m_tree->horizontalScrollBar()->value() : 0;

    SettingsDialog dialog(m_config, m_paths,
        [this](const QKeySequence& gif, const QKeySequence& png, QString* error) {
            return applyCaptureHotkeys(gif, png, error);
        }, this);
    dialog.exec();
    // A manual check from Settings may have replaced or cleared the alert.
    reloadCachedUpdateAlert();
    // A newly enabled preference takes effect immediately after Settings
    // closes. Disabled or not-yet-due schedules return without network I/O.
    startAutomaticUpdateCheckIfDue();
    // Path tab edits (e.g. BIOS Folder) must apply to this session too,
    // without waiting for a rescan.
    refreshResolvedPaths();
    loadGameProfiles();
    loadGamePlaytime();
    loadGameLibraryState();
    refreshLibraryView(true);

    const auto restoreTreeViewport =
        [this, treeVertical, treeHorizontal]() {
            if (!m_tree) return;
            m_tree->doItemsLayout();
            QScrollBar* vertical = m_tree->verticalScrollBar();
            QScrollBar* horizontal = m_tree->horizontalScrollBar();
            vertical->setValue(std::clamp(
                treeVertical, vertical->minimum(), vertical->maximum()));
            horizontal->setValue(std::clamp(
                treeHorizontal, horizontal->minimum(), horizontal->maximum()));
        };

    // Re-expanding many parents completes over queued Qt layout passes. Keep
    // the user's viewport independent from selection restoration after each.
    restoreTreeViewport();
    QTimer::singleShot(0, m_tree, [this, restoreTreeViewport]() {
        restoreTreeViewport();
        QTimer::singleShot(0, m_tree, restoreTreeViewport);
    });
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
    AboutDialog dialog(m_config, this);
    dialog.exec();
    // A manual check from About may have replaced or cleared the alert.
    reloadCachedUpdateAlert();
}

} // namespace goliath
