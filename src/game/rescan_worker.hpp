// Runs the integrated ROM scanner outside the UI thread and forwards its output
// through Qt signals so RescanDialog can show live progress and the final
// summary while database/games.json is rebuilt.
#pragma once

#include "game/db_scanner.hpp"
#include "game/library_rescan_comparison.hpp"

#include <QMetaType>
#include <QString>
#include <QThread>

#include <atomic>
#include <vector>

#include "common/goliath_common.hpp"

namespace goliath {

struct RescanReport {
    ScanResult scan;
    LibraryRescanComparison comparison;
    std::vector<LibraryVerificationIssue> verification_issues;
};

class RescanWorker : public QThread {
    Q_OBJECT
public:
    explicit RescanWorker(Config config, std::vector<Game> previousGames,
                          QObject* parent = nullptr);

    // Ask the worker to stop the scan and exit the run() loop.
    void stop() { m_stop.store(true, std::memory_order_release); }

signals:
    void outputLine(QString line);
    void completed(goliath::RescanReport report);
    void error(QString message);

protected:
    void run() override;

private:
    Config m_config;
    std::vector<Game> m_previousGames;
    std::atomic<bool> m_stop{false};
};

} // namespace goliath

Q_DECLARE_METATYPE(goliath::RescanReport)
