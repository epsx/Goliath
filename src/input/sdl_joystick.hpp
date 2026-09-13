// sdl_joystick.hpp — thin SDL3 joystick enumeration helper shared by
// SettingsDialog (device picker combo) and JoystickListener (polling
// thread). Centralizing SDL_Init/enumeration here avoids double-initializing
// the joystick subsystem and keeps SDL usage in one place.
//
// Why SDL3 instead of the legacy winmm/evdev code: the actual emulator core
// (jgrf/geolith, see jgrf/src/input.c) is itself SDL3-based and assigns
// "j<port>" identifiers to physical joysticks purely by hotplug connection
// order (first device connected -> port 0, second -> port 1, ...). Using
// SDL3 here too means our enumeration order matches what jgrf will see,
// instead of guessing via a different, platform-specific API. It's still a
// best-effort match (jgrf assigns ports at its own runtime, in a separate
// process, so if devices are unplugged/replugged in a different order
// between configuring input here and launching the game, the physical
// device behind a given port can shift) — but it's as close as a frontend
// can get without controlling jgrf itself.
#pragma once

#include <QString>

#include <cstdint>
#include <vector>

namespace goliath {

struct DetectedJoystick {
    int port = 0;              // 0-based position in the current enumeration order
    std::uint32_t instanceId = 0; // SDL_JoystickID of the underlying device
    QString name;              // Friendly device name (falls back to "Joystick N")
};

// Lazily initializes SDL's joystick subsystem (SDL_INIT_JOYSTICK). Safe to
// call repeatedly. Returns false if SDL failed to initialize.
bool ensure_sdl_joystick_init();

// Re-scans currently connected joysticks. Order matches SDL_GetJoysticks(),
// which reflects connection order (same assumption jgrf's own hotplug
// handler relies on).
std::vector<DetectedJoystick> list_joysticks();

} // namespace goliath
