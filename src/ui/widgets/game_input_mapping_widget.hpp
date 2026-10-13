// game_input_mapping_widget.hpp — exact-media input override editor embedded
// in GameProfileDialog. It reads global mappings for inherited display only;
// edits stay in GameLaunchProfile until the dialog is saved.
#pragma once

#include <QString>
#include <QWidget>

#include <string>
#include <vector>

#include "common/paths.hpp"
#include "game/game_profile.hpp"
#include "input/input_maps.hpp"
#include "input/sdl_joystick.hpp"

class QComboBox;
class QCheckBox;
class QEvent;
class QKeyEvent;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace goliath {

class IniDocument;
class JoystickListener;

class GameInputMappingWidget final : public QWidget {
public:
    GameInputMappingWidget(AppPaths paths,
                           std::string system,
                           GameInputOverrides initialOverrides,
                           QWidget* parent = nullptr);
    ~GameInputMappingWidget() override;

    GameInputOverrides overrides() const;
    void setOverrides(GameInputOverrides overrides);
    void cancelListening();
    void clearAllOverrides();

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void rebuild();
    void refreshJoystickList();
    void startListening(QPushButton* button,
                        const std::string& section,
                        const std::string& key);
    void stopJoystickListener();
    void applyMapping(const QString& value,
                      const QString& displayName = {});
    void loadControllerProfile();
    void resetCurrentProfile();
    void resetOneMapping(const std::string& section,
                         const std::string& key);

    int initialMappingProfile() const;
    std::string effectiveValue(const IniDocument& globalConfig,
                               const std::string& section,
                               const std::string& key,
                               bool* overridden) const;
    void updateButtonDisplay(QPushButton* button,
                             const IniDocument& globalConfig,
                             const std::string& section,
                             const std::string& key,
                             const InputBindingConflicts& conflicts);

    AppPaths m_paths;
    std::string m_system;
    GameInputOverrides m_overrides;

    QComboBox* m_mappingProfileCombo = nullptr;
    QCheckBox* m_overridesOnlyCheck = nullptr;
    QComboBox* m_joystickCombo = nullptr;
    QScrollArea* m_scroll = nullptr;
    QVBoxLayout* m_containerLayout = nullptr;
    QWidget* m_container = nullptr;
    std::vector<DetectedJoystick> m_detectedJoysticks;

    QPushButton* m_listeningButton = nullptr;
    std::string m_listeningSection;
    std::string m_listeningKey;
    QString m_listeningDisplay;
    QString m_listeningTooltip;
    JoystickListener* m_joystickListener = nullptr;
};

} // namespace goliath
