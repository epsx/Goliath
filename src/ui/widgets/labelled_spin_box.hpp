// labelled_spin_box.hpp — compact numeric stepper whose visible values can
// differ from the integer indices used by the underlying setting schema.
#pragma once

#include <QSpinBox>
#include <QStringList>
#include <QValidator>

#include <algorithm>
#include <utility>

namespace goliath {

class LabelledSpinBox final : public QSpinBox {
public:
    explicit LabelledSpinBox(QWidget* parent = nullptr)
        : QSpinBox(parent) {
        setAccelerated(true);
        setKeyboardTracking(false);
    }

    void setLabels(QStringList labels) {
        m_labels = std::move(labels);
        setRange(0, std::max(0, static_cast<int>(m_labels.size()) - 1));
    }

protected:
    QString textFromValue(int value) const override {
        return value >= 0 && value < m_labels.size()
            ? m_labels.at(value)
            : QString::number(value);
    }

    int valueFromText(const QString& text) const override {
        const QString candidate = text.trimmed();
        for (int index = 0; index < m_labels.size(); ++index) {
            if (m_labels.at(index).compare(candidate, Qt::CaseInsensitive) == 0)
                return index;
        }
        return value();
    }

    QValidator::State validate(QString& input, int& position) const override {
        Q_UNUSED(position);
        const QString candidate = input.trimmed();
        if (candidate.isEmpty()) return QValidator::Intermediate;

        bool prefix = false;
        for (const QString& label : m_labels) {
            if (label.compare(candidate, Qt::CaseInsensitive) == 0)
                return QValidator::Acceptable;
            if (label.startsWith(candidate, Qt::CaseInsensitive))
                prefix = true;
        }
        return prefix ? QValidator::Intermediate : QValidator::Invalid;
    }

private:
    QStringList m_labels;
};

} // namespace goliath
