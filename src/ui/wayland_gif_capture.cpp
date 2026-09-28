#include "ui/wayland_gif_capture.hpp"
#include "ui/main_window.hpp"
#include "ui/recording_id.hpp"
#include "game/animated_gif.hpp"
#include "common/debug_logger.hpp"

#if defined(GOLIATH_WAYLAND_CAPTURE)

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QEventLoop>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QRect>
#include <QSaveFile>
#include <QStatusBar>
#include <QUuid>

#include <spa/param/format-utils.h>
#include <spa/param/buffers.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/buffer/buffer.h>
#include <spa/buffer/meta.h>
#include <spa/pod/builder.h>

#include <cstdint>
#include <memory>
#include <array>

#include <unistd.h>

namespace goliath {
// Connect to the predictable request path before invoking the portal: a quick
// CreateSession reply can otherwise arrive before the signal subscription.
class PortalReply final : public QObject {
    Q_OBJECT
public:
    bool completed = false;
    uint code = 2;
    QVariantMap values;
signals:
    void arrived();
public slots:
    void response(uint result, const QVariantMap& data) {
        completed = true;
        code = result;
        values = data;
        emit arrived();
    }
};

namespace {

constexpr auto kPortal = "org.freedesktop.portal.Desktop";
constexpr auto kDesktop = "/org/freedesktop/portal/desktop";
constexpr auto kScreenCast = "org.freedesktop.portal.ScreenCast";

QString token() {
    return "goliath_" + QUuid::createUuid().toString(QUuid::Id128);
}

bool request(const char* method, const QVariantList& arguments, QVariantMap opts,
             QVariantMap* result, QString* error) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString handleToken = token();
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    const QString path = QString("/org/freedesktop/portal/desktop/request/%1/%2")
                             .arg(sender, handleToken);
    PortalReply reply;
    if (!bus.connect(kPortal, path, "org.freedesktop.portal.Request",
                     "Response", &reply, SLOT(response(uint,QVariantMap)))) {
        *error = "Cannot subscribe to the desktop portal response.";
        return false;
    }
    QVariantList args = arguments;
    opts.insert("handle_token", handleToken);
    args.append(opts);
    QDBusMessage call = QDBusMessage::createMethodCall(kPortal, kDesktop,
                                                       kScreenCast, method);
    call.setArguments(args);
    const QDBusMessage answer = bus.call(call);
    if (answer.type() == QDBusMessage::ErrorMessage || answer.arguments().isEmpty()) {
        *error = "ScreenCast portal request failed: " + answer.errorMessage();
        bus.disconnect(kPortal, path, "org.freedesktop.portal.Request",
                       "Response", &reply, SLOT(response(uint,QVariantMap)));
        return false;
    }
    const QString actualPath = answer.arguments().first().value<QDBusObjectPath>().path();
    if (actualPath != path) {
        *error = "ScreenCast portal returned an unexpected request path.";
        bus.disconnect(kPortal, path, "org.freedesktop.portal.Request",
                       "Response", &reply, SLOT(response(uint,QVariantMap)));
        return false;
    }
    if (!reply.completed) {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&reply, &PortalReply::arrived, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(120000); // Allows time to choose a window in the system dialog.
        loop.exec();
    }
    bus.disconnect(kPortal, path, "org.freedesktop.portal.Request",
                   "Response", &reply, SLOT(response(uint,QVariantMap)));
    if (!reply.completed || reply.code != 0) {
        *error = !reply.completed ? "ScreenCast portal timed out."
                                  : reply.code == 1 ? "Window selection was cancelled."
                                                    : "Window capture was denied by the portal.";
        return false;
    }
    *result = reply.values;
    return true;
}

uint32_t firstStreamId(const QVariant& value) {
    if (!value.isValid() || !value.canConvert<QDBusArgument>()) return SPA_ID_INVALID;
    const QDBusArgument data = qvariant_cast<QDBusArgument>(value);
    const QDBusArgument& reader = data; // Extraction must use the const D-Bus API.
    reader.beginArray();
    uint32_t id = SPA_ID_INVALID;
    if (!reader.atEnd()) {
        QVariantMap streamProperties;
        reader.beginStructure();
        reader >> id >> streamProperties;
        reader.endStructure();
    }
    reader.endArray();
    return id;
}

} // namespace

WaylandGifCapture::WaylandGifCapture(QObject* parent) : QObject(parent) {
    m_pollTimer.setInterval(10);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() {
        if (m_loop) {
            for (int i = 0; i < 8 && pw_loop_iterate(m_loop, 0) > 0; ++i) {}
        }
    });
}

WaylandGifCapture::~WaylandGifCapture() {
    m_pollTimer.stop();
    if (m_stream) pw_stream_destroy(m_stream);
    if (m_core) pw_core_disconnect(m_core);
    if (m_context) pw_context_destroy(m_context);
    if (m_loop) pw_loop_destroy(m_loop);
    closeSession();
}

void WaylandGifCapture::closeSession() {
    if (m_sessionPath.isEmpty()) return;
    QDBusMessage call = QDBusMessage::createMethodCall(kPortal, m_sessionPath,
        "org.freedesktop.portal.Session", "Close");
    QDBusConnection::sessionBus().asyncCall(call);
    m_sessionPath.clear();
}

bool WaylandGifCapture::start(QString* error) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        *error = "Cannot connect to the session D-Bus.";
        return false;
    }
    QVariantMap options{{"session_handle_token", token()}};
    QVariantMap response;
    if (!request("CreateSession", {}, options, &response, error)) return false;
    m_sessionPath = response.value("session_handle").toString();
    if (m_sessionPath.isEmpty()) {
        *error = "ScreenCast portal did not create a session.";
        return false;
    }

    QVariantMap select{{"types", 2u}, {"multiple", false}}; // Window only.
    if (!request("SelectSources", {QVariant::fromValue(QDBusObjectPath(m_sessionPath))},
                 select, &response, error)) return false;
    // The portal picker is responsible for choosing the JGRF window.
    if (!request("Start", {QVariant::fromValue(QDBusObjectPath(m_sessionPath)),
                            QString()}, {}, &response, error)) return false;
    const uint32_t streamId = firstStreamId(response.value("streams"));
    if (streamId == SPA_ID_INVALID) {
        *error = "ScreenCast portal returned no window video stream.";
        return false;
    }
    QDBusMessage remote = QDBusMessage::createMethodCall(kPortal, kDesktop,
        kScreenCast, "OpenPipeWireRemote");
    remote << QVariant::fromValue(QDBusObjectPath(m_sessionPath)) << QVariantMap{};
    const QDBusMessage fdReply = bus.call(remote);
    if (fdReply.type() == QDBusMessage::ErrorMessage || fdReply.arguments().isEmpty()) {
        *error = "Cannot open the portal PipeWire stream: " + fdReply.errorMessage();
        return false;
    }
    const QDBusUnixFileDescriptor descriptor =
        fdReply.arguments().first().value<QDBusUnixFileDescriptor>();
    if (!descriptor.isValid()) {
        *error = "ScreenCast portal returned an invalid PipeWire descriptor.";
        return false;
    }

    pw_init(nullptr, nullptr);
    m_loop = pw_loop_new(nullptr);
    if (m_loop) m_context = pw_context_new(m_loop, nullptr, 0);
    const int ownedFd = dup(descriptor.fileDescriptor());
    if (ownedFd < 0 || !m_context) {
        if (ownedFd >= 0) close(ownedFd);
        *error = "Cannot initialize the PipeWire video receiver.";
        return false;
    }
    m_core = pw_context_connect_fd(m_context, ownedFd, nullptr, 0);
    if (!m_core) {
        *error = "Cannot connect to the permitted PipeWire stream.";
        return false;
    }
    m_stream = pw_stream_new(m_core, "Goliath game GIF",
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
                          "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));
    if (!m_stream) {
        *error = "Cannot create the PipeWire video stream.";
        return false;
    }
    static const pw_stream_events events = [] {
        pw_stream_events result{};
        result.version = PW_VERSION_STREAM_EVENTS;
        result.state_changed = &WaylandGifCapture::stateChanged;
        result.param_changed = &WaylandGifCapture::formatChanged;
        result.process = &WaylandGifCapture::process;
        return result;
    }();
    pw_stream_add_listener(m_stream, &m_listener, &events, this);

    uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod* formats[] = {static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4,
            SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA,
            SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA)))};
    if (pw_stream_connect(m_stream, PW_DIRECTION_INPUT, streamId,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                     PW_STREAM_FLAG_MAP_BUFFERS), formats, 1) < 0) {
        *error = "PipeWire could not connect to the selected window.";
        return false;
    }
    m_pollTimer.start();
    return true;
}

void WaylandGifCapture::formatChanged(void* data, uint32_t id, const spa_pod* param) {
    auto* self = static_cast<WaylandGifCapture*>(data);
    if (id != SPA_PARAM_Format || !param) return;
    if (spa_format_video_raw_parse(param, &self->m_video) < 0) {
        self->m_error = "PipeWire negotiated an unsupported video format.";
        return;
    }

    // A window stream can use a monitor-sized buffer even when the selected
    // window is smaller. Ask the producer to mark the actual window rectangle.
    uint8_t storage[256];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(storage, sizeof(storage));
    const spa_pod* metadata[] = {static_cast<const spa_pod*>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
        SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoCrop),
        SPA_PARAM_META_size, SPA_POD_Int(sizeof(spa_meta_region))))};
    if (pw_stream_update_params(self->m_stream, metadata, 1) < 0)
        self->m_error = "PipeWire could not request the video crop metadata.";
}

void WaylandGifCapture::stateChanged(void* data, pw_stream_state,
                                      pw_stream_state state, const char* error) {
    if (state == PW_STREAM_STATE_ERROR)
        static_cast<WaylandGifCapture*>(data)->m_error =
            QString::fromUtf8(error ? error : "PipeWire video stream failed.");
}

void WaylandGifCapture::process(void* data) {
    auto* self = static_cast<WaylandGifCapture*>(data);
    pw_buffer* frame = pw_stream_dequeue_buffer(self->m_stream);
    if (!frame) return;
    spa_buffer* buffer = frame->buffer;
    if (buffer && buffer->n_datas && buffer->datas[0].data &&
        buffer->datas[0].chunk) {
        const spa_data& bytes = buffer->datas[0];
        const spa_chunk& chunk = *bytes.chunk;
        const int width = static_cast<int>(self->m_video.size.width);
        const int height = static_cast<int>(self->m_video.size.height);
        const int stride = chunk.stride;
        QImage::Format format = QImage::Format_Invalid;
        switch (self->m_video.format) {
        case SPA_VIDEO_FORMAT_BGRx: format = QImage::Format_RGB32; break;
        case SPA_VIDEO_FORMAT_BGRA: format = QImage::Format_ARGB32; break;
        case SPA_VIDEO_FORMAT_RGBx: format = QImage::Format_RGBX8888; break;
        case SPA_VIDEO_FORMAT_RGBA: format = QImage::Format_RGBA8888; break;
        default: break;
        }
        const std::uint64_t needed = std::uint64_t(stride > 0 ? stride : 0) * height;
        if (format != QImage::Format_Invalid && width >= 160 && height >= 120 &&
            width <= 8192 && height <= 8192 && stride >= width * 4 &&
            needed <= chunk.size && chunk.offset <= bytes.maxsize &&
            needed <= bytes.maxsize - chunk.offset) {
            const auto* pixels = static_cast<const uchar*>(bytes.data) + chunk.offset;
            QRect visible(0, 0, width, height);
            const auto* crop = static_cast<const spa_meta_region*>(spa_buffer_find_meta_data(
                buffer, SPA_META_VideoCrop, sizeof(spa_meta_region)));
            if (crop && spa_meta_region_is_valid(crop)) {
                const auto& region = crop->region;
                const int x = region.position.x;
                const int y = region.position.y;
                const auto w = region.size.width;
                const auto h = region.size.height;
                if (x >= 0 && y >= 0 && x < width && y < height &&
                    w > 0 && h > 0 && w <= static_cast<unsigned>(width - x) &&
                    h <= static_cast<unsigned>(height - y))
                    visible = QRect(x, y, static_cast<int>(w), static_cast<int>(h));
            }
            if (self->m_frame.isNull())
                DebugLogger::logInfo(QString("Wayland GIF frame: buffer=%1x%2 crop=%3,%4 %5x%6 metadata=%7")
                    .arg(width).arg(height).arg(visible.x()).arg(visible.y())
                    .arg(visible.width()).arg(visible.height()).arg(crop ? "present" : "absent"));
            self->m_frame = QImage(pixels, width, height, stride, format).copy(visible);
        }
    }
    pw_stream_queue_buffer(self->m_stream, frame);
}

void MainWindow::recordWaylandGameGif() {
    if (m_gifRecording) return;
    std::string media;
    int running = 0;
    for (const auto& tracker : m_trackedGameProcesses) {
        if (tracker->state() == DetachedProcessState::Exited) continue;
        media = tracker->media();
        ++running;
    }
    if (running != 1) {
        QMessageBox::information(this, "Game GIF",
            "Start exactly one game through Goliath before recording on Wayland.");
        return;
    }
    // Wayland does not expose other applications' window IDs or PIDs. The
    // desktop's window picker is the source of truth for this capture.
    statusBar()->showMessage("Choose the Goliath-launched game's window in the sharing dialog.",
                             12000);
    auto capture = std::make_shared<WaylandGifCapture>();
    QString error;
    if (!capture->start(&error)) {
        if (error != "Window selection was cancelled.")
            QMessageBox::warning(this, "Game GIF", error);
        return;
    }
    bool valid = false;
    int seconds = QString::fromStdString(m_config.get("Hotkeys", "gif_duration_seconds", "7"))
                      .toInt(&valid);
    if (!valid || (seconds != 5 && seconds != 7 && seconds != 10)) seconds = 7;
    const int targetFrames = seconds * 8;
    auto encoder = std::make_shared<AnimatedGifEncoder>();
    auto frames = std::make_shared<int>(0);
    auto dimensions = std::make_shared<QSize>();
    auto* timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(125);
    auto attempts = std::make_shared<int>(0);
    m_gifRecording = true;
    updateScreenshotAction();
    statusBar()->showMessage(QString("Recording %1-second game GIF...").arg(seconds),
                             seconds * 1000);
    connect(timer, &QTimer::timeout, this,
            [this, capture, encoder, frames, dimensions, attempts,
             timer, targetFrames, media]() {
        const auto fail = [this, timer](const QString& problem) {
            timer->stop();
            timer->deleteLater();
            m_gifRecording = false;
            updateScreenshotAction();
            QMessageBox::warning(this, "Game GIF", problem);
        };
        if (!capture->error().isEmpty()) {
            fail("Window stream stopped: " + capture->error());
            return;
        }
        QImage image = capture->frame();
        if (image.isNull()) {
            if (++*attempts > 32)
                fail("No video frames arrived from the selected window.");
            return;
        }
        if (dimensions->isEmpty()) {
            *dimensions = image.size().scaled(640, 640, Qt::KeepAspectRatio);
            if (!encoder->begin(dimensions->width(), dimensions->height())) {
                fail("The selected window is too small to record.");
                return;
            }
        }
        image = image.scaled(*dimensions, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                     .convertToFormat(QImage::Format_Indexed8);
        const auto colors = image.colorTable();
        std::array<std::uint32_t, 256> palette{};
        for (int i = 0; i < colors.size() && i < 256; ++i)
            palette[static_cast<std::size_t>(i)] = colors[i];
        const int delay = (*frames & 1) ? 13 : 12;
        if (!encoder->addFrame(image.constBits(), image.bytesPerLine(), palette.data(),
                               static_cast<std::size_t>(colors.size()), delay)) {
            fail("Could not encode the GIF frame.");
            return;
        }
        if (++*frames != targetFrames) return;

        timer->stop();
        timer->deleteLater();
        m_gifRecording = false;
        updateScreenshotAction();
        const auto bytes = encoder->finish();
        if (bytes.empty()) {
            QMessageBox::warning(this, "Game GIF", "Could not finish the GIF.");
            return;
        }
        const QDateTime now = QDateTime::currentDateTime();
        const auto folderPath = m_paths.base_dir / "recordings" /
            recordingIdForMedia(QString::fromStdString(media)).toStdWString() /
            now.toString("yyyy-MM-dd").toStdWString();
        const QString folder = QString::fromStdWString(folderPath.wstring());
        if (!QDir().mkpath(folder)) {
            QMessageBox::warning(this, "Game GIF", "Could not create the recordings folder.");
            return;
        }
        const QString stem = now.toString("HH-mm-ss");
        QString output = QDir(folder).filePath(stem + ".gif");
        for (int suffix = 2; QFileInfo::exists(output); ++suffix)
            output = QDir(folder).filePath(stem + "-" + QString::number(suffix) + ".gif");
        QSaveFile file(output);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size()) ||
            !file.commit()) {
            file.cancelWriting();
            QMessageBox::warning(this, "Game GIF", "Could not save the GIF file.");
            return;
        }
        statusBar()->showMessage(QString("Saved game GIF: %1").arg(output), 8000);
        DebugLogger::logInfo(QString("saved game GIF: %1").arg(output));
        // Polling the stream stops when the last callback releases capture.
    });
    timer->start();
}

} // namespace goliath
#include "wayland_gif_capture.moc"
#endif
