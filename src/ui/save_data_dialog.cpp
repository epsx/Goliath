#include "ui/save_data_dialog.hpp"

#include "game/filesystem_io.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

QString formatted_size(std::uintmax_t bytes) {
    const QLocale locale;
    if (bytes < 1024) return locale.toString(static_cast<qulonglong>(bytes)) + " B";
    const double kib = static_cast<double>(bytes) / 1024.0;
    if (kib < 1024.0) return locale.toString(kib, 'f', 1) + " KiB";
    return locale.toString(kib / 1024.0, 'f', 2) + " MiB";
}

QString modified_text(const fs::path& path) {
    return QFileInfo(path_to_qstring(filesystem_io_path(path)))
        .lastModified().toString("yyyy-MM-dd HH:mm:ss");
}

QTableWidget* make_table(const QStringList& headers) {
    auto* table = new QTableWidget();
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}

QTableWidgetItem* item(const QString& text) {
    auto* result = new QTableWidgetItem(text);
    result->setToolTip(text);
    return result;
}

} // namespace

SaveDataDialog::SaveDataDialog(
        QString display_name,
        std::string system,
        std::string media,
        AppPaths paths,
        std::function<bool()> mutations_blocked,
        QWidget* parent)
    : QDialog(parent),
      m_displayName(std::move(display_name)),
      m_system(std::move(system)),
      m_media(std::move(media)),
      m_paths(std::move(paths)),
      m_savePaths(resolve_jgrf_save_paths(m_paths)),
      m_mutationsBlocked(std::move(mutations_blocked)) {
    resize(980, 720);

    QVBoxLayout* layout = nullptr;
    setupFramelessDialog(this, "Save Data Manager", &layout, true, true);

    auto* heading = new QLabel(m_displayName);
    heading->setTextFormat(Qt::PlainText);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setPointSize(headingFont.pointSize() + 1);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto* identity = new QLabel(
        QString("Exact media: %1\nJGRF game name: %2\nLayout: %3")
            .arg(QString::fromStdString(m_media))
            .arg(QString::fromStdString(jgrf_game_name(m_media)))
            .arg(jgrf_save_layout_name(m_savePaths.layout)));
    identity->setTextFormat(Qt::PlainText);
    identity->setWordWrap(true);
    identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(identity);

    auto* locations = new QLabel(
        QString("State directory: %1\nSave directory: %2")
            .arg(path_to_qstring(m_savePaths.state_directory))
            .arg(path_to_qstring(m_savePaths.save_directory)));
    locations->setTextFormat(Qt::PlainText);
    locations->setWordWrap(true);
    locations->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(locations);

    m_readOnlyNote = new QLabel();
    m_readOnlyNote->setWordWrap(true);
    m_readOnlyNote->setObjectName("warning_note");
    layout->addWidget(m_readOnlyNote);

    auto* saveBox = new QGroupBox("Current JGRF / Geolith data");
    auto* saveLayout = new QVBoxLayout(saveBox);
    m_saveTable = make_table({"Type", "Filename", "Size", "Modified"});
    m_saveTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    m_saveTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    m_saveTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_saveTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    saveLayout->addWidget(m_saveTable);

    auto* saveButtons = new QHBoxLayout();
    m_createBackupButton = new QPushButton("Create Backup");
    m_deleteSaveButton = new QPushButton("Delete Selected File");
    auto* openState = new QPushButton("Open State Folder");
    auto* openSave = new QPushButton("Open Save Folder");
    saveButtons->addWidget(m_createBackupButton);
    saveButtons->addWidget(m_deleteSaveButton);
    saveButtons->addStretch();
    saveButtons->addWidget(openState);
    saveButtons->addWidget(openSave);
    saveLayout->addLayout(saveButtons);
    layout->addWidget(saveBox, 1);

    auto* backupBox = new QGroupBox("Exact-media backups");
    auto* backupLayout = new QVBoxLayout(backupBox);
    m_backupTable = make_table(
        {"Archive", "Contents", "Size", "Modified", "Status"});
    m_backupTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column) {
        m_backupTable->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    backupLayout->addWidget(m_backupTable);

    auto* backupButtons = new QHBoxLayout();
    m_restoreBackupButton = new QPushButton("Restore Selected Backup");
    m_deleteBackupButton = new QPushButton("Delete Selected Backup");
    auto* openBackups = new QPushButton("Open Backup Folder");
    backupButtons->addWidget(m_restoreBackupButton);
    backupButtons->addWidget(m_deleteBackupButton);
    backupButtons->addStretch();
    backupButtons->addWidget(openBackups);
    backupLayout->addLayout(backupButtons);
    layout->addWidget(backupBox, 1);

    auto* footer = new QHBoxLayout();
    m_status = new QLabel();
    m_status->setTextFormat(Qt::PlainText);
    auto* refreshButton = new QPushButton("Refresh");
    auto* closeButton = new QPushButton("Close");
    footer->addWidget(m_status, 1);
    footer->addWidget(refreshButton);
    footer->addWidget(closeButton);
    layout->addLayout(footer);

    connect(m_saveTable, &QTableWidget::itemSelectionChanged,
            this, [this]() { updateActions(); });
    connect(m_backupTable, &QTableWidget::itemSelectionChanged,
            this, [this]() { updateActions(); });
    connect(m_createBackupButton, &QPushButton::clicked,
            this, [this]() { createBackup(); });
    connect(m_deleteSaveButton, &QPushButton::clicked,
            this, [this]() { deleteSaveData(); });
    connect(m_restoreBackupButton, &QPushButton::clicked,
            this, [this]() { restoreBackup(); });
    connect(m_deleteBackupButton, &QPushButton::clicked,
            this, [this]() { deleteBackup(); });
    connect(openState, &QPushButton::clicked, this, [this]() {
        openDirectory(m_savePaths.state_directory, "state");
    });
    connect(openSave, &QPushButton::clicked, this, [this]() {
        openDirectory(m_savePaths.save_directory, "save");
    });
    connect(openBackups, &QPushButton::clicked, this, [this]() {
        openDirectory(game_save_backup_directory(
                          m_paths, m_system, m_media),
                      "backup");
    });
    connect(refreshButton, &QPushButton::clicked,
            this, [this]() { refresh(); });
    connect(closeButton, &QPushButton::clicked,
            this, &QDialog::accept);

    refresh();
}

bool SaveDataDialog::mutationAllowed() {
    if (!m_mutationsBlocked || !m_mutationsBlocked()) return true;
    QMessageBox::warning(
        this, "Save Data Manager",
        "A JGRF game launched by Goliath is still running. Close every game "
        "before creating, restoring, or deleting save data.");
    refresh();
    return false;
}

void SaveDataDialog::refresh() {
    const bool blocked = m_mutationsBlocked && m_mutationsBlocked();
    m_readOnlyNote->setVisible(blocked);
    m_readOnlyNote->setText(
        "Read-only mode: a Goliath-tracked JGRF process is still active. "
        "Close every running game, then press Refresh to enable backup, "
        "restore, and delete operations.");

    std::string saveError;
    m_saveFiles = list_game_save_data(m_paths, m_media, &saveError);
    m_saveTable->setRowCount(static_cast<int>(m_saveFiles.size()));
    for (int row = 0; row < static_cast<int>(m_saveFiles.size()); ++row) {
        const ManagedSaveFile& file = m_saveFiles[static_cast<std::size_t>(row)];
        m_saveTable->setItem(
            row, 0, item(managed_save_kind_name(file.kind)));
        m_saveTable->setItem(
            row, 1, item(path_to_qstring(file.path.filename())));
        m_saveTable->setItem(row, 2, item(formatted_size(file.size)));
        m_saveTable->setItem(row, 3, item(modified_text(file.path)));
    }

    std::string backupError;
    m_backups = list_game_save_backups(
        m_paths, m_system, m_media, &backupError);
    m_backupTable->setRowCount(static_cast<int>(m_backups.size()));
    for (int row = 0; row < static_cast<int>(m_backups.size()); ++row) {
        const SaveBackupRecord& backup =
            m_backups[static_cast<std::size_t>(row)];
        m_backupTable->setItem(
            row, 0, item(path_to_qstring(backup.path.filename())));
        m_backupTable->setItem(
            row, 1,
            item(backup.compatible
                     ? QString("%1 file(s)").arg(
                           static_cast<qulonglong>(backup.file_count))
                     : QString("Unknown")));
        m_backupTable->setItem(row, 2, item(formatted_size(backup.size)));
        m_backupTable->setItem(row, 3, item(modified_text(backup.path)));
        QTableWidgetItem* status = item(
            backup.compatible ? "Ready" : "Invalid / incompatible");
        if (!backup.problem.empty())
            status->setToolTip(QString::fromStdString(backup.problem));
        m_backupTable->setItem(row, 4, status);
    }

    QString statusText = QString("%1 current file(s), %2 backup(s).")
        .arg(static_cast<qulonglong>(m_saveFiles.size()))
        .arg(static_cast<qulonglong>(m_backups.size()));
    if (!saveError.empty())
        statusText = "Save-data error: " + QString::fromStdString(saveError);
    else if (!backupError.empty())
        statusText = "Backup error: " + QString::fromStdString(backupError);
    m_status->setText(statusText);
    updateActions();
}

void SaveDataDialog::updateActions() {
    const bool blocked = m_mutationsBlocked && m_mutationsBlocked();
    const int saveRow = m_saveTable->currentRow();
    const int backupRow = m_backupTable->currentRow();
    const bool saveSelected =
        saveRow >= 0 && saveRow < static_cast<int>(m_saveFiles.size());
    const bool backupSelected =
        backupRow >= 0 && backupRow < static_cast<int>(m_backups.size());
    const bool compatible = backupSelected &&
        m_backups[static_cast<std::size_t>(backupRow)].compatible;

    m_createBackupButton->setEnabled(!blocked && !m_saveFiles.empty());
    m_deleteSaveButton->setEnabled(!blocked && saveSelected);
    m_restoreBackupButton->setEnabled(!blocked && compatible);
    m_deleteBackupButton->setEnabled(!blocked && backupSelected);
}

void SaveDataDialog::createBackup() {
    if (!mutationAllowed()) return;
    const SaveDataOperationResult result = create_game_save_backup(
        m_paths, m_system, m_media);
    if (!result.success) {
        QMessageBox::critical(
            this, "Create Save-Data Backup",
            QString::fromStdString(result.error));
        refresh();
        return;
    }
    QMessageBox::information(
        this, "Create Save-Data Backup",
        QString("Backed up %1 file(s).\n\n%2")
            .arg(static_cast<qulonglong>(result.file_count))
            .arg(path_to_qstring(result.backup_path)));
    refresh();
}

void SaveDataDialog::restoreBackup() {
    const int row = m_backupTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_backups.size()) ||
        !m_backups[static_cast<std::size_t>(row)].compatible ||
        !mutationAllowed()) {
        return;
    }
    const SaveBackupRecord& backup =
        m_backups[static_cast<std::size_t>(row)];
    if (QMessageBox::question(
            this, "Restore Save-Data Backup",
            QString("Restore this exact-media snapshot?\n\n%1\n\n"
                    "Current files will be backed up first, then replaced by "
                    "the snapshot contents.")
                .arg(path_to_qstring(backup.path.filename()))) !=
        QMessageBox::Yes) {
        return;
    }

    const SaveDataOperationResult result = restore_game_save_backup(
        m_paths, m_system, m_media, backup.path);
    if (!result.success) {
        QMessageBox::critical(
            this, "Restore Save-Data Backup",
            QString::fromStdString(result.error));
        refresh();
        return;
    }

    QString message = QString("Restored %1 file(s).").arg(
        static_cast<qulonglong>(result.file_count));
    if (result.protective_backup.has_value()) {
        message += "\n\nProtective backup:\n" +
                   path_to_qstring(*result.protective_backup);
    } else {
        message += "\n\nThere was no previous save data to protect.";
    }
    QMessageBox::information(this, "Restore Save-Data Backup", message);
    refresh();
}

void SaveDataDialog::deleteSaveData() {
    const int row = m_saveTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_saveFiles.size()) ||
        !mutationAllowed()) {
        return;
    }
    const ManagedSaveFile& file = m_saveFiles[static_cast<std::size_t>(row)];
    if (QMessageBox::question(
            this, "Delete Save Data",
            QString("Delete %1?\n\n%2\n\nA protective backup of all "
                    "current files will be created first.")
                .arg(managed_save_kind_name(file.kind))
                .arg(path_to_qstring(file.path.filename()))) !=
        QMessageBox::Yes) {
        return;
    }

    const SaveDataOperationResult result = delete_game_save_data(
        m_paths, m_system, m_media, file.kind);
    if (!result.success) {
        QMessageBox::critical(
            this, "Delete Save Data", QString::fromStdString(result.error));
        refresh();
        return;
    }
    QString message = "The selected file was deleted.";
    if (result.protective_backup.has_value()) {
        message += "\n\nProtective backup:\n" +
                   path_to_qstring(*result.protective_backup);
    }
    QMessageBox::information(this, "Delete Save Data", message);
    refresh();
}

void SaveDataDialog::deleteBackup() {
    const int row = m_backupTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_backups.size()) ||
        !mutationAllowed()) {
        return;
    }
    const SaveBackupRecord& backup = m_backups[static_cast<std::size_t>(row)];
    if (QMessageBox::question(
            this, "Delete Save-Data Backup",
            QString("Permanently delete this backup archive?\n\n%1")
                .arg(path_to_qstring(backup.path.filename()))) !=
        QMessageBox::Yes) {
        return;
    }

    const SaveDataOperationResult result = delete_game_save_backup(
        m_paths, m_system, m_media, backup.path);
    if (!result.success) {
        QMessageBox::critical(
            this, "Delete Save-Data Backup",
            QString::fromStdString(result.error));
    }
    refresh();
}

void SaveDataDialog::openDirectory(const fs::path& directory,
                                   const QString& purpose) {
    std::error_code ec;
    fs::create_directories(filesystem_io_path(directory), ec);
    if (ec) {
        QMessageBox::warning(
            this, "Open Folder",
            QString("Could not create the %1 directory.\n\n%2")
                .arg(purpose)
                .arg(QString::fromStdString(ec.message())));
        return;
    }
    if (!QDesktopServices::openUrl(
            QUrl::fromLocalFile(path_to_qstring(directory)))) {
        QMessageBox::warning(
            this, "Open Folder",
            QString("Could not open the %1 directory.\n\n%2")
                .arg(purpose)
                .arg(path_to_qstring(directory)));
    }
}

} // namespace goliath
