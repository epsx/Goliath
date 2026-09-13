// save_data_manager.hpp — safe discovery, backup, restore, and deletion of
// the save-state and persistent-data files owned by JGRF/Geolith.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace goliath {

struct AppPaths;

enum class JgrfSaveLayout {
    DynamicCore,
    StaticCore,
};

struct JgrfSavePaths {
    JgrfSaveLayout layout = JgrfSaveLayout::DynamicCore;
    std::filesystem::path root;
    std::filesystem::path state_directory;
    std::filesystem::path save_directory;
};

enum class ManagedSaveKind {
    StateSlot0,
    StateSlot1,
    Nvram,
    CartridgeRam,
    MemoryCard,
    CdBackupRam,
    Dips,
};

struct ManagedSaveFile {
    ManagedSaveKind kind = ManagedSaveKind::StateSlot0;
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::filesystem::file_time_type modified{};
};

struct SaveBackupRecord {
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::filesystem::file_time_type modified{};
    std::size_t file_count = 0;
    bool compatible = false;
    std::string problem;
};

struct SaveDataOperationResult {
    bool success = false;
    std::string error;
    std::filesystem::path backup_path;
    std::optional<std::filesystem::path> protective_backup;
    std::size_t file_count = 0;
};

const char* jgrf_save_layout_name(JgrfSaveLayout layout);
const char* managed_save_kind_name(ManagedSaveKind kind);

// Matches JGRF's fixed char gamename[128] behavior: select the final path
// component, truncate to 127 bytes, and only then strip the final extension.
std::string jgrf_game_name(std::string_view media);

JgrfSavePaths resolve_jgrf_save_paths(const AppPaths& paths);

std::filesystem::path managed_save_file_path(
    const JgrfSavePaths& paths,
    std::string_view game_name,
    ManagedSaveKind kind);

std::filesystem::path game_save_backup_directory(
    const AppPaths& paths,
    const std::string& system,
    const std::string& media);

std::vector<ManagedSaveFile> list_game_save_data(
    const AppPaths& paths,
    const std::string& media,
    std::string* error = nullptr);

std::vector<SaveBackupRecord> list_game_save_backups(
    const AppPaths& paths,
    const std::string& system,
    const std::string& media,
    std::string* error = nullptr);

// Creates a manual exact-media ZIP snapshot. No archive is created when the
// game has no current state or persistent-data files.
SaveDataOperationResult create_game_save_backup(
    const AppPaths& paths,
    const std::string& system,
    const std::string& media);

// Restores a validated exact-media archive. Existing data is snapshotted first
// and all archive payloads are validated before any live file is replaced.
SaveDataOperationResult restore_game_save_backup(
    const AppPaths& paths,
    const std::string& system,
    const std::string& media,
    const std::filesystem::path& backup_path);

// Deletes only one of the seven known files for this JGRF game name. A
// protective snapshot is created before deletion.
SaveDataOperationResult delete_game_save_data(
    const AppPaths& paths,
    const std::string& system,
    const std::string& media,
    ManagedSaveKind kind);

// Deletes only a regular ZIP file directly inside this exact-media backup
// directory. Symlinks and arbitrary paths are rejected.
SaveDataOperationResult delete_game_save_backup(
    const AppPaths& paths,
    const std::string& system,
    const std::string& media,
    const std::filesystem::path& backup_path);

} // namespace goliath
