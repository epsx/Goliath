// notification_center_dialog.cpp - themed notification history and actions.
#include "ui/notification_center_dialog.hpp"

#include "ui/widgets/title_bar.hpp"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace goliath {
namespace {

QString categoryLabel(NotificationCategory category) {
    switch (category) {
    case NotificationCategory::Update: return "UPDATE";
    case NotificationCategory::Library: return "LIBRARY";
    case NotificationCategory::Configuration: return "CONFIGURATION";
    case NotificationCategory::Integrity: return "INTEGRITY";
    }
    return "GOLIATH";
}

QString severityKey(NotificationSeverity severity) {
    switch (severity) {
    case NotificationSeverity::Information: return "information";
    case NotificationSeverity::Warning: return "warning";
    case NotificationSeverity::Error: return "error";
    }
    return "information";
}

QString actionLabel(NotificationAction action) {
    switch (action) {
    case NotificationAction::OpenUpdate: return "View Update";
    case NotificationAction::OpenSettings: return "Open Settings";
    case NotificationAction::RescanLibrary: return "Rescan Library";
    case NotificationAction::OpenVerificationReport:
        return "View Verification Report";
    case NotificationAction::VerifyBios: return "Verify BIOS";
    case NotificationAction::OpenLogs: return "Open Logs";
    case NotificationAction::None: break;
    }
    return {};
}

QString timestampText(std::int64_t epoch) {
    const QDateTime timestamp = QDateTime::fromSecsSinceEpoch(epoch).toLocalTime();
    const QDateTime now = QDateTime::currentDateTime();
    if (timestamp.date() == now.date())
        return QString("Today, %1").arg(timestamp.time().toString("HH:mm"));
    if (timestamp.date() == now.date().addDays(-1))
        return QString("Yesterday, %1").arg(timestamp.time().toString("HH:mm"));
    return timestamp.toString("yyyy-MM-dd HH:mm");
}

} // namespace

NotificationCenterDialog::NotificationCenterDialog(NotificationStore& store,
                                                   QWidget* parent)
    : QDialog(parent), m_store(store) {
    setWindowTitle("Notifications");
    setModal(true);
    resize(720, 620);
    setMinimumSize(620, 460);

    QVBoxLayout* content = nullptr;
    setupFramelessDialog(this, windowTitle(), &content, true, true);
    content->setSpacing(12);

    auto* heading = new QLabel("Notification Center");
    heading->setObjectName("notification_heading");
    content->addWidget(heading);

    m_summary = new QLabel();
    m_summary->setObjectName("notification_summary");
    m_summary->setWordWrap(true);
    content->addWidget(m_summary);

    auto* scroll = new QScrollArea();
    scroll->setObjectName("notification_scroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollContents = new QWidget();
    scrollContents->setObjectName("notification_scroll_contents");
    m_cardsLayout = new QVBoxLayout(scrollContents);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(9);
    scroll->setWidget(scrollContents);
    content->addWidget(scroll, 1);

    auto* footer = new QHBoxLayout();
    m_markAllRead = new QPushButton("Mark all read");
    connect(m_markAllRead, &QPushButton::clicked, this, [this]() {
        if (m_store.mark_all_read()) m_changed = true;
        rebuildCards();
    });
    footer->addWidget(m_markAllRead);

    m_clearHistory = new QPushButton("Clear history...");
    connect(m_clearHistory, &QPushButton::clicked, this, [this]() {
        const auto reply = QMessageBox::question(
            this, "Clear Notification History",
            "Remove every notification from this device?",
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (reply != QMessageBox::Yes) return;
        if (m_store.clear()) m_changed = true;
        rebuildCards();
    });
    footer->addWidget(m_clearHistory);
    footer->addStretch();

    auto* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeButton);
    content->addLayout(footer);

    rebuildCards();
}

void NotificationCenterDialog::rebuildCards() {
    while (QLayoutItem* item = m_cardsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (m_store.items().empty()) {
        auto* empty = new QFrame();
        empty->setObjectName("notification_empty");
        auto* layout = new QVBoxLayout(empty);
        layout->setContentsMargins(20, 28, 20, 28);
        auto* title = new QLabel("No notifications", empty);
        title->setObjectName("notification_title");
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);
        auto* detail = new QLabel(
            "Important update, library, configuration and integrity events "
            "will appear here.", empty);
        detail->setObjectName("notification_message");
        detail->setAlignment(Qt::AlignCenter);
        detail->setWordWrap(true);
        layout->addWidget(detail);
        m_cardsLayout->addWidget(empty);
        m_cardsLayout->addStretch();
        updateFooterState();
        return;
    }

    for (const AppNotification& notification : m_store.items()) {
        const std::string id = notification.id;
        auto* card = new QFrame();
        card->setObjectName("notification_card");
        card->setProperty("notificationSeverity",
                          severityKey(notification.severity));
        card->setProperty("notificationUnread", !notification.read);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(7);

        auto* metadata = new QHBoxLayout();
        auto* category = new QLabel(categoryLabel(notification.category), card);
        category->setObjectName("notification_category");
        metadata->addWidget(category);
        if (!notification.read) {
            auto* badge = new QLabel("NEW", card);
            badge->setObjectName("notification_new_badge");
            metadata->addWidget(badge);
        }
        metadata->addStretch();
        auto* timestampLabel = new QLabel(
            timestampText(notification.created_at_epoch), card);
        timestampLabel->setObjectName("notification_time");
        metadata->addWidget(timestampLabel);
        cardLayout->addLayout(metadata);

        auto* title = new QLabel(QString::fromStdString(notification.title), card);
        title->setObjectName("notification_title");
        title->setWordWrap(true);
        cardLayout->addWidget(title);

        if (!notification.message.empty()) {
            auto* message = new QLabel(
                QString::fromStdString(notification.message), card);
            message->setObjectName("notification_message");
            message->setWordWrap(true);
            message->setTextInteractionFlags(Qt::TextSelectableByMouse);
            if (!notification.hover_details.empty()) {
                message->setToolTip(
                    QString::fromStdString(notification.hover_details));
                message->setCursor(Qt::WhatsThisCursor);
                message->setAccessibleDescription(
                    QString::fromStdString(notification.hover_details));
            }
            cardLayout->addWidget(message);
        }

        auto* actions = new QHBoxLayout();
        actions->addStretch();
        if (!notification.read) {
            auto* markRead = new QPushButton("Mark read", card);
            connect(markRead, &QPushButton::clicked, this, [this, id]() {
                if (m_store.mark_read(id)) m_changed = true;
                rebuildCards();
            });
            actions->addWidget(markRead);
        }

        const QString actionText = actionLabel(notification.action);
        if (!actionText.isEmpty()) {
            auto* action = new QPushButton(actionText, card);
            action->setObjectName("notification_primary_action");
            connect(action, &QPushButton::clicked, this,
                    [this, id, selected = notification.action]() {
                        if (m_store.mark_read(id)) m_changed = true;
                        m_selectedAction = selected;
                        accept();
                    });
            actions->addWidget(action);
        }

        auto* dismiss = new QPushButton("Dismiss", card);
        connect(dismiss, &QPushButton::clicked, this, [this, id]() {
            if (m_store.remove(id)) m_changed = true;
            rebuildCards();
        });
        actions->addWidget(dismiss);
        cardLayout->addLayout(actions);
        m_cardsLayout->addWidget(card);
    }
    m_cardsLayout->addStretch();
    updateFooterState();
}

void NotificationCenterDialog::updateFooterState() {
    const std::size_t unread = m_store.unread_count();
    const std::size_t total = m_store.size();
    m_summary->setText(unread == 0
        ? QString("%1 saved notification%2. You are all caught up.")
              .arg(static_cast<qulonglong>(total))
              .arg(total == 1 ? "" : "s")
        : QString("%1 unread notification%2. Only events that may need your "
                  "attention are kept here.")
              .arg(static_cast<qulonglong>(unread))
              .arg(unread == 1 ? "" : "s"));
    m_markAllRead->setEnabled(unread != 0);
    m_clearHistory->setEnabled(total != 0);
}

} // namespace goliath
