// bios_verify.hpp — checks the bios/ folder against the known Neo Geo/
// Geolith BIOS zip archives (bios_sets()): confirms each required archive
// exists and, where a CRC32 is known, that the decompressed member matches
// it. Uses miniz to read zip members and recompute CRC32 over the
// decompressed bytes.
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace goliath {

struct BiosFileEntry {
    std::string member;
    std::optional<std::string> expected_crc; // nullopt = only check presence
    std::string description;
    // Optional members unlock an additional BIOS mode without invalidating
    // another usable mode provided by the same archive.
    bool required = true;
};

struct BiosSet {
    std::string zip_name;
    bool required;
    std::string purpose;
    std::vector<BiosFileEntry> files; // in the exact order geolith documents them
};

// The 5 known BIOS archives, in display order.
const std::vector<BiosSet>& bios_sets();

enum class BiosMemberStatus {
    Verified,
    Present,
    Missing,
    MissingOptional,
    ReadError,
    CrcMismatch,
};

struct BiosMemberVerification {
    std::string member;
    std::string description;
    std::optional<std::string> expected_crc;
    std::optional<std::string> actual_crc;
    bool required = true;
    BiosMemberStatus status = BiosMemberStatus::Missing;
};

enum class BiosArchiveStatus {
    Ready,
    MissingRequired,
    MissingOptional,
    InvalidArchive,
    MemberProblems,
};

struct BiosArchiveVerification {
    std::string zip_name;
    std::string purpose;
    bool required = false;
    BiosArchiveStatus status = BiosArchiveStatus::MissingOptional;
    std::vector<BiosMemberVerification> members;
    int problems = 0;
    std::size_t optional_missing_members = 0;
};

struct BiosVerifyResult {
    // Kept for the exportable diagnostic report and command-line support.
    std::vector<std::string> lines;
    // Structured data is authoritative for the interactive BIOS dialog.
    std::vector<BiosArchiveVerification> archives;
    int problems = 0;
    std::size_t ready_archives = 0;
    std::size_t optional_missing_archives = 0;
    std::size_t optional_missing_members = 0;
    std::size_t checked_members = 0;
};

// Returns nullopt if bios_dir itself doesn't exist (caller should show its
// own "BIOS folder not found" message).
std::optional<BiosVerifyResult> verify_bios(const std::filesystem::path& bios_dir);

} // namespace goliath
