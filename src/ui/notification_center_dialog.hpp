// notification_center_dialog.hpp - compact history of actionable app events.
#pragma once

#include "common/notification_store.hpp"

#include <QDialog>

#include <optional>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace goliath {

class NotificationCenterDialog final : public QDialog {
public:
    explicit NotificationCenterDialog(NotificationStore& store,
                                      QWidget* parent = nullptr);

    [[nodiscard]] bool changed() const { return m_changed; }
    [[nodiscard]] std::optional<NotificationAction> selectedAction() const {
        return m_selectedAction;
    }

private:
    void rebuildCards();
    void updateFooterState();

    NotificationStore& m_store;
    QVBoxLayout* m_cardsLayout = nullptr;
    QLabel* m_summary = nullptr;
    QPushButton* m_markAllRead = nullptr;
    QPushButton* m_clearHistory = nullptr;
    bool m_changed = false;
    std::optional<NotificationAction> m_selectedAction;
};

} // namespace goliath
