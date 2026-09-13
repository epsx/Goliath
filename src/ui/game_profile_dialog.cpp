#include "ui/game_profile_dialog.hpp"

#include "game/jollygood_capabilities.hpp"
#include "game/jollygood_executable.hpp"
#include "input/sdl_joystick.hpp"
#include "ui/widgets/game_input_mapping_widget.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace goliath {

namespace {

constexpr int kInherit = -1;

using Option = std::pair<int, QString>;

QString value_name(int value, const std::vector<Option>& options) {
    for (const auto& [candidate, text] : options) {
        if (candidate == value) return text;
    }
    return QString::number(value);
}

QComboBox* add_optional_combo(QFormLayout* form,
                              const QString& label,
                              int globalValue,
                              const std::vector<Option>& options) {
    auto* combo = new QComboBox();
    combo->addItem(QString("Inherit global (%1)")
                       .arg(value_name(globalValue, options)),
                   kInherit);
    for (const auto& [value, text] : options)
        combo->addItem(text, value);
    form->addRow(label + ":", combo);
    return combo;
}

QComboBox* add_optional_bool(QFormLayout* form,
                             const QString& label,
                             int globalValue) {
    return add_optional_combo(form, label, globalValue,
                              {{0, "Off"}, {1, "On"}});
}

void select_optional(QComboBox* combo, const std::optional<int>& value) {
    if (!combo) return;
    const int wanted = value.value_or(kInherit);
    const int index = combo->findData(wanted);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void select_optional(QComboBox* combo, const std::optional<bool>& value) {
    if (!combo) return;
    const int wanted = value.has_value() ? (*value ? 1 : 0) : kInherit;
    const int index = combo->findData(wanted);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

std::optional<int> optional_int(const QComboBox* combo) {
    if (!combo) return std::nullopt;
    const int value = combo->currentData().toInt();
    return value == kInherit ? std::nullopt : std::optional<int>(value);
}

std::optional<bool> optional_bool(const QComboBox* combo) {
    const std::optional<int> value = optional_int(combo);
    return value.has_value() ? std::optional<bool>(*value != 0) : std::nullopt;
}

int effective_combo_value(const QComboBox* combo, int globalValue) {
    const std::optional<int> value = optional_int(combo);
    return value.value_or(globalValue);
}

const std::vector<Option>& cartridge_system_options() {
    static const std::vector<Option> options = {
        {0, "AES (Console)"},
        {1, "MVS (Arcade)"},
        {2, "Universe BIOS"},
    };
    return options;
}

const std::vector<Option>& cd_system_options() {
    static const std::vector<Option> options = {
        {0, "Neo Geo CD (Front Loader)"},
        {1, "Neo Geo CD (Top Loader)"},
        {2, "Neo Geo CDZ"},
        {3, "Universe BIOS"},
    };
    return options;
}

const std::vector<Option>& universe_hardware_options() {
    static const std::vector<Option> options = {
        {0, "AES (Console)"},
        {1, "MVS (Arcade)"},
    };
    return options;
}

const std::vector<Option>& region_options() {
    static const std::vector<Option> options = {
        {0, "US"}, {1, "JP"}, {2, "AS"}, {3, "EU"},
    };
    return options;
}

int global_jgrf_video_value(const GameProfileGlobalSettings& global,
                            const VideoSettingSpec& spec) {
    const std::string_view key(spec.key);
    if (key == "api") return global.video_api;
    if (key == "shader") return global.shader;
    return video_setting_value(global.jgrf_video, spec);
}

int global_geolith_video_value(const GameProfileGlobalSettings& global,
                               const VideoSettingSpec& spec) {
    return video_setting_value(global.geolith_video, spec);
}

} // namespace

GameProfileDialog::GameProfileDialog(QString displayName,
                                     std::string system,
                                     std::string media,
                                     AppPaths paths,
                                     GameProfileGlobalSettings global,
                                     const GameLaunchProfile* existing,
                                     QWidget* parent)
    : QDialog(parent),
      m_system(std::move(system)),
      m_media(std::move(media)),
      m_paths(std::move(paths)),
      m_global(global) {
    if (existing) m_initialVideoApi = existing->video_api;
    resize(820, 780);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Per-game Settings", &layout, true, false);

    auto* heading = new QLabel(displayName);
    heading->setTextFormat(Qt::PlainText);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setPointSize(headingFont.pointSize() + 1);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto* mediaLabel = new QLabel(QString("Profile target: %1")
                                      .arg(QString::fromStdString(m_media)));
    mediaLabel->setTextFormat(Qt::PlainText);
    mediaLabel->setWordWrap(true);
    mediaLabel->setToolTip(
        "Profiles are keyed by system and relative media path. Parent ROMs, "
        "variants, CUE files and CHD files can therefore use different settings.");
    layout->addWidget(mediaLabel);

    auto* tabs = new QTabWidget();
    auto* generalTab = new QWidget(tabs);
    auto* generalTabLayout = new QVBoxLayout(generalTab);
    generalTabLayout->setContentsMargins(0, 0, 0, 0);

    auto* generalScroll = new QScrollArea(generalTab);
    generalScroll->setWidgetResizable(true);
    auto* generalContainer = new QWidget(generalScroll);
    auto* generalLayout = new QVBoxLayout(generalContainer);
    generalLayout->setContentsMargins(10, 10, 10, 10);
    generalLayout->setSpacing(10);

    auto* systemBox = new QGroupBox("System / BIOS");
    auto* systemForm = new QFormLayout(systemBox);
    if (m_system == "neogeo") {
        m_cartridgeSystem = add_optional_combo(
            systemForm, "Cartridge System", m_global.cartridge_system,
            cartridge_system_options());
        m_universeHardware = add_optional_combo(
            systemForm, "Universe BIOS Hardware", m_global.universe_hardware,
            universe_hardware_options());
    } else {
        m_cdSystem = add_optional_combo(
            systemForm, "CD System", m_global.cd_system,
            cd_system_options());
    }
    m_region = add_optional_combo(
        systemForm, "Region", m_global.region, region_options());
    m_freePlay = add_optional_bool(
        systemForm, "Free Play", m_global.free_play);
    m_settingMode = add_optional_bool(
        systemForm, "Setting Mode", m_global.setting_mode);
    generalLayout->addWidget(systemBox);

    auto* inputBox = new QGroupBox("Input");
    auto* inputForm = new QFormLayout(inputBox);
    std::vector<Option> inputOptions = {
        {0, "Auto"},
        {1, "Neo Geo Joysticks"},
    };
    if (m_system == "neogeo") {
        inputOptions.push_back({2, "Mahjong"});
        inputOptions.push_back({3, "4-Player (JP/AS MVS only)"});
    }
    m_inputDevice = add_optional_combo(
        inputForm, "Emulated Input", m_global.input_device, inputOptions);

    m_controllerPort = new QComboBox();
    m_controllerPort->addItem("Inherit global mappings", kInherit);
    const std::vector<DetectedJoystick> joysticks = list_joysticks();
    for (const DetectedJoystick& joystick : joysticks) {
        if (joystick.port < 0 || joystick.port > 9) continue;
        m_controllerPort->addItem(
            QString("Port %1: %2").arg(joystick.port + 1).arg(joystick.name),
            joystick.port);
    }
    inputForm->addRow("Player 1 Controller:", m_controllerPort);
    m_controllerPort->setToolTip(
        "Retargets inherited Player 1/single-player joystick bindings to the "
        "selected JGRF SDL port. JGRF assigns j0, j1, ... by current connection "
        "order; reconnecting controllers in another order can change the device.");
    generalLayout->addWidget(inputBox);

    m_constraintNote = new QLabel();
    m_constraintNote->setWordWrap(true);
    m_constraintNote->setObjectName("themed_note");
    generalLayout->addWidget(m_constraintNote);
    generalLayout->addStretch();

    generalScroll->setWidget(generalContainer);
    generalTabLayout->addWidget(generalScroll);

    tabs->addTab(generalTab, "General");

    auto* videoTab = new QWidget(tabs);
    auto* videoLayout = new QVBoxLayout(videoTab);
    videoLayout->setContentsMargins(0, 0, 0, 0);

    auto* videoScroll = new QScrollArea(videoTab);
    videoScroll->setWidgetResizable(true);
    auto* videoContainer = new QWidget(videoScroll);
    auto* videoContainerLayout = new QVBoxLayout(videoContainer);
    videoContainerLayout->setContentsMargins(10, 10, 10, 10);
    videoContainerLayout->setSpacing(10);

    auto* frontendBox = new QGroupBox("JollyGood Renderer / Window");
    auto* frontendForm = new QFormLayout(frontendBox);
    m_crteaGroup = new QGroupBox("CRTea Shader");
    auto* crteaForm = new QFormLayout(m_crteaGroup);

    for (const VideoSettingSpec& spec : jgrf_video_setting_specs()) {
        const int globalValue = global_jgrf_video_value(m_global, spec);
        const std::string_view key(spec.key);
        QFormLayout* target = key.starts_with("crtea_")
                                  ? crteaForm
                                  : frontendForm;
        m_jgrfVideoFields.addToForm(target, spec, globalValue);
    }

    m_videoApi = qobject_cast<QComboBox*>(
        m_jgrfVideoFields.widget("api"));
    m_shader = qobject_cast<QComboBox*>(
        m_jgrfVideoFields.widget("shader"));
    const int initialVulkanIndex = m_videoApi->findData(3);
    if (initialVulkanIndex >= 0)
        m_videoApi->removeItem(initialVulkanIndex);
    m_videoApi->setEnabled(false);
    m_videoApi->setToolTip(
        "The per-game value is passed as JGRF --video after the global launch "
        "arguments, so it affects only this game. Vulkan is offered only when "
        "the configured JGRF executable advertises support.");
    m_shader->setToolTip(
        "The per-game value is passed as JGRF --shader after the global launch "
        "arguments, so it affects only this game. Value 0 is Nearest Neighbour "
        "and is the no-filter option.");

    m_videoApiNote = new QLabel("Checking JGRF video capabilities...");
    m_videoApiNote->setWordWrap(true);
    m_videoApiNote->setObjectName("secondary_text");
    frontendForm->addRow(m_videoApiNote);
    videoContainerLayout->addWidget(frontendBox);
    videoContainerLayout->addWidget(m_crteaGroup);

    auto* geolithVideoBox = new QGroupBox("Geolith Core Video");
    auto* geolithVideoForm = new QFormLayout(geolithVideoBox);
    for (const VideoSettingSpec& spec : geolith_video_setting_specs()) {
        m_geolithVideoFields.addToForm(
            geolithVideoForm, spec,
            global_geolith_video_value(m_global, spec));
    }
    videoContainerLayout->addWidget(geolithVideoBox);

    m_crteaNote = new QLabel();
    m_crteaNote->setWordWrap(true);
    m_crteaNote->setObjectName("themed_note");
    videoContainerLayout->addWidget(m_crteaNote);

    auto* resetVideoButton = new QPushButton("Reset Video to Global");
    resetVideoButton->setToolTip(
        "Clears only this profile's video overrides. System, input and exact-"
        "media mappings remain unchanged until Save Profile is pressed.");
    videoContainerLayout->addWidget(resetVideoButton);
    videoContainerLayout->addStretch();
    videoScroll->setWidget(videoContainer);
    videoLayout->addWidget(videoScroll);
    tabs->addTab(videoTab, "Video");

    m_inputMapping = new GameInputMappingWidget(
        m_paths, m_system,
        existing ? existing->input_overrides : GameInputOverrides{}, tabs);
    tabs->addTab(m_inputMapping, "Input Mapping");
    layout->addWidget(tabs, 1);

    auto* buttons = new QHBoxLayout();
    auto* saveButton = new QPushButton("Save Profile");
    auto* resetButton = new QPushButton("Reset to Global");
    auto* cancelButton = new QPushButton("Cancel");
    buttons->addWidget(saveButton);
    buttons->addWidget(resetButton);
    buttons->addStretch();
    buttons->addWidget(cancelButton);
    layout->addLayout(buttons);

    connect(saveButton, &QPushButton::clicked, this,
            [this]() { validateAndAccept(); });
    connect(resetButton, &QPushButton::clicked, this,
            [this]() { resetToGlobal(); });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(resetVideoButton, &QPushButton::clicked, this,
            [this]() { resetVideoToGlobal(); });
    connect(tabs, &QTabWidget::currentChanged, this,
            [this](int) {
                if (m_inputMapping) m_inputMapping->cancelListening();
            });
    connect(this, &QDialog::finished, this,
            [this](int) {
                if (m_inputMapping) m_inputMapping->cancelListening();
            });

    if (m_cartridgeSystem) {
        connect(m_cartridgeSystem,
                QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this]() { updateConstraints(); });
    }
    if (m_universeHardware) {
        connect(m_universeHardware,
                QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this]() { updateConstraints(); });
    }
    connect(m_region, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { updateConstraints(); });
    connect(m_inputDevice,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { updateConstraints(); });
    connect(m_videoApi,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { updateVideoApiNote(); });
    connect(m_shader,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { updateVideoControls(); });
    if (auto* crteaMode = qobject_cast<QComboBox*>(
            m_jgrfVideoFields.widget("crtea_mode"))) {
        connect(crteaMode,
                QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this]() { updateVideoControls(); });
    }

    if (existing) {
        select_optional(m_cartridgeSystem, existing->cartridge_system);
        select_optional(m_cdSystem, existing->cd_system);
        select_optional(m_universeHardware, existing->universe_hardware);
        select_optional(m_region, existing->region);
        VideoSettingValues frontendVideo = existing->jgrf_video;
        if (existing->video_api.has_value() && *existing->video_api != 3)
            frontendVideo["api"] = *existing->video_api;
        if (existing->shader.has_value())
            frontendVideo["shader"] = *existing->shader;
        m_jgrfVideoFields.setOverrides(frontendVideo);
        m_geolithVideoFields.setOverrides(existing->geolith_video);
        select_optional(m_inputDevice, existing->input_device);
        select_optional(m_freePlay, existing->free_play);
        select_optional(m_settingMode, existing->setting_mode);

        if (existing->controller_port.has_value()) {
            int index = m_controllerPort->findData(*existing->controller_port);
            if (index < 0) {
                m_controllerPort->addItem(
                    QString("Port %1 (currently unavailable)")
                        .arg(*existing->controller_port + 1),
                    *existing->controller_port);
                index = m_controllerPort->count() - 1;
            }
            m_controllerPort->setCurrentIndex(index);
        }
    }

    updateConstraints();
    updateVideoControls();
    QTimer::singleShot(0, this,
                       [this]() { probeJollygoodVideoCapabilities(); });
}

GameLaunchProfile GameProfileDialog::profile() const {
    GameLaunchProfile result;
    if (m_system == "neogeo") {
        result.cartridge_system = optional_int(m_cartridgeSystem);
        const int cartridgeSystem =
            effective_combo_value(m_cartridgeSystem, m_global.cartridge_system);
        if (cartridgeSystem == 2)
            result.universe_hardware = optional_int(m_universeHardware);
    } else {
        result.cd_system = optional_int(m_cdSystem);
    }

    result.region = optional_int(m_region);
    result.video_api = m_jgrfVideoFields.overrideValue("api");
    result.shader = m_jgrfVideoFields.overrideValue("shader");
    result.jgrf_video = m_jgrfVideoFields.overrides();
    result.jgrf_video.erase("api");
    result.jgrf_video.erase("shader");
    result.geolith_video = m_geolithVideoFields.overrides();
    result.input_device = optional_int(m_inputDevice);
    result.controller_port = optional_int(m_controllerPort);
    if (m_inputMapping)
        result.input_overrides = m_inputMapping->overrides();

    GameLaunchProfile effective = result;
    if (game_profile_uses_mvs_hardware(m_system, effective, m_global)) {
        result.free_play = optional_bool(m_freePlay);
        result.setting_mode = optional_bool(m_settingMode);
    }
    return result;
}

void GameProfileDialog::probeJollygoodVideoCapabilities() {
    if (m_capabilityProcess) return;

    const std::filesystem::path executable =
        resolve_jollygood_executable(m_paths.jollygood_exe);
    if (executable.empty()) {
        applyVulkanCapability(false, false);
        return;
    }

    auto* process = new QProcess(this);
    auto timedOut = std::make_shared<bool>(false);
    m_capabilityProcess = process;
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::finished, this,
            [this, process, timedOut](int, QProcess::ExitStatus) {
                if (m_capabilityProcess != process) return;
                const QByteArray output = process->readAll();
                const std::string help(
                    output.constData(), static_cast<std::size_t>(output.size()));
                m_capabilityProcess = nullptr;
                applyVulkanCapability(
                    !*timedOut,
                    !*timedOut && jgrf_help_reports_vulkan(help));
                process->deleteLater();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart ||
                    m_capabilityProcess != process) {
                    return;
                }
                m_capabilityProcess = nullptr;
                applyVulkanCapability(false, false);
                process->deleteLater();
            });

    process->start(QString::fromStdString(executable.string()), {"--help"});
    QTimer::singleShot(kJgrfHelpProbeTimeoutMs, process,
                       [process, timedOut]() {
        if (process->state() == QProcess::NotRunning) return;
        *timedOut = true;
        process->kill();
    });
}

void GameProfileDialog::applyVulkanCapability(bool probeSucceeded,
                                               bool available) {
    m_vulkanProbeComplete = true;
    m_vulkanProbeSucceeded = probeSucceeded;
    m_vulkanAvailable = probeSucceeded && available;
    m_videoApi->setEnabled(true);

    int vulkanIndex = m_videoApi->findData(3);
    if (m_vulkanAvailable) {
        if (vulkanIndex < 0) {
            m_videoApi->addItem("Vulkan (Experimental)", 3);
            vulkanIndex = m_videoApi->findData(3);
        }
        if (m_initialVideoApi == std::optional<int>(3))
            m_videoApi->setCurrentIndex(vulkanIndex);
    } else if (m_initialVideoApi == std::optional<int>(3)) {
        if (vulkanIndex < 0) {
            m_videoApi->addItem("Vulkan (currently unavailable)", 3);
            vulkanIndex = m_videoApi->findData(3);
        }
        m_videoApi->setCurrentIndex(vulkanIndex);
        if (auto* model = qobject_cast<QStandardItemModel*>(m_videoApi->model())) {
            if (QStandardItem* item = model->item(vulkanIndex))
                item->setEnabled(false);
        }
    } else if (vulkanIndex >= 0) {
        m_videoApi->removeItem(vulkanIndex);
    }

    updateVideoApiNote();
}

void GameProfileDialog::updateVideoApiNote() {
    if (!m_videoApiNote) return;

    if (!m_vulkanProbeComplete) {
        m_videoApiNote->setText("Checking JGRF video capabilities...");
        m_videoApiNote->show();
        return;
    }

    if (!m_vulkanProbeSucceeded) {
        QString text =
            "Could not detect Vulkan support from the configured JGRF "
            "executable. New per-game Vulkan overrides are unavailable.";
        if (optional_int(m_videoApi) == std::optional<int>(3)) {
            text += " The stored Vulkan override is preserved; select another "
                    "API before saving, or Cancel to keep the profile unchanged.";
        }
        m_videoApiNote->setText(text);
        m_videoApiNote->show();
        return;
    }

    if (!m_vulkanAvailable) {
        QString text =
            "The configured JGRF executable does not advertise Vulkan. "
            "Per-game Vulkan overrides are unavailable.";
        if (optional_int(m_videoApi) == std::optional<int>(3)) {
            text += " The stored Vulkan override is preserved; select another "
                    "API before saving, or Cancel to keep the profile unchanged.";
        }
        m_videoApiNote->setText(text);
        m_videoApiNote->show();
        return;
    }

    if (optional_int(m_videoApi) == std::optional<int>(3)) {
        m_videoApiNote->setText(
            QString::fromUtf8(
                "\xE2\x9A\xA0 Vulkan is experimental in JGRF and requires "
                "the matching shaders/*.spv runtime assets."));
        m_videoApiNote->show();
        return;
    }

    m_videoApiNote->hide();
}

void GameProfileDialog::updateVideoControls() {
    if (!m_crteaGroup || !m_crteaNote) return;

    const int shader = m_jgrfVideoFields.effectiveValue(
        "shader", m_global.shader);
    const VideoSettingSpec* modeSpec = find_video_setting(
        jgrf_video_setting_specs(), "crtea_mode");
    const int globalMode = modeSpec
                               ? global_jgrf_video_value(m_global, *modeSpec)
                               : 2;
    const int mode = m_jgrfVideoFields.effectiveValue(
        "crtea_mode", globalMode);
    const bool crteaActive = shader == 5;
    const bool customMode = crteaActive && mode == 4;

    m_crteaGroup->setEnabled(crteaActive);
    for (const char* key : {
             "crtea_masktype", "crtea_maskstr",
             "crtea_scanstr", "crtea_sharpness"}) {
        m_jgrfVideoFields.setEnabled(key, customMode);
    }

    if (!crteaActive) {
        m_crteaNote->setText(
            "CRTea parameters are preserved but inactive because the effective "
            "shader is not CRTea.");
    } else if (!customMode) {
        m_crteaNote->setText(
            "The selected CRTea preset supplies Mask Type, Mask Strength, "
            "Scanline Strength and Sharpness. Curve, Corner and Trinitron "
            "Curve remain active and may still be overridden for this game.");
    } else {
        m_crteaNote->setText(
            "CRTea Custom mode uses every control below. All values apply only "
            "to this exact ROM, CUE or CHD profile.");
    }
}

void GameProfileDialog::resetVideoToGlobal() {
    m_jgrfVideoFields.resetToInherit();
    m_geolithVideoFields.resetToInherit();
    updateVideoApiNote();
    updateVideoControls();
}

void GameProfileDialog::updateConstraints() {
    GameLaunchProfile current;
    current.cartridge_system = optional_int(m_cartridgeSystem);
    current.universe_hardware = optional_int(m_universeHardware);
    current.region = optional_int(m_region);

    const int cartridgeSystem =
        m_system == "neogeo"
            ? effective_combo_value(m_cartridgeSystem, m_global.cartridge_system)
            : -1;
    if (m_universeHardware)
        m_universeHardware->setEnabled(cartridgeSystem == 2);

    const bool mvsHardware =
        game_profile_uses_mvs_hardware(m_system, current, m_global);
    m_freePlay->setEnabled(mvsHardware);
    m_settingMode->setEnabled(mvsHardware);

    const int input = effective_combo_value(m_inputDevice, m_global.input_device);
    const bool fourPlayerValid = input != 3 ||
        game_profile_supports_four_player(m_system, current, m_global);

    QString note;
    if (m_system == "neogeocd") {
        note = "Neo Geo CD does not use the MVS Free Play/Setting Mode DIP "
               "switches. CD profiles therefore keep those fields disabled.";
    } else if (!mvsHardware) {
        note = "Free Play and Setting Mode require MVS hardware. Select MVS, "
               "or Universe BIOS with MVS hardware, to configure them.";
    } else {
        note = "Free Play and Setting Mode apply to this MVS launch only.";
    }

    if (!fourPlayerValid) {
        note += "\n\n4-Player requires direct MVS mode and JP or AS region.";
    }
    note += "\n\nThe physical controller choice uses SDL connection order "
            "(Port 1 = j0, Port 2 = j1, ...).";
    m_constraintNote->setText(note);
}

void GameProfileDialog::resetToGlobal() {
    if (m_inputMapping) m_inputMapping->clearAllOverrides();
    m_jgrfVideoFields.resetToInherit();
    m_geolithVideoFields.resetToInherit();
    const std::vector<QComboBox*> combos = {
        m_cartridgeSystem, m_cdSystem, m_universeHardware, m_region,
        m_inputDevice, m_controllerPort,
        m_freePlay, m_settingMode,
    };
    for (QComboBox* combo : combos) {
        if (combo) combo->setCurrentIndex(0);
    }
    accept();
}

void GameProfileDialog::validateAndAccept() {
    if (m_inputMapping) m_inputMapping->cancelListening();
    if (!m_vulkanProbeComplete) {
        QMessageBox::information(
            this, "Per-game Settings",
            "Please wait for the JGRF video capability check to finish.");
        return;
    }

    GameLaunchProfile current = profile();
    if (current.video_api == std::optional<int>(3) && !m_vulkanAvailable) {
        QMessageBox::warning(
            this, "Per-game Settings",
            "The configured JGRF executable does not advertise Vulkan support.\n\n"
            "Select Inherit global or an OpenGL API before saving. Cancel keeps "
            "the existing profile unchanged.");
        return;
    }
    // Reject an explicitly requested invalid 4-player override. An inherited
    // global 4-player value is normalized to Auto by the isolated runtime when
    // this profile changes the system/region to an incompatible combination.
    if (current.input_device == std::optional<int>(3) &&
        !game_profile_supports_four_player(m_system, current, m_global)) {
        QMessageBox::warning(
            this, "Per-game Settings",
            "Geolith 4-Player mode requires direct MVS mode and JP or AS region.\n\n"
            "Change the System/Region fields or select another input device.");
        return;
    }
    accept();
}

} // namespace goliath
