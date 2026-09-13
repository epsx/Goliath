// audio_tab.hpp — Settings dialog tab for JGRF's stock audio settings plus
// Goliath-side audio status/volume helpers. JGRF itself remains unmodified.
#pragma once

#include <QWidget>

#include <filesystem>

#include "ui/widgets/settings_field.hpp"

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;

namespace goliath {

struct AudioDeviceSnapshot;
class Config;

class AudioTab : public QWidget {
    Q_OBJECT
public:
    AudioTab(Config& appConfig, std::filesystem::path jollygoodSettingsIni,
             std::filesystem::path geolithIni, QWidget* parent = nullptr);

private slots:
    void save();
    void refreshDevices();

private:
    void setupUi();
    void load();
    void applyDeviceSnapshot(const AudioDeviceSnapshot& snapshot);

    Config& m_appConfig;
    std::filesystem::path m_jollygoodSettingsIni;
    std::filesystem::path m_geolithIni;
    FieldWidgetSet m_widgets;

    QComboBox* m_outputCombo = nullptr;
    QListWidget* m_deviceList = nullptr;
    QLabel* m_deviceStatus = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QSlider* m_volumeSlider = nullptr;
    QSpinBox* m_volumeSpin = nullptr;
    bool m_refreshInProgress = false;
};

} // namespace goliath
