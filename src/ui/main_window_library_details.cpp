#include "ui/main_window.hpp"

#include "common/qt_path.hpp"
#include "game/game_profile_runtime.hpp"
#include "game/game_profile_summary.hpp"
#include "game/jollygood_launch.hpp"
#include "ui/library_details_presentation.hpp"
#include "ui/library_item_roles.hpp"
#include "ui/library_verification_dialog.hpp"
#include "ui/library_view_logic.hpp"
#include "ui/recording_id.hpp"

#include <QAction>
#include <QDateTime>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <cstddef>
#include <filesystem>

namespace fs = std::filesystem;

namespace goliath {

namespace {

bool treeItemIsEffectivelyVisible(const QTreeWidgetItem* item) {
    for (const QTreeWidgetItem* current = item; current;
         current = current->parent()) {
        if (current->isHidden()) return false;
        if (current->parent() && !current->parent()->isExpanded()) return false;
    }
    return item != nullptr;
}

} // namespace

void MainWindow::setSelectionActionsEnabled(bool enabled) {
    for (QAction* action : m_selectionActions) {
        if (action) action->setEnabled(enabled);
    }
    if (m_launchButton) m_launchButton->setEnabled(enabled);
    if (m_gameSettingsButton) m_gameSettingsButton->setEnabled(enabled);
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
        m_favoriteButton->setText("Favorite");
    }
    updateRatingButtons(0, false);

    const bool isCd = m_librarySystem == "neogeocd";
    m_detailsTitleLabel->setText(
        filtered ? "No matching games"
                 : (isCd ? "Neo Geo CD" : "Neo Geo MVS/AES"));
    m_variantLabel->clear();
    m_variantLabel->setToolTip(QString());
    for (auto& entry : m_infoFieldLabels)
        entry.second->show();
    for (auto& entry : m_infoLabels) {
        entry.second->show();
        entry.second->setText("-");
        entry.second->setToolTip(QString());
    }
    m_infoFieldNameLabels["catalog_id"]->setText("MAME ID");
    m_infoLabels["catalog_id"]->setProperty("externalUrl", QString());
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
    syncIconSelectionFromTree();
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        setSelectionActionsEnabled(false);
        return;
    }

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0) {
        setSelectionActionsEnabled(false);
        return;
    }
    const std::size_t gameIndex = static_cast<std::size_t>(gameIdx);
    if (gameIndex >= m_games.size()) {
        setSelectionActionsEnabled(false);
        return;
    }

    const Game& game = m_games[gameIndex];
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
        m_favoriteButton->setText("Favorite");
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

    const GameLaunchProfile* selectedProfile =
        playtimeMedia.has_value()
            ? m_gameProfiles.find(game.system, *playtimeMedia)
            : nullptr;
    const GameProfileSummary profileSummary = summarize_game_profile(
        game.system, selectedProfile,
        load_game_profile_global_settings(m_paths));
    const GamePlaytimeRecord* playtime = playtimeMedia.has_value()
        ? m_gamePlaytime.find(game.system, *playtimeMedia)
        : nullptr;
    const LibraryDetailsPresentation presentation =
        buildLibraryDetailsPresentation(
            game, romIdx, playtimeMedia, profileSummary, playtime);

    auto setOptionalInfoRow = [this](
            const char* key,
            const LibraryDetailsRowPresentation& row) {
        m_infoFieldLabels[key]->setVisible(row.visible);
        m_infoLabels[key]->setVisible(row.visible);
        m_infoLabels[key]->setText(row.visible ? row.text : QString());
        m_infoLabels[key]->setToolTip(
            row.visible ? row.tooltip : QString());
    };

    m_detailsTitleLabel->setText(presentation.title);
    m_infoLabels["year"]->setText(presentation.year);
    m_infoLabels["manufacturer"]->setText(presentation.manufacturer);
    setOptionalInfoRow("alt_title", presentation.alternate_title);
    m_infoLabels["genre"]->setText(presentation.genre);
    m_infoLabels["players"]->setText(presentation.players);
    m_infoLabels["series"]->setText(presentation.series);

    m_infoFieldNameLabels["catalog_id"]->setText(
        presentation.catalog_label);
    m_infoLabels["catalog_id"]->setProperty(
        "externalUrl", presentation.catalog_url);
    m_infoLabels["catalog_id"]->setToolTip(presentation.catalog_url);
    m_infoLabels["catalog_id"]->setText(presentation.catalog_html);

    setOptionalInfoRow("serial", presentation.serial);
    setOptionalInfoRow("release", presentation.release);
    setOptionalInfoRow("media_format", presentation.media_format);
    setOptionalInfoRow("program_rom", presentation.program_rom);

    m_infoLabels["verification"]->setText(
        presentation.verification_html);
    m_infoLabels["verification"]->setProperty(
        "verificationDetailsAvailable",
        presentation.verification_details_available);
    m_infoLabels["verification"]->setToolTip(
        presentation.verification_tooltip);

    m_infoLabels["profile_status"]->setText(presentation.profile_status);
    m_infoLabels["profile_system"]->setText(presentation.profile_system);
    m_infoLabels["profile_video"]->setText(presentation.profile_video);
    m_infoLabels["profile_input"]->setText(presentation.profile_input);

    m_infoLabels["playtime"]->setToolTip(QString());
    m_infoLabels["sessions"]->setToolTip(QString());
    m_infoLabels["playtime"]->setText(presentation.playtime);
    m_infoLabels["sessions"]->setText(presentation.sessions);
    const auto formatSessionTimestamp = [](
            const std::optional<std::int64_t>& epoch) {
        if (!epoch.has_value()) return QStringLiteral("-");
        const QDateTime timestamp = QDateTime::fromSecsSinceEpoch(
            static_cast<qint64>(*epoch)).toLocalTime();
        return timestamp.isValid()
            ? timestamp.toString("yyyy-MM-dd HH:mm")
            : QStringLiteral("-");
    };
    m_infoLabels["last_started"]->setText(
        formatSessionTimestamp(presentation.last_started_epoch));
    m_infoLabels["last_ended"]->setText(
        formatSessionTimestamp(presentation.last_ended_epoch));
    m_infoLabels["last_session"]->setText(presentation.last_session);

    if (presentation.selection_kind ==
        LibraryDetailsSelectionKind::InvalidVariant) return;

    m_variantLabel->setText(presentation.variant_text);
    m_variantLabel->setToolTip(presentation.variant_tooltip);

    if (presentation.selection_kind ==
        LibraryDetailsSelectionKind::Game) {
        loadSnapshotFor(presentation.snapshot_path,
                        presentation.snapshot_short_name,
                        presentation.recording_media);
        m_historyText->setText(presentation.history);
        finishDetailsUpdate();
        return;
    }

    loadSnapshotFor(resolveSnapshot(
                        presentation.snapshot_short_name,
                        presentation.snapshot_parent_short_name),
                    presentation.snapshot_short_name,
                    presentation.recording_media);
    m_historyText->setText(presentation.history);
    finishDetailsUpdate();
}

QString MainWindow::resolveSnapshot(const std::string& shortName, const std::string& parentShort) const {
    if (shortName.empty()) return QString();
    fs::path path = m_snapDir / (shortName + ".png");
    if (fs::exists(path)) return path_to_qstring(path);
    if (!parentShort.empty() && parentShort != shortName) {
        fs::path parentPath = m_snapDir / (parentShort + ".png");
        if (fs::exists(parentPath)) return path_to_qstring(parentPath);
    }
    return QString();
}

void MainWindow::loadSnapshotFor(QString path, const std::string& shortFallback,
                                 const std::string& recordingMedia) {
    bool pathValid = !path.isEmpty() && fs::exists(qstring_to_path(path));
    if (!pathValid && !shortFallback.empty()) {
        fs::path fallback = m_snapDir / (shortFallback + ".png");
        if (fs::exists(fallback)) {
            path = path_to_qstring(fallback);
            pathValid = true;
        } else {
            pathValid = false;
        }
    }
    if (!pathValid) path.clear();
    const QString romId = QString::fromStdString(shortFallback);
    const QString recordingId = recordingMedia.empty() ? QString()
        : recordingIdForMedia(QString::fromStdString(recordingMedia));
    if (!romId.isEmpty() && romId == m_galleryRomId &&
        path == m_gallerySnapshotPath &&
        recordingId == m_galleryRecordingId) return;
    stopGalleryMovie();
    m_galleryRomId = romId;
    m_galleryRecordingId = recordingId;
    m_gallerySnapshotPath = path;
    m_gallerySnapshot = QPixmap();
    if (pathValid) {
        QPixmap pixmap(path);
        if (!pixmap.isNull()) {
            m_gallerySnapshot = pixmap.scaled(m_snapshotLabel->size(),
                                              Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation);
        }
    }
    refreshGameGallery();
}

void MainWindow::showVerificationDetails() {
    if (!m_tree) return;
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;

    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0) return;
    const std::size_t gameIndex = static_cast<std::size_t>(gameIdx);
    if (gameIndex >= m_games.size()) return;

    const Game& game = m_games[gameIndex];
    const Rom* rom = libraryDetailsRom(game, romIdx);
    if (!rom || rom->hashes.empty()) return;

    const LibraryVerificationPresentation presentation =
        libraryVerificationPresentation(game, rom);

    const QString title = QString::fromStdString(
        rom->name.value_or(game.name));
    showLibraryVerificationDialog(title, *rom, presentation, this);
}

} // namespace goliath
