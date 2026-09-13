#include "ui/audio_export_dialog.hpp"

#include "game/audio_export.hpp"

#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryFile>

#include <system_error>

namespace fs = std::filesystem;

namespace goliath {

namespace {

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

fs::path qstring_to_path(const QString& path) {
#if defined(_WIN32)
    return fs::path(path.toStdWString());
#else
    return fs::path(path.toStdString());
#endif
}

bool probe_output_directory(const fs::path& directory,
                            QString* leftover,
                            QString* error) {
    const fs::path probeTemplate =
        directory / ".goliath-wav-write-test-XXXXXX";
    QTemporaryFile probe(path_to_qstring(probeTemplate));
    probe.setAutoRemove(false);
    if (!probe.open()) {
        if (error) *error = "Goliath could not write to this directory.";
        return false;
    }
    probe.close();
    if (!probe.remove()) {
        if (leftover) *leftover = probe.fileName();
        if (error) {
            *error = "The directory write test succeeded, but its temporary "
                     "file could not be removed.";
        }
        return false;
    }
    return true;
}

} // namespace

std::optional<fs::path> request_audio_export_target(
        const QString& displayName,
        const fs::path& defaultDirectory,
        QWidget* parent) {
    std::error_code ec;
    fs::create_directories(defaultDirectory, ec);
    if (ec || !fs::is_directory(defaultDirectory, ec)) {
        QMessageBox::critical(
            parent, "Audio WAV Export",
            QString("Could not prepare the default audio export directory.\n\n%1")
                .arg(path_to_qstring(defaultDirectory)));
        return std::nullopt;
    }

    const std::string sanitized =
        sanitize_audio_export_stem(displayName.toStdString());
    const QString defaultFilename =
        QString("%1 - %2.wav")
            .arg(QString::fromUtf8(
                     sanitized.data(), static_cast<qsizetype>(sanitized.size())))
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
    const fs::path defaultPath = unique_audio_export_path(
        defaultDirectory / qstring_to_path(defaultFilename));

    std::optional<fs::path> outputPath;
    while (!outputPath.has_value()) {
        const QString selected = QFileDialog::getSaveFileName(
            parent,
            "Export Selected Game Audio",
            path_to_qstring(defaultPath),
            "Wave audio (*.wav)",
            nullptr,
            QFileDialog::DontConfirmOverwrite);
        if (selected.isEmpty()) return std::nullopt;

        const fs::path candidate = ensure_audio_export_wav_extension(
            qstring_to_path(selected));
        const AudioExportTargetValidation validation =
            validate_audio_export_target(candidate);
        if (!validation.success) {
            QMessageBox::warning(
                parent, "Audio WAV Export",
                QString("The selected output cannot be used.\n\n%1")
                    .arg(QString::fromStdString(validation.error)));
            continue;
        }

        // JGRF 2.0.1 does not safely recover when its wave writer cannot open
        // a selected directory. Probe it without touching the requested
        // target; the temporary file is removed before launch.
        QString leftover;
        QString probeError;
        if (!probe_output_directory(
                candidate.parent_path(), &leftover, &probeError)) {
            QString detail = probeError;
            if (!leftover.isEmpty()) detail += "\n\n" + leftover;
            QMessageBox::warning(parent, "Audio WAV Export", detail);
            continue;
        }
        outputPath = candidate;
    }

    QMessageBox confirmation(QMessageBox::Question,
                             "Audio WAV Export", QString(),
                             QMessageBox::NoButton, parent);
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.setText(
        QString("Launch %1 and record its JGRF audio output?\n\n"
                "Output: %2\n\n"
                "The capture is one-shot and does not modify global or "
                "per-game settings. Close the JGRF game normally to finalize "
                "the WAV header. Goliath may be closed while recording continues.")
            .arg(displayName)
            .arg(path_to_qstring(*outputPath)));
    QPushButton* startButton = confirmation.addButton(
        "Launch && Record", QMessageBox::AcceptRole);
    confirmation.addButton(QMessageBox::Cancel);
    confirmation.setDefaultButton(startButton);
    confirmation.exec();
    if (confirmation.clickedButton() != startButton) return std::nullopt;
    return outputPath;
}

} // namespace goliath
