#include "core_tab.hpp"
#include "core_tab_help.hpp"
#include "ini/ini_document.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QStyle>
#include <QVBoxLayout>

#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {
using Kind = FieldSpec::Kind;

struct GroupedField {
    QString group;
    FieldSpec spec;
};

void showUniverseBiosHelp(QWidget* parent) {
    QDialog dialog(parent);
    dialog.resize(640, 470);
    dialog.setModal(true);
    dialog.setSizeGripEnabled(false);

    QVBoxLayout* contentLayout = nullptr;
    setupFramelessDialog(
        &dialog, "Universe BIOS Settings", &contentLayout, false, false);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(16);

    auto* iconLabel = new QLabel(&dialog);
    iconLabel->setAccessibleName("Information");
    iconLabel->setPixmap(dialog.style()->standardIcon(
        QStyle::SP_MessageBoxInformation, nullptr, &dialog).pixmap(QSize(48, 48)));
    bodyLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textLabel = new QLabel(universe_bios_help_text(), &dialog);
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bodyLayout->addWidget(textLabel, 1, Qt::AlignTop);
    contentLayout->addLayout(bodyLayout, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    contentLayout->addWidget(buttons);

    dialog.exec();
}

const std::vector<GroupedField>& geolithCoreFields() {
    static const std::vector<GroupedField> fields = {
        {"System", {"system", "System Type", 0, Kind::Combo,
                     {{0, "AES (Console)"}, {1, "MVS (Arcade)"}, {2, "Universe BIOS"}}, 0, 0}},
        {"System", {"cdsystem", "CD System", 2, Kind::Combo,
                     {{0, "Neo Geo CD (Front Loader)"}, {1, "Neo Geo CD (Top Loader)"},
                      {2, "Neo Geo CDZ"}, {3, "Universe BIOS"}}, 0, 0}},
        {"System", {"unihw", "Universe BIOS HW", 1, Kind::Combo,
                     {{0, "AES (Console)"}, {1, "MVS (Arcade)"}}, 0, 0}},
        {"System", {"region", "Region", 0, Kind::Combo,
                     {{0, "US"}, {1, "JP"}, {2, "AS"}, {3, "EU"}}, 0, 0}},

        {"Memory Card", {"memcard_inserted", "Memory Card Inserted", 1, Kind::Bool, {}, 0, 0}},
        {"Memory Card", {"memcard_wp", "Memory Card Write-Protected", 0, Kind::Bool, {}, 0, 0}},

        {"Misc", {"freeplay", "Free Play (Only MVS)", 0, Kind::Bool, {}, 0, 0}},
        {"Misc", {"settingmode", "Setting Mode (Only MVS)", 0, Kind::Bool, {}, 0, 0}},
        {"Misc", {"adpcm_wrap", "ADPCM Accumulator Wrap", 1, Kind::Bool, {}, 0, 0}},
        {"Misc", {"cd_dma_len_limit", "CD DMA Length Limit (Hack)", 0, Kind::Bool, {}, 0, 0}},
    };
    return fields;
}
} // namespace

CoreTab::CoreTab(fs::path geolithIni, QWidget* parent)
    : QWidget(parent), m_geolithIni(std::move(geolithIni)) {
    setupUi();
    load();
}

void CoreTab::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* container = new QWidget();
    auto* form = new QVBoxLayout(container);

    QMap<QString, QFormLayout*> groups;
    for (const GroupedField& gf : geolithCoreFields()) {
        QFormLayout* groupLayout = groups.value(gf.group, nullptr);
        if (!groupLayout) {
            auto* box = new QGroupBox(gf.group);
            groupLayout = new QFormLayout(box);
            groups[gf.group] = groupLayout;
            form->addWidget(box);

            if (gf.group == "System") {
                auto* helpButton = new QPushButton("Universe BIOS Help");
                helpButton->setObjectName("core_universe_bios_help");
                helpButton->setIcon(helpButton->style()->standardIcon(
                    QStyle::SP_MessageBoxInformation, nullptr, helpButton));
                helpButton->setIconSize(QSize(16, 16));
                helpButton->setToolTip(
                    "Explain how System Type, CD System, Universe BIOS HW, and Region interact.");
                connect(helpButton, &QPushButton::clicked, this,
                        [this]() { showUniverseBiosHelp(this); });
                groupLayout->addRow(helpButton);
            }
        }
        m_widgets.addToForm(groupLayout, gf.spec);

        if (gf.spec.key == "adpcm_wrap") {
            m_widgets.widget(gf.spec.key)->setToolTip(
                "Geolith compatibility hack. Disabling ADPCM accumulator wrap may fix "
                "sound effects in some buggy games such as Ganryu and Nightmare in the Dark.");
        } else if (gf.spec.key == "cd_dma_len_limit") {
            m_widgets.widget(gf.spec.key)->setToolTip(
                "Geolith Neo Geo CD compatibility hack. Corrects BIOS upload pointers when "
                "a CD buffer DMA is larger than one sector; this can fix corrupted sound "
                "in Art of Fighting CD's bonus stage. Real hardware behavior is unconfirmed.");
        }
    }

    form->addStretch();
    scroll->setWidget(container);
    layout->addWidget(scroll);

    auto* saveBtn = new QPushButton("Save Core Settings");
    connect(saveBtn, &QPushButton::clicked, this, &CoreTab::save);
    layout->addWidget(saveBtn);
}

void CoreTab::load() {
    IniDocument cfg;
    cfg.load(m_geolithIni);
    for (const GroupedField& gf : geolithCoreFields()) {
        int val = gf.spec.default_value;
        if (cfg.has_option("geolith", gf.spec.key)) {
            try {
                val = std::stoi(cfg.get("geolith", gf.spec.key));
            } catch (...) {
                val = gf.spec.default_value;
            }
        }
        val = normalize_field_value(gf.spec, val);
        m_widgets.setValue(gf.spec.key, val);
    }
}

void CoreTab::save() {
    std::error_code ec;
    fs::create_directories(m_geolithIni.parent_path(), ec);

    IniDocument cfg;
    cfg.load(m_geolithIni);
    for (const GroupedField& gf : geolithCoreFields()) {
        int value = m_widgets.value(gf.spec.key);
        cfg.set("geolith", gf.spec.key, std::to_string(value));
    }
    cfg.save(m_geolithIni);

}

} // namespace goliath
