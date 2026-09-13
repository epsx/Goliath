#include "settings_field.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

namespace goliath {

void FieldWidgetSet::addToForm(QFormLayout* form, const FieldSpec& spec) {
    QWidget* widget = nullptr;

    switch (spec.kind) {
        case FieldSpec::Kind::Combo: {
            auto* combo = new QComboBox();
            for (const auto& opt : spec.combo_options) {
                combo->addItem(opt.text, opt.value);
            }
            widget = combo;
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

    m_entries[spec.key] = Entry{spec.kind, widget};
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
            return qobject_cast<QComboBox*>(e.widget)->currentData().toInt();
        case FieldSpec::Kind::Bool:
            return qobject_cast<QCheckBox*>(e.widget)->isChecked() ? 1 : 0;
        case FieldSpec::Kind::Spin:
            return qobject_cast<QSpinBox*>(e.widget)->value();
    }
    return 0;
}

} // namespace goliath
