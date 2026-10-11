#include "ui/library_details_presentation.hpp"

#include "ui/library_view_logic.hpp"

#include <QDate>
#include <QRegularExpression>
#include <QStringList>

#include <cstdint>

namespace goliath {

namespace {

QString optionalOrPlaceholder(const std::optional<std::string>& value) {
    if (!value.has_value() || value->empty()) return QStringLiteral("-");
    return QString::fromStdString(*value);
}

LibraryDetailsRowPresentation optionalRow(
        const QString& text, const QString& tooltip = {}) {
    return {text, tooltip, !text.trimmed().isEmpty()};
}

QString catalogListLines(const std::optional<std::string>& value) {
    const QString text = libraryUtf8Text(value).trimmed();
    if (text.isEmpty()) return {};
    return text.split(QRegularExpression(QStringLiteral("\\s*,\\s*")),
                      Qt::SkipEmptyParts).join(QChar('\n'));
}

QString releaseDateLines(const std::optional<std::string>& value) {
    const QString raw = libraryUtf8Text(value).trimmed();
    if (raw.isEmpty()) return {};

    QStringList formatted;
    const QRegularExpression datePattern(
        QStringLiteral("^(\\d{4})(\\d{2})(\\d{2})(.*)$"));
    const QStringList entries = raw.split(
        QRegularExpression(QStringLiteral("\\s*,\\s*")),
        Qt::SkipEmptyParts);
    for (const QString& entry : entries) {
        const QString trimmed = entry.trimmed();
        const QRegularExpressionMatch match = datePattern.match(trimmed);
        if (!match.hasMatch()) {
            formatted.push_back(trimmed);
            continue;
        }

        const QDate date = QDate::fromString(
            match.captured(1) + match.captured(2) + match.captured(3),
            QStringLiteral("yyyyMMdd"));
        formatted.push_back(
            date.isValid()
                ? date.toString(QStringLiteral("yyyy-MM-dd")) +
                      match.captured(4)
                : trimmed);
    }
    return formatted.join(QChar('\n'));
}

QString mediaFormatText(const Rom* rom) {
    if (!rom) return {};
    const QString part = libraryUtf8Text(rom->part);
    const QString interfaceName = libraryUtf8Text(rom->interface);

    QString display;
    if (part == QStringLiteral("cart"))
        display = QStringLiteral("Cartridge");
    else if (part == QStringLiteral("cdrom"))
        display = QStringLiteral("CD-ROM");
    else
        display = part;

    if (!interfaceName.isEmpty()) {
        if (display.isEmpty()) return interfaceName;
        display += QStringLiteral(" (") + interfaceName + QChar(')');
    }
    return display;
}

QString binarySizeText(std::uintmax_t size) {
    constexpr std::uintmax_t kib = 1024;
    constexpr std::uintmax_t mib = 1024 * kib;
    if (size != 0 && size % mib == 0)
        return QString::number(size / mib) + QStringLiteral(" MiB");
    if (size != 0 && size % kib == 0)
        return QString::number(size / kib) + QStringLiteral(" KiB");
    return QString::number(static_cast<qulonglong>(size)) +
           QStringLiteral(" bytes");
}

QString romRegionsText(const Rom* rom) {
    if (!rom) return {};
    QStringList parts;
    if (rom->program_size.has_value())
        parts.push_back(QStringLiteral("P ") +
                        binarySizeText(*rom->program_size));
    if (rom->fixed_size.has_value())
        parts.push_back(QStringLiteral("S ") +
                        binarySizeText(*rom->fixed_size));
    if (rom->audio_cpu_size.has_value())
        parts.push_back(QStringLiteral("M ") +
                        binarySizeText(*rom->audio_cpu_size));
    if (rom->audio_data_size.has_value())
        parts.push_back(QStringLiteral("V ") +
                        binarySizeText(*rom->audio_data_size));
    if (rom->graphics_size.has_value())
        parts.push_back(QStringLiteral("C ") +
                        binarySizeText(*rom->graphics_size));
    return parts.join(QStringLiteral(" ") + QChar(u'\u00B7') + QChar(' '));
}

QString romRegionsTooltip(const Rom* rom) {
    if (!rom) return {};
    QStringList lines;
    lines.push_back(QStringLiteral(
        "MAME neogeo.xml data-area sizes (not the .neo file size):"));
    auto addRegion = [&](const QString& code, const QString& description,
                         const QString& dataArea,
                         const std::optional<std::uintmax_t>& size) {
        if (!size.has_value()) return;
        lines.push_back(
            code + QStringLiteral(" — ") + description +
            QStringLiteral(" / ") + dataArea + QStringLiteral(": ") +
            binarySizeText(*size) + QStringLiteral(" (0x") +
            QString::number(static_cast<qulonglong>(*size), 16).toUpper() +
            QStringLiteral(")"));
    };
    addRegion("P", "Program ROM", "maincpu", rom->program_size);
    addRegion("S", "Fixed-layer ROM", "fixed", rom->fixed_size);
    addRegion("M", "Audio CPU ROM", "audiocpu / audiocrypt",
              rom->audio_cpu_size);
    addRegion("V", "Audio sample ROM", "ADPCM-A + ADPCM-B",
              rom->audio_data_size);
    addRegion("C", "Graphics ROM", "sprites", rom->graphics_size);
    if (rom->program_width.has_value() ||
        rom->program_endianness.has_value()) {
        QStringList attributes;
        if (rom->program_width.has_value()) {
            attributes.push_back(
                QString::number(
                    static_cast<qulonglong>(*rom->program_width)) +
                QStringLiteral("-bit"));
        }
        if (rom->program_endianness.has_value()) {
            QString endianness = libraryUtf8Text(rom->program_endianness);
            if (!endianness.isEmpty() && !endianness.endsWith("-endian"))
                endianness += QStringLiteral("-endian");
            if (!endianness.isEmpty()) attributes.push_back(endianness);
        }
        if (!attributes.isEmpty()) {
            lines.push_back(QStringLiteral("P attributes: ") +
                            attributes.join(QStringLiteral(" ") +
                                            QChar(u'\u00B7') + QChar(' ')));
        }
    }
    return lines.size() > 1 ? lines.join(QChar('\n')) : QString();
}

QString verificationRichText(
        const LibraryVerificationPresentation& presentation) {
    QString html = QString::fromStdString(presentation.label).toHtmlEscaped();
    if (!presentation.value.empty()) {
        QString color;
        if (presentation.tone == VerificationTone::Success)
            color = QStringLiteral("#5fd171");
        else if (presentation.tone == VerificationTone::Error)
            color = QStringLiteral("#ef6262");

        html += QStringLiteral("&nbsp;&nbsp;");
        const QString value =
            QString::fromStdString(presentation.value).toHtmlEscaped();
        if (presentation.details_available && color.isEmpty())
            color = QStringLiteral("#8c8c8c");
        if (color.isEmpty()) {
            html += value;
        } else {
            const QString underline = presentation.details_available
                ? QStringLiteral(" text-decoration:underline;")
                : QString();
            html += QStringLiteral("<span style=\"color:%1; "
                                   "font-weight:600;%2\">%3</span>")
                        .arg(color, underline, value);
        }
    }
    return html;
}

QString cdVerificationDetailsText(const Game& game) {
    if (game.system != "neogeocd") return {};

    const LibraryCdVerificationBadge badge =
        libraryCdVerificationBadge(game);
    if (!badge.text.isEmpty())
        return QStringLiteral("Verification: ") + badge.text;

    if (game.identified)
        return QStringLiteral(
            "Verification: Metadata only (content not verified)");

    return QStringLiteral("Verification: ? Unknown");
}

QString cdVerificationDetailsTooltip(const Game& game) {
    if (game.system != "neogeocd") return {};

    const LibraryCdVerificationBadge badge =
        libraryCdVerificationBadge(game);
    if (!badge.tooltip.isEmpty()) return badge.tooltip;

    if (game.identified) {
        return QStringLiteral(
            "The game was identified from Neo Geo CD metadata/title matching, "
            "but the image has not passed complete Redump or MAME set "
            "verification.");
    }

    return QStringLiteral(
        "No trusted Neo Geo CD identity is available for this image.");
}

} // namespace

QString libraryBinarySizeText(std::uintmax_t size) {
    return binarySizeText(size);
}

QString libraryVariantFullName(const Rom& rom) {
    if (rom.name.has_value() && !rom.name->empty())
        return QString::fromStdString(*rom.name);
    if (rom.label.has_value() && !rom.label->empty())
        return QString::fromStdString(*rom.label);
    if (!rom.mame.empty()) return QString::fromStdString(rom.mame);
    return QString::fromStdString(rom.file);
}

QString libraryUtf8Text(const std::optional<std::string>& value) {
    if (!value.has_value() || value->empty()) return {};
    return QString::fromUtf8(value->data(),
                             static_cast<qsizetype>(value->size()));
}

const Rom* libraryDetailsRom(const Game& game, int romIndex) {
    if (romIndex >= 0 && romIndex < static_cast<int>(game.roms.size()))
        return &game.roms[static_cast<std::size_t>(romIndex)];
    if (game.main_rom.has_value()) {
        for (const Rom& rom : game.roms) {
            if (rom.main && rom.file == *game.main_rom) return &rom;
        }
    }
    for (const Rom& rom : game.roms) {
        if (rom.main) return &rom;
    }
    return nullptr;
}

LibraryCdVerificationBadge libraryCdVerificationBadge(const Game& game) {
    if (game.system != "neogeocd") return {};

    if (game.verification == std::optional<std::string>("redump-cue")) {
        return {
            QString::fromUtf8("\xE2\x9C\x93 Redump set"),
            "The CUE and all BIN files match the local Redump Neo Geo CD DAT "
            "by byte size and SHA-1."
        };
    }

    if (game.verification ==
        std::optional<std::string>("redump-tracks-only")) {
        return {
            QString::fromUtf8("\xE2\x9A\xA0 CUE mismatch"),
            "All BIN files match the local Redump Neo Geo CD DAT by size and "
            "SHA-1, but the CUE file does not match its Redump size/SHA-1. "
            "The CUE may be renamed, reformatted, or modified. The image may "
            "still launch, but it is not a complete byte-perfect Redump set."
        };
    }

    if (game.verification == std::optional<std::string>("mame-chd")) {
        return {
            QString::fromUtf8("\xE2\x9C\x93 MAME set"),
            "The CHD internal combined SHA-1 matches the local MAME "
            "neocd.xml entry."
        };
    }

    if (game.verification ==
        std::optional<std::string>("mame-chd-mismatch")) {
        return {
            QString::fromUtf8("\xE2\x9A\xA0 CHD mismatch"),
            "The CHD combined header SHA-1 does not match the "
            "filename-identified MAME neocd.xml entry. Open Verification "
            "Details to compare the expected and calculated hashes."
        };
    }

    if (!game.identified) {
        return {
            "? Unknown",
            "No trusted Neo Geo CD identity is available for this image."
        };
    }

    return {};
}

LibraryDetailsPresentation buildLibraryDetailsPresentation(
        const Game& game,
        int romIndex,
        const std::optional<std::string>& selectedMedia,
        const GameProfileSummary& profile,
        const GamePlaytimeRecord* playtime) {
    LibraryDetailsPresentation result;
    const Rom* metadataRom = libraryDetailsRom(game, romIndex);

    result.title = QString::fromStdString(
        libraryDetailsTitle(game.system == "neogeocd", game.name,
                            game.display));
    result.year = optionalOrPlaceholder(game.year);
    result.manufacturer = optionalOrPlaceholder(game.manufacturer);
    result.alternate_title = optionalRow(
        metadataRom ? libraryUtf8Text(metadataRom->alt_title) : QString());
    result.genre = optionalOrPlaceholder(game.genre);
    result.players = optionalOrPlaceholder(game.players);
    result.series = optionalOrPlaceholder(game.series);

    const QString mediaText = selectedMedia.has_value()
        ? QString::fromStdString(*selectedMedia)
        : QStringLiteral("-");
    const LibraryCatalogIdPresentation catalog =
        libraryCatalogIdPresentation(game, metadataRom);
    result.catalog_label = QString::fromStdString(catalog.label);
    const QString catalogId = QString::fromStdString(catalog.value);
    result.catalog_url = QString::fromStdString(catalog.url);
    if (catalogId.isEmpty()) {
        result.catalog_html = QStringLiteral("-");
    } else if (result.catalog_url.isEmpty()) {
        result.catalog_html = catalogId.toHtmlEscaped();
    } else {
        result.catalog_html = QStringLiteral(
            "<a href=\"%1\" style=\"text-decoration:underline;\">%2</a>")
            .arg(result.catalog_url.toHtmlEscaped(), catalogId.toHtmlEscaped());
    }

    result.serial = optionalRow(
        metadataRom ? catalogListLines(metadataRom->serial) : QString());
    result.release = optionalRow(
        metadataRom ? releaseDateLines(metadataRom->release) : QString());
    result.media_format = optionalRow(
        mediaFormatText(metadataRom), mediaText);
    result.program_rom = optionalRow(
        romRegionsText(metadataRom), romRegionsTooltip(metadataRom));

    const LibraryVerificationPresentation verification =
        libraryVerificationPresentation(game, metadataRom);
    result.verification_html = verificationRichText(verification);
    result.verification_tooltip =
        QString::fromStdString(verification.tooltip);
    result.verification_details_available =
        verification.details_available;

    result.profile_status = profile.custom
        ? QStringLiteral("Custom overrides")
        : QStringLiteral("Global defaults");
    result.profile_system = QString::fromStdString(profile.system);
    result.profile_video = QString::fromStdString(profile.video);
    result.profile_input = QString::fromStdString(profile.input);

    if (playtime) {
        result.playtime = QString::fromStdString(
            format_playtime_seconds(playtime->total_seconds));
        result.sessions = QString::number(
            static_cast<qlonglong>(playtime->session_count));
        if (playtime->last_started_epoch > 0) {
            result.last_started_epoch = playtime->last_started_epoch;
        }
        if (playtime->last_ended_epoch > 0) {
            result.last_ended_epoch = playtime->last_ended_epoch;
        }
        result.last_session = playtime->last_session_seconds >=
                kMinimumTrackedSessionSeconds
            ? QString::fromStdString(
                  format_playtime_seconds(playtime->last_session_seconds))
            : QStringLiteral("-");
    } else {
        result.playtime = QStringLiteral("Not played yet");
        result.sessions = QStringLiteral("0");
        result.last_session = QStringLiteral("-");
    }

    result.history = game.history.has_value()
        ? QString::fromStdString(*game.history) : QString();
    result.snapshot_short_name = game.short_name;
    result.recording_media = game.system == "neogeocd"
        ? selectedMedia.value_or("") : std::string();

    if (romIndex < 0) {
        result.selection_kind = LibraryDetailsSelectionKind::Game;
        if (game.system == "neogeocd") {
            result.variant_text = cdVerificationDetailsText(game);
            result.variant_tooltip = cdVerificationDetailsTooltip(game);
        }
        result.snapshot_path = game.snapshot.has_value()
            ? QString::fromStdString(*game.snapshot) : QString();
        return result;
    }

    if (romIndex >= static_cast<int>(game.roms.size())) {
        result.selection_kind = LibraryDetailsSelectionKind::InvalidVariant;
        return result;
    }

    result.selection_kind = LibraryDetailsSelectionKind::Variant;
    const Rom& rom = game.roms[static_cast<std::size_t>(romIndex)];
    const QString fullName = libraryVariantFullName(rom);
    result.variant_text = QStringLiteral("Selected variant: ") + fullName;
    result.variant_tooltip = fullName;
    result.snapshot_short_name = rom.mame;
    result.snapshot_parent_short_name = game.short_name;
    result.recording_media = game.system == "neogeocd"
        ? rom.file : std::string();
    return result;
}

} // namespace goliath
