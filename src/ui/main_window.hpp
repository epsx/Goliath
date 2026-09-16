// main_window.hpp — the main application window: custom title bar, themed
// toolbar, game library (tree, search, exact-media personal filters, Favorites,
// sort, show-variants), the details/snapshot panel, theme switching, launching
// or benchmarking a ROM via
// jollygood, exact-media playtime observation, safe save-data management,
// one-shot WAV audio export, Settings, About, ROM rescan, and BIOS verification.

#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QtCore/qnamespace.h>
#include <QtGlobal>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "game/detached_process_tracker.hpp"
#include "game/game_library_state.hpp"
#include "game/game_model.hpp"
#include "game/game_playtime.hpp"
#include "game/game_profile.hpp"
#include "common/goliath_common.hpp"
#include "common/paths.hpp"
#include "ui/library_view_logic.hpp"

class QCloseEvent;
class QEvent;
class QResizeEvent;
class QSplitter;
class QScrollArea;
class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QLabel;
class QTextEdit;
class QPushButton;
class QAction;
class QActionGroup;
class QTimer;

namespace goliath {

class TitleBar;
class RescanWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Config config, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;
    bool event(QEvent* e) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSortChanged();
    void onShowVariantsChanged(bool checked);
    void onFavoritesOnlyChanged(bool checked);
    void toggleSelectedFavorite(bool favorite);
    void setSelectedRating(int rating);
    void updateSelection();
    void filterGames(const QString& text);
    void launchSelected();
    void launchSelectedItem(QTreeWidgetItem* item, int column);
    void openGameSettings();
    void manageSaveData();
    void benchmarkSelected();
    void exportSelectedAudio();
    void openLogging();
    void openAbout();
    void focusSearch();
    void expandAll();
    void collapseAll();
    void launchRandomGame();
    void openRomFolder();
    void openSnapshotFolder();
    void openGameConfigFolder();
    void showTreeContextMenu(const QPoint& pos);

    void openSettings();
    void rescanRoms();
    void onRescanFinished();
    void onRescanError(QString msg);
    void verifyBios();

private:
    void buildUi();
    // Apply a built-in theme by key; unknown keys fall back to Dark Modern.
    void applyTheme(const QString& themeName);
    void loadGames();
    std::vector<int> sortedGameOrder() const; // indices into m_games
    void populateTree();
    void refreshLibraryView(bool preserveSelection,
                            const QString& preferredRomOverride = {});
    void setAllGroupsExpanded(bool expanded);
    void setRatingFilter(LibraryRatingFilter filter);
    void setPlaytimeFilter(LibraryPlaytimeFilter filter);
    void clearLibraryFilters();
    void updateFiltersButton();
    void selectFirstSortedResult();
    bool mediaMatchesPersonalFilters(const Game& game,
                                     const std::string& media) const;
    std::int64_t mediaPlaytimeSeconds(const Game& game,
                                      const std::string& media) const;
    std::int64_t gameSortMetric(const Game& game, bool ratingMetric) const;
    std::vector<int> sortedVariantOrder(const Game& game) const;
    void setLibrarySystem(const std::string& system);
    void clearDetailsForNoSelection(bool filtered);
    void ensureVisibleSelection(bool filtered);
    void setSelectionActionsEnabled(bool enabled);
    void updateRatingButtons(int rating, bool enabled);
    void updateStatus();
    void centerWindow();
    void refreshResolvedPaths();
    void loadGameProfiles();
    void loadGamePlaytime();
    void loadGameLibraryState();
    void saveGamePlaytime();
    void trackGameProcess(qint64 pid, const std::string& system,
                          const std::string& media);
    void pollTrackedGameProcesses();
    void finishTrackedGameSessions();
    bool hasActiveTrackedGameProcess() const;
    QString resolveSnapshot(const std::string& shortName, const std::string& parentShort = "") const;
    void loadSnapshotFor(QString path, const std::string& shortFallback = "");
    QString selectedRomFile() const;   // bare filename of the currently selected rom, used for persistence
    QString selectedRomPath() const;   // full path of the currently selected rom, used for "Open ROM folder"
    void restoreSelection(const QString& preferredRom = {});
    void launchItem(QTreeWidgetItem* item);
    void revealInExplorer(const QString& path);

    // --- launch pipeline: resolves jollygood, ensures BIOS/config are in
    // place, then starts the emulator process for the selected cartridge/CD
    // media using the correct configured root and JGRF system arguments. ---
    void launchMedia(const QString& mediaFile, const std::string& system,
                     const GameLaunchProfile* profile = nullptr,
                     std::optional<std::filesystem::path> waveOutputPath =
                         std::nullopt);
    // Warn if the BIOS folder or expected archives are missing. The warning
    // is non-blocking; the user can still proceed.
    void warnIfBiosMissing();
    Config m_config;
    std::filesystem::path m_romDir;
    std::filesystem::path m_neocdDir;
    std::filesystem::path m_snapDir;
    AppPaths m_paths;
    std::vector<Game> m_games;
    GameProfileStore m_gameProfiles;
    GamePlaytimeStore m_gamePlaytime;
    GameLibraryStateStore m_gameLibraryState;
    bool m_gamePlaytimePersistenceAvailable = true;
    bool m_gameLibraryStatePersistenceAvailable = true;
    // Diagnostics toggle for this Goliath session only. It is intentionally
    // absent from goliath.ini and exact-media profiles.
    bool m_verboseJgrfLogging = false;
    std::vector<std::unique_ptr<DetachedProcessTracker>>
        m_trackedGameProcesses;
    QTimer* m_playtimeTimer = nullptr;
    bool m_exitWhenTrackedProcessesFinish = false;

    QString m_sortKey = "display";
    bool m_showVariants = true;
    bool m_favoritesOnly = false;
    LibraryRatingFilter m_ratingFilter = LibraryRatingFilter::Any;
    LibraryPlaytimeFilter m_playtimeFilter = LibraryPlaytimeFilter::Any;
    bool m_rebuildingLibraryView = false;
    std::string m_librarySystem = "neogeo";

    TitleBar* m_titleBar = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QPushButton* m_filtersButton = nullptr;
    QActionGroup* m_ratingFilterGroup = nullptr;
    QActionGroup* m_playtimeFilterGroup = nullptr;
    QAction* m_clearFiltersAction = nullptr;
    QAction* m_rescanAction = nullptr;

    QPushButton* m_mvsAesButton = nullptr;
    QPushButton* m_cdButton = nullptr;
    QTreeWidget* m_tree = nullptr;
    QLineEdit* m_searchEntry = nullptr;
    std::vector<QAction*> m_selectionActions;
    QPushButton* m_launchButton = nullptr;

    QScrollArea* m_detailsScroll = nullptr;
    QLabel* m_snapshotLabel = nullptr;
    QLabel* m_detailsTitleLabel = nullptr;
    QPushButton* m_favoriteButton = nullptr;
    std::array<QPushButton*, 5> m_ratingButtons{};
    QLabel* m_variantLabel = nullptr;
    std::map<std::string, QLabel*> m_infoLabels;
    QTextEdit* m_historyText = nullptr;
    QString m_detailsSelectionKey;
    RescanWorker* m_rescanWorker = nullptr;

    QSplitter* m_splitter = nullptr;
    bool m_detailsCollapsed = false;
    double m_savedSplitterRatio = 0.0;
};

} // namespace goliath
