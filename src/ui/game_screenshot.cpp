#include "ui/main_window.hpp"

#include "common/debug_logger.hpp"
#include "game/animated_gif.hpp"
#include "ui/capture_hotkeys.hpp"
#include "ui/recording_id.hpp"
#if defined(GOLIATH_WAYLAND_CAPTURE)
#include "ui/wayland_gif_capture.hpp"
#include "ui/wayland_gif_hotkey.hpp"
#endif
#if defined(_WIN32)
#include "ui/windows_desktop_capture.hpp"
#endif

#include <QAction>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QSaveFile>
#include <QScreen>
#include <QStringList>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#if defined(GOLIATH_X11_CAPTURE)
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

namespace goliath {
namespace {

#if defined(_WIN32)
constexpr int kGifHotkeyId = 0x4731;
constexpr int kPngHotkeyId = 0x4732;
HWINEVENTHOOK g_captureFocusHook = nullptr;
std::function<void()> g_captureFocusCallback;

void CALLBACK captureFocusChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG,
                                  DWORD, DWORD) {
    if (g_captureFocusCallback) g_captureFocusCallback();
}

struct GameWindowCandidate {
    DWORD pid = 0;
    HWND hwnd = nullptr;
    long long area = 0;
};

BOOL CALLBACK findGameWindow(HWND hwnd, LPARAM context) {
    auto& candidate = *reinterpret_cast<GameWindowCandidate*>(context);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != candidate.pid || !IsWindowVisible(hwnd) || IsIconic(hwnd) ||
        GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    RECT bounds{};
    if (!GetWindowRect(hwnd, &bounds)) return TRUE;
    const long long width = bounds.right - bounds.left;
    const long long height = bounds.bottom - bounds.top;
    if (width < 160 || height < 120) return TRUE;
    const long long area = width * height;
    if (area > candidate.area) {
        candidate.area = area;
        candidate.hwnd = hwnd;
    }
    return TRUE;
}

// A Vulkan swapchain may appear black when Qt grabs the HWND. In that case,
// capture its visible desktop rectangle; the game must remain unobscured.
QPixmap grabVisibleGameRectangle(HWND hwnd) {
    RECT client{};
    if (!IsWindow(hwnd) || IsIconic(hwnd) || !GetClientRect(hwnd, &client)) return {};
    POINT origin{client.left, client.top};
    if (!ClientToScreen(hwnd, &origin)) return {};
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) return {};

    HDC desktop = GetDC(nullptr);
    if (!desktop) return {};
    HDC memory = CreateCompatibleDC(desktop);
    if (!memory) { ReleaseDC(nullptr, desktop); return {}; }
    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = width;
    bitmap.bmiHeader.biHeight = -height; // Top down; QImage owns a copy below.
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP surface = CreateDIBSection(desktop, &bitmap, DIB_RGB_COLORS,
                                       &pixels, nullptr, 0);
    QPixmap result;
    if (surface && pixels) {
        HGDIOBJ previous = SelectObject(memory, surface);
        if (previous && previous != HGDI_ERROR) {
            if (BitBlt(memory, 0, 0, width, height, desktop,
                       origin.x, origin.y, SRCCOPY | CAPTUREBLT)) {
                const QImage view(static_cast<const uchar*>(pixels), width, height,
                                  width * 4, QImage::Format_RGB32);
                result = QPixmap::fromImage(view.copy());
            }
            SelectObject(memory, previous);
        }
    }
    if (surface) DeleteObject(surface);
    DeleteDC(memory);
    ReleaseDC(nullptr, desktop);
    return result;
}

bool mostlyBlack(const QPixmap& pixmap) {
    if (pixmap.isNull()) return true;
    const QImage frame = pixmap.toImage().convertToFormat(QImage::Format_RGB32);
    int dark = 0;
    for (int y = 0; y < 8; ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(
            frame.constScanLine((y * frame.height()) / 8));
        for (int x = 0; x < 8; ++x) {
            const QRgb pixel = row[(x * frame.width()) / 8];
            if (qRed(pixel) < 12 && qGreen(pixel) < 12 && qBlue(pixel) < 12)
                ++dark;
        }
    }
    return dark >= 62;
}

QPixmap grabGameWindowWithVisibleFallback(QScreen* screen, HWND hwnd) {
    if (!screen || !IsWindow(hwnd)) return {};
    const QPixmap window = screen->grabWindow(reinterpret_cast<WId>(hwnd));
    if (!mostlyBlack(window)) return window;
    DWORD gamePid = 0;
    GetWindowThreadProcessId(hwnd, &gamePid);
    DWORD foregroundPid = 0;
    if (const HWND foreground = GetForegroundWindow())
        GetWindowThreadProcessId(foreground, &foregroundPid);
    if (!gamePid || gamePid != foregroundPid) return window;
    const QPixmap visible = grabVisibleGameRectangle(hwnd);
    return !mostlyBlack(visible) ? visible : window;
}
#endif

#if defined(GOLIATH_X11_CAPTURE)
int ignoreX11CaptureError(Display*, XErrorEvent*) { return 0; }
Display* g_x11HotkeyErrorDisplay = nullptr;
int* g_x11HotkeyErrorCode = nullptr;

int captureX11HotkeyError(Display* display, XErrorEvent* event) {
    if (display == g_x11HotkeyErrorDisplay && g_x11HotkeyErrorCode &&
        !*g_x11HotkeyErrorCode) *g_x11HotkeyErrorCode = event->error_code;
    return 0;
}

class X11HotkeyErrors {
public:
    X11HotkeyErrors(Display* display, int& code)
        : m_display(display) {
        g_x11HotkeyErrorDisplay = display;
        g_x11HotkeyErrorCode = &code;
        m_previous = XSetErrorHandler(captureX11HotkeyError);
        XSync(display, False);
        code = 0; // Ignore errors queued before this specific grab/ungrab.
    }
    ~X11HotkeyErrors() {
        XSync(m_display, False);
        XSetErrorHandler(m_previous);
        g_x11HotkeyErrorDisplay = nullptr;
        g_x11HotkeyErrorCode = nullptr;
    }
    X11HotkeyErrors(const X11HotkeyErrors&) = delete;
    X11HotkeyErrors& operator=(const X11HotkeyErrors&) = delete;
private:
    Display* m_display;
    XErrorHandler m_previous = nullptr;
};

// A game may close while its X11 window is being inspected or recorded.
class X11CaptureQuery {
public:
    explicit X11CaptureQuery(Display* display)
        : m_display(display), m_previous(XSetErrorHandler(ignoreX11CaptureError)) {}
    ~X11CaptureQuery() {
        XSync(m_display, False);
        XSetErrorHandler(m_previous);
    }
    X11CaptureQuery(const X11CaptureQuery&) = delete;
    X11CaptureQuery& operator=(const X11CaptureQuery&) = delete;
private:
    Display* m_display;
    XErrorHandler m_previous;
};

struct X11WindowCandidate {
    Window window = 0;
    std::int64_t pid = 0;
    long long area = 0;
};

std::optional<std::int64_t> x11WindowPid(Display* display, Window window,
                                         Atom pidAtom) {
    Atom actualType = 0;
    int actualFormat = 0;
    unsigned long count = 0, bytesRemaining = 0;
    unsigned char* value = nullptr;
    const int result = XGetWindowProperty(display, window, pidAtom, 0, 1,
        False, XA_CARDINAL, &actualType, &actualFormat, &count,
        &bytesRemaining, &value);
    std::optional<std::int64_t> pid;
    if (result == Success && actualType == XA_CARDINAL &&
        actualFormat == 32 && count == 1 && value)
        pid = static_cast<std::int64_t>(*reinterpret_cast<unsigned long*>(value));
    if (value) XFree(value);
    return pid;
}

bool x11ContainsFocus(Display* display, Window window, Window focus) {
    X11CaptureQuery query(display);
    while (focus && focus != None && focus != PointerRoot) {
        if (focus == window) return true;
        Window root = 0, parent = 0, *children = nullptr;
        unsigned int count = 0;
        if (!XQueryTree(display, focus, &root, &parent, &children, &count))
            return false;
        if (children) XFree(children);
        if (parent == focus || parent == root) return parent == window;
        focus = parent;
    }
    return false;
}

X11WindowCandidate x11GameWindow(Display* display, std::int64_t pid) {
    X11CaptureQuery query(display);
    X11WindowCandidate best;
    const Window root = DefaultRootWindow(display);
    const Atom clientsAtom = XInternAtom(display, "_NET_CLIENT_LIST", True);
    Window* windows = nullptr;
    unsigned long count = 0;
    if (clientsAtom != None) {
        Atom actualType = 0;
        int actualFormat = 0;
        unsigned long bytesRemaining = 0;
        unsigned char* value = nullptr;
        if (XGetWindowProperty(display, root, clientsAtom, 0, 65536,
                False, XA_WINDOW, &actualType, &actualFormat, &count,
                &bytesRemaining, &value) == Success &&
            actualType == XA_WINDOW && actualFormat == 32 && value &&
            bytesRemaining == 0) {
            windows = reinterpret_cast<Window*>(value);
        } else {
            if (value) XFree(value);
            count = 0;
        }
    }
    if (!windows) {
        Window rootReturn = 0, parent = 0;
        unsigned int childCount = 0;
        if (XQueryTree(display, root, &rootReturn, &parent,
                       &windows, &childCount)) count = childCount;
    }
    const Atom pidAtom = XInternAtom(display, "_NET_WM_PID", True);
    if (windows && pidAtom != None) {
        for (unsigned long i = 0; i < count; ++i) {
            const Window window = windows[i];
            if (x11WindowPid(display, window, pidAtom) != pid) continue;
            XWindowAttributes attributes{};
            if (!XGetWindowAttributes(display, window, &attributes) ||
                attributes.map_state != IsViewable ||
                attributes.width < 160 || attributes.height < 120) continue;
            const long long area = static_cast<long long>(attributes.width) *
                                   attributes.height;
            if (area > best.area) best = {window, pid, area};
        }
    }
    if (windows) XFree(windows);
    return best;
}
#endif

} // namespace

#if defined(GOLIATH_X11_CAPTURE)
// Passive grabs belong to the JGRF window, so they only activate with that
// window (or one of its children) focused. This avoids a desktop-wide grab.
class X11GifHotkey {
public:
    X11GifHotkey(QObject* owner, std::function<void()> trigger,
                 std::function<void(const QString&)> warning)
        : m_owner(owner), m_trigger(std::move(trigger)),
          m_warning(std::move(warning)), m_display(XOpenDisplay(nullptr)) {
        if (!m_display) return;
        Bool detectable = False;
        XkbSetDetectableAutoRepeat(m_display, True, &detectable);
        m_detectableRepeat = detectable;
        m_ignoredMasks = {0};
        XModifierKeymap* map = XGetModifierMapping(m_display);
        const KeyCode num = XKeysymToKeycode(m_display, XK_Num_Lock);
        const KeyCode scroll = XKeysymToKeycode(m_display, XK_Scroll_Lock);
        std::vector<unsigned int> masks = {LockMask};
        if (map) {
            for (int modifier = 0; modifier < 8; ++modifier) {
                for (int i = 0; i < map->max_keypermod; ++i) {
                    const KeyCode code = map->modifiermap[
                        modifier * map->max_keypermod + i];
                    if (code && (code == num || code == scroll))
                        masks.push_back(1u << modifier);
                }
            }
            XFreeModifiermap(map);
        }
        for (const unsigned int mask : masks) {
            const auto previous = m_ignoredMasks;
            for (const unsigned int value : previous)
                m_ignoredMasks.push_back(value | mask);
            std::sort(m_ignoredMasks.begin(), m_ignoredMasks.end());
            m_ignoredMasks.erase(std::unique(m_ignoredMasks.begin(),
                                             m_ignoredMasks.end()), m_ignoredMasks.end());
        }
        m_timer.setInterval(80);
        QObject::connect(&m_timer, &QTimer::timeout, owner, [this]() { poll(); });
    }

    ~X11GifHotkey() {
        m_timer.stop();
        clearGrabs();
        if (m_display) XCloseDisplay(m_display);
    }

    bool setShortcut(const QKeySequence& sequence, QString* error) {
        if (!m_display) {
            if (error) *error = "Cannot connect to the X11 display.";
            return false;
        }
        QString problem;
        if (!parseCaptureHotkey(sequence, &problem)) {
            if (error) *error = "GIF: " + problem;
            return false;
        }
        KeyCode code = 0;
        unsigned int modifiers = 0;
        if (!sequence.isEmpty()) {
            const auto combination = sequence[0];
            const int key = combination.key();
            KeySym symbol = NoSymbol;
            if (key >= Qt::Key_A && key <= Qt::Key_Z)
                symbol = XK_a + (key - Qt::Key_A);
            else if (key >= Qt::Key_0 && key <= Qt::Key_9)
                symbol = XK_0 + (key - Qt::Key_0);
            else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
                symbol = XK_F1 + (key - Qt::Key_F1);
            else if (key >= Qt::Key_Space && key < 0x7f)
                symbol = static_cast<KeySym>(key);
            code = symbol == NoSymbol ? 0 : XKeysymToKeycode(m_display, symbol);
            if (!code) {
                if (error) *error = "The GIF key is not present in the current X11 keyboard layout.";
                return false;
            }
            const auto qtModifiers = combination.keyboardModifiers();
            if (qtModifiers & Qt::ControlModifier) modifiers |= ControlMask;
            if (qtModifiers & Qt::AltModifier) modifiers |= Mod1Mask;
            if (qtModifiers & Qt::ShiftModifier) modifiers |= ShiftMask;
        }
        if (code == m_keycode && modifiers == m_modifiers) return true;
        const KeyCode oldCode = m_keycode;
        const unsigned int oldModifiers = m_modifiers;
        clearGrabs();
        m_keycode = code;
        m_modifiers = modifiers;
        m_failedWindows.clear();
        if (!refreshWindows()) {
            clearGrabs();
            m_keycode = oldCode;
            m_modifiers = oldModifiers;
            m_failedWindows.clear();
            refreshWindows();
            if (error) *error = "X11 could not reserve the GIF key for the running game. "
                                "Choose another shortcut; the previous one was restored.";
            return false;
        }
        m_pressed = false;
        if (m_keycode && !m_pids.empty()) m_timer.start();
        else m_timer.stop();
        return true;
    }

    void sync(const std::vector<std::unique_ptr<DetachedProcessTracker>>& trackers) {
        m_pids.clear();
        for (const auto& tracker : trackers)
            if (tracker->state() != DetachedProcessState::Exited)
                m_pids.push_back(tracker->pid());
        if (m_pids.empty() || !m_keycode) {
            m_timer.stop();
            clearGrabs();
            m_failedWindows.clear();
            m_pressed = false;
            return;
        }
        refreshWindows();
        if (!m_timer.isActive()) m_timer.start();
    }

private:
    void clearGrabs() {
        if (!m_display || m_windows.empty()) return;
        int code = 0;
        {
            X11HotkeyErrors errors(m_display, code);
            for (const Window window : m_windows)
                for (const unsigned int mask : m_ignoredMasks)
                    XUngrabKey(m_display, m_keycode, m_modifiers | mask, window);
        }
        m_windows.clear();
    }

    bool refreshWindows() {
        if (!m_display || !m_keycode) return true;
        std::vector<Window> wanted;
        for (const auto pid : m_pids) {
            const auto candidate = x11GameWindow(m_display, pid);
            if (candidate.window) wanted.push_back(candidate.window);
        }
        for (auto it = m_windows.begin(); it != m_windows.end();) {
            if (std::find(wanted.begin(), wanted.end(), *it) != wanted.end()) {
                ++it;
                continue;
            }
            int code = 0;
            {
                X11HotkeyErrors errors(m_display, code);
                for (const unsigned int mask : m_ignoredMasks)
                    XUngrabKey(m_display, m_keycode, m_modifiers | mask, *it);
            }
            it = m_windows.erase(it);
            m_pressed = false;
        }
        for (const Window window : wanted) {
            if (std::find(m_windows.begin(), m_windows.end(), window) != m_windows.end() ||
                std::find(m_failedWindows.begin(), m_failedWindows.end(), window) !=
                    m_failedWindows.end()) continue;
            int code = 0;
            {
                X11HotkeyErrors errors(m_display, code);
                for (const unsigned int mask : m_ignoredMasks)
                    XGrabKey(m_display, m_keycode, m_modifiers | mask, window,
                             False, GrabModeAsync, GrabModeAsync);
            }
            if (code) {
                int ignored = 0;
                {
                    X11HotkeyErrors errors(m_display, ignored);
                    for (const unsigned int mask : m_ignoredMasks)
                        XUngrabKey(m_display, m_keycode, m_modifiers | mask, window);
                }
                m_failedWindows.push_back(window);
                m_warning("GIF shortcut unavailable for this game window; "
                          "use Tools or choose another shortcut in Settings.");
                return false;
            }
            m_windows.push_back(window);
        }
        m_failedWindows.erase(std::remove_if(m_failedWindows.begin(),
            m_failedWindows.end(), [&wanted](Window window) {
                return std::find(wanted.begin(), wanted.end(), window) == wanted.end();
            }), m_failedWindows.end());
        return true;
    }

    void poll() {
        while (m_display && XPending(m_display)) {
            XEvent event{};
            XNextEvent(m_display, &event);
            if ((event.type != KeyPress && event.type != KeyRelease) ||
                event.xkey.keycode != m_keycode ||
                std::find(m_windows.begin(), m_windows.end(), event.xkey.window) ==
                    m_windows.end()) continue;
            if (event.type == KeyRelease) {
                if (!m_detectableRepeat && XPending(m_display)) {
                    XEvent next{};
                    XPeekEvent(m_display, &next);
                    if (next.type == KeyPress &&
                        next.xkey.keycode == event.xkey.keycode &&
                        next.xkey.time == event.xkey.time)
                        continue; // Synthetic release caused by X11 key repeat.
                }
                m_pressed = false;
                continue;
            }
            const unsigned int keyboard = ShiftMask | LockMask | ControlMask |
                Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask;
            unsigned int ignored = 0;
            for (const unsigned int mask : m_ignoredMasks) ignored |= mask;
            if ((event.xkey.state & keyboard & ~ignored) != m_modifiers || m_pressed)
                continue;
            Window focus = None;
            int revert = 0;
            {
                X11CaptureQuery query(m_display);
                XGetInputFocus(m_display, &focus, &revert);
            }
            if (!x11ContainsFocus(m_display, event.xkey.window, focus)) continue;
            m_pressed = true;
            QTimer::singleShot(0, m_owner, m_trigger);
        }
        if (++m_pollCount % 5 == 0) refreshWindows();
    }

    QObject* m_owner;
    std::function<void()> m_trigger;
    std::function<void(const QString&)> m_warning;
    Display* m_display = nullptr;
    QTimer m_timer;
    std::vector<std::int64_t> m_pids;
    std::vector<Window> m_windows;
    std::vector<Window> m_failedWindows;
    std::vector<unsigned int> m_ignoredMasks;
    KeyCode m_keycode = 0;
    unsigned int m_modifiers = 0;
    int m_pollCount = 0;
    bool m_pressed = false;
    bool m_detectableRepeat = false;
};
#endif

MainWindow::~MainWindow() {
#if defined(GOLIATH_WAYLAND_CAPTURE)
    m_waylandGifHotkey.reset();
#endif
#if defined(GOLIATH_X11_CAPTURE)
    m_x11GifHotkey.reset();
#endif
#if defined(_WIN32)
    if (m_screenshotHotkeyRegistered)
        UnregisterHotKey(reinterpret_cast<HWND>(winId()), kGifHotkeyId);
    if (m_pngHotkeyRegistered)
        UnregisterHotKey(reinterpret_cast<HWND>(winId()), kPngHotkeyId);
    if (g_captureFocusHook) {
        UnhookWinEvent(g_captureFocusHook);
        g_captureFocusHook = nullptr;
        g_captureFocusCallback = {};
    }
#endif
}

void MainWindow::registerScreenshotHotkey() {
#if defined(_WIN32) || defined(GOLIATH_X11_CAPTURE) || defined(GOLIATH_WAYLAND_CAPTURE)
    // The Wayland compositor may reserve Ctrl+Alt+Fn for system actions.
    // Start unassigned so the user can select a key in Settings > Hotkeys.
    const char* defaultGif = "Ctrl+Alt+F12";
#if defined(GOLIATH_WAYLAND_CAPTURE)
    if (QGuiApplication::platformName() == "wayland") defaultGif = "";
#endif
    const QKeySequence gif(QString::fromStdString(
        m_config.get("Hotkeys", "gif", defaultGif)), QKeySequence::PortableText);
    const QKeySequence png(QString::fromStdString(
        m_config.get("Hotkeys", "png", "")), QKeySequence::PortableText);
    QString error;
    if (!applyCaptureHotkeys(gif, png, &error))
        DebugLogger::logInfo(QString("Capture shortcuts unavailable: %1").arg(error));
#endif
}

bool MainWindow::applyCaptureHotkeys(const QKeySequence& gif,
                                      const QKeySequence& png, QString* error,
                                      bool configureWayland) {
#if defined(GOLIATH_WAYLAND_CAPTURE)
    if (QGuiApplication::platformName() == "wayland") {
        Q_UNUSED(png); // PNG capture currently needs the Windows game-window API.
        if (!m_waylandGifHotkey) {
            m_waylandGifHotkey = std::make_shared<WaylandGifHotkey>(
                [this]() {
                    DebugLogger::logInfo("Wayland GIF shortcut activated");
                    if (hasActiveTrackedGameProcess() && !m_gifRecording)
                        QTimer::singleShot(0, this, [this]() { recordWaylandGameGif(); });
                }, this);
            connect(m_waylandGifHotkey.get(),
                    &WaylandGifHotkey::assignedTriggerChanged, this,
                    [this](const QString& trigger) {
                        m_gifCaptureShortcut = trigger;
                        const QString message = trigger.isEmpty()
                            ? QStringLiteral("Wayland GIF shortcut is unassigned")
                            : QString("Wayland GIF shortcut assigned by desktop: %1")
                                  .arg(trigger);
                        DebugLogger::logInfo(message);
                        statusBar()->showMessage(message, 8000);
                    });
        }
        if (!m_waylandGifHotkey->setShortcut(gif, error, configureWayland)) return false;
        if (m_gifCaptureShortcut.isEmpty())
            m_gifCaptureShortcut = m_waylandGifHotkey->assignedTrigger();
        updateScreenshotAction();
        return true;
    }
#endif
    Q_UNUSED(configureWayland);
#if defined(GOLIATH_X11_CAPTURE)
    Q_UNUSED(png); // Linux PNG capture is not available yet.
    if (QGuiApplication::platformName() != "xcb") {
        if (error) *error = "GIF shortcuts require an X11 desktop session.";
        return false;
    }
    if (!m_x11GifHotkey) {
        m_x11GifHotkey = std::make_shared<X11GifHotkey>(this,
            [this]() { recordGameGif(); },
            [this](const QString& warning) {
                DebugLogger::logInfo(warning);
                statusBar()->showMessage(warning, 8000);
            });
    }
    if (!m_x11GifHotkey->setShortcut(gif, error)) return false;
    m_gifCaptureShortcut = gif.toString(QKeySequence::PortableText);
    updateScreenshotAction();
    return true;
#elif !defined(_WIN32)
    Q_UNUSED(gif);
    Q_UNUSED(png);
    if (error) *error = "Global capture shortcuts are currently available on Windows only.";
    return false;
#else
    QString problem;
    const auto gifKey = parseCaptureHotkey(gif, &problem);
    if (!gifKey) {
        if (error) *error = "GIF: " + problem;
        return false;
    }
    const auto pngKey = parseCaptureHotkey(png, &problem);
    if (!pngKey) {
        if (error) *error = "PNG: " + problem;
        return false;
    }
    if (gifKey->virtualKey && gifKey->virtualKey == pngKey->virtualKey &&
        gifKey->modifiers == pngKey->modifiers) {
        if (error) *error = "GIF and PNG cannot use the same shortcut.";
        return false;
    }

    const bool newBare = (gifKey->virtualKey && !gifKey->modifiers) ||
                         (pngKey->virtualKey && !pngKey->modifiers);
    bool installedFocusHook = false;
    if (newBare && !g_captureFocusHook) {
        g_captureFocusHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
            captureFocusChanged, 0, 0, WINEVENT_OUTOFCONTEXT);
        if (!g_captureFocusHook) {
            if (error) *error = "Windows could not monitor game focus for single-key shortcuts.";
            return false;
        }
        installedFocusHook = true;
    }

    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const QKeySequence previousGif(m_gifCaptureShortcut, QKeySequence::PortableText);
    const QKeySequence previousPng(m_pngCaptureShortcut, QKeySequence::PortableText);
    if (m_screenshotHotkeyRegistered) UnregisterHotKey(hwnd, kGifHotkeyId);
    if (m_pngHotkeyRegistered) UnregisterHotKey(hwnd, kPngHotkeyId);
    m_screenshotHotkeyRegistered = false;
    m_pngHotkeyRegistered = false;

    const auto registerOne = [hwnd](int id, const CaptureHotkey& key) {
        return key.virtualKey && RegisterHotKey(
            hwnd, id, key.modifiers | MOD_NOREPEAT, key.virtualKey) != 0;
    };
    if (gifKey->virtualKey) m_screenshotHotkeyRegistered = registerOne(kGifHotkeyId, *gifKey);
    const bool gifOk = !gifKey->virtualKey || m_screenshotHotkeyRegistered;
    unsigned long winError = gifOk ? 0 : GetLastError();
    if (gifOk && pngKey->virtualKey)
        m_pngHotkeyRegistered = registerOne(kPngHotkeyId, *pngKey);
    const bool pngOk = !pngKey->virtualKey || m_pngHotkeyRegistered;
    if (!pngOk) winError = GetLastError();
    if (!gifOk || !pngOk) {
        if (m_screenshotHotkeyRegistered) UnregisterHotKey(hwnd, kGifHotkeyId);
        if (m_pngHotkeyRegistered) UnregisterHotKey(hwnd, kPngHotkeyId);
        m_screenshotHotkeyRegistered = false;
        m_pngHotkeyRegistered = false;
        const auto oldGif = parseCaptureHotkey(previousGif);
        const auto oldPng = parseCaptureHotkey(previousPng);
        if (oldGif && oldGif->virtualKey && oldGif->modifiers)
            m_screenshotHotkeyRegistered = registerOne(kGifHotkeyId, *oldGif);
        if (oldPng && oldPng->virtualKey && oldPng->modifiers)
            m_pngHotkeyRegistered = registerOne(kPngHotkeyId, *oldPng);
        if (installedFocusHook) {
            UnhookWinEvent(g_captureFocusHook);
            g_captureFocusHook = nullptr;
        }
        updateBareCaptureHotkeys();
        if (error) {
            *error = QString("Windows could not register the %1 shortcut "
                             "(error %2). Choose another combination.")
                .arg(gifOk ? "PNG" : "GIF")
                .arg(winError);
            if ((oldGif && oldGif->virtualKey && oldGif->modifiers &&
                 !m_screenshotHotkeyRegistered) ||
                (oldPng && oldPng->virtualKey && oldPng->modifiers &&
                 !m_pngHotkeyRegistered))
                *error += " The previous shortcut could not be restored either.";
        }
        updateScreenshotAction();
        return false;
    }
    m_gifCaptureShortcut = gif.toString(QKeySequence::PortableText);
    m_pngCaptureShortcut = png.toString(QKeySequence::PortableText);
    if (gifKey->virtualKey && !gifKey->modifiers) {
        UnregisterHotKey(hwnd, kGifHotkeyId);
        m_screenshotHotkeyRegistered = false;
    }
    if (pngKey->virtualKey && !pngKey->modifiers) {
        UnregisterHotKey(hwnd, kPngHotkeyId);
        m_pngHotkeyRegistered = false;
    }
    if (newBare) {
        g_captureFocusCallback = [this]() { updateBareCaptureHotkeys(); };
        updateBareCaptureHotkeys();
    } else if (g_captureFocusHook) {
        UnhookWinEvent(g_captureFocusHook);
        g_captureFocusHook = nullptr;
        g_captureFocusCallback = {};
    }
    updateScreenshotAction();
    return true;
#endif
}

void MainWindow::updateBareCaptureHotkeys() {
#if defined(GOLIATH_X11_CAPTURE)
    if (m_x11GifHotkey) m_x11GifHotkey->sync(m_trackedGameProcesses);
#elif defined(_WIN32)
    DWORD foregroundPid = 0;
    if (const HWND foreground = GetForegroundWindow())
        GetWindowThreadProcessId(foreground, &foregroundPid);
    bool gameFocused = false;
    for (const auto& tracker : m_trackedGameProcesses) {
        if (tracker->state() != DetachedProcessState::Exited &&
            static_cast<unsigned long long>(tracker->pid()) == foregroundPid) {
            gameFocused = true;
            break;
        }
    }
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const auto toggleBare = [hwnd, gameFocused](int id, const QString& value,
                                               bool& registered) {
        const auto key = parseCaptureHotkey(
            QKeySequence(value, QKeySequence::PortableText));
        if (!key || !key->virtualKey || key->modifiers) return;
        if (!gameFocused && registered) {
            UnregisterHotKey(hwnd, id);
            registered = false;
        } else if (gameFocused && !registered) {
            registered = RegisterHotKey(hwnd, id, MOD_NOREPEAT, key->virtualKey) != 0;
            if (!registered)
                DebugLogger::logInfo(QString("Single-key capture unavailable (code %1)")
                                         .arg(GetLastError()));
        }
    };
    toggleBare(kGifHotkeyId, m_gifCaptureShortcut, m_screenshotHotkeyRegistered);
    toggleBare(kPngHotkeyId, m_pngCaptureShortcut, m_pngHotkeyRegistered);
#endif
}

#if defined(_WIN32)
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message,
                             qintptr* result) {
    if (eventType == "windows_generic_MSG") {
        const auto* nativeMessage = static_cast<MSG*>(message);
        if (nativeMessage->message == WM_HOTKEY &&
            (nativeMessage->wParam == kGifHotkeyId ||
             nativeMessage->wParam == kPngHotkeyId)) {
            if (hasActiveTrackedGameProcess()) {
                // Handle the hotkey on the next Qt turn so we never open a
                // dialog or capture inside Windows' native message callback.
                const bool gif = nativeMessage->wParam == kGifHotkeyId;
                QTimer::singleShot(0, this, [this, gif]() {
                    if (gif) recordGameGif();
                    else captureGameScreenshotAfter(0);
                });
            }
            *result = 0;
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::updateScreenshotAction() {
    if (m_gifAction) {
#if defined(GOLIATH_WAYLAND_CAPTURE)
        if (QGuiApplication::platformName() == "wayland") {
            m_gifAction->setText("Record game GIF...");
            m_gifAction->setEnabled(hasActiveTrackedGameProcess() && !m_gifRecording);
            m_gifAction->setToolTip("Choose the running game's window in the system sharing dialog");
        } else
#endif
#if defined(GOLIATH_X11_CAPTURE)
        {
            m_gifAction->setEnabled(QGuiApplication::platformName() == "xcb" &&
                                    hasActiveTrackedGameProcess() && !m_gifRecording);
            m_gifAction->setToolTip(
                "Return to the game within three seconds, or use its GIF shortcut while playing");
        }
#elif !defined(_WIN32)
        {
            m_gifAction->setEnabled(false);
        }
#endif
    }
    if (!m_screenshotAction) return;
#if defined(_WIN32)
    m_screenshotAction->setEnabled(hasActiveTrackedGameProcess());
    m_screenshotAction->setToolTip(
        m_screenshotHotkeyRegistered
            ? QString("Tools: PNG after three seconds. GIF: %1; PNG shortcut: %2. "
                      "Change keys in Settings > Hotkeys")
                  .arg(m_gifCaptureShortcut,
                       m_pngCaptureShortcut.isEmpty() ? "unassigned" : m_pngCaptureShortcut)
            : "Return to the running game within three seconds; preview the PNG before saving");
#else
    m_screenshotAction->setEnabled(false);
    m_screenshotAction->setToolTip(
        "Game-window screenshot capture is currently available on Windows");
#endif
}

void MainWindow::captureGameScreenshot() {
    captureGameScreenshotAfter(3000);
}

void MainWindow::recordGameGifAfter(int delayMs) {
#if defined(GOLIATH_WAYLAND_CAPTURE)
    if (QGuiApplication::platformName() == "wayland") {
        recordWaylandGameGif(); // The portal picker replaces the X11 focus delay.
        return;
    }
#endif
#if defined(GOLIATH_X11_CAPTURE)
    if (m_gifRecording || !hasActiveTrackedGameProcess() ||
        QGuiApplication::platformName() != "xcb") return;
    statusBar()->showMessage("Switch to the game; GIF recording in 3 seconds...", delayMs);
    QTimer::singleShot(delayMs, this, [this]() { recordGameGif(); });
#else
    Q_UNUSED(delayMs);
#endif
}

void MainWindow::recordGameGif() {
#if defined(GOLIATH_WAYLAND_CAPTURE)
    if (QGuiApplication::platformName() == "wayland") {
        recordWaylandGameGif();
        return;
    }
#endif
#if defined(_WIN32) || defined(GOLIATH_X11_CAPTURE)
    if (m_gifRecording) return;
    bool validDuration = false;
    int durationSeconds = QString::fromStdString(
        m_config.get("Hotkeys", "gif_duration_seconds", "7"))
                              .toInt(&validDuration);
    if (!validDuration || (durationSeconds != 5 && durationSeconds != 7 &&
                           durationSeconds != 10)) durationSeconds = 7;
    const int targetFrames = durationSeconds * 8;
#if defined(_WIN32)
    const HWND foreground = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &foregroundPid);
#else
    if (QGuiApplication::platformName() != "xcb") return;
    auto xdisplay = std::shared_ptr<Display>(XOpenDisplay(nullptr),
                                             [](Display* d) { if (d) XCloseDisplay(d); });
    if (!xdisplay) {
        QMessageBox::warning(this, "Game GIF", "Cannot connect to the X11 display.");
        return;
    }
    Window foreground = None;
    int revertTo = 0;
    {
        X11CaptureQuery query(xdisplay.get());
        XGetInputFocus(xdisplay.get(), &foreground, &revertTo);
    }
#endif

    std::int64_t pid = 0;
    std::string media;
    int activeGames = 0;
#if defined(GOLIATH_X11_CAPTURE)
    bool matchedFocusedGame = false;
#endif
    for (const auto& tracker : m_trackedGameProcesses) {
        if (tracker->state() == DetachedProcessState::Exited) continue;
        ++activeGames;
#if defined(_WIN32)
        const bool focused = static_cast<unsigned long long>(tracker->pid()) == foregroundPid;
#else
        const auto gameWindow = x11GameWindow(xdisplay.get(), tracker->pid());
        const bool focused = gameWindow.window &&
            x11ContainsFocus(xdisplay.get(), gameWindow.window, foreground);
#endif
        if (focused) {
            pid = tracker->pid();
            media = tracker->media();
#if defined(GOLIATH_X11_CAPTURE)
            matchedFocusedGame = true;
#endif
            break;
        }
        if (activeGames == 1) {
            pid = tracker->pid();
            media = tracker->media();
        }
    }
#if defined(_WIN32)
    if (activeGames > 1 && static_cast<unsigned long long>(pid) != foregroundPid)
        return; // An unrelated window has focus; never record the wrong game.
    if (pid <= 0 || static_cast<unsigned long long>(pid) > MAXDWORD) return;

    GameWindowCandidate candidate;
    candidate.pid = static_cast<DWORD>(pid);
    EnumWindows(findGameWindow, reinterpret_cast<LPARAM>(&candidate));
#else
    if (!matchedFocusedGame || pid <= 0) {
        QMessageBox::information(this, "Game GIF",
            "Focus a game launched from Goliath and try again.");
        return;
    }
    const X11WindowCandidate candidate = x11GameWindow(xdisplay.get(), pid);
    if (!candidate.window ||
        !x11ContainsFocus(xdisplay.get(), candidate.window, foreground)) {
        QMessageBox::information(this, "Game GIF",
            "Focus the running game window and try again.");
        return;
    }
#endif
#if defined(_WIN32)
    if (!candidate.hwnd) {
        QMessageBox::information(this, "Game GIF",
            "The running game window is hidden or minimized.");
        return;
    }
#endif

    auto encoder = std::make_shared<AnimatedGifEncoder>();
    auto* timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(125);
    auto frames = std::make_shared<int>(0);
    auto dimensions = std::make_shared<QSize>();
#if defined(_WIN32)
    auto desktopCapture = std::make_shared<WindowsDesktopCapture>();
    auto desktopAttempts = std::make_shared<int>(0);
    auto fullscreenSample = std::make_shared<QImage>();
    auto fullscreenMoved = std::make_shared<bool>(false);
    auto fullscreenCaptured = std::make_shared<bool>(false);
#endif
    m_gifRecording = true;
    updateScreenshotAction();
    statusBar()->showMessage(
        QString("Recording %1-second game GIF...").arg(durationSeconds),
        durationSeconds * 1000);

    const auto grab = [this, timer, encoder, frames, dimensions,
                       targetFrames, media,
#if defined(_WIN32)
                       hwnd = candidate.hwnd, desktopCapture, desktopAttempts,
                       fullscreenSample, fullscreenMoved, fullscreenCaptured
#else
                       window = candidate.window, xdisplay
#endif
                       ]() {
        QScreen* screen = QGuiApplication::primaryScreen();
#if defined(_WIN32)
        QImage image;
        if (IsWindow(hwnd) && isFullscreenGameWindow(hwnd)) {
            DWORD gamePid = 0;
            GetWindowThreadProcessId(hwnd, &gamePid);
            DWORD foregroundPid = 0;
            if (const HWND foreground = GetForegroundWindow())
                GetWindowThreadProcessId(foreground, &foregroundPid);
            if (gamePid && gamePid == foregroundPid)
                image = desktopCapture->nextFrame(hwnd);
            if (image.isNull() && gamePid == foregroundPid && ++*desktopAttempts < 16)
                return; // A mode switch may temporarily invalidate DXGI duplication.
            if (image.isNull()) {
                timer->stop();
                timer->deleteLater();
                m_gifRecording = false;
                updateScreenshotAction();
                QMessageBox::warning(this, "Game GIF",
                    "Fullscreen capture could not read the game display. "
                    "Keep the game focused, or switch to windowed mode.");
                return;
            }
            if (*frames == 0)
                DebugLogger::logInfo("Windows fullscreen GIF capture: Desktop Duplication");
            const QImage sample = image.scaled(64, 64, Qt::IgnoreAspectRatio,
                                               Qt::FastTransformation)
                                     .convertToFormat(QImage::Format_RGB32);
            if (!fullscreenSample->isNull() && *fullscreenSample != sample)
                *fullscreenMoved = true;
            *fullscreenSample = sample;
            *fullscreenCaptured = true;
        } else if (IsWindow(hwnd) && screen) {
            image = grabGameWindowWithVisibleFallback(screen, hwnd).toImage();
        }
        if (image.isNull()) {
#else
        XWindowAttributes attributes{};
        bool visible = false;
        {
            X11CaptureQuery query(xdisplay.get());
            visible = XGetWindowAttributes(xdisplay.get(), window, &attributes) &&
                      attributes.map_state == IsViewable;
        }
        const QPixmap pixmap = visible && screen
            ? screen->grabWindow(static_cast<WId>(window)) : QPixmap();
        if (pixmap.isNull()) {
#endif
            timer->stop();
            timer->deleteLater();
            m_gifRecording = false;
            updateScreenshotAction();
            QMessageBox::warning(this, "Game GIF",
                "The game window could not be captured. Check the renderer "
                "and window visibility.");
            return;
        }

#if !defined(_WIN32)
        QImage image = pixmap.toImage();
#endif
        if (dimensions->isEmpty()) {
            *dimensions = image.size().scaled(640, 640, Qt::KeepAspectRatio);
            if (!encoder->begin(dimensions->width(), dimensions->height())) {
                timer->stop();
                timer->deleteLater();
                m_gifRecording = false;
                updateScreenshotAction();
                QMessageBox::warning(this, "Game GIF", "Game window is too small to record.");
                return;
            }
        }
        image = image.scaled(*dimensions, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                     .convertToFormat(QImage::Format_Indexed8);
        const auto colors = image.colorTable();
        std::array<std::uint32_t, 256> palette{};
        for (int i = 0; i < colors.size() && i < 256; ++i)
            palette[static_cast<std::size_t>(i)] = colors[i];
        const int delay = (*frames & 1) ? 13 : 12; // Eight frames per second.
        if (!encoder->addFrame(image.constBits(), image.bytesPerLine(),
                               palette.data(), static_cast<std::size_t>(colors.size()), delay)) {
            timer->stop();
            timer->deleteLater();
            m_gifRecording = false;
            updateScreenshotAction();
            QMessageBox::warning(this, "Game GIF", "Could not encode the GIF frame.");
            return;
        }
        if (++*frames < targetFrames) return;

        timer->stop();
        timer->deleteLater();
        m_gifRecording = false;
        updateScreenshotAction();
#if defined(_WIN32)
        if (*fullscreenCaptured && !*fullscreenMoved) {
            QMessageBox::warning(this, "Game GIF",
                "Fullscreen capture returned the same image for every frame. "
                "No static GIF was saved; try windowed mode or another renderer.");
            return;
        }
#endif
        const auto data = encoder->finish();
        if (data.empty()) return;
        const QString base = recordingIdForMedia(QString::fromStdString(media));
        const QDateTime now = QDateTime::currentDateTime();
        const QString folder = QString::fromStdWString(
            (m_paths.base_dir / "recordings" / base.toStdWString() /
             now.toString("yyyy-MM-dd").toStdWString()).wstring());
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
            file.write(reinterpret_cast<const char*>(data.data()),
                       static_cast<qint64>(data.size())) != static_cast<qint64>(data.size()) ||
            !file.commit()) {
            file.cancelWriting();
            QMessageBox::warning(this, "Game GIF", "Could not save the GIF file.");
            return;
        }
        statusBar()->showMessage(QString("Saved game GIF: %1").arg(output), 8000);
        DebugLogger::logInfo(QString("saved game GIF: %1").arg(output));
    };
    connect(timer, &QTimer::timeout, this, grab);
    timer->start();
    grab(); // First frame immediately; rest follow the 125 ms clock.
#endif
}

void MainWindow::captureGameScreenshotAfter(int delayMs) {
#if !defined(_WIN32)
    Q_UNUSED(delayMs);
    QMessageBox::information(this, "Game screenshot",
        "Game-window screenshot capture is currently available on Windows.");
#else
    struct RunningGame {
        std::int64_t pid;
        std::string media;
    };
    std::vector<RunningGame> games;
    QStringList gameNames;
    for (const auto& tracker : m_trackedGameProcesses) {
        if (tracker->state() == DetachedProcessState::Exited) continue;
        games.push_back({tracker->pid(), tracker->media()});
        gameNames << QString("%1 (PID %2)")
            .arg(QString::fromStdString(tracker->media()))
            .arg(static_cast<qlonglong>(tracker->pid()));
    }
    if (games.empty()) {
        QMessageBox::information(this, "Game screenshot",
            "Launch a game from Goliath before taking its screenshot.");
        return;
    }

    int selected = 0;
    if (games.size() > 1) {
        if (delayMs == 0) {
            DWORD foregroundPid = 0;
            if (const HWND foreground = GetForegroundWindow())
                GetWindowThreadProcessId(foreground, &foregroundPid);
            selected = -1;
            for (int i = 0; i < static_cast<int>(games.size()); ++i)
                if (static_cast<unsigned long long>(games[static_cast<std::size_t>(i)].pid)
                    == foregroundPid) selected = i;
            if (selected < 0) return;
        } else {
            bool accepted = false;
            const QString choice = QInputDialog::getItem(
                this, "Game screenshot", "Select the running game:",
                gameNames, 0, false, &accepted);
            if (!accepted) return;
            selected = gameNames.indexOf(choice);
            if (selected < 0) return;
        }
    }

    const auto game = games[static_cast<std::size_t>(selected)];
    if (delayMs > 0)
        statusBar()->showMessage("Switch to the game; screenshot in 3 seconds...", delayMs);
    QTimer::singleShot(delayMs, this, [this, game]() {
    if (game.pid <= 0 ||
        static_cast<unsigned long long>(game.pid) > MAXDWORD) return;
    GameWindowCandidate candidate;
    candidate.pid = static_cast<DWORD>(game.pid);
    EnumWindows(findGameWindow, reinterpret_cast<LPARAM>(&candidate));
    if (!candidate.hwnd) {
        QMessageBox::information(this, "Game screenshot",
            "The JGRF game window is not visible yet, or it is minimized. "
            "Return to the game and try again.");
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QPixmap screenshot = grabGameWindowWithVisibleFallback(screen, candidate.hwnd);
    if (screenshot.isNull()) {
        QMessageBox::warning(this, "Game screenshot",
            "This game window could not be captured with the current "
            "renderer or Windows display mode.");
        return;
    }

    QDialog preview(this);
    preview.setWindowTitle("Review game screenshot");
    auto* layout = new QVBoxLayout(&preview);
    auto* label = new QLabel(&preview);
    label->setPixmap(screenshot.scaled(
        760, 520, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &preview);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &preview, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &preview, &QDialog::reject);
    if (preview.exec() != QDialog::Accepted) return;

    const QString folder = QString::fromStdWString(
        (m_paths.data_dir / "goliath" / "screenshots").wstring());
    if (!QDir().mkpath(folder)) {
        QMessageBox::warning(this, "Game screenshot",
            "Could not create the screenshots folder.");
        return;
    }
    const QString base = recordingIdForMedia(QString::fromStdString(game.media));
    const QString suggestion = QDir(folder).filePath(
        base + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") +
        ".png");
    const QString output = QFileDialog::getSaveFileName(
        this, "Save game screenshot", suggestion, "PNG image (*.png)");
    if (output.isEmpty()) return;

    QSaveFile file(output);
    if (!file.open(QIODevice::WriteOnly) ||
        !screenshot.toImage().save(&file, "PNG") || !file.commit()) {
        file.cancelWriting();
        QMessageBox::warning(this, "Game screenshot",
            "Could not save the screenshot as a PNG file.");
        return;
    }
    DebugLogger::logInfo(QString("saved game screenshot: %1").arg(output));
    });
#endif
}

} // namespace goliath
