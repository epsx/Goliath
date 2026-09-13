#include "ui/widgets/game_input_mapping_widget.hpp"

#include "ini/ini_document.hpp"
#include "input/input_maps.hpp"
#include "input/joystick_listener.hpp"
#include "ui/widgets/input_panel_widgets.hpp"

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace goliath {

namespace {

struct MappingProfileOption {
    int value;
    const char* label;
};

const std::vector<MappingProfileOption>& cartridge_mapping_profiles() {
    static const std::vector<MappingProfileOption> options = {
        {1, "Neo Geo Joysticks"},
        {2, "Mahjong"},
        {3, "4-Player (AS/JP MVS)"},
        {4, "V-Liner"},
        {5, "Irritating Maze"},
    };
    return options;
}

QStringList sections_for_mapping_profile(int profile) {
    QStringList sections = {"neogeosystem"};
    switch (profile) {
    case 2:
        sections << "neogeomahjong";
        break;
    case 3:
        sections << "neogeojs1" << "neogeojs2"
                 << "neogeojs3" << "neogeojs4";
        break;
    case 4:
        sections << "neogeovliner";
        break;
    case 5:
        sections << "neogeoirrmaze";
        break;
    default:
        sections << "neogeojs1" << "neogeojs2";
        break;
    }
    return sections;
}

bool keys_subset_of_input_defs(const QStringList& widgetKeys,
                               const std::string& section) {
    const auto definition = input_defs().find(section);
    if (definition == input_defs().end()) return false;
    for (const QString& key : widgetKeys) {
        if (std::find(definition->second.begin(), definition->second.end(),
                      key.toStdString()) == definition->second.end()) {
            return false;
        }
    }
    return true;
}

InputPanelWidget* create_input_panel(const std::string& section,
                                     QWidget* parent) {
    if (section.starts_with("neogeojs") &&
        keys_subset_of_input_defs(ControllerInputWidget::KEYS, section)) {
        return new ControllerInputWidget(parent);
    }
    if (section == "neogeomahjong" &&
        keys_subset_of_input_defs(MahjongInputWidget::KEYS, section)) {
        return new MahjongInputWidget(parent);
    }
    if (section == "neogeovliner" &&
        keys_subset_of_input_defs(VlinerInputWidget::KEYS, section)) {
        return new VlinerInputWidget(parent);
    }
    if (section == "neogeoirrmaze" &&
        keys_subset_of_input_defs(IrrmazeInputWidget::KEYS, section)) {
        return new IrrmazeInputWidget(parent);
    }
    if (section == "neogeosystem" &&
        keys_subset_of_input_defs(SystemInputWidget::KEYS, section)) {
        return new SystemInputWidget(parent);
    }
    return nullptr;
}

QString mapping_tooltip(const QString& stored, bool overridden) {
    const QString shownValue = stored.isEmpty() ? "not set" : stored;
    QString text = overridden
        ? QString("Custom mapping for this game.\nStored value: %1"
                  "\n\nRight-click to inherit the global mapping.")
              .arg(shownValue)
        : QString("Inherited from the global mapping.\nStored value: %1")
              .arg(shownValue);

    const auto action =
        jgrf_hotkey_action_for_binding(stored.toStdString());
    if (action.has_value()) {
        text += QString::fromUtf8(
                    "\n\n\xE2\x9A\xA0 JGRF frontend hotkey: %1"
                    "\nThis keyboard key also triggers a JGRF frontend action "
                    "while hotkeys are enabled."
                    "\n\nPress Shift+Tab in JGRF to toggle frontend hotkey processing.")
                    .arg(QString::fromStdString(*action));
    }
    return text;
}

} // namespace

GameInputMappingWidget::GameInputMappingWidget(
        AppPaths paths,
        std::string system,
        GameInputOverrides initialOverrides,
        QWidget* parent)
    : QWidget(parent),
      m_paths(std::move(paths)),
      m_system(std::move(system)),
      m_overrides(std::move(initialOverrides)) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* note = new QLabel(
        "Unchanged controls inherit Settings -> Input. A custom mapping "
        "affects only this exact ROM/CUE/CHD and is marked with a bullet. "
        "The mapping profile only chooses which controls are edited; it "
        "does not change General -> Emulated Input. V-Liner and Irritating "
        "Maze are auto-detected by Geolith. "
        "Click a waiting control again to cancel; right-click a custom "
        "mapping to restore inheritance.");
    note->setWordWrap(true);
    note->setObjectName("themed_note");
    layout->addWidget(note);

    auto* profileRow = new QHBoxLayout();
    profileRow->addWidget(new QLabel("Mapping profile:"));
    m_mappingProfileCombo = new QComboBox();
    const auto& profileOptions = cartridge_mapping_profiles();
    const std::size_t optionCount =
        m_system == "neogeocd" ? 1 : profileOptions.size();
    for (std::size_t index = 0; index < optionCount; ++index) {
        m_mappingProfileCombo->addItem(
            profileOptions[index].label, profileOptions[index].value);
    }
    const int initialIndex =
        m_mappingProfileCombo->findData(initialMappingProfile());
    m_mappingProfileCombo->setCurrentIndex(initialIndex >= 0 ? initialIndex : 0);
    connect(m_mappingProfileCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                cancelListening();
                rebuild();
            });
    profileRow->addWidget(m_mappingProfileCombo);
    profileRow->addStretch();
    layout->addLayout(profileRow);

    auto* controllerRow = new QHBoxLayout();
    controllerRow->addWidget(new QLabel("Capture from:"));
    m_joystickCombo = new QComboBox();
    m_joystickCombo->setMinimumWidth(260);
    m_joystickCombo->setToolTip(
        "Keyboard capture is always active. The selected SDL controller is "
        "captured alongside it.");
    connect(m_joystickCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { cancelListening(); });
    controllerRow->addWidget(m_joystickCombo);

    auto* refreshButton =
        new QPushButton(QString::fromUtf8("\xE2\x86\xBB Refresh"));
    connect(refreshButton, &QPushButton::clicked,
            this, &GameInputMappingWidget::refreshJoystickList);
    controllerRow->addWidget(refreshButton);
    controllerRow->addStretch();
    layout->addLayout(controllerRow);

    auto* portNote = new QLabel(
        "The General-tab Player 1 Controller retargets inherited bindings "
        "only. An explicit joystick mapping here retains the captured jN port.");
    portNote->setWordWrap(true);
    layout->addWidget(portNote);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    m_container = new QWidget();
    m_containerLayout = new QVBoxLayout(m_container);
    m_containerLayout->setSizeConstraint(QLayout::SetMinimumSize);
    scroll->setWidget(m_container);
    layout->addWidget(scroll);

    auto* resetButton = new QPushButton(
        QString::fromUtf8("\xE2\x86\xBB Reset shown mappings to Global"));
    resetButton->setToolTip(
        "Clears per-game overrides for the displayed mapping profile, "
        "including its shared Neo Geo System controls. Other mapping "
        "profiles remain unchanged.");
    connect(resetButton, &QPushButton::clicked,
            this, &GameInputMappingWidget::resetCurrentProfile);
    layout->addWidget(resetButton, 0, Qt::AlignLeft);

    refreshJoystickList();
    rebuild();
}

GameInputMappingWidget::~GameInputMappingWidget() {
    cancelListening();
}

GameInputOverrides GameInputMappingWidget::overrides() const {
    return m_overrides;
}

int GameInputMappingWidget::initialMappingProfile() const {
    if (m_system == "neogeocd") return 1;
    if (m_overrides.contains("neogeoirrmaze")) return 5;
    if (m_overrides.contains("neogeovliner")) return 4;
    if (m_overrides.contains("neogeojs3") ||
        m_overrides.contains("neogeojs4")) {
        return 3;
    }
    if (m_overrides.contains("neogeomahjong")) return 2;
    return 1;
}

QString GameInputMappingWidget::effectiveValue(
        const IniDocument& globalConfig,
        const std::string& section,
        const std::string& key,
        bool* overridden) const {
    const auto sectionOverride = m_overrides.find(section);
    if (sectionOverride != m_overrides.end()) {
        const auto keyOverride = sectionOverride->second.find(key);
        if (keyOverride != sectionOverride->second.end()) {
            if (overridden) *overridden = true;
            return QString::fromStdString(keyOverride->second);
        }
    }
    if (overridden) *overridden = false;

    std::optional<std::string> globalValue;
    for (const std::string& rawKey : globalConfig.options(section)) {
        if (canonical_input_key(section, rawKey) == key)
            globalValue = globalConfig.get(section, rawKey);
    }
    if (globalValue.has_value() && !globalValue->empty())
        return QString::fromStdString(*globalValue);

    const auto defaultSection = default_input_config().find(section);
    if (defaultSection != default_input_config().end()) {
        const auto defaultKey = defaultSection->second.find(key);
        if (defaultKey != defaultSection->second.end())
            return QString::fromStdString(defaultKey->second);
    }
    return {};
}

void GameInputMappingWidget::updateButtonDisplay(
        QPushButton* button,
        const IniDocument& globalConfig,
        const std::string& section,
        const std::string& key) {
    bool overridden = false;
    const QString value =
        effectiveValue(globalConfig, section, key, &overridden);
    const auto [stored, display] = normalize_input_value(value.toStdString());

    QString shown = value.isEmpty()
        ? QStringLiteral("--")
        : QString::fromStdString(display);
    if (overridden)
        shown.prepend(QString::fromUtf8("\xE2\x80\xA2 "));

    if (!value.isEmpty() &&
        !normalize_input_binding_for_definition(section, key, stored).has_value()) {
        shown = QStringLiteral("Invalid: ") + shown;
    }

    setButtonDisplayText(button, shown);
    button->setToolTip(mapping_tooltip(
        QString::fromStdString(stored), overridden));
}

void GameInputMappingWidget::rebuild() {
    if (!m_containerLayout || !m_mappingProfileCombo) return;

    QLayoutItem* child = nullptr;
    while ((child = m_containerLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    IniDocument globalConfig;
    globalConfig.load(m_paths.input_config);

    const int mappingProfile =
        m_mappingProfileCombo->currentData().toInt();
    for (const QString& sectionName :
         sections_for_mapping_profile(mappingProfile)) {
        const std::string section = sectionName.toStdString();
        const auto definition = input_defs().find(section);
        if (definition == input_defs().end()) continue;

        auto* heading = new QLabel(
            QString("<b>%1</b>").arg(friendlySectionName(sectionName)));
        heading->setObjectName("section_heading");
        m_containerLayout->addWidget(heading);

        InputPanelWidget* panel =
            create_input_panel(section, m_container);
        if (panel) m_containerLayout->addWidget(panel);

        for (const std::string& key : definition->second) {
            QPushButton* button =
                panel ? panel->button(QString::fromStdString(key)) : nullptr;
            if (!button) {
                auto* rowWidget = new QWidget(m_container);
                auto* row = new QHBoxLayout(rowWidget);
                auto* label = new QLabel(QString::fromStdString(key));
                label->setFixedWidth(120);
                row->addWidget(label);
                button = new QPushButton();
                row->addWidget(button);
                m_containerLayout->addWidget(rowWidget);
            }

            updateButtonDisplay(button, globalConfig, section, key);
            const QString qSection = sectionName;
            const QString qKey = QString::fromStdString(key);
            connect(button, &QPushButton::clicked,
                    this, [this, button, qSection, qKey]() {
                        startListening(button, qSection, qKey);
                    });

            button->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(button, &QWidget::customContextMenuRequested,
                    this, [this, button, section, key](const QPoint& point) {
                        QMenu menu(button);
                        QAction* resetAction =
                            menu.addAction("Inherit global mapping");
                        const auto sectionOverride = m_overrides.find(section);
                        const bool hasOverride =
                            sectionOverride != m_overrides.end() &&
                            sectionOverride->second.contains(key);
                        resetAction->setEnabled(hasOverride);
                        if (menu.exec(button->mapToGlobal(point)) == resetAction)
                            resetOneMapping(section, key);
                    });
        }
    }

    m_containerLayout->addStretch();
}

void GameInputMappingWidget::refreshJoystickList() {
    if (!m_joystickCombo) return;
    cancelListening();

    const std::uint32_t previousId =
        m_joystickCombo->currentData().toUInt();
    m_joystickCombo->clear();
    m_detectedJoysticks = list_joysticks();

    int restoreIndex = 0;
    for (const DetectedJoystick& joystick : m_detectedJoysticks) {
        if (joystick.port < 0 || joystick.port > 9) continue;
        m_joystickCombo->addItem(
            QString("Keyboard + Port %1 (j%2): %3")
                .arg(joystick.port + 1)
                .arg(joystick.port)
                .arg(joystick.name),
            joystick.instanceId);
        if (joystick.instanceId == previousId)
            restoreIndex = m_joystickCombo->count() - 1;
    }

    if (m_joystickCombo->count() == 0) {
        m_joystickCombo->addItem(
            "Keyboard only (no controller detected)", 0u);
        m_joystickCombo->setEnabled(false);
        return;
    }

    m_joystickCombo->setEnabled(true);
    m_joystickCombo->setCurrentIndex(restoreIndex);
}

void GameInputMappingWidget::startListening(
        QPushButton* button,
        const QString& section,
        const QString& key) {
    if (button == m_listeningButton) {
        cancelListening();
        return;
    }
    cancelListening();

    m_listeningButton = button;
    m_listeningSection = section;
    m_listeningKey = key;
    m_listeningDisplay = buttonDisplayText(button);
    m_listeningTooltip = button->toolTip();
    setButtonDisplayText(
        button,
        QString::fromUtf8("\xE2\x8C\xA8\xEF\xB8\x8F/\xF0\x9F\x8E\xAE Waiting..."));
    button->setToolTip(
        "Press a keyboard key or use the selected controller. "
        "Click this control again to cancel.");
    button->setStyleSheet("background-color: #3daee9; color: white;");
    grabKeyboard();

    if (!m_joystickCombo || !m_joystickCombo->isEnabled() ||
        m_joystickCombo->currentIndex() < 0) {
        return;
    }

    const std::uint32_t instanceId =
        m_joystickCombo->currentData().toUInt();
    int port = -1;
    for (const DetectedJoystick& joystick : m_detectedJoysticks) {
        if (joystick.instanceId == instanceId) {
            port = joystick.port;
            break;
        }
    }
    if (port < 0 || port > 9) return;

    const JoystickCaptureMode mode = is_analog_input_definition(
        section.toStdString(), key.toStdString())
        ? JoystickCaptureMode::AnalogAxis
        : JoystickCaptureMode::Digital;
    m_joystickListener =
        new JoystickListener(instanceId, port, mode, this);
    connect(m_joystickListener, &JoystickListener::mapped,
            this, [this](const QString& value) { applyMapping(value); });
    connect(m_joystickListener, &QThread::finished, this, [this]() {
        auto* finished = qobject_cast<JoystickListener*>(sender());
        if (finished == m_joystickListener)
            m_joystickListener = nullptr;
        if (finished) finished->deleteLater();
    });
    m_joystickListener->start();
}

void GameInputMappingWidget::stopJoystickListener() {
    if (!m_joystickListener) return;
    JoystickListener* listener = m_joystickListener;
    m_joystickListener = nullptr;
    listener->stop();
}

void GameInputMappingWidget::cancelListening() {
    stopJoystickListener();

    if (m_listeningButton) {
        setButtonDisplayText(m_listeningButton, m_listeningDisplay);
        m_listeningButton->setToolTip(m_listeningTooltip);
        resetButtonStyle(m_listeningButton);
    }

    m_listeningButton = nullptr;
    m_listeningSection.clear();
    m_listeningKey.clear();
    m_listeningDisplay.clear();
    m_listeningTooltip.clear();
    if (QWidget::keyboardGrabber() == this) releaseKeyboard();
}

void GameInputMappingWidget::applyMapping(
        const QString& value,
        const QString& displayName) {
    if (!m_listeningButton) return;

    const std::string section = m_listeningSection.toStdString();
    const std::string key = m_listeningKey.toStdString();
    const auto stored = normalize_input_binding_for_definition(
        section, key, value.toStdString());
    if (!stored.has_value()) {
        setButtonDisplayText(m_listeningButton, "Unsupported input; try again...");
        m_listeningButton->setToolTip(
            "This value is not safe for the selected JGRF input definition. "
            "Click the control again to cancel.");
        return;
    }

    stopJoystickListener();
    m_overrides[section][key] = *stored;

    QString display;
    if (!displayName.isEmpty()) {
        display = QString("%1 (%2)").arg(
            displayName, QString::fromStdString(*stored));
    } else {
        display = QString::fromStdString(
            normalize_input_value(*stored).second);
    }
    setButtonDisplayText(
        m_listeningButton,
        QString::fromUtf8("\xE2\x80\xA2 ") + display);
    m_listeningButton->setToolTip(
        mapping_tooltip(QString::fromStdString(*stored), true));
    resetButtonStyle(m_listeningButton);

    m_listeningButton = nullptr;
    m_listeningSection.clear();
    m_listeningKey.clear();
    m_listeningDisplay.clear();
    m_listeningTooltip.clear();
    if (QWidget::keyboardGrabber() == this) releaseKeyboard();
}

void GameInputMappingWidget::resetOneMapping(
        const std::string& section,
        const std::string& key) {
    cancelListening();
    const auto sectionOverride = m_overrides.find(section);
    if (sectionOverride == m_overrides.end()) return;
    sectionOverride->second.erase(key);
    if (sectionOverride->second.empty())
        m_overrides.erase(sectionOverride);
    rebuild();
}

void GameInputMappingWidget::resetCurrentProfile() {
    const int profile = m_mappingProfileCombo->currentData().toInt();
    const auto reply = QMessageBox::question(
        this, "Reset Per-Game Mappings",
        "Clear all per-game mappings shown for this mapping profile, "
        "including its shared Neo Geo System controls? Other mapping "
        "profiles will remain unchanged.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    cancelListening();
    for (const QString& section : sections_for_mapping_profile(profile))
        m_overrides.erase(section.toStdString());
    rebuild();
}

void GameInputMappingWidget::clearAllOverrides() {
    cancelListening();
    m_overrides.clear();
    rebuild();
}

bool GameInputMappingWidget::event(QEvent* event) {
    if (m_listeningButton && event->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return true;
    }
    return QWidget::event(event);
}

void GameInputMappingWidget::keyPressEvent(QKeyEvent* event) {
    if (!m_listeningButton) {
        QWidget::keyPressEvent(event);
        return;
    }

    const int key = event->key();
    if (key == Qt::Key_Shift || key == Qt::Key_Control ||
        key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }

    if (is_analog_input_definition(
            m_listeningSection.toStdString(),
            m_listeningKey.toStdString())) {
        setButtonDisplayText(m_listeningButton, "Move analog axis...");
        m_listeningButton->setToolTip(
            "This emulated axis requires a physical analog joystick axis. "
            "Click the control again to cancel.");
        return;
    }

    std::optional<int> scancode;
    if (event->modifiers() & Qt::KeypadModifier) {
        const auto mapped = qt_keypad_to_sdl_scancode().find(key);
        if (mapped != qt_keypad_to_sdl_scancode().end())
            scancode = mapped->second;
    }
    if (!scancode.has_value()) {
        const auto mapped = qt_key_to_sdl_scancode().find(key);
        if (mapped != qt_key_to_sdl_scancode().end())
            scancode = mapped->second;
    }
    if (!scancode.has_value() && !event->text().isEmpty()) {
        QString alias = event->text().toLower();
        alias.remove(' ');
        const auto mapped = name_to_sdl_scancode().find(alias.toStdString());
        if (mapped != name_to_sdl_scancode().end())
            scancode = mapped->second;
    }

    if (!scancode.has_value()) {
        setButtonDisplayText(m_listeningButton, "Unsupported key; try again...");
        m_listeningButton->setToolTip(
            "The key has no known SDL scancode. Click the control again to cancel.");
        return;
    }

    const auto name = sdl_scancode_names().find(*scancode);
    applyMapping(
        QString::number(*scancode),
        name != sdl_scancode_names().end()
            ? QString::fromStdString(name->second)
            : QString::number(*scancode));
}

} // namespace goliath
