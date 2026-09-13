#include "catch2/catch.hpp"

#include "common/debug_logger.hpp"
#include "common/filesystem_safety.hpp"
#include "common/log_file.hpp"
#include "game/jollygood_process.hpp"

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#if defined(_WIN32)
#include <QProcess>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

using namespace goliath;

namespace {

fs::path make_log_test_root(const std::string& name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_log_" + name + "_" + std::to_string(stamp));
    std::error_code ec;
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);
    return root;
}

void write_log(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << text;
    REQUIRE(output.good());
}

std::string read_log(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

bool create_directory_link(const fs::path& link,
                           const fs::path& target) {
#if defined(_WIN32)
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(
        "cmd.exe",
        {"/d", "/c", "mklink", "/J",
         QString::fromStdWString(link.wstring()),
         QString::fromStdWString(target.wstring())});
    return process.waitForStarted(5000) && process.waitForFinished(5000) &&
           process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
#else
    std::error_code ec;
    fs::create_directory_symlink(target, link, ec);
    return !ec;
#endif
}

bool remove_directory_link(const fs::path& link) {
#if defined(_WIN32)
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(
        "cmd.exe",
        {"/d", "/c", "rmdir",
         QString::fromStdWString(link.wstring())});
    return process.waitForStarted(5000) && process.waitForFinished(5000) &&
           process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
#else
    std::error_code ec;
    return fs::remove(link, ec) && !ec;
#endif
}

} // namespace

TEST_CASE("direct log preparation preserves data and append keeps byte order",
          "[logging][filesystem][safety]") {
    const fs::path root = make_log_test_root("prepare_append");
    const fs::path log = root / "jollygood.log";

    const LogFileOperationResult prepared = prepare_regular_log_file(log);
    REQUIRE(prepared.success);
    REQUIRE_FALSE(prepared.existed);
    REQUIRE(inspect_direct_path(log).kind == DirectPathKind::RegularFile);
    REQUIRE(fs::file_size(log) == 0);

    const LogFileOperationResult first =
        append_regular_log_file(
            log, std::string_view("first\0chunk\n", 12));
    REQUIRE(first.success);
    REQUIRE(first.existed);
    REQUIRE(first.error.empty());

    const LogFileOperationResult second =
        append_regular_log_file(log, "second chunk\n");
    REQUIRE(second.success);
    REQUIRE(second.existed);
    REQUIRE(read_log(log) ==
            std::string("first\0chunk\n", 12) + "second chunk\n");

    const LogFileOperationResult preserved = prepare_regular_log_file(log);
    REQUIRE(preserved.success);
    REQUIRE(preserved.existed);
    REQUIRE(read_log(log) ==
            std::string("first\0chunk\n", 12) + "second chunk\n");

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("regular log clearing is idempotent and truncates existing data",
          "[logging][clear]") {
    const fs::path root = make_log_test_root("truncate");
    const fs::path log = root / "jollygood.log";

    const LogFileClearResult missing = clear_regular_log_file(log);
    REQUIRE(missing.success);
    REQUIRE_FALSE(missing.existed);
    REQUIRE_FALSE(fs::exists(log));

    write_log(log, "verbose line\nsecond line\n");
    REQUIRE(fs::file_size(log) > 0);

    const LogFileClearResult cleared = clear_regular_log_file(log);
    REQUIRE(cleared.success);
    REQUIRE(cleared.existed);
    REQUIRE(fs::is_regular_file(log));
    REQUIRE(fs::file_size(log) == 0);

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("log clearing refuses empty and non-regular targets",
          "[logging][clear][safety]") {
    const LogFileClearResult empty = clear_regular_log_file({});
    REQUIRE_FALSE(empty.success);
    REQUIRE_FALSE(empty.error.empty());

    const fs::path root = make_log_test_root("refuse");
    const LogFileClearResult directory = clear_regular_log_file(root);
    REQUIRE_FALSE(directory.success);
    REQUIRE(directory.existed);
    REQUIRE_FALSE(directory.error.empty());
    REQUIRE(fs::is_directory(root));

    const LogFileOperationResult prepared = prepare_regular_log_file(root);
    REQUIRE_FALSE(prepared.success);
    REQUIRE(prepared.existed);
    REQUIRE_FALSE(prepared.error.empty());

    const LogFileOperationResult appended =
        append_regular_log_file(root, "must not be written");
    REQUIRE_FALSE(appended.success);
    REQUIRE(appended.existed);
    REQUIRE_FALSE(appended.error.empty());

    QString frontendError;
    REQUIRE_FALSE(DebugLogger::initialize(
        path_to_qstring(root), &frontendError));
    REQUIRE_FALSE(frontendError.isEmpty());

    QString launchError;
    REQUIRE_FALSE(launch_jollygood_detached(
        {"must-not-start.exe"},
        path_to_qstring(root),
        QProcessEnvironment::systemEnvironment(),
        path_to_qstring(root),
        nullptr,
        &launchError));
    REQUIRE_FALSE(launchError.isEmpty());
    REQUIRE(launchError.contains("direct regular file"));

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}

TEST_CASE("all log writers refuse a substituted direct target",
          "[logging][filesystem][link][safety]") {
    const fs::path root = make_log_test_root("substituted");
    const fs::path outside = root / "outside";
    const fs::path sentinel = outside / "user-owned.log";
    const fs::path log = root / "jollygood.log";

    fs::create_directories(outside);
    REQUIRE(fs::is_directory(outside));
    write_log(sentinel, "keep external bytes\n");
    REQUIRE(create_directory_link(log, outside));
    REQUIRE(inspect_direct_path(log).kind == DirectPathKind::Other);

    const LogFileOperationResult prepared = prepare_regular_log_file(log);
    REQUIRE_FALSE(prepared.success);
    REQUIRE(prepared.existed);
    REQUIRE_FALSE(prepared.error.empty());

    const LogFileOperationResult appended =
        append_regular_log_file(log, "must not escape\n");
    REQUIRE_FALSE(appended.success);
    REQUIRE(appended.existed);
    REQUIRE_FALSE(appended.error.empty());

    const LogFileClearResult cleared = clear_regular_log_file(log);
    REQUIRE_FALSE(cleared.success);
    REQUIRE(cleared.existed);
    REQUIRE_FALSE(cleared.error.empty());

    QString frontendError;
    REQUIRE_FALSE(DebugLogger::initialize(
        path_to_qstring(log), &frontendError));
    REQUIRE_FALSE(frontendError.isEmpty());

    QString launchError;
    REQUIRE_FALSE(launch_jollygood_detached(
        {"must-not-start.exe"},
        path_to_qstring(root),
        QProcessEnvironment::systemEnvironment(),
        path_to_qstring(log),
        nullptr,
        &launchError));
    REQUIRE(launchError.contains("direct regular file"));
    REQUIRE(read_log(sentinel) == "keep external bytes\n");

    REQUIRE(remove_directory_link(log));
    REQUIRE_FALSE(fs::exists(log));
    REQUIRE(read_log(sentinel) == "keep external bytes\n");

    std::error_code ec;
    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}
