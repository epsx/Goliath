#include "catch2/catch.hpp"

#include "ui/gallery_frame.hpp"

TEST_CASE("GIF gallery crop fills its viewport without stretching", "[gallery][gif]") {
    CHECK(goliath::galleryGifCrop(QSize(640, 360), QSize(500, 370)) ==
          QRect(77, 0, 486, 360));
    CHECK(goliath::galleryGifCrop(QSize(320, 240), QSize(500, 370)) ==
          QRect(0, 2, 320, 236));
    CHECK(goliath::galleryGifCrop(QSize(500, 370), QSize(500, 370)) ==
          QRect(0, 0, 500, 370));
    CHECK(goliath::galleryGifCrop({}, QSize(500, 370)).isEmpty());
    CHECK(goliath::galleryGifCrop(QSize(640, 360), {}).isEmpty());
}
