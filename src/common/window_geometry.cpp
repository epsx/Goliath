#include "common/window_geometry.hpp"

#include <algorithm>

namespace goliath {

namespace {

QSize boundedWindowSize(const QSize& requestedSize,
                        const QRect& availableGeometry) {
    return {
        std::clamp(requestedSize.width(), 1, availableGeometry.width()),
        std::clamp(requestedSize.height(), 1, availableGeometry.height()),
    };
}

} // namespace

QRect centeredWindowGeometry(const QSize& requestedSize,
                             const QRect& availableGeometry) {
    if (!availableGeometry.isValid()) {
        return {{}, requestedSize.expandedTo(QSize(1, 1))};
    }

    const QSize size = boundedWindowSize(requestedSize, availableGeometry);
    const QPoint topLeft(
        availableGeometry.x() + (availableGeometry.width() - size.width()) / 2,
        availableGeometry.y() + (availableGeometry.height() - size.height()) / 2);
    return {topLeft, size};
}

QRect constrainedWindowGeometry(const QRect& requestedGeometry,
                                const QRect& availableGeometry) {
    if (!availableGeometry.isValid()) return requestedGeometry;

    const QSize size = boundedWindowSize(requestedGeometry.size(),
                                         availableGeometry);
    const int maxX = availableGeometry.x() + availableGeometry.width() - size.width();
    const int maxY = availableGeometry.y() + availableGeometry.height() - size.height();
    const QPoint topLeft(
        std::clamp(requestedGeometry.x(), availableGeometry.x(), maxX),
        std::clamp(requestedGeometry.y(), availableGeometry.y(), maxY));
    return {topLeft, size};
}

} // namespace goliath
