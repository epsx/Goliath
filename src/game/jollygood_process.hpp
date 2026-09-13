// jollygood_process.hpp — launches jollygood as a *detached* process so it
// keeps running even if the launcher window is closed. stdout and stderr are
// merged before being appended through one log destination, preventing
// competing detached file handles from corrupting jollygood.log.
#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace goliath {

// Returns true if the process was started. args[0] is the executable. When
// pid is non-null, receives the detached process id (used on Windows to apply
// Goliath's per-session volume without modifying JGRF). The output path must
// be missing or a direct regular file; error receives a refusal/start detail.
bool launch_jollygood_detached(const QStringList& args, const QString& cwd,
                               const QProcessEnvironment& env, const QString& logPath,
                               qint64* pid = nullptr,
                               QString* error = nullptr);

} // namespace goliath
