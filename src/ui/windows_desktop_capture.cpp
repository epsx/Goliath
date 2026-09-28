#include "ui/windows_desktop_capture.hpp"

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi1_2.h>

#include <QRect>

#include <cstring>

namespace goliath {
namespace {
template <typename T> void release(T*& value) {
    if (value) { value->Release(); value = nullptr; }
}

bool clientOnScreen(HWND window, RECT& client, HMONITOR& monitor) {
    if (!IsWindow(window) || IsIconic(window) || !GetClientRect(window, &client)) return false;
    POINT origin{client.left, client.top};
    if (!ClientToScreen(window, &origin)) return false;
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return false;
    client = {origin.x, origin.y, origin.x + width, origin.y + height};
    monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    return monitor != nullptr;
}
} // namespace

bool isFullscreenGameWindow(HWND window) {
    RECT client{};
    HMONITOR monitor = nullptr;
    if (!clientOnScreen(window, client, monitor)) return false;
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return false;
    constexpr int tolerance = 8;
    return client.left <= info.rcMonitor.left + tolerance &&
           client.top <= info.rcMonitor.top + tolerance &&
           client.right >= info.rcMonitor.right - tolerance &&
           client.bottom >= info.rcMonitor.bottom - tolerance;
}

struct WindowsDesktopCapture::Impl {
    IDXGIFactory1* factory = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    IDXGIOutput1* output = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11Texture2D* staging = nullptr;
    HMONITOR monitor = nullptr;
    RECT desktop{};
    QImage last;

    ~Impl() { clear(); }

    void clear() {
        release(staging);
        release(duplication);
        release(context);
        release(device);
        release(output);
        release(adapter);
        release(factory);
        monitor = nullptr;
        last = {};
    }

    bool start(HMONITOR wanted) {
        clear();
        if (FAILED(CreateDXGIFactory1(IID_IDXGIFactory1,
                                      reinterpret_cast<void**>(&factory)))) return false;
        for (UINT a = 0; ; ++a) {
            IDXGIAdapter1* foundAdapter = nullptr;
            if (FAILED(factory->EnumAdapters1(a, &foundAdapter)) || !foundAdapter) break;
            for (UINT o = 0; ; ++o) {
                IDXGIOutput* foundOutput = nullptr;
                if (FAILED(foundAdapter->EnumOutputs(o, &foundOutput)) || !foundOutput) break;
                DXGI_OUTPUT_DESC info{};
                const bool match = SUCCEEDED(foundOutput->GetDesc(&info)) &&
                                   info.Monitor == wanted && info.AttachedToDesktop;
                if (match && SUCCEEDED(foundOutput->QueryInterface(
                        IID_IDXGIOutput1, reinterpret_cast<void**>(&output)))) {
                    adapter = foundAdapter;
                    desktop = info.DesktopCoordinates;
                }
                foundOutput->Release();
                if (adapter) break;
            }
            if (adapter) break;
            foundAdapter->Release();
        }
        if (!adapter || !output ||
            FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                     D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                     D3D11_SDK_VERSION, &device, nullptr, &context)) ||
            FAILED(output->DuplicateOutput(device, &duplication))) {
            clear();
            return false;
        }
        DXGI_OUTDUPL_DESC mode{};
        duplication->GetDesc(&mode);
        // DXGI returns unrotated surfaces in portrait modes; refuse an incorrect crop.
        if ((mode.Rotation != DXGI_MODE_ROTATION_IDENTITY &&
             mode.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED) ||
            !mode.ModeDesc.Width || !mode.ModeDesc.Height) {
            clear();
            return false;
        }
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = mode.ModeDesc.Width;
        desc.Height = mode.ModeDesc.Height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) {
            clear();
            return false;
        }
        monitor = wanted;
        return true;
    }
};

WindowsDesktopCapture::WindowsDesktopCapture() : m_impl(std::make_unique<Impl>()) {}
WindowsDesktopCapture::~WindowsDesktopCapture() = default;

QImage WindowsDesktopCapture::nextFrame(HWND window) {
    RECT client{};
    HMONITOR monitor = nullptr;
    if (!clientOnScreen(window, client, monitor)) return {};
    auto& state = *m_impl;
    if ((!state.duplication || monitor != state.monitor) && !state.start(monitor)) return {};

    DXGI_OUTDUPL_FRAME_INFO info{};
    IDXGIResource* resource = nullptr;
    const HRESULT acquired = state.duplication->AcquireNextFrame(10, &info, &resource);
    if (acquired == DXGI_ERROR_WAIT_TIMEOUT) return state.last;
    if (acquired == DXGI_ERROR_ACCESS_LOST) { state.clear(); return {}; }
    if (FAILED(acquired)) return {};

    ID3D11Texture2D* surface = nullptr;
    bool copied = false;
    if (resource && SUCCEEDED(resource->QueryInterface(
            IID_ID3D11Texture2D, reinterpret_cast<void**>(&surface)))) {
        state.context->CopyResource(state.staging, surface);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(state.context->Map(state.staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            D3D11_TEXTURE2D_DESC desc{};
            state.staging->GetDesc(&desc);
            QImage image(static_cast<int>(desc.Width), static_cast<int>(desc.Height),
                         QImage::Format_ARGB32);
            if (!image.isNull()) {
                for (int y = 0; y < image.height(); ++y)
                    std::memcpy(image.scanLine(y),
                                static_cast<const unsigned char*>(mapped.pData) +
                                    static_cast<size_t>(y) * mapped.RowPitch,
                                static_cast<size_t>(image.width()) * 4);
                const QRect screen(0, 0, image.width(), image.height());
                const QRect game(client.left - state.desktop.left,
                                 client.top - state.desktop.top,
                                 client.right - client.left, client.bottom - client.top);
                const QRect crop = game.intersected(screen);
                if (crop.isValid() && !crop.isEmpty()) {
                    state.last = image.copy(crop);
                    copied = true;
                }
            }
            state.context->Unmap(state.staging, 0);
        }
    }
    release(surface);
    release(resource);
    state.duplication->ReleaseFrame();
    return copied ? state.last : QImage{};
}
} // namespace goliath
#endif
