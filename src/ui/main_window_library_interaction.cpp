#include "ui/main_window.hpp"

#include "common/qt_path.hpp"
#include "game/jollygood_launch.hpp"
#include "ui/library_item_roles.hpp"
#include "ui/library_view_logic.hpp"
#include "ui/recording_id.hpp"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QSignalBlocker>
#include <QString>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <cstddef>
#include <filesystem>

namespace goliath {

namespace {
QTreeWidgetItem* treeItemForIndexes(
        QTreeWidget* tree, int gameIndex, int romIndex) {
    if (!tree) return nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = tree->topLevelItem(i);
        if (!parent || parent->data(0, GameIndexRole).toInt() != gameIndex) {
            continue;
        }
        if (romIndex < 0) return parent;
        for (int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem* child = parent->child(j);
            if (child && child->data(0, RomIndexRole).toInt() == romIndex) {
                return child;
            }
        }
        return nullptr;
    }
    return nullptr;
}


} // namespace

void MainWindow::selectIconItem(QListWidgetItem* item) {
    if (!item || !m_tree) return;
    QTreeWidgetItem* treeItem = treeItemForIndexes(
        m_tree, item->data(GameIndexRole).toInt(),
        item->data(RomIndexRole).toInt());
    if (!treeItem) return;
    if (treeItem->parent()) treeItem->parent()->setExpanded(true);
    m_tree->setCurrentItem(treeItem);
}

void MainWindow::expandAll() {
    setAllGroupsExpanded(true);
}

void MainWindow::collapseAll() {
    setAllGroupsExpanded(false);
}

void MainWindow::setAllGroupsExpanded(bool expanded) {
    QTreeWidgetItem* anchor = m_tree->itemAt(0, 0);
    if (!anchor) anchor = m_tree->currentItem();
    if (!expanded && anchor && anchor->parent()) {
        anchor = anchor->parent();
    }
    const int anchorGameIndex = anchor
        ? anchor->data(0, GameIndexRole).toInt()
        : -1;
    const int anchorRomIndex = anchor
        ? anchor->data(0, RomIndexRole).toInt()
        : -1;

    bool changed = false;
    {
        // Individual itemExpanded/itemCollapsed signals each refresh every
        // visible row. Suppress them during a bulk operation and perform the
        // stripe/selection work once after the final group changes state.
        const QSignalBlocker blocker(m_tree);
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* parent = m_tree->topLevelItem(i);
            if (parent->isExpanded() == expanded) continue;
            parent->setExpanded(expanded);
            changed = true;
        }
    }

    if (!changed) return;
    if (!expanded) ensureVisibleSelection(false);
    refreshLibraryListRowBackgrounds();
    if (!anchor) return;

    // Bulk expansion/collapse finishes across queued layout passes. Restore
    // the same top row after both so selection and scroll remain independent.
    QTimer::singleShot(0, m_tree,
        [this, anchorGameIndex, anchorRomIndex]() {
            QTreeWidgetItem* activeAnchor = treeItemForIndexes(
                m_tree, anchorGameIndex, anchorRomIndex);
            if (!activeAnchor) return;
            m_tree->doItemsLayout();
            m_tree->scrollToItem(
                activeAnchor, QAbstractItemView::PositionAtTop);
            QTimer::singleShot(0, m_tree,
                [this, anchorGameIndex, anchorRomIndex]() {
                    QTreeWidgetItem* finalAnchor = treeItemForIndexes(
                        m_tree, anchorGameIndex, anchorRomIndex);
                    if (!finalAnchor) return;
                    m_tree->doItemsLayout();
                    m_tree->scrollToItem(
                        finalAnchor, QAbstractItemView::PositionAtTop);
                });
        });
}

void MainWindow::showTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;
    showLibraryContextMenu(
        item, m_tree->viewport()->mapToGlobal(pos), true);
}

void MainWindow::showIconContextMenu(const QPoint& pos) {
    if (!m_iconView) return;
    QListWidgetItem* iconItem = m_iconView->itemAt(pos);
    if (!iconItem) return;
    QTreeWidgetItem* treeItem = treeItemForIndexes(
        m_tree, iconItem->data(GameIndexRole).toInt(),
        iconItem->data(RomIndexRole).toInt());
    if (!treeItem) return;
    m_iconView->setCurrentItem(iconItem);
    showLibraryContextMenu(
        treeItem, m_iconView->viewport()->mapToGlobal(pos), false);
}

void MainWindow::showLibraryContextMenu(
        QTreeWidgetItem* item, const QPoint& globalPos,
        bool allowExpansionActions) {
    if (!item) return;
    m_tree->setCurrentItem(item);
    int romIdx = item->data(0, RomIndexRole).toInt();
    const int gameIdx = item->data(0, GameIndexRole).toInt();

    QMenu menu(this);
    auto addDeferredAction = [this, &menu](
            const QString& text, auto action) {
        menu.addAction(text, this, [this, action]() {
            QTimer::singleShot(0, this, [this, action]() {
                (this->*action)();
            });
        });
    };
    addDeferredAction(QStringLiteral("Launch"),
                      &MainWindow::launchSelected);
    if (gameIdx >= 0 && static_cast<std::size_t>(gameIdx) < m_games.size()) {
        const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
        const auto media = selected_launch_media(game, romIdx);
        if (media.has_value()) {
            const bool favorite =
                m_gameLibraryState.is_favorite(game.system, *media);
            menu.addAction(
                favorite ? "Remove from Favorites" : "Add to Favorites",
                this, [this, favorite]() {
                    toggleSelectedFavorite(!favorite);
                });
        }
    }
    addDeferredAction(QStringLiteral("Export audio WAV..."),
                      &MainWindow::exportSelectedAudio);
    addDeferredAction(QStringLiteral("Game settings..."),
                      &MainWindow::openGameSettings);
    addDeferredAction(QStringLiteral("Save Data Manager..."),
                      &MainWindow::manageSaveData);
    addDeferredAction(QStringLiteral("Benchmark..."),
                      &MainWindow::benchmarkSelected);
    addDeferredAction(QStringLiteral("Verify game integrity..."),
                      &MainWindow::verifySelectedGameIntegrity);
    menu.addSeparator();
    menu.addAction("Open ROM folder", this, &MainWindow::openRomFolder);
    menu.addAction("Open snapshot folder", this, &MainWindow::openSnapshotFolder);

    if (gameIdx >= 0 && static_cast<std::size_t>(gameIdx) < m_games.size()) {
        const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
        const auto media = selected_launch_media(game, romIdx);
        const QString romId = QString::fromStdString(
            romIdx >= 0 && static_cast<std::size_t>(romIdx) < game.roms.size()
                ? game.roms[static_cast<std::size_t>(romIdx)].mame : game.short_name);
        const QString recordingId = game.system == "neogeocd" && media.has_value()
            ? recordingIdForMedia(QString::fromStdString(*media)) : romId;
        QString gifFolder = recordingFolderWithGifs(
            QCoreApplication::applicationDirPath(), recordingId);
        if (gifFolder.isEmpty()) {
            gifFolder = legacyGifFolder(path_to_qstring(
                m_paths.data_dir / "goliath" / "screenshots"), romId);
        }
        if (!gifFolder.isEmpty()) {
            menu.addAction("Open GIF folder", this, [this, gifFolder]() {
                revealInExplorer(gifFolder);
            });
        }
        if (media.has_value() && m_gameProfiles.find(game.system, *media)) {
            menu.addAction("Open per-game config folder", this,
                           &MainWindow::openGameConfigFolder);
        }
    }

    menu.addSeparator();
    if (allowExpansionActions && romIdx < 0) {
        if (treeItemHasExpandableChildren(item->childCount())) {
            if (item->isExpanded()) {
                menu.addAction("Collapse", [item]() { item->setExpanded(false); });
            } else {
                menu.addAction("Expand", [item]() { item->setExpanded(true); });
            }
        }
        menu.addAction("Expand all", this, &MainWindow::expandAll);
        menu.addAction("Collapse all", this, &MainWindow::collapseAll);
    }
    menu.exec(globalPos);
}

} // namespace goliath
