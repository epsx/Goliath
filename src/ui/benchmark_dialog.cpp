#include "ui/benchmark_dialog.hpp"

#include "common/debug_logger.hpp"
#include "common/log_file.hpp"
#include "game/jollygood_benchmark.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QClipboard>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

#include <filesystem>
#include <string_view>
#include <utility>

namespace goliath {

namespace {

QString video_api_name(int value) {
    switch (value) {
    case 0: return "OpenGL Core";
    case 1: return "OpenGL ES";
    case 2: return "OpenGL Compatibility";
    case 3: return "Vulkan (Experimental)";
    default: return QString("Unknown (%1)").arg(value);
    }
}

QString shader_name(int value) {
    switch (value) {
    case 0: return "Nearest Neighbour";
    case 1: return "Linear";
    case 2: return "Sharp Bilinear";
    case 3: return "Anti-Aliased Nearest Neighbour";
    case 4: return "CRT-Yee64";
    case 5: return "CRTea";
    case 6: return "LCD";
    default: return QString("Unknown (%1)").arg(value);
    }
}

QString formatted_frames(std::uint64_t frames) {
    return QLocale().toString(static_cast<qulonglong>(frames));
}

std::filesystem::path qstring_to_path(const QString& path) {
#if defined(_WIN32)
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

} // namespace

BenchmarkDialog::BenchmarkDialog(
        QString displayName,
        QString media,
        bool profileApplied,
        int videoApi,
        int shader,
        JollygoodLaunchPreparation preparation,
        QWidget* parent)
    : QDialog(parent),
      m_displayName(std::move(displayName)),
      m_media(std::move(media)),
      m_profileApplied(profileApplied),
      m_videoApi(videoApi),
      m_shader(shader),
      m_preparation(std::move(preparation)) {
    resize(680, 570);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Performance Benchmark", &layout, true, false);

    auto* heading = new QLabel(m_displayName);
    heading->setTextFormat(Qt::PlainText);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setPointSize(headingFont.pointSize() + 1);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto* target = new QLabel(QString("Benchmark target: %1").arg(m_media));
    target->setTextFormat(Qt::PlainText);
    target->setWordWrap(true);
    layout->addWidget(target);

    auto* configuration = new QLabel(
        QString("Configuration: %1 | %2 | %3")
            .arg(m_profileApplied ? "Exact-media profile" : "Global settings")
            .arg(video_api_name(m_videoApi))
            .arg(shader_name(m_shader)));
    configuration->setTextFormat(Qt::PlainText);
    configuration->setWordWrap(true);
    layout->addWidget(configuration);

    auto* form = new QFormLayout();
    m_preset = new QComboBox();
    m_preset->addItem("Quick (5,000 frames)", 5000);
    m_preset->addItem("Standard (10,000 frames)", kBenchmarkDefaultFrames);
    m_preset->addItem("Extended (30,000 frames)", 30000);
    m_preset->addItem("Custom", 0);
    m_preset->setCurrentIndex(1);
    form->addRow("Run length:", m_preset);

    m_frames = new QSpinBox();
    m_frames->setRange(kBenchmarkMinimumFrames, kBenchmarkMaximumFrames);
    m_frames->setSingleStep(1000);
    m_frames->setGroupSeparatorShown(true);
    m_frames->setSuffix(" frames");
    m_frames->setValue(kBenchmarkDefaultFrames);
    form->addRow("Frames:", m_frames);
    layout->addLayout(form);

    auto* note = new QLabel(
        "JGRF Benchmark Mode bypasses per-frame audio sample processing, "
        "requests OpenGL VSync off, and "
        "Vulkan requests immediate presentation when the driver supports it. "
        "The reported total includes core/media/window startup and process "
        "shutdown, so compare the same game, profile, renderer, and hardware. "
        "Close other games and heavy applications before a comparison.");
    note->setWordWrap(true);
    note->setObjectName("themed_note");
    layout->addWidget(note);

    m_status = new QLabel("Ready.");
    layout->addWidget(m_status);

    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    layout->addWidget(m_progress);

    m_result = new QTextEdit();
    m_result->setReadOnly(true);
    m_result->setFont(QFont("Consolas", 9));
    m_result->setPlaceholderText(
        "The completed benchmark result will appear here.");
    layout->addWidget(m_result, 1);

    auto* buttons = new QHBoxLayout();
    m_startButton = new QPushButton("Start Benchmark");
    m_copyButton = new QPushButton("Copy Result");
    m_closeButton = new QPushButton("Close");
    m_copyButton->setEnabled(false);
    buttons->addWidget(m_startButton);
    buttons->addWidget(m_copyButton);
    buttons->addStretch();
    buttons->addWidget(m_closeButton);
    layout->addLayout(buttons);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setStandardInputFile(QProcess::nullDevice());

    connect(m_preset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { updateFramePreset(); });
    connect(m_startButton, &QPushButton::clicked,
            this, [this]() { startBenchmark(); });
    connect(m_copyButton, &QPushButton::clicked,
            this, [this]() { copyResult(); });
    connect(m_closeButton, &QPushButton::clicked,
            this, &BenchmarkDialog::reject);
    connect(m_process, &QProcess::started,
            this, [this]() { processStarted(); });
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, [this]() { appendProcessOutput(); });
    connect(m_process, &QProcess::finished,
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                processFinished(exitCode, exitStatus);
            });
    connect(m_process, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                processError(error);
            });

    updateFramePreset();
}

BenchmarkDialog::~BenchmarkDialog() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        disconnect(m_process, nullptr, this, nullptr);
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void BenchmarkDialog::updateFramePreset() {
    const int presetFrames = m_preset->currentData().toInt();
    const bool custom = presetFrames == 0;
    if (!custom)
        m_frames->setValue(presetFrames);
    m_frames->setEnabled(custom && !m_runActive);
}

void BenchmarkDialog::startBenchmark() {
    if (m_runActive)
        return;

    m_requestedFrames = m_frames->value();
    const auto benchmarkArgs = build_jollygood_benchmark_args(
        m_preparation.args, m_requestedFrames);
    if (!benchmarkArgs.has_value()) {
        showFailure("The benchmark frame count or prepared launch command is invalid.");
        return;
    }

    const LogFileOperationResult logReady = prepare_regular_log_file(
        qstring_to_path(m_preparation.log_path));
    if (!logReady.success) {
        showFailure(
            QString("The JGRF output log is not safe to use.\n\n%1")
                .arg(QString::fromStdString(logReady.error)));
        return;
    }

    m_output.clear();
    m_resultText.clear();
    m_result->clear();
    m_copyButton->setEnabled(false);
    m_progress->setRange(0, 0);
    m_status->setText("Starting JGRF benchmark...");
    m_startButton->setEnabled(false);
    m_preset->setEnabled(false);
    m_frames->setEnabled(false);
    m_closeButton->setText("Cancel Benchmark");

    m_cancelRequested = false;
    m_logWriteFailureReported = false;
    m_runActive = true;
    m_timer.invalidate();

    m_process->setProgram(benchmarkArgs->first());
    m_process->setArguments(benchmarkArgs->mid(1));
    m_process->setWorkingDirectory(m_preparation.working_directory);
    m_process->setProcessEnvironment(m_preparation.environment);

    DebugLogger::logInfo(
        QString("benchmark args: %1").arg(benchmarkArgs->join(" ")));
    m_process->start();
}

void BenchmarkDialog::processStarted() {
    m_timer.start();
    m_status->setText(
        QString("Running %1 frames...").arg(
            QLocale().toString(m_requestedFrames)));
}

void BenchmarkDialog::appendProcessOutput() {
    const QByteArray chunk = m_process->readAllStandardOutput();
    if (chunk.isEmpty())
        return;

    m_output.append(chunk);
    if (!m_preparation.log_path.isEmpty()) {
        const LogFileOperationResult appended = append_regular_log_file(
            qstring_to_path(m_preparation.log_path),
            std::string_view(
                chunk.constData(), static_cast<std::size_t>(chunk.size())));
        if (!appended.success && !m_logWriteFailureReported) {
            m_logWriteFailureReported = true;
            DebugLogger::logError(
                QString("benchmark log append refused: %1")
                    .arg(QString::fromStdString(appended.error)));
        }
    }
}

void BenchmarkDialog::processFinished(int exitCode,
                                      QProcess::ExitStatus exitStatus) {
    appendProcessOutput();
    if (!m_runActive)
        return;

    const qint64 elapsed = m_timer.isValid() ? m_timer.elapsed() : 0;
    m_runActive = false;

    if (m_cancelRequested) {
        DebugLogger::logInfo("benchmark cancelled by user");
        QDialog::reject();
        return;
    }

    const std::string_view output(
        m_output.constData(), static_cast<std::size_t>(m_output.size()));
    const auto completedFrames = parse_jollygood_benchmark_completion(output);
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        showFailure(QString("JGRF exited abnormally (exit code %1).\n\n"
                            "See %2 for the captured frontend output.")
                        .arg(exitCode)
                        .arg(m_preparation.log_path));
        return;
    }
    if (!completedFrames.has_value()) {
        showFailure(QString("JGRF exited without its benchmark completion marker.\n\n"
                            "See %1 for the captured frontend output.")
                        .arg(m_preparation.log_path));
        return;
    }
    if (*completedFrames != static_cast<std::uint64_t>(m_requestedFrames)) {
        showFailure(
            QString("JGRF reported %1 completed frames, but Goliath requested %2.")
                .arg(formatted_frames(*completedFrames))
                .arg(QLocale().toString(m_requestedFrames)));
        return;
    }

    const auto metrics = calculate_jollygood_benchmark_metrics(
        *completedFrames, elapsed);
    if (!metrics.has_value()) {
        showFailure("The benchmark completed, but its elapsed time was invalid.");
        return;
    }

    const QLocale locale;
    const QStringList resultLines = {
        "Goliath Performance Benchmark",
        "",
        "Game             : " + m_displayName,
        "Media            : " + m_media,
        QString("Configuration    : ") +
            (m_profileApplied ? "Exact-media profile" : "Global settings"),
        "Video API        : " + video_api_name(m_videoApi),
        "Shader           : " + shader_name(m_shader),
        "Frames           : " + formatted_frames(metrics->frames),
        "Total time       : " +
            locale.toString(
                static_cast<double>(metrics->elapsed_milliseconds) / 1000.0,
                'f', 3) + " s",
        "Throughput       : " +
            locale.toString(metrics->frames_per_second, 'f', 2) + " frames/s",
        "Average frame    : " +
            locale.toString(metrics->milliseconds_per_frame, 'f', 3) + " ms/frame",
        "",
        "Total time includes JGRF/core/media/window startup and process shutdown. "
        "Per-frame audio sample processing is bypassed by JGRF Benchmark Mode.",
    };
    m_resultText = resultLines.join('\n');

    m_status->setText("Benchmark completed successfully.");
    m_result->setPlainText(m_resultText);
    m_copyButton->setEnabled(true);
    DebugLogger::logInfo(
        QString("benchmark completed: frames=%1 elapsed_ms=%2 fps=%3")
            .arg(static_cast<qulonglong>(metrics->frames))
            .arg(metrics->elapsed_milliseconds)
            .arg(metrics->frames_per_second, 0, 'f', 2));
    restoreIdleControls();
}

void BenchmarkDialog::processError(QProcess::ProcessError error) {
    if (error != QProcess::FailedToStart || !m_runActive)
        return;

    m_runActive = false;
    if (m_cancelRequested) {
        QDialog::reject();
        return;
    }
    showFailure(QString("Could not start JGRF.\n\n%1")
                    .arg(m_process->errorString()));
}

void BenchmarkDialog::restoreIdleControls() {
    m_progress->setRange(0, 100);
    m_progress->setValue(m_resultText.isEmpty() ? 0 : 100);
    m_startButton->setEnabled(true);
    m_preset->setEnabled(true);
    m_closeButton->setText("Close");
    updateFramePreset();
}

void BenchmarkDialog::showFailure(const QString& detail) {
    m_status->setText("Benchmark failed.");
    m_resultText.clear();
    m_result->setPlainText(detail);
    m_copyButton->setEnabled(false);
    DebugLogger::logError(QString("benchmark failed: %1").arg(detail));
    restoreIdleControls();
}

void BenchmarkDialog::copyResult() const {
    if (!m_resultText.isEmpty())
        QGuiApplication::clipboard()->setText(m_resultText);
}

void BenchmarkDialog::reject() {
    if (!m_runActive) {
        QDialog::reject();
        return;
    }

    const QMessageBox::StandardButton choice = QMessageBox::question(
        this, "Cancel Benchmark",
        "Stop the active benchmark and close this window?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes)
        return;
    // The process can finish while the confirmation dialog runs its nested
    // event loop. In that case there is nothing left to kill.
    if (!m_runActive) {
        QDialog::reject();
        return;
    }

    m_cancelRequested = true;
    m_status->setText("Stopping benchmark...");
    m_closeButton->setEnabled(false);
    m_process->kill();
}

} // namespace goliath
