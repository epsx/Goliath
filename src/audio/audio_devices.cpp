#include "audio_devices.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // MinGW-w64 declares the Core Audio COM GUIDs in the headers, but its
    // uuid import library does not provide all of them.  Instantiate the
    // GUIDs in this translation unit so IID_IAudioSessionManager2,
    // IID_ISimpleAudioVolume, CLSID_MMDeviceEnumerator, etc. link correctly.
    #ifndef INITGUID
        #define INITGUID
    #endif
    #include <initguid.h>
    #include <mmdeviceapi.h>
    #include <audiopolicy.h>
#endif

namespace goliath {

bool ensure_sdl_audio_init() {
    if (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) return true;
    return SDL_InitSubSystem(SDL_INIT_AUDIO);
}

AudioDeviceSnapshot query_audio_playback_devices() {
    AudioDeviceSnapshot result;

    if (!ensure_sdl_audio_init()) {
        const char* err = SDL_GetError();
        result.error = err ? err : "SDL audio initialization failed";
        return result;
    }

    result.initialized = true;

    SDL_ClearError();
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
    if (!devices) {
        const char* err = SDL_GetError();
        if (err && *err) result.error = err;
        return result;
    }

    result.playback_devices.reserve(std::max(count, 0));
    for (int i = 0; i < count; ++i) {
        const char* name = SDL_GetAudioDeviceName(devices[i]);
        if (name && *name) {
            result.playback_devices.emplace_back(name);
        } else {
            result.playback_devices.emplace_back("Playback Device " + std::to_string(i + 1));
        }
    }

    SDL_free(devices);
    return result;
}

int clamp_audio_volume(int percent) {
    return std::clamp(percent, 0, 100);
}

#if defined(_WIN32)
namespace {

template <typename T>
void safe_release(T*& ptr) {
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

class ComInitGuard {
public:
    ComInitGuard() {
        m_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_shouldUninitialize = SUCCEEDED(m_result);
    }

    ~ComInitGuard() {
        if (m_shouldUninitialize) CoUninitialize();
    }

    bool usable() const {
        // Qt or another library may already have initialized COM using a
        // different apartment model. RPC_E_CHANGED_MODE means COM is already
        // initialized on this thread, so the Core Audio calls can still be used.
        return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT m_result = E_FAIL;
    bool m_shouldUninitialize = false;
};

bool set_volume_on_device(IMMDevice* device, DWORD process_id, float gain) {
    if (!device) return false;

    IAudioSessionManager2* manager = nullptr;
    HRESULT hr = device->Activate(IID_IAudioSessionManager2, CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(&manager));
    if (FAILED(hr) || !manager) return false;

    IAudioSessionEnumerator* sessions = nullptr;
    hr = manager->GetSessionEnumerator(&sessions);
    if (FAILED(hr) || !sessions) {
        safe_release(manager);
        return false;
    }

    int count = 0;
    sessions->GetCount(&count);
    bool applied = false;

    for (int i = 0; i < count && !applied; ++i) {
        IAudioSessionControl* control = nullptr;
        if (FAILED(sessions->GetSession(i, &control)) || !control) continue;

        IAudioSessionControl2* control2 = nullptr;
        hr = control->QueryInterface(IID_IAudioSessionControl2,
                                     reinterpret_cast<void**>(&control2));
        if (SUCCEEDED(hr) && control2) {
            DWORD session_pid = 0;
            if (SUCCEEDED(control2->GetProcessId(&session_pid)) && session_pid == process_id) {
                ISimpleAudioVolume* volume = nullptr;
                hr = control2->QueryInterface(IID_ISimpleAudioVolume,
                                              reinterpret_cast<void**>(&volume));
                if (SUCCEEDED(hr) && volume) {
                    applied = SUCCEEDED(volume->SetMasterVolume(gain, nullptr));
                }
                safe_release(volume);
            }
        }

        safe_release(control2);
        safe_release(control);
    }

    safe_release(sessions);
    safe_release(manager);
    return applied;
}

} // namespace
#endif

bool set_process_audio_volume(std::uint32_t process_id, int percent) {
#if defined(_WIN32)
    if (process_id == 0) return false;

    ComInitGuard com;
    if (!com.usable()) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                  IID_IMMDeviceEnumerator,
                                  reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) return false;

    IMMDeviceCollection* devices = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
    if (FAILED(hr) || !devices) {
        safe_release(enumerator);
        return false;
    }

    UINT count = 0;
    devices->GetCount(&count);

    const float gain = static_cast<float>(clamp_audio_volume(percent)) / 100.0f;
    bool applied = false;

    for (UINT i = 0; i < count && !applied; ++i) {
        IMMDevice* device = nullptr;
        if (SUCCEEDED(devices->Item(i, &device)) && device) {
            applied = set_volume_on_device(device, static_cast<DWORD>(process_id), gain);
        }
        safe_release(device);
    }

    safe_release(devices);
    safe_release(enumerator);
    return applied;
#else
    (void)process_id;
    (void)percent;
    return false;
#endif
}

} // namespace goliath
