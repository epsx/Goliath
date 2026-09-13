// about_dialog.hpp - themed product identity, build information, credits,
// and verified upstream links. Runtime component capabilities remain in
// Settings -> Info.
#pragma once

#include <QDialog>

namespace goliath {

class AboutDialog final : public QDialog {
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

} // namespace goliath
