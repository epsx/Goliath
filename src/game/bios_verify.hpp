// bios_verify.hpp — checks the bios/ folder against the known Neo Geo/
// Geolith BIOS zip archives (bios_sets()): confirms each required archive
// exists and, where a CRC32 is known, that the decompressed member matches
// it. Uses miniz to read zip members and recompute CRC32 over the
// decompressed bytes.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace goliath {

struct BiosFileEntry {
    std::string member;
    std::optional<std::string> expected_crc; // nullopt = only check presence (geolith CD BIOS entries)
    std::string description;
};

struct BiosSet {
    std::string zip_name;
    bool required;
    std::string purpose;
    std::vector<BiosFileEntry> files; // in the exact order geolith documents them
};

// The 5 known BIOS archives, in display order.
const std::vector<BiosSet>& bios_sets();

struct BiosVerifyResult {
    std::vector<std::string> lines; // pre-formatted report lines, one bios_sets() entry after another
    int problems = 0;
};

// Returns nullopt if bios_dir itself doesn't exist (caller should show its
// own "BIOS folder not found" message).
std::optional<BiosVerifyResult> verify_bios(const std::filesystem::path& bios_dir);

} // namespace goliath
