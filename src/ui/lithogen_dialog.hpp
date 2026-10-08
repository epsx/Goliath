#pragma once

#include "common/goliath_common.hpp"

#include <QDialog>
#include <QProcess>
#include <QString>
#include <QStringList>

class QCheckBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QTableWidget;
class QTextEdit;
class QWidget;

namespace goliath {

// Launches a user-supplied Lithogen CLI. No Lithogen code is linked into Goliath.
class LithogenDialog final : public QDialog {
public:
    LithogenDialog(Config& config, const QString& romFolder, QWidget* parent = nullptr);

protected:
    void reject() override;

private:
    void updateInputMode();
    void startConversion();
    void startNextJob();
    void stopConversion();
    void conversionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void finishRun();
    void setJobResult(int row, const QString& result);
    void appendOutput(const QString& text);
    void persistExecutablePath(const QString& path);

    Config& m_config;
    QString m_romFolder;
    QString m_targetDir;
    QString m_executablePath;
    QString m_parentPath;
    QString m_setId;
    QStringList m_jobs;
    QProcess* m_process = nullptr;
    QFormLayout* m_form = nullptr;
    QWidget* m_controls = nullptr;
    QWidget* m_inputRow = nullptr;
    QWidget* m_folderRow = nullptr;
    QWidget* m_advancedFields = nullptr;
    QWidget* m_singleExtras = nullptr;
    QRadioButton* m_single = nullptr;
    QCheckBox* m_advanced = nullptr;
    QCheckBox* m_strict = nullptr;
    QCheckBox* m_autoParent = nullptr;
    QLineEdit* m_executable = nullptr;
    QLineEdit* m_input = nullptr;
    QLineEdit* m_folder = nullptr;
    QLineEdit* m_output = nullptr;
    QLineEdit* m_extraParent = nullptr;
    QLineEdit* m_set = nullptr;
    QTableWidget* m_results = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_summary = nullptr;
    QTextEdit* m_log = nullptr;
    QPushButton* m_convert = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_rescan = nullptr;
    int m_current = 0;
    int m_created = 0;
    int m_failed = 0;
    int m_skipped = 0;
    bool m_active = false;
    bool m_stopRequested = false;
    bool m_useStrict = false;
    bool m_useAutoParent = true;
    bool m_canRescan = false;
    bool m_batch = false;
};

} // namespace goliath
