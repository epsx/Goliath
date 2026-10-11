#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "ui/update_check.hpp"
#include "update/update_alert.hpp"
#include "update/update_schedule.hpp"

#include <QDateTime>
#include <QPushButton>
#include <QString>

#include <string>
#include <utility>

#ifndef GOLIATH_VERSION
#define GOLIATH_VERSION "development"
#endif

namespace goliath {

void MainWindow::startAutomaticUpdateCheckIfDue() {
    if (m_automaticUpdateCheckPending) return;

    const UpdateCheckInterval interval = parse_update_check_interval(
        m_config.get("Updates", "check_interval", "disabled"));
    const auto lastSuccessfulCheck = parse_update_check_epoch(m_config.get(
        "Updates", "last_successful_check_epoch", "0"));
    if (!automatic_update_check_due(
            interval, lastSuccessfulCheck,
            QDateTime::currentSecsSinceEpoch())) {
        return;
    }

    m_automaticUpdateCheckPending = true;
    DebugLogger::logInfo("starting automatic update check");
    checkForUpdatesAutomatically(this, [this](UpdateCheckResult result) {
        m_automaticUpdateCheckPending = false;
        if (!result.successful) return;

        recordSuccessfulUpdateCheck(m_config, result);
        m_availableUpdate = std::move(result.available_release);
        updateAvailableIndicator();
    });
}

void MainWindow::reloadCachedUpdateAlert() {
    std::string error;
    auto cached = load_update_alert_cache(update_alert_cache_path(), &error);
    if (!error.empty()) {
        DebugLogger::logWarn(
            QString("could not restore persistent release alert: %1")
                .arg(QString::fromStdString(error)));
        std::string clearError;
        if (!clear_update_alert_cache(update_alert_cache_path(), &clearError)) {
            DebugLogger::logWarn(
                QString("could not clear invalid release alert: %1")
                    .arg(QString::fromStdString(clearError)));
        }
        cached.reset();
    }

    if (cached &&
        !release_is_newer_than_installed(*cached, GOLIATH_VERSION)) {
        std::string clearError;
        if (!clear_update_alert_cache(update_alert_cache_path(), &clearError)) {
            DebugLogger::logWarn(
                QString("could not clear expired release alert: %1")
                    .arg(QString::fromStdString(clearError)));
        }
        cached.reset();
    }

    m_availableUpdate = std::move(cached);
    updateAvailableIndicator();
}

void MainWindow::updateAvailableIndicator() {
    if (!m_updateAvailableButton) return;
    if (!m_availableUpdate) {
        m_updateAvailableButton->hide();
        return;
    }

    const ReleaseVersion& version = m_availableUpdate->version;
    const QString versionText = QString("%1.%2.%3")
        .arg(version.major)
        .arg(version.minor)
        .arg(version.patch);
    const QString fullText = QString("Update available: %1").arg(versionText);
    m_updateAvailableButton->setText(
        width() < 1050 ? QString("Update: %1").arg(versionText) : fullText);
    m_updateAvailableButton->setAccessibleName(fullText);
    m_updateAvailableButton->setToolTip(
        QString("%1\n%2\nClick to read the release notes and open the "
                "download page.")
            .arg(fullText,
                 QString::fromStdString(m_availableUpdate->name)));
    m_updateAvailableButton->show();
}

void MainWindow::openAvailableUpdate() {
    if (m_availableUpdate) {
        showUpdateRelease(this, *m_availableUpdate);
    }
}

} // namespace goliath
