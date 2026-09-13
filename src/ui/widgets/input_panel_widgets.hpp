// input_panel_widgets.hpp — the visual input remapping panels shown in the
// Settings dialog's Input tab (one panel per input device type: Neo Geo
// joystick, Mahjong, V-Liner, IRR Maze, System/coin). InputPanelWidget is a
// small abstract base so SettingsDialog can hold one pointer type and
// dispatch to whichever panel is currently showing.
#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QPushButton;
class QPaintEvent;

namespace goliath {

class ControllerButton;

class InputPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit InputPanelWidget(QWidget* parent = nullptr);
    virtual QPushButton* button(const QString& key) const = 0;
    virtual QStringList keys() const = 0;

protected:
    void paintEvent(QPaintEvent* event) override;
};

// --- Neo Geo joystick (Up/Down/Left/Right/Select/Start/A/B/C/D) ---
class ControllerInputWidget : public InputPanelWidget {
    Q_OBJECT
public:
    static const QStringList KEYS;

    explicit ControllerInputWidget(QWidget* parent = nullptr);
    QPushButton* button(const QString& key) const override;
    QStringList keys() const override { return KEYS; }

private:
    void buildButtons();
    QMap<QString, ControllerButton*> m_buttons;
};

// --- Mahjong panel (A-N tiles + Pon/Chi/Kan/Reach/Ron/Select/Start) ---
class MahjongInputWidget : public InputPanelWidget {
    Q_OBJECT
public:
    static const QStringList KEYS;

    explicit MahjongInputWidget(QWidget* parent = nullptr);
    QPushButton* button(const QString& key) const override;
    QStringList keys() const override { return KEYS; }

private:
    void buildButtons();
    QMap<QString, ControllerButton*> m_buttons;
};

// --- V-Liner betting panel ---
class VlinerInputWidget : public InputPanelWidget {
    Q_OBJECT
public:
    static const QStringList KEYS;

    explicit VlinerInputWidget(QWidget* parent = nullptr);
    QPushButton* button(const QString& key) const override;
    QStringList keys() const override { return KEYS; }

private:
    void buildButtons();
    QMap<QString, ControllerButton*> m_buttons;
};

// --- IRR Maze trackball panel ---
class IrrmazeInputWidget : public InputPanelWidget {
    Q_OBJECT
public:
    static const QStringList KEYS;

    explicit IrrmazeInputWidget(QWidget* parent = nullptr);
    QPushButton* button(const QString& key) const override;
    QStringList keys() const override { return KEYS; }

private:
    void buildButtons();
    QMap<QString, ControllerButton*> m_buttons;
};

// --- System/coin panel (Coin1/Coin2/Service/Test) ---
class SystemInputWidget : public InputPanelWidget {
    Q_OBJECT
public:
    static const QStringList KEYS;

    explicit SystemInputWidget(QWidget* parent = nullptr);
    QPushButton* button(const QString& key) const override;
    QStringList keys() const override { return KEYS; }

private:
    void buildButtons();
    QMap<QString, ControllerButton*> m_buttons;
};

// Translate raw INI section names (e.g. "neogeojs1") into user-friendly labels.
QString friendlySectionName(const QString& section);

// --- free helpers used wherever code needs to treat "a ControllerButton or
//     a plain QPushButton" uniformly (loadInputConfig's rare fallback rows,
//     startListening/applyMapping, saveInputConfig) ---

// Sets the two-line value display if btn is a ControllerButton, otherwise
// just QPushButton::setText().
void setButtonDisplayText(QPushButton* btn, const QString& text);

// Same idea as the above, for reading back the currently-displayed text.
QString buttonDisplayText(QPushButton* btn);

// Restores a button's default stylesheet (the colored ControllerButton look,
// or an empty stylesheet for a plain QPushButton).
void resetButtonStyle(QPushButton* btn);

} // namespace goliath
