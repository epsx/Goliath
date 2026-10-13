// controller_profiles_dialog.hpp — custom card-based manager for reusable
// Settings -> Input snapshots.
#pragma once

#include "input/controller_profiles.hpp"

#include <QDialog>

#include <filesystem>
#include <functional>
#include <string>

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace goliath {

class ControllerProfilesDialog final : public QDialog {
public:
    enum class Mode {
        Manage,
        LoadOnly,
    };

    using ApplyCallback =
        std::function<void(const ControllerProfileBindings&)>;

    ControllerProfilesDialog(
        std::filesystem::path storePath,
        int currentDevice,
        ControllerProfileBindings currentBindings,
        ApplyCallback apply,
        QWidget* parent = nullptr,
        Mode mode = Mode::Manage);

private:
    void rebuildCards();
    void saveCurrentOrRename();
    void beginRename(const ControllerProfile& profile);
    void cancelRename();
    void replaceFromCurrent(const std::string& id);
    void deleteProfile(const std::string& id);
    bool persistCandidate(ControllerProfileStore candidate,
                          const QString& successMessage);
    void showError(const QString& message);

    std::filesystem::path m_storePath;
    int m_currentDevice = 1;
    ControllerProfileBindings m_currentBindings;
    ApplyCallback m_apply;
    Mode m_mode = Mode::Manage;
    ControllerProfileStore m_store;
    bool m_storeLoaded = false;

    QVBoxLayout* m_cardsLayout = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QPushButton* m_primaryButton = nullptr;
    QPushButton* m_cancelRenameButton = nullptr;
    QLabel* m_status = nullptr;
    std::string m_renamingId;
    std::string m_pendingDeleteId;
};

} // namespace goliath
