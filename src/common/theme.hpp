// theme.hpp — the set of selectable color themes plus the Qt stylesheet
// (QSS) generator applied to the whole app.
#pragma once

#include <string>
#include <vector>

namespace goliath {

struct Theme {
    std::string key; // lookup key and text shown in the theme selector combo box
    std::string bg_primary;
    std::string bg_secondary;
    std::string bg_tertiary;
    std::string bg_hover;
    std::string accent;
    std::string accent_hover;
    std::string success;
    std::string danger;
    std::string text_primary;
    std::string text_secondary;
    std::string border;
    int border_radius;
};

// The 12 built-in themes, in display order for the theme selector combo box.
const std::vector<Theme>& all_themes();

// Look up a theme by key ("Dark Modern", "Goliath Pearl", ...). Legacy
// One Dark Pro and Monokai Pro keys migrate to their replacements; every other
// unknown key falls back to Dark Modern.
const Theme& find_theme(const std::string& name);

// Return black or white, whichever has the stronger WCAG contrast against the
// supplied #RRGGBB background. Invalid colors conservatively use white.
std::string contrast_text_for(const std::string& background);

// Full application stylesheet for a given theme. The four branch-indicator
// image URLs are left as literal "<<BRANCH_CLOSED>>" / "<<BRANCH_OPEN>>" /
// "<<BRANCH_CLOSED_SELECTED>>" / "<<BRANCH_OPEN_SELECTED>>" tokens, to be
// substituted by the caller after generating the branch PNGs on disk.
std::string generate_theme_style(const Theme& theme);

// Dedicated stylesheet installed directly on the theme selector's popup
// view. Keeping this separate from the window stylesheet prevents the combo
// button's accent foreground from winning Qt's popup cascade on Windows.
std::string generate_theme_selector_popup_style(const Theme& theme);

// Foreground used by the theme selector popup delegate. Rendering this text
// explicitly prevents the QComboBox button palette from leaking into its
// separate popup view on Windows.
std::string theme_selector_popup_text(const Theme& theme, bool selected);

} // namespace goliath
