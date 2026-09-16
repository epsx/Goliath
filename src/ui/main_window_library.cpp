#include "ui/main_window.hpp"

#include "game/jollygood_benchmark.hpp"
#include "game/jollygood_bios.hpp"
#include "game/jollygood_launch.hpp"
#include "game/game_profile_runtime.hpp"
#include "ui/audio_export_dialog.hpp"
#include "ui/benchmark_dialog.hpp"
#include "ui/game_profile_dialog.hpp"
#include "ui/library_view_logic.hpp"
#include "ui/save_data_dialog.hpp"

#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>
#include <QStringList>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <random>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {
constexpr int GameIndexRole = Qt::UserRole;
constexpr int RomIndexRole = Qt::UserRole + 1;
constexpr int ExactMatchRole = Qt::UserRole + 2;
constexpr int FilterAutoExpandedRole = Qt::UserRole + 3;

std::string lowercaseAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return text;
}

QString variantFullName(const Rom& rom) {
    if (rom.name.has_value() && !rom.name->empty())
        return QString::fromStdString(*rom.name);
    if (rom.label.has_value() && !rom.label->empty())
        return QString::fromStdString(*rom.label);
    if (!rom.mame.empty())
        return QString::fromStdString(rom.mame);
    return QString::fromStdString(rom.file);
}

bool treeItemIsEffectivelyVisible(const QTreeWidgetItem* item) {
    for (const QTreeWidgetItem* current = item; current;
         current = current->parent()) {
        if (current->isHidden()) return false;
        if (current->parent() && !current->parent()->isExpanded()) return false;
    }
    return item != nullptr;
}

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

struct CdVerificationBadge {
    QString text;
    QString tooltip;
};

CdVerificationBadge cdVerificationBadge(const Game& game) {
    if (game.system != "neogeocd")
        return {};

    if (game.verification == std::optional<std::string>("redump-cue")) {
        return {
            QString::fromUtf8("\xE2\x9C\x93 Redump set"),
            "The CUE and all BIN files match the local Redump Neo Geo CD DAT "
            "by byte size and SHA-1."
        };
    }

    if (game.verification == std::optional<std::string>("redump-tracks-only")) {
        return {
            QString::fromUtf8("\xE2\x9A\xA0 CUE mismatch"),
            "All BIN files match the local Redump Neo Geo CD DAT by size and "
            "SHA-1, but the CUE file does not match its Redump size/SHA-1. "
            "The CUE may be renamed, reformatted, or modified. The image may "
            "still launch, but it is not a complete byte-perfect Redump set."
        };
    }

    if (game.verification == std::optional<std::string>("mame-chd")) {
        return {
            QString::fromUtf8("\xE2\x9C\x93 MAME set"),
            "The CHD internal combined SHA-1 matches the local MAME "
            "neocd.xml entry."
        };
    }

    if (!game.identified) {
        return {
            "? Unknown",
            "No trusted Neo Geo CD identity is available for this image."
        };
    }

    return {};
}

QString cdVerificationDetailsText(const Game& game) {
    if (game.system != "neogeocd")
        return {};

    const CdVerificationBadge badge = cdVerificationBadge(game);
    if (!badge.text.isEmpty())
        return QStringLiteral("Verification: ") + badge.text;

    if (game.identified)
        return QStringLiteral("Verification: Metadata only (content not verified)");

    return QStringLiteral("Verification: ? Unknown");
}

QString cdVerificationDetailsTooltip(const Game& game) {
    if (game.system != "neogeocd")
        return {};

    const CdVerificationBadge badge = cdVerificationBadge(game);
    if (!badge.tooltip.isEmpty())
        return badge.tooltip;

    if (game.identified)
        return QStringLiteral(
            "The game was identified from Neo Geo CD metadata/title matching, "
            "but the image has not passed complete Redump or MAME set verification.");

    return QStringLiteral("No trusted Neo Geo CD identity is available for this image.");
}

QString benchmarkPreparationError(
        const JollygoodLaunchPreparation& preparation,
        const AppPaths& paths,
        const QString& media) {
    switch (preparation.status) {
    case LaunchPreparationStatus::Ready:
        return {};
    case LaunchPreparationStatus::ExecutableNotFound:
        return QString("Jollygood executable not found:\n%1")
            .arg(QString::fromStdString(paths.jollygood_exe.string()));
    case LaunchPreparationStatus::CapabilityProbeFailed:
        return QString("Goliath could not verify the installed Geolith core's "
                       "Neo Geo CD capabilities.\n\n%1")
            .arg(QString::fromStdString(preparation.detail));
    case LaunchPreparationStatus::ChdUnsupported:
        return QString("The installed Geolith core does not advertise CHD "
                       "support for Neo Geo CD.\n\nAdvertised formats: %1")
            .arg(preparation.advertised_formats.empty()
                     ? QString("None")
                     : QString::fromStdString(preparation.advertised_formats));
    case LaunchPreparationStatus::UnsupportedMedia:
        return QString("Unsupported Neo Geo CD image type: %1").arg(media);
    case LaunchPreparationStatus::ProfileConfigurationFailed:
        return QString("Goliath could not prepare the isolated configuration "
                       "for this game.\n\n%1\n\nThe global JGRF configuration "
                       "was not modified.")
            .arg(QString::fromStdString(preparation.detail));
    case LaunchPreparationStatus::WaveOutputConflict:
    case LaunchPreparationStatus::WaveOutputInvalid:
        return QString("Audio WAV export preparation failed.\n\n%1")
            .arg(QString::fromStdString(preparation.detail));
    }
    return "Unknown benchmark preparation error.";
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
    for (int index = 0; index < static_cast<int>(game.roms.size()); ++index) {
        if (!game.roms[index].main) order.push_back(index);
    }

    const bool ratingSort = m_sortKey == "rating" ||
                            m_sortKey == "rating_desc";
    const bool playtimeSort = m_sortKey == "playtime" ||
                              m_sortKey == "playtime_desc";
    if (!ratingSort && !playtimeSort) return order;

    const bool descending = m_sortKey.endsWith("_desc");
    std::stable_sort(order.begin(), order.end(), [&](int left, int right) {
        const Rom& leftRom = game.roms[left];
        const Rom& rightRom = game.roms[right];
        const std::int64_t leftMetric = ratingSort
            ? static_cast<std::int64_t>(
                  m_gameLibraryState.rating(game.system, leftRom.file))
            : mediaPlaytimeSeconds(game, leftRom.file);
        const std::int64_t rightMetric = ratingSort
            ? static_cast<std::int64_t>(
                  m_gameLibraryState.rating(game.system, rightRom.file))
            : mediaPlaytimeSeconds(game, rightRom.file);

        if (libraryMetricPrecedes(leftMetric, rightMetric, descending)) {
            return true;
        }
        if (libraryMetricPrecedes(rightMetric, leftMetric, descending)) {
            return false;
        }
        const std::string& leftName = leftRom.mame.empty()
            ? leftRom.file : leftRom.mame;
        const std::string& rightName = rightRom.mame.empty()
            ? rightRom.file : rightRom.mame;
        return lowercaseAscii(leftName) < lowercaseAscii(rightName);
    });
    return order;
}

std::vector<int> MainWindow::sortedGameOrder() const {
    std::vector<int> idx(m_games.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);

    auto yearValue = [&](const Game& game) -> int {
        if (!game.year.has_value() || game.year->empty()) return 0;
        try {
            return std::stoi(*game.year);
        } catch (...) {
            return 0;
        }
    };

    if (m_sortKey == "display_desc") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return lowercaseAscii(m_games[a].display) >
                   lowercaseAscii(m_games[b].display);
        });
    } else if (m_sortKey == "year_desc") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return yearValue(m_games[a]) > yearValue(m_games[b]);
        });
    } else if (m_sortKey == "year") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return yearValue(m_games[a]) < yearValue(m_games[b]);
        });
    } else if (m_sortKey == "rating" || m_sortKey == "rating_desc" ||
               m_sortKey == "playtime" || m_sortKey == "playtime_desc") {
        const bool ratingMetric = m_sortKey.startsWith("rating");
        const bool descending = m_sortKey.endsWith("_desc");
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            const std::int64_t left = gameSortMetric(
                m_games[a], ratingMetric);
            const std::int64_t right = gameSortMetric(
                m_games[b], ratingMetric);
            if (libraryMetricPrecedes(left, right, descending)) return true;
            if (libraryMetricPrecedes(right, left, descending)) return false;
            return lowercaseAscii(m_games[a].display) <
                   lowercaseAscii(m_games[b].display);
        });
    } else {
        // "display" and legacy/unknown sort keys fall back to Name (A -> Z).
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return lowercaseAscii(m_games[a].display) <
                   lowercaseAscii(m_games[b].display);
        });
    }

    return idx;
}

void MainWindow::populateTree() {
    // Column 1 exists only for Neo Geo CD verification badges.  Hiding it for
    // MVS/AES keeps the cartridge library visually single-column instead of
    // leaving an empty CD badge section/seam in every row.
    m_tree->setColumnHidden(1, m_librarySystem != "neogeocd");

    m_tree->clear();
    for (int idx : sortedGameOrder()) {
        const Game& game = m_games[idx];
        if (game.system != m_librarySystem) continue;

        auto* parentItem = new QTreeWidgetItem(m_tree);
        QString parentText = QString::fromStdString(game.display);
        QString parentTooltip;
        const auto parentMedia = selected_launch_media(game, -1);
        const bool parentFavorite = parentMedia.has_value() &&
            m_gameLibraryState.is_favorite(game.system, *parentMedia);
        if (parentFavorite) {
            parentText.prepend(QString::fromUtf8("\xE2\x98\x85 ")); // ★
            parentTooltip = "This exact media item is in Favorites.";
        }
        if (game.main_rom.has_value() &&
            m_gameProfiles.find(game.system, *game.main_rom)) {
            parentText += QString::fromUtf8(" \xE2\x9A\x99");
            if (!parentTooltip.isEmpty()) parentTooltip += "\n\n";
            parentTooltip +=
                "A per-game launch profile is active for this media. "
                "Right-click and choose Game settings to inspect or reset it.";
        }
        parentItem->setText(0, parentText);
        if (!parentTooltip.isEmpty()) parentItem->setToolTip(0, parentTooltip);
        parentItem->setData(0, GameIndexRole, idx);
        parentItem->setData(0, RomIndexRole, -1);
        parentItem->setData(0, ExactMatchRole, true);

        const CdVerificationBadge badge = cdVerificationBadge(game);
        if (!badge.text.isEmpty()) {
            parentItem->setText(1, badge.text);
            parentItem->setToolTip(1, badge.tooltip);
            parentItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        }

        if (game.icon.has_value() && fs::exists(*game.icon)) {
            QPixmap pixmap(QString::fromStdString(*game.icon));
            if (!pixmap.isNull()) {
                pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                parentItem->setIcon(0, QIcon(pixmap));
            }
        }

        for (int romIdx : sortedVariantOrder(game)) {
            const Rom& rom = game.roms[romIdx];
            auto* childItem = new QTreeWidgetItem(parentItem);
            QString childText = QString::fromStdString(
                rom.mame.empty() ? rom.file : rom.mame);
            QString childTooltip = variantFullName(rom);
            if (m_gameLibraryState.is_favorite(game.system, rom.file)) {
                childText.prepend(QString::fromUtf8("\xE2\x98\x85 ")); // ★
                if (!childTooltip.isEmpty()) childTooltip += "\n\n";
                childTooltip += "This exact variant is in Favorites.";
            }
            if (m_gameProfiles.find(game.system, rom.file)) {
                childText += QString::fromUtf8(" \xE2\x9A\x99");
                if (!childTooltip.isEmpty()) childTooltip += "\n\n";
                childTooltip +=
                    "A per-game launch profile is active for this variant. "
                    "Right-click and choose Game settings to inspect or reset it.";
            }
            childItem->setText(0, childText);
            childItem->setToolTip(0, childTooltip);
            childItem->setData(0, GameIndexRole, idx);
            childItem->setData(0, RomIndexRole, romIdx);
            childItem->setData(0, ExactMatchRole, true);
            childItem->setHidden(!m_showVariants);
        }

        parentItem->setExpanded(false);
    }

    if (m_tree->topLevelItemCount() == 0) {
        clearDetailsForNoSelection(false);
    }

    updateStatus();
}

void MainWindow::refreshLibraryView(
        bool preserveSelection, const QString& preferredRomOverride) {
    const QString preferredRom = !preferredRomOverride.isEmpty()
        ? preferredRomOverride
        : (preserveSelection ? selectedRomFile() : QString());
    std::vector<int> expandedGameIndexes;
    if (preserveSelection && m_tree) {
        expandedGameIndexes.reserve(m_tree->topLevelItemCount());
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
        libraryPersonalFiltersActive(m_ratingFilter, m_playtimeFilter);
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
}

void MainWindow::setLibrarySystem(const std::string& system) {
    if (system != "neogeo" && system != "neogeocd") return;

    m_librarySystem = system;
    if (m_mvsAesButton) m_mvsAesButton->setChecked(system == "neogeo");
    if (m_cdButton) m_cdButton->setChecked(system == "neogeocd");

    m_config.set("UI", "library_system", system);
    save_config(m_config);

    refreshLibraryView(false);
}

void MainWindow::setSelectionActionsEnabled(bool enabled) {
    for (QAction* action : m_selectionActions) {
        if (action) action->setEnabled(enabled);
    }
    if (m_launchButton) m_launchButton->setEnabled(enabled);
    if (m_favoriteButton) m_favoriteButton->setEnabled(enabled);
    for (QPushButton* button : m_ratingButtons) {
        if (button) button->setEnabled(enabled);
    }
}

void MainWindow::updateRatingButtons(int rating, bool enabled) {
    for (std::size_t index = 0; index < m_ratingButtons.size(); ++index) {
        QPushButton* button = m_ratingButtons[index];
        if (!button) continue;

        const int star = static_cast<int>(index) + 1;
        const bool filled = star <= rating;
        button->setEnabled(enabled);
        button->setText(filled
            ? QString::fromUtf8("\xE2\x98\x85") // ★
            : QString::fromUtf8("\xE2\x98\x86")); // ☆
        button->setProperty("rated", filled);
        button->setToolTip(!enabled
            ? "Select a parent, variant, CUE, or CHD to rate it"
            : (star == rating
                   ? QString("Clear the %1-star rating").arg(star)
                   : QString("Set rating to %1 star%2")
                         .arg(star)
                         .arg(star == 1 ? "" : "s")));
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void MainWindow::ensureVisibleSelection(bool filtered) {
    QTreeWidgetItem* current = m_tree->currentItem();
    const auto isExactMatch = [filtered](QTreeWidgetItem* item) {
        return !filtered || item->data(0, ExactMatchRole).toBool();
    };
    const auto isSelectable = [&isExactMatch, filtered](QTreeWidgetItem* item) {
        return item && librarySelectionAllowed(
            treeItemIsEffectivelyVisible(item), filtered,
            isExactMatch(item));
    };
    const auto firstSelectableInGroup = [&isExactMatch](
            QTreeWidgetItem* parent) -> QTreeWidgetItem* {
        if (!parent || parent->isHidden()) return nullptr;
        if (isExactMatch(parent)) return parent;

        for (int i = 0; i < parent->childCount(); ++i) {
            QTreeWidgetItem* child = parent->child(i);
            if (child && !child->isHidden() && isExactMatch(child)) {
                return child;
            }
        }
        return nullptr;
    };

    QTreeWidgetItem* currentGroup = current;
    while (currentGroup && currentGroup->parent()) {
        currentGroup = currentGroup->parent();
    }
    QTreeWidgetItem* replacement = firstSelectableInGroup(currentGroup);

    if (!replacement) {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            replacement = firstSelectableInGroup(m_tree->topLevelItem(i));
            if (replacement) break;
        }
    }

    const VisibleSelectionDecision decision = visibleSelectionDecision(
        isSelectable(current), replacement != nullptr);
    switch (decision) {
    case VisibleSelectionDecision::KeepCurrent:
        break;
    case VisibleSelectionDecision::SelectVisibleReplacement:
        if (replacement->parent()) replacement->parent()->setExpanded(true);
        m_tree->setCurrentItem(replacement);
        break;
    case VisibleSelectionDecision::ClearSelection:
        m_tree->setCurrentItem(nullptr);
        m_tree->clearSelection();
        clearDetailsForNoSelection(filtered);
        break;
    }

    if (visibleSelectionShouldBeRevealed(decision)) {
        if (QTreeWidgetItem* selected = m_tree->currentItem()) {
            m_tree->scrollToItem(selected);
        }
    }
}

void MainWindow::clearDetailsForNoSelection(bool filtered) {
    m_detailsSelectionKey.clear();
    setSelectionActionsEnabled(false);

    if (m_favoriteButton) {
        const QSignalBlocker blocker(m_favoriteButton);
        m_favoriteButton->setChecked(false);
        m_favoriteButton->setText(
            QString::fromUtf8("\xE2\x98\x86 Favorite")); // ☆
        m_favoriteButton->setToolTip(
            "Select a parent, variant, CUE, or CHD to add it to Favorites");
    }
    updateRatingButtons(0, false);

    const bool isCd = m_librarySystem == "neogeocd";
    m_detailsTitleLabel->setText(
        filtered ? "No matching games"
                 : (isCd ? "Neo Geo CD" : "Neo Geo MVS/AES"));
    m_variantLabel->clear();
    m_variantLabel->setToolTip(QString());
    for (auto& entry : m_infoLabels) {
        entry.second->setText("-");
        entry.second->setToolTip(QString());
    }
    m_historyText->setPlainText(filtered
        ? "No games match the current library filters."
        : (isCd
               ? "No Neo Geo CD games are currently in the database."
               : "No Neo Geo MVS/AES games are currently in the database."));
    loadSnapshotFor(QString());

    QTimer::singleShot(0, this, [this]() {
        if (!m_detailsSelectionKey.isEmpty()) return;
        if (m_detailsScroll) {
            m_detailsScroll->verticalScrollBar()->setValue(0);
            m_detailsScroll->horizontalScrollBar()->setValue(0);
        }
        if (m_historyText) {
            m_historyText->verticalScrollBar()->setValue(0);
            m_historyText->horizontalScrollBar()->setValue(0);
        }
    });
}

void MainWindow::updateSelection() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        setSelectionActionsEnabled(false);
        return;
    }

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) {
        setSelectionActionsEnabled(false);
        return;
    }

    const Game& game = m_games[gameIdx];
    const auto playtimeMedia = selected_launch_media(game, romIdx);
    const QString selectionKey =
        QString::fromStdString(game.system) + QChar(u'\x1f') +
        (playtimeMedia.has_value()
             ? QString::fromStdString(*playtimeMedia)
             : QString::fromStdString(game.short_name) + QChar(u'\x1f') +
                   QString::number(romIdx));
    const bool selectionChanged = selectionKey != m_detailsSelectionKey;
    const int detailsVertical = m_detailsScroll
        ? m_detailsScroll->verticalScrollBar()->value() : 0;
    const int detailsHorizontal = m_detailsScroll
        ? m_detailsScroll->horizontalScrollBar()->value() : 0;
    const int historyVertical = m_historyText
        ? m_historyText->verticalScrollBar()->value() : 0;
    const int historyHorizontal = m_historyText
        ? m_historyText->horizontalScrollBar()->value() : 0;

    m_detailsSelectionKey = selectionKey;
    setSelectionActionsEnabled(true);

    const bool favorite = playtimeMedia.has_value() &&
        m_gameLibraryState.is_favorite(game.system, *playtimeMedia);
    const int rating = playtimeMedia.has_value()
        ? m_gameLibraryState.rating(game.system, *playtimeMedia)
        : 0;
    if (m_favoriteButton) {
        const QSignalBlocker blocker(m_favoriteButton);
        m_favoriteButton->setEnabled(playtimeMedia.has_value());
        m_favoriteButton->setChecked(favorite);
        m_favoriteButton->setText(favorite
            ? QString::fromUtf8("\xE2\x98\x85 Favorite")
            : QString::fromUtf8("\xE2\x98\x86 Favorite"));
        m_favoriteButton->setToolTip(favorite
            ? "Remove the selected exact media item from Favorites"
            : "Add the selected exact media item to Favorites");
    }
    updateRatingButtons(rating, playtimeMedia.has_value());

    const auto finishDetailsUpdate =
        [this, selectionKey, selectionChanged,
         detailsVertical, detailsHorizontal,
         historyVertical, historyHorizontal]() {
            const int targetDetailsVertical = detailsScrollTarget(
                selectionChanged, detailsVertical);
            const int targetDetailsHorizontal = detailsScrollTarget(
                selectionChanged, detailsHorizontal);
            const int targetHistoryVertical = detailsScrollTarget(
                selectionChanged, historyVertical);
            const int targetHistoryHorizontal = detailsScrollTarget(
                selectionChanged, historyHorizontal);

            QTimer::singleShot(0, this,
                [this, selectionKey,
                 targetDetailsVertical, targetDetailsHorizontal,
                 targetHistoryVertical, targetHistoryHorizontal]() {
                    if (m_detailsSelectionKey != selectionKey) return;
                    if (m_detailsScroll) {
                        m_detailsScroll->verticalScrollBar()->setValue(
                            targetDetailsVertical);
                        m_detailsScroll->horizontalScrollBar()->setValue(
                            targetDetailsHorizontal);
                    }
                    if (m_historyText) {
                        m_historyText->verticalScrollBar()->setValue(
                            targetHistoryVertical);
                        m_historyText->horizontalScrollBar()->setValue(
                            targetHistoryHorizontal);
                    }
                });
        };

    auto showOrPlaceholder = [](const std::optional<std::string>& value) -> QString {
        if (!value.has_value() || value->empty()) return "-";
        return QString::fromStdString(*value);
    };

    m_detailsTitleLabel->setText(QString::fromStdString(
        libraryDetailsTitle(game.system == "neogeocd", game.name,
                            game.display)));
    m_infoLabels["year"]->setText(showOrPlaceholder(game.year));
    m_infoLabels["manufacturer"]->setText(showOrPlaceholder(game.manufacturer));
    m_infoLabels["genre"]->setText(showOrPlaceholder(game.genre));
    m_infoLabels["players"]->setText(showOrPlaceholder(game.players));
    m_infoLabels["series"]->setText(showOrPlaceholder(game.series));

    const GamePlaytimeRecord* playtime = playtimeMedia.has_value()
        ? m_gamePlaytime.find(game.system, *playtimeMedia)
        : nullptr;
    if (playtime) {
        const QString duration = QString::fromStdString(
            format_playtime_seconds(playtime->total_seconds));
        const QString sessions = QString::number(
            static_cast<qlonglong>(playtime->session_count));
        m_infoLabels["playtime"]->setText(duration);
        m_infoLabels["playtime"]->setToolTip(
            QString("Total playtime: %1").arg(duration));
        m_infoLabels["sessions"]->setText(sessions);
        m_infoLabels["sessions"]->setToolTip(
            QString("%1 recorded session%2")
                .arg(sessions)
                .arg(playtime->session_count == 1 ? "" : "s"));

        const QDateTime lastPlayed = QDateTime::fromSecsSinceEpoch(
            static_cast<qint64>(playtime->last_played_epoch)).toLocalTime();
        m_infoLabels["last_played"]->setText(
            lastPlayed.isValid()
                ? lastPlayed.toString("yyyy-MM-dd HH:mm")
                : QString("-"));
    } else {
        m_infoLabels["playtime"]->setText("Not played yet");
        m_infoLabels["playtime"]->setToolTip("Not played yet");
        m_infoLabels["sessions"]->setText("0");
        m_infoLabels["sessions"]->setToolTip("No recorded sessions");
        m_infoLabels["last_played"]->setText("-");
    }

    if (romIdx < 0) {
        if (game.system == "neogeocd") {
            m_variantLabel->setText(cdVerificationDetailsText(game));
            m_variantLabel->setToolTip(cdVerificationDetailsTooltip(game));
        } else {
            m_variantLabel->clear();
            m_variantLabel->setToolTip(QString());
        }

        loadSnapshotFor(
            game.snapshot.has_value() ? QString::fromStdString(*game.snapshot) : QString(),
            game.short_name);

        m_historyText->setText(
            game.history.has_value() ? QString::fromStdString(*game.history) : QString());
        finishDetailsUpdate();
        return;
    }

    if (romIdx >= static_cast<int>(game.roms.size())) return;

    const Rom& rom = game.roms[romIdx];
    const QString fullName = variantFullName(rom);
    m_variantLabel->setText(
        QStringLiteral("Selected variant: ") + fullName);
    m_variantLabel->setToolTip(fullName);

    loadSnapshotFor(resolveSnapshot(rom.mame, game.short_name));
    m_historyText->setText(
        game.history.has_value() ? QString::fromStdString(*game.history) : QString());
    finishDetailsUpdate();
}

QString MainWindow::resolveSnapshot(const std::string& shortName, const std::string& parentShort) const {
    if (shortName.empty()) return QString();
    fs::path path = m_snapDir / (shortName + ".png");
    if (fs::exists(path)) return QString::fromStdString(path.string());
    if (!parentShort.empty() && parentShort != shortName) {
        fs::path parentPath = m_snapDir / (parentShort + ".png");
        if (fs::exists(parentPath)) return QString::fromStdString(parentPath.string());
    }
    return QString();
}

void MainWindow::loadSnapshotFor(QString path, const std::string& shortFallback) {
    bool pathValid = !path.isEmpty() && fs::exists(path.toStdString());
    if (!pathValid && !shortFallback.empty()) {
        fs::path fallback = m_snapDir / (shortFallback + ".png");
        if (fs::exists(fallback)) {
            path = QString::fromStdString(fallback.string());
            pathValid = true;
        } else {
            pathValid = false;
        }
    }
    if (pathValid) {
        QPixmap pixmap(path);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(500, 370, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_snapshotLabel->setPixmap(pixmap);
            m_snapshotLabel->setText("");
            return;
        }
    }
    m_snapshotLabel->setPixmap(QPixmap());
    m_snapshotLabel->setText("No snapshot available");
}

void MainWindow::filterGames(const QString& text) {
    QString needle = text.toLower().trimmed();
    const bool personalFiltersActive = libraryPersonalFiltersActive(
        m_ratingFilter, m_playtimeFilter);

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        int gameIdx = parent->data(0, GameIndexRole).toInt();
        const Game& game = m_games[gameIdx];

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
        const bool parentOwnMatch = gameMatch && parentFilterMatch &&
            (!m_favoritesOnly || parentFavorite);
        parent->setData(0, ExactMatchRole, parentOwnMatch);

        bool visibleMatchingVariant = false;
        for (int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem* child = parent->child(j);
            int romIdx = child->data(0, RomIndexRole).toInt();
            const Rom& rom = game.roms[romIdx];
            QString romHay = QString("%1 %2 %3 %4")
                                  .arg(QString::fromStdString(rom.mame))
                                  .arg(QString::fromStdString(rom.name.value_or("")))
                                  .arg(QString::fromStdString(rom.label.value_or("")))
                                  .arg(QString::fromStdString(rom.file))
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
                childFilterMatch && (!m_favoritesOnly || childFavorite);

            child->setHidden(!childVisible);
            child->setData(0, ExactMatchRole, childVisible);
            visibleMatchingVariant |= childVisible;
        }

        parent->setHidden(!(parentOwnMatch || visibleMatchingVariant));

        // A parent that does not itself match remains the necessary container
        // for an exact matching variant. Reveal that result immediately.
        if (!parentOwnMatch && visibleMatchingVariant &&
            (m_favoritesOnly || personalFiltersActive)) {
            parent->setData(0, FilterAutoExpandedRole, true);
            parent->setExpanded(true);
        }
    }
    if (!m_rebuildingLibraryView) {
        ensureVisibleSelection(!needle.isEmpty() || m_favoritesOnly ||
                               personalFiltersActive);
    }
    updateStatus();
}

void MainWindow::launchSelected() {
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return;
    launchItem(items.first());
}

void MainWindow::launchSelectedItem(QTreeWidgetItem* item, int /*column*/) {
    launchItem(item);
}

void MainWindow::launchItem(QTreeWidgetItem* item) {
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) return;

    const Game& game = m_games[gameIdx];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return;

    const GameLaunchProfile* profile =
        m_gameProfiles.find(game.system, *media);
    launchMedia(QString::fromStdString(*media), game.system, profile);
}

void MainWindow::openGameSettings() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) return;

    const Game& game = m_games[gameIdx];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return;

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && romIdx < static_cast<int>(game.roms.size())) {
        const Rom& rom = game.roms[romIdx];
        display = variantFullName(rom);
    }

    const GameLaunchProfile* existing =
        m_gameProfiles.find(game.system, *media);
    const std::optional<GameLaunchProfile> previous =
        existing ? std::optional<GameLaunchProfile>(*existing) : std::nullopt;

    GameProfileDialog dialog(
        display, game.system, *media, m_paths,
        load_game_profile_global_settings(m_paths), existing, this);
    if (dialog.exec() != QDialog::Accepted) return;

    const GameLaunchProfile updated = dialog.profile();
    m_gameProfiles.set(game.system, *media, updated);

    std::string error;
    if (!m_gameProfiles.save(m_paths.game_profiles_json, &error)) {
        if (previous.has_value())
            m_gameProfiles.set(game.system, *media, *previous);
        else
            m_gameProfiles.remove(game.system, *media);

        QMessageBox::critical(
            this, "Per-game Settings",
            QString("Could not save the per-game profile.\n\n%1")
                .arg(QString::fromStdString(error)));
        return;
    }

    refreshLibraryView(true);
}

void MainWindow::manageSaveData() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) return;

    const Game& game = m_games[gameIdx];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return;

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && romIdx < static_cast<int>(game.roms.size())) {
        const Rom& rom = game.roms[romIdx];
        display = variantFullName(rom);
    }

    pollTrackedGameProcesses();
    SaveDataDialog dialog(
        display, game.system, *media, m_paths,
        [this]() { return hasActiveTrackedGameProcess(); }, this);
    dialog.exec();
}

void MainWindow::benchmarkSelected() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        QMessageBox::information(
            this, "Performance Benchmark",
            "Select a parent, variant/hack, CUE, or CHD first.");
        return;
    }

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size()))
        return;

    const Game& game = m_games[gameIdx];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) {
        QMessageBox::warning(
            this, "Performance Benchmark",
            "The selected library row has no launchable media.");
        return;
    }

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && romIdx < static_cast<int>(game.roms.size())) {
        const Rom& rom = game.roms[romIdx];
        display = variantFullName(rom);
    }

    const GameLaunchProfile* profile =
        m_gameProfiles.find(game.system, *media);
    JollygoodLaunchPreparation preparation = prepare_jollygood_launch(
        m_paths, m_romDir, m_neocdDir, game.system, *media, profile);
    if (preparation.status != LaunchPreparationStatus::Ready) {
        QMessageBox::warning(
            this, "Performance Benchmark",
            benchmarkPreparationError(
                preparation, m_paths, QString::fromStdString(*media)));
        return;
    }

    warnIfBiosMissing();
    ensure_jollygood_bios(m_paths);

    const GameProfileGlobalSettings global =
        load_game_profile_global_settings(m_paths);
    const int videoApi = effective_jollygood_numeric_option(
        preparation.args, "-a", "--video", global.video_api, 0, 3);
    const int shader = effective_jollygood_numeric_option(
        preparation.args, "-s", "--shader", global.shader, 0, 6);

    BenchmarkDialog dialog(
        display, QString::fromStdString(*media), preparation.profile_applied,
        videoApi, shader, std::move(preparation), this);
    dialog.exec();
}

void MainWindow::exportSelectedAudio() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        QMessageBox::information(
            this, "Audio WAV Export",
            "Select a parent, variant/hack, CUE, or CHD first.");
        return;
    }

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) return;

    const Game& game = m_games[gameIdx];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) {
        QMessageBox::warning(
            this, "Audio WAV Export",
            "The selected library row has no launchable media.");
        return;
    }

    if (jollygood_args_have_wave_output(m_paths.jollygood_args)) {
        QMessageBox::warning(
            this, "Audio WAV Export",
            "JGRF Arguments already contain -o/--wave. Remove that configured "
            "writer from goliath.ini before using Goliath's explicit one-shot "
            "audio export.");
        return;
    }

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && romIdx < static_cast<int>(game.roms.size())) {
        const Rom& rom = game.roms[romIdx];
        display = variantFullName(rom);
    }

    const std::optional<fs::path> outputPath = request_audio_export_target(
        display, m_paths.audio_export_dir, this);
    if (!outputPath.has_value()) return;

    const GameLaunchProfile* profile =
        m_gameProfiles.find(game.system, *media);
    launchMedia(QString::fromStdString(*media), game.system, profile,
                *outputPath);
}

void MainWindow::focusSearch() {
    m_searchEntry->setFocus();
    m_searchEntry->selectAll();
}

void MainWindow::onSortChanged() {
    m_sortKey = m_sortCombo->currentData().toString();
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
            gameIdx < static_cast<int>(m_games.size())) {
            const Game& game = m_games[gameIdx];
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
                        romIdx >= static_cast<int>(game.roms.size())) {
                        continue;
                    }
                    if (metricForMedia(game.roms[romIdx].file) ==
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
    m_filtersButton->setToolTip(activeCount == 0
        ? "Filter the library by rating or playtime"
        : descriptions.join("\n"));
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
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) {
        updateSelection();
        return;
    }

    const Game& game = m_games[gameIdx];
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
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) {
        updateSelection();
        return;
    }

    const Game& game = m_games[gameIdx];
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
        updateSelection();
    }
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
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        if (parent->isExpanded() == expanded) continue;
        parent->setExpanded(expanded);
        changed = true;
    }

    if (!changed) return;
    if (!expanded) ensureVisibleSelection(false);
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

void MainWindow::launchRandomGame() {
    std::vector<QTreeWidgetItem*> visibleItems;
    visibleItems.reserve(m_tree->topLevelItemCount());

    const bool restrictiveView = m_favoritesOnly ||
        libraryPersonalFiltersActive(m_ratingFilter, m_playtimeFilter) ||
        (m_searchEntry && !m_searchEntry->text().trimmed().isEmpty());

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(i);
        if (!parent || parent->isHidden()) continue;

        if (!restrictiveView) {
            visibleItems.push_back(parent);
            continue;
        }

        if (parent->data(0, ExactMatchRole).toBool()) {
            visibleItems.push_back(parent);
        }

        for (int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem* child = parent->child(j);
            if (!child || child->isHidden()) continue;
            if (child->data(0, ExactMatchRole).toBool()) {
                visibleItems.push_back(child);
            }
        }
    }

    if (visibleItems.empty()) return;

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, visibleItems.size() - 1);

    QTreeWidgetItem* item = visibleItems[dist(rng)];
    m_tree->setCurrentItem(item);
    launchItem(item);
}

void MainWindow::revealInExplorer(const QString& path) {
    if (path.isEmpty()) return;
    QFileInfo info(path);
    if (!info.exists()) return;
    QString target = info.isDir() ? path : info.absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}

QString MainWindow::selectedRomFile() const {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return {};

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) return {};

    const auto media = selected_launch_media(m_games[gameIdx], romIdx);
    return media.has_value() ? QString::fromStdString(*media) : QString();
}

QString MainWindow::selectedRomPath() const {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return {};

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) return {};

    const Game& game = m_games[gameIdx];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return {};

    const fs::path& mediaDir = (game.system == "neogeocd") ? m_neocdDir : m_romDir;
    return QString::fromStdString((mediaDir / *media).string());
}

void MainWindow::openRomFolder() {
    QString path = selectedRomPath();
    if (path.isEmpty()) {
        const fs::path& mediaDir = (m_librarySystem == "neogeocd") ? m_neocdDir : m_romDir;
        path = QString::fromStdString(mediaDir.string());
    }
    revealInExplorer(path);
}

void MainWindow::openGameConfigFolder() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) return;

    const Game& game = m_games[gameIdx];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return;

    const GameLaunchProfile* profile =
        m_gameProfiles.find(game.system, *media);
    if (!profile) return;

    // Runtime INIs are derived and disposable. Refresh them before opening
    // the directory so it always represents the current global settings plus
    // the profile for this exact parent, variant/hack, CUE, or CHD.
    const GameProfileRuntimeResult runtime =
        materialize_game_profile_runtime(
            m_paths, game.system, *media, *profile);
    if (!runtime.success) {
        QMessageBox::warning(
            this, "Open Per-Game Config Folder",
            QString("Could not prepare the per-game configuration folder.\n\n%1")
                .arg(QString::fromStdString(runtime.error)));
        return;
    }

    revealInExplorer(
        QString::fromStdString((runtime.config_root / "jollygood").string()));
}

void MainWindow::openSnapshotFolder() {
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        revealInExplorer(QString::fromStdString(m_snapDir.string()));
        return;
    }
    QTreeWidgetItem* item = items.first();
    int gameIdx = item->data(0, GameIndexRole).toInt();
    int romIdx = item->data(0, RomIndexRole).toInt();
    std::string shortName;
    if (gameIdx >= 0 && gameIdx < (int)m_games.size()) {
        const Game& game = m_games[gameIdx];
        if (romIdx >= 0 && romIdx < (int)game.roms.size()) {
            shortName = game.roms[romIdx].mame;
        } else {
            shortName = game.short_name;
        }
    }
    QString path = resolveSnapshot(shortName);
    if (path.isEmpty()) path = QString::fromStdString(m_snapDir.string());
    revealInExplorer(path);
}

void MainWindow::showTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;
    m_tree->setCurrentItem(item);
    int romIdx = item->data(0, RomIndexRole).toInt();
    const int gameIdx = item->data(0, GameIndexRole).toInt();

    QMenu menu(m_tree);
    menu.addAction("Launch", this, &MainWindow::launchSelected);
    if (gameIdx >= 0 && gameIdx < static_cast<int>(m_games.size())) {
        const Game& game = m_games[gameIdx];
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
    menu.addAction("Export audio WAV...", this,
                   &MainWindow::exportSelectedAudio);
    menu.addAction("Game settings...", this, &MainWindow::openGameSettings);
    menu.addAction("Manage save data...", this, &MainWindow::manageSaveData);
    menu.addAction("Benchmark...", this, &MainWindow::benchmarkSelected);
    menu.addSeparator();
    menu.addAction("Open ROM folder", this, &MainWindow::openRomFolder);
    menu.addAction("Open snapshot folder", this, &MainWindow::openSnapshotFolder);

    if (gameIdx >= 0 && gameIdx < static_cast<int>(m_games.size())) {
        const Game& game = m_games[gameIdx];
        const auto media = selected_launch_media(game, romIdx);
        if (media.has_value() && m_gameProfiles.find(game.system, *media)) {
            menu.addAction("Open per-game config folder", this,
                           &MainWindow::openGameConfigFolder);
        }
    }

    menu.addSeparator();
    if (romIdx < 0) {
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
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void MainWindow::updateStatus() {
    std::size_t totalRoms = 0;
    std::size_t parents = 0;
    std::size_t homebrew = 0;
    std::size_t identified = 0;
    std::size_t verifiedSets = 0;
    std::size_t cueMismatch = 0;
    std::size_t metadataOnly = 0;
    std::size_t unknown = 0;
    std::size_t favorites = 0;
    for (const Game& g : m_games) {
        if (g.system != m_librarySystem) continue;
        totalRoms += g.roms.size();
        parents++;
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
            } else if (g.identified) {
                metadataOnly++;
            }
        }
    }
    const std::size_t variants = totalRoms >= parents ? totalRoms - parents : 0;

    QString message;
    if (m_librarySystem == "neogeocd") {
        message = QString(
                      "Neo Geo CD - Games: %1 - Identified: %2 - Verified sets: %3 - "
                      "CUE mismatch: %4 - Metadata only: %5 - Unknown: %6")
                      .arg(parents)
                      .arg(identified)
                      .arg(verifiedSets)
                      .arg(cueMismatch)
                      .arg(metadataOnly)
                      .arg(unknown);
    } else {
        message = QString("Neo Geo MVS/AES - ROMs: %1 - Parents: %2 - Variants: %3 - Homebrew: %4")
                      .arg(totalRoms).arg(parents).arg(variants).arg(homebrew);
    }

    message += QString(" - Favorites: %1").arg(
        static_cast<qulonglong>(favorites));

    int count = m_tree->topLevelItemCount();
    int visible = 0;
    for (int i = 0; i < count; ++i) if (!m_tree->topLevelItem(i)->isHidden()) visible++;
    if (visible != count) {
        message += QString("  (showing %1 of %2)").arg(visible).arg(count);
    }
    statusBar()->showMessage(message);
}

void MainWindow::restoreSelection(const QString& preferredRom) {
    std::string targetRom = preferredRom.toStdString();
    if (targetRom.empty()) {
        targetRom = m_config.get("UI", "last_rom", "");
    }

    if (!targetRom.empty() && !m_games.empty()) {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* parent = m_tree->topLevelItem(i);
            if (!parent || parent->isHidden()) continue;

            const int gameIdx = parent->data(0, GameIndexRole).toInt();
            if (gameIdx < 0 || gameIdx >= static_cast<int>(m_games.size())) continue;

            const Game& game = m_games[gameIdx];
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
                if (romIdx < 0 || romIdx >= static_cast<int>(game.roms.size())) continue;
                if (game.roms[romIdx].file != targetRom) continue;

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
