#pragma once

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>

namespace goliath {

// The recorder and the gallery must resolve an exact media filename to the
// same safe directory component. CD display names and database IDs differ.
inline QString recordingIdForMedia(const QString& media) {
    QString id = QFileInfo(media).completeBaseName();
    static const QRegularExpression unsafe("[^A-Za-z0-9_-]");
    id.replace(unsafe, "_");
    return id.isEmpty() ? QStringLiteral("game") : id;
}

// Match the dated directories read by the GIF gallery; don't offer an empty
// recording folder in the game's context menu.
inline QString recordingFolderWithGifs(const QString& applicationDir,
                                       const QString& recordingId) {
    static const QRegularExpression validId("^[A-Za-z0-9_-]+$");
    static const QRegularExpression validDate(R"(^\d{4}-\d{2}-\d{2}$)");
    if (!validId.match(recordingId).hasMatch()) return {};

    const QDir folder(QDir(applicationDir).filePath("recordings/" + recordingId));
    if (!folder.exists()) return {};
    const auto dates = folder.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QFileInfo& date : dates) {
        if (!validDate.match(date.fileName()).hasMatch()) continue;
        if (!QDir(date.absoluteFilePath()).entryList(
                {"*.gif", "*.GIF"}, QDir::Files | QDir::NoSymLinks).isEmpty())
            return folder.absolutePath();
    }
    return {};
}

inline QString legacyGifFolder(const QString& screenshotsDir,
                               const QString& romId) {
    static const QRegularExpression validId("^[A-Za-z0-9_-]+$");
    if (!validId.match(romId).hasMatch()) return {};
    const QDir folder(screenshotsDir);
    if (!folder.exists()) return {};
    const auto files = folder.entryInfoList(
        {romId + "-*.gif", romId + "-*.GIF"},
        QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo& file : files) {
        if (file.fileName().startsWith(romId + "-", Qt::CaseInsensitive))
            return folder.absolutePath();
    }
    return {};
}

} // namespace goliath
