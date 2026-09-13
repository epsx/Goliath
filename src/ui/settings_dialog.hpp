// settings_dialog.hpp — the Settings dialog: Video/Audio/Core/Input/Path
// tabs. The Input tab contains its own "Input Device" combo and shows a
// different set of visual controller panels depending on the selected
// device (Neo Geo Joysticks/Mahjong/4-Player/V-Liner/Irritating Maze). It supports remapping via
// keyboard (keyPressEvent, while a button is in "listening" mode) or
// joystick (JoystickListener thread).
#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "common/paths.hpp"
#include "input/sdl_joystick.hpp"

class QCloseEvent;
class QEvent;
class QVBoxLayout;
class QLineEdit;
class QPushButton;
class QComboBox;
class QKeyEvent;
class QTabWidget;
class QSpinBox;

namespace goliath {

class Config;
class JoystickListener;
class InfoTab;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    // config is the *live* app config (same object MainWindow holds) - Path
    // tab edits are only persisted via save_config() when its "Save Path
    // Settings" button is clicked (matches the Video/Audio/Core tabs).
    SettingsDialog(Config& config, AppPaths paths, QWidget* parent = nullptr);
    ~SettingsDialog() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    // Intercepts KeyPress while a mapping button is "listening", before Qt
    // uses the key for focus navigation (arrows/Tab would otherwise never
    // reach keyPressEvent).
    bool event(QEvent* e) override;

private slots:
    void saveInputConfig();
    void saveJgrfInputSettings();
    void resetInputConfig();
    void onJoystickMapped(QString code);
    void onTabChanged(int index);
    void onInputDeviceChanged(int index);

private:
    void buildInputTab(QWidget* inputWidget);
    void buildPathTab(QWidget* pathWidget);
    void loadJgrfInputSettings();
    void persistJgrfInputSettings(int deadzone);

    // Frontend input profile selected in the Input tab. Profile ids 4/5 are
    // Goliath-only choices for V-Liner/Irritating Maze; Geolith itself accepts
    // only input=0..3.
    int currentInputProfile() const;
    // Load the persisted frontend profile, falling back to legacy Geolith
    // input values 1..3 when no Goliath profile has been saved yet.
    int storedInputProfile() const;
    // Which geolith_input.ini sections to show for a given input profile
    // (-1 = use currentInputProfile()).
    QStringList targetInputSections(int device = -1) const;

    void loadInputConfig();
    void createDefaultInputConfig(int device = -1);
    std::string canonicalInputKey(const std::string& section, const std::string& key) const;
    // Returns {stored_value, display_text}.
    std::pair<std::string, std::string> normalizeInputValue(const std::string& value) const;
    // Fallback plain row for any key that doesn't map to a visual panel button.
    void addInputRow(const QString& section, const QString& key, const std::string& value);

    void startListening(QPushButton* btn, const QString& section, const QString& key);
    // display_name empty => derive display text the same way applyMapping()
    // does when called with no override.
    void applyMapping(const QString& code, const QString& displayName = QString());
    void stopJoystickListener();
    void cancelListening();

    // Re-scans connected joysticks via SDL and repopulates m_joystickCombo.
    void refreshJoystickList();

    void browsePath(const std::string& key, const QString& label);
    void savePathConfig();
    QString relativePath(const QString& path) const;

    Config& m_config;
    AppPaths m_paths;

    QTabWidget* m_tabs = nullptr;
    InfoTab* m_infoTab = nullptr;
    QVBoxLayout* m_inputLayout = nullptr;
    QWidget* m_inputContainer = nullptr;
    QComboBox* m_inputDeviceCombo = nullptr;
    QSpinBox* m_axisDeadzoneSpin = nullptr;

    struct InputRow {
        QString section;
        QString key;
        QPushButton* button;
    };
    std::vector<InputRow> m_inputEntries;
    int m_lastInputDevice = -1;

    QPushButton* m_listeningBtn = nullptr;
    QString m_listeningSection;
    QString m_listeningKey;
    QString m_listeningDisplay;
    QString m_listeningTooltip;

    JoystickListener* m_jsThread = nullptr;

    QComboBox* m_joystickCombo = nullptr;
    std::vector<DetectedJoystick> m_detectedJoysticks;

    std::map<std::string, QLineEdit*> m_pathEntries;
};

} // namespace goliath
