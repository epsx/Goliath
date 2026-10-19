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
    if (!depthColor.isValid()) {
        depthColor = palette().button().color().lightness() < 128
            ? QColor(0, 0, 0, 190)
            : QColor(255, 255, 255, 210);
    }
    if (!hoverColor.isValid()) hoverColor = palette().highlight().color();
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
    } else if (m_glyph == Glyph::Random) {
        const QRectF die = bounds.adjusted(1.8, 1.8, -1.8, -1.8);
        painter.drawRoundedRect(die, 2.2, 2.2);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(die.left() + 2.6,
                                    die.top() + 2.6), 0.9, 0.9);
        painter.drawEllipse(centre, 0.9, 0.9);
        painter.drawEllipse(QPointF(die.right() - 2.6,
                                    die.bottom() - 2.6), 0.9, 0.9);
        painter.setBrush(Qt::NoBrush);
    } else if (m_glyph == Glyph::Alerts) {
        QPainterPath bell;
        bell.moveTo(centre.x(), bounds.top() + 1.5);
        bell.cubicTo(bounds.left() + 3.0, bounds.top() + 2.0,
                     bounds.left() + 3.0, centre.y() + 1.0,
                     bounds.left() + 2.0, bounds.bottom() - 3.2);
        bell.lineTo(bounds.right() - 2.0, bounds.bottom() - 3.2);
        bell.cubicTo(bounds.right() - 3.0, centre.y() + 1.0,
                     bounds.right() - 3.0, bounds.top() + 2.0,
                     centre.x(), bounds.top() + 1.5);
        painter.drawPath(bell);
        painter.drawLine(QPointF(bounds.left() + 1.3, bounds.bottom() - 3.0),
                         QPointF(bounds.right() - 1.3, bounds.bottom() - 3.0));
        painter.drawArc(QRectF(centre.x() - 1.8, bounds.bottom() - 4.0,
                               3.6, 3.6), 180 * 16, 180 * 16);
    } else if (m_glyph == Glyph::Save) {
        const QRectF body = bounds.adjusted(1.5, 1.2, -1.5, -1.2);
        painter.drawRoundedRect(body, 1.4, 1.4);
        painter.drawRect(QRectF(body.left() + 2.2, body.top(),
                                body.width() - 4.4, body.height() * 0.36));
        painter.drawRoundedRect(
            QRectF(body.left() + 2.4, body.center().y() + 1.0,
                   body.width() - 4.8, body.height() * 0.30),
            0.8, 0.8);
        painter.drawLine(QPointF(body.right() - 3.4, body.top() + 0.8),
                         QPointF(body.right() - 3.4,
                                 body.top() + body.height() * 0.30));
    } else if (m_glyph == Glyph::Reset ||
               m_glyph == Glyph::Refresh) {
        const QRectF arc = bounds.adjusted(2.0, 2.0, -2.0, -2.0);
        painter.drawArc(arc, 35 * 16, 285 * 16);
        QPainterPath arrow;
        if (m_glyph == Glyph::Reset) {
            arrow.moveTo(bounds.left() + 1.3, bounds.top() + 5.0);
            arrow.lineTo(bounds.left() + 5.2, bounds.top() + 4.4);
            arrow.lineTo(bounds.left() + 3.7, bounds.top() + 8.0);
        } else {
            arrow.moveTo(bounds.right() - 1.3, bounds.bottom() - 5.0);
            arrow.lineTo(bounds.right() - 5.2, bounds.bottom() - 4.4);
            arrow.lineTo(bounds.right() - 3.7, bounds.bottom() - 8.0);
        }
        painter.drawPath(arrow);
    } else if (m_glyph == Glyph::Browse) {
        QPainterPath folder;
        folder.moveTo(bounds.left() + 1.2, bounds.top() + 4.3);
        folder.lineTo(bounds.left() + 5.8, bounds.top() + 4.3);
        folder.lineTo(bounds.left() + 7.4, bounds.top() + 2.3);
        folder.lineTo(bounds.right() - 1.2, bounds.top() + 2.3);
        folder.lineTo(bounds.right() - 1.2, bounds.bottom() - 1.8);
        folder.lineTo(bounds.left() + 1.2, bounds.bottom() - 1.8);
        folder.closeSubpath();
        painter.drawPath(folder);
        painter.drawLine(QPointF(bounds.left() + 1.7, bounds.top() + 6.0),
                         QPointF(bounds.right() - 1.7, bounds.top() + 6.0));
    } else if (m_glyph == Glyph::Help) {
        painter.drawEllipse(bounds.adjusted(1.5, 1.5, -1.5, -1.5));
        QFont glyphFont = painter.font();
        glyphFont.setBold(true);
        glyphFont.setPixelSize(11);
        painter.setFont(glyphFont);
        painter.drawText(bounds.adjusted(0.0, -0.5, 0.0, 0.0),
                         Qt::AlignCenter, QStringLiteral("?"));
    } else if (m_glyph == Glyph::Profiles) {
        const QRectF pad = bounds.adjusted(1.0, 3.0, -1.0, -2.0);
        QPainterPath body;
        body.moveTo(pad.left() + 3.0, pad.top());
        body.lineTo(pad.right() - 3.0, pad.top());
        body.cubicTo(pad.right(), pad.top(), pad.right(), pad.bottom(),
                     pad.right() - 2.0, pad.bottom());
        body.lineTo(pad.center().x() + 2.0, pad.bottom() - 2.0);
        body.lineTo(pad.center().x() - 2.0, pad.bottom() - 2.0);
        body.lineTo(pad.left() + 2.0, pad.bottom());
        body.cubicTo(pad.left(), pad.bottom(), pad.left(), pad.top(),
                     pad.left() + 3.0, pad.top());
        body.closeSubpath();
        painter.drawPath(body);
        painter.drawLine(QPointF(pad.left() + 3.0, pad.center().y()),
                         QPointF(pad.left() + 7.0, pad.center().y()));
        painter.drawLine(QPointF(pad.left() + 5.0, pad.center().y() - 2.0),
                         QPointF(pad.left() + 5.0, pad.center().y() + 2.0));
        painter.drawPoint(QPointF(pad.right() - 4.2, pad.center().y() - 1.3));
        painter.drawPoint(QPointF(pad.right() - 2.4, pad.center().y() + 1.3));
    } else if (m_glyph == Glyph::Import ||
               m_glyph == Glyph::Export) {
        const bool importing = m_glyph == Glyph::Import;
        const qreal stemX = centre.x();
        const qreal stemTop = bounds.top() + 1.5;
        const qreal stemBottom = bounds.bottom() - 4.8;
        painter.drawLine(QPointF(stemX, stemTop),
                         QPointF(stemX, stemBottom));
        QPainterPath arrow;
        const qreal tipY = importing ? stemBottom : stemTop;
        const qreal wingY = importing ? stemBottom - 3.0 : stemTop + 3.0;
        arrow.moveTo(stemX - 3.0, wingY);
        arrow.lineTo(stemX, tipY);
        arrow.lineTo(stemX + 3.0, wingY);
        painter.drawPath(arrow);
        painter.drawLine(QPointF(bounds.left() + 2.0, bounds.bottom() - 2.0),
                         QPointF(bounds.right() - 2.0, bounds.bottom() - 2.0));
        painter.drawLine(QPointF(bounds.left() + 2.0, bounds.bottom() - 2.0),
                         QPointF(bounds.left() + 2.0, bounds.bottom() - 5.0));
        painter.drawLine(QPointF(bounds.right() - 2.0, bounds.bottom() - 2.0),
                         QPointF(bounds.right() - 2.0, bounds.bottom() - 5.0));
    } else if (m_glyph == Glyph::Cancel) {
        painter.drawLine(bounds.topLeft() + QPointF(2.2, 2.2),
                         bounds.bottomRight() - QPointF(2.2, 2.2));
        painter.drawLine(QPointF(bounds.right() - 2.2,
                                 bounds.top() + 2.2),
                         QPointF(bounds.left() + 2.2,
                                 bounds.bottom() - 2.2));
    } else {
        const QRectF lens(bounds.left() + 1.5, bounds.top() + 1.5,
                          bounds.width() * 0.62, bounds.height() * 0.62);
        painter.drawEllipse(lens);
        painter.drawLine(lens.bottomRight() - QPointF(1.0, 1.0),
                         bounds.bottomRight() - QPointF(1.4, 1.4));
    }
}

} // namespace goliath
