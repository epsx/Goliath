// controller_button.hpp — a colored (round or square) button showing a key
// name and a separately-settable "mapped value" line, used by the visual
// input panels (ControllerInputWidget, MahjongInputWidget, etc.)
//
// NOTE: QAbstractButton::setText()/text() are NOT virtual in Qt, so instead
// of overriding setText()/text(), this exposes distinctly-named
// setValueText()/valueText() for the two-line display, and callers that
// need to treat "any button, ControllerButton or plain QPushButton"
// uniformly should use the free helpers in input_panel_widgets.hpp.
#pragma once

#include <QPushButton>
#include <QString>

class QLabel;
class QResizeEvent;

namespace goliath {

class ControllerButton : public QPushButton {
    Q_OBJECT
public:
    explicit ControllerButton(const QString& keyName, const QString& color = QString(),
                               bool isRound = true, QWidget* parent = nullptr);

    void setValueText(const QString& text);
    QString valueText() const { return m_rawText; }

    QString defaultStyleSheet() const { return m_defaultStyleSheet; }
    QString listeningStyleSheet() const { return m_listeningStyleSheet; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QString buildStyleSheet(const QString& color, bool isRound) const;
    void layoutLabels();

    QString m_rawText = "--";
    QString m_defaultStyleSheet;
    QString m_listeningStyleSheet;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_valueLabel = nullptr;
};

} // namespace goliath
