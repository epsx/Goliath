#include "rescan_worker.hpp"
#include "db_scanner.hpp"

#include <utility>

namespace goliath {

RescanWorker::RescanWorker(Config config, QObject* parent)
    : QThread(parent), m_config(std::move(config)) {}

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
        emit completed();
    } else {
        emit error(QString::fromStdString(result.error_message));
    }
}

} // namespace goliath
