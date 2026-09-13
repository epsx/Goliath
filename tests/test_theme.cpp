#include "catch2/catch.hpp"

#include "common/theme.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

using goliath::Theme;
using goliath::all_themes;
using goliath::contrast_text_for;
using goliath::find_theme;
using goliath::generate_theme_selector_popup_style;
using goliath::generate_theme_style;
using goliath::theme_selector_popup_text;

namespace {

int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool parse_rgb(const std::string& color, std::array<int, 3>& rgb) {
    if (color.size() != 7 || color.front() != '#') return false;
    for (std::size_t channel = 0; channel < rgb.size(); ++channel) {
        const int high = hex_value(color[1 + channel * 2]);
        const int low = hex_value(color[2 + channel * 2]);
        if (high < 0 || low < 0) return false;
        rgb[channel] = high * 16 + low;
    }
    return true;
}

double linear_channel(int value) {
    const double srgb = static_cast<double>(value) / 255.0;
    return srgb <= 0.04045
        ? srgb / 12.92
        : std::pow((srgb + 0.055) / 1.055, 2.4);
}

double luminance(const std::string& color) {
    std::array<int, 3> rgb{};
    if (!parse_rgb(color, rgb)) return -1.0;
    return
        0.2126 * linear_channel(rgb[0]) +
        0.7152 * linear_channel(rgb[1]) +
        0.0722 * linear_channel(rgb[2]);
}

double contrast_ratio(const std::string& first, const std::string& second) {
    const double first_luminance = luminance(first);
    const double second_luminance = luminance(second);
    if (first_luminance < 0.0 || second_luminance < 0.0) return 0.0;
    const double lighter = std::max(first_luminance, second_luminance);
    const double darker = std::min(first_luminance, second_luminance);
    return (lighter + 0.05) / (darker + 0.05);
}

std::array<std::string, 11> palette(const Theme& theme) {
    return {
        theme.bg_primary,
        theme.bg_secondary,
        theme.bg_tertiary,
        theme.bg_hover,
        theme.accent,
        theme.accent_hover,
        theme.success,
        theme.danger,
        theme.text_primary,
        theme.text_secondary,
        theme.border,
    };
}

bool valid_theme(const Theme& theme) {
    if (theme.key.empty()) return false;
    for (const std::string& color : palette(theme)) {
        std::array<int, 3> rgb{};
        if (!parse_rgb(color, rgb)) return false;
    }
    return true;
}

double minimum_role_contrast(const Theme& theme) {
    const std::array<std::string, 4> backgrounds = {
        theme.accent,
        theme.accent_hover,
        theme.success,
        theme.danger,
    };
    double minimum = 100.0;
    for (const std::string& background : backgrounds) {
        minimum = std::min(
            minimum,
            contrast_ratio(background, contrast_text_for(background)));
    }
    return minimum;
}

bool embeds_palette(const std::string& stylesheet, const Theme& theme) {
    for (const std::string& color : palette(theme)) {
        if (stylesheet.find(color) == std::string::npos) return false;
    }
    return true;
}

bool preserves_branch_tokens(const std::string& stylesheet) {
    return
        stylesheet.find("<<BRANCH_CLOSED>>") != std::string::npos &&
        stylesheet.find("<<BRANCH_OPEN>>") != std::string::npos &&
        stylesheet.find("<<BRANCH_CLOSED_SELECTED>>") != std::string::npos &&
        stylesheet.find("<<BRANCH_OPEN_SELECTED>>") != std::string::npos;
}

} // namespace

TEST_CASE("theme catalog exposes balanced dark and light palettes", "[theme]") {
    const std::vector<std::string> expected_keys = {
        "Dark Modern",
        "Dracula",
        "Tokyo Night",
        "Nord",
        "Gruvbox Dark",
        "GitHub Dark",
        "Goliath Pearl",
        "Warm Ivory",
        "Arctic Blue",
        "Neo Geo Classic",
        "High Contrast Light",
        "Sakura Paper",
    };
    const auto& themes = all_themes();
    REQUIRE(themes.size() == expected_keys.size());

    std::set<std::string> keys;
    for (std::size_t index = 0; index < themes.size(); ++index) {
        INFO("theme=" << themes[index].key);
        CHECK(themes[index].key == expected_keys[index]);
        CHECK(keys.insert(themes[index].key).second);
        CHECK(valid_theme(themes[index]));
        CHECK((themes[index].border_radius >= 4 &&
               themes[index].border_radius <= 10));
    }
}

TEST_CASE("legacy and unknown theme keys resolve predictably", "[theme]") {
    CHECK(find_theme("One Dark Pro").key == "Tokyo Night");
    CHECK(find_theme("Monokai Pro").key == "Gruvbox Dark");
    CHECK(find_theme("missing theme").key == "Dark Modern");
    CHECK(find_theme("Sakura Paper").key == "Sakura Paper");
}

TEST_CASE("generated theme styles use readable semantic contrast", "[theme]") {
    CHECK(contrast_text_for("#ffffff") == "#000000");
    CHECK(contrast_text_for("#000000") == "#ffffff");
    CHECK(contrast_text_for("invalid") == "#ffffff");

    for (const Theme& theme : all_themes()) {
        INFO("theme=" << theme.key);
        const std::string stylesheet = generate_theme_style(theme);
        CHECK(minimum_role_contrast(theme) >= 4.5);
        CHECK(stylesheet.find('@') == std::string::npos);
        CHECK(preserves_branch_tokens(stylesheet));
        CHECK(embeds_palette(stylesheet, theme));
    }
}

TEST_CASE("disabled Launch button uses inactive theme colors", "[theme][launch]") {
    for (const Theme& theme : all_themes()) {
        INFO("theme=" << theme.key);
        const std::string stylesheet = generate_theme_style(theme);
        const std::string expected_rule =
            "QPushButton#launch_btn:disabled {\n"
            "    background-color: " + theme.bg_tertiary + ";\n"
            "    border-color: " + theme.border + ";\n"
            "    color: " + theme.text_secondary + ";\n"
            "}";
        const std::size_t disabled_rule = stylesheet.find(expected_rule);

        CHECK(disabled_rule != std::string::npos);
        CHECK(disabled_rule >
              stylesheet.find("QPushButton#launch_btn:hover"));
    }
}

TEST_CASE("frameless windows receive a theme-aware perimeter", "[theme]") {
    for (const Theme& theme : all_themes()) {
        INFO("theme=" << theme.key);
        const std::string expected_rule =
            "QMainWindow#frameless_window,\n"
            "QDialog#frameless_window {\n"
            "    background-color: " + theme.border + ";\n"
            "}";
        CHECK(generate_theme_style(theme).find(expected_rule) !=
              std::string::npos);
    }
}

TEST_CASE("input panels retain their theme-aware outer cards", "[theme]") {
    for (const Theme& theme : all_themes()) {
        INFO("theme=" << theme.key);
        const std::string expected_rule =
            "QWidget#input_panel {\n"
            "    background-color: " + theme.bg_secondary + ";\n"
            "    border: 1px solid " + theme.border + ";\n"
            "    border-radius: 16px;\n"
            "}";
        CHECK(generate_theme_style(theme).find(expected_rule) !=
              std::string::npos);
    }
}

TEST_CASE("About Goliath remains theme-aware across every palette", "[theme]") {
    for (const Theme& theme : all_themes()) {
        INFO("theme=" << theme.key);
        const std::string stylesheet = generate_theme_style(theme);
        const std::string expected_cards =
            "QFrame#about_identity_card,\n"
            "QFrame#about_information_card {\n"
            "    background-color: " + theme.bg_secondary + ";\n"
            "    border: 1px solid " + theme.border + ";\n"
            "    border-radius: 12px;\n"
            "}";
        const std::string expected_close =
            "QPushButton#about_close_button {\n"
            "    background-color: " + theme.accent + ";\n"
            "    border-color: " + theme.accent + ";\n"
            "    color: " + contrast_text_for(theme.accent) + ";";
        CHECK(stylesheet.find(expected_cards) != std::string::npos);
        CHECK(stylesheet.find(expected_close) != std::string::npos);
    }
}

TEST_CASE("theme selector popup remains readable across every palette", "[theme]") {
    constexpr const char* popup_selector =
        "QAbstractItemView#theme_selector_popup";

    CHECK(generate_theme_style(all_themes().front()).find(popup_selector) ==
          std::string::npos);

    for (const Theme& theme : all_themes()) {
        INFO("theme=" << theme.key);
        const std::string popup_style =
            generate_theme_selector_popup_style(theme);
        CHECK(contrast_ratio(theme.bg_hover, theme.text_primary) >= 4.5);
        CHECK(popup_style.find('@') == std::string::npos);
        CHECK(popup_style.find("QAbstractItemView") != std::string::npos);
        CHECK(popup_style.find(theme.bg_hover) != std::string::npos);
        CHECK(popup_style.find(theme.text_primary) != std::string::npos);
        CHECK(popup_style.find(theme.accent) != std::string::npos);
        CHECK(popup_style.find(contrast_text_for(theme.accent)) !=
              std::string::npos);
    }
}

TEST_CASE("theme selector popup delegate owns its foreground colors", "[theme]") {
    for (const Theme& theme : all_themes()) {
        INFO("theme=" << theme.key);
        const std::string normal =
            theme_selector_popup_text(theme, false);
        const std::string selected =
            theme_selector_popup_text(theme, true);
        CHECK(normal == theme.text_primary);
        CHECK(selected == contrast_text_for(theme.accent));
        CHECK((contrast_ratio(theme.bg_hover, normal) >= 4.5 &&
               contrast_ratio(theme.accent, selected) >= 4.5));
    }
}
