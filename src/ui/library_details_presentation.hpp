#pragma once

#include "game/game_model.hpp"
#include "game/game_playtime.hpp"
#include "game/game_profile_summary.hpp"

#include <QString>

#include <cstdint>
#include <optional>
#include <string>

namespace goliath {

struct LibraryDetailsRowPresentation {
    QString text;
    QString tooltip;
    bool visible = true;
};

enum class LibraryDetailsSelectionKind {
    Game,
    Variant,
    InvalidVariant,
};

// Side-effect-free data model for the library details pane. MainWindow keeps
// widget updates, local-time conversion, and snapshot path resolution.
struct LibraryDetailsPresentation {
    QString title;
    QString year;
    QString manufacturer;
    LibraryDetailsRowPresentation alternate_title;
    QString genre;
    QString players;
    QString series;

    QString catalog_label;
    QString catalog_html;
    QString catalog_url;
    LibraryDetailsRowPresentation serial;
    LibraryDetailsRowPresentation release;
    LibraryDetailsRowPresentation media_format;
    LibraryDetailsRowPresentation program_rom;

    QString verification_html;
    QString verification_tooltip;
    bool verification_details_available = false;

    QString profile_status;
    QString profile_system;
    QString profile_video;
    QString profile_input;

    QString playtime;
    QString sessions;
    std::optional<std::int64_t> last_started_epoch;
    std::optional<std::int64_t> last_ended_epoch;
    QString last_session;

    LibraryDetailsSelectionKind selection_kind =
        LibraryDetailsSelectionKind::Game;
    QString variant_text;
    QString variant_tooltip;
    QString history;
    QString snapshot_path;
    std::string snapshot_short_name;
    std::string snapshot_parent_short_name;
    std::string recording_media;
};

QString libraryVariantFullName(const Rom& rom);
QString libraryUtf8Text(const std::optional<std::string>& value);
QString libraryBinarySizeText(std::uintmax_t size);
const Rom* libraryDetailsRom(const Game& game, int romIndex);

struct LibraryCdVerificationBadge {
    QString text;
    QString tooltip;
};

LibraryCdVerificationBadge libraryCdVerificationBadge(const Game& game);

LibraryDetailsPresentation buildLibraryDetailsPresentation(
    const Game& game,
    int romIndex,
    const std::optional<std::string>& selectedMedia,
    const GameProfileSummary& profile,
    const GamePlaytimeRecord* playtime);

} // namespace goliath
