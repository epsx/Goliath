#include "ui/main_window.hpp"

#include "common/qt_path.hpp"
#include "common/debug_logger.hpp"
#include "game/game_integrity.hpp"
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
#include "ui/library_verification_dialog.hpp"
#include "ui/save_data_dialog.hpp"
#include "ui/widgets/title_bar.hpp"

#include <QDesktopServices>
#include <QDateTime>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <atomic>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace goliath {

namespace {
QString benchmarkPreparationError(
        const JollygoodLaunchPreparation& preparation) {
    return QString::fromStdString(
        launch_preparation_failure_text(preparation));
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
    if (!media.has_value()) {
        showThemedMessageDialog(
            this, "Game Cannot Start",
            "The selected library row has no launchable media.\n\n"
            "Rescan the library to rebuild its parent/variant mapping.",
            ThemedMessageTone::Warning);
        return;
    }

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

        showThemedMessageDialog(
            this, "Per-game Settings",
            QString("Could not save the per-game profile.\n\n%1")
                .arg(QString::fromStdString(error)),
            ThemedMessageTone::Critical);
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
        showThemedMessageDialog(
            this, "Performance Benchmark",
            "Select a parent, variant/hack, CUE, or CHD first.",
            ThemedMessageTone::Information);
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
        showThemedMessageDialog(
            this, "Performance Benchmark",
            "The selected library row has no launchable media.",
            ThemedMessageTone::Warning);
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
        showThemedMessageDialog(
            this, "Performance Benchmark",
            benchmarkPreparationError(preparation),
            ThemedMessageTone::Warning);
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

void MainWindow::verifySelectedGameIntegrity() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 ||
        static_cast<std::size_t>(gameIdx) >= m_games.size()) {
        return;
    }

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) {
        showThemedMessageDialog(
            this, "Game Integrity Check",
            "The selected library row has no game media to verify.",
            ThemedMessageTone::Warning);
        return;
    }

    QString display = QString::fromStdString(game.display);
    if (romIdx >= 0 &&
        static_cast<std::size_t>(romIdx) < game.roms.size()) {
        display = libraryVariantFullName(
            game.roms[static_cast<std::size_t>(romIdx)]);
    }

    const Rom* metadataRom = libraryDetailsRom(game, romIdx);
    const std::optional<Rom> romSnapshot = metadataRom
        ? std::optional<Rom>(*metadataRom)
        : std::nullopt;
    const fs::path mediaRoot =
        game.system == "neogeocd" ? m_neocdDir : m_romDir;
    const fs::path cacheDirectory = m_paths.json_file.parent_path();
    const std::string mediaName = *media;

    auto result = std::make_shared<GameIntegrityResult>();
    auto failure = std::make_shared<std::string>();
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    QThread* worker = QThread::create(
        [mediaRoot, mediaName, romSnapshot, cacheDirectory,
         result, failure, cancel]() {
            try {
                *result = verify_game_integrity(
                    mediaRoot, mediaName,
                    romSnapshot.has_value() ? &*romSnapshot : nullptr,
                    cacheDirectory, cancel.get());
            } catch (const std::exception& error) {
                *failure = error.what();
            } catch (...) {
                *failure = "Unknown integrity verification error.";
            }
        });

    QDialog progress(this);
    progress.setModal(true);
    progress.resize(640, 220);
    progress.setMinimumSize(520, 200);
    QVBoxLayout* progressLayout = nullptr;
    setupFramelessDialog(
        &progress, "Game Integrity Check", &progressLayout, false, false);

    auto* progressText = new QLabel(
        QString("Checking %1 against the latest library references...")
            .arg(display),
        &progress);
    progressText->setWordWrap(true);
    progressLayout->addWidget(progressText);

    auto* progressBar = new QProgressBar(&progress);
    progressBar->setRange(0, 0);
    progressBar->setTextVisible(false);
    progressLayout->addWidget(progressBar);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* cancelButton = new QPushButton("Cancel", &progress);
    cancelButton->setObjectName("about_close_button");
    buttonRow->addWidget(cancelButton);
    progressLayout->addLayout(buttonRow);

    connect(cancelButton, &QPushButton::clicked, &progress,
            [cancel, cancelButton, progressText]() {
                cancel->store(true, std::memory_order_release);
                cancelButton->setEnabled(false);
                cancelButton->setText("Stopping...");
                progressText->setText(
                    "Stopping the integrity check safely...");
            });
    connect(&progress, &QDialog::rejected, &progress,
            [cancel]() {
                cancel->store(true, std::memory_order_release);
            });
    QEventLoop verificationLoop;
    QTimer progressDelay;
    QElapsedTimer progressVisibleTimer;
    progressDelay.setSingleShot(true);
    progressDelay.setInterval(500);
    connect(&progressDelay, &QTimer::timeout, &progress,
            [&progress, &progressVisibleTimer]() {
                progressVisibleTimer.start();
                progress.show();
                progress.raise();
                progress.activateWindow();
            });
    connect(worker, &QThread::finished, &progress,
            [&progress, &progressVisibleTimer, &verificationLoop]() {
                auto finishProgress = [&progress, &verificationLoop]() {
                    if (progress.isVisible()) progress.accept();
                    verificationLoop.quit();
                };
                if (!progress.isVisible() ||
                    !progressVisibleTimer.isValid()) {
                    finishProgress();
                    return;
                }

                constexpr qint64 minimumVisibleMs = 300;
                const qint64 elapsedMs = progressVisibleTimer.elapsed();
                if (elapsedMs >= minimumVisibleMs) {
                    finishProgress();
                    return;
                }

                QTimer::singleShot(
                    static_cast<int>(minimumVisibleMs - elapsedMs),
                    &progress, finishProgress);
            });

    worker->start();
    progressDelay.start();
    if (worker->isRunning()) verificationLoop.exec();
    progressDelay.stop();
    progress.hide();
    if (worker->isRunning())
        cancel->store(true, std::memory_order_release);
    worker->wait();
    delete worker;

    if (!failure->empty()) {
        DebugLogger::logError(
            QString("game integrity verification failed: %1")
                .arg(QString::fromStdString(*failure)));
        addNotification({
            "integrity:failure",
            NotificationCategory::Integrity,
            NotificationSeverity::Error,
            "Game integrity check failed",
            QString::fromStdString(*failure).left(1000).toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            NotificationAction::OpenLogs,
        });
        showThemedMessageDialog(
            this, "Game Integrity Check Failed",
            QString("The integrity check could not be completed.\n\n%1")
                .arg(QString::fromStdString(*failure)),
            ThemedMessageTone::Critical);
        return;
    }
    if (result->status == GameIntegrityStatus::Canceled) return;

    const QString mediaPath = path_to_qstring(result->media_path);
    DebugLogger::logInfo(
        QString("game integrity result for %1: matched=%2 problems=%3")
            .arg(mediaPath)
            .arg(static_cast<qulonglong>(result->matched_files))
            .arg(static_cast<qulonglong>(result->problem_files)));

    if (result->status == GameIntegrityStatus::ProblemsFound) {
        addNotification({
            "integrity:problem",
            NotificationCategory::Integrity,
            NotificationSeverity::Warning,
            "Game integrity problems found",
            QString("%1\nMedia: %2")
                .arg(QString::fromStdString(result->detail), mediaPath)
                .left(1000).toStdString(),
            QDateTime::currentSecsSinceEpoch(),
            false,
            NotificationAction::None,
        });
    }

    if (result->hashes.empty()) {
        showThemedMessageDialog(
            this,
            result->status == GameIntegrityStatus::ProblemsFound
                ? QStringLiteral("Game Integrity Problems Found")
                : QStringLiteral("Game File Available"),
            QString("%1\n\nMedia: %2")
                .arg(QString::fromStdString(result->detail), mediaPath),
            result->status == GameIntegrityStatus::ProblemsFound
                ? ThemedMessageTone::Warning
                : ThemedMessageTone::Information);
        return;
    }

    Rom current = romSnapshot.value_or(Rom{});
    current.hashes = result->hashes;
    LibraryVerificationPresentation presentation;
    presentation.label = "Current integrity";
    presentation.value =
        result->status == GameIntegrityStatus::Passed
            ? "Passed"
            : "Problems found";
    presentation.tooltip = result->detail + " Media: " +
        path_to_utf8(result->media_path);
    presentation.tone =
        result->status == GameIntegrityStatus::Passed
            ? VerificationTone::Success
            : VerificationTone::Error;
    presentation.details_available = true;
    showLibraryVerificationDialog(display, current, presentation, this);
}

void MainWindow::exportSelectedAudio() {
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    if (items.isEmpty()) {
        showThemedMessageDialog(
            this, "Audio WAV Export",
            "Select a parent, variant/hack, CUE, or CHD first.",
            ThemedMessageTone::Information);
        return;
    }

    QTreeWidgetItem* item = items.first();
    const int gameIdx = item->data(0, GameIndexRole).toInt();
    const int romIdx = item->data(0, RomIndexRole).toInt();
    if (gameIdx < 0 || static_cast<std::size_t>(gameIdx) >= m_games.size()) return;

    const Game& game = m_games[static_cast<std::size_t>(gameIdx)];
    const auto media = selected_launch_media(game, romIdx);
    if (!media.has_value()) {
        showThemedMessageDialog(
            this, "Audio WAV Export",
            "The selected library row has no launchable media.",
            ThemedMessageTone::Warning);
        return;
    }

    if (jollygood_args_have_wave_output(m_paths.jollygood_args)) {
        showThemedMessageDialog(
            this, "Audio WAV Export",
            "JGRF Arguments already contain -o/--wave. Remove that configured "
            "writer from goliath.ini before using Goliath's explicit one-shot "
            "audio export.",
            ThemedMessageTone::Warning);
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
        !m_genreFilter.empty() ||
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
