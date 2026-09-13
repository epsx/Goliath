// Runs the integrated ROM scanner outside the UI thread and forwards its output
// through Qt signals so RescanDialog can show live progress and the final
// summary while database/games.json is rebuilt.
#pragma once

#include <QString>
#include <QThread>

#include <atomic>

#include "common/goliath_common.hpp"

namespace goliath {

class RescanWorker : public QThread {
    Q_OBJECT
public:
    explicit RescanWorker(Config config, QObject* parent = nullptr);

    // Ask the worker to stop the scan and exit the run() loop.
    void stop() { m_stop.store(true, std::memory_order_release); }

signals:
    void outputLine(QString line);
    void completed();
    void error(QString message);

protected:
    void run() override;

private:
    Config m_config;
    std::atomic<bool> m_stop{false};
};

} // namespace goliath
