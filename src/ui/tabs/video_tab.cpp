#include "video_tab.hpp"
#include "game/jollygood_capabilities.hpp"
#include "game/jollygood_executable.hpp"
#include "game/video_settings.hpp"
#include "ini/ini_document.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

using Kind = FieldSpec::Kind;

FieldSpec qt_field_spec(const VideoSettingSpec& source) {
    FieldSpec result;
    result.key = source.key;
    result.label = QString::fromUtf8(source.label);
    result.default_value = source.default_value;
    switch (source.kind) {
        case VideoSettingKind::Choice:
            result.kind = Kind::Combo;
            for (const VideoSettingOption& option : source.options) {
                result.combo_options.push_back(
                    {option.value, QString::fromUtf8(option.label)});
            }
            break;
        case VideoSettingKind::Boolean:
            result.kind = Kind::Bool;
            break;
        case VideoSettingKind::Integer:
            result.kind = Kind::Spin;
            result.spin_min = source.minimum;
            result.spin_max = source.maximum;
            break;
    }
    return result;
}

std::vector<FieldSpec> qt_field_specs(
        const std::vector<VideoSettingSpec>& sources) {
    std::vector<FieldSpec> result;
    result.reserve(sources.size());
    for (const VideoSettingSpec& source : sources)
        result.push_back(qt_field_spec(source));
    return result;
}

const std::vector<FieldSpec>& jollygoodVideoFields() {
    static const std::vector<FieldSpec> fields =
        qt_field_specs(jgrf_video_setting_specs());
    return fields;
}

const std::vector<FieldSpec>& geolithVideoFields() {
    static const std::vector<FieldSpec> fields =
        qt_field_specs(geolith_video_setting_specs());
    return fields;
}

} // namespace

VideoTab::VideoTab(fs::path jollygoodSettingsIni,
                   fs::path geolithIni,
                   fs::path jollygoodExe,
                   QWidget* parent)
    : QWidget(parent),
      m_jollygoodSettingsIni(std::move(jollygoodSettingsIni)),
      m_geolithIni(std::move(geolithIni)),
      m_jollygoodExe(std::move(jollygoodExe)) {
    setupUi();
    load();

    // Capability probing is deliberately asynchronous. Settings must appear
    // immediately even if the executable is on a slow disk or a probe fails.
    QTimer::singleShot(0, this, &VideoTab::probeJollygoodCapabilities);
}

void VideoTab::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* container = new QWidget();
    auto* form = new QVBoxLayout(container);

    auto* jgrfGroup = new QGroupBox("JollyGood (jgrf) Video Settings");
    auto* jgrfLayout = new QFormLayout(jgrfGroup);
    for (const FieldSpec& spec : jollygoodVideoFields()) {
        m_jollygoodWidgets.addToForm(jgrfLayout, spec);
    }

    m_videoApiCombo = qobject_cast<QComboBox*>(m_jollygoodWidgets.widget("api"));
    if (m_videoApiCombo) {
        // Vulkan is opt-in at JGRF build time. Keep it hidden until the exact
        // configured executable confirms support via its --help output.
        const int vulkanIndex = m_videoApiCombo->findData(3);
        if (vulkanIndex >= 0) m_videoApiCombo->removeItem(vulkanIndex);
        connect(m_videoApiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &VideoTab::updateVulkanNote);
    }
    form->addWidget(jgrfGroup);

    auto* geoGroup = new QGroupBox("Geolith Core Video Settings");
    auto* geoLayout = new QFormLayout(geoGroup);
    for (const FieldSpec& spec : geolithVideoFields()) {
        m_geolithWidgets.addToForm(geoLayout, spec);
    }
    form->addWidget(geoGroup);

    form->addStretch();
    scroll->setWidget(container);
    layout->addWidget(scroll);

    m_vulkanNote = new QLabel("Checking JGRF video capabilities...");
    m_vulkanNote->setWordWrap(true);
    m_vulkanNote->setObjectName("secondary_text");
    layout->addWidget(m_vulkanNote);

    auto* buttonRow = new QHBoxLayout();
    auto* saveBtn = new QPushButton("Save Video Settings");
    connect(saveBtn, &QPushButton::clicked, this, &VideoTab::save);
    buttonRow->addWidget(saveBtn);

    auto* resetBtn = new QPushButton(QString::fromUtf8("\xE2\x86\xBB Reset to Defaults"));
    connect(resetBtn, &QPushButton::clicked, this, &VideoTab::resetDefaults);
    buttonRow->addWidget(resetBtn);
    layout->addLayout(buttonRow);
}

void VideoTab::load() {
    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);

    // JGRF loads settings.ini first and then applies core-specific frontend
    // overrides from geolith.ini. Reflect the same effective values here.
    IniDocument cfg2;
    cfg2.load(m_geolithIni);

    for (const FieldSpec& spec : jollygoodVideoFields()) {
        int val = spec.default_value;
        if (cfg.has_option("video", spec.key)) {
            try {
                val = normalize_field_value(
                    spec, std::stoi(cfg.get("video", spec.key)));
            } catch (...) {
                val = spec.default_value;
            }
        }

        if (cfg2.has_option("video", spec.key)) {
            try {
                const int overrideValue = std::stoi(cfg2.get("video", spec.key));
                // JGRF keeps the already-loaded global value when a core
                // override is invalid, rather than falling back again.
                if (is_valid_field_value(spec, overrideValue))
                    val = overrideValue;
            } catch (...) {
            }
        }

        if (spec.key == "api") m_loadedVideoApi = val;
        m_jollygoodWidgets.setValue(spec.key, val);
    }

    for (const FieldSpec& spec : geolithVideoFields()) {
        int val = spec.default_value;
        if (cfg2.has_option("geolith", spec.key)) {
            try {
                val = std::stoi(cfg2.get("geolith", spec.key));
            } catch (...) {
                val = spec.default_value;
            }
        }
        val = normalize_field_value(spec, val);
        m_geolithWidgets.setValue(spec.key, val);
    }

    updateVulkanNote();
}

void VideoTab::probeJollygoodCapabilities() {
    if (m_capabilityProcess) return;

    const fs::path probeExe = resolve_jollygood_executable(m_jollygoodExe);
    if (probeExe.empty()) {
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
        const std::string help(output.constData(), static_cast<std::size_t>(output.size()));
        m_capabilityProcess = nullptr;
        applyVulkanCapability(!*timedOut,
                              !*timedOut && jgrf_help_reports_vulkan(help));
        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_capabilityProcess != process) return;
        m_capabilityProcess = nullptr;
        applyVulkanCapability(false, false);
        process->deleteLater();
    });

    process->start(QString::fromStdString(probeExe.string()), {"--help"});
    QTimer::singleShot(kJgrfHelpProbeTimeoutMs, process,
                       [process, timedOut]() {
        if (process->state() == QProcess::NotRunning) return;
        *timedOut = true;
        process->kill();
    });
}

void VideoTab::applyVulkanCapability(bool probeSucceeded, bool available) {
    m_vulkanProbeComplete = true;
    m_vulkanProbeSucceeded = probeSucceeded;
    m_vulkanAvailable = probeSucceeded && available;

    if (m_videoApiCombo) {
        int vulkanIndex = m_videoApiCombo->findData(3);
        if (m_vulkanAvailable) {
            if (vulkanIndex < 0) {
                m_videoApiCombo->addItem("Vulkan (Experimental)", 3);
                vulkanIndex = m_videoApiCombo->findData(3);
            }
            if (m_loadedVideoApi == 3 && vulkanIndex >= 0)
                m_videoApiCombo->setCurrentIndex(vulkanIndex);
        } else if (vulkanIndex >= 0) {
            if (m_videoApiCombo->currentData().toInt() == 3)
                m_videoApiCombo->setCurrentIndex(m_videoApiCombo->findData(0));
            m_videoApiCombo->removeItem(vulkanIndex);
        }
    }

    updateVulkanNote();
}

void VideoTab::updateVulkanNote() {
    if (!m_vulkanNote) return;

    if (!m_vulkanProbeComplete) {
        m_vulkanNote->setText("Checking JGRF video capabilities...");
        m_vulkanNote->show();
        return;
    }

    if (!m_vulkanProbeSucceeded) {
        m_vulkanNote->setText(
            "Could not detect Vulkan support from the configured JGRF executable. "
            "The Vulkan option is hidden for safety.");
        m_vulkanNote->show();
        return;
    }

    if (m_vulkanAvailable && m_videoApiCombo && m_videoApiCombo->currentData().toInt() == 3) {
        m_vulkanNote->setText(
            QString::fromUtf8("\xE2\x9A\xA0 Vulkan is an experimental renderer in JGRF. "
                              "Stability may vary depending on the GPU and driver."));
        m_vulkanNote->show();
        return;
    }

    m_vulkanNote->hide();
}

void VideoTab::save() {
    std::error_code ec;
    fs::create_directories(m_jollygoodSettingsIni.parent_path(), ec);

    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);
    for (const FieldSpec& spec : jollygoodVideoFields()) {
        cfg.set("video", spec.key, std::to_string(m_jollygoodWidgets.value(spec.key)));
    }
    cfg.save(m_jollygoodSettingsIni);

    IniDocument cfg2;
    cfg2.load(m_geolithIni);
    // geolith.ini is a JGRF core-override file. A stale [video] section here
    // overrides settings.ini at launch, so migrate Goliath-managed frontend
    // video settings back to the canonical global file.
    cfg2.remove_section("video");
    for (const FieldSpec& spec : geolithVideoFields()) {
        cfg2.set("geolith", spec.key, std::to_string(m_geolithWidgets.value(spec.key)));
    }
    cfg2.save(m_geolithIni);

}

void VideoTab::resetDefaults() {
    const auto reply = QMessageBox::question(
        this, "Reset Video Settings",
        "Reset JGRF and Geolith video settings to their defaults?\n\n"
        "Other Geolith Core settings are not affected.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    std::error_code ec;
    fs::create_directories(m_jollygoodSettingsIni.parent_path(), ec);

    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);
    for (const FieldSpec& spec : jollygoodVideoFields()) {
        cfg.set("video", spec.key, std::to_string(spec.default_value));
    }
    cfg.save(m_jollygoodSettingsIni);

    IniDocument cfg2;
    cfg2.load(m_geolithIni);
    // geolith.ini is a JGRF core-override file. A stale [video] section here
    // overrides settings.ini at launch, so migrate Goliath-managed frontend
    // video settings back to the canonical global file.
    cfg2.remove_section("video");
    for (const FieldSpec& spec : geolithVideoFields()) {
        cfg2.set("geolith", spec.key, std::to_string(spec.default_value));
    }
    cfg2.save(m_geolithIni);

    load();
}

} // namespace goliath
