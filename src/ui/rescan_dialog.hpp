// Modal progress dialog for RescanWorker. Shows live status, an indeterminate
// progress bar, and a read-only summary or error when scanning finishes.
#pragma once

#include <QDialog>
#include <QStringList>

class QLabel;
class QProgressBar;
class QTextEdit;
class QPushButton;

namespace goliath {

class RescanWorker;

class RescanDialog : public QDialog {
    Q_OBJECT
public:
    explicit RescanDialog(RescanWorker* worker, QWidget* parent = nullptr);

private slots:
    void onOutput(QString line);
    void onCompleted();
    void onError(QString msg);

private:
    RescanWorker* m_worker;
    QLabel* m_statusLabel;
    QProgressBar* m_progress;
    QTextEdit* m_summaryText;
    QPushButton* m_closeBtn;

    QStringList m_outputLines;
    bool m_summaryStarted = false;
    QStringList m_summaryLines;
};

} // namespace goliath
