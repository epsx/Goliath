#include "input_panel_widgets.hpp"
#include "controller_button.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

namespace goliath {

QString friendlySectionName(const QString& section) {
    if (section == "neogeosystem") return "Neo Geo System";
    if (section == "neogeojs1") return "Player 1";
    if (section == "neogeojs2") return "Player 2";
    if (section == "neogeojs3") return "Player 3";
    if (section == "neogeojs4") return "Player 4";
    if (section == "neogeomahjong") return "Mahjong Controller";
    if (section == "neogeovliner") return "V-Liner Controls";
    if (section == "neogeoirrmaze") return "Irritating Maze Controls";
    return section;
}

InputPanelWidget::InputPanelWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName("input_panel");
}

void InputPanelWidget::paintEvent(QPaintEvent*) {
    // QWidget subclasses do not automatically paint their QSS background and
    // border. Draw PE_Widget so the shared #input_panel rule remains the sole
    // source of the card colors, outline, and corner radius for every theme.
    QStyleOption option;
    option.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
}

// ============================== ControllerInputWidget ======================

const QStringList ControllerInputWidget::KEYS = {"Up", "Down", "Left", "Right", "Select",
                                                    "Start", "A", "B", "C", "D"};

ControllerInputWidget::ControllerInputWidget(QWidget* parent)
    : InputPanelWidget(parent) {
    setMinimumSize(520, 220);
    buildButtons();
}

void ControllerInputWidget::buildButtons() {
    auto* main = new QHBoxLayout(this);
    main->setSpacing(20);
    main->setContentsMargins(15, 15, 15, 15);

    // D-Pad group
    auto* dpadBox = new QGroupBox("D-Pad");
    auto* dpadLayout = new QGridLayout(dpadBox);
    dpadLayout->setSpacing(8);
    dpadLayout->setContentsMargins(8, 12, 8, 8);
    const char* dpadKeys[3][3] = {
        {nullptr, "Up", nullptr},
        {"Left", nullptr, "Right"},
        {nullptr, "Down", nullptr},
    };
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (dpadKeys[row][col]) {
                QString key(dpadKeys[row][col]);
                auto* btn = new ControllerButton(key, "#3daee9", false);
                btn->setMinimumSize(50, 50);
                m_buttons[key] = btn;
                dpadLayout->addWidget(btn, row, col, Qt::AlignCenter);
            } else {
                dpadLayout->addItem(new QSpacerItem(50, 50, QSizePolicy::Minimum, QSizePolicy::Minimum), row, col);
            }
        }
    }
    main->addWidget(dpadBox);

    // Select / Start group
    auto* ssBox = new QGroupBox("Start / Select");
    auto* ssLayout = new QVBoxLayout(ssBox);
    ssLayout->setSpacing(12);
    ssLayout->setContentsMargins(8, 12, 8, 8);
    for (const auto& key : {"Select", "Start"}) {
        auto* btn = new ControllerButton(key, "#7f8c8d", false);
        btn->setMinimumSize(90, 45);
        m_buttons[key] = btn;
        ssLayout->addWidget(btn, 0, Qt::AlignCenter);
    }
    ssLayout->addStretch();
    main->addWidget(ssBox);

    // Action buttons group
    auto* actBox = new QGroupBox("Action Buttons");
    auto* actLayout = new QGridLayout(actBox);
    actLayout->setSpacing(10);
    actLayout->setContentsMargins(8, 12, 8, 8);
    static const QMap<QString, QString> colors = {
        {"A", "#2ecc71"}, {"B", "#f1c40f"}, {"C", "#3498db"}, {"D", "#e74c3c"}};
    QStringList abcd = {"A", "B", "C", "D"};
    for (int i = 0; i < abcd.size(); ++i) {
        const QString& key = abcd[i];
        auto* btn = new ControllerButton(key, colors[key], false);
        btn->setMinimumSize(60, 60);
        m_buttons[key] = btn;
        actLayout->addWidget(btn, i / 2, i % 2, Qt::AlignCenter);
    }
    main->addWidget(actBox);

    main->addStretch();
}

QPushButton* ControllerInputWidget::button(const QString& key) const {
    return m_buttons.value(key, nullptr);
}

// ============================== MahjongInputWidget ==========================

const QStringList MahjongInputWidget::KEYS = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
    "Pon", "Chi", "Kan", "Reach", "Ron", "Select", "Start"};

MahjongInputWidget::MahjongInputWidget(QWidget* parent)
    : InputPanelWidget(parent) {
    setMinimumSize(720, 240);
    buildButtons();
}

void MahjongInputWidget::buildButtons() {
    auto* main = new QHBoxLayout(this);
    main->setSpacing(20);
    main->setContentsMargins(15, 15, 15, 15);

    // Tiles group
    auto* tilesBox = new QGroupBox("Tiles");
    auto* tilesLayout = new QGridLayout(tilesBox);
    tilesLayout->setSpacing(8);
    tilesLayout->setContentsMargins(8, 12, 8, 8);
    const QString tileColor = "#2c3e50";
    for (int i = 0; i < 14; ++i) {
        const QString& key = KEYS[i];
        auto* btn = new ControllerButton(key, tileColor, false);
        btn->setMinimumSize(45, 45);
        tilesLayout->addWidget(btn, i / 7, i % 7, Qt::AlignCenter);
        m_buttons[key] = btn;
    }
    main->addWidget(tilesBox);

    // Calls group
    auto* callsBox = new QGroupBox("Calls");
    auto* callsLayout = new QGridLayout(callsBox);
    callsLayout->setSpacing(8);
    callsLayout->setContentsMargins(8, 12, 8, 8);
    static const QMap<QString, QString> callColors = {
        {"Pon", "#e67e22"}, {"Chi", "#f1c40f"}, {"Kan", "#2ecc71"},
        {"Reach", "#3498db"}, {"Ron", "#e74c3c"}};
    const QStringList calls = {"Pon", "Chi", "Kan", "Reach", "Ron"};
    for (int i = 0; i < calls.size(); ++i) {
        const QString& key = calls[i];
        auto* btn = new ControllerButton(key, callColors.value(key, "#7f8c8d"), false);
        btn->setMinimumSize(70, 45);
        callsLayout->addWidget(btn, i / 3, i % 3, Qt::AlignCenter);
        m_buttons[key] = btn;
    }
    main->addWidget(callsBox);

    // System group
    auto* sysBox = new QGroupBox("System");
    auto* sysLayout = new QVBoxLayout(sysBox);
    sysLayout->setSpacing(8);
    sysLayout->setContentsMargins(8, 12, 8, 8);
    for (const auto& key : {"Select", "Start"}) {
        auto* btn = new ControllerButton(key, "#7f8c8d", false);
        btn->setMinimumSize(90, 45);
        m_buttons[key] = btn;
        sysLayout->addWidget(btn, 0, Qt::AlignCenter);
    }
    sysLayout->addStretch();
    main->addWidget(sysBox);

    main->addStretch();
}

QPushButton* MahjongInputWidget::button(const QString& key) const {
    return m_buttons.value(key, nullptr);
}

// ============================== VlinerInputWidget ===========================

const QStringList VlinerInputWidget::KEYS = {"Up", "Down", "Left", "Right", "Big", "Small",
                                               "D-Up", "Start", "Operator", "ClearCredit", "HopperOut"};

VlinerInputWidget::VlinerInputWidget(QWidget* parent)
    : InputPanelWidget(parent) {
    setMinimumSize(540, 260);
    buildButtons();
}

void VlinerInputWidget::buildButtons() {
    auto* main = new QHBoxLayout(this);
    main->setSpacing(20);
    main->setContentsMargins(15, 15, 15, 15);

    // D-Pad group
    auto* dpadBox = new QGroupBox("D-Pad");
    auto* dpadLayout = new QGridLayout(dpadBox);
    dpadLayout->setSpacing(8);
    dpadLayout->setContentsMargins(8, 12, 8, 8);
    const char* dpadKeys[3][3] = {
        {nullptr, "Up", nullptr},
        {"Left", nullptr, "Right"},
        {nullptr, "Down", nullptr},
    };
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (dpadKeys[row][col]) {
                QString key(dpadKeys[row][col]);
                auto* btn = new ControllerButton(key, "#3daee9", false);
                btn->setMinimumSize(50, 50);
                m_buttons[key] = btn;
                dpadLayout->addWidget(btn, row, col, Qt::AlignCenter);
            } else {
                dpadLayout->addItem(new QSpacerItem(50, 50, QSizePolicy::Minimum, QSizePolicy::Minimum), row, col);
            }
        }
    }
    main->addWidget(dpadBox);

    // Bet / Start group
    auto* betBox = new QGroupBox("Bet / Start");
    auto* betLayout = new QVBoxLayout(betBox);
    betLayout->setSpacing(10);
    betLayout->setContentsMargins(8, 12, 8, 8);
    auto* dup = new ControllerButton("D-Up", "#9b59b6", false);
    dup->setMinimumSize(90, 45);
    m_buttons["D-Up"] = dup;
    betLayout->addWidget(dup, 0, Qt::AlignCenter);
    auto* start = new ControllerButton("Start", "#7f8c8d", false);
    start->setMinimumSize(90, 45);
    m_buttons["Start"] = start;
    betLayout->addWidget(start, 0, Qt::AlignCenter);
    betLayout->addStretch();
    main->addWidget(betBox);

    // Action / Operator group
    auto* actionBox = new QGroupBox("Buttons");
    auto* actionLayout = new QVBoxLayout(actionBox);
    actionLayout->setSpacing(10);
    actionLayout->setContentsMargins(8, 12, 8, 8);

    auto* bigSmall = new QHBoxLayout();
    bigSmall->setSpacing(12);
    auto* big = new ControllerButton("Big", "#2ecc71", false);
    big->setMinimumSize(70, 50);
    m_buttons["Big"] = big;
    auto* small = new ControllerButton("Small", "#e74c3c", false);
    small->setMinimumSize(70, 50);
    m_buttons["Small"] = small;
    bigSmall->addWidget(big);
    bigSmall->addWidget(small);
    actionLayout->addLayout(bigSmall);

    auto* ops = new QHBoxLayout();
    ops->setSpacing(8);
    struct OpBtn { const char* key; const char* color; };
    const OpBtn opBtns[] = {
        {"Operator", "#f39c12"}, {"ClearCredit", "#e67e22"}, {"HopperOut", "#1abc9c"}};
    for (const auto& o : opBtns) {
        auto* btn = new ControllerButton(o.key, o.color, false);
        btn->setMinimumSize(70, 40);
        m_buttons[o.key] = btn;
        ops->addWidget(btn);
    }
    actionLayout->addLayout(ops);
    actionLayout->addStretch();
    main->addWidget(actionBox);

    main->addStretch();
}

QPushButton* VlinerInputWidget::button(const QString& key) const {
    return m_buttons.value(key, nullptr);
}

// ============================== IrrmazeInputWidget ==========================

const QStringList IrrmazeInputWidget::KEYS = {"XAxis", "YAxis", "LeftA", "LeftB", "RightA", "RightB", "Start"};

IrrmazeInputWidget::IrrmazeInputWidget(QWidget* parent)
    : InputPanelWidget(parent) {
    setMinimumSize(520, 220);
    buildButtons();
}

void IrrmazeInputWidget::buildButtons() {
    auto* main = new QHBoxLayout(this);
    main->setSpacing(20);
    main->setContentsMargins(15, 15, 15, 15);

    // JG exposes Irritating Maze as one pointer/trackball device with two
    // analog axes followed by five digital buttons. Keep X/Y together so the
    // UI does not imply that they are separate left/right trackballs.
    auto* axisBox = new QGroupBox("Trackball Axes (Analog)");
    auto* axisLayout = new QVBoxLayout(axisBox);
    axisLayout->setSpacing(8);
    axisLayout->setContentsMargins(8, 12, 8, 8);
    const char* axisKeys[] = {"XAxis", "YAxis"};
    for (const char* key : axisKeys) {
        auto* btn = new ControllerButton(key, "#7f8c8d", false);
        btn->setMinimumSize(100, 45);
        m_buttons[key] = btn;
        axisLayout->addWidget(btn, 0, Qt::AlignCenter);
    }
    axisLayout->addStretch();
    main->addWidget(axisBox);

    auto* buttonBox = new QGroupBox("Buttons");
    auto* buttonLayout = new QGridLayout(buttonBox);
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(8, 12, 8, 8);
    struct IrrBtn { const char* key; const char* color; int row; int col; };
    const IrrBtn buttons[] = {
        {"LeftA",  "#3498db", 0, 0}, {"LeftB",  "#2980b9", 1, 0},
        {"RightA", "#2ecc71", 0, 1}, {"RightB", "#27ae60", 1, 1},
    };
    for (const auto& c : buttons) {
        auto* btn = new ControllerButton(c.key, c.color, false);
        btn->setMinimumSize(90, 45);
        m_buttons[c.key] = btn;
        buttonLayout->addWidget(btn, c.row, c.col, Qt::AlignCenter);
    }
    main->addWidget(buttonBox);

    auto* startBox = new QGroupBox("Start");
    auto* startLayout = new QVBoxLayout(startBox);
    startLayout->setContentsMargins(8, 12, 8, 8);
    auto* start = new ControllerButton("Start", "#e74c3c", false);
    start->setMinimumSize(90, 50);
    m_buttons["Start"] = start;
    startLayout->addWidget(start, 1, Qt::AlignCenter);
    main->addWidget(startBox);

    main->addStretch();
}

QPushButton* IrrmazeInputWidget::button(const QString& key) const {
    return m_buttons.value(key, nullptr);
}

// ============================== SystemInputWidget ===========================

const QStringList SystemInputWidget::KEYS = {"Coin1", "Coin2", "Service", "Test"};

SystemInputWidget::SystemInputWidget(QWidget* parent)
    : InputPanelWidget(parent) {
    setMinimumSize(420, 130);
    buildButtons();
}

void SystemInputWidget::buildButtons() {
    auto* main = new QHBoxLayout(this);
    main->setSpacing(20);
    main->setContentsMargins(15, 15, 15, 15);

    auto* box = new QGroupBox("Cabinet");
    box->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    auto* layout = new QHBoxLayout(box);
    layout->setSpacing(12);
    layout->setContentsMargins(8, 12, 8, 8);

    static const QMap<QString, QString> colors = {
        {"Coin1", "#f1c40f"}, {"Coin2", "#f1c40f"}, {"Service", "#2ecc71"}, {"Test", "#e74c3c"}};
    for (const QString& key : KEYS) {
        auto* btn = new ControllerButton(key, colors.value(key, "#7f8c8d"), false);
        btn->setMinimumSize(70, 60);
        m_buttons[key] = btn;
        layout->addWidget(btn);
    }
    layout->addStretch();
    main->addWidget(box, 0, Qt::AlignLeft | Qt::AlignTop);
    main->addStretch();
}

QPushButton* SystemInputWidget::button(const QString& key) const {
    return m_buttons.value(key, nullptr);
}

// ============================== free helpers ================================

void setButtonDisplayText(QPushButton* btn, const QString& text) {
    if (!btn) return;
    if (auto* cb = qobject_cast<ControllerButton*>(btn)) {
        cb->setValueText(text);
    } else {
        btn->setText(text);
    }
}

QString buttonDisplayText(QPushButton* btn) {
    if (!btn) return QString();
    if (auto* cb = qobject_cast<ControllerButton*>(btn)) {
        return cb->valueText();
    }
    return btn->text();
}

void resetButtonStyle(QPushButton* btn) {
    if (!btn) return;
    if (auto* cb = qobject_cast<ControllerButton*>(btn)) {
        btn->setStyleSheet(cb->defaultStyleSheet());
    } else {
        btn->setStyleSheet("");
    }
}

} // namespace goliath
