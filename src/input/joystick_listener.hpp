// joystick_listener.hpp — SDL3-backed input listener used by SettingsDialog
// to capture a single button/axis/hat press for remapping.
//
// Emits mapped(code) once. Digital capture uses "j<port>b<N>",
// "j<port>a<N>+/-", or "j<port>h0<D>". Analog-axis capture emits the
// directionless JGRF axis form "j<port>a<N>".
#pragma once

#include <QString>
#include <QThread>

#include <atomic>
#include <cstdint>
#include <string>

namespace goliath {

std::string make_joystick_axis_mapping(int port, int axis, double value);
std::string make_joystick_axis_binding(int port, int axis);
std::string make_joystick_button_mapping(int port, int button);
std::string make_joystick_hat_mapping(int port, int direction);

enum class JoystickCaptureMode {
    Digital,
    AnalogAxis,
};

class JoystickListener : public QThread {
    Q_OBJECT
public:
    // instanceId: SDL_JoystickID of the physical device to poll (from
    // list_joysticks()). port: value embedded in the emitted "j<port>..."
    // code - normally the same enumeration index the device had in
    // list_joysticks() at selection time.
    explicit JoystickListener(std::uint32_t instanceId, int port,
                              JoystickCaptureMode mode = JoystickCaptureMode::Digital,
                              QObject* parent = nullptr);

    // Requests cancellation and, when called from another thread, waits until
    // run() has returned so QObject-parent destruction remains safe.
    void stop();

signals:
    void mapped(QString code);

protected:
    void run() override;

private:
    std::uint32_t m_instanceId;
    int m_port;
    JoystickCaptureMode m_mode;
    std::atomic<bool> m_running{true};
};

} // namespace goliath
