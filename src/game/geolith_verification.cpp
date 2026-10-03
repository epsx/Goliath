#include "geolith_verification.hpp"

#include "miniz.h"
#include "tinyxml2.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
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

std::string crc32_hex(mz_ulong crc) {
    char value[9]{};
    std::snprintf(value, sizeof(value), "%08x",
                  static_cast<unsigned int>(crc));
    return value;
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
    const std::atomic<bool>* cancel) {
    const auto expected = catalog.find(to_lower(catalog_filename));
    if (expected == catalog.end())
        return std::nullopt;

    if (cancel && cancel->load(std::memory_order_acquire))
        return std::nullopt;

    std::ifstream input(neo_path, std::ios::binary);
    if (!input)
        return std::nullopt;

    mz_ulong crc = MZ_CRC32_INIT;
    std::array<char, 1024 * 1024> buffer{};
    while (input) {
        if (cancel && cancel->load(std::memory_order_acquire))
            return std::nullopt;

        input.read(buffer.data(),
                   static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            crc = mz_crc32(
                crc,
                reinterpret_cast<const mz_uint8*>(buffer.data()),
                static_cast<std::size_t>(count));
        }
    }
    if (input.bad())
        return std::nullopt;

    return GeolithCrcVerification{
        expected->second,
        crc32_hex(crc),
    };
}

} // namespace goliath
