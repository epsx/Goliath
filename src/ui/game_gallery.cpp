#include "ui/main_window.hpp"
#include "ui/gallery_frame.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QMovie>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>

#include <algorithm>
#include <vector>

namespace goliath {

namespace {

QPixmap missingSnapshotPlaceholder(const QLabel* label) {
    if (!label || label->size().isEmpty()) return {};

    QPixmap placeholder(label->size());
    placeholder.fill(Qt::transparent);

    const QIcon icon(QStringLiteral(":/icons/goliath-qt.ico"));
    const QPixmap logo = icon.pixmap(QSize(160, 160));

    QPainter painter(&placeholder);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int spacing = 18;
    QFont textFont = label->font();
    textFont.setPointSizeF(textFont.pointSizeF() + 1.0);
    painter.setFont(textFont);
    painter.setPen(label->palette().color(QPalette::WindowText));

    const QFontMetrics metrics(textFont);
    const int contentHeight = logo.height() + spacing + metrics.height();
    const int logoY = (placeholder.height() - contentHeight) / 2;
    const int logoX = (placeholder.width() - logo.width()) / 2;
    painter.drawPixmap(logoX, logoY, logo);
    painter.drawText(
        QRect(0, logoY + logo.height() + spacing,
              placeholder.width(), metrics.height()),
        Qt::AlignHCenter | Qt::AlignVCenter,
        QStringLiteral("No snapshot available"));
    return placeholder;
}

} // namespace

void MainWindow::stopGalleryMovie() {
    if (!m_galleryMovie) return;
    QMovie* movie = m_galleryMovie;
    m_galleryMovie = nullptr;
    movie->stop();
    movie->deleteLater();
    m_galleryPlay->setIcon(m_galleryPlay->style()->standardIcon(
        QStyle::SP_MediaPlay, nullptr, m_galleryPlay));
    m_galleryPlay->setAccessibleName("Play GIF");
}

void MainWindow::refreshGameGallery() {
    stopGalleryMovie();
    m_galleryGifPaths.clear();
    m_galleryIndex = 0;

    // New recordings use a sanitized media basename. Older captures use the
    // database ROM ID as a screenshot filename prefix. Check both separately.
    static const QRegularExpression validId("^[A-Za-z0-9_-]+$");
    const QString recordingId = m_galleryRecordingId.isEmpty()
        ? m_galleryRomId : m_galleryRecordingId;
    std::vector<QFileInfo> candidates;
    if (validId.match(recordingId).hasMatch()) {
        const QDir recordings(QCoreApplication::applicationDirPath() +
                              "/recordings/" + recordingId);
        if (recordings.exists()) {
            const auto dates = recordings.entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                QDir::Name | QDir::Reversed);
            for (const QFileInfo& date : dates) {
                static const QRegularExpression validDate("^\\d{4}-\\d{2}-\\d{2}$");
                if (!validDate.match(date.fileName()).hasMatch()) continue;
                const QDir folder(date.absoluteFilePath());
                const auto gifs = folder.entryInfoList(
                    {"*.gif", "*.GIF"}, QDir::Files | QDir::NoSymLinks,
                    QDir::Time);
                for (const QFileInfo& gif : gifs) {
                    candidates.push_back(gif);
                    if (candidates.size() >= 400) break;
                }
                if (candidates.size() >= 400) break;
            }
        }
    }

    if (validId.match(m_galleryRomId).hasMatch()) {
        // Earlier recordings use <ROM>-<timestamp>.gif in the app data folder.
        const QDir screenshots(QString::fromStdWString(
            (m_paths.data_dir / "goliath" / "screenshots").wstring()));
        const auto olderGifs = screenshots.entryInfoList(
            {m_galleryRomId + "-*.gif", m_galleryRomId + "-*.GIF"},
            QDir::Files | QDir::NoSymLinks, QDir::Time);
        for (const QFileInfo& gif : olderGifs) {
            if (!gif.fileName().startsWith(m_galleryRomId + "-", Qt::CaseInsensitive))
                continue;
            candidates.push_back(gif);
            if (candidates.size() >= 800) break;
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const QFileInfo& a, const QFileInfo& b) {
                  return a.lastModified() > b.lastModified();
              });
    for (const QFileInfo& gif : candidates)
        m_galleryGifPaths.push_back(gif.absoluteFilePath());

    m_galleryGifs->setText(QString("GIFs (%1)").arg(m_galleryGifPaths.size()));
    showGalleryItem();
}

void MainWindow::showGalleryItem() {
    m_snapshotLabel->setToolTip(QString());
    const bool gifTab = m_galleryGifs->isChecked();
    const int count = m_galleryGifPaths.size();
    m_galleryPrevious->setVisible(gifTab);
    m_galleryNext->setVisible(gifTab);
    m_galleryCountLabel->setVisible(gifTab);
    m_galleryPlay->setVisible(gifTab);
    m_galleryPrevious->setEnabled(gifTab && m_galleryIndex > 0);
    m_galleryNext->setEnabled(gifTab && m_galleryIndex + 1 < count);
    m_galleryPlay->setEnabled(gifTab && count > 0);
    m_galleryCountLabel->setText(
        QString("%1/%2").arg(count ? m_galleryIndex + 1 : 0).arg(count));

    if (!gifTab) {
        m_snapshotLabel->setPixmap(
            m_gallerySnapshot.isNull()
                ? missingSnapshotPlaceholder(m_snapshotLabel)
                : m_gallerySnapshot);
        m_snapshotLabel->setText("");
    } else if (count == 0) {
        m_snapshotLabel->setPixmap(QPixmap());
        m_snapshotLabel->setText("No GIF recordings for this game");
    } else {
        const QString path = m_galleryGifPaths.at(m_galleryIndex);
        QImageReader reader(path, "gif");
        const QImage firstFrame = reader.read();
        if (firstFrame.isNull()) {
            m_snapshotLabel->setPixmap(QPixmap());
            m_snapshotLabel->setText("Could not read GIF");
            m_galleryPlay->setEnabled(false);
        } else {
            m_snapshotLabel->setPixmap(galleryGifFrame(firstFrame,
                                                       m_snapshotLabel->size()));
            m_snapshotLabel->setText("");
        }
    }
}

} // namespace goliath
