// input_maps.hpp — static input-remapping tables: which keys each
// geolith_input.ini section expects, their defaults, the SDL scancode
// name tables, and the Qt::Key -> SDL scancode lookup used when the user
// presses a keyboard key while remapping.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace goliath {

// Fixed mapping-array sizes in JGRF 2.0.1. Goliath uses the same limits when
// validating stored bindings and when capturing physical controller input.
inline constexpr int kJgrfJoystickAxisCount = 6;
inline constexpr int kJgrfJoystickButtonCount = 32;
inline constexpr int kJgrfHatDirectionCount = 4;

// section -> canonical key names, in the order Jollygood/geolith expects.
const std::map<std::string, std::vector<std::string>>& input_defs();

// section -> (key -> default SDL scancode string), used when
// geolith_input.ini doesn't exist yet.
const std::map<std::string, std::map<std::string, std::string>>& default_input_config();

// SDL scancode (int) -> human-readable lowercase name.
const std::map<int, std::string>& sdl_scancode_names();

// lowercase name (canonical + aliases, e.g. "enter" -> same code as "return")
// -> SDL scancode.
const std::map<std::string, int>& name_to_sdl_scancode();

// Qt::Key -> SDL scancode, for keys typed on the main keyboard rows.
const std::map<int, int>& qt_key_to_sdl_scancode();

// Qt::Key -> SDL scancode, for the same key codes when Qt::KeypadModifier is
// set (i.e. the physical key was on the numeric keypad).
const std::map<int, int>& qt_keypad_to_sdl_scancode();

// Returns {stored_value, display_text} for a raw input config value.
std::pair<std::string, std::string> normalize_input_value(const std::string& valueIn);

// Canonical key name for a raw key inside a section, ignoring case.
std::string canonical_input_key(const std::string& section, const std::string& key);

// Maps the Input-tab profile id to the value accepted by Geolith's
// [geolith] input setting. V-Liner and Irritating Maze are auto-detected by
// Geolith from the loaded game, so their frontend-only profile ids map to 0
// (Auto), not to invalid core values 4/5.
int geolith_input_setting_for_device(int device);

// True for definitions that represent an emulated analog axis rather than a
// digital button. Geolith currently exposes XAxis/YAxis for Irritating Maze.
bool is_analog_input_definition(const std::string& section, const std::string& key);

// True only for the directionless JGRF analog-axis form (e.g. j0a1).
// A trailing +/- denotes a physical axis acting as a digital button.
bool is_joystick_axis_binding(const std::string& value);

// Canonicalizes and validates one binding for a known Geolith input
// definition. The returned value is safe for JGRF's fixed-size keyboard,
// joystick and mouse mapping arrays; malformed or incompatible bindings are
// rejected instead of being copied into a generated input INI.
std::optional<std::string> normalize_input_binding_for_definition(
    const std::string& section,
    const std::string& key,
    const std::string& value);

// Returns the JGRF frontend action associated with a direct keyboard
// scancode, or std::nullopt when the key is not a frontend hotkey.
std::optional<std::string> jgrf_hotkey_action(int scancode);

// Same check for an input binding as stored in geolith_input.ini. Keyboard
// aliases/names are normalized first; joystick/mouse bindings never match.
std::optional<std::string> jgrf_hotkey_action_for_binding(const std::string& value);

} // namespace goliath
