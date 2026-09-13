// benchmark_dialog.hpp — modal owner for one selected-media JGRF benchmark.
// Unlike normal gameplay, the benchmark process stays attached so Goliath can
// time it, validate its completion marker, cancel it, and present a result.
#pragma once

#include <QByteArray>
#include <QDialog>
#include <QElapsedTimer>
#include <QProcess>
#include <QString>

#include "game/jollygood_launch.hpp"

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTextEdit;

namespace goliath {

class BenchmarkDialog final : public QDialog {
public:
    BenchmarkDialog(QString displayName,
                    QString media,
                    bool profileApplied,
                    int videoApi,
                    int shader,
                    JollygoodLaunchPreparation preparation,
                    QWidget* parent = nullptr);
    ~BenchmarkDialog() override;

public slots:
    void reject() override;

private:
    void startBenchmark();
    void appendProcessOutput();
    void processStarted();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);
    void updateFramePreset();
    void restoreIdleControls();
    void showFailure(const QString& detail);
    void copyResult() const;

    QString m_displayName;
    QString m_media;
    bool m_profileApplied = false;
    int m_videoApi = 0;
    int m_shader = 0;
    JollygoodLaunchPreparation m_preparation;

    QProcess* m_process = nullptr;
    QComboBox* m_preset = nullptr;
    QSpinBox* m_frames = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;
    QTextEdit* m_result = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    QElapsedTimer m_timer;
    QByteArray m_output;
    QString m_resultText;
    int m_requestedFrames = 0;
    bool m_runActive = false;
    bool m_cancelRequested = false;
    bool m_logWriteFailureReported = false;
};

} // namespace goliath
