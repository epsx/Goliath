#include "jollygood_audio.hpp"

#include "audio/audio_devices.hpp"
#include "common/debug_logger.hpp"

#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include <cstdint>

namespace goliath {

bool confirm_jollygood_audio_before_launch(QWidget* parent) {
    const AudioDeviceSnapshot snapshot = query_audio_playback_devices();
    if (snapshot.has_playback_device()) return true;

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle("No Audio Output Device");
    box.setText("Goliath could not detect an available audio playback device.");
    box.setInformativeText(
        "The game / JGRF might fail to start or crash without audio.\n\n"
        "Connect speakers, headphones, or another audio output device before starting the game.\n\n"
        "You can check or refresh detected audio devices in Settings > Audio.");

    if (!snapshot.error.empty()) {
        box.setDetailedText(QString("SDL audio detection: %1")
                                .arg(QString::fromUtf8(snapshot.error.c_str())));
    }

    QPushButton* launchAnyway = box.addButton("Launch Anyway", QMessageBox::AcceptRole);
    QPushButton* cancel = box.addButton("Cancel", QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();

    return box.clickedButton() == launchAnyway;
}

void apply_jollygood_audio_volume_with_retry(QObject* context, qint64 pid,
                                              int configured_volume, int attempt) {
#if defined(_WIN32)
    if (!context || pid <= 0) return;

    constexpr int kMaxAttempts = 24;
    constexpr int kRetryDelayMs = 250;
    const int volume = clamp_audio_volume(configured_volume);

    if (set_process_audio_volume(static_cast<std::uint32_t>(pid), volume)) {
        DebugLogger::logInfo(QString("applied JGRF session volume: %1% (pid %2)")
                                 .arg(volume)
                                 .arg(pid));
        return;
    }

    if (attempt + 1 < kMaxAttempts) {
        QTimer::singleShot(kRetryDelayMs, context,
                           [context, pid, volume, attempt] {
                               apply_jollygood_audio_volume_with_retry(
                                   context, pid, volume, attempt + 1);
                           });
        return;
    }

    DebugLogger::logError(
        QString("could not find/apply JGRF audio session volume after %1 attempts (pid %2)")
            .arg(kMaxAttempts)
            .arg(pid));
#else
    Q_UNUSED(context);
    Q_UNUSED(pid);
    Q_UNUSED(configured_volume);
    Q_UNUSED(attempt);
#endif
}

} // namespace goliath
