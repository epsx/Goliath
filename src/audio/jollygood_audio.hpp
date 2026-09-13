// jollygood_audio.hpp — launch-time audio checks and Windows session-volume
// handling for the detached JGRF process.
#pragma once

#include <QtGlobal>

class QObject;
class QWidget;

namespace goliath {

// Checks SDL playback devices before launch. If none are available, presents
// the existing warning and lets the user explicitly choose Launch Anyway or
// Cancel. Returns true when launch should continue.
bool confirm_jollygood_audio_before_launch(QWidget* parent);

// Windows-only best-effort retry loop. JGRF creates its Core Audio session
// shortly after process start, so the session volume cannot always be set on
// the first attempt. On non-Windows platforms this is a no-op.
void apply_jollygood_audio_volume_with_retry(QObject* context, qint64 pid,
                                              int configured_volume,
                                              int attempt = 0);

} // namespace goliath
