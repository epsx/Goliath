#include "ui/main_window.hpp"

#include "common/qt_path.hpp"
#include "game/game_profile_runtime.hpp"
#include "game/game_profile_summary.hpp"
#include "game/jollygood_benchmark.hpp"
#include "game/jollygood_bios.hpp"
#include "game/jollygood_launch.hpp"
#include "ui/audio_export_dialog.hpp"
#include "ui/benchmark_dialog.hpp"
#include "ui/game_profile_dialog.hpp"
#include "ui/library_details_presentation.hpp"
#include "ui/library_item_roles.hpp"
#include "ui/library_view_logic.hpp"
#include "ui/save_data_dialog.hpp"

#include <QDesktopServices>
#include <QDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace goliath {

namespace {
QString benchmarkPreparationError(
        const JollygoodLaunchPreparation& preparation,
        const AppPaths& paths,
        const QString& media) {
    switch (preparation.status) {
    case LaunchPreparationStatus::Ready:
        return {};
    case LaunchPreparationStatus::ExecutableNotFound:
        return QString("Jollygood executable not found:\n%1")
            .arg(path_to_qstring(paths.jollygood_exe));
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
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size()) return;

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return;

    const GameLaunchProfile* profile =
        m_gameProfiles.find(game.system, *media);
    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && static_cast<std::size_t>(romIdx) < game.roms.size())
        display = libraryVariantFullName(
            game.roms[static_cast<std::size_t>(romIdx)]);
    launchMedia(QString::fromStdString(*media), game.system, profile,
                std::nullopt, command_dat_lookup_ids(game, romIdx), display);
}

void MainWindow::openGameSettings() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size()) return;

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return;

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && static_cast<std::size_t>(romIdx) < game.roms.size()) {
        const Rom& rom = game.roms[static_cast<std::size_t>(romIdx)];
        display = libraryVariantFullName(rom);
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
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size()) return;

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return;

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && static_cast<std::size_t>(romIdx) < game.roms.size()) {
        const Rom& rom = game.roms[static_cast<std::size_t>(romIdx)];
        display = libraryVariantFullName(rom);
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
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size())
        return;

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) {
        QMessageBox::warning(
            this, "Performance Benchmark",
            "The selected library row has no launchable media.");
        return;
    }

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 && static_cast<std::size_t>(romIdx) < game.roms.size()) {
        const Rom& rom = game.roms[static_cast<std::size_t>(romIdx)];
        display = libraryVariantFullName(rom);
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
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size()) return;

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
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
    if (romIdx >= 0 && static_cast<std::size_t>(romIdx) < game.roms.size()) {
        const Rom& rom = game.roms[static_cast<std::size_t>(romIdx)];
        display = libraryVariantFullName(rom);
    }

    const std::optional<fs::path> outputPath = request_audio_export_target(
        display, m_paths.audio_export_dir, this);
    if (!outputPath.has_value()) return;

    const GameLaunchProfile* profile =
        m_gameProfiles.find(game.system, *media);
    launchMedia(QString::fromStdString(*media), game.system, profile,
                *outputPath);
}

void MainWindow::launchRandomGame() {
    std::vector<QTreeWidgetItem*> visibleItems;
    visibleItems.reserve(
        static_cast<std::size_t>(m_tree->topLevelItemCount()));

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

QString MainWindow::selectedRomPath() const {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return {};

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size()) return {};

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) return {};

    const fs::path& mediaDir = (game.system == "neogeocd") ? m_neocdDir : m_romDir;
    return path_to_qstring(mediaDir / path_from_utf8(*media));
}

void MainWindow::openRomFolder() {
    QString path = selectedRomPath();
    if (path.isEmpty()) {
        const fs::path& mediaDir = (m_librarySystem == "neogeocd") ? m_neocdDir : m_romDir;
        path = path_to_qstring(mediaDir);
    }
    revealInExplorer(path);
}

void MainWindow::openGameConfigFolder() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size()) return;

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
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

    revealInExplorer(path_to_qstring(runtime.config_root / "jollygood"));
}

void MainWindow::openSnapshotFolder() {
    QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        revealInExplorer(path_to_qstring(m_snapDir));
        return;
    }
    QTreeWidgetItem* item = items.first();
    int gameIdx = item->data(0, GameIndexRole).toInt();
    int romIdx = item->data(0, RomIndexRole).toInt();
    std::string shortName;
    if (gameIdx >= 0 && static_cast<std::size_t>(gameIdx) < m_games.size()) {
        const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
        if (romIdx >= 0 &&
            static_cast<std::size_t>(romIdx) < game.roms.size()) {
            shortName = game.roms[static_cast<std::size_t>(romIdx)].mame;
        } else {
            shortName = game.short_name;
        }
    }
    QString path = resolveSnapshot(shortName);
    if (path.isEmpty()) path = path_to_qstring(m_snapDir);
    revealInExplorer(path);
}

} // namespace goliath
