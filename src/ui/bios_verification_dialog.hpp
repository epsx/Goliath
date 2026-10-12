// Compact custom presentation for a structured BIOS verification result.
#pragma once

#include "game/bios_verify.hpp"

#include <QDialog>

#include <filesystem>

class QPushButton;
class QTextEdit;

namespace goliath {

class BiosVerificationDialog final : public QDialog {
public:
    BiosVerificationDialog(const BiosVerifyResult& result,
                           const std::filesystem::path& biosDir,
                           QWidget* parent = nullptr);

private:
    void scheduleContentResize(bool detailsVisible);

    QPushButton* m_detailsButton = nullptr;
    QTextEdit* m_details = nullptr;
};

} // namespace goliath
