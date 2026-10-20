// verification_report_dialog.hpp - complete, current library verification
// overview opened from actionable rescan notifications.
#pragma once

#include "game/library_rescan_comparison.hpp"

#include <QDialog>

#include <vector>

class QLabel;
class QVBoxLayout;

namespace goliath {

class VerificationReportDialog final : public QDialog {
    Q_OBJECT
public:
    explicit VerificationReportDialog(const std::vector<Game>& games,
                                      QWidget* parent = nullptr);

private:
    void showEntries(LibraryVerificationStatus status);

    std::vector<LibraryVerificationEntry> m_entries;
    QLabel* m_entryHeading = nullptr;
    QVBoxLayout* m_entryLayout = nullptr;
};

} // namespace goliath
