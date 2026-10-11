#include "ui/main_window.hpp"

#include "game/jollygood_launch.hpp"
#include "ui/library_item_delegate.hpp"
#include "ui/library_item_roles.hpp"
#include "ui/library_view_logic.hpp"

#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QSize>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace goliath {

namespace {

std::string lowercaseAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return text;
}

struct LibrarySortValue {
    std::int64_t numeric = 0;
    std::string text;
};

std::int64_t libraryYearSortValue(const Game& game) {
    if (!game.year.has_value() || game.year->empty()) return 0;
    try {
        return std::stoi(*game.year);
    } catch (...) {
        return 0;
    }
}

} // namespace

std::int64_t MainWindow::mediaPlaytimeSeconds(
        const Game& game, const std::string& media) const {
    const GamePlaytimeRecord* record =
        m_gamePlaytime.find(game.system, media);
    return record ? record->total_seconds : 0;
}

bool MainWindow::mediaMatchesPersonalFilters(
        const Game& game, const std::string& media) const {
    return libraryPersonalFiltersAllow(
        m_ratingFilter,
        m_playtimeFilter,
        m_gameLibraryState.rating(game.system, media),
        mediaPlaytimeSeconds(game, media));
}

std::int64_t MainWindow::gameSortMetric(
        const Game& game, bool ratingMetric) const {
    std::int64_t best = 0;
    const auto consider = [&](const std::string& media) {
        const std::int64_t value = ratingMetric
            ? static_cast<std::int64_t>(
                  m_gameLibraryState.rating(game.system, media))
            : mediaPlaytimeSeconds(game, media);
        best = std::max(best, value);
    };

    const auto parentMedia = selected_launch_media(game, -1);
    if (parentMedia.has_value()) consider(*parentMedia);
    for (const Rom& rom : game.roms) {
        if (!rom.main) consider(rom.file);
    }
    return best;
}

std::vector<int> MainWindow::sortedVariantOrder(const Game& game) const {
    std::vector<int> order;
    order.reserve(game.roms.size());
    for (std::size_t index = 0; index < game.roms.size(); ++index) {
        if (!game.roms[index].main)
            order.push_back(static_cast<int>(index));
    }

    const bool ratingSort = m_sortKey == "rating" ||
                            m_sortKey == "rating_desc";
    const bool playtimeSort = m_sortKey == "playtime" ||
                              m_sortKey == "playtime_desc";
    if (!ratingSort && !playtimeSort) return order;

    const bool descending = m_sortKey.endsWith("_desc");
    std::vector<LibrarySortValue> sortValues(game.roms.size());
    for (int index : order) {
        const std::size_t valueIndex = static_cast<std::size_t>(index);
        const Rom& rom = game.roms[valueIndex];
        LibrarySortValue& value = sortValues[valueIndex];
        value.numeric = ratingSort
            ? static_cast<std::int64_t>(
                  m_gameLibraryState.rating(game.system, rom.file))
            : mediaPlaytimeSeconds(game, rom.file);
        value.text = lowercaseAscii(
            rom.mame.empty() ? rom.file : rom.mame);
    }

    const auto sortValueForIndex =
        [&sortValues](int itemIndex) -> const LibrarySortValue& {
            return sortValues[static_cast<std::size_t>(itemIndex)];
        };
    std::stable_sort(order.begin(), order.end(), [&](int left, int right) {
        const LibrarySortValue& leftValue = sortValueForIndex(left);
        const LibrarySortValue& rightValue = sortValueForIndex(right);

        if (libraryMetricPrecedes(
                leftValue.numeric, rightValue.numeric, descending)) {
            return true;
        }
        if (libraryMetricPrecedes(
                rightValue.numeric, leftValue.numeric, descending)) {
            return false;
        }
        return leftValue.text < rightValue.text;
    });
    return order;
}

std::vector<int> MainWindow::sortedGameOrder(
        const std::vector<std::string>& listTitles) const {
    std::vector<int> idx(m_games.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);

    const bool yearSort = m_sortKey == "year" ||
                          m_sortKey == "year_desc";
    const bool ratingSort = m_sortKey == "rating" ||
                            m_sortKey == "rating_desc";
    const bool playtimeSort = m_sortKey == "playtime" ||
                              m_sortKey == "playtime_desc";
    const bool metricSort = ratingSort || playtimeSort;

    std::vector<LibrarySortValue> sortValues(m_games.size());
    for (std::size_t index = 0; index < m_games.size(); ++index) {
        LibrarySortValue& value = sortValues[index];
        if (yearSort) {
            value.numeric = libraryYearSortValue(m_games[index]);
            continue;
        }

        value.text = lowercaseAscii(listTitles[index]);
        if (metricSort) {
            value.numeric = gameSortMetric(m_games[index], ratingSort);
        }
    }

    const auto sortValueForIndex =
        [&sortValues](int itemIndex) -> const LibrarySortValue& {
            return sortValues[static_cast<std::size_t>(itemIndex)];
        };
    if (m_sortKey == "display_desc") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return sortValueForIndex(a).text > sortValueForIndex(b).text;
        });
    } else if (m_sortKey == "year_desc") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return sortValueForIndex(a).numeric > sortValueForIndex(b).numeric;
        });
    } else if (m_sortKey == "year") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return sortValueForIndex(a).numeric < sortValueForIndex(b).numeric;
        });
    } else if (metricSort) {
        const bool descending = m_sortKey.endsWith("_desc");
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            const LibrarySortValue& left = sortValueForIndex(a);
            const LibrarySortValue& right = sortValueForIndex(b);
            if (libraryMetricPrecedes(
                    left.numeric, right.numeric, descending)) return true;
            if (libraryMetricPrecedes(
                    right.numeric, left.numeric, descending)) return false;
            return left.text < right.text;
        });
    } else {
        // "display" and legacy/unknown sort keys fall back to Name (A -> Z).
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return sortValueForIndex(a).text < sortValueForIndex(b).text;
        });
    }

    return idx;
}

void MainWindow::setLibraryDisplayMode(LibraryDisplayMode mode,
                                       bool persist) {
    m_libraryDisplayMode = mode;

    if (m_iconView) {
        const int iconPixels = libraryDisplayModeIconPixels(mode);
        m_iconView->setIconSize(QSize(iconPixels, iconPixels));
        if (m_libraryTileDelegate)
            m_libraryTileDelegate->setDisplayMode(mode);
        if (mode != LibraryDisplayMode::List) {
            const bool expandedMvsGrid =
                mode == LibraryDisplayMode::Grid &&
                m_librarySystem == "neogeo";
            m_iconView->setWordWrap(true);
            m_iconView->setTextElideMode(
                expandedMvsGrid ? Qt::ElideNone : Qt::ElideRight);
            m_iconView->setUniformItemSizes(!expandedMvsGrid);
            updateIconViewGridSize();
        }
    }

    if (m_libraryViewStack && m_tree && m_iconView) {
        if (mode == LibraryDisplayMode::List)
            m_libraryViewStack->setCurrentWidget(m_tree);
        else
            m_libraryViewStack->setCurrentWidget(m_iconView);
    }

    refreshIconView();
    if (persist) {
        m_config.set("UI", "library_view",
                     std::string(libraryDisplayModeKey(mode)));
        save_config(m_config);
    }
}

void MainWindow::updateIconViewGridSize() {
    if (!m_iconView || m_libraryDisplayMode == LibraryDisplayMode::List) {
        return;
    }

    const LibraryTileMetrics metrics = libraryTileMetrics(
        m_libraryDisplayMode);
    const int cellWidth = libraryResponsiveGridCellWidth(
        m_iconView->viewport()->width(), metrics.width,
        m_iconView->spacing());
    const QSize targetSize(cellWidth, metrics.height);
    if (m_iconView->gridSize() != targetSize) {
        m_iconView->setGridSize(targetSize);
    }
}

void MainWindow::refreshIconView() {
    if (!m_iconView || !m_tree) return;

    const QSignalBlocker blocker(m_iconView);
    m_iconView->clear();
    if (m_libraryDisplayMode == LibraryDisplayMode::List) return;

    const bool expandedMvsGrid =
        m_libraryDisplayMode == LibraryDisplayMode::Grid &&
        m_librarySystem == "neogeo";
    m_iconView->setTextElideMode(
        expandedMvsGrid ? Qt::ElideNone : Qt::ElideRight);
    m_iconView->setUniformItemSizes(!expandedMvsGrid);
    updateIconViewGridSize();

    struct IconTile {
        QTreeWidgetItem* treeItem = nullptr;
        QIcon icon;
        QString title;
        QString tooltip;
        QString sortTitle;
        std::int64_t numericSortValue = 0;
    };
    std::vector<IconTile> tiles;

    const bool yearSort = m_sortKey == "year" ||
                          m_sortKey == "year_desc";
    const bool ratingSort = m_sortKey == "rating" ||
                            m_sortKey == "rating_desc";
    const bool playtimeSort = m_sortKey == "playtime" ||
                              m_sortKey == "playtime_desc";
    const bool metricSort = ratingSort || playtimeSort;

    const auto collectTile = [this, &tiles, yearSort, ratingSort,
                              playtimeSort](QTreeWidgetItem* treeItem,
                                            const QIcon& icon) {
        if (!treeItem) return;
        const int gameIndex = treeItem->data(0, GameIndexRole).toInt();
        const int romIndex = treeItem->data(0, RomIndexRole).toInt();
        if (gameIndex < 0) return;
        const std::size_t gameValueIndex =
            static_cast<std::size_t>(gameIndex);
        if (gameValueIndex >= m_games.size()) return;

        const Game& game = m_games[gameValueIndex];
        const Rom* rom = nullptr;
        if (romIndex >= 0) {
            const std::size_t romValueIndex =
                static_cast<std::size_t>(romIndex);
            if (romValueIndex < game.roms.size())
                rom = &game.roms[romValueIndex];
        }
        QString title = rom
            ? QString::fromStdString(libraryTileTitle(game, rom))
            : treeItem->text(0);
        const QString sortTitle = title.toCaseFolded();

        if (rom) {
            if (m_gameProfiles.find(game.system, rom->file))
                title += QString::fromUtf8(" \xE2\x9A\x99");
            if (hasCommands(game, romIndex))
                title += QString::fromUtf8(" \xF0\x9F\x91\x8A");
        }

        QString tooltip = title;
        if (rom && !rom->mame.empty()) {
            tooltip += QStringLiteral("\nMAME ID: ") +
                QString::fromStdString(rom->mame);
        }

        std::int64_t numericSortValue = 0;
        if (yearSort) {
            numericSortValue = libraryYearSortValue(game);
        } else if (ratingSort || playtimeSort) {
            const auto media = selected_launch_media(game, romIndex);
            if (media.has_value()) {
                numericSortValue = ratingSort
                    ? static_cast<std::int64_t>(
                          m_gameLibraryState.rating(game.system, *media))
                    : mediaPlaytimeSeconds(game, *media);
            }
        }

        tiles.push_back({
            treeItem, icon, title, tooltip, sortTitle, numericSortValue,
        });
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        if (!parent || parent->isHidden()) continue;

        // Filter-only container parents exist so a matching variant can be
        // reached in List mode; they are not themselves results/tiles.
        if (parent->data(0, ExactMatchRole).toBool()) {
            collectTile(parent, parent->icon(0));
        }
        for (int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem* child = parent->child(j);
            if (!child || child->isHidden() ||
                !child->data(0, ExactMatchRole).toBool()) {
                continue;
            }
            collectTile(child, parent->icon(0));
        }
    }

    const auto titlePrecedes = [](const IconTile& left,
                                  const IconTile& right) {
        const int comparison = QString::compare(
            left.sortTitle, right.sortTitle, Qt::CaseSensitive);
        if (comparison != 0) return comparison < 0;
        const int leftGame = left.treeItem->data(0, GameIndexRole).toInt();
        const int rightGame = right.treeItem->data(0, GameIndexRole).toInt();
        if (leftGame != rightGame) return leftGame < rightGame;
        return left.treeItem->data(0, RomIndexRole).toInt() <
               right.treeItem->data(0, RomIndexRole).toInt();
    };
    std::stable_sort(tiles.begin(), tiles.end(),
                     [&](const IconTile& left, const IconTile& right) {
        if (m_sortKey == "display_desc")
            return titlePrecedes(right, left);
        if (m_sortKey == "year" || m_sortKey == "year_desc") {
            if (left.numericSortValue != right.numericSortValue) {
                return m_sortKey == "year_desc"
                    ? left.numericSortValue > right.numericSortValue
                    : left.numericSortValue < right.numericSortValue;
            }
            return titlePrecedes(left, right);
        }
        if (metricSort) {
            const bool descending = m_sortKey.endsWith("_desc");
            if (libraryMetricPrecedes(left.numericSortValue,
                                      right.numericSortValue,
                                      descending)) {
                return true;
            }
            if (libraryMetricPrecedes(right.numericSortValue,
                                      left.numericSortValue,
                                      descending)) {
                return false;
            }
        }
        return titlePrecedes(left, right);
    });

    for (const IconTile& tile : tiles) {
        auto* item = new QListWidgetItem(
            tile.icon, tile.title, m_iconView);
        item->setData(GameIndexRole,
                      tile.treeItem->data(0, GameIndexRole));
        item->setData(RomIndexRole,
                      tile.treeItem->data(0, RomIndexRole));
        item->setData(Qt::AccessibleTextRole, tile.title);
        item->setToolTip(tile.tooltip);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    }
    syncIconSelectionFromTree();
    QTimer::singleShot(0, m_iconView, [this]() {
        updateIconViewGridSize();
    });
}

void MainWindow::syncIconSelectionFromTree() {
    if (!m_iconView || !m_tree) return;
    const QSignalBlocker blocker(m_iconView);
    QTreeWidgetItem* current = m_tree->currentItem();
    if (!current) {
        m_iconView->setCurrentItem(nullptr);
        m_iconView->clearSelection();
        return;
    }

    const int gameIndex = current->data(0, GameIndexRole).toInt();
    const int romIndex = current->data(0, RomIndexRole).toInt();
    for (int i = 0; i < m_iconView->count(); ++i) {
        QListWidgetItem* item = m_iconView->item(i);
        if (item && item->data(GameIndexRole).toInt() == gameIndex &&
            item->data(RomIndexRole).toInt() == romIndex) {
            m_iconView->setCurrentItem(item);
            m_iconView->scrollToItem(item);
            return;
        }
    }
    m_iconView->setCurrentItem(nullptr);
    m_iconView->clearSelection();
}

} // namespace goliath
