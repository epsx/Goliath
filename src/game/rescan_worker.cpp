#include "rescan_worker.hpp"
#include "common/paths.hpp"
#include "db_scanner.hpp"
#include "game/game_model.hpp"
#include "game/library_rescan_comparison.hpp"

#include <utility>

namespace goliath {

RescanWorker::RescanWorker(Config config, std::vector<Game> previousGames,
                           QObject* parent)
    : QThread(parent),
      m_config(std::move(config)),
      m_previousGames(std::move(previousGames)) {
    qRegisterMetaType<RescanReport>();
}

void RescanWorker::run() {
    auto callback = [this](const std::string& line) {
        if (m_stop) return;
        emit outputLine(QString::fromStdString(line));
    };

    ScanResult result = scan_roms(m_config, callback, &m_stop);

    if (m_stop) {
        // User cancellation exits silently without completion or error signals.
        return;
    }

    if (result.success) {
        const std::vector<Game> currentGames =
            load_games(compute_app_paths(m_config).json_file);
        RescanReport report;
        report.scan = std::move(result);
        report.comparison = compare_library_snapshots(
            m_previousGames, currentGames);
        emit completed(std::move(report));
    } else {
        emit error(QString::fromStdString(result.error_message));
    }
}

} // namespace goliath
