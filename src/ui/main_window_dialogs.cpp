#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "common/qt_path.hpp"
#include "game/bios_verify.hpp"
#include "game/jollygood_launch.hpp"
#include "ui/about_dialog.hpp"
#include "ui/bios_verification_dialog.hpp"
#include "ui/logging_dialog.hpp"
#include "ui/settings_dialog.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QKeySequence>
#include <QDateTime>
#include <QScrollBar>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace goliath {

void MainWindow::verifyBios() {
    auto resultOpt = verify_bios(m_paths.bios_dir);
    if (!resultOpt.has_value()) {
        addNotification({
            "configuration:bios",
            NotificationCategory::Configuration,
            NotificationSeverity::Warning,
            "BIOS folder not found",
            QString("Configured folder: %1")
                .arg(path_to_qstring(m_paths.bios_dir)).toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            NotificationAction::OpenSettings,
        });
        showThemedMessageDialog(
            this, "Verify BIOS",
            QString("BIOS folder not found:\n%1")
                .arg(path_to_qstring(m_paths.bios_dir)),
            ThemedMessageTone::Warning);
        return;
    }

    if (resultOpt->problems == 0) {
        removeNotification("configuration:bios");
    } else {
        addNotification({
            "configuration:bios",
            NotificationCategory::Configuration,
            NotificationSeverity::Warning,
            "BIOS verification found problems",
            QString("%1 required BIOS problem%2 found. Open Verify BIOS for "
                    "the complete per-archive report.")
                .arg(resultOpt->problems)
                .arg(resultOpt->problems == 1 ? "" : "s")
                .toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            NotificationAction::VerifyBios,
        });
    }
    BiosVerificationDialog dialog(*resultOpt, m_paths.bios_dir, this);
    dialog.exec();
}

void MainWindow::warnIfBiosMissing() {
    std::error_code ec;

    if (!fs::is_directory(m_paths.bios_dir, ec)) {
        addNotification({
            "configuration:bios",
            NotificationCategory::Configuration,
            NotificationSeverity::Warning,
            "BIOS folder not found",
            QString("Configured folder: %1")
                .arg(path_to_qstring(m_paths.bios_dir)).toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            NotificationAction::OpenSettings,
        });
        showThemedMessageDialog(this, "BIOS folder not found",
            QString("The configured BIOS folder does not exist:\n%1\n\n"
                    "Set it in Settings > Path, or run Tools > Verify BIOS.")
                .arg(path_to_qstring(m_paths.bios_dir)),
            ThemedMessageTone::Warning);
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
        addNotification({
            "configuration:bios",
            NotificationCategory::Configuration,
            NotificationSeverity::Warning,
            "BIOS archives are missing",
            QString("%1 archive%2 missing. Run Verify BIOS for details.")
                .arg(missing.size())
                .arg(missing.size() == 1 ? " is" : "s are")
                .toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            NotificationAction::VerifyBios,
        });
        showThemedMessageDialog(this, "Missing BIOS files",
            QString("The following BIOS archives are missing from:\n%1\n\n%2\n\n"
                    "Games may fail to launch. Run Tools > Verify BIOS for details.")
                .arg(path_to_qstring(m_paths.bios_dir))
                .arg(missing.join("\n")),
            ThemedMessageTone::Warning);
        return;
    }
    removeNotification("configuration:bios");
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
