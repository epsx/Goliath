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
#include "input/sdl_joystick.hpp"

class QComboBox;
class QEvent;
class QKeyEvent;
class QPushButton;
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
    void cancelListening();
    void clearAllOverrides();

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void rebuild();
    void refreshJoystickList();
    void startListening(QPushButton* button,
                        const QString& section,
                        const QString& key);
    void stopJoystickListener();
    void applyMapping(const QString& value,
                      const QString& displayName = {});
    void resetCurrentProfile();
    void resetOneMapping(const std::string& section,
                         const std::string& key);

    int initialMappingProfile() const;
    QString effectiveValue(const IniDocument& globalConfig,
                           const std::string& section,
                           const std::string& key,
                           bool* overridden) const;
    void updateButtonDisplay(QPushButton* button,
                             const IniDocument& globalConfig,
                             const std::string& section,
                             const std::string& key);

    AppPaths m_paths;
    std::string m_system;
    GameInputOverrides m_overrides;

    QComboBox* m_mappingProfileCombo = nullptr;
    QComboBox* m_joystickCombo = nullptr;
    QVBoxLayout* m_containerLayout = nullptr;
    QWidget* m_container = nullptr;
    std::vector<DetectedJoystick> m_detectedJoysticks;

    QPushButton* m_listeningButton = nullptr;
    QString m_listeningSection;
    QString m_listeningKey;
    QString m_listeningDisplay;
    QString m_listeningTooltip;
    JoystickListener* m_joystickListener = nullptr;
};

} // namespace goliath
