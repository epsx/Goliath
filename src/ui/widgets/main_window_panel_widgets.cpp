#include "ui/widgets/main_window_panel_widgets.hpp"

#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace goliath {

HatchedBackgroundWidget::HatchedBackgroundWidget(QWidget* parent)
    : QWidget(parent) {
    setProperty("goliathHatchedBackground", true);
}

void HatchedBackgroundWidget::paintEvent(QPaintEvent*) {
    QStyleOption option;
    option.initFrom(this);

    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);

    const QColor hatchColor(property("goliathHatchColor").toString());
    if (!hatchColor.isValid()) return;

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(hatchColor, 1.0));
    constexpr int spacing = 14;
    const QPoint windowOrigin = mapTo(window(), QPoint(0, 0));
    const int originSum = windowOrigin.x() + windowOrigin.y();
    const int firstSum = ((-originSum % spacing) + spacing) % spacing;
    for (int sum = firstSum; sum <= width() + height(); sum += spacing) {
        const int startX = std::max(0, sum - height());
        const int endX = std::min(width(), sum);
        painter.drawLine(startX, sum - startX, endX, sum - endX);
    }
}

VerificationDetailsLabel::VerificationDetailsLabel(
        std::function<void()> activate, QWidget* parent)
    : QLabel(parent), m_activate(std::move(activate)) {}

void VerificationDetailsLabel::mouseReleaseEvent(QMouseEvent* event) {
    const bool activate =
        event->button() == Qt::LeftButton &&
        rect().contains(event->pos()) &&
        property("verificationDetailsAvailable").toBool();
    if (!activate) {
        QLabel::mouseReleaseEvent(event);
        return;
    }

    event->accept();
    QTimer::singleShot(0, this, [this]() {
        if (m_activate) m_activate();
    });
}

CatalogLinkLabel::CatalogLinkLabel(
        std::function<void(const QString&)> activate, QWidget* parent)
    : QLabel(parent), m_activate(std::move(activate)) {}

void CatalogLinkLabel::mouseReleaseEvent(QMouseEvent* event) {
    const QString url = property("externalUrl").toString();
    const bool activate = event->button() == Qt::LeftButton &&
                          rect().contains(event->pos()) &&
                          !url.isEmpty();
    if (!activate) {
        QLabel::mouseReleaseEvent(event);
        return;
    }

    event->accept();
    QTimer::singleShot(0, this, [this, url]() {
        if (m_activate) m_activate(url);
    });
}

void addPanelElevation(QWidget* panel) {
    panel->setProperty("goliathElevatedPanel", true);

    auto* shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(14.0);
    shadow->setOffset(0.0, 0.0);
    shadow->setColor(QColor(0, 0, 0, 72));
    panel->setGraphicsEffect(shadow);
}

} // namespace goliath
