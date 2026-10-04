#include "geolith_verification.hpp"

#include "crc32_cache.hpp"
#include "tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace fs = std::filesystem;

namespace goliath {
namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return value;
}

bool valid_crc32(const std::string& value) {
    return value.size() == 8 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::isxdigit(c) != 0;
           });
}

} // namespace

GeolithCrcCatalog load_geolith_crc_catalog(
    const fs::path& xml_path,
    MetadataProgressCallback callback) {
    GeolithCrcCatalog catalog;

    std::error_code ec;
    if (!fs::is_regular_file(xml_path, ec)) {
        if (callback) {
            callback("Geolith CRC-32 catalog not found: " +
                     xml_path.string() + "\n");
        }
        return catalog;
    }

    tinyxml2::XMLDocument document;
    if (document.LoadFile(xml_path.string().c_str()) !=
        tinyxml2::XML_SUCCESS) {
        if (callback) {
            callback("Warning: could not parse " + xml_path.string() +
                     "\n");
        }
        return catalog;
    }

    tinyxml2::XMLElement* root = document.RootElement();
    if (!root || !root->Name() ||
        std::string(root->Name()) != "geolith-metadata") {
        if (callback) {
            callback("Warning: invalid Geolith metadata root in " +
                     xml_path.string() + "\n");
        }
        return catalog;
    }

    tinyxml2::XMLElement* neo_catalog =
        root->FirstChildElement("neo-catalog");
    const char* algorithm = neo_catalog
        ? neo_catalog->Attribute("algorithm")
        : nullptr;
    if (!neo_catalog || !algorithm ||
        to_lower(algorithm) != "crc32") {
        if (callback) {
            callback("Warning: geolith.xml does not contain a CRC-32 "
                     "Neo Geo catalog\n");
        }
        return catalog;
    }

    for (tinyxml2::XMLElement* rom =
             neo_catalog->FirstChildElement("rom");
         rom;
         rom = rom->NextSiblingElement("rom")) {
        const char* file_attribute = rom->Attribute("file");
        const char* crc_attribute = rom->Attribute("crc32");
        if (!file_attribute || !*file_attribute ||
            !crc_attribute || !*crc_attribute) {
            continue;
        }

        const std::string file = to_lower(file_attribute);
        const std::string crc = to_lower(crc_attribute);
        if (fs::path(file).extension().string() != ".neo" ||
            !valid_crc32(crc)) {
            continue;
        }
        catalog.try_emplace(file, crc);
    }

    if (callback) {
        callback("Geolith CRC-32 entries: " +
                 std::to_string(catalog.size()) + "\n");
    }
    return catalog;
}

std::optional<GeolithCrcVerification> verify_geolith_neo(
    const fs::path& neo_path,
    const std::string& catalog_filename,
    const GeolithCrcCatalog& catalog,
    const std::atomic<bool>* cancel,
    Crc32Cache* cache) {
    const auto expected = catalog.find(to_lower(catalog_filename));
    if (expected == catalog.end())
        return std::nullopt;

    if (cancel && cancel->load(std::memory_order_acquire))
        return std::nullopt;

    std::optional<std::string> actual_crc32;
    if (cache) {
        std::error_code ec;
        const std::uintmax_t size = fs::file_size(neo_path, ec);
        if (ec)
            return std::nullopt;
        actual_crc32 = cache->file_crc32(neo_path, size, cancel);
    } else {
        // Preserve the standalone verifier's original behavior. Only scanner
        // calls opt into persistence by supplying its cache explicitly.
        actual_crc32 = Crc32Cache::calculate_file_crc32(neo_path, cancel);
    }
    if (!actual_crc32.has_value())
        return std::nullopt;

    return GeolithCrcVerification{
        expected->second,
        *actual_crc32,
    };
}

} // namespace goliath
