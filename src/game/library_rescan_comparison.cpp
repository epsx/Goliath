#include "game/library_rescan_comparison.hpp"

#include "game/game_profile.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>

namespace goliath {

namespace {

void append_value(std::ostringstream& out, const std::string& value) {
    out << value.size() << ':' << value << ';';
}

void append_value(std::ostringstream& out,
                  const std::optional<std::string>& value) {
    if (!value.has_value()) {
        out << "-;";
        return;
    }
    append_value(out, *value);
}

template <typename T>
void append_number(std::ostringstream& out, const std::optional<T>& value) {
    if (!value.has_value()) {
        out << "-;";
        return;
    }
    out << *value << ';';
}

std::string fingerprint(const Game& game, const Rom& rom) {
    std::ostringstream out;
    append_value(out, game.name);
    append_value(out, game.display);
    append_value(out, game.short_name);
    append_value(out, game.source);
    append_value(out, game.system);
    out << game.identified << ';';
    append_value(out, game.verification);
    append_value(out, game.redump_id);
    append_value(out, game.year);
    append_value(out, game.manufacturer);
    append_value(out, game.developer);
    append_value(out, game.publisher);
    append_value(out, game.genre);
    append_value(out, game.players);
    append_value(out, game.series);
    append_value(out, game.history);
    append_value(out, game.icon);
    append_value(out, game.snapshot);
    append_value(out, rom.file);
    append_value(out, rom.name);
    append_value(out, rom.label);
    append_value(out, rom.mame);
    append_value(out, rom.cloneof);
    out << rom.main << ';';
    append_value(out, rom.alt_title);
    append_value(out, rom.serial);
    append_value(out, rom.release);
    append_value(out, rom.part);
    append_value(out, rom.interface);
    append_number(out, rom.program_width);
    append_value(out, rom.program_endianness);
    append_number(out, rom.program_size);
    append_number(out, rom.fixed_size);
    append_number(out, rom.audio_cpu_size);
    append_number(out, rom.audio_data_size);
    append_number(out, rom.graphics_size);
    append_value(out, rom.verification);
    append_value(out, rom.crc32);
    append_value(out, rom.expected_crc32);
    out << rom.hashes.size() << ';';
    for (const MediaHashDetail& hash : rom.hashes) {
        append_value(out, hash.file);
        append_value(out, hash.catalog_file);
        append_value(out, hash.role);
        append_value(out, hash.algorithm);
        append_number(out, hash.size);
        append_number(out, hash.expected_size);
        append_value(out, hash.actual);
        append_value(out, hash.expected);
        out << hash.matched << ';';
    }
    return out.str();
}

struct SnapshotEntry {
    LibraryRescanChange change;
    std::string fingerprint;
};

using Snapshot = std::map<std::string, SnapshotEntry>;

Snapshot make_snapshot(const std::vector<Game>& games) {
    Snapshot snapshot;
    for (const Game& game : games) {
        for (const Rom& rom : game.roms) {
            if (rom.file.empty()) continue;
            const std::string key =
                make_game_profile_key(game.system, rom.file);
            std::string display = game.display.empty()
                ? game.name : game.display;
            if (!rom.main) {
                const std::string variant = rom.label.value_or(
                    rom.name.value_or(rom.file));
                if (!variant.empty()) display += " — " + variant;
            }
            snapshot[key] = {
                {std::move(display), game.system, rom.file},
                fingerprint(game, rom)};
        }
    }
    return snapshot;
}

void sort_changes(std::vector<LibraryRescanChange>& changes) {
    std::sort(changes.begin(), changes.end(),
              [](const LibraryRescanChange& left,
                 const LibraryRescanChange& right) {
                  if (left.display != right.display)
                      return left.display < right.display;
                  if (left.system != right.system)
                      return left.system < right.system;
                  return left.media < right.media;
              });
}

} // namespace

LibraryRescanComparison compare_library_snapshots(
        const std::vector<Game>& previous,
        const std::vector<Game>& current) {
    const Snapshot before = make_snapshot(previous);
    const Snapshot after = make_snapshot(current);

    LibraryRescanComparison result;
    result.previous_count = before.size();
    result.current_count = after.size();

    for (const auto& [key, entry] : after) {
        const auto old = before.find(key);
        if (old == before.end()) {
            result.added.push_back(entry.change);
        } else if (old->second.fingerprint != entry.fingerprint) {
            result.updated.push_back(entry.change);
        } else {
            ++result.unchanged_count;
        }
    }
    for (const auto& [key, entry] : before) {
        if (!after.contains(key)) result.removed.push_back(entry.change);
    }

    sort_changes(result.added);
    sort_changes(result.removed);
    sort_changes(result.updated);
    return result;
}

std::vector<LibraryVerificationIssue> collect_library_verification_issues(
        const std::vector<Game>& current, std::size_t maximum_entries) {
    std::vector<LibraryVerificationIssue> issues;
    issues.reserve(std::min(maximum_entries, current.size()));

    const auto add_issue = [&](const Game& game, const std::string& media,
                               std::string problem) {
        if (issues.size() >= maximum_entries) return;
        issues.push_back({
            game.display.empty() ? game.name : game.display,
            media,
            std::move(problem),
        });
    };

    for (const Game& game : current) {
        if (issues.size() >= maximum_entries) break;

        if (game.system == "neogeocd") {
            const std::string media = game.main_rom.value_or(
                game.roms.empty() ? std::string() : game.roms.front().file);
            if (!game.identified) {
                add_issue(game, media, "Unknown Neo Geo CD image");
            } else if (game.verification ==
                       std::optional<std::string>("mame-chd-mismatch")) {
                add_issue(game, media, "CHD SHA-1 mismatch");
            }
            continue;
        }

        for (const Rom& rom : game.roms) {
            if (rom.verification !=
                std::optional<std::string>("geolith-crc32-mismatch")) {
                continue;
            }
            add_issue(game, rom.file, "CRC-32 mismatch");
            if (issues.size() >= maximum_entries) break;
        }
    }
    return issues;
}

} // namespace goliath
