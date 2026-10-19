#include "ui/tabs/misc_tab.hpp"
#include "ini/ini_document.hpp"
#include "ui/widgets/engraved_controls.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {
using Kind = FieldSpec::Kind;

const std::vector<FieldSpec>& jollygoodMiscFields() {
    static const std::vector<FieldSpec> fields = {
        {"cheatauto", "Auto-activate Cheats at Boot", 0, Kind::Bool, {}, 0, 0},
        {"corelog", "Core Log Level", 1, Kind::Combo,
         {{0, "Debug"}, {1, "Info"}, {2, "Warning"}, {3, "Error"}}, 0, 0},
        {"frontendlog", "Frontend Log Level", 1, Kind::Combo,
         {{0, "Debug"}, {1, "Info"}, {2, "Warning"}, {3, "Error"}}, 0, 0},
        {"bgfx", "Background Effects", 2, Kind::Combo,
         {{0, "None"}, {1, "Shade"}, {2, "Tea"}, {3, "Snow"},
          {4, "Mushrooms"}, {5, "Fire"}, {6, "Danmaku"}}, 0, 0},
        {"font", "OSD Font", 0, Kind::Combo,
         {{0, "6x8"}, {1, "8x8"}}, 0, 0},
    };
    return fields;
}
} // namespace

MiscTab::MiscTab(fs::path jollygoodSettingsIni, fs::path geolithIni, QWidget* parent)
    : QWidget(parent), m_jollygoodSettingsIni(std::move(jollygoodSettingsIni)),
      m_geolithIni(std::move(geolithIni)) {
    setupUi();
    load();
}

void MiscTab::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(
        layout->contentsMargins().left(),
        layout->contentsMargins().top(),
        layout->contentsMargins().right(), 0);

    auto* group = new QGroupBox("JollyGood Misc Settings");
    auto* form = new QFormLayout(group);
    for (const FieldSpec& spec : jollygoodMiscFields()) {
        m_widgets.addToForm(form, spec);
    }
    layout->addWidget(group);

    layout->addStretch();

    auto* buttonRow = new QHBoxLayout();
    auto* saveBtn = new EngravedActionButton(
        "Save Misc Settings", EngravedActionButton::Glyph::Save);
    connect(saveBtn, &QPushButton::clicked, this, &MiscTab::save);
    buttonRow->addWidget(saveBtn);

    auto* resetBtn = new EngravedActionButton(
        "Reset to Defaults", EngravedActionButton::Glyph::Reset);
    connect(resetBtn, &QPushButton::clicked, this, &MiscTab::resetDefaults);
    buttonRow->addWidget(resetBtn);
    layout->addLayout(buttonRow);
}

void MiscTab::load() {
    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);

    IniDocument coreCfg;
    coreCfg.load(m_geolithIni);

    for (const FieldSpec& spec : jollygoodMiscFields()) {
        int val = spec.default_value;
        if (cfg.has_option("misc", spec.key)) {
            try {
                val = normalize_field_value(
                    spec, std::stoi(cfg.get("misc", spec.key)));
            } catch (...) {
                val = spec.default_value;
            }
        }

        // Match JGRF's core-specific override order: geolith.ini wins over
        // settings.ini when it contains a valid frontend [misc] value.
        if (coreCfg.has_option("misc", spec.key)) {
            try {
                const int overrideValue = std::stoi(coreCfg.get("misc", spec.key));
                if (is_valid_field_value(spec, overrideValue))
                    val = overrideValue;
            } catch (...) {
            }
        }

        m_widgets.setValue(spec.key, val);
    }
}

void MiscTab::save() {
    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);
    for (const FieldSpec& spec : jollygoodMiscFields()) {
        cfg.set("misc", spec.key, std::to_string(m_widgets.value(spec.key)));
    }
    IniDocument coreCfg;
    coreCfg.load(m_geolithIni);
    coreCfg.remove_section("misc");
    std::string error;
    if (!save_ini_documents({
            {m_jollygoodSettingsIni, &cfg},
            {m_geolithIni, &coreCfg},
        }, &error)) {
        QMessageBox::warning(
            this, "Misc Settings",
            QStringLiteral(
                "Misc settings could not be saved as one transaction.\n\n") +
                QString::fromStdString(error));
    }
}

void MiscTab::resetDefaults() {
    const auto reply = QMessageBox::question(
        this, "Reset Misc Settings",
        "Reset JGRF Misc settings to their defaults?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    IniDocument cfg;
    cfg.load(m_jollygoodSettingsIni);
    for (const FieldSpec& spec : jollygoodMiscFields()) {
        cfg.set("misc", spec.key, std::to_string(spec.default_value));
    }
    IniDocument coreCfg;
    coreCfg.load(m_geolithIni);
    coreCfg.remove_section("misc");
    std::string error;
    if (!save_ini_documents({
            {m_jollygoodSettingsIni, &cfg},
            {m_geolithIni, &coreCfg},
        }, &error)) {
        QMessageBox::warning(
            this, "Reset Misc Settings",
            QStringLiteral(
                "Misc defaults could not be saved as one transaction.\n\n") +
                QString::fromStdString(error));
        return;
    }

    load();
}

} // namespace goliath
