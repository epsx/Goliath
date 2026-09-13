#include "catch2/catch.hpp"

#include "common/window_geometry.hpp"

using namespace goliath;

TEST_CASE("dialog geometry keeps natural sizes centered in the work area",
          "[window-geometry]") {
    const QRect available(0, 0, 1366, 728);

    CHECK(centeredWindowGeometry(QSize(500, 400), available) ==
          QRect(433, 164, 500, 400));
}

TEST_CASE("dialog geometry limits oversized dimensions before centering",
          "[window-geometry]") {
    const QRect available(0, 0, 1366, 728);

    CHECK(centeredWindowGeometry(QSize(820, 780), available) ==
          QRect(273, 0, 820, 728));
}

TEST_CASE("dialog geometry supports monitors with negative coordinates",
          "[window-geometry]") {
    const QRect available(-1920, 0, 1920, 1040);

    CHECK(centeredWindowGeometry(QSize(980, 720), available) ==
          QRect(-1450, 160, 980, 720));
}

TEST_CASE("dialog geometry moves an off-screen window into the work area",
          "[window-geometry]") {
    const QRect available(0, 0, 1366, 728);

    CHECK(constrainedWindowGeometry(QRect(1300, 650, 500, 400), available) ==
          QRect(866, 328, 500, 400));
    CHECK(constrainedWindowGeometry(QRect(200, 150, 500, 400), available) ==
          QRect(200, 150, 500, 400));
}
