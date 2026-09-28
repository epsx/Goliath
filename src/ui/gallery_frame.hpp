#pragma once

#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QtGlobal>

namespace goliath {

inline QRect galleryGifCrop(const QSize& frame, const QSize& viewport) {
    if (frame.isEmpty() || viewport.isEmpty()) return {};
    int width = frame.width();
    int height = frame.height();
    if (static_cast<qint64>(width) * viewport.height() >
        static_cast<qint64>(height) * viewport.width()) {
        width = static_cast<int>(static_cast<qint64>(height) *
                                 viewport.width() / viewport.height());
    } else {
        height = static_cast<int>(static_cast<qint64>(width) *
                                  viewport.height() / viewport.width());
    }
    width = qMax(1, width);
    height = qMax(1, height);
    return QRect((frame.width() - width) / 2,
                 (frame.height() - height) / 2, width, height);
}

// Fill the gallery viewport without stretching the GIF. Only the displayed
// frame is cropped; the recording on disk retains its full original image.
inline QPixmap galleryGifFrame(const QImage& frame, const QSize& viewport) {
    if (frame.isNull() || viewport.isEmpty()) return {};
    const QRect visible = galleryGifCrop(frame.size(), viewport);
    return QPixmap::fromImage(frame.copy(visible)).scaled(
        viewport, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace goliath
