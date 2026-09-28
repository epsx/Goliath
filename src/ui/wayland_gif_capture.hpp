#pragma once

#if defined(GOLIATH_WAYLAND_CAPTURE)

#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

#include <pipewire/pipewire.h>
#include <spa/param/video/raw.h>

namespace goliath {

// The portal owns the selected window and limits the PipeWire connection to it.
// All stream callbacks and the GIF timer run on the Qt thread.
class WaylandGifCapture final : public QObject {
    Q_OBJECT
public:
    explicit WaylandGifCapture(QObject* parent = nullptr);
    ~WaylandGifCapture() override;
    bool start(QString* error);
    QImage frame() const { return m_frame; }
    QString error() const { return m_error; }

private:
    static void formatChanged(void* data, uint32_t id, const spa_pod* param);
    static void process(void* data);
    static void stateChanged(void* data, pw_stream_state oldState,
                             pw_stream_state state, const char* error);
    void closeSession();

    QString m_sessionPath;
    QTimer m_pollTimer;
    QImage m_frame;
    QString m_error;
    pw_loop* m_loop = nullptr;
    pw_context* m_context = nullptr;
    pw_core* m_core = nullptr;
    pw_stream* m_stream = nullptr;
    spa_hook m_listener{};
    spa_video_info_raw m_video{};
};

} // namespace goliath
#endif
