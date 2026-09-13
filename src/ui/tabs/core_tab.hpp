// core_tab.hpp — Settings dialog tab for the geolith core's
// system/emulation settings (geolith.ini), grouped into System/Input/
// Memory Card/Misc group boxes.
#pragma once

#include <QWidget>
#include <filesystem>

#include "ui/widgets/settings_field.hpp"

namespace goliath {

class CoreTab : public QWidget {
    Q_OBJECT
public:
    explicit CoreTab(std::filesystem::path geolithIni, QWidget* parent = nullptr);

private slots:
    void save();

private:
    void setupUi();
    void load();

    std::filesystem::path m_geolithIni;
    FieldWidgetSet m_widgets;
};

} // namespace goliath
