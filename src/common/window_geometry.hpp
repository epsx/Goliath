// Pure geometry helpers shared by top-level windows and their unit tests.
#pragma once

#include <QRect>
#include <QSize>

namespace goliath {

// Returns requestedSize centered inside availableGeometry, shrinking either
// dimension only when it would exceed the monitor's available work area.
QRect centeredWindowGeometry(const QSize& requestedSize,
                             const QRect& availableGeometry);

// Keeps an existing window rectangle inside availableGeometry. The current
// position is preserved whenever possible; oversized dimensions are shrunk.
QRect constrainedWindowGeometry(const QRect& requestedGeometry,
                                const QRect& availableGeometry);

} // namespace goliath
