#include "ui/controller_profiles_dialog.hpp"

#include "ui/widgets/title_bar.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace goliath {

namespace {

QString bindingCountText(const ControllerProfile& profile) {
    const std::size_t count =
        controller_profile_binding_count(profile.bindings);
    return count == 1
        ? QStringLiteral("1 assigned control")
        : QStringLiteral("%1 assigned controls")
              .arg(static_cast<qulonglong>(count));
}

QLabel* makeHeading(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName("controller_profiles_heading");
    return label;
}

} // namespace

ControllerProfilesDialog::ControllerProfilesDialog(
        std::filesystem::path storePath,
        int currentDevice,
        ControllerProfileBindings currentBindings,
        ApplyCallback apply,
        QWidget* parent,
        Mode mode)
    : QDialog(parent),
      m_storePath(std::move(storePath)),
      m_currentDevice(currentDevice),
      m_currentBindings(sanitize_controller_profile_bindings(
          currentDevice, currentBindings)),
      m_apply(std::move(apply)),
      m_mode(mode) {
    const bool loadOnly = m_mode == Mode::LoadOnly;
    setWindowTitle(loadOnly
        ? "Load Controller Profile"
        : "Controller Profiles");
    setModal(true);
    resize(880, 650);
    setMinimumSize(760, 520);

    QVBoxLayout* content = nullptr;
    setupFramelessDialog(this, windowTitle(), &content, true, true);
    content->setSpacing(12);

    const QString deviceName = QString::fromStdString(
        controller_profile_device_name(m_currentDevice));
    auto* intro = new QLabel(loadOnly
        ? QString("Choose a <b>%1</b> controller profile. Loading it replaces "
                  "the mappings for the displayed per-game layout only; use "
                  "<b>Save Profile</b> to keep the changes.").arg(deviceName)
        : QString("Save and reuse complete controller mappings. The current "
                  "editor layout is <b>%1</b>. Applying a profile changes the "
                  "editor only; use <b>Save Input Config</b> afterwards to "
                  "write it to disk.").arg(deviceName));
    intro->setObjectName("themed_note");
    intro->setWordWrap(true);
    content->addWidget(intro);

    if (!loadOnly) {
        content->addWidget(makeHeading("Create a controller profile"));
        auto* editorCard = new QFrame();
        editorCard->setObjectName("controller_profile_editor");
        auto* editorLayout = new QHBoxLayout(editorCard);
        editorLayout->setContentsMargins(14, 12, 14, 12);
        editorLayout->setSpacing(8);

        m_nameEdit = new QLineEdit(editorCard);
        m_nameEdit->setPlaceholderText("Profile name");
        m_nameEdit->setMaxLength(64);
        m_nameEdit->setClearButtonEnabled(true);
        editorLayout->addWidget(m_nameEdit, 1);

        m_primaryButton = new QPushButton(
            "Create Profile from Current Mapping...", editorCard);
        m_primaryButton->setObjectName("controller_profile_primary");
        connect(m_primaryButton, &QPushButton::clicked,
                this, [this]() { saveCurrentOrRename(); });
        editorLayout->addWidget(m_primaryButton);

        m_cancelRenameButton = new QPushButton("Cancel", editorCard);
        connect(m_cancelRenameButton, &QPushButton::clicked,
                this, [this]() { cancelRename(); });
        m_cancelRenameButton->hide();
        editorLayout->addWidget(m_cancelRenameButton);
        content->addWidget(editorCard);
    }

    m_status = new QLabel();
    m_status->setObjectName("controller_profile_status");
    m_status->setWordWrap(true);
    m_status->hide();
    content->addWidget(m_status);

    content->addWidget(makeHeading(
        loadOnly ? "Compatible profiles" : "Saved profiles"));

    auto* scroll = new QScrollArea();
    scroll->setObjectName("controller_profiles_scroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollContents = new QWidget();
    scrollContents->setObjectName("controller_profiles_scroll_contents");
    m_cardsLayout = new QVBoxLayout(scrollContents);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(9);
    scroll->setWidget(scrollContents);
    content->addWidget(scroll, 1);

    auto* footer = new QHBoxLayout();
    footer->addStretch();
    auto* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeButton);
    content->addLayout(footer);

    std::string error;
    m_storeLoaded = m_store.load(m_storePath, &error);
    if (!m_storeLoaded) {
        showError(QString::fromStdString(error));
        if (m_nameEdit) m_nameEdit->setEnabled(false);
        if (m_primaryButton) m_primaryButton->setEnabled(false);
    }
    if (m_nameEdit) {
        connect(m_nameEdit, &QLineEdit::returnPressed,
                this, [this]() { saveCurrentOrRename(); });
    }
    rebuildCards();
}

void ControllerProfilesDialog::rebuildCards() {
    while (QLayoutItem* item = m_cardsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const bool loadOnly = m_mode == Mode::LoadOnly;
    const bool hasVisibleProfile = std::any_of(
        m_store.profiles().begin(), m_store.profiles().end(),
        [this, loadOnly](const ControllerProfile& profile) {
            return !loadOnly || profile.device == m_currentDevice;
        });
    if (!hasVisibleProfile) {
        auto* empty = new QFrame();
        empty->setObjectName("controller_profile_empty");
        auto* emptyLayout = new QVBoxLayout(empty);
        emptyLayout->setContentsMargins(18, 20, 18, 20);
        auto* title = new QLabel(loadOnly
            ? "No compatible controller profiles"
            : "No controller profiles yet", empty);
        title->setObjectName("controller_profile_name");
        title->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(title);
        auto* help = new QLabel(loadOnly
            ? "Create a profile for this input layout in Settings > Input, "
              "then return here to load it for this game."
            : "Map the controls in Settings > Input, enter a profile name "
              "above, then choose Create Profile from Current Mapping.", empty);
        help->setObjectName("controller_profile_secondary");
        help->setAlignment(Qt::AlignCenter);
        help->setWordWrap(true);
        emptyLayout->addWidget(help);
        m_cardsLayout->addWidget(empty);
        m_cardsLayout->addStretch();
        return;
    }

    for (const ControllerProfile& profile : m_store.profiles()) {
        if (loadOnly && profile.device != m_currentDevice) continue;
        auto* card = new QFrame();
        card->setObjectName("controller_profile_card");
        const bool compatible = profile.device == m_currentDevice;
        card->setProperty("profileCompatible", compatible);

        auto* cardLayout = new QGridLayout(card);
        cardLayout->setContentsMargins(14, 11, 14, 11);
        cardLayout->setHorizontalSpacing(12);
        cardLayout->setVerticalSpacing(4);
        cardLayout->setColumnStretch(0, 1);

        auto* name = new QLabel(QString::fromStdString(profile.name), card);
        name->setObjectName("controller_profile_name");
        cardLayout->addWidget(name, 0, 0);

        auto* device = new QLabel(
            QString::fromStdString(controller_profile_device_name(profile.device)),
            card);
        device->setObjectName("controller_profile_device");
        device->setProperty("profileCompatible", compatible);
        device->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cardLayout->addWidget(device, 0, 1);

        QString summary = bindingCountText(profile);
        if (!compatible) summary += " · Select this input layout to apply or update";
        auto* details = new QLabel(summary, card);
        details->setObjectName("controller_profile_secondary");
        details->setWordWrap(true);
        cardLayout->addWidget(details, 1, 0, 1, 2);

        auto* buttons = new QHBoxLayout();
        buttons->setSpacing(6);

        auto* applyButton = new QPushButton(
            loadOnly ? "Load" : "Apply", card);
        applyButton->setObjectName("controller_profile_apply");
        applyButton->setEnabled(compatible && m_storeLoaded);
        applyButton->setToolTip(loadOnly
            ? "Load these mappings as per-game overrides without saving yet."
            : (compatible
                ? "Load this profile into the Input editor without saving yet."
                : "Select the matching Input Device before applying this profile."));
        const std::string id = profile.id;
        connect(applyButton, &QPushButton::clicked, this, [this, id]() {
            const ControllerProfile* selected = m_store.find(id);
            if (!selected || selected->device != m_currentDevice) return;
            if (m_apply) m_apply(selected->bindings);
            accept();
        });
        buttons->addWidget(applyButton);

        if (loadOnly) {
            buttons->addStretch();
            cardLayout->addLayout(buttons, 2, 0, 1, 2);
            m_cardsLayout->addWidget(card);
            continue;
        }

        auto* updateButton = new QPushButton("Update", card);
        updateButton->setEnabled(compatible && m_storeLoaded);
        updateButton->setToolTip(compatible
            ? "Replace this profile with the current editor mapping."
            : "Only the matching Input Device can update this profile.");
        connect(updateButton, &QPushButton::clicked,
                this, [this, id]() { replaceFromCurrent(id); });
        buttons->addWidget(updateButton);

        auto* renameButton = new QPushButton("Rename", card);
        renameButton->setEnabled(m_storeLoaded);
        connect(renameButton, &QPushButton::clicked, this, [this, id]() {
            const ControllerProfile* selected = m_store.find(id);
            if (selected) beginRename(*selected);
        });
        buttons->addWidget(renameButton);

        auto* deleteButton = new QPushButton(
            m_pendingDeleteId == id ? "Confirm Delete" : "Delete", card);
        deleteButton->setObjectName(
            m_pendingDeleteId == id
                ? "controller_profile_delete_confirm"
                : "controller_profile_delete");
        deleteButton->setEnabled(m_storeLoaded);
        connect(deleteButton, &QPushButton::clicked,
                this, [this, id]() { deleteProfile(id); });
        buttons->addWidget(deleteButton);
        buttons->addStretch();
        cardLayout->addLayout(buttons, 2, 0, 1, 2);

        m_cardsLayout->addWidget(card);
    }
    m_cardsLayout->addStretch();
}

void ControllerProfilesDialog::saveCurrentOrRename() {
    if (!m_storeLoaded) return;
    const std::string name = m_nameEdit->text().toStdString();
    ControllerProfileStore candidate = m_store;
    std::string error;
    QString success;
    if (m_renamingId.empty()) {
        if (!candidate.add(name, m_currentDevice, m_currentBindings,
                           nullptr, &error)) {
            showError(QString::fromStdString(error));
            return;
        }
        success = "Controller profile saved.";
    } else {
        if (!candidate.rename(m_renamingId, name, &error)) {
            showError(QString::fromStdString(error));
            return;
        }
        success = "Controller profile renamed.";
    }
    if (!persistCandidate(std::move(candidate), success)) return;
    cancelRename();
    rebuildCards();
}

void ControllerProfilesDialog::beginRename(
        const ControllerProfile& profile) {
    m_pendingDeleteId.clear();
    m_renamingId = profile.id;
    m_nameEdit->setText(QString::fromStdString(profile.name));
    m_primaryButton->setText("Save Name");
    m_cancelRenameButton->show();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
    QTimer::singleShot(0, this, [this]() { rebuildCards(); });
}

void ControllerProfilesDialog::cancelRename() {
    m_renamingId.clear();
    m_nameEdit->clear();
    m_primaryButton->setText("Create Profile from Current Mapping...");
    m_cancelRenameButton->hide();
}

void ControllerProfilesDialog::replaceFromCurrent(const std::string& id) {
    ControllerProfileStore candidate = m_store;
    std::string error;
    if (!candidate.replace(
            id, m_currentDevice, m_currentBindings, &error)) {
        showError(QString::fromStdString(error));
        return;
    }
    m_pendingDeleteId.clear();
    if (persistCandidate(
            std::move(candidate), "Controller profile updated.")) {
        QTimer::singleShot(0, this, [this]() { rebuildCards(); });
    }
}

void ControllerProfilesDialog::deleteProfile(const std::string& id) {
    if (m_pendingDeleteId != id) {
        m_pendingDeleteId = id;
        m_status->setText(
            "Click Confirm Delete on the same card to remove this profile.");
        m_status->setProperty("profileStatus", "warning");
        m_status->style()->unpolish(m_status);
        m_status->style()->polish(m_status);
        m_status->show();
        QTimer::singleShot(0, this, [this]() { rebuildCards(); });
        return;
    }

    ControllerProfileStore candidate = m_store;
    if (!candidate.erase(id)) return;
    m_pendingDeleteId.clear();
    if (persistCandidate(
            std::move(candidate), "Controller profile deleted.")) {
        if (m_renamingId == id) cancelRename();
        QTimer::singleShot(0, this, [this]() { rebuildCards(); });
    }
}

bool ControllerProfilesDialog::persistCandidate(
        ControllerProfileStore candidate,
        const QString& successMessage) {
    std::string error;
    if (!candidate.save(m_storePath, &error)) {
        showError(QString::fromStdString(error));
        return false;
    }
    m_store = std::move(candidate);
    m_status->setText(successMessage);
    m_status->setProperty("profileStatus", "success");
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
    m_status->show();
    return true;
}

void ControllerProfilesDialog::showError(const QString& message) {
    m_status->setText(message);
    m_status->setProperty("profileStatus", "error");
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
    m_status->show();
}

} // namespace goliath
