// game_profile_dialog.hpp — modal editor for one launchable ROM/CUE/CHD.
// Every field supports "Inherit global"; saving an all-inherit profile removes
// the record and returns the launch to the original global configuration.
#pragma once

#include <QDialog>
#include <QString>

#include <optional>
#include <string>

#include "common/paths.hpp"
#include "game/game_profile.hpp"
#include "game/game_profile_runtime.hpp"
#include "ui/widgets/optional_video_fields.hpp"

class QComboBox;
class QLabel;
class QProcess;
class QGroupBox;

namespace goliath {

class GameInputMappingWidget;

class GameProfileDialog : public QDialog {
public:
    GameProfileDialog(QString displayName,
                      std::string system,
                      std::string media,
                      AppPaths paths,
                      GameProfileGlobalSettings global,
                      const GameLaunchProfile* existing,
                      QWidget* parent = nullptr);

    GameLaunchProfile profile() const;

private:
    void probeJollygoodVideoCapabilities();
    void applyVulkanCapability(bool probeSucceeded, bool available);
    void updateVideoApiNote();
    void updateVideoControls();
    void resetVideoToGlobal();
    void updateConstraints();
    void resetToGlobal();
    void validateAndAccept();

    std::string m_system;
    std::string m_media;
    AppPaths m_paths;
    GameProfileGlobalSettings m_global;

    QComboBox* m_cartridgeSystem = nullptr;
    QComboBox* m_cdSystem = nullptr;
    QComboBox* m_universeHardware = nullptr;
    QComboBox* m_region = nullptr;
    QComboBox* m_videoApi = nullptr;
    QComboBox* m_shader = nullptr;
    QComboBox* m_inputDevice = nullptr;
    QComboBox* m_controllerPort = nullptr;
    QComboBox* m_freePlay = nullptr;
    QComboBox* m_settingMode = nullptr;
    OptionalVideoFieldSet m_jgrfVideoFields;
    OptionalVideoFieldSet m_geolithVideoFields;
    QLabel* m_videoApiNote = nullptr;
    QLabel* m_crteaNote = nullptr;
    QGroupBox* m_crteaGroup = nullptr;
    QLabel* m_constraintNote = nullptr;
    QProcess* m_capabilityProcess = nullptr;
    bool m_vulkanProbeComplete = false;
    bool m_vulkanProbeSucceeded = false;
    bool m_vulkanAvailable = false;
    std::optional<int> m_initialVideoApi;
    GameInputMappingWidget* m_inputMapping = nullptr;
};

} // namespace goliath
