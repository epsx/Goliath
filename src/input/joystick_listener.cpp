#include "joystick_listener.hpp"
#include "input_maps.hpp"
#include "sdl_joystick.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace goliath {

std::string make_joystick_axis_mapping(int port, int axis, double value) {
    const char direction = value >= 0.0 ? '+' : '-';

    return "j" +
           std::to_string(port) +
           "a" +
           std::to_string(axis) +
           direction;
}

std::string make_joystick_axis_binding(int port, int axis) {
    return "j" +
           std::to_string(port) +
           "a" +
           std::to_string(axis);
}

std::string make_joystick_button_mapping(int port, int button) {
    return "j" +
           std::to_string(port) +
           "b" +
           std::to_string(button);
}

std::string make_joystick_hat_mapping(int port, int direction) {
    return "j" +
           std::to_string(port) +
           "h0" +
           std::to_string(direction);
}

JoystickListener::JoystickListener(std::uint32_t instanceId, int port,
                                   JoystickCaptureMode mode, QObject* parent)
    : QThread(parent), m_instanceId(instanceId), m_port(port), m_mode(mode) {}

void JoystickListener::stop() {
    m_running.store(false, std::memory_order_release);
    // The QObject parent must never destroy a running QThread. The poll loop
    // sleeps in short bursts, so a normal stop completes promptly; wait until
    // it has actually returned instead of turning a timeout into a lifetime
    // race during dialog/widget destruction.
    if (QThread::currentThread() != this) wait();
}

void JoystickListener::run() {
    if (!ensure_sdl_joystick_init()) return;

    SDL_Joystick* joystick = SDL_OpenJoystick(static_cast<SDL_JoystickID>(m_instanceId));
    if (!joystick) return;

    const int numButtons = std::clamp(
        SDL_GetNumJoystickButtons(joystick), 0,
        kJgrfJoystickButtonCount);
    const int numAxes = std::clamp(
        SDL_GetNumJoystickAxes(joystick), 0,
        kJgrfJoystickAxisCount);
    // Only hat 0 is polled: jgrf's own hat handler hardcodes hat index 0 in
    // the "j<port>h0<dir>" codes it writes.
    bool hasHat = SDL_GetNumJoystickHats(joystick) > 0;

    std::vector<bool> prevButtons(numButtons > 0 ? numButtons : 0, false);
    std::vector<double> prevAxes(numAxes > 0 ? numAxes : 0, 0.0);
    // Index order matches jgrf's SDL_EVENT_JOYSTICK_HAT_MOTION handler:
    // Up=0, Down=1, Left=2, Right=3.
    bool prevHatDir[4] = {false, false, false, false};

    SDL_UpdateJoysticks();
    for (int i = 0; i < numButtons; ++i) {
        prevButtons[i] = SDL_GetJoystickButton(joystick, i);
    }
    for (int i = 0; i < numAxes; ++i) {
        prevAxes[i] = SDL_GetJoystickAxis(joystick, i) / 32768.0; // -1..1
    }
    if (hasHat) {
        Uint8 hat = SDL_GetJoystickHat(joystick, 0);
        prevHatDir[0] = (hat & SDL_HAT_UP) != 0;
        prevHatDir[1] = (hat & SDL_HAT_DOWN) != 0;
        prevHatDir[2] = (hat & SDL_HAT_LEFT) != 0;
        prevHatDir[3] = (hat & SDL_HAT_RIGHT) != 0;
    }

    while (m_running.load(std::memory_order_acquire)) {
        SDL_UpdateJoysticks();

        if (m_mode == JoystickCaptureMode::Digital) {
            for (int i = 0; i < numButtons; ++i) {
                bool now = SDL_GetJoystickButton(joystick, i);
                if (now && !prevButtons[i]) {
                    emit mapped(QString::fromStdString(
                        make_joystick_button_mapping(m_port, i)));
                    SDL_CloseJoystick(joystick);
                    return;
                }
                prevButtons[i] = now;
            }
        }

        for (int i = 0; i < numAxes; ++i) {
            double value = SDL_GetJoystickAxis(joystick, i) / 32768.0;
            const bool activeNow = std::fabs(value) > 0.8;
            const bool axisChanged = std::fabs(value - prevAxes[i]) > 0.5;

            if (activeNow && axisChanged) {
                const std::string mapping =
                    m_mode == JoystickCaptureMode::AnalogAxis
                        ? make_joystick_axis_binding(m_port, i)
                        : make_joystick_axis_mapping(m_port, i, value);
                emit mapped(QString::fromStdString(mapping));
                SDL_CloseJoystick(joystick);
                return;
            }

            prevAxes[i] = value;
        }

        if (m_mode == JoystickCaptureMode::Digital && hasHat) {
            Uint8 hat = SDL_GetJoystickHat(joystick, 0);
            bool hatDir[4] = {
                (hat & SDL_HAT_UP) != 0,
                (hat & SDL_HAT_DOWN) != 0,
                (hat & SDL_HAT_LEFT) != 0,
                (hat & SDL_HAT_RIGHT) != 0,
            };
            for (int d = 0; d < 4; ++d) {
                if (hatDir[d] && !prevHatDir[d]) {
                    emit mapped(QString::fromStdString(
                        make_joystick_hat_mapping(m_port, d)));
                    SDL_CloseJoystick(joystick);
                    return;
                }
                prevHatDir[d] = hatDir[d];
            }
        }

        msleep(50);
    }

    SDL_CloseJoystick(joystick);
}

} // namespace goliath
