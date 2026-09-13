#include "jollygood_process.hpp"

#include "common/log_file.hpp"

#include <QProcess>

#include <filesystem>

namespace goliath {

bool launch_jollygood_detached(const QStringList& args, const QString& cwd,
                               const QProcessEnvironment& env, const QString& logPath,
                               qint64* pid, QString* error) {
    if (error) error->clear();
    if (args.isEmpty()) {
        if (error) *error = "The JGRF command is empty.";
        return false;
    }

#if defined(_WIN32)
    const std::filesystem::path directLogPath(logPath.toStdWString());
#else
    const std::filesystem::path directLogPath(logPath.toStdString());
#endif
    const LogFileOperationResult logReady =
        prepare_regular_log_file(directLogPath);
    if (!logReady.success) {
        if (error) *error = QString::fromStdString(logReady.error);
        return false;
    }

    QProcess process;
    process.setProgram(args.first());
    process.setArguments(args.mid(1));
    process.setWorkingDirectory(cwd);
    process.setProcessEnvironment(env);
    process.setStandardInputFile(QProcess::nullDevice());
    // JGRF writes informational output to stdout and verbose/core output to
    // stderr. Merge them before redirection so the detached child inherits one
    // append destination. Two independent file redirections can race and
    // overwrite/interleave bytes in jollygood.log on Windows.
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setStandardOutputFile(logPath, QIODevice::Append);

    if (!process.startDetached(pid)) {
        if (error) *error = process.errorString();
        return false;
    }
    return true;
}

} // namespace goliath
