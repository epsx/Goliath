#include "audio_tab.hpp"

#include "audio/audio_devices.hpp"
#include "common/goliath_common.hpp"
#include "ini/ini_document.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <memory>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {
using Kind = FieldSpec::Kind;

const std::vector<FieldSpec>& jollygoodAudioFields() {
    static const std::vector<FieldSpec> fields = {
        {"rsqual", "Resampler Quality", 3, Kind::Spin, {}, 0, 10},
    };
    return fields;
}
} // namespace

AudioTab::AudioTab(Config& appConfig, fs::path jollygoodSettingsIni,
                   fs::path geolithIni, QWidget* parent)
    : QWidget(parent), m_appConfig(appConfig),
      m_jollygoodSettingsIni(std::move(jollygoodSettingsIni)),
      m_geolithIni(std::move(geolithIni)) {
    setupUi();
    load();

    // Let the Settings dialog become visible first, then start the potentially
    // slow SDL audio-device scan on a worker thread.
    QTimer::singleShot(0, this, &AudioTab::refreshDevices);
}

void AudioTab::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* group = new QGroupBox("JollyGood Audio Settings");
    auto* form = new QFormLayout(group);

    m_outputCombo = new QComboBox();
    m_outputCombo->addItem("System Default (JGRF stock)", "system_default");
    m_outputCombo->setToolTip(
        "Unmodified JGRF opens SDL's system-default playback device.\n"
        "Goliath can show detected devices, but selecting a specific device\n"
        "would require audio-output support inside JGRF itself.");

    auto* outputRow = new QWidget();
    auto* outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->addWidget(m_outputCombo, 1);
    m_refreshButton = new QPushButton(QString::fromUtf8("\xE2\x86\xBB Refresh")); // ↻ Refresh
    connect(m_refreshButton, &QPushButton::clicked, this, &AudioTab::refreshDevices);
    outputLayout->addWidget(m_refreshButton);
    form->addRow("Output Device:", outputRow);

    m_deviceStatus = new QLabel();
    m_deviceStatus->setWordWrap(true);
    form->addRow("Audio Status:", m_deviceStatus);

    m_deviceList = new QListWidget();
    m_deviceList->setSelectionMode(QAbstractItemView::NoSelection);
    m_deviceList->setMaximumHeight(120);
    form->addRow("Detected Playback Devices:", m_deviceList);

    auto* volumeRow = new QWidget();
    auto* volumeLayout = new QHBoxLayout(volumeRow);
    volumeLayout->setContentsMargins(0, 0, 0, 0);

    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setSingleStep(1);
    m_volumeSlider->setPageStep(5);
    volumeLayout->addWidget(m_volumeSlider, 1);

    m_volumeSpin = new QSpinBox();
    m_volumeSpin->setRange(0, 100);
    m_volumeSpin->setSuffix(" %");
    m_volumeSpin->setFixedWidth(80);
    volumeLayout->addWidget(m_volumeSpin);

    connect(m_volumeSlider, &QSlider::valueChanged, m_volumeSpin, &QSpinBox::setValue);
    connect(m_volumeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_volumeSlider, &QSlider::setValue);

    form->addRow("Volume:", volumeRow);

    auto* volumeInfo = new QLabel();
    volumeInfo->setWordWrap(true);
#if defined(_WIN32)
    volumeInfo->setText(
        "Goliath stores this volume and reapplies it to the JGRF audio session on each launch in Windows. "
        "JGRF itself is not modified.");
#else
    volumeInfo->setText(
        "Goliath stores this volume. Applying per-process JGRF volume is currently supported on Windows only; "
        "JGRF itself remains unmodified.");
#endif
    form->addRow("", volumeInfo);

    for (const FieldSpec& spec : jollygoodAudioFields()) {
        m_widgets.addToForm(form, spec);
    }

    layout->addWidget(group);
    layout->addStretch();

    auto* saveBtn = new QPushButton("Save Audio Settings");
    connect(saveBtn, &QPushButton::clicked, this, &AudioTab::save);
    layout->addWidget(saveBtn);
}

void AudioTab::load() {
    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);

    // JGRF loads settings.ini first, then applies core-specific frontend
    // overrides from geolith.ini. Reflect the effective value in the UI, the
    // same way Video and Misc do.
    IniDocument coreCfg;
    coreCfg.load(m_geolithIni);

    for (const FieldSpec& spec : jollygoodAudioFields()) {
        int val = spec.default_value;
        if (cfg.has_option("audio", spec.key)) {
            try {
                val = normalize_field_value(
                    spec, std::stoi(cfg.get("audio", spec.key)));
            } catch (...) {
                val = spec.default_value;
            }
        }

        if (coreCfg.has_option("audio", spec.key)) {
            try {
                const int overrideValue = std::stoi(coreCfg.get("audio", spec.key));
                if (is_valid_field_value(spec, overrideValue))
                    val = overrideValue;
            } catch (...) {
            }
        }

        m_widgets.setValue(spec.key, val);
    }

    const int volume = clamp_audio_volume(m_appConfig.get_int("Audio", "volume", 100));
    m_volumeSlider->setValue(volume);
    m_volumeSpin->setValue(volume);
}

void AudioTab::refreshDevices() {
    if (m_refreshInProgress) return;

    m_refreshInProgress = true;
    if (m_refreshButton) m_refreshButton->setEnabled(false);
    m_deviceStatus->setText("Detecting playback devices...");
    m_deviceList->clear();
    m_deviceList->addItem("(Scanning...)");

    // SDL audio enumeration can block for several seconds on Windows when an
    // endpoint/driver is slow to respond. Never run it in the Settings dialog
    // constructor thread, otherwise opening Settings appears to hang.
    auto snapshot = std::make_shared<AudioDeviceSnapshot>();
    QThread* worker = QThread::create([snapshot]() {
        *snapshot = query_audio_playback_devices();
    });

    // The receiver context suppresses delivery automatically if Settings is
    // closed before the scan finishes. No QWidget/QPointer is read from the
    // worker thread.
    connect(worker, &QThread::finished, this,
            [this, snapshot]() { applyDeviceSnapshot(*snapshot); });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void AudioTab::applyDeviceSnapshot(const AudioDeviceSnapshot& snapshot) {
    m_deviceList->clear();
    for (const std::string& name : snapshot.playback_devices) {
        m_deviceList->addItem(QString::fromUtf8(name.c_str()));
    }

    if (snapshot.has_playback_device()) {
        m_deviceStatus->setText(
            QString("%1 playback device%2 detected. JGRF will use the system default.")
                .arg(static_cast<int>(snapshot.playback_devices.size()))
                .arg(snapshot.playback_devices.size() == 1 ? "" : "s"));
    } else {
        m_deviceList->addItem("(No playback devices detected)");
        QString status = "No audio playback device is currently available.";
        if (!snapshot.error.empty()) {
            status += QString("\nSDL: %1").arg(QString::fromUtf8(snapshot.error.c_str()));
        }
        m_deviceStatus->setText(status);
    }

    m_refreshInProgress = false;
    if (m_refreshButton) m_refreshButton->setEnabled(true);
}

void AudioTab::save() {
    std::error_code ec;
    fs::create_directories(m_jollygoodSettingsIni.parent_path(), ec);

    // Only write settings that stock JGRF actually understands.
    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);
    for (const FieldSpec& spec : jollygoodAudioFields()) {
        cfg.set("audio", spec.key, std::to_string(m_widgets.value(spec.key)));
    }
    cfg.save(m_jollygoodSettingsIni);

    // Keep settings.ini canonical for frontend settings. A stale [audio]
    // section in geolith.ini would otherwise override the value above.
    IniDocument coreCfg;
    coreCfg.load(m_geolithIni);
    coreCfg.remove_section("audio");
    coreCfg.save(m_geolithIni);

    // Volume is a Goliath-side setting; do not invent unsupported JGRF keys.
    const int volume = clamp_audio_volume(m_volumeSpin->value());
    m_appConfig.set("Audio", "volume", std::to_string(volume));
    save_config(m_appConfig);

}

} // namespace goliath
