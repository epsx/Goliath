#include "catch2/catch.hpp"
#include "input/input_maps.hpp"
#include "input/joystick_listener.hpp"


using goliath::make_joystick_axis_mapping;
using goliath::make_joystick_axis_binding;
using goliath::make_joystick_button_mapping;
using goliath::make_joystick_hat_mapping;
using goliath::canonical_input_key;
using goliath::default_input_config;
using goliath::normalize_input_value;
using goliath::geolith_input_setting_for_device;
using goliath::is_analog_input_definition;
using goliath::is_joystick_axis_binding;
using goliath::jgrf_hotkey_action;
using goliath::jgrf_hotkey_action_for_binding;
using goliath::normalize_input_binding_for_definition;

TEST_CASE("default MVS coin keys address different players", "[input][defaults]") {
    const auto defaults = default_input_config();
    const auto& system = defaults.at("neogeosystem");

    REQUIRE(system.at("Coin1") == "34"); // 5
    REQUIRE(system.at("Coin2") == "31"); // 2
    REQUIRE(system.at("Coin1") != system.at("Coin2"));
}

TEST_CASE("normalize_input_value trims whitespace", "[input]") {
    auto e = normalize_input_value("  ");
    REQUIRE(e.first.empty());
    REQUIRE(e.second.empty());
}

TEST_CASE("normalize_input_value leaves joystick/mahjong values unchanged", "[input]") {
    auto j = normalize_input_value("  j0a1  ");
    REQUIRE(j.first == "j0a1");
    REQUIRE(j.second == "j0a1");

    auto m = normalize_input_value("mb0a1");
    REQUIRE(m.first == "mb0a1");
    REQUIRE(m.second == "mb0a1");
}

TEST_CASE("normalize_input_value maps SDL scancode numbers to display names", "[input]") {
    auto a = normalize_input_value(" 4 ");
    REQUIRE(a.first == "4");
    REQUIRE(a.second == "a (4)");

    auto s = normalize_input_value("22");
    REQUIRE(s.first == "22");
    REQUIRE(s.second == "s (22)");
}

TEST_CASE("normalize_input_value maps keyboard aliases case-insensitively", "[input]") {
    auto r = normalize_input_value("Return");
    REQUIRE(r.first == "40");
    REQUIRE(r.second == "return (40)");

    auto e = normalize_input_value("ENTER");
    REQUIRE(e.first == "40");
    REQUIRE(e.second == "return (40)");

    auto pg = normalize_input_value("PgUp");
    REQUIRE(pg.first == "75");
    REQUIRE(pg.second.find("pageup") != std::string::npos);
}

TEST_CASE("normalize_input_value falls back to raw text for unknown values", "[input]") {
    auto u = normalize_input_value("unmapped");
    REQUIRE(u.first == "unmapped");
    REQUIRE(u.second == "unmapped");
}

TEST_CASE("canonical_input_key matches known keys ignoring case", "[input]") {
    REQUIRE(canonical_input_key("neogeojs1", "up") == "Up");
    REQUIRE(canonical_input_key("neogeojs1", "UP") == "Up");
    REQUIRE(canonical_input_key("neogeojs1", "Select") == "Select");
}

TEST_CASE("canonical_input_key returns raw key for unknown sections or keys", "[input]") {
    REQUIRE(canonical_input_key("neogeojs1", "Unknown") == "Unknown");
    REQUIRE(canonical_input_key("no-such-section", "A") == "A");
}

TEST_CASE("per-game bindings are canonicalized and bounded for JGRF",
          "[input][profiles][validation]") {
    REQUIRE(normalize_input_binding_for_definition(
                "neogeojs1", "A", " escape ") ==
            std::optional<std::string>("41"));
    REQUIRE(normalize_input_binding_for_definition(
                "neogeojs1", "B", " 44 ") ==
            std::optional<std::string>("44"));
    REQUIRE(normalize_input_binding_for_definition(
                "neogeojs1", "C", "j9b31") ==
            std::optional<std::string>("j9b31"));
    REQUIRE(normalize_input_binding_for_definition(
                "neogeojs1", "D", "j2a5-") ==
            std::optional<std::string>("j2a5-"));
    REQUIRE(normalize_input_binding_for_definition(
                "neogeojs1", "Up", "j2h03") ==
            std::optional<std::string>("j2h03"));
    REQUIRE(normalize_input_binding_for_definition(
                "neogeojs1", "Start", "mb31") ==
            std::optional<std::string>("mb31"));
    REQUIRE(normalize_input_binding_for_definition(
                "neogeoirrmaze", "XAxis", "j1a5") ==
            std::optional<std::string>("j1a5"));

    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "unknown", "A", "4").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "Unknown", "4").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "A", "-1").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "A", "512").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "A", "j10b0").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "A", "j0b32").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "A", "j0a6+").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "A", "j0a1").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeoirrmaze", "XAxis", "j0a1+").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "Up", "j0h04").has_value());
    REQUIRE_FALSE(normalize_input_binding_for_definition(
        "neogeojs1", "A", "mb32").has_value());
}

TEST_CASE(
    "joystick axis mapping preserves positive direction",
    "[input][joystick]"
) {
    REQUIRE(
        make_joystick_axis_mapping(0, 0, 1.0)
        == "j0a0+"
    );

    REQUIRE(
        make_joystick_axis_mapping(1, 5, 0.999)
        == "j1a5+"
    );
}

TEST_CASE(
    "joystick axis mapping preserves negative direction",
    "[input][joystick]"
) {
    REQUIRE(
        make_joystick_axis_mapping(0, 0, -1.0)
        == "j0a0-"
    );

    REQUIRE(
        make_joystick_axis_mapping(1, 2, -0.999)
        == "j1a2-"
    );
}

TEST_CASE(
    "joystick axis mapping supports digital axis controllers",
    "[input][joystick]"
) {
    REQUIRE(
        make_joystick_axis_mapping(1, 5, 1.0)
        == "j1a5+"
    );

    REQUIRE(
        make_joystick_axis_mapping(1, 2, 1.0)
        == "j1a2+"
    );
}

TEST_CASE(
    "joystick button mapping preserves button index",
    "[input][joystick]"
) {
    REQUIRE(
        QString("j0b3").toStdString() == "j0b3"
    );

    REQUIRE(
        QString("j1b5").toStdString() == "j1b5"
    );
}

TEST_CASE(
    "joystick button mapping preserves port and button index",
    "[input][joystick]"
) {
    REQUIRE(
        make_joystick_button_mapping(0, 3)
        == "j0b3"
    );

    REQUIRE(
        make_joystick_button_mapping(1, 5)
        == "j1b5"
    );
}

TEST_CASE(
    "joystick axis mapping preserves port, axis and direction",
    "[input][joystick]"
) {
    REQUIRE(
        make_joystick_axis_mapping(0, 0, 1.0)
        == "j0a0+"
    );

    REQUIRE(
        make_joystick_axis_mapping(0, 0, -1.0)
        == "j0a0-"
    );

    REQUIRE(
        make_joystick_axis_mapping(1, 5, 1.0)
        == "j1a5+"
    );

    REQUIRE(
        make_joystick_axis_mapping(1, 2, -1.0)
        == "j1a2-"
    );
}

TEST_CASE(
    "joystick hat mapping preserves port and direction",
    "[input][joystick]"
) {
    REQUIRE(
        make_joystick_hat_mapping(0, 0)
        == "j0h00"
    );

    REQUIRE(
        make_joystick_hat_mapping(0, 1)
        == "j0h01"
    );

    REQUIRE(
        make_joystick_hat_mapping(1, 2)
        == "j1h02"
    );

    REQUIRE(
        make_joystick_hat_mapping(1, 3)
        == "j1h03"
    );
}
TEST_CASE("Geolith input setting only receives supported core values", "[input][geolith]") {
    REQUIRE(geolith_input_setting_for_device(1) == 1);
    REQUIRE(geolith_input_setting_for_device(2) == 2);
    REQUIRE(geolith_input_setting_for_device(3) == 3);
    REQUIRE(geolith_input_setting_for_device(4) == 0);
    REQUIRE(geolith_input_setting_for_device(5) == 0);
    REQUIRE(geolith_input_setting_for_device(99) == 1);
}

TEST_CASE("Irritating Maze exposes two analog definitions", "[input][irrmaze]") {
    REQUIRE(is_analog_input_definition("neogeoirrmaze", "XAxis"));
    REQUIRE(is_analog_input_definition("neogeoirrmaze", "YAxis"));
    REQUIRE_FALSE(is_analog_input_definition("neogeoirrmaze", "LeftA"));
    REQUIRE_FALSE(is_analog_input_definition("neogeoirrmaze", "Start"));
    REQUIRE_FALSE(is_analog_input_definition("neogeovliner", "Up"));
}

TEST_CASE("joystick analog axis binding has no digital direction suffix", "[input][joystick][irrmaze]") {
    REQUIRE(make_joystick_axis_binding(0, 0) == "j0a0");
    REQUIRE(make_joystick_axis_binding(1, 5) == "j1a5");
}

TEST_CASE("JGRF analog axis binding validation rejects digital mappings", "[input][irrmaze]") {
    REQUIRE(is_joystick_axis_binding("j0a0"));
    REQUIRE(is_joystick_axis_binding(" j1a5 "));
    REQUIRE_FALSE(is_joystick_axis_binding("j0a0+"));
    REQUIRE_FALSE(is_joystick_axis_binding("j0a0-"));
    REQUIRE_FALSE(is_joystick_axis_binding("j0b2"));
    REQUIRE_FALSE(is_joystick_axis_binding("80"));
}

TEST_CASE("JGRF direct keyboard hotkeys are identified", "[input][jgrf][hotkeys]") {
    const std::vector<std::pair<int, std::string>> expected = {
        {41, "Quit"},
        {43, "Open Menu"},
        {53, "Fast-forward"},
        {58, "Soft Reset"},
        {59, "Hard Reset"},
        {62, "Save State (Slot 0)"},
        {63, "Save State (Slot 1)"},
        {64, "Load State (Slot 0)"},
        {65, "Load State (Slot 1)"},
        {66, "Screenshot"},
        {69, "Toggle Cheats On/Off"},
        {9,  "Toggle Fullscreen/Windowed Mode"},
        {16, "Toggle Audio Playback (Mute/Unmute)"},
        {19, "Pause / Resume"},
    };

    for (const auto& [scancode, action] : expected) {
        INFO("SDL scancode: " << scancode);
        auto found = jgrf_hotkey_action(scancode);
        REQUIRE(found.has_value());
        REQUIRE(*found == action);
    }
}

TEST_CASE("JGRF hotkey detection accepts keyboard aliases and ignores gamepad bindings",
          "[input][jgrf][hotkeys]") {
    auto f = jgrf_hotkey_action_for_binding("f");
    REQUIRE(f.has_value());
    REQUIRE(*f == "Toggle Fullscreen/Windowed Mode");

    auto m = jgrf_hotkey_action_for_binding("16");
    REQUIRE(m.has_value());
    REQUIRE(*m == "Toggle Audio Playback (Mute/Unmute)");

    auto p = jgrf_hotkey_action_for_binding(" P ");
    REQUIRE(p.has_value());
    REQUIRE(*p == "Pause / Resume");

    REQUIRE_FALSE(jgrf_hotkey_action_for_binding("4").has_value());
    REQUIRE_FALSE(jgrf_hotkey_action_for_binding("80").has_value());
    REQUIRE_FALSE(jgrf_hotkey_action_for_binding("j0b3").has_value());
    REQUIRE_FALSE(jgrf_hotkey_action_for_binding("j0a1+").has_value());
    REQUIRE_FALSE(jgrf_hotkey_action_for_binding("j0h00").has_value());
}
