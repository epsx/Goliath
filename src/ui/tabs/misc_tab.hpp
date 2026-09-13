// misc_tab.hpp — Settings dialog tab for jollygood's misc settings
// (settings.ini [misc] section).
#pragma once

#include <QWidget>
#include <filesystem>

#include "ui/widgets/settings_field.hpp"

namespace goliath {

class MiscTab : public QWidget {
    Q_OBJECT
public:
    MiscTab(std::filesystem::path jollygoodSettingsIni, std::filesystem::path geolithIni,
            QWidget* parent = nullptr);

private slots:
    void save();
    void resetDefaults();

private:
    void setupUi();
    void load();

    std::filesystem::path m_jollygoodSettingsIni;
    std::filesystem::path m_geolithIni;
    FieldWidgetSet m_widgets;
};

} // namespace goliath
