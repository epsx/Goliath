#include "bios_verify.hpp"

#include "common/native_file.hpp"
#include "miniz.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

const std::vector<BiosSet>& bios_sets() {
    static const std::vector<BiosSet> sets = {
        {"neogeo.zip", true, "MVS (Arcade) / Universe BIOS", {
            {"sp-u2.sp1", std::string("e72943de"), "MVS BIOS (US)"},
            {"japan-j3.bin", std::string("dff6d41f"), "MVS BIOS (JP)"},
            {"sp-45.sp1", std::string("03cc9f6a"), "MVS BIOS (AS)"},
            {"sp-s2.sp1", std::string("9036d879"), "MVS BIOS (EU)"},
            {"uni-bios_4_0.rom", std::string("a7aab458"), "Universe BIOS 4.0"},
            {"sfix.sfix", std::string("c2ea0cfd"), "Fix layer graphics"},
            {"sm1.sm1", std::string("94416d67"), "Z80 sound BIOS"},
            {"000-lo.lo", std::string("5a86cff2"), "Zoom lookup table"},
        }},
        {"aes.zip", true, "AES (Home Console)", {
            {"neo-epo.bin", std::string("d27a71f1"), "AES BIOS (Export)"},
            {"neo-po.bin", std::string("16d0c132"), "AES BIOS (JP)"},
            {"000-lo.lo", std::string("5a86cff2"), "Zoom lookup table"},
        }},
        {"irrmaze.zip", false, "The Irritating Maze (MVS)", {
            {"236-bios.sp1", std::string("853e6b96"), "Irritating Maze BIOS"},
        }},
        {"neocd.zip", false, "Neo Geo CD (Front/Top Loader)", {
            {"front-sp1.bin", std::string("cac62307"), "CD Front Loader BIOS"},
            {"top-sp1.bin", std::string("c36a47c0"), "CD Top Loader BIOS"},
        }},
        {"neocdz.zip", false, "Neo Geo CDZ / CD Universe BIOS", {
            {"neocd.bin", std::string("df9de490"), "CDZ BIOS"},
            {"000-lo.lo", std::string("5a86cff2"), "Zoom lookup table"},
            {"uni-bioscd33.rom", std::string("ff3abc59"),
             "CD Universe BIOS 3.3", false},
        }},
    };
    return sets;
}

namespace {

std::string padRight(const std::string& s, std::size_t width) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), ' ');
}

std::string crc32Hex(mz_ulong crc) {
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08x", (unsigned)crc);
    return std::string(buf);
}

} // namespace

std::optional<BiosVerifyResult> verify_bios(const fs::path& bios_dir) {
    std::error_code ec;
    if (!fs::is_directory(bios_dir, ec)) return std::nullopt;

    BiosVerifyResult result;

    for (const BiosSet& set : bios_sets()) {
        fs::path zipPath = bios_dir / set.zip_name;
        std::string tag = set.required ? "required" : "optional";
        BiosArchiveVerification archive{};
        archive.zip_name = set.zip_name;
        archive.purpose = set.purpose;
        archive.required = set.required;
        result.lines.push_back("=== " + set.zip_name + "  (" + set.purpose + ") ===");

        if (!fs::is_regular_file(zipPath, ec)) {
            if (set.required) {
                result.problems++;
                archive.problems++;
                archive.status = BiosArchiveStatus::MissingRequired;
                result.lines.push_back("  \xE2\x9D\x8C MISSING (" + tag + ")"); // ❌
            } else {
                archive.status = BiosArchiveStatus::MissingOptional;
                result.optional_missing_archives++;
                result.lines.push_back("  \xE2\x9A\xA0 not found (" + tag + ", skipped)"); // ⚠
            }
            result.lines.push_back("");
            result.archives.push_back(std::move(archive));
            continue;
        }

        mz_zip_archive zip;
        std::memset(&zip, 0, sizeof(zip));
        NativeFile zipFile = open_native_file(zipPath);
        if (!zipFile ||
            !mz_zip_reader_init_cfile(&zip, zipFile.get(), 0, 0)) {
            result.problems++;
            archive.problems++;
            archive.status = BiosArchiveStatus::InvalidArchive;
            result.lines.push_back("  \xE2\x9D\x8C corrupt or not a zip file");
            result.lines.push_back("");
            result.archives.push_back(std::move(archive));
            continue;
        }

        const mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
        std::set<std::string> names;
        for (mz_uint i = 0; i < numFiles; ++i) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&zip, i, &st)) {
                names.insert(st.m_filename);
            }
        }

        for (const BiosFileEntry& fileEntry : set.files) {
            BiosMemberVerification member{};
            member.member = fileEntry.member;
            member.description = fileEntry.description;
            member.expected_crc = fileEntry.expected_crc;
            member.required = fileEntry.required;
            int idx = names.count(fileEntry.member)
                          ? mz_zip_reader_locate_file(&zip, fileEntry.member.c_str(), nullptr, 0)
                          : -1;
            if (idx < 0) {
                if (fileEntry.required) {
                    result.problems++;
                    archive.problems++;
                    member.status = BiosMemberStatus::Missing;
                    result.lines.push_back(
                        "  \xE2\x9D\x8C " + padRight(fileEntry.member, 20) +
                        " MISSING \xE2\x80\x93 " + fileEntry.description); // – (en dash)
                } else {
                    archive.optional_missing_members++;
                    result.optional_missing_members++;
                    member.status = BiosMemberStatus::MissingOptional;
                    result.lines.push_back(
                        "  \xE2\x9A\xA0 " + padRight(fileEntry.member, 20) +
                        " optional component not found \xE2\x80\x93 " +
                        fileEntry.description);
                }
                archive.members.push_back(std::move(member));
                continue;
            }

            std::size_t size = 0;
            void* buf = mz_zip_reader_extract_to_heap(
                &zip, static_cast<mz_uint>(idx), &size, 0);
            if (!buf) {
                if (fileEntry.required) {
                    result.problems++;
                    archive.problems++;
                } else {
                    archive.optional_missing_members++;
                    result.optional_missing_members++;
                }
                member.status = BiosMemberStatus::ReadError;
                result.lines.push_back(
                    std::string(fileEntry.required
                                    ? "  \xE2\x9D\x8C "
                                    : "  \xE2\x9A\xA0 ") +
                    padRight(fileEntry.member, 20) +
                    " error reading archive \xE2\x80\x93 " +
                    fileEntry.description);
                archive.members.push_back(std::move(member));
                continue;
            }
            std::string actualCrc = crc32Hex(mz_crc32(MZ_CRC32_INIT, (const mz_uint8*)buf, size));
            mz_free(buf);
            member.actual_crc = actualCrc;
            result.checked_members++;

            if (!fileEntry.expected_crc.has_value()) {
                member.status = BiosMemberStatus::Present;
                result.lines.push_back("  \xE2\x9C\x85 " + padRight(fileEntry.member, 20) + " found (crc " +
                                        actualCrc + ") \xE2\x80\x93 " + fileEntry.description); // ✅
            } else if (actualCrc == *fileEntry.expected_crc) {
                member.status = BiosMemberStatus::Verified;
                result.lines.push_back("  \xE2\x9C\x85 " + padRight(fileEntry.member, 20) + " OK (crc " +
                                        actualCrc + ") \xE2\x80\x93 " + fileEntry.description);
            } else {
                if (fileEntry.required) {
                    result.problems++;
                    archive.problems++;
                } else {
                    archive.optional_missing_members++;
                    result.optional_missing_members++;
                }
                member.status = BiosMemberStatus::CrcMismatch;
                result.lines.push_back(
                    std::string(fileEntry.required
                                    ? "  \xE2\x9D\x8C "
                                    : "  \xE2\x9A\xA0 ") +
                    padRight(fileEntry.member, 20) +
                    " BAD CRC (expected " + *fileEntry.expected_crc +
                    ", got " + actualCrc + ") \xE2\x80\x93 " +
                    fileEntry.description);
            }
            archive.members.push_back(std::move(member));
        }

        mz_zip_reader_end(&zip);
        if (archive.problems == 0) {
            archive.status = BiosArchiveStatus::Ready;
            result.ready_archives++;
        } else {
            archive.status = BiosArchiveStatus::MemberProblems;
        }
        result.archives.push_back(std::move(archive));
        result.lines.push_back("");
    }

    return result;
}

} // namespace goliath
