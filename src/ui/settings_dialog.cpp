#include "ui/settings_dialog.hpp"

#include "common/goliath_common.hpp"
#include "common/theme.hpp"
#include "ui/tabs/audio_tab.hpp"
#include "ui/tabs/core_tab.hpp"
#include "ui/tabs/misc_tab.hpp"
#include "ui/tabs/info_tab.hpp"
#include "ui/widgets/title_bar.hpp"
#include "ini/ini_document.hpp"
#include "input/input_maps.hpp"
#include "ui/widgets/input_panel_widgets.hpp"
#include "input/joystick_listener.hpp"
#include "input/sdl_joystick.hpp"
#include "ui/tabs/video_tab.hpp"

#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

constexpr int kDefaultAxisDeadzone = 5;

const std::vector<std::pair<int, QString>>& inputDeviceOptions() {
    static const std::vector<std::pair<int, QString>> opts = {
        {1, "Neo Geo Joysticks"},
        {2, "Mahjong"},
        {3, "4-Player (AS/JP MVS)"},
        {4, "V-Liner"},
        {5, "Irritating Maze"},
    };
    return opts;
}

// (label, config key, is_file) — user-facing folders only. "database"
// (games.json location), "config" (jollygood/geolith.ini location) and
// "jollygood" (frontend executable) are deliberately not exposed here: the
// Video/Core/Input tabs hardcode the "geolith" ini schema and the
// "<config>/jollygood/" subfolder layout regardless of what these would be
// set to, so letting users redirect them here would be misleading.
// The database scanner reads its metadata files from the resolved Metadata
// Folder. Neo Geo cartridge and Neo Geo CD media folders are intentionally
// separate because CD sets are normally organized recursively.
struct PathSetting { const char* label; const char* key; };
const std::vector<PathSetting>& pathSettings() {
    static const std::vector<PathSetting> settings = {
        {"Neo Geo MVS/AES ROMs", "roms"},
        {"Neo Geo CD Images", "neocd"},
        {"Icons Folder", "icons"},
        {"Snapshots Folder", "snaps"},
        {"Metadata Folder", "metadata"},
        {"BIOS Folder", "bios"},
    };
    return settings;
}

// True if every key a visual input panel widget would show is also a valid
// key for this ini section per input_defs() (i.e. the panel is a safe fit).
bool keysSubsetOfInputDefs(const QStringList& widgetKeys, const std::string& section) {
    auto it = input_defs().find(section);
    if (it == input_defs().end()) return false;
    const auto& defKeys = it->second;
    for (const QString& k : widgetKeys) {
        if (std::find(defKeys.begin(), defKeys.end(), k.toStdString()) == defKeys.end()) return false;
    }
    return true;
}

QString inputMappingTooltip(const QString& stored) {
    const QString shownValue = stored.isEmpty() ? "not set" : stored;
    QString text = QString("Stored value: %1").arg(shownValue);

    if (!stored.isEmpty()) {
        auto action = jgrf_hotkey_action_for_binding(stored.toStdString());
        if (action.has_value()) {
            text += QString::fromUtf8(
                "\n\n\xE2\x9A\xA0 JGRF frontend hotkey: %1"
                "\nThis keyboard key also triggers a JGRF frontend action while hotkeys are enabled."
                "\n\nPress Shift+Tab in JGRF to toggle frontend hotkey processing.")
                .arg(QString::fromStdString(*action));
        }
    }

    return text;
}
} // namespace

SettingsDialog::SettingsDialog(Config& config, AppPaths paths, QWidget* parent)
    : QDialog(parent), m_config(config), m_paths(std::move(paths)) {
    resize(900, 730);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Settings", &layout);

    m_tabs = new QTabWidget();
    layout->addWidget(m_tabs);
    connect(m_tabs, &QTabWidget::currentChanged, this, &SettingsDialog::onTabChanged);

    auto* videoTab = new VideoTab(m_paths.jollygood_settings_ini, m_paths.geolith_ini,
                                  m_paths.jollygood_exe, this);
    m_tabs->addTab(videoTab, "Video");

    auto* audioTab = new AudioTab(m_config, m_paths.jollygood_settings_ini,
                                  m_paths.geolith_ini, this);
    m_tabs->addTab(audioTab, "Audio");

    auto* miscTab = new MiscTab(m_paths.jollygood_settings_ini, m_paths.geolith_ini, this);
    m_tabs->addTab(miscTab, "Misc");

    auto* coreTab = new CoreTab(m_paths.geolith_ini, this);
    m_tabs->addTab(coreTab, "Core");

    auto* inputWidget = new QWidget();
    m_tabs->addTab(inputWidget, "Input");
    buildInputTab(inputWidget);

    auto* pathWidget = new QWidget();
    m_tabs->addTab(pathWidget, "Path");
    buildPathTab(pathWidget);

    m_infoTab = new InfoTab(m_paths.jollygood_exe, this);
    m_tabs->addTab(m_infoTab, "Info");

    const std::string themeKey =
        m_config.get("UI", "theme", "Dark Modern");
    const Theme& theme = find_theme(themeKey);
    applyComboPopupStyleToDescendants(
        this,
        QString::fromStdString(generate_combo_popup_style(theme)));
}

SettingsDialog::~SettingsDialog() {
    cancelListening();
}

void SettingsDialog::buildInputTab(QWidget* inputWidget) {
    auto* pageLayout = new QVBoxLayout(inputWidget);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto* pageScroll = new QScrollArea();
    pageScroll->setObjectName("input_tab_scroll");
    pageScroll->setWidgetResizable(true);
    pageLayout->addWidget(pageScroll);

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 10);
    pageScroll->setWidget(content);

    auto* deviceRow = new QHBoxLayout();
    deviceRow->addWidget(new QLabel("Input Device:"));
    m_inputDeviceCombo = new QComboBox();
    for (const auto& opt : inputDeviceOptions()) {
        m_inputDeviceCombo->addItem(opt.second, opt.first);
    }
    m_inputDeviceCombo->setCurrentIndex(0);
    connect(m_inputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onInputDeviceChanged);
    deviceRow->addWidget(m_inputDeviceCombo);
    deviceRow->addStretch();
    layout->addLayout(deviceRow);

    auto* joystickRow = new QHBoxLayout();
    joystickRow->addWidget(new QLabel("Capture from:"));
    m_joystickCombo = new QComboBox();
    m_joystickCombo->setMinimumWidth(220);
    m_joystickCombo->setToolTip(
        "Keyboard capture is always active. The selected SDL controller is "
        "captured alongside it.");
    connect(m_joystickCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { cancelListening(); });
    joystickRow->addWidget(m_joystickCombo);
    auto* refreshBtn = new QPushButton(QString::fromUtf8("\xE2\x86\xBB Refresh")); // ↻ Refresh
    connect(refreshBtn, &QPushButton::clicked, this, &SettingsDialog::refreshJoystickList);
    joystickRow->addWidget(refreshBtn);
    joystickRow->addStretch();
    layout->addLayout(joystickRow);
    refreshJoystickList();

    auto* jgrfInputGroup = new QGroupBox("JollyGood Input Settings");
    auto* jgrfInputForm = new QFormLayout(jgrfInputGroup);
    m_axisDeadzoneSpin = new QSpinBox();
    m_axisDeadzoneSpin->setRange(0, 9);
    m_axisDeadzoneSpin->setToolTip(
        "JGRF analog-axis deadzone. Range 0-9; default 5. "
        "Higher values ignore more movement around the center of an analog axis.");
    jgrfInputForm->addRow("Axis Deadzone:", m_axisDeadzoneSpin);

    auto* saveJgrfInputBtn = new QPushButton("Save JGRF Input Settings");
    connect(saveJgrfInputBtn, &QPushButton::clicked,
            this, &SettingsDialog::saveJgrfInputSettings);
    jgrfInputForm->addRow("", saveJgrfInputBtn);
    layout->addWidget(jgrfInputGroup);
    loadJgrfInputSettings();

    auto* hotkeyNote = new QLabel(QString::fromUtf8(
        "<b>Input capture:</b> The keyboard is always active; the selected SDL "
        "controller is captured alongside it. Click a waiting control again to cancel."
        "<br><br><b>JGRF hotkeys:</b> Keyboard mappings may overlap with frontend hotkeys "
        "(for example <b>F</b> = Fullscreen, <b>M</b> = Mute, <b>P</b> = Pause). "
        "Hover a mapped button for details. Press <b>Shift+Tab</b> in JGRF to "
        "toggle frontend hotkey processing."));
    hotkeyNote->setWordWrap(true);
    hotkeyNote->setObjectName("themed_note");
    layout->addWidget(hotkeyNote);

    m_inputContainer = new QWidget();
    m_inputLayout = new QVBoxLayout(m_inputContainer);
    m_inputLayout->setSizeConstraint(QLayout::SetMinimumSize);
    layout->addWidget(m_inputContainer);

    auto* inputBtnRow = new QHBoxLayout();
    auto* saveBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x92\xBE Save Input Config"));
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveInputConfig);
    inputBtnRow->addWidget(saveBtn);

    auto* resetBtn = new QPushButton(QString::fromUtf8("\xE2\x86\xBB Reset to Defaults")); // ↻ Reset to Defaults
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetInputConfig);
    inputBtnRow->addWidget(resetBtn);
    layout->addLayout(inputBtnRow);

    int storedDevice = storedInputProfile();
    int idx = m_inputDeviceCombo->findData(storedDevice);
    if (idx >= 0) {
        m_inputDeviceCombo->blockSignals(true);
        m_inputDeviceCombo->setCurrentIndex(idx);
        m_inputDeviceCombo->blockSignals(false);
    }

    loadInputConfig();
}

void SettingsDialog::buildPathTab(QWidget* pathWidget) {
    auto* pathLayout = new QVBoxLayout(pathWidget);

    for (const auto& ps : pathSettings()) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(QString("%1:").arg(ps.label));
        lbl->setFixedWidth(190);
        row->addWidget(lbl);

        auto* entry = new QLineEdit(QString::fromStdString(resolve_path(m_config, ps.key).string()));
        entry->setReadOnly(true);
        row->addWidget(entry);

        auto* btn = new QPushButton("Browse...");
        std::string key = ps.key;
        QString label = ps.label;
        connect(btn, &QPushButton::clicked, this, [this, key, label]() {
            browsePath(key, label);
        });
        row->addWidget(btn);

        m_pathEntries[ps.key] = entry;
        pathLayout->addLayout(row);
    }

    pathLayout->addStretch();

    auto* saveBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x92\xBE Save Path Settings"));
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::savePathConfig);
    pathLayout->addWidget(saveBtn);

    auto* note = new QLabel("After saving, use Tools \xE2\x96\xBC \xE2\x86\x92 Rescan ROMs (or F5) "
                             "so the library picks up the new folders.");
    note->setWordWrap(true);
    pathLayout->addWidget(note);
}

void SettingsDialog::loadJgrfInputSettings() {
    if (!m_axisDeadzoneSpin) return;

    auto validDeadzone = [](int value) { return value >= 0 && value <= 9; };

    IniDocument cfg;
    cfg.load(m_paths.jollygood_settings_ini);
    int value = kDefaultAxisDeadzone;
    if (cfg.has_option("input", "axis_deadzone")) {
        try {
            const int candidate = std::stoi(cfg.get("input", "axis_deadzone"));
            value = validDeadzone(candidate) ? candidate : kDefaultAxisDeadzone;
        } catch (...) {
            value = kDefaultAxisDeadzone;
        }
    }

    // JGRF applies geolith.ini after settings.ini. A valid core-specific
    // [input] override therefore represents the effective runtime value.
    IniDocument coreCfg;
    coreCfg.load(m_paths.geolith_ini);
    if (coreCfg.has_option("input", "axis_deadzone")) {
        try {
            const int overrideValue = std::stoi(coreCfg.get("input", "axis_deadzone"));
            if (validDeadzone(overrideValue)) value = overrideValue;
        } catch (...) {
        }
    }

    m_axisDeadzoneSpin->setValue(value);
}

void SettingsDialog::persistJgrfInputSettings(int deadzone) {
    std::error_code ec;
    fs::create_directories(m_paths.jollygood_settings_ini.parent_path(), ec);

    IniDocument cfg;
    cfg.load(m_paths.jollygood_settings_ini);
    cfg.set("input", "axis_deadzone", std::to_string(deadzone));
    cfg.save(m_paths.jollygood_settings_ini);

    // Keep global settings.ini canonical for frontend settings; otherwise a
    // stale core override would silently win at the next JGRF launch.
    IniDocument coreCfg;
    coreCfg.load(m_paths.geolith_ini);
    coreCfg.remove_section("input");
    coreCfg.save(m_paths.geolith_ini);
}

void SettingsDialog::saveJgrfInputSettings() {
    if (!m_axisDeadzoneSpin) return;

    persistJgrfInputSettings(m_axisDeadzoneSpin->value());

}

int SettingsDialog::storedInputProfile() const {
    const std::string stored = m_config.get("UI", "input_device", "");
    if (!stored.empty()) {
        try {
            const int value = std::stoi(stored);
            if (value >= 1 && value <= 5) return value;
        } catch (...) {
        }
    }

    // Migration fallback for configurations made before the frontend profile
    // was stored separately. Only 1..3 were ever valid Geolith values.
    IniDocument cfg;
    cfg.load(m_paths.geolith_ini);
    try {
        const int value = std::stoi(cfg.get("geolith", "input", "1"));
        if (value >= 1 && value <= 3) return value;
    } catch (...) {
    }
    return 1;
}

int SettingsDialog::currentInputProfile() const {
    if (m_inputDeviceCombo) {
        const int value = m_inputDeviceCombo->currentData().toInt();
        if (value >= 1 && value <= 5) return value;
    }
    return storedInputProfile();
}

QStringList SettingsDialog::targetInputSections(int device) const {
    if (device < 0) device = currentInputProfile();

    QStringList sections = {"neogeosystem"};
    switch (device) {
        case 2:
            sections << "neogeomahjong";
            break;
        case 3:
            sections << "neogeojs1" << "neogeojs2" << "neogeojs3" << "neogeojs4";
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

void SettingsDialog::onTabChanged(int index) {
    cancelListening();

    const QString tabName = m_tabs->tabText(index);
    if (tabName == "Info") {
        if (m_infoTab) m_infoTab->activate();
        return;
    }
    if (tabName != "Input") return;
    // Keep the Input tab on Goliath's frontend profile. V-Liner and
    // Irritating Maze both use Geolith's Auto mode underneath.
    const int storedDevice = storedInputProfile();
    if (m_inputDeviceCombo) {
        int idx = m_inputDeviceCombo->findData(storedDevice);
        if (idx >= 0 && m_inputDeviceCombo->currentIndex() != idx) {
            m_inputDeviceCombo->setCurrentIndex(idx);
        }
    }
    int device = currentInputProfile();
    if (m_lastInputDevice != device) {
        loadInputConfig();
    }
}

void SettingsDialog::onInputDeviceChanged(int) {
    cancelListening();
    loadInputConfig();
}

std::string SettingsDialog::canonicalInputKey(const std::string& section, const std::string& key) const {
    return canonical_input_key(section, key);
}

void SettingsDialog::createDefaultInputConfig(int device) {
    if (device < 0) device = currentInputProfile();

    std::error_code ec;
    fs::create_directories(m_paths.input_config.parent_path(), ec);

    IniDocument doc;
    for (const QString& qsection : targetInputSections(device)) {
        std::string section = qsection.toStdString();
        auto keysIt = input_defs().find(section);
        if (keysIt == input_defs().end()) continue;

        doc.ensure_section(section);
        auto defIt = default_input_config().find(section);
        for (const std::string& key : keysIt->second) {
            std::string value;
            if (defIt != default_input_config().end()) {
                auto kIt = defIt->second.find(key);
                if (kIt != defIt->second.end()) value = kIt->second;
            }
            doc.set(section, key, value);
        }
    }
    doc.save(m_paths.input_config);
}

std::pair<std::string, std::string> SettingsDialog::normalizeInputValue(const std::string& valueIn) const {
    return normalize_input_value(valueIn);
}

void SettingsDialog::addInputRow(const QString& section, const QString& key, const std::string& value) {
    auto* rowWidget = new QWidget();
    auto* row = new QHBoxLayout(rowWidget);
    auto* label = new QLabel(key);
    label->setFixedWidth(100);
    row->addWidget(label);

    auto [stored, display] = normalizeInputValue(value);
    auto* btn = new QPushButton(!value.empty() ? QString::fromStdString(display) : "--");
    btn->setProperty("input_value", !value.empty() ? QString::fromStdString(stored) : QString());
    btn->setToolTip(inputMappingTooltip(!value.empty() ? QString::fromStdString(stored) : QString()));

    connect(btn, &QPushButton::clicked, this, [this, btn, section, key]() {
        startListening(btn, section, key);
    });
    row->addWidget(btn);

    m_inputEntries.push_back(InputRow{section, key, btn});
    m_inputLayout->addWidget(rowWidget);
}

void SettingsDialog::loadInputConfig() {
    QLayoutItem* child;
    while ((child = m_inputLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    m_inputEntries.clear();

    int device = currentInputProfile();
    QStringList targetSections = targetInputSections(device);

    if (!fs::is_regular_file(m_paths.input_config)) {
        createDefaultInputConfig(device);
    }

    IniDocument config;
    config.load(m_paths.input_config);
    for (const QString& qs : targetSections) {
        config.ensure_section(qs.toStdString());
    }

    for (const QString& qs : targetSections) {
        std::string section = qs.toStdString();

        std::vector<std::string> orderedCanonical;
        std::map<std::string, std::string> values;
        for (const std::string& rawKey : config.options(section)) {
            std::string canon = canonicalInputKey(section, rawKey);
            if (values.find(canon) == values.end()) orderedCanonical.push_back(canon);
            values[canon] = config.get(section, rawKey);
        }

        std::vector<std::string> shownKeys;
        auto defIt = input_defs().find(section);
        if (defIt != input_defs().end()) shownKeys = defIt->second;
        for (const auto& k : orderedCanonical) {
            if (std::find(shownKeys.begin(), shownKeys.end(), k) == shownKeys.end()) {
                shownKeys.push_back(k);
            }
        }

        auto* header = new QLabel(QString("<b>%1</b>").arg(friendlySectionName(qs)));
        header->setObjectName("section_heading");
        m_inputLayout->addWidget(header);

        InputPanelWidget* widget = nullptr;
        if (section.starts_with("neogeojs") && keysSubsetOfInputDefs(ControllerInputWidget::KEYS, section)) {
            widget = new ControllerInputWidget(m_inputContainer);
        } else if (section == "neogeomahjong" && keysSubsetOfInputDefs(MahjongInputWidget::KEYS, section)) {
            widget = new MahjongInputWidget(m_inputContainer);
        } else if (section == "neogeovliner" && keysSubsetOfInputDefs(VlinerInputWidget::KEYS, section)) {
            widget = new VlinerInputWidget(m_inputContainer);
        } else if (section == "neogeoirrmaze" && keysSubsetOfInputDefs(IrrmazeInputWidget::KEYS, section)) {
            widget = new IrrmazeInputWidget(m_inputContainer);
        } else if (section == "neogeosystem" && keysSubsetOfInputDefs(SystemInputWidget::KEYS, section)) {
            widget = new SystemInputWidget(m_inputContainer);
        }

        if (widget) {
            m_inputLayout->addWidget(widget);
        }

        for (const std::string& key : shownKeys) {
            std::string value;
            auto vIt = values.find(key);
            if (vIt != values.end()) value = vIt->second;
            if (value.empty()) {
                auto dsIt = default_input_config().find(section);
                if (dsIt != default_input_config().end()) {
                    auto dkIt = dsIt->second.find(key);
                    if (dkIt != dsIt->second.end()) value = dkIt->second;
                }
            }

            QPushButton* btn = widget ? widget->button(QString::fromStdString(key)) : nullptr;
            if (!btn) {
                addInputRow(qs, QString::fromStdString(key), value);
                continue;
            }

            auto [stored, display] = normalizeInputValue(value);
            setButtonDisplayText(btn, !value.empty() ? QString::fromStdString(display) : "--");
            btn->setProperty("input_value", !value.empty() ? QString::fromStdString(stored) : QString());
            btn->setToolTip(inputMappingTooltip(!value.empty() ? QString::fromStdString(stored) : QString()));
            if (is_analog_input_definition(section, key) && !value.empty() &&
                !is_joystick_axis_binding(stored)) {
                setButtonDisplayText(btn, QString("Analog required: ") +
                    QString::fromStdString(display));
                btn->setToolTip(
                    QString("Invalid analog-axis mapping: %1. Move a physical analog axis to remap.")
                        .arg(QString::fromStdString(stored)));
            }

            QString qsection = qs;
            QString qkey = QString::fromStdString(key);
            connect(btn, &QPushButton::clicked, this, [this, btn, qsection, qkey]() {
                startListening(btn, qsection, qkey);
            });

            m_inputEntries.push_back(InputRow{qsection, qkey, btn});
        }
    }

    m_inputLayout->addStretch();
    m_lastInputDevice = device;
    qDebug() << "loadInputConfig done";
}

void SettingsDialog::refreshJoystickList() {
    if (!m_joystickCombo) return;
    cancelListening();

    std::uint32_t previousId = m_joystickCombo->currentData().toUInt();
    m_joystickCombo->clear();
    m_detectedJoysticks = list_joysticks();

    if (m_detectedJoysticks.empty()) {
        m_joystickCombo->addItem("Keyboard only (no controller detected)", 0u);
        m_joystickCombo->setEnabled(false);
        return;
    }

    m_joystickCombo->setEnabled(true);
    int restoreIdx = 0;
    for (const DetectedJoystick& js : m_detectedJoysticks) {
        m_joystickCombo->addItem(
            QString("Keyboard + Port %1 (j%2): %3")
                .arg(js.port + 1)
                .arg(js.port)
                .arg(js.name),
            js.instanceId);
        if (js.instanceId == previousId) restoreIdx = m_joystickCombo->count() - 1;
    }
    m_joystickCombo->setCurrentIndex(restoreIdx);
}

void SettingsDialog::startListening(QPushButton* btn, const QString& section, const QString& key) {
    if (btn == m_listeningBtn) {
        cancelListening();
        return;
    }
    cancelListening();

    m_listeningBtn = btn;
    m_listeningSection = section;
    m_listeningKey = key;
    m_listeningDisplay = buttonDisplayText(btn);
    m_listeningTooltip = btn->toolTip();
    setButtonDisplayText(btn, QString::fromUtf8("\xE2\x8C\xA8\xEF\xB8\x8F/\xF0\x9F\x8E\xAE Waiting..."));
    btn->setToolTip(
        "Press a keyboard key or use the selected controller. "
        "Click this control again to cancel.");
    btn->setStyleSheet("background-color: #3daee9; color: white;");

    // Redirect ALL keyboard events to this dialog while listening - otherwise
    // arrow keys/Tab are consumed by Qt focus navigation on the focused
    // button and never reach keyPressEvent().
    grabKeyboard();

    if (m_joystickCombo && m_joystickCombo->isEnabled() && m_joystickCombo->currentIndex() >= 0) {
        std::uint32_t instanceId = m_joystickCombo->currentData().toUInt();
        int port = m_joystickCombo->currentIndex();
        for (const DetectedJoystick& js : m_detectedJoysticks) {
            if (js.instanceId == instanceId) { port = js.port; break; }
        }

        const bool analogAxis = is_analog_input_definition(
            section.toStdString(), key.toStdString());
        const JoystickCaptureMode mode = analogAxis
            ? JoystickCaptureMode::AnalogAxis
            : JoystickCaptureMode::Digital;
        m_jsThread = new JoystickListener(instanceId, port, mode, this);
        connect(m_jsThread, &JoystickListener::mapped, this, &SettingsDialog::onJoystickMapped);
        connect(m_jsThread, &QThread::finished, this, [this]() {
            auto* finished = qobject_cast<JoystickListener*>(sender());
            if (finished == m_jsThread) m_jsThread = nullptr;
            if (finished) finished->deleteLater();
        });
        m_jsThread->start();
    }
}

void SettingsDialog::onJoystickMapped(QString code) {
    applyMapping(code);
}

void SettingsDialog::stopJoystickListener() {
    if (!m_jsThread) return;

    JoystickListener* listener = m_jsThread;
    m_jsThread = nullptr;
    listener->stop();
}

void SettingsDialog::cancelListening() {
    stopJoystickListener();

    if (m_listeningBtn) {
        setButtonDisplayText(m_listeningBtn, m_listeningDisplay);
        m_listeningBtn->setToolTip(m_listeningTooltip);
        resetButtonStyle(m_listeningBtn);
    }

    m_listeningBtn = nullptr;
    m_listeningSection.clear();
    m_listeningKey.clear();
    m_listeningDisplay.clear();
    m_listeningTooltip.clear();
    if (QWidget::keyboardGrabber() == this) releaseKeyboard();
}

void SettingsDialog::applyMapping(const QString& code, const QString& displayNameIn) {
    if (!m_listeningBtn) return;

    QString stored = code;
    QString display;
    if (!displayNameIn.isEmpty()) {
        display = QString("%1 (%2)").arg(displayNameIn, code);
    } else if (code.startsWith("j") || code.startsWith("mb")) {
        display = code;
    } else {
        bool ok = false;
        int num = code.toInt(&ok);
        if (ok) {
            auto& names = sdl_scancode_names();
            auto it = names.find(num);
            QString name = it != names.end() ? QString::fromStdString(it->second) : code;
            display = QString("%1 (%2)").arg(name).arg(num);
        } else {
            display = code;
        }
    }

    setButtonDisplayText(m_listeningBtn, display);
    m_listeningBtn->setProperty("input_value", stored);
    m_listeningBtn->setToolTip(inputMappingTooltip(stored));
    resetButtonStyle(m_listeningBtn);
    stopJoystickListener();
    m_listeningBtn = nullptr;
    m_listeningSection.clear();
    m_listeningKey.clear();
    m_listeningDisplay.clear();
    m_listeningTooltip.clear();
    if (QWidget::keyboardGrabber() == this) releaseKeyboard();
}

bool SettingsDialog::event(QEvent* e) {
    // While listening, consume KeyPress here so Qt does not use arrows/Tab
    // for focus navigation before keyPressEvent() gets a chance to run.
    if (m_listeningBtn && e->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(e));
        return true;
    }
    return QDialog::event(e);
}

void SettingsDialog::keyPressEvent(QKeyEvent* event) {
    if (!m_listeningBtn) {
        QDialog::keyPressEvent(event);
        return;
    }

    int key = event->key();
    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }

    if (is_analog_input_definition(m_listeningSection.toStdString(),
                                   m_listeningKey.toStdString())) {
        // JGRF deliberately refuses digital inputs for emulated axes. Keep
        // listening for a physical analog joystick axis rather than saving a
        // keyboard/button value that JGRF will ignore.
        setButtonDisplayText(m_listeningBtn, "Move analog axis...");
        m_listeningBtn->setToolTip(
            "Analog axis required. Keyboard keys, buttons and hats cannot be assigned here.");
        return;
    }

    std::optional<int> sdl;
    if (event->modifiers() & Qt::KeypadModifier) {
        auto& kp = qt_keypad_to_sdl_scancode();
        auto it = kp.find(key);
        if (it != kp.end()) sdl = it->second;
    }
    if (!sdl.has_value()) {
        auto& main = qt_key_to_sdl_scancode();
        auto it = main.find(key);
        if (it != main.end()) sdl = it->second;
    }
    if (!sdl.has_value() && !event->text().isEmpty()) {
        QString alias = event->text().toLower().replace(" ", "");
        auto& n2s = name_to_sdl_scancode();
        auto it = n2s.find(alias.toStdString());
        if (it != n2s.end()) sdl = it->second;
    }

    if (sdl.has_value()) {
        auto& names = sdl_scancode_names();
        auto it = names.find(*sdl);
        QString name = it != names.end() ? QString::fromStdString(it->second) : QString::number(*sdl);
        applyMapping(QString::number(*sdl), name);
    } else {
        QString name = QKeySequence(key).toString().toLower();
        applyMapping(name);
    }
}

void SettingsDialog::saveInputConfig() {
    cancelListening();

    IniDocument config;
    config.load(m_paths.input_config); // preserve mappings for other input devices

    for (const InputRow& row : m_inputEntries) {
        std::string section = row.section.toStdString();
        std::string key = row.key.toStdString();

        // drop stale duplicate raw keys that normalize to this canonical key
        for (const std::string& rawKey : config.options(section)) {
            if (rawKey != key && canonicalInputKey(section, rawKey) == key) {
                config.remove_option(section, rawKey);
            }
        }

        QVariant prop = row.button->property("input_value");
        QString value = prop.isValid() ? prop.toString() : QString();
        if (value.isEmpty()) {
            value = buttonDisplayText(row.button);
        }
        if (value.isEmpty() || value == "--") continue;

        config.set(section, key, value.toStdString());
    }

    for (const std::string& section : config.sections()) {
        if (config.options(section).empty()) {
            config.remove_section(section);
        }
    }

    std::error_code ec;
    fs::create_directories(m_paths.input_config.parent_path(), ec);
    config.save(m_paths.input_config);

    if (m_inputDeviceCombo) {
        const int profile = currentInputProfile();

        // Geolith only accepts input=0..3. V-Liner and Irritating Maze are
        // auto-detected from the loaded game's flags, so their frontend
        // profiles must use input=0 (Auto), never invalid values 4/5.
        IniDocument coreCfg;
        coreCfg.load(m_paths.geolith_ini);
        coreCfg.set("geolith", "input",
                    std::to_string(geolith_input_setting_for_device(profile)));
        coreCfg.save(m_paths.geolith_ini);

        // Persist Goliath's richer profile separately so V-Liner/Irrmaze stay
        // selected when the Settings dialog is reopened.
        m_config.set("UI", "input_device", std::to_string(profile));
        save_config(m_config);
    }

}

void SettingsDialog::resetInputConfig() {
    cancelListening();

    int device = currentInputProfile();
    QStringList sections = targetInputSections(device);

    auto reply = QMessageBox::question(this, "Reset Input Config",
        "Reset the JGRF Axis Deadzone and all key/button mappings for the "
        "currently selected input device to their defaults? This does not "
        "affect mappings for other input devices.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    IniDocument config;
    config.load(m_paths.input_config); // preserve mappings for other input devices

    for (const QString& qsection : sections) {
        std::string section = qsection.toStdString();
        auto keysIt = input_defs().find(section);
        if (keysIt == input_defs().end()) continue;

        auto defIt = default_input_config().find(section);
        for (const std::string& key : keysIt->second) {
            std::string value;
            if (defIt != default_input_config().end()) {
                auto kIt = defIt->second.find(key);
                if (kIt != defIt->second.end()) value = kIt->second;
            }
            config.set(section, key, value);
        }
    }

    std::error_code ec;
    fs::create_directories(m_paths.input_config.parent_path(), ec);
    config.save(m_paths.input_config);

    // "Reset to Defaults" applies to the whole Input tab. Axis Deadzone is
    // a global JGRF input setting, while the mappings above are profile-specific.
    if (m_axisDeadzoneSpin) m_axisDeadzoneSpin->setValue(kDefaultAxisDeadzone);
    persistJgrfInputSettings(kDefaultAxisDeadzone);

    loadInputConfig();
}

void SettingsDialog::browsePath(const std::string& key, const QString& label) {
    const QString current = QString::fromStdString(resolve_path(m_config, key).string());
    const QString path = QFileDialog::getExistingDirectory(
        this, "Select " + label,
        current.isEmpty() ? QString::fromStdString(m_paths.base_dir.string()) : current);

    if (!path.isEmpty()) {
        m_pathEntries[key]->setText(path);
    }
}

void SettingsDialog::savePathConfig() {
    for (const auto& ps : pathSettings()) {
        auto it = m_pathEntries.find(ps.key);
        if (it == m_pathEntries.end()) continue;
        m_config.set("Paths", ps.key, relativePath(it->second->text()).toStdString());
    }
    save_config(m_config);

}

QString SettingsDialog::relativePath(const QString& path) const {
    std::string base = m_paths.base_dir.string();
    std::string p = path.toStdString();
    std::string prefix = base + std::string(1, (char)fs::path::preferred_separator);
    if (p.starts_with(prefix)) {
        std::error_code ec;
        fs::path rel = fs::relative(fs::path(p), m_paths.base_dir, ec);
        if (!ec) return QString::fromStdString(rel.generic_string());
    }
    return path;
}

void SettingsDialog::closeEvent(QCloseEvent* event) {
    cancelListening();
    QDialog::closeEvent(event);
}

} // namespace goliath
