#include "sdl_joystick.hpp"

#include <SDL3/SDL.h>

namespace goliath {

bool ensure_sdl_joystick_init() {
    static const bool initialized = [] {
        if (SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK) return true;
        return SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    }();
    return initialized;
}

std::vector<DetectedJoystick> list_joysticks() {
    std::vector<DetectedJoystick> result;
    if (!ensure_sdl_joystick_init()) return result;

    // Process any pending hotplug add/remove events so the enumeration
    // below reflects devices connected/disconnected since the last call.
    SDL_UpdateJoysticks();
    SDL_PumpEvents();

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (!ids) return result;

    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        DetectedJoystick dj;
        dj.port = i;
        dj.instanceId = ids[i];
        const char* name = SDL_GetJoystickNameForID(ids[i]);
        dj.name = name ? QString::fromUtf8(name) : QString("Joystick %1").arg(i);
        result.push_back(dj);
    }

    SDL_free(ids);
    return result;
}

} // namespace goliath
