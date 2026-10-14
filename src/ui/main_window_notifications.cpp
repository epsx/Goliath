// main_window_notifications.cpp - notification center persistence and routing.
#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "common/notification_store.hpp"
#include "ui/notification_center_dialog.hpp"

#include <QPushButton>
#include <QStyle>

#include <utility>

namespace goliath {

void MainWindow::loadNotifications() {
    NotificationStore loaded;
    std::string error;
    if (!loaded.load(notification_store_path(), &error)) {
        m_notificationPersistenceAvailable = false;
        DebugLogger::logError(
            QString("could not load notification history: %1")
                .arg(QString::fromStdString(error)));
        return;
    }
    m_notifications = std::move(loaded);
    m_notificationPersistenceAvailable = true;
    DebugLogger::logInfo(
        QString("loaded %1 notification(s), %2 unread")
            .arg(static_cast<qulonglong>(m_notifications.size()))
            .arg(static_cast<qulonglong>(m_notifications.unread_count())));
}

void MainWindow::saveNotifications() {
    if (!m_notificationPersistenceAvailable) return;
    std::string error;
    if (m_notifications.save(notification_store_path(), &error)) return;
    m_notificationPersistenceAvailable = false;
    DebugLogger::logError(
        QString("could not save notification history: %1")
            .arg(QString::fromStdString(error)));
}

void MainWindow::addNotification(AppNotification notification) {
    if (!m_notifications.upsert(std::move(notification))) return;
    saveNotifications();
    updateNotificationsButton();
}

void MainWindow::removeNotification(const std::string& id) {
    if (!m_notifications.remove(id)) return;
    saveNotifications();
    updateNotificationsButton();
}

void MainWindow::updateNotificationsButton() {
    if (!m_notificationsButton) return;
    const std::size_t unread = m_notifications.unread_count();
    m_notificationsButton->setText(unread == 0
        ? QStringLiteral("Alerts")
        : QString("Alerts (%1)").arg(static_cast<qulonglong>(unread)));
    m_notificationsButton->setAccessibleName(unread == 0
        ? QStringLiteral("Notifications, no unread items")
        : QString("Notifications, %1 unread").arg(
              static_cast<qulonglong>(unread)));
    m_notificationsButton->setToolTip(unread == 0
        ? QStringLiteral("Open notification history")
        : QString("Open notification history — %1 unread").arg(
              static_cast<qulonglong>(unread)));
    m_notificationsButton->setProperty("hasUnread", unread != 0);
    m_notificationsButton->style()->unpolish(m_notificationsButton);
    m_notificationsButton->style()->polish(m_notificationsButton);
    m_notificationsButton->updateGeometry();
}

void MainWindow::openNotificationCenter() {
    NotificationCenterDialog dialog(m_notifications, this);
    dialog.exec();
    if (dialog.changed()) saveNotifications();
    updateNotificationsButton();
    if (const auto action = dialog.selectedAction())
        performNotificationAction(*action);
}

void MainWindow::performNotificationAction(NotificationAction action) {
    switch (action) {
    case NotificationAction::OpenUpdate:
        openAvailableUpdate();
        break;
    case NotificationAction::OpenSettings:
        openSettings();
        break;
    case NotificationAction::RescanLibrary:
        rescanRoms();
        break;
    case NotificationAction::VerifyBios:
        verifyBios();
        break;
    case NotificationAction::OpenLogs:
        openLogging();
        break;
    case NotificationAction::None:
        break;
    }
}

} // namespace goliath
