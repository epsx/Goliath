#include "ui/main_window.hpp"

#include "game/jollygood_launch.hpp"
#include "ui/library_details_presentation.hpp"
#include "ui/library_item_roles.hpp"
#include "ui/library_view_logic.hpp"

#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QString>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace goliath {

void MainWindow::populateTree() {
    refreshCommandCatalog();
    // Column 1 exists only for Neo Geo CD verification badges.  Hiding it for
    // MVS/AES keeps the cartridge library visually single-column instead of
    // leaving an empty CD badge section/seam in every row.
    m_tree->setColumnHidden(1, m_librarySystem != "neogeocd");

    m_tree->clear();
    const std::vector<std::string> listTitles =
        libraryGameListTitles(m_games);
    for (int idx : sortedGameOrder(listTitles)) {
        if (idx < 0) continue;
        const std::size_t gameIndex = static_cast<std::size_t>(idx);
        if (gameIndex >= m_games.size() ||
            gameIndex >= listTitles.size()) continue;
        const Game& game = m_games[gameIndex];
        if (game.system != m_librarySystem) continue;

        auto* parentItem = new QTreeWidgetItem(m_tree);
        QString parentText = QString::fromStdString(listTitles[gameIndex]);
        const auto parentMedia = selected_launch_media(game, -1);
        const bool parentFavorite = parentMedia.has_value() &&
            m_gameLibraryState.is_favorite(game.system, *parentMedia);
        const int parentRating = parentMedia.has_value()
            ? m_gameLibraryState.rating(game.system, *parentMedia) : 0;
        if (game.main_rom.has_value() &&
            m_gameProfiles.find(game.system, *game.main_rom)) {
            parentText += QString::fromUtf8(" \xE2\x9A\x99");
        }
        if (hasCommands(game, -1)) {
            parentText += QString::fromUtf8(" \xF0\x9F\x91\x8A"); // 👊
        }
        parentItem->setText(0, parentText);
        parentItem->setData(0, GameIndexRole, idx);
        parentItem->setData(0, RomIndexRole, -1);
        parentItem->setData(0, ExactMatchRole, true);
        parentItem->setData(0, LibraryFavoriteRole, parentFavorite);
        parentItem->setData(
            0, LibraryRatingRole, libraryListRatingBadge(parentRating));

        const LibraryCdVerificationBadge badge =
            libraryCdVerificationBadge(game);
        if (!badge.text.isEmpty()) {
            parentItem->setText(1, badge.text);
            parentItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        }

        QIcon gameIcon;
        if (game.icon.has_value() && fs::exists(*game.icon)) {
            QPixmap pixmap(QString::fromStdString(*game.icon));
            if (!pixmap.isNull()) {
                gameIcon = QIcon(pixmap);
            }
        }
        if (gameIcon.isNull()) {
            gameIcon = QIcon(QStringLiteral(":/icons/goliath-qt.ico"));
        }
        parentItem->setIcon(0, gameIcon);

        for (int romIdx : sortedVariantOrder(game)) {
            if (romIdx < 0) continue;
            const std::size_t variantIndex =
                static_cast<std::size_t>(romIdx);
            if (variantIndex >= game.roms.size()) continue;
            const Rom& rom = game.roms[variantIndex];
            auto* childItem = new QTreeWidgetItem(parentItem);
            QString childText = QString::fromStdString(
                rom.mame.empty() ? rom.file : rom.mame);
            const bool childFavorite =
                m_gameLibraryState.is_favorite(game.system, rom.file);
            const int childRating =
                m_gameLibraryState.rating(game.system, rom.file);
            if (m_gameProfiles.find(game.system, rom.file)) {
                childText += QString::fromUtf8(" \xE2\x9A\x99");
            }
            if (hasCommands(game, romIdx)) {
                childText += QString::fromUtf8(" \xF0\x9F\x91\x8A"); // 👊
            }
            childItem->setText(0, childText);
            childItem->setData(0, GameIndexRole, idx);
            childItem->setData(0, RomIndexRole, romIdx);
            childItem->setData(0, ExactMatchRole, true);
            childItem->setData(0, LibraryFavoriteRole, childFavorite);
            childItem->setData(
                0, LibraryRatingRole, libraryListRatingBadge(childRating));
            const std::string variantTooltip =
                libraryListVariantTooltip(game, rom);
            if (!variantTooltip.empty()) {
                childItem->setToolTip(
                    0, QString::fromStdString(variantTooltip));
            }
            childItem->setHidden(!m_showVariants);
        }

        parentItem->setExpanded(false);
    }

    if (m_tree->topLevelItemCount() == 0) {
        clearDetailsForNoSelection(false);
    }

    refreshLibraryListRowBackgrounds();
    updateStatus();
}

void MainWindow::refreshLibraryListRowBackgrounds() {
    if (!m_tree) return;

    int visibleRow = 0;
    const auto applyBackground = [&visibleRow](QTreeWidgetItem* item) {
        const bool alternate =
            libraryListVisibleRowUsesAlternateBackground(visibleRow);
        item->setData(
            0, LibraryAlternateBackgroundRole, alternate);
        item->setData(
            1, LibraryAlternateBackgroundRole, alternate);
        ++visibleRow;
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        if (!parent || parent->isHidden()) continue;

        applyBackground(parent);
        if (!parent->isExpanded()) continue;

        for (int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem* child = parent->child(j);
            if (!child || child->isHidden()) continue;
            applyBackground(child);
        }
    }
}

void MainWindow::refreshLibraryView(
        bool preserveSelection, const QString& preferredRomOverride) {
    const QString preferredRom = !preferredRomOverride.isEmpty()
        ? preferredRomOverride
        : (preserveSelection ? selectedRomFile() : QString());
    std::vector<int> expandedGameIndexes;
    if (preserveSelection && m_tree) {
        expandedGameIndexes.reserve(
            static_cast<std::size_t>(m_tree->topLevelItemCount()));
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* parent = m_tree->topLevelItem(i);
            if (parent && libraryExpansionShouldBePreserved(
                    parent->isExpanded(),
                    parent->data(0, FilterAutoExpandedRole).toBool())) {
                expandedGameIndexes.push_back(
                    parent->data(0, GameIndexRole).toInt());
            }
        }
    }

    populateTree();

    const bool filtered =
        (m_searchEntry && !m_searchEntry->text().isEmpty()) ||
        m_favoritesOnly ||
        libraryPersonalFiltersActive(m_ratingFilter, m_playtimeFilter) ||
        !m_genreFilter.empty();
    m_rebuildingLibraryView = true;
    if (filtered) {
        filterGames(m_searchEntry->text());
    }

    restoreSelection(preferredRom);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        if (!parent) continue;
        const int gameIdx = parent->data(0, GameIndexRole).toInt();
        if (std::find(expandedGameIndexes.begin(), expandedGameIndexes.end(),
                      gameIdx) != expandedGameIndexes.end()) {
            parent->setExpanded(true);
            parent->setData(0, FilterAutoExpandedRole, false);
        }
    }
    m_rebuildingLibraryView = false;
    ensureVisibleSelection(filtered);
    refreshIconView();
}

void MainWindow::setLibrarySystem(const std::string& system) {
    if (system != "neogeo" && system != "neogeocd") return;
    if (!librarySystemChangeRequired(m_librarySystem, system)) return;

    rememberCurrentLibrarySelection();
    rememberCurrentLibraryListViewport();
    m_librarySystem = system;
    if (m_mvsAesButton) m_mvsAesButton->setChecked(system == "neogeo");
    if (m_cdButton) m_cdButton->setChecked(system == "neogeocd");

    m_config.set("UI", "library_system", system);
    save_config(m_config);

    rebuildGenreFilterMenu();
    refreshLibraryView(false);
    restoreCurrentLibraryListViewport();
}

void MainWindow::filterGames(const QString& text) {
    QString needle = text.toLower().trimmed();
    const bool personalFiltersActive = libraryPersonalFiltersActive(
        m_ratingFilter, m_playtimeFilter);
    const bool genreFilterActive = !m_genreFilter.empty();

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        int gameIdx = parent->data(0, GameIndexRole).toInt();
        if (gameIdx < 0) continue;
        const std::size_t gameIndex =
            static_cast<std::size_t>(gameIdx);
        if (gameIndex >= m_games.size()) continue;
        const Game& game = m_games[gameIndex];
        const bool genreMatch =
            libraryGenreFilterAllows(m_genreFilter, game.genre);

        QString haystack = QString("%1 %2 %3 %4")
                                .arg(QString::fromStdString(game.display))
                                .arg(QString::fromStdString(game.short_name))
                                .arg(QString::fromStdString(game.year.value_or("")))
                                .arg(QString::fromStdString(game.manufacturer.value_or("")))
                                .toLower();
        bool gameMatch = haystack.contains(needle);

        const auto parentMedia = selected_launch_media(game, -1);
        const bool parentFavorite = parentMedia.has_value() &&
            m_gameLibraryState.is_favorite(game.system, *parentMedia);
        const bool parentFilterMatch = parentMedia.has_value() &&
            mediaMatchesPersonalFilters(game, *parentMedia);
        const bool parentOwnMatch = gameMatch && genreMatch &&
            parentFilterMatch &&
            (!m_favoritesOnly || parentFavorite);
        parent->setData(0, ExactMatchRole, parentOwnMatch);

        bool visibleMatchingVariant = false;
        for (int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem* child = parent->child(j);
            int romIdx = child->data(0, RomIndexRole).toInt();
            if (romIdx < 0) continue;
            const std::size_t variantIndex =
                static_cast<std::size_t>(romIdx);
            if (variantIndex >= game.roms.size()) continue;
            const Rom& rom = game.roms[variantIndex];
            QString romHay = QString("%1 %2 %3 %4 %5")
                                  .arg(QString::fromStdString(rom.mame))
                                  .arg(QString::fromStdString(rom.name.value_or("")))
                                  .arg(QString::fromStdString(rom.label.value_or("")))
                                  .arg(QString::fromStdString(rom.file))
                                  .arg(libraryUtf8Text(rom.alt_title))
                                  .toLower();
            const bool textMatch = romHay.contains(needle);
            const bool childFavorite =
                m_gameLibraryState.is_favorite(game.system, rom.file);
            const bool childFilterMatch =
                mediaMatchesPersonalFilters(game, rom.file);
            const bool searchAllowsChild = gameMatch || textMatch;
            const bool variantsAllowed = libraryVariantAllowed(
                m_showVariants, m_favoritesOnly, childFavorite,
                personalFiltersActive, childFilterMatch);
            const bool childVisible = variantsAllowed && searchAllowsChild &&
                genreMatch &&
                childFilterMatch && (!m_favoritesOnly || childFavorite);

            child->setHidden(!childVisible);
            child->setData(0, ExactMatchRole, childVisible);
            visibleMatchingVariant |= childVisible;
        }

        parent->setHidden(!(parentOwnMatch || visibleMatchingVariant));

        // A parent that does not itself match remains the necessary container
        // for an exact matching variant. Reveal that result immediately.
        if (!parentOwnMatch && visibleMatchingVariant &&
            (m_favoritesOnly || personalFiltersActive || genreFilterActive)) {
            parent->setData(0, FilterAutoExpandedRole, true);
            parent->setExpanded(true);
        }
    }
    refreshLibraryListRowBackgrounds();
    if (!m_rebuildingLibraryView) {
        ensureVisibleSelection(!needle.isEmpty() || m_favoritesOnly ||
                               personalFiltersActive || genreFilterActive);
        refreshIconView();
    }
    updateStatus();
}

QString MainWindow::selectedRomFile() const {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return {};

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0) return {};
    const std::size_t gameIndex = static_cast<std::size_t>(gameIdx);
    if (gameIndex >= m_games.size()) return {};

    const auto media = selected_launch_media(m_games[gameIndex], romIdx);
    return media.has_value() ? QString::fromStdString(*media) : QString();
}

void MainWindow::rememberCurrentLibrarySelection() {
    const QString lastRom = selectedRomFile();
    if (lastRom.isEmpty()) return;

    const std::string value = lastRom.toStdString();
    m_config.set(
        "UI", std::string(librarySelectionConfigKey(m_librarySystem)), value);
    // Keep the legacy key current for compatibility with older builds.
    m_config.set("UI", "last_rom", value);
}

void MainWindow::rememberCurrentLibraryListViewport() {
    if (!m_tree || m_libraryDisplayMode != LibraryDisplayMode::List) return;

    m_libraryListViewportMemory.remember(
        m_librarySystem, m_tree->verticalScrollBar()->value());
}

void MainWindow::restoreCurrentLibraryListViewport() {
    if (!m_tree || m_libraryDisplayMode != LibraryDisplayMode::List) return;

    const auto remembered =
        m_libraryListViewportMemory.recall(m_librarySystem);
    if (!remembered.has_value()) return;

    const std::string system = m_librarySystem;
    const int verticalPosition = *remembered;
    const auto restore = [this, system, verticalPosition]() {
        if (m_librarySystem != system || !m_tree ||
            m_libraryDisplayMode != LibraryDisplayMode::List) return;

        m_tree->doItemsLayout();
        QScrollBar* scroll = m_tree->verticalScrollBar();
        scroll->setValue(std::clamp(
            verticalPosition, scroll->minimum(), scroll->maximum()));
    };

    restore();
    QTimer::singleShot(0, this, restore);
}

void MainWindow::updateStatus() {
    const LibraryMediaCounts mediaCounts =
        libraryMediaCounts(m_games, m_librarySystem);
    std::size_t homebrew = 0;
    std::size_t identified = 0;
    std::size_t verifiedSets = 0;
    std::size_t cueMismatch = 0;
    std::size_t chdMismatch = 0;
    std::size_t metadataOnly = 0;
    std::size_t unknown = 0;
    std::size_t favorites = 0;
    for (const Game& g : m_games) {
        if (g.system != m_librarySystem) continue;
        if (g.source == "homebrew") homebrew++;

        const auto parentMedia = selected_launch_media(g, -1);
        if (parentMedia.has_value() &&
            m_gameLibraryState.is_favorite(g.system, *parentMedia)) {
            ++favorites;
        }
        for (const Rom& rom : g.roms) {
            if (!rom.main &&
                m_gameLibraryState.is_favorite(g.system, rom.file)) {
                ++favorites;
            }
        }

        if (g.system == "neogeocd") {
            if (g.identified) identified++;
            else unknown++;

            if (g.verification == std::optional<std::string>("redump-cue") ||
                g.verification == std::optional<std::string>("mame-chd")) {
                verifiedSets++;
            } else if (g.verification == std::optional<std::string>("redump-tracks-only")) {
                cueMismatch++;
            } else if (g.verification ==
                       std::optional<std::string>("mame-chd-mismatch")) {
                chdMismatch++;
            } else if (g.identified) {
                metadataOnly++;
            }
        }
    }
    QString message;
    if (m_librarySystem == "neogeocd") {
        message = QString(
                      "Neo Geo CD - Games: %1 - Identified: %2 - Verified sets: %3 - "
                      "CUE mismatch: %4 - CHD mismatch: %5 - Metadata only: %6 - "
                      "Unknown: %7")
                      .arg(mediaCounts.groups)
                      .arg(identified)
                      .arg(verifiedSets)
                      .arg(cueMismatch)
                      .arg(chdMismatch)
                      .arg(metadataOnly)
                      .arg(unknown);
    } else {
        message = QString("Neo Geo MVS/AES - ROMs: %1 - Parents: %2 - Variants: %3 - Homebrew: %4")
                      .arg(mediaCounts.media)
                      .arg(mediaCounts.parents)
                      .arg(mediaCounts.variants)
                      .arg(homebrew);
    }

    message += QString(" - Favorites: %1").arg(
        static_cast<qulonglong>(favorites));

    int count = m_tree->topLevelItemCount();
    int visible = 0;
    for (int i = 0; i < count; ++i) if (!m_tree->topLevelItem(i)->isHidden()) visible++;
    if (visible != count) {
        message += QString("  (showing %1 of %2)").arg(visible).arg(count);
    }
    m_libraryStatusLabel->setText(message);
}

void MainWindow::restoreSelection(const QString& preferredRom) {
    const std::string selectionKey(
        librarySelectionConfigKey(m_librarySystem));
    const std::string targetRom = libraryRememberedSelection(
        preferredRom.toStdString(),
        m_config.get("UI", selectionKey, ""),
        m_config.get("UI", "last_rom", ""));

    if (!targetRom.empty() && !m_games.empty()) {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* parent = m_tree->topLevelItem(i);
            if (!parent || parent->isHidden()) continue;

            const int gameIdx = parent->data(0, GameIndexRole).toInt();
            if (gameIdx < 0) continue;
            const std::size_t gameIndex =
                static_cast<std::size_t>(gameIdx);
            if (gameIndex >= m_games.size()) continue;

            const Game& game = m_games[gameIndex];
            for (const Rom& rom : game.roms) {
                if (rom.main && rom.file == targetRom) {
                    m_tree->setCurrentItem(parent);
                    parent->setExpanded(false);
                    return;
                }
            }

            for (int j = 0; j < parent->childCount(); ++j) {
                QTreeWidgetItem* child = parent->child(j);
                const int romIdx = child->data(0, RomIndexRole).toInt();
                if (romIdx < 0) continue;
                const std::size_t variantIndex =
                    static_cast<std::size_t>(romIdx);
                if (variantIndex >= game.roms.size()) continue;
                if (game.roms[variantIndex].file != targetRom) continue;

                if (!child->isHidden()) {
                    m_tree->setCurrentItem(child);
                    parent->setExpanded(true);
                } else {
                    m_tree->setCurrentItem(parent);
                    parent->setExpanded(false);
                }
                return;
            }
        }
    }

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        if (item && !item->isHidden()) {
            m_tree->setCurrentItem(item);
            return;
        }
    }
}

} // namespace goliath
