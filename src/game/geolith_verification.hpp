#pragma once

#include "neogeo_metadata.hpp"

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace goliath {

using GeolithCrcCatalog = std::unordered_map<std::string, std::string>;

struct GeolithCrcVerification {
    std::string expected_crc32;
    std::string actual_crc32;

    bool matches() const noexcept {
        return expected_crc32 == actual_crc32;
    }
};

// Loads metadata/geolith.xml. Catalog keys and CRC-32 values are normalized
// to lower case so Windows and Linux filename spelling behaves identically.
GeolithCrcCatalog load_geolith_crc_catalog(
    const std::filesystem::path& xml_path,
    MetadataProgressCallback callback = nullptr);

// Returns no value when the filename is absent from the catalog, the file
// cannot be read, or cancellation is requested. A returned result always
// contains both the catalog CRC-32 and the CRC-32 calculated from the .neo.
std::optional<GeolithCrcVerification> verify_geolith_neo(
    const std::filesystem::path& neo_path,
    const std::string& catalog_filename,
    const GeolithCrcCatalog& catalog,
    const std::atomic<bool>* cancel = nullptr);

} // namespace goliath
