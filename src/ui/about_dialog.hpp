// about_dialog.hpp - themed product identity, build information, credits,
// and verified upstream links. Runtime component capabilities remain in
// Settings -> Info.
#pragma once

#include <QDialog>

namespace goliath {

class Config;

class AboutDialog final : public QDialog {
public:
    AboutDialog(Config& config, QWidget* parent = nullptr);

private:
    Config& m_config;
};

} // namespace goliath
