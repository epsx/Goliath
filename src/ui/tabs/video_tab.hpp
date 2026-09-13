// video_tab.hpp — Settings dialog tab for both jollygood's and the geolith
// core's video settings (settings.ini and geolith.ini).
#pragma once

#include <QWidget>
#include <filesystem>

#include "ui/widgets/settings_field.hpp"

class QComboBox;
class QLabel;
class QProcess;

namespace goliath {

class VideoTab : public QWidget {
    Q_OBJECT
public:
    VideoTab(std::filesystem::path jollygoodSettingsIni,
             std::filesystem::path geolithIni,
             std::filesystem::path jollygoodExe,
             QWidget* parent = nullptr);

private slots:
    void save();
    void resetDefaults();
    void probeJollygoodCapabilities();
    void updateVulkanNote();

private:
    void setupUi();
    void load();
    void applyVulkanCapability(bool probeSucceeded, bool available);

    std::filesystem::path m_jollygoodSettingsIni;
    std::filesystem::path m_geolithIni;
    std::filesystem::path m_jollygoodExe;

    FieldWidgetSet m_jollygoodWidgets;
    FieldWidgetSet m_geolithWidgets;

    QComboBox* m_videoApiCombo = nullptr;
    QLabel* m_vulkanNote = nullptr;
    QProcess* m_capabilityProcess = nullptr;
    bool m_vulkanProbeComplete = false;
    bool m_vulkanProbeSucceeded = false;
    bool m_vulkanAvailable = false;
    int m_loadedVideoApi = 0;
};

} // namespace goliath
