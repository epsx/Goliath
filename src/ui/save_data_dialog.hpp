// save_data_dialog.hpp — modal, exact-media manager for JGRF/Geolith save
// states, persistent save files, and Goliath-owned validated backups.
#pragma once

#include <QDialog>
#include <QString>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "common/paths.hpp"
#include "game/save_data_manager.hpp"

class QLabel;
class QPushButton;
class QTableWidget;

namespace goliath {

class SaveDataDialog : public QDialog {
public:
    SaveDataDialog(QString display_name,
                   std::string system,
                   std::string media,
                   AppPaths paths,
                   std::function<bool()> mutations_blocked,
                   QWidget* parent = nullptr);

private:
    bool mutationAllowed();
    void refresh();
    void updateActions();
    void createBackup();
    void restoreBackup();
    void deleteSaveData();
    void deleteBackup();
    void openDirectory(const std::filesystem::path& directory,
                       const QString& purpose);

    QString m_displayName;
    std::string m_system;
    std::string m_media;
    AppPaths m_paths;
    JgrfSavePaths m_savePaths;
    std::function<bool()> m_mutationsBlocked;

    std::vector<ManagedSaveFile> m_saveFiles;
    std::vector<SaveBackupRecord> m_backups;

    QLabel* m_readOnlyNote = nullptr;
    QLabel* m_status = nullptr;
    QTableWidget* m_saveTable = nullptr;
    QTableWidget* m_backupTable = nullptr;
    QPushButton* m_createBackupButton = nullptr;
    QPushButton* m_deleteSaveButton = nullptr;
    QPushButton* m_restoreBackupButton = nullptr;
    QPushButton* m_deleteBackupButton = nullptr;
};

} // namespace goliath
