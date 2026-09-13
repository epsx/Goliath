#include "controller_button.hpp"

#include <QLabel>
#include <QResizeEvent>

namespace goliath {

ControllerButton::ControllerButton(const QString& keyName, const QString& color, bool isRound,
                                     QWidget* parent)
    : QPushButton(parent) {
    m_defaultStyleSheet = buildStyleSheet(color, isRound);
    setStyleSheet(m_defaultStyleSheet);
    setFlat(true);

    m_nameLabel = new QLabel(keyName, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setStyleSheet("color: white; background: transparent; font-weight: bold; font-size: 10px;");

    m_valueLabel = new QLabel("--", this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setWordWrap(true);
    m_valueLabel->setStyleSheet("color: white; background: transparent; font-size: 8px;");

    layoutLabels();
}

QString ControllerButton::buildStyleSheet(const QString& colorIn, bool isRound) const {
    QString color = colorIn.isEmpty() ? "#555555" : colorIn;
    QString radius = isRound ? "999px" : "8px";
    return QString("QPushButton { background-color: %1; color: white; "
                    "border: 2px solid #222222; border-radius: %2; }"
                    "QPushButton:hover { border: 2px solid #ffffff; }")
        .arg(color, radius);
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
