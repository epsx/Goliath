#pragma once

#include <QLabel>
#include <QString>
#include <QWidget>

#include <functional>

class QMouseEvent;
class QPaintEvent;

namespace goliath {

class HatchedBackgroundWidget final : public QWidget {
public:
    explicit HatchedBackgroundWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

class VerificationDetailsLabel final : public QLabel {
public:
    explicit VerificationDetailsLabel(std::function<void()> activate,
                                      QWidget* parent = nullptr);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    std::function<void()> m_activate;
};

class CatalogLinkLabel final : public QLabel {
public:
    explicit CatalogLinkLabel(
        std::function<void(const QString&)> activate,
        QWidget* parent = nullptr);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    std::function<void(const QString&)> m_activate;
};

void addPanelElevation(QWidget* panel);

} // namespace goliath
