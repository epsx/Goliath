#include "theme.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <sstream>

namespace goliath {

namespace {

std::string tok_replace(std::string s, const std::string& token, const std::string& value) {
    std::size_t pos = 0;
    while ((pos = s.find(token, pos)) != std::string::npos) {
        s.replace(pos, token.size(), value);
        pos += value.size();
    }
    return s;
}

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

} // namespace

const std::vector<Theme>& all_themes() {
    static const std::vector<Theme> themes = {
        {"Dark Modern", "#121212", "#1a1a1a", "#1e1e1e", "#2a2a2a",
         "#3daee9", "#50b8e8", "#28a745", "#dc3545", "#e0e0e0", "#c0c0c0", "#333333", 8},
        {"Dracula", "#282a36", "#44475a", "#6272a4", "#44475a",
         "#bd93f9", "#caa9fa", "#50fa7b", "#ff5555", "#f8f8f2", "#bfbfbf", "#6272a4", 10},
        {"Tokyo Night", "#1a1b26", "#16161e", "#2a2b3d", "#363b54",
         "#7aa2f7", "#89b4fa", "#9ece6a", "#f7768e", "#c0caf5", "#a9b1d6", "#414868", 8},
        {"Nord", "#2e3440", "#3b4252", "#434c5e", "#4c566a",
         "#88c0d0", "#8fbcbb", "#a3be8c", "#bf616a", "#d8dee9", "#b48ead", "#4c566a", 8},
        {"Gruvbox Dark", "#282828", "#1d2021", "#3c3836", "#504945",
         "#fabd2f", "#fe8019", "#b8bb26", "#fb4934", "#ebdbb2", "#a89984", "#3c3836", 8},
        {"GitHub Dark", "#24292e", "#1f2428", "#2d333b", "#282e34",
         "#539bf5", "#6cb6ff", "#3fb950", "#f85149", "#c9d1d9", "#8b949e", "#30363d", 6},
        {"Goliath Pearl", "#f3f5f7", "#ffffff", "#e8ecf1", "#dce5f2",
         "#2864dc", "#1f4fb5", "#2e7d32", "#c62828", "#20242a", "#59636f", "#c8d0da", 8},
        {"Warm Ivory", "#f4f0e7", "#fffbf3", "#e9e0d1", "#dfd3c0",
         "#b85c23", "#8f4318", "#567638", "#b23a32", "#302d29", "#6f655b", "#cbbda9", 8},
        {"Arctic Blue", "#eaf2f6", "#f9fcfe", "#dce9ef", "#cce0e8",
         "#007c91", "#006678", "#2f7658", "#b43c4d", "#18242c", "#526773", "#b8ccd5", 8},
        {"Neo Geo Classic", "#f1f1f1", "#ffffff", "#e2e2e2", "#d7d7d7",
         "#c62828", "#a91f1f", "#2e7d32", "#8e2424", "#1d1d1d", "#5f5f5f", "#bdbdbd", 6},
        {"High Contrast Light", "#ffffff", "#f7f7f7", "#e6e6e6", "#d5e5ff",
         "#0047ab", "#003580", "#006b2e", "#a40000", "#000000", "#343434", "#000000", 4},
        {"Sakura Paper", "#f8f2f3", "#fffdfd", "#f0e1e5", "#e8d2d9",
         "#a83e5b", "#873047", "#477a52", "#b02f48", "#33292c", "#705d63", "#d8bdc5", 10},
    };
    return themes;
}

const Theme& find_theme(const std::string& name) {
    std::string canonical_name = name;
    if (canonical_name == "One Dark Pro") canonical_name = "Tokyo Night";
    if (canonical_name == "Monokai Pro") canonical_name = "Gruvbox Dark";

    for (const auto& t : all_themes()) {
        if (t.key == canonical_name) return t;
    }
    for (const auto& t : all_themes()) {
        if (t.key == "Dark Modern") return t;
    }
    return all_themes().front();
}

std::string contrast_text_for(const std::string& background) {
    std::array<int, 3> rgb{};
    if (!parse_rgb(background, rgb)) return "#ffffff";

    const double luminance =
        0.2126 * linear_channel(rgb[0]) +
        0.7152 * linear_channel(rgb[1]) +
        0.0722 * linear_channel(rgb[2]);
    const double black_contrast = (luminance + 0.05) / 0.05;
    const double white_contrast = 1.05 / (luminance + 0.05);
    return black_contrast >= white_contrast ? "#000000" : "#ffffff";
}

// @token@ markers stand in for theme fields; <<BRANCH_*>> tokens are left
// untouched here and get substituted by the caller once the branch-arrow
// PNGs have been written to disk.
std::string generate_theme_style(const Theme& theme) {
    static const char* TEMPLATE = R"QSS(
QWidget {
    font-family: "Segoe UI", "Helvetica Neue", "Arial", sans-serif;
    font-size: 10pt;
    color: @text_primary@;
    background-color: @bg_primary@;
}

QMainWindow {
    background-color: @bg_primary@;
}

QLabel {
    background-color: transparent;
}

QLineEdit {
    background-color: @bg_tertiary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    padding: 6px 10px;
    color: @text_primary@;
    selection-background-color: @accent@;
    selection-color: @on_accent@;
}

QLineEdit:focus {
    border: 1px solid @accent@;
}

QPushButton {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    padding: 6px 14px;
    color: @text_primary@;
    font-weight: 500;
}

QPushButton:hover {
    background-color: @bg_hover@;
    border: 1px solid @accent@;
}

QPushButton:pressed {
    background-color: @bg_tertiary@;
}

QWidget:disabled {
    color: @text_secondary@;
}

QFrame#system_selector_frame {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
}

QPushButton#system_selector_button {
    background-color: transparent;
    border: none;
    padding: 7px 12px;
}

QPushButton#system_selector_button:hover {
    background-color: @bg_hover@;
    border: none;
}

QPushButton#system_selector_button:checked {
    background-color: @accent@;
    color: @on_accent@;
    border: none;
}

QPushButton#launch_btn {
    background-color: @success@;
    border-color: @success@;
    color: @on_success@;
    font-weight: bold;
    padding: 8px 18px;
}

QPushButton#launch_btn:hover {
    background-color: @accent@;
    border-color: @accent@;
    color: @on_accent@;
}

QPushButton#launch_btn:disabled {
    background-color: @bg_tertiary@;
    border-color: @border@;
    color: @text_secondary@;
}

QTreeWidget {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    outline: none;
    padding: 4px;
    alternate-background-color: @bg_tertiary@;
}

QTreeWidget::item {
    padding: 6px;
    border-radius: 4px;
    margin: 1px 0px;
}

QTreeWidget::item:selected {
    background-color: @accent@;
    color: @on_accent@;
}

QTableView,
QTableWidget {
    background-color: @bg_secondary@;
    alternate-background-color: @bg_tertiary@;
    color: @text_primary@;
    gridline-color: @border@;
    border: 1px solid @border@;
    outline: none;
    selection-background-color: @accent@;
    selection-color: @on_accent@;
}

QTableView::item:hover:!selected,
QTableWidget::item:hover:!selected {
    background-color: @bg_hover@;
    color: @text_primary@;
}

QTableView::item:selected,
QTableWidget::item:selected,
QTableView::item:selected:active,
QTableWidget::item:selected:active,
QTableView::item:selected:!active,
QTableWidget::item:selected:!active {
    background-color: @accent@;
    color: @on_accent@;
}

QHeaderView::section {
    background-color: @bg_tertiary@;
    color: @text_primary@;
    border: none;
    border-right: 1px solid @border@;
    border-bottom: 1px solid @border@;
    padding: 5px;
}

QTableCornerButton::section {
    background-color: @bg_tertiary@;
    border: none;
    border-right: 1px solid @border@;
    border-bottom: 1px solid @border@;
}

QTreeWidget::branch {
    background-color: @bg_secondary@;
    selection-background-color: @bg_secondary@;
}

QTreeWidget::branch:selected,
QTreeWidget::branch:selected:active,
QTreeWidget::branch:selected:!active {
    background-color: @bg_secondary@;
    selection-background-color: @bg_secondary@;
    border: none;
}

QTreeWidget::branch:has-children:!has-siblings:closed,
QTreeWidget::branch:closed:has-children:has-siblings {
    background-color: @bg_secondary@;
    border-image: none;
    image: url("<<BRANCH_CLOSED>>");
}

QTreeWidget::branch:has-children:!has-siblings:open,
QTreeWidget::branch:open:has-children:has-siblings {
    background-color: @bg_secondary@;
    border-image: none;
    image: url("<<BRANCH_OPEN>>");
}

QTreeWidget::branch:selected:has-children:!has-siblings:closed,
QTreeWidget::branch:selected:closed:has-children:has-siblings {
    background-color: @bg_secondary@;
    border-image: none;
    image: url("<<BRANCH_CLOSED_SELECTED>>");
}

QTreeWidget::branch:selected:has-children:!has-siblings:open,
QTreeWidget::branch:selected:open:has-children:has-siblings {
    background-color: @bg_secondary@;
    border-image: none;
    image: url("<<BRANCH_OPEN_SELECTED>>");
}

QTextEdit {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    color: @text_primary@;
    padding: 8px;
}

QTextEdit QScrollBar:vertical {
    background-color: @bg_secondary@;
    width: 12px;
}

QScrollArea {
    border: none;
    background-color: transparent;
}

QScrollBar:vertical,
QScrollBar:horizontal {
    background-color: @bg_secondary@;
    border: none;
}

QScrollBar::handle:vertical,
QScrollBar::handle:horizontal {
    background-color: @border@;
    border-radius: 5px;
    min-height: 24px;
    min-width: 24px;
}

QScrollBar::handle:vertical:hover,
QScrollBar::handle:horizontal:hover {
    background-color: @accent@;
}

QSplitter::handle {
    background-color: @border@;
}

QTabWidget::pane {
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    background-color: @bg_secondary@;
    top: -1px;
}

QTabBar::tab {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-bottom: none;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    padding: 8px 16px;
    margin-right: 2px;
}

QTabBar::tab:selected {
    background-color: @accent@;
    color: @on_accent@;
}

QTabBar::tab:hover:!selected {
    background-color: @bg_hover@;
}

QGroupBox {
    color: @text_primary@;
    font-weight: bold;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    margin-top: 12px;
    padding-top: 10px;
    padding-bottom: 10px;
    padding-left: 14px;
    padding-right: 14px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 6px;
    color: @accent@;
}

QComboBox {
    background-color: @bg_tertiary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    padding: 4px 8px;
    color: @text_primary@;
    min-width: 80px;
}

QComboBox:hover {
    border: 1px solid @accent@;
}

QComboBox::drop-down {
    border: none;
    width: 24px;
}

QComboBox QAbstractItemView {
    background-color: @bg_tertiary@;
    border: 1px solid @border@;
    color: @text_primary@;
    selection-background-color: @accent@;
    selection-color: @on_accent@;
}

QSpinBox {
    background-color: @bg_tertiary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    padding: 4px 8px;
    color: @text_primary@;
}

QSpinBox:focus {
    border: 1px solid @accent@;
}

QCheckBox {
    color: @text_primary@;
    background-color: transparent;
    spacing: 6px;
}

QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border-radius: 4px;
    border: 1px solid @border@;
    background-color: @bg_tertiary@;
}

QCheckBox::indicator:checked {
    background-color: @accent@;
    border: 1px solid @accent@;
}

QProgressBar {
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    background-color: @bg_tertiary@;
    text-align: center;
    color: @text_primary@;
}

QProgressBar::chunk {
    background-color: @accent@;
    border-radius: @border_radius@px;
}

QMessageBox {
    background-color: @bg_secondary@;
}

QDialog {
    background-color: @bg_primary@;
}

/* The one-pixel top-level contents margin exposes this themed perimeter. */
QMainWindow#frameless_window,
QDialog#frameless_window {
    background-color: @border@;
}

QStatusBar {
    color: @text_secondary@;
    background-color: @bg_secondary@;
    border: none;
}

QStatusBar::item {
    border: none;
}

/* Custom Title Bar */
QFrame#title_bar {
    background-color: @bg_secondary@;
    border-bottom: 1px solid @border@;
}

QLabel#titlebar_title {
    color: @text_primary@;
    font-size: 13px;
    font-weight: 600;
}

QPushButton#titlebar_btn, QPushButton#titlebar_close {
    background-color: transparent;
    border: none;
    border-radius: 4px;
    color: @text_secondary@;
    font-size: 13px;
    padding: 0;
}

QPushButton#titlebar_btn:hover {
    background-color: @bg_hover@;
    color: @text_primary@;
}

QPushButton#titlebar_close:hover {
    background-color: @danger@;
    color: @on_danger@;
}

QMenu {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    color: @text_primary@;
}

QMenu::item {
    padding: 6px 20px;
    background-color: transparent;
}

QMenu::item:selected {
    background-color: @accent@;
    color: @on_accent@;
}

QMenu::separator {
    height: 1px;
    background-color: @border@;
    margin: 4px 10px;
}

/* Modern Toolbar */
QFrame#toolbar_frame {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    padding: 8px;
}

QComboBox#theme_selector {
    background-color: @accent@;
    color: @on_accent@;
    border: none;
    padding: 6px 12px;
    border-radius: @border_radius@px;
}

QComboBox#theme_selector:hover {
    background-color: @accent_hover@;
    color: @on_accent_hover@;
}

QLabel#toolbar_label {
    color: @text_primary@;
    padding: 0 8px;
}

QLabel#details_title,
QLabel#details_section_title,
QLabel#section_heading {
    color: @text_primary@;
}

QLabel#details_title {
    padding-bottom: 4px;
}

QLabel#details_section_title {
    padding-top: 10px;
    padding-bottom: 2px;
}

QLabel#section_heading {
    font-size: 14px;
    font-weight: 600;
    margin-top: 8px;
}

QLabel#variant_label,
QLabel#details_value,
QLabel#secondary_text,
QLabel#snapshot_label {
    color: @text_secondary@;
}

QLabel#variant_label {
    font-size: 11px;
}

QFrame#details_separator {
    color: @border@;
    background-color: @border@;
    border: none;
    max-height: 1px;
}

QFrame#snapshot_card {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: 12px;
}

QFrame#about_identity_card,
QFrame#about_information_card {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: 12px;
}

QLabel#about_product_name {
    color: @text_primary@;
    font-size: 26px;
    font-weight: 700;
}

QLabel#about_heading {
    color: @text_primary@;
    font-size: 18px;
    font-weight: 600;
}

QLabel#about_section_heading {
    color: @accent@;
    font-size: 12px;
    font-weight: 600;
    padding-top: 4px;
}

QLabel#about_body {
    color: @text_primary@;
}

QLabel#about_tagline,
QLabel#about_version,
QLabel#about_build,
QLabel#about_status,
QLabel#about_credit {
    color: @text_secondary@;
}

QPushButton#about_link_button {
    background-color: @bg_tertiary@;
}

QPushButton#about_link_button:disabled {
    background-color: @bg_primary@;
    border-color: @border@;
    color: @text_secondary@;
}

QPushButton#about_close_button {
    background-color: @accent@;
    border-color: @accent@;
    color: @on_accent@;
    font-weight: 600;
    min-width: 76px;
}

QPushButton#about_close_button:hover {
    background-color: @accent_hover@;
    border-color: @accent_hover@;
    color: @on_accent_hover@;
}

QLabel#snapshot_label {
    background-color: transparent;
    border: none;
}

QLabel#themed_note,
QLabel#warning_note {
    color: @text_primary@;
    background-color: @bg_tertiary@;
    border: 1px solid @border@;
    border-radius: 6px;
    padding: 8px;
}

QLabel#warning_note {
    border-color: @danger@;
}

QWidget#input_panel {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: 16px;
}

QToolTip {
    color: @text_primary@;
    background-color: @bg_tertiary@;
    border: 1px solid @border@;
}
)QSS";

    std::string out = TEMPLATE;
    out = tok_replace(out, "@bg_primary@", theme.bg_primary);
    out = tok_replace(out, "@bg_secondary@", theme.bg_secondary);
    out = tok_replace(out, "@bg_tertiary@", theme.bg_tertiary);
    out = tok_replace(out, "@bg_hover@", theme.bg_hover);
    out = tok_replace(out, "@on_accent_hover@", contrast_text_for(theme.accent_hover));
    out = tok_replace(out, "@on_accent@", contrast_text_for(theme.accent));
    out = tok_replace(out, "@on_success@", contrast_text_for(theme.success));
    out = tok_replace(out, "@on_danger@", contrast_text_for(theme.danger));
    out = tok_replace(out, "@accent_hover@", theme.accent_hover);
    out = tok_replace(out, "@accent@", theme.accent);
    out = tok_replace(out, "@success@", theme.success);
    out = tok_replace(out, "@danger@", theme.danger);
    out = tok_replace(out, "@text_primary@", theme.text_primary);
    out = tok_replace(out, "@text_secondary@", theme.text_secondary);
    out = tok_replace(out, "@border_radius@", std::to_string(theme.border_radius));
    out = tok_replace(out, "@border@", theme.border);
    return out;
}

std::string generate_theme_selector_popup_style(const Theme& theme) {
    static const char* TEMPLATE = R"QSS(
QAbstractItemView {
    background-color: @bg_hover@;
    color: @text_primary@;
    border: 1px solid @border@;
    outline: none;
    selection-background-color: @accent@;
    selection-color: @on_accent@;
}

QAbstractItemView::item {
    background-color: @bg_hover@;
    color: @text_primary@;
    min-height: 24px;
    padding: 2px 8px;
}

QAbstractItemView::item:hover:!selected {
    background-color: @bg_tertiary@;
    color: @text_primary@;
}

QAbstractItemView::item:selected {
    background-color: @accent@;
    color: @on_accent@;
}
)QSS";

    std::string out = TEMPLATE;
    out = tok_replace(out, "@bg_tertiary@", theme.bg_tertiary);
    out = tok_replace(out, "@bg_hover@", theme.bg_hover);
    out = tok_replace(out, "@on_accent@", contrast_text_for(theme.accent));
    out = tok_replace(out, "@accent@", theme.accent);
    out = tok_replace(out, "@text_primary@", theme.text_primary);
    out = tok_replace(out, "@border@", theme.border);
    return out;
}

std::string theme_selector_popup_text(const Theme& theme, bool selected) {
    return selected ? contrast_text_for(theme.accent) : theme.text_primary;
}

} // namespace goliath
