#include "catch2/catch.hpp"

#include "common/filesystem_safety.hpp"
#include "common/paths.hpp"
#include "game/filesystem_io.hpp"
#include "game/game_profile.hpp"
#include "game/save_data_manager.hpp"
#include "json.hpp"
#include "miniz.h"

#if defined(_WIN32)
#include <QProcess>
#include <QString>
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;

using goliath::AppPaths;
using goliath::DirectPathKind;
using goliath::JgrfSaveLayout;
using goliath::ManagedSaveKind;
using goliath::create_game_save_backup;
using goliath::delete_game_save_backup;
using goliath::delete_game_save_data;
using goliath::exact_media_storage_id;
using goliath::game_save_backup_directory;
using goliath::jgrf_game_name;
using goliath::list_game_save_backups;
using goliath::list_game_save_data;
using goliath::managed_save_file_path;
using goliath::resolve_jgrf_save_paths;
using goliath::inspect_direct_path;
using goliath::restore_game_save_backup;

namespace {

fs::path make_save_test_root(const std::string& name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_save_data_" + name + "_" + std::to_string(stamp));
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void write_bytes(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string read_bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

AppPaths make_paths(const fs::path& root, bool dynamic_core = true) {
    AppPaths paths;
    paths.base_dir = root / "runtime";
    paths.data_dir = root / "data";
    paths.config_dir = root / "config";
    paths.jollygood_exe = paths.base_dir / "jollygood.exe";
    paths.jollygood_args = dynamic_core
        ? std::vector<std::string>{"-c", "geolith"}
        : std::vector<std::string>{};
    paths.save_backup_dir = paths.data_dir / "goliath" / "save_backups";
    fs::create_directories(paths.base_dir);
    write_bytes(paths.jollygood_exe, "test executable placeholder");
    return paths;
}

fs::path save_path(const AppPaths& paths,
                   const std::string& media,
                   ManagedSaveKind kind) {
    return managed_save_file_path(
        resolve_jgrf_save_paths(paths), jgrf_game_name(media), kind);
}

bool write_zip(const fs::path& path,
               const std::vector<std::pair<std::string, std::string>>& members) {
    mz_zip_archive archive{};
    mz_zip_zero_struct(&archive);
    if (!mz_zip_writer_init_heap(&archive, 0, 4096)) return false;

    for (const auto& [name, contents] : members) {
        if (!mz_zip_writer_add_mem(&archive, name.c_str(), contents.data(),
                                   contents.size(), MZ_BEST_SPEED)) {
            mz_zip_writer_end(&archive);
            return false;
        }
    }

    void* memory = nullptr;
    std::size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&archive, &memory, &size)) {
        mz_zip_writer_end(&archive);
        return false;
    }
    if (!mz_zip_writer_end(&archive) || !memory || size == 0) {
        if (memory) mz_free(memory);
        return false;
    }

    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(static_cast<const char*>(memory),
                 static_cast<std::streamsize>(size));
    mz_free(memory);
    return static_cast<bool>(output);
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

TEST_CASE("JGRF game names match its fixed 128-byte buffer semantics",
          "[save_data][identity]") {
    CHECK(jgrf_game_name("sets/kof98.neo") == "kof98");
    CHECK(jgrf_game_name("sets\\last.blade.cue") == "last.blade");
    CHECK(jgrf_game_name(".hidden") == ".hidden");

    const std::string longName(140, 'a');
    CHECK(jgrf_game_name(longName + ".neo") == std::string(127, 'a'));

    const std::string boundaryName(124, 'b');
    CHECK(jgrf_game_name(boundaryName + ".neo") == boundaryName);
}

TEST_CASE("save paths distinguish dynamic and static JGRF layouts",
          "[save_data][paths]") {
    const fs::path root = make_save_test_root("layouts");

    AppPaths dynamic = make_paths(root / "dynamic", true);
    fs::create_directories(dynamic.base_dir / "state");
    const auto dynamicPaths = resolve_jgrf_save_paths(dynamic);
    CHECK(dynamicPaths.layout == JgrfSaveLayout::DynamicCore);
    CHECK(dynamicPaths.state_directory ==
          dynamic.data_dir / "jollygood" / "state" / "geolith");
    CHECK(dynamicPaths.save_directory ==
          dynamic.data_dir / "jollygood" / "save" / "geolith");

    AppPaths staticCore = make_paths(root / "static", false);
#if defined(_WIN32)
    fs::create_directories(staticCore.base_dir / "state");
    fs::create_directories(staticCore.base_dir / "save");
#else
    fs::create_directories(staticCore.data_dir / "geolith" / "state");
    fs::create_directories(staticCore.data_dir / "geolith" / "save");
#endif
    const auto staticPaths = resolve_jgrf_save_paths(staticCore);
    CHECK(staticPaths.layout == JgrfSaveLayout::StaticCore);
#if defined(_WIN32)
    CHECK(staticPaths.state_directory == staticCore.base_dir / "state");
    CHECK(staticPaths.save_directory == staticCore.base_dir / "save");
#else
    CHECK(staticPaths.state_directory ==
          staticCore.data_dir / "geolith" / "state");
    CHECK(staticPaths.save_directory ==
          staticCore.data_dir / "geolith" / "save");
#endif

    AppPaths detectedDynamic = make_paths(root / "detected", false);
    fs::create_directories(
        detectedDynamic.data_dir / "jollygood" / "save" / "geolith");
    CHECK(resolve_jgrf_save_paths(detectedDynamic).layout ==
          JgrfSaveLayout::DynamicCore);

    AppPaths freshStatic = make_paths(root / "fresh-static", false);
    CHECK(resolve_jgrf_save_paths(freshStatic).layout ==
          JgrfSaveLayout::StaticCore);

    fs::remove_all(root);
}

TEST_CASE("inventory accepts only JGRF slots and Geolith save extensions",
          "[save_data][inventory]") {
    const fs::path root = make_save_test_root("inventory");
    const AppPaths paths = make_paths(root);
    const std::string media = "sets/kof98.neo";

    const std::vector<ManagedSaveKind> kinds = {
        ManagedSaveKind::StateSlot0,
        ManagedSaveKind::StateSlot1,
        ManagedSaveKind::Nvram,
        ManagedSaveKind::CartridgeRam,
        ManagedSaveKind::MemoryCard,
        ManagedSaveKind::CdBackupRam,
        ManagedSaveKind::Dips,
    };
    for (ManagedSaveKind kind : kinds)
        write_bytes(save_path(paths, media, kind), "payload");
    write_bytes(resolve_jgrf_save_paths(paths).state_directory / "kof98.st2",
                "unsupported slot");
    write_bytes(resolve_jgrf_save_paths(paths).save_directory / "other.nv",
                "other game");

    std::string error;
    const auto files = list_game_save_data(paths, media, &error);
    CHECK(error.empty());
    REQUIRE(files.size() == 7);
    for (ManagedSaveKind kind : kinds) {
        CHECK(std::any_of(files.begin(), files.end(),
                          [kind](const auto& file) {
                              return file.kind == kind;
                          }));
    }

    fs::remove_all(root);
}

TEST_CASE("manual backup and restore round-trip an exact snapshot",
          "[save_data][backup][restore]") {
    const fs::path root = make_save_test_root("roundtrip");
    const AppPaths paths = make_paths(root);
    const std::string media = "sets/mslug3.neo";
    const fs::path state0 =
        save_path(paths, media, ManagedSaveKind::StateSlot0);
    const fs::path nvram = save_path(paths, media, ManagedSaveKind::Nvram);
    const fs::path card = save_path(paths, media, ManagedSaveKind::MemoryCard);
    const fs::path dips = save_path(paths, media, ManagedSaveKind::Dips);
    write_bytes(state0, "original state");
    write_bytes(nvram, "original nvram");
    write_bytes(card, "original card");

    const auto backup = create_game_save_backup(paths, "neogeo", media);
    INFO(backup.error);
    REQUIRE(backup.success);
    CHECK(backup.file_count == 3);
    CHECK(fs::is_regular_file(backup.backup_path));

    write_bytes(state0, "changed state");
    fs::remove(nvram);
    write_bytes(dips, "new dips");

    const auto restored = restore_game_save_backup(
        paths, "neogeo", media, backup.backup_path);
    REQUIRE(restored.success);
    CHECK(restored.file_count == 3);
    REQUIRE(restored.protective_backup.has_value());
    CHECK(fs::is_regular_file(*restored.protective_backup));
    CHECK(read_bytes(state0) == "original state");
    CHECK(read_bytes(nvram) == "original nvram");
    CHECK(read_bytes(card) == "original card");
    CHECK_FALSE(fs::exists(dips));

    std::string error;
    const auto backups = list_game_save_backups(
        paths, "neogeo", media, &error);
    CHECK(error.empty());
    REQUIRE(backups.size() == 2);
    CHECK(backups[0].compatible);
    CHECK(backups[1].compatible);

    fs::remove_all(root);
}

TEST_CASE("backups remain isolated for exact media sharing one JGRF name",
          "[save_data][backup][identity]") {
    const fs::path root = make_save_test_root("isolation");
    const AppPaths paths = make_paths(root);
    const std::string mediaA = "collection-a/game.neo";
    const std::string mediaB = "collection-b/game.neo";
    write_bytes(save_path(paths, mediaA, ManagedSaveKind::Nvram), "shared");

    const auto backupA = create_game_save_backup(paths, "neogeo", mediaA);
    const auto backupB = create_game_save_backup(paths, "neogeo", mediaB);
    INFO(backupA.error);
    REQUIRE(backupA.success);
    INFO(backupB.error);
    REQUIRE(backupB.success);
    CHECK(backupA.backup_path.parent_path() !=
          backupB.backup_path.parent_path());
    CHECK(backupA.backup_path.parent_path().filename() ==
          exact_media_storage_id("neogeo", mediaA));
    CHECK(backupB.backup_path.parent_path().filename() ==
          exact_media_storage_id("neogeo", mediaB));

    fs::remove_all(root);
}

TEST_CASE("restore rejects archives for another exact media identity",
          "[save_data][restore][validation]") {
    const fs::path root = make_save_test_root("wrong_identity");
    const AppPaths paths = make_paths(root);
    const std::string sourceMedia = "set-a/game.neo";
    const std::string targetMedia = "set-b/game.neo";
    write_bytes(save_path(paths, sourceMedia, ManagedSaveKind::Nvram),
                "source save");

    const auto source = create_game_save_backup(
        paths, "neogeo", sourceMedia);
    INFO(source.error);
    REQUIRE(source.success);
    const fs::path copied =
        game_save_backup_directory(paths, "neogeo", targetMedia) /
        "copied.zip";
    fs::create_directories(copied.parent_path());
    fs::copy_file(source.backup_path, copied);

    const auto result = restore_game_save_backup(
        paths, "neogeo", targetMedia, copied);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("different exact media") != std::string::npos);
    CHECK(read_bytes(save_path(
              paths, targetMedia, ManagedSaveKind::Nvram)) == "source save");

    fs::remove_all(root);
}

TEST_CASE("unsafe ZIP members are rejected without filesystem traversal",
          "[save_data][restore][security]") {
    const fs::path root = make_save_test_root("traversal");
    const AppPaths paths = make_paths(root);
    const std::string media = "sets/kof98.neo";
    const fs::path directory =
        game_save_backup_directory(paths, "neogeo", media);
    const fs::path archive = directory / "unsafe.zip";
    const json manifest = {
        {"version", 1},
        {"system", "neogeo"},
        {"media", media},
        {"jgrf_game_name", "kof98"},
        {"files", json::array({{
            {"name", "../escaped.st0"},
            {"kind", "state-slot-0"},
            {"size", 4},
        }})},
    };
    REQUIRE(write_zip(archive, {
        {"manifest.json", manifest.dump()},
        {"../escaped.st0", "evil"},
    }));

    const auto result = restore_game_save_backup(
        paths, "neogeo", media, archive);
    CHECK_FALSE(result.success);
    CHECK_FALSE(fs::exists(directory.parent_path() / "escaped.st0"));

    std::string error;
    const auto backups = list_game_save_backups(
        paths, "neogeo", media, &error);
    CHECK(error.empty());
    REQUIRE(backups.size() == 1);
    CHECK_FALSE(backups[0].compatible);

    fs::remove_all(root);
}

TEST_CASE("managed deletion is protected and arbitrary archive paths fail",
          "[save_data][delete][security]") {
    const fs::path root = make_save_test_root("delete");
    const AppPaths paths = make_paths(root);
    const std::string media = "sets/lastblad.neo";
    const fs::path state1 =
        save_path(paths, media, ManagedSaveKind::StateSlot1);
    write_bytes(state1, "slot one");

    const auto deleted = delete_game_save_data(
        paths, "neogeo", media, ManagedSaveKind::StateSlot1);
    INFO(deleted.error);
    REQUIRE(deleted.success);
    REQUIRE(deleted.protective_backup.has_value());
    CHECK_FALSE(fs::exists(state1));
    CHECK(fs::is_regular_file(*deleted.protective_backup));

    const auto restored = restore_game_save_backup(
        paths, "neogeo", media, *deleted.protective_backup);
    REQUIRE(restored.success);
    CHECK(read_bytes(state1) == "slot one");

    const fs::path outside = root / "outside.zip";
    write_bytes(outside, "not a zip");
    const auto refused = delete_game_save_backup(
        paths, "neogeo", media, outside);
    CHECK_FALSE(refused.success);
    CHECK(fs::is_regular_file(outside));

    const auto removed = delete_game_save_backup(
        paths, "neogeo", media, *deleted.protective_backup);
    CHECK(removed.success);
    CHECK_FALSE(fs::exists(*deleted.protective_backup));

    const std::string blockedMedia = "sets/blocked.neo";
    write_bytes(save_path(paths, blockedMedia, ManagedSaveKind::Nvram),
                "blocked save");
    const fs::path blockedDirectory =
        game_save_backup_directory(paths, "neogeo", blockedMedia);
    write_bytes(blockedDirectory, "not a directory");
    const auto blocked = create_game_save_backup(
        paths, "neogeo", blockedMedia);
    CHECK_FALSE(blocked.success);
    CHECK(blocked.error.find("not a plain directory") != std::string::npos);
    CHECK(fs::is_regular_file(blockedDirectory));

    fs::remove_all(root);
}

TEST_CASE("save data operations refuse a substituted live JGRF directory",
          "[save_data][filesystem][safety]") {
    const fs::path root = make_save_test_root("live_directory_link");
    const AppPaths paths = make_paths(root);
    const std::string media = "sets/mslug3.neo";
    const fs::path original =
        save_path(paths, media, ManagedSaveKind::MemoryCard);
    write_bytes(original, "archived memory card");

    const auto backup = create_game_save_backup(paths, "neogeo", media);
    INFO(backup.error);
    REQUIRE(backup.success);
    REQUIRE(fs::is_regular_file(backup.backup_path));

    const auto livePaths = resolve_jgrf_save_paths(paths);
    const fs::path saveBranch = livePaths.root / "save";
    const fs::path outside = root / "external-save-target";
    const fs::path externalFile =
        outside / "geolith" / original.filename();
    const std::string externalBytes = "keep external save bytes";

    std::error_code ec;
    fs::remove_all(saveBranch, ec);
    REQUIRE_FALSE(ec);
    write_bytes(externalFile, externalBytes);
    REQUIRE(create_directory_link(saveBranch, outside));
    REQUIRE(inspect_direct_path(saveBranch).kind == DirectPathKind::Other);

    std::string listError;
    const auto files = list_game_save_data(paths, media, &listError);
    REQUIRE(files.empty());
    REQUIRE(listError.find("not a direct directory") != std::string::npos);

    const auto created = create_game_save_backup(paths, "neogeo", media);
    REQUIRE_FALSE(created.success);
    REQUIRE(created.error.find("not a direct directory") != std::string::npos);

    const auto restored = restore_game_save_backup(
        paths, "neogeo", media, backup.backup_path);
    REQUIRE_FALSE(restored.success);
    REQUIRE(restored.error.find("not a direct directory") != std::string::npos);

    const auto deleted = delete_game_save_data(
        paths, "neogeo", media, ManagedSaveKind::MemoryCard);
    REQUIRE_FALSE(deleted.success);
    REQUIRE(deleted.error.find("not a direct directory") != std::string::npos);

    REQUIRE(read_bytes(externalFile) == externalBytes);
    REQUIRE(fs::is_regular_file(backup.backup_path));
    REQUIRE(remove_directory_link(saveBranch));
    REQUIRE_FALSE(fs::exists(saveBranch));
    REQUIRE(read_bytes(externalFile) == externalBytes);

    fs::remove_all(root, ec);
    REQUIRE_FALSE(ec);
}
