#include "controller_button.hpp"

#include <QColor>
#include <QLabel>
#include <QResizeEvent>

namespace goliath {

ControllerButton::ControllerButton(const QString& keyName, const QString& color, bool isRound,
                                     QWidget* parent)
    : QPushButton(parent) {
    m_defaultStyleSheet = buildStyleSheet(color, isRound);
    // Capture mode must retain the complete keycap box model. Replacing this
    // with a partial stylesheet makes compact controls inherit the theme's
    // generic QPushButton padding and visibly resize while listening.
    m_listeningStyleSheet = buildStyleSheet("#3daee9", isRound);
    setStyleSheet(m_defaultStyleSheet);
    setFlat(false);
    setCursor(Qt::PointingHandCursor);

    QColor base(color);
    if (!base.isValid()) base = QColor("#555555");
    const QString foreground = base.lightness() >= 150
        ? QStringLiteral("#171717")
        : QStringLiteral("#ffffff");

    m_nameLabel = new QLabel(keyName, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_nameLabel->setStyleSheet(
        QString("color: %1; background: transparent; font-weight: 700; "
                "font-size: 10px;").arg(foreground));

    m_valueLabel = new QLabel("--", this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setWordWrap(true);
    m_valueLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_valueLabel->setStyleSheet(
        QString("color: %1; background: transparent; font-size: 8px;")
            .arg(foreground));

    layoutLabels();
}

QString ControllerButton::buildStyleSheet(const QString& colorIn, bool isRound) const {
    QColor base(colorIn);
    if (!base.isValid()) base = QColor("#555555");

    const QColor top = base.lighter(122);
    const QColor bottom = base.darker(116);
    const QColor rim = base.darker(205);
    const QColor depth = base.darker(245);
    const QColor hoverTop = base.lighter(138);
    const QColor hoverBottom = base.darker(106);
    const QColor pressedTop = base.darker(108);
    const QColor pressedBottom = base.darker(128);
    const QColor disabledTop = base.lighter(112);
    const QColor disabledBottom = base.darker(108);
    const QString foreground = base.lightness() >= 150
        ? QStringLiteral("#171717")
        : QStringLiteral("#ffffff");
    const QString radius = isRound ? QStringLiteral("999px")
                                   : QStringLiteral("9px");

    return QString(
        "QPushButton {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:0.48 %2, stop:1 %3);"
        " color:%4; border:1px solid %5; border-bottom:4px solid %6;"
        " border-radius:%7; padding:0 1px 3px 1px;"
        "}"
        "QPushButton:hover {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %8, stop:0.48 %1, stop:1 %9);"
        " border:2px solid %10; border-bottom:4px solid %6;"
        "}"
        "QPushButton:focus { border:2px solid %10;"
        " border-bottom:4px solid %6; }"
        "QPushButton:pressed {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %11, stop:1 %12);"
        " border:1px solid %5; border-bottom:1px solid %6;"
        " padding:3px 1px 0 1px;"
        "}"
        "QPushButton:disabled {"
        " background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %13, stop:1 %14);"
        " border:1px solid %5; border-bottom:3px solid %6;"
        " color:%4;"
        "}")
        .arg(top.name(), base.name(), bottom.name(), foreground,
             rim.name(), depth.name(), radius, hoverTop.name(),
             hoverBottom.name(), QColor("#f4f7fb").name(),
             pressedTop.name(), pressedBottom.name(), disabledTop.name(),
             disabledBottom.name());
}

void ControllerButton::layoutLabels() {
    int w = width();
    int h = height();
    m_nameLabel->setGeometry(0, int(h * 0.12), w, int(h * 0.38));
    m_valueLabel->setGeometry(2, int(h * 0.50), w - 4, int(h * 0.40));
}

void ControllerButton::resizeEvent(QResizeEvent* event) {
    QPushButton::resizeEvent(event);
    layoutLabels();
}

void ControllerButton::setValueText(const QString& text) {
    m_rawText = text;
    if (m_valueLabel) m_valueLabel->setText(text);
}

} // namespace goliath
