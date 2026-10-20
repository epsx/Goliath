// verification_report_dialog.hpp - complete, current library verification
// overview opened from actionable rescan notifications.
#pragma once

#include "game/game_model.hpp"

#include <QDialog>

#include <vector>

namespace goliath {

class VerificationReportDialog final : public QDialog {
    Q_OBJECT
public:
    explicit VerificationReportDialog(const std::vector<Game>& games,
                                      QWidget* parent = nullptr);
};

} // namespace goliath
