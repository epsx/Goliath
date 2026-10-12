// Custom modal progress and compact structured result dialog for ROM rescans.
#pragma once

#include "game/rescan_worker.hpp"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QTextEdit;
class QVBoxLayout;
class QWidget;

namespace goliath {

class RescanDialog : public QDialog {
    Q_OBJECT
public:
    explicit RescanDialog(RescanWorker* worker, QWidget* parent = nullptr);

private slots:
    void onOutput(QString line);
    void onCompleted(goliath::RescanReport report);
    void onError(QString msg);

private:
    void populateReport(const RescanReport& report);

    QLabel* m_statusLabel;
    QProgressBar* m_progress;
    QWidget* m_reportContainer;
    QVBoxLayout* m_reportLayout;
    QTextEdit* m_errorText;
    QPushButton* m_changesButton;
    QWidget* m_changesDetails;
    QPushButton* m_technicalButton;
    QWidget* m_technicalDetails;
    QPushButton* m_closeBtn;
};

} // namespace goliath
