// audio_devices.hpp — SDL3 playback-device discovery used by Settings > Audio
// and the pre-launch audio check. JGRF itself remains unmodified and continues
// to use SDL's system-default playback device.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace goliath {

struct AudioDeviceSnapshot {
    bool initialized = false;
    std::vector<std::string> playback_devices;
    std::string error;

    bool has_playback_device() const { return !playback_devices.empty(); }
};

// Lazily initializes SDL's audio subsystem. Safe to call repeatedly from the
// GUI/main thread.
bool ensure_sdl_audio_init();

// Returns the currently connected SDL3 playback devices. SDL3 deliberately
// does not expose the friendly name of the current "system default" device;
// JGRF stock uses SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, so Goliath presents that
// as a generic "System Default" output and lists the physical devices only as
// status/information.
AudioDeviceSnapshot query_audio_playback_devices();

// Goliath stores a frontend-side volume percentage. JGRF has no stock volume
// setting, so on Windows Goliath applies this to the launched jollygood.exe
// Core Audio session without modifying JGRF.
int clamp_audio_volume(int percent);

// Returns true once a matching Windows audio session was found and updated.
// On non-Windows platforms this returns false (the UI explains the limitation).
bool set_process_audio_volume(std::uint32_t process_id, int percent);

} // namespace goliath
