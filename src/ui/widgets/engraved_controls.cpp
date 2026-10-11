#include "ui/widgets/engraved_controls.hpp"

#include <QBrush>
#include <QColor>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>

#include <algorithm>
#include <cmath>

namespace goliath {

EngravedTextLabel::EngravedTextLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent) {
    setProperty("goliathEngravedSurface", true);
}

void EngravedTextLabel::paintEvent(QPaintEvent*) {
    QColor textColor(property("goliathEngravedText").toString());
    QColor depthColor(property("goliathEngravedDepth").toString());
    if (!textColor.isValid()) textColor = palette().windowText().color();
    if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
    if (!isEnabled()) textColor.setAlpha(120);

    QRect drawRect = contentsRect().adjusted(margin(), margin(),
                                              -margin(), -margin());
    int flags = static_cast<int>(alignment().toInt());
    if (wordWrap()) flags |= Qt::TextWordWrap;

    QPainter painter(this);
    painter.setFont(font());
    painter.setPen(depthColor);
    painter.drawText(drawRect.translated(1, 1), flags, text());
    painter.setPen(textColor);
    painter.drawText(drawRect, flags, text());
}

EngravedComboBox::EngravedComboBox(QWidget* parent)
    : QComboBox(parent) {
    setProperty("goliathEngravedSurface", true);
}

void EngravedComboBox::paintEvent(QPaintEvent*) {
    QStyleOptionComboBox option;
    initStyleOption(&option);
    const QString displayText = option.currentText;
    option.currentText.clear();
    option.currentIcon = QIcon();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    style()->drawComplexControl(QStyle::CC_ComboBox, &option, &painter,
                                this);

    QColor textColor(property("goliathEngravedText").toString());
    QColor depthColor(property("goliathEngravedDepth").toString());
    QColor hoverColor(property("goliathEngravedHover").toString());
    if (!textColor.isValid()) textColor = palette().buttonText().color();
    if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
    if (!hoverColor.isValid()) hoverColor = textColor;
    if (option.state.testFlag(QStyle::State_MouseOver) && isEnabled())
        textColor = hoverColor;
    if (!isEnabled()) textColor.setAlpha(120);

    QRect textRect = style()->subControlRect(
        QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
    const QString elided = fontMetrics().elidedText(
        displayText, Qt::ElideRight, textRect.width());
    constexpr int flags = Qt::AlignLeft | Qt::AlignVCenter |
                          Qt::TextSingleLine;
    painter.setFont(font());
    painter.setPen(depthColor);
    painter.drawText(textRect.translated(1, 1), flags, elided);
    painter.setPen(textColor);
    painter.drawText(textRect, flags, elided);
}

EngravedCheckBox::EngravedCheckBox(const QString& label, QWidget* parent)
    : QCheckBox(label, parent) {
    setProperty("goliathEngravedSurface", true);
}

void EngravedCheckBox::paintEvent(QPaintEvent*) {
    QStyleOptionButton option;
    initStyleOption(&option);

    QColor textColor(property("goliathEngravedText").toString());
    QColor depthColor(property("goliathEngravedDepth").toString());
    QColor hoverColor(property("goliathEngravedHover").toString());
    if (!textColor.isValid()) textColor = palette().windowText().color();
    if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
    if (!hoverColor.isValid()) hoverColor = textColor;
    if (option.state.testFlag(QStyle::State_MouseOver) && isEnabled())
        textColor = hoverColor;
    if (!isEnabled()) textColor.setAlpha(120);

    QRect indicator = style()->subElementRect(
        QStyle::SE_CheckBoxIndicator, &option, this);
    const QRect textRect = style()->subElementRect(
        QStyle::SE_CheckBoxContents, &option, this);
    indicator = indicator.adjusted(1, 1, -1, -1);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(depthColor, 1.4));
    painter.drawRoundedRect(indicator.translated(1, 1), 3.0, 3.0);
    painter.setPen(QPen(textColor, 1.4));
    painter.drawRoundedRect(indicator, 3.0, 3.0);

    if (option.state.testFlag(QStyle::State_On)) {
        QPainterPath check;
        check.moveTo(indicator.left() + indicator.width() * 0.20,
                     indicator.center().y());
        check.lineTo(indicator.left() + indicator.width() * 0.43,
                     indicator.bottom() - indicator.height() * 0.22);
        check.lineTo(indicator.right() - indicator.width() * 0.16,
                     indicator.top() + indicator.height() * 0.20);
        painter.setPen(QPen(depthColor, 2.0, Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(check.translated(1.0, 1.0));
        painter.setPen(QPen(hoverColor, 2.0, Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(check);
    }

    const int baseline = textRect.center().y() +
        (fontMetrics().ascent() - fontMetrics().descent()) / 2;
    const QPoint textPoint(textRect.left(), baseline);
    painter.setFont(font());
    painter.setPen(depthColor);
    painter.drawText(textPoint + QPoint(1, 1), text());
    painter.setPen(textColor);
    painter.drawText(textPoint, text());
}

EngravedTextButton::EngravedTextButton(const QString& label,
                                       QWidget* parent)
    : QPushButton(label, parent) {
    setProperty("goliathEngravedSurface", true);
}

void EngravedTextButton::paintEvent(QPaintEvent*) {
    QStyleOptionButton option;
    initStyleOption(&option);
    const QString displayText = option.text;
    option.text.clear();
    option.icon = QIcon();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

    QColor textColor(property("goliathEngravedText").toString());
    QColor depthColor(property("goliathEngravedDepth").toString());
    QColor accentColor(property("goliathEngravedHover").toString());
    if (!textColor.isValid()) textColor = palette().buttonText().color();
    if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
    if (!accentColor.isValid()) accentColor = textColor;

    const bool accented = isChecked() || property("rated").toBool() ||
        option.state.testFlag(QStyle::State_MouseOver);
    if (accented && isEnabled()) textColor = accentColor;
    if (!isEnabled()) textColor.setAlpha(120);

    const QRect content = style()->subElementRect(
        QStyle::SE_PushButtonContents, &option, this);
    const int pressedOffset = option.state.testFlag(QStyle::State_Sunken)
        ? 1 : 0;
    constexpr int flags = Qt::AlignCenter | Qt::TextSingleLine;

    painter.setFont(font());
    if (property("goliathCircledStar").toBool()) {
        constexpr int iconSize = 17;
        constexpr int gap = 6;
        const int textWidth = fontMetrics().horizontalAdvance(displayText);
        const int totalWidth = iconSize + gap + textWidth;
        const int startX = content.center().x() - totalWidth / 2 +
            pressedOffset;
        const int iconY = content.center().y() - iconSize / 2 +
            pressedOffset;
        const QRectF iconRect(startX, iconY, iconSize, iconSize);
        const int baseline = content.center().y() +
            (fontMetrics().ascent() - fontMetrics().descent()) / 2 +
            pressedOffset;
        const QPoint textPoint(startX + iconSize + gap, baseline);

        drawCircledStar(painter, iconRect.translated(1.0, 1.0),
                        depthColor, isChecked());
        painter.setPen(depthColor);
        painter.drawText(textPoint + QPoint(1, 1), displayText);
        drawCircledStar(painter, iconRect, textColor, isChecked());
        painter.setPen(textColor);
        painter.drawText(textPoint, displayText);
        return;
    }

    const QRect textRect = content.translated(pressedOffset, pressedOffset);
    painter.setPen(depthColor);
    painter.drawText(textRect.translated(1, 1), flags, displayText);
    painter.setPen(textColor);
    painter.drawText(textRect, flags, displayText);
}

void EngravedTextButton::drawCircledStar(
        QPainter& painter, const QRectF& bounds,
        const QColor& color, bool filled) {
    painter.setPen(QPen(color, 1.25, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(bounds.adjusted(1.0, 1.0, -1.0, -1.0));

    QPainterPath star;
    const QPointF centre = bounds.center();
    constexpr int points = 10;
    constexpr qreal pi = 3.14159265358979323846;
    for (int index = 0; index < points; ++index) {
        const qreal radius = index % 2 == 0 ? 5.1 : 2.25;
        const qreal angle = -pi / 2.0 + index * pi / 5.0;
        const QPointF point(centre.x() + std::cos(angle) * radius,
                            centre.y() + std::sin(angle) * radius);
        if (index == 0) star.moveTo(point);
        else star.lineTo(point);
    }
    star.closeSubpath();
    painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));
    painter.drawPath(star);
    painter.setBrush(Qt::NoBrush);
}

EngravedActionButton::EngravedActionButton(
        const QString& label, Glyph glyph, QWidget* parent)
    : QPushButton(label, parent), m_glyph(glyph) {
    setProperty("goliathEngravedAction", true);
    setProperty("goliathEngravedSurface", true);
}

QSize EngravedActionButton::sizeHint() const {
    const QFontMetrics metrics(font());
    return QSize(metrics.horizontalAdvance(text()) + 48,
                 std::max(30, metrics.height() + 12));
}

void EngravedActionButton::paintEvent(QPaintEvent*) {
    QStyleOptionButton option;
    initStyleOption(&option);
    option.features.setFlag(QStyleOptionButton::HasMenu, false);
    option.text.clear();
    option.icon = QIcon();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

    QColor textColor(property("goliathEngravedText").toString());
    QColor depthColor(property("goliathEngravedDepth").toString());
    QColor hoverColor(property("goliathEngravedHover").toString());
    if (!textColor.isValid()) textColor = palette().buttonText().color();
    if (!depthColor.isValid()) depthColor = QColor(255, 255, 255, 120);
    if (!hoverColor.isValid()) hoverColor = textColor;
    if (option.state.testFlag(QStyle::State_MouseOver) && isEnabled())
        textColor = hoverColor;
    if (!isEnabled()) textColor.setAlpha(120);

    const QRect content = style()->subElementRect(
        QStyle::SE_PushButtonContents, &option, this);
    const QFontMetrics metrics(font());
    constexpr int iconSize = 15;
    constexpr int gap = 6;
    const int textWidth = metrics.horizontalAdvance(text());
    const int totalWidth = iconSize + gap + textWidth;
    const int pressedOffset = option.state.testFlag(QStyle::State_Sunken)
        ? 1 : 0;
    const int startX = content.center().x() - totalWidth / 2 + pressedOffset;
    const int iconY = content.center().y() - iconSize / 2 + pressedOffset;
    const QRectF iconRect(startX, iconY, iconSize, iconSize);
    const int baseline = content.center().y() +
        (metrics.ascent() - metrics.descent()) / 2 + pressedOffset;
    const QPoint textPoint(startX + iconSize + gap, baseline);

    drawGlyph(painter, iconRect.translated(1.0, 1.0), depthColor);
    painter.setPen(depthColor);
    painter.drawText(textPoint + QPoint(1, 1), text());

    drawGlyph(painter, iconRect, textColor);
    painter.setPen(textColor);
    painter.drawText(textPoint, text());
}

void EngravedActionButton::drawGlyph(
        QPainter& painter, const QRectF& bounds,
        const QColor& color) const {
    QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QPointF centre = bounds.center();

    if (m_glyph == Glyph::Tools) {
        painter.drawLine(
            QPointF(bounds.left() + 3.0, bounds.bottom() - 2.5),
            QPointF(bounds.right() - 3.5, bounds.top() + 3.0));
        painter.drawEllipse(QPointF(bounds.left() + 2.8,
                                    bounds.bottom() - 2.8), 1.8, 1.8);
        QPainterPath jaw;
        jaw.moveTo(bounds.right() - 6.0, bounds.top() + 2.0);
        jaw.lineTo(bounds.right() - 3.0, bounds.top() + 4.8);
        jaw.lineTo(bounds.right() - 1.3, bounds.top() + 1.2);
        painter.drawPath(jaw);
        painter.drawLine(
            QPointF(bounds.left() + 3.0, bounds.top() + 2.0),
            QPointF(bounds.right() - 2.0, bounds.bottom() - 3.0));
        painter.drawLine(
            QPointF(bounds.left() + 1.8, bounds.top() + 1.2),
            QPointF(bounds.left() + 4.8, bounds.top() + 4.5));
    } else if (m_glyph == Glyph::Settings) {
        painter.drawEllipse(centre, 3.0, 3.0);
        painter.drawEllipse(centre, 1.0, 1.0);
        for (int index = 0; index < 8; ++index) {
            const qreal angle = index * 3.14159265358979323846 / 4.0;
            const QPointF inner(centre.x() + std::cos(angle) * 4.2,
                                centre.y() + std::sin(angle) * 4.2);
            const QPointF outer(centre.x() + std::cos(angle) * 6.2,
                                centre.y() + std::sin(angle) * 6.2);
            painter.drawLine(inner, outer);
        }
    } else if (m_glyph == Glyph::About) {
        painter.drawEllipse(bounds.adjusted(1.5, 1.5, -1.5, -1.5));
        painter.drawPoint(QPointF(centre.x(), bounds.top() + 4.2));
        painter.drawLine(QPointF(centre.x(), bounds.top() + 6.7),
                         QPointF(centre.x(), bounds.bottom() - 3.0));
    } else if (m_glyph == Glyph::Filters) {
        QPainterPath funnel;
        funnel.moveTo(bounds.left() + 1.5, bounds.top() + 2.0);
        funnel.lineTo(bounds.right() - 1.5, bounds.top() + 2.0);
        funnel.lineTo(centre.x() + 2.0, centre.y() + 1.0);
        funnel.lineTo(centre.x() + 2.0, bounds.bottom() - 2.0);
        funnel.lineTo(centre.x() - 1.5, bounds.bottom() - 3.5);
        funnel.lineTo(centre.x() - 1.5, centre.y() + 1.0);
        funnel.closeSubpath();
        painter.drawPath(funnel);
    } else {
        const QRectF die = bounds.adjusted(1.8, 1.8, -1.8, -1.8);
        painter.drawRoundedRect(die, 2.2, 2.2);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(die.left() + 2.6,
                                    die.top() + 2.6), 0.9, 0.9);
        painter.drawEllipse(centre, 0.9, 0.9);
        painter.drawEllipse(QPointF(die.right() - 2.6,
                                    die.bottom() - 2.6), 0.9, 0.9);
        painter.setBrush(Qt::NoBrush);
    }
}

} // namespace goliath
