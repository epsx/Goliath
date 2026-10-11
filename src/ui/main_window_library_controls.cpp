#include "ui/main_window.hpp"

#include "game/jollygood_launch.hpp"
#include "ui/library_details_presentation.hpp"
#include "ui/library_item_roles.hpp"
#include "ui/library_view_logic.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <cstddef>
#include <cstdint>
#include <string>

namespace goliath {

void MainWindow::focusSearch() {
    m_searchEntry->setFocus();
    m_searchEntry->selectAll();
}

void MainWindow::onSortChanged() {
    if (m_sortKey.isEmpty()) m_sortKey = "display";

    m_config.set("UI", "sort_key", m_sortKey.toStdString());
    save_config(m_config);

    refreshLibraryView(true);
    selectFirstSortedResult();
}

void MainWindow::selectFirstSortedResult() {
    if (!m_tree) return;

    const bool filtered =
        (m_searchEntry && !m_searchEntry->text().trimmed().isEmpty()) ||
        m_favoritesOnly ||
        libraryPersonalFiltersActive(m_ratingFilter, m_playtimeFilter);
    if (m_libraryDisplayMode != LibraryDisplayMode::List && m_iconView) {
        if (m_iconView->count() > 0) {
            QListWidgetItem* first = m_iconView->item(0);
            m_iconView->setCurrentItem(first);
            m_iconView->scrollToItem(
                first, QAbstractItemView::PositionAtTop);
            selectIconItem(first);
        } else {
            m_tree->setCurrentItem(nullptr);
            m_tree->clearSelection();
            clearDetailsForNoSelection(filtered);
        }
        return;
    }
    const bool ratingSort = m_sortKey == "rating" ||
                            m_sortKey == "rating_desc";
    const bool playtimeSort = m_sortKey == "playtime" ||
                              m_sortKey == "playtime_desc";

    const auto isSelectable = [filtered](QTreeWidgetItem* item) {
        return item && !item->isHidden() &&
               (!filtered || item->data(0, ExactMatchRole).toBool());
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        if (!parent || parent->isHidden()) continue;

        QTreeWidgetItem* target = nullptr;
        const int gameIdx = parent->data(0, GameIndexRole).toInt();
        if ((ratingSort || playtimeSort) && gameIdx >= 0 &&
            static_cast<std::size_t>(gameIdx) < m_games.size()) {
            const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
            const bool ratingMetric = ratingSort;
            const std::int64_t groupMetric = gameSortMetric(
                game, ratingMetric);
            const auto metricForMedia = [&](const std::string& media) {
                return ratingMetric
                    ? static_cast<std::int64_t>(
                          m_gameLibraryState.rating(game.system, media))
                    : mediaPlaytimeSeconds(game, media);
            };

            const auto parentMedia = selected_launch_media(game, -1);
            if (isSelectable(parent) && parentMedia.has_value() &&
                metricForMedia(*parentMedia) == groupMetric) {
                target = parent;
            }

            if (!target) {
                for (int j = 0; j < parent->childCount(); ++j) {
                    QTreeWidgetItem* child = parent->child(j);
                    if (!isSelectable(child)) continue;
                    const int romIdx = child->data(0, RomIndexRole).toInt();
                    if (romIdx < 0 ||
                        static_cast<std::size_t>(romIdx) >= game.roms.size()) {
                        continue;
                    }
                    if (metricForMedia(
                            game.roms[static_cast<std::size_t>(romIdx)].file) ==
                        groupMetric) {
                        target = child;
                        break;
                    }
                }
            }
        }

        if (!target && isSelectable(parent)) target = parent;
        if (!target) {
            for (int j = 0; j < parent->childCount(); ++j) {
                QTreeWidgetItem* child = parent->child(j);
                if (isSelectable(child)) {
                    target = child;
                    break;
                }
            }
        }
        if (!target) continue;

        if (target->parent()) target->parent()->setExpanded(true);
        m_tree->setCurrentItem(target);
        m_tree->scrollToItem(parent, QAbstractItemView::PositionAtTop);
        return;
    }

    m_tree->setCurrentItem(nullptr);
    m_tree->clearSelection();
    clearDetailsForNoSelection(filtered);
}

void MainWindow::setRatingFilter(LibraryRatingFilter filter) {
    if (m_ratingFilter == filter) return;
    m_ratingFilter = filter;
    m_config.set("UI", "rating_filter",
                 std::string(libraryRatingFilterKey(filter)));
    save_config(m_config);
    updateFiltersButton();
    refreshLibraryView(true);
}

void MainWindow::setPlaytimeFilter(LibraryPlaytimeFilter filter) {
    if (m_playtimeFilter == filter) return;
    m_playtimeFilter = filter;
    m_config.set("UI", "playtime_filter",
                 std::string(libraryPlaytimeFilterKey(filter)));
    save_config(m_config);
    updateFiltersButton();
    refreshLibraryView(true);
}

void MainWindow::clearLibraryFilters() {
    if (!libraryPersonalFiltersActive(m_ratingFilter, m_playtimeFilter)) {
        return;
    }

    m_ratingFilter = LibraryRatingFilter::Any;
    m_playtimeFilter = LibraryPlaytimeFilter::Any;
    m_config.set("UI", "rating_filter", "any");
    m_config.set("UI", "playtime_filter", "any");
    save_config(m_config);

    if (m_ratingFilterGroup) {
        for (QAction* action : m_ratingFilterGroup->actions()) {
            if (action && action->data().toInt() ==
                    static_cast<int>(LibraryRatingFilter::Any)) {
                action->setChecked(true);
                break;
            }
        }
    }
    if (m_playtimeFilterGroup) {
        for (QAction* action : m_playtimeFilterGroup->actions()) {
            if (action && action->data().toInt() ==
                    static_cast<int>(LibraryPlaytimeFilter::Any)) {
                action->setChecked(true);
                break;
            }
        }
    }

    updateFiltersButton();
    refreshLibraryView(true);
}

void MainWindow::updateFiltersButton() {
    if (!m_filtersButton) return;

    QStringList descriptions;
    switch (m_ratingFilter) {
    case LibraryRatingFilter::Rated: descriptions << "Rating: Rated"; break;
    case LibraryRatingFilter::Unrated: descriptions << "Rating: Unrated"; break;
    case LibraryRatingFilter::AtLeast1:
        descriptions << "Rating: At least 1 star"; break;
    case LibraryRatingFilter::AtLeast2:
        descriptions << "Rating: At least 2 stars"; break;
    case LibraryRatingFilter::AtLeast3:
        descriptions << "Rating: At least 3 stars"; break;
    case LibraryRatingFilter::AtLeast4:
        descriptions << "Rating: At least 4 stars"; break;
    case LibraryRatingFilter::AtLeast5:
        descriptions << "Rating: 5 stars"; break;
    case LibraryRatingFilter::Any: break;
    }
    switch (m_playtimeFilter) {
    case LibraryPlaytimeFilter::Played:
        descriptions << "Playtime: Played"; break;
    case LibraryPlaytimeFilter::NotPlayed:
        descriptions << "Playtime: Not played"; break;
    case LibraryPlaytimeFilter::Any: break;
    }

    const int activeCount = static_cast<int>(descriptions.size());
    m_filtersButton->setText(activeCount == 0
        ? "Filters"
        : QString("Filters (%1)").arg(activeCount));
#if defined(GOLIATH_X11_CAPTURE)
    m_filtersButton->setText(m_filtersButton->text() + QString::fromUtf8(" \xE2\x96\xBC"));
#endif
    m_filtersButton->setToolTip(activeCount == 0
        ? "Choose the library view, sort, show variants, or filter by rating "
          "or playtime"
        : QString("Choose the view, sort, and filter the library\n%1")
              .arg(descriptions.join("\n")));
    if (m_clearFiltersAction) {
        m_clearFiltersAction->setEnabled(activeCount > 0);
    }
}

void MainWindow::onShowVariantsChanged(bool checked) {
    m_showVariants = checked;

    m_config.set("UI", "show_variants", m_showVariants ? "true" : "false");
    save_config(m_config);

    refreshLibraryView(true);
}

void MainWindow::onCommandOverlayChanged(bool checked) {
    m_commandOverlayEnabled = checked;

    m_config.set("UI", "command_overlay",
                 m_commandOverlayEnabled ? "true" : "false");
    save_config(m_config);
}

void MainWindow::onFavoritesOnlyChanged(bool checked) {
    m_favoritesOnly = checked;

    m_config.set("UI", "favorites_only", m_favoritesOnly ? "true" : "false");
    save_config(m_config);

    refreshLibraryView(true);
}

void MainWindow::toggleSelectedFavorite(bool favorite) {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        updateSelection();
        return;
    }

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 ||
        static_cast<std::size_t>(gameIdx) >= m_games.size()) {
        updateSelection();
        return;
    }

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) {
        updateSelection();
        return;
    }

    if (!m_gameLibraryStatePersistenceAvailable) {
        QMessageBox::critical(
            this, "Favorites",
            "The personal library state could not be loaded safely. "
            "Goliath will not overwrite it. Check Diagnostics & Logs for details.");
        updateSelection();
        return;
    }

    const bool previous =
        m_gameLibraryState.is_favorite(game.system, *media);
    if (previous == favorite) {
        updateSelection();
        return;
    }

    m_gameLibraryState.set_favorite(game.system, *media, favorite);
    std::string error;
    if (!m_gameLibraryState.save(m_paths.game_library_state_json, &error)) {
        m_gameLibraryState.set_favorite(game.system, *media, previous);
        QMessageBox::critical(
            this, "Favorites",
            QString("Could not save the Favorites list.\n\n%1")
                .arg(QString::fromStdString(error)));
        updateSelection();
        return;
    }

    const bool preserveTreeViewport = !m_favoritesOnly;
    const int treeScrollValue = preserveTreeViewport
        ? m_tree->verticalScrollBar()->value()
        : 0;

    refreshLibraryView(true);

    if (preserveTreeViewport) {
        m_tree->doItemsLayout();
        m_tree->verticalScrollBar()->setValue(treeScrollValue);
    }
}

void MainWindow::setSelectedRating(int rating) {
    if (rating < 1 || rating > 5) return;

    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        updateSelection();
        return;
    }

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 ||
        static_cast<std::size_t>(gameIdx) >= m_games.size()) {
        updateSelection();
        return;
    }

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) {
        updateSelection();
        return;
    }

    if (!m_gameLibraryStatePersistenceAvailable) {
        QMessageBox::critical(
            this, "Rating",
            "The personal library state could not be loaded safely. "
            "Goliath will not overwrite it. Check Diagnostics & Logs for details.");
        updateSelection();
        return;
    }

    const int previous = m_gameLibraryState.rating(game.system, *media);
    const int next = previous == rating ? 0 : rating;
    m_gameLibraryState.set_rating(game.system, *media, next);

    std::string error;
    if (!m_gameLibraryState.save(m_paths.game_library_state_json, &error)) {
        m_gameLibraryState.set_rating(game.system, *media, previous);
        QMessageBox::critical(
            this, "Rating",
            QString("Could not save the game rating.\n\n%1")
                .arg(QString::fromStdString(error)));
        updateSelection();
        return;
    }

    const bool ratingAffectsView = m_sortKey.startsWith("rating") ||
        m_ratingFilter != LibraryRatingFilter::Any;
    if (ratingAffectsView) {
        refreshLibraryView(true);
    } else {
        item->setData(
            0, LibraryRatingRole, libraryListRatingBadge(next));
        updateSelection();
    }
}

} // namespace goliath
