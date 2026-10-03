#include "ui/widgets/optional_video_fields.hpp"
#include "ui/widgets/labelled_spin_box.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

#include <cstddef>
#include <utility>

namespace goliath {

namespace {

constexpr int kComboInherit = -1;

QString choice_name(const VideoSettingSpec& spec, int value) {
    if (spec.kind == VideoSettingKind::Boolean)
        return value == 0 ? QStringLiteral("Off") : QStringLiteral("On");
    for (const VideoSettingOption& option : spec.options) {
        if (option.value == value) return QString::fromUtf8(option.label);
    }
    return QString::number(value);
}

bool has_numeric_labels(const VideoSettingSpec& spec) {
    if (spec.kind != VideoSettingKind::Choice || spec.options.empty())
        return false;
    for (const VideoSettingOption& option : spec.options) {
        bool numeric = false;
        QString::fromUtf8(option.label).toInt(&numeric);
        if (!numeric) return false;
    }
    return true;
}

} // namespace

void OptionalVideoFieldSet::addToForm(QFormLayout* form,
                                      const VideoSettingSpec& spec,
                                      int globalValue) {
    globalValue = normalize_video_setting_value(spec, globalValue);
    QWidget* widget = nullptr;
    int inheritValue = kComboInherit;
    bool numericChoice = false;

    if (spec.kind == VideoSettingKind::Integer) {
        auto* spin = new QSpinBox();
        inheritValue = spec.minimum - 1;
        spin->setRange(inheritValue, spec.maximum);
        spin->setSpecialValueText(
            QString("Inherit global (%1)").arg(globalValue));
        spin->setValue(inheritValue);
        widget = spin;
    } else if (has_numeric_labels(spec)) {
        numericChoice = true;
        auto* spin = new LabelledSpinBox();
        QStringList labels = {
            QString("Inherit global (%1)")
                .arg(choice_name(spec, globalValue))};
        for (const VideoSettingOption& option : spec.options)
            labels.push_back(QString::fromUtf8(option.label));
        spin->setLabels(std::move(labels));
        spin->setValue(0);
        widget = spin;
    } else {
        auto* combo = new QComboBox();
        combo->addItem(
            QString("Inherit global (%1)")
                .arg(choice_name(spec, globalValue)),
            kComboInherit);
        if (spec.kind == VideoSettingKind::Boolean) {
            combo->addItem("Off", 0);
            combo->addItem("On", 1);
        } else {
            for (const VideoSettingOption& option : spec.options)
                combo->addItem(QString::fromUtf8(option.label), option.value);
        }
        widget = combo;
    }

    m_entries[spec.key] = Entry{
        &spec, widget, inheritValue, numericChoice};
    form->addRow(QString::fromUtf8(spec.label) + ":", widget);
}

QWidget* OptionalVideoFieldSet::widget(const std::string& key) const {
    const auto it = m_entries.find(key);
    return it == m_entries.end() ? nullptr : it->second.widget;
}

void OptionalVideoFieldSet::setOverride(const std::string& key,
                                        std::optional<int> value) {
    const auto it = m_entries.find(key);
    if (it == m_entries.end()) return;
    const Entry& entry = it->second;
    if (value.has_value() &&
        !is_valid_video_setting_value(*entry.spec, *value)) {
        value.reset();
    }

    if (entry.spec->kind == VideoSettingKind::Integer) {
        auto* spin = qobject_cast<QSpinBox*>(entry.widget);
        spin->setValue(value.value_or(entry.inherit_value));
    } else if (entry.numeric_choice) {
        auto* spin = qobject_cast<QSpinBox*>(entry.widget);
        int index = 0;
        if (value.has_value()) {
            for (std::size_t optionIndex = 0;
                 optionIndex < entry.spec->options.size(); ++optionIndex) {
                if (entry.spec->options[optionIndex].value == *value) {
                    index = static_cast<int>(optionIndex) + 1;
                    break;
                }
            }
        }
        spin->setValue(index);
    } else {
        auto* combo = qobject_cast<QComboBox*>(entry.widget);
        const int index = combo->findData(value.value_or(entry.inherit_value));
        combo->setCurrentIndex(index >= 0 ? index : 0);
    }
}

std::optional<int> OptionalVideoFieldSet::overrideValue(
        const std::string& key) const {
    const auto it = m_entries.find(key);
    if (it == m_entries.end()) return std::nullopt;
    const Entry& entry = it->second;

    int value = entry.inherit_value;
    if (entry.spec->kind == VideoSettingKind::Integer) {
        value = qobject_cast<QSpinBox*>(entry.widget)->value();
    } else if (entry.numeric_choice) {
        const int index = qobject_cast<QSpinBox*>(entry.widget)->value();
        if (index <= 0 ||
            index > static_cast<int>(entry.spec->options.size())) {
            return std::nullopt;
        }
        value = entry.spec->options[static_cast<std::size_t>(index - 1)].value;
    } else {
        value = qobject_cast<QComboBox*>(entry.widget)->currentData().toInt();
    }
    if (value == entry.inherit_value ||
        !is_valid_video_setting_value(*entry.spec, value)) {
        return std::nullopt;
    }
    return value;
}

int OptionalVideoFieldSet::effectiveValue(const std::string& key,
                                          int globalValue) const {
    const std::optional<int> value = overrideValue(key);
    return value.value_or(globalValue);
}

VideoSettingValues OptionalVideoFieldSet::overrides() const {
    VideoSettingValues result;
    for (const auto& [key, entry] : m_entries) {
        (void)entry;
        const std::optional<int> value = overrideValue(key);
        if (value.has_value()) result.emplace(key, *value);
    }
    return result;
}

void OptionalVideoFieldSet::setOverrides(const VideoSettingValues& values) {
    for (const auto& [key, entry] : m_entries) {
        (void)entry;
        const auto it = values.find(key);
        setOverride(key, it == values.end()
                             ? std::nullopt
                             : std::optional<int>(it->second));
    }
}

void OptionalVideoFieldSet::setEnabled(const std::string& key, bool enabled) {
    QWidget* field = widget(key);
    if (field) field->setEnabled(enabled);
}

void OptionalVideoFieldSet::resetToInherit() {
    for (const auto& [key, entry] : m_entries) {
        (void)entry;
        setOverride(key, std::nullopt);
    }
}

} // namespace goliath
