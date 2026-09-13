// settings_field.hpp — VideoTab and AudioTab both build their form rows from
// a list of (key, label, default, kind, options) specs. FieldSpec describes
// one such row and FieldWidgetSet creates/tracks the matching Qt widget
// (combo box, checkbox, or spin box), shared by both tabs.
#pragma once

#include <QString>
#include <map>
#include <string>
#include <vector>

class QFormLayout;
class QWidget;

namespace goliath {

struct ComboOption {
    int value;
    QString text;
};

struct FieldSpec {
    enum class Kind { Combo, Bool, Spin };

    std::string key;
    QString label;
    int default_value;
    Kind kind;
    std::vector<ComboOption> combo_options; // Kind::Combo
    int spin_min = 0;
    int spin_max = 0; // Kind::Spin
};

// Match JGRF/Geolith setting semantics: an out-of-range value is not
// clamped to the nearest UI value; it falls back to the setting default.
inline bool is_valid_field_value(const FieldSpec& spec, int value) {
    switch (spec.kind) {
        case FieldSpec::Kind::Combo:
            for (const auto& opt : spec.combo_options) {
                if (opt.value == value) return true;
            }
            return false;
        case FieldSpec::Kind::Bool:
            return value == 0 || value == 1;
        case FieldSpec::Kind::Spin:
            return value >= spec.spin_min && value <= spec.spin_max;
    }
    return false;
}

inline int normalize_field_value(const FieldSpec& spec, int value) {
    return is_valid_field_value(spec, value) ? value : spec.default_value;
}

class FieldWidgetSet {
public:
    // Creates the right widget for spec.kind, adds it as a form row
    // ("label:" -> widget), and starts tracking it.
    void addToForm(QFormLayout* form, const FieldSpec& spec);

    // Returns the tracked widget for a field so a tab can attach behavior
    // that is specific to one setting (for example capability-dependent
    // combo-box options). Ownership remains with Qt's parent hierarchy.
    QWidget* widget(const std::string& key) const;

    void setValue(const std::string& key, int value);
    int value(const std::string& key) const;

private:
    struct Entry {
        FieldSpec::Kind kind;
        QWidget* widget;
    };
    std::map<std::string, Entry> m_entries;
};

} // namespace goliath
