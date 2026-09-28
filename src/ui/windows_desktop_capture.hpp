#pragma once

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <QImage>
#include <memory>

namespace goliath {

// Desktop Duplication reads the displayed output, including fullscreen games.
// The returned image is cropped to the game's client area.
bool isFullscreenGameWindow(HWND window);

class WindowsDesktopCapture {
public:
    WindowsDesktopCapture();
    ~WindowsDesktopCapture();
    WindowsDesktopCapture(const WindowsDesktopCapture&) = delete;
    WindowsDesktopCapture& operator=(const WindowsDesktopCapture&) = delete;

    QImage nextFrame(HWND window);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace goliath
#endif
