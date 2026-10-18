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

std::string rgba_color(const std::string& color, int alpha) {
    std::array<int, 3> rgb{};
    if (!parse_rgb(color, rgb)) return "rgba(0, 0, 0, 0)";

    std::ostringstream out;
    out << "rgba(" << rgb[0] << ", " << rgb[1] << ", " << rgb[2]
        << ", " << alpha << ")";
    return out.str();
}

} // namespace

const std::vector<Theme>& all_themes() {
    static const std::vector<Theme> themes = {
        {"Dark Modern", "#121212", "#1a1a1a", "#1e1e1e", "#2a2a2a",
         "#3daee9", "#50b8e8", "#28a745", "#dc3545", "#e0e0e0", "#c0c0c0", "#333333", 8},
        {"AMOLED Black", "#000000", "#090909", "#141414", "#1f1f1f",
         "#d8aa45", "#f0c264", "#4caf50", "#ef5350", "#f5f1e8", "#bdb6a6", "#303030", 8},
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

// @token@ markers stand in for theme fields; <<BRANCH_*>> and <<SPIN_*>>
// tokens are left untouched here and get substituted by the caller once the
// small theme-colored indicator PNGs have been written to disk.
std::string generate_theme_style(const Theme& theme) {
    static const char* TEMPLATE = R"QSS(
QWidget {
    font-family: "Segoe UI", "Helvetica Neue", "Arial", sans-serif;
    font-size: 10pt;
    color: @text_primary@;
    background-color: @bg_primary@;
}

QWidget#gallery_panel,
QWidget#gallery_rail {
    background-color: transparent;
    border: none;
}

QWidget#details_field_label {
    background-color: transparent;
    border: none;
}

QSplitter#library_details_splitter,
QSplitter#library_details_splitter::handle {
    background-color: transparent;
    border: none;
}

QStackedWidget#library_view_stack {
    background-color: transparent;
    border: none;
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

QPushButton#rating_star_btn {
    background-color: transparent;
    border: none;
    border-radius: 4px;
    padding: 0;
    color: @text_secondary@;
    font-size: 18px;
    font-weight: normal;
}

QPushButton#rating_star_btn[rated="true"] {
    color: @accent@;
}

QPushButton#rating_star_btn:hover {
    background-color: @bg_hover@;
    border: none;
    color: @accent_hover@;
}

QPushButton#rating_star_btn:disabled {
    background-color: transparent;
    border: none;
    color: @text_secondary@;
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

QTreeWidget,
QListWidget#library_icon_view {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    outline: none;
    padding: 4px;
    alternate-background-color: @bg_tertiary@;
}

QTreeWidget::item,
QListWidget#library_icon_view::item {
    padding: 6px;
    border-radius: 4px;
    margin: 1px 0px;
}

QTreeWidget::item:selected,
QListWidget#library_icon_view::item:selected {
    background-color: @accent@;
    color: @on_accent@;
}

QListWidget#library_icon_view::item:hover:!selected {
    background-color: @bg_hover@;
    color: @text_primary@;
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

QAbstractScrollArea::corner {
    background-color: @bg_secondary@;
    border: none;
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

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0px;
    border: none;
    background: transparent;
}

QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal {
    width: 0px;
    border: none;
    background: transparent;
}

QScrollBar::up-arrow:vertical,
QScrollBar::down-arrow:vertical,
QScrollBar::left-arrow:horizontal,
QScrollBar::right-arrow:horizontal {
    width: 0px;
    height: 0px;
    image: none;
}

QSplitter::handle {
    background-color: @border@;
}

QSplitter#library_details_splitter::handle {
    background-color: transparent;
    border: none;
}

QTabWidget::pane {
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    background-color: @bg_secondary@;
    top: -1px;
}

QTabWidget#settings_tabs::pane,
QTabWidget#game_profile_tabs::pane {
    border: none;
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

QPushButton#gallery_mode_button {
    color: @text_primary@;
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: 6px;
    font-weight: 600;
    padding: 0;
}

QPushButton#gallery_mode_button:hover:!checked {
    background-color: @bg_hover@;
}

QPushButton#gallery_mode_button:checked {
    color: @on_accent@;
    background-color: @accent@;
    border-color: @accent@;
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
    padding: 4px 22px 4px 8px;
    color: @text_primary@;
}

QSpinBox:focus {
    border: 1px solid @accent@;
}

QSpinBox::up-button,
QSpinBox::down-button {
    subcontrol-origin: border;
    width: 18px;
    background-color: transparent;
    border: none;
}

QSpinBox::up-button {
    subcontrol-position: top right;
    border-top-right-radius: @border_radius@px;
}

QSpinBox::down-button {
    subcontrol-position: bottom right;
    border-bottom-right-radius: @border_radius@px;
}

QSpinBox::up-button:hover,
QSpinBox::down-button:hover {
    background-color: @bg_hover@;
}

QSpinBox::up-button:pressed,
QSpinBox::down-button:pressed {
    background-color: @accent@;
}

QSpinBox::up-arrow {
    image: url("<<SPIN_UP>>");
    width: 10px;
    height: 6px;
}

QSpinBox::down-arrow {
    image: url("<<SPIN_DOWN>>");
    width: 10px;
    height: 6px;
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

QStatusBar {
    color: @text_secondary@;
    background-color: @bg_secondary@;
    border: none;
}

QStatusBar::item {
    border: none;
}

QLabel#library_status {
    color: @text_secondary@;
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

QListWidget#genre_filter_list {
    background-color: @bg_secondary@;
    color: @text_primary@;
    border: none;
    outline: none;
}

QListWidget#genre_filter_list::item {
    padding: 6px 10px;
    border: none;
}

QListWidget#genre_filter_list::item:selected {
    background-color: @accent@;
    color: @on_accent@;
}

QListWidget#genre_filter_list::item:disabled {
    color: @text_secondary@;
}

/* Modern Toolbar */
QFrame#toolbar_frame {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
    padding: 8px;
}

QWidget#toolbar_actions {
    background-color: transparent;
    border: none;
}

QPushButton#tools_btn,
QPushButton#alerts_btn,
QPushButton#settings_btn,
QPushButton#about_btn,
QPushButton#filters_btn,
QPushButton#random_btn,
QPushButton#favorite_btn {
    background-color: transparent;
    border: none;
}

QPushButton#tools_btn:hover,
QPushButton#alerts_btn:hover,
QPushButton#settings_btn:hover,
QPushButton#about_btn:hover,
QPushButton#filters_btn:hover,
QPushButton#random_btn:hover,
QPushButton#favorite_btn:hover {
    background-color: @bg_hover@;
    border: none;
}

QPushButton#tools_btn:pressed,
QPushButton#alerts_btn:pressed,
QPushButton#settings_btn:pressed,
QPushButton#about_btn:pressed,
QPushButton#filters_btn:pressed,
QPushButton#random_btn:pressed,
QPushButton#favorite_btn:pressed {
    background-color: @bg_tertiary@;
    border: none;
}

QPushButton#favorite_btn:checked {
    color: @accent@;
}

QPushButton#alerts_btn[hasUnread="true"] {
    color: @accent@;
}

QPushButton#update_available_indicator {
    color: @accent@;
    background-color: transparent;
    border: none;
    border-radius: 3px;
    padding: 0px 2px;
    font-size: 10px;
    font-weight: 600;
}

QPushButton#update_available_indicator:hover,
QPushButton#update_available_indicator:focus {
    color: @accent_hover@;
    background-color: @bg_hover@;
    border: none;
}

QPushButton#update_available_indicator:pressed {
    color: @accent@;
    background-color: @bg_tertiary@;
    border: none;
}

QPushButton#tools_btn::menu-indicator,
QPushButton#filters_btn::menu-indicator {
    image: none;
    width: 0px;
    height: 0px;
}

QComboBox#theme_selector {
    background-color: @bg_tertiary@;
    color: @text_primary@;
    border: none;
    padding: 6px 12px;
    border-radius: @border_radius@px;
}

QComboBox#theme_selector:hover {
    background-color: @bg_hover@;
    color: @accent@;
}

QLabel#toolbar_label {
    color: @text_primary@;
    padding: 0 8px;
}

QLabel#details_title,
QLabel#details_section_title,
QLabel#details_subsection_title,
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

QLabel#details_subsection_title {
    color: @text_primary@;
    padding-bottom: 3px;
}

QFrame#details_info_vertical_divider {
    min-width: 2px;
    max-width: 2px;
    background: qlineargradient(
        x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 @divider_shadow@,
        stop: 0.5 @divider_shadow@,
        stop: 0.5 @divider_highlight@,
        stop: 1 @divider_highlight@);
    border: none;
}

QFrame#details_info_horizontal_divider {
    min-height: 2px;
    max-height: 2px;
    background: qlineargradient(
        x1: 0, y1: 0, x2: 0, y2: 1,
        stop: 0 @divider_shadow@,
        stop: 0.5 @divider_shadow@,
        stop: 0.5 @divider_highlight@,
        stop: 1 @divider_highlight@);
    border: none;
}

QPushButton#profile_configure_btn {
    min-width: 110px;
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

QFrame#details_header_card,
QFrame#details_info_card,
QFrame#snapshot_card,
QTextEdit#details_description_card {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: @border_radius@px;
}

QFrame#snapshot_card {
    border-radius: 12px;
}

QFrame#about_identity_card,
QFrame#about_information_card {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: 12px;
}

QFrame#rescan_summary_card,
QFrame#rescan_change_card,
QFrame#bios_summary_card,
QFrame#bios_metric_card,
QFrame#bios_archive_row,
QFrame#controller_profile_editor,
QFrame#controller_profile_card,
QFrame#controller_profile_empty {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: 12px;
}

QFrame#notification_card,
QFrame#notification_empty {
    background-color: @bg_secondary@;
    border: 1px solid @border@;
    border-radius: 12px;
}

QFrame#notification_card[notificationUnread="true"] {
    border-left: 3px solid @accent@;
}

QFrame#notification_card[notificationSeverity="warning"],
QFrame#notification_card[notificationSeverity="error"] {
    border-left: 3px solid @danger@;
}

QScrollArea#notification_scroll,
QScrollArea#notification_scroll QWidget#qt_scrollarea_viewport,
QWidget#notification_scroll_contents {
    background-color: transparent;
    border: none;
}

QLabel#notification_heading,
QLabel#notification_title {
    color: @text_primary@;
    font-weight: 600;
}

QLabel#notification_heading {
    color: @accent@;
    font-size: 14px;
}

QLabel#notification_summary,
QLabel#notification_message,
QLabel#notification_time {
    color: @text_secondary@;
}

QLabel#notification_category {
    color: @accent@;
    font-size: 10px;
    font-weight: 700;
}

QLabel#notification_new_badge {
    color: @on_accent@;
    background-color: @accent@;
    border-radius: 5px;
    padding: 1px 5px;
    font-size: 9px;
    font-weight: 700;
}

QPushButton#notification_primary_action {
    background-color: @accent@;
    border-color: @accent@;
    color: @on_accent@;
    font-weight: 600;
}

QPushButton#notification_primary_action:hover {
    background-color: @accent_hover@;
    border-color: @accent_hover@;
    color: @on_accent_hover@;
}

QFrame#controller_profile_card[profileCompatible="true"] {
    border-left: 3px solid @accent@;
}

QScrollArea#controller_profiles_scroll,
QScrollArea#controller_profiles_scroll QWidget#qt_scrollarea_viewport,
QWidget#controller_profiles_scroll_contents {
    background-color: transparent;
    border: none;
}

QLabel#controller_profiles_heading,
QLabel#controller_profile_name {
    color: @text_primary@;
    font-weight: 600;
}

QLabel#controller_profiles_heading {
    color: @accent@;
    font-size: 12px;
}

QLabel#controller_profile_device,
QLabel#controller_profile_secondary {
    color: @text_secondary@;
}

QLabel#controller_profile_device[profileCompatible="true"] {
    color: @accent@;
    font-weight: 600;
}

QLabel#controller_profile_status {
    color: @text_primary@;
    background-color: @bg_tertiary@;
    border: 1px solid @border@;
    border-radius: 6px;
    padding: 7px;
}

QLabel#controller_profile_status[profileStatus="success"] {
    border-color: @success@;
}

QLabel#controller_profile_status[profileStatus="warning"],
QLabel#controller_profile_status[profileStatus="error"] {
    border-color: @danger@;
}

QPushButton#controller_profile_primary,
QPushButton#controller_profile_apply {
    background-color: @accent@;
    border-color: @accent@;
    color: @on_accent@;
    font-weight: 600;
}

QPushButton#controller_profile_primary:hover,
QPushButton#controller_profile_apply:hover {
    background-color: @accent_hover@;
    border-color: @accent_hover@;
    color: @on_accent_hover@;
}

QPushButton#controller_profile_apply:disabled {
    background-color: @bg_tertiary@;
    border-color: @border@;
    color: @text_secondary@;
}

QPushButton#controller_profile_delete_confirm {
    border-color: @danger@;
    color: @danger@;
    font-weight: 600;
}

QFrame#bios_archive_row:hover,
QFrame#bios_archive_row:focus {
    border-color: @accent@;
}

QFrame#bios_archive_details {
    background-color: transparent;
    border: none;
    border-top: 1px solid @border@;
}

QScrollArea#bios_archive_scroll,
QScrollArea#bios_archive_scroll QWidget#qt_scrollarea_viewport,
QWidget#bios_archive_scroll_contents,
QWidget#bios_archive_header,
QWidget#bios_member_row {
    background-color: transparent;
    border: none;
}

QLabel#bios_section_heading,
QLabel#bios_archive_name,
QLabel#bios_archive_chevron,
QLabel#bios_member_name {
    color: @accent@;
    font-weight: 600;
}

QLabel#bios_archive_chevron {
    font-size: 18px;
}

QLabel#bios_status_heading {
    color: @text_primary@;
    font-size: 14px;
    font-weight: 700;
}

QLabel#bios_status_heading[biosStatus="ready"],
QFrame#bios_metric_card[biosMetric="ready"] QLabel#bios_metric_value,
QLabel#bios_archive_status[biosStatus="ready"],
QLabel#bios_member_status[biosStatus="ready"] {
    color: @success@;
}

QLabel#bios_status_heading[biosStatus="problem"],
QFrame#bios_metric_card[biosMetric="problem"] QLabel#bios_metric_value,
QLabel#bios_archive_status[biosStatus="problem"],
QLabel#bios_member_status[biosStatus="problem"] {
    color: @danger@;
}

QFrame#bios_metric_card[biosMetric="optional"] QLabel#bios_metric_value,
QLabel#bios_archive_status[biosStatus="optional"],
QLabel#bios_member_status[biosStatus="optional"] {
    color: @text_secondary@;
}

QLabel#bios_metric_value {
    color: @text_primary@;
    font-size: 19px;
    font-weight: 700;
}

QLabel#bios_metric_label,
QLabel#bios_folder_path,
QLabel#bios_archive_purpose,
QLabel#bios_archive_requirement,
QLabel#bios_member_description,
QLabel#bios_member_crc {
    color: @text_secondary@;
}

QLabel#bios_archive_status,
QLabel#bios_member_status {
    font-weight: 600;
}

QLabel#rescan_status {
    color: @text_primary@;
    font-weight: 600;
}

QLabel#rescan_section_heading,
QLabel#rescan_card_heading {
    color: @accent@;
    font-size: 12px;
    font-weight: 600;
}

QLabel#rescan_metric_label,
QLabel#rescan_change_label,
QLabel#rescan_comparison_total {
    color: @text_secondary@;
}

QLabel#rescan_metric_value {
    color: @text_primary@;
    font-weight: 600;
}

QLabel#rescan_metric_problem {
    color: @danger@;
    font-weight: 700;
}

QLabel#rescan_change_value {
    color: @text_primary@;
    font-size: 19px;
    font-weight: 700;
}

QFrame#rescan_change_card[changeKind="added"] QLabel#rescan_change_value {
    color: @success@;
}

QFrame#rescan_change_card[changeKind="removed"] QLabel#rescan_change_value {
    color: @danger@;
}

QFrame#rescan_change_card[changeKind="updated"] QLabel#rescan_change_value {
    color: @accent@;
}

QFrame#rescan_change_card[changeKind="unchanged"] QLabel#rescan_change_value {
    color: @text_secondary@;
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

QPushButton#input_section_toggle {
    color: @text_secondary@;
    background-color: @bg_tertiary@;
    padding: 4px 10px;
    text-align: left;
}

QPushButton#input_section_toggle:hover,
QPushButton#input_section_toggle:checked {
    color: @accent@;
    border-color: @accent@;
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
    const bool dark_card = contrast_text_for(theme.bg_secondary) == "#ffffff";
    const std::string divider_shadow = dark_card
        ? rgba_color("#000000", 100)
        : rgba_color(theme.border, 88);
    const std::string divider_highlight = dark_card
        ? rgba_color(theme.text_primary, 28)
        : rgba_color("#ffffff", 150);
    out = tok_replace(out, "@divider_shadow@", divider_shadow);
    out = tok_replace(out, "@divider_highlight@", divider_highlight);
    out = tok_replace(out, "@border_radius@", std::to_string(theme.border_radius));
    out = tok_replace(out, "@border@", theme.border);
    return out;
}

std::string generate_combo_popup_style(const Theme& theme) {
    static const char* TEMPLATE = R"QSS(
QWidget#combo_popup_container {
    background-color: @bg_hover@;
    border: 1px solid @border@;
}

QAbstractItemView {
    background-color: @bg_hover@;
    color: @text_primary@;
    border: none;
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
