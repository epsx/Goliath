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
#include <QStatusBar>
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

std::vector<int> MainWindow::sortedGameOrder() const {
    std::vector<int> idx(m_games.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    };

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
            return lower(m_games[a].display) > lower(m_games[b].display);
        });
    } else if (m_sortKey == "year_desc") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return yearValue(m_games[a]) > yearValue(m_games[b]);
        });
    } else if (m_sortKey == "year") {
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return yearValue(m_games[a]) < yearValue(m_games[b]);
        });
    } else {
        // "display" and legacy/unknown sort keys fall back to Name (A -> Z).
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return lower(m_games[a].display) < lower(m_games[b].display);
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
        if (game.main_rom.has_value() &&
            m_gameProfiles.find(game.system, *game.main_rom)) {
            parentText += QString::fromUtf8(" \xE2\x9A\x99");
            parentItem->setToolTip(
                0, "A per-game launch profile is active for this media. "
                   "Right-click and choose Game settings to inspect or reset it.");
        }
        parentItem->setText(0, parentText);
        parentItem->setData(0, GameIndexRole, idx);
        parentItem->setData(0, RomIndexRole, -1);

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

        for (int romIdx = 0; romIdx < (int)game.roms.size(); ++romIdx) {
            const Rom& rom = game.roms[romIdx];
            if (rom.main) continue;
            auto* childItem = new QTreeWidgetItem(parentItem);
            QString childText = QString::fromStdString(
                rom.mame.empty() ? rom.file : rom.mame);
            QString childTooltip = variantFullName(rom);
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
            childItem->setHidden(!m_showVariants);
        }

        parentItem->setExpanded(false);
    }

    if (m_tree->topLevelItemCount() == 0) {
        clearDetailsForNoSelection(false);
    }

    updateStatus();
}

void MainWindow::refreshLibraryView(bool preserveSelection) {
    const QString preferredRom = preserveSelection ? selectedRomFile() : QString();

    populateTree();

    const bool filtered = m_searchEntry && !m_searchEntry->text().isEmpty();
    m_rebuildingLibraryView = true;
    if (filtered) {
        filterGames(m_searchEntry->text());
    }

    restoreSelection(preferredRom);
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
}

void MainWindow::ensureVisibleSelection(bool filtered) {
    QTreeWidgetItem* current = m_tree->currentItem();
    QTreeWidgetItem* replacement = current;
    while (replacement && replacement->parent()) {
        replacement = replacement->parent();
    }
    if (replacement && replacement->isHidden()) replacement = nullptr;

    if (!replacement) {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_tree->topLevelItem(i);
            if (item && !item->isHidden()) {
                replacement = item;
                break;
            }
        }
    }

    const VisibleSelectionDecision decision = visibleSelectionDecision(
        treeItemIsEffectivelyVisible(current), replacement != nullptr);
    switch (decision) {
    case VisibleSelectionDecision::KeepCurrent:
        break;
    case VisibleSelectionDecision::SelectVisibleReplacement:
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

    const bool isCd = m_librarySystem == "neogeocd";
    m_detailsTitleLabel->setText(
        filtered ? "No matching games"
                 : (isCd ? "Neo Geo CD" : "Neo Geo MVS/AES"));
    m_variantLabel->clear();
    m_variantLabel->setToolTip(QString());
    for (auto& entry : m_infoLabels) {
        entry.second->setText("-");
    }
    m_historyText->setPlainText(filtered
        ? "No games match the current search."
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
        m_infoLabels["playtime"]->setText(
            QString("%1 (%2 session%3)")
                .arg(QString::fromStdString(
                    format_playtime_seconds(playtime->total_seconds)))
                .arg(static_cast<qlonglong>(playtime->session_count))
                .arg(playtime->session_count == 1 ? "" : "s"));

        const QDateTime lastPlayed = QDateTime::fromSecsSinceEpoch(
            static_cast<qint64>(playtime->last_played_epoch)).toLocalTime();
        m_infoLabels["last_played"]->setText(
            lastPlayed.isValid()
                ? lastPlayed.toString("yyyy-MM-dd HH:mm")
                : QString("-"));
    } else {
        m_infoLabels["playtime"]->setText("Not played yet");
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

        bool variantMatch = false;
        if (m_showVariants) {
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
                if (romHay.contains(needle)) {
                    variantMatch = true;
                    child->setHidden(false);
                } else {
                    child->setHidden(true);
                }
            }
        }

        if (gameMatch || variantMatch) {
            parent->setHidden(false);
            if (gameMatch && m_showVariants) {
                for (int j = 0; j < parent->childCount(); ++j) parent->child(j)->setHidden(false);
            }
        } else {
            parent->setHidden(true);
        }
    }
    if (!m_rebuildingLibraryView) {
        ensureVisibleSelection(!needle.isEmpty());
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
}

void MainWindow::onShowVariantsChanged(bool checked) {
    m_showVariants = checked;

    m_config.set("UI", "show_variants", m_showVariants ? "true" : "false");
    save_config(m_config);

    refreshLibraryView(true);
}

void MainWindow::expandAll() {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) m_tree->topLevelItem(i)->setExpanded(true);
}

void MainWindow::collapseAll() {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) m_tree->topLevelItem(i)->setExpanded(false);
    ensureVisibleSelection(false);
}

void MainWindow::launchRandomGame() {
    std::vector<QTreeWidgetItem*> visibleItems;
    visibleItems.reserve(m_tree->topLevelItemCount());

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        if (item && !item->isHidden()) visibleItems.push_back(item);
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

    QMenu menu(m_tree);
    menu.addAction("Launch", this, &MainWindow::launchSelected);
    menu.addAction("Export audio WAV...", this,
                   &MainWindow::exportSelectedAudio);
    menu.addAction("Game settings...", this, &MainWindow::openGameSettings);
    menu.addAction("Manage save data...", this, &MainWindow::manageSaveData);
    menu.addAction("Benchmark...", this, &MainWindow::benchmarkSelected);
    menu.addSeparator();
    menu.addAction("Open ROM folder", this, &MainWindow::openRomFolder);
    menu.addAction("Open snapshot folder", this, &MainWindow::openSnapshotFolder);

    const int gameIdx = item->data(0, GameIndexRole).toInt();
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
    for (const Game& g : m_games) {
        if (g.system != m_librarySystem) continue;
        totalRoms += g.roms.size();
        parents++;
        if (g.source == "homebrew") homebrew++;
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
