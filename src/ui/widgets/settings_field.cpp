#include "settings_field.hpp"
#include "ui/widgets/labelled_spin_box.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace goliath {

namespace {

bool hasNumericChoiceLabels(const FieldSpec& spec) {
    if (spec.combo_options.empty()) return false;
    for (const ComboOption& option : spec.combo_options) {
        bool numeric = false;
        option.text.toInt(&numeric);
        if (!numeric) return false;
    }
    return true;
}

} // namespace

void FieldWidgetSet::addToForm(QFormLayout* form, const FieldSpec& spec) {
    QWidget* widget = nullptr;
    std::vector<int> numericChoiceValues;

    switch (spec.kind) {
        case FieldSpec::Kind::Combo: {
            if (hasNumericChoiceLabels(spec)) {
                auto* spin = new LabelledSpinBox();
                QStringList labels;
                labels.reserve(static_cast<int>(spec.combo_options.size()));
                numericChoiceValues.reserve(spec.combo_options.size());
                for (const ComboOption& option : spec.combo_options) {
                    labels.push_back(option.text);
                    numericChoiceValues.push_back(option.value);
                }
                spin->setLabels(std::move(labels));
                widget = spin;
            } else {
                auto* combo = new QComboBox();
                for (const auto& opt : spec.combo_options) {
                    combo->addItem(opt.text, opt.value);
                }
                widget = combo;
            }
            break;
        }
        case FieldSpec::Kind::Bool: {
            widget = new QCheckBox();
            break;
        }
        case FieldSpec::Kind::Spin: {
            auto* spin = new QSpinBox();
            spin->setRange(spec.spin_min, spec.spin_max);
            widget = spin;
            break;
        }
    }

    m_entries[spec.key] = Entry{
        spec.kind, widget, std::move(numericChoiceValues)};
    form->addRow(spec.label + ":", widget);
}

QWidget* FieldWidgetSet::widget(const std::string& key) const {
    auto it = m_entries.find(key);
    return it == m_entries.end() ? nullptr : it->second.widget;
}

void FieldWidgetSet::setValue(const std::string& key, int value) {
    auto it = m_entries.find(key);
    if (it == m_entries.end()) return;
    const Entry& e = it->second;

    switch (e.kind) {
        case FieldSpec::Kind::Combo: {
            if (auto* spin = qobject_cast<QSpinBox*>(e.widget)) {
                const auto choice = std::find(
                    e.numeric_choice_values.begin(),
                    e.numeric_choice_values.end(), value);
                spin->setValue(choice == e.numeric_choice_values.end()
                    ? 0
                    : static_cast<int>(std::distance(
                          e.numeric_choice_values.begin(), choice)));
                break;
            }
            auto* combo = qobject_cast<QComboBox*>(e.widget);
            int idx = combo->findData(value);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
            break;
        }
        case FieldSpec::Kind::Bool: {
            qobject_cast<QCheckBox*>(e.widget)->setChecked(value != 0);
            break;
        }
        case FieldSpec::Kind::Spin: {
            qobject_cast<QSpinBox*>(e.widget)->setValue(value);
            break;
        }
    }
}

int FieldWidgetSet::value(const std::string& key) const {
    auto it = m_entries.find(key);
    if (it == m_entries.end()) return 0;
    const Entry& e = it->second;

    switch (e.kind) {
        case FieldSpec::Kind::Combo:
            if (auto* spin = qobject_cast<QSpinBox*>(e.widget)) {
                const int index = spin->value();
                return index >= 0 &&
                               index < static_cast<int>(
                                   e.numeric_choice_values.size())
                    ? e.numeric_choice_values[static_cast<std::size_t>(index)]
                    : 0;
            }
            return qobject_cast<QComboBox*>(e.widget)->currentData().toInt();
        case FieldSpec::Kind::Bool:
            return qobject_cast<QCheckBox*>(e.widget)->isChecked() ? 1 : 0;
        case FieldSpec::Kind::Spin:
            return qobject_cast<QSpinBox*>(e.widget)->value();
    }
    return 0;
}

} // namespace goliath
