#include "catch2/catch.hpp"

#include "game/animated_gif.hpp"

#include <array>
#include <cstdint>
#include <cstring>

TEST_CASE("GIF encoder supports every configured recording duration", "[gif]") {
    for (const int frameCount : {40, 56, 80}) {
        goliath::AnimatedGifEncoder encoder;
        REQUIRE(encoder.begin(16, 16));

        std::array<std::uint8_t, 16 * 16> pixels{};
        std::array<std::uint32_t, 256> colors{};
        for (int index = 0; index < 256; ++index)
            colors[index] = static_cast<std::uint32_t>(index) * 0x010101u;

        for (int frame = 0; frame < frameCount; ++frame) {
            pixels.fill(static_cast<std::uint8_t>(frame));
            REQUIRE(encoder.addFrame(pixels.data(), 16, colors.data(),
                                     colors.size(), (frame & 1) ? 13 : 12));
        }
        CHECK(encoder.frameCount() == static_cast<std::size_t>(frameCount));
        if (frameCount == 80) {
            CHECK_FALSE(encoder.addFrame(pixels.data(), 16, colors.data(),
                                         colors.size(), 12));
        }

        const auto gif = encoder.finish();
        REQUIRE(gif.size() >= 7);
        CHECK(std::memcmp(gif.data(), "GIF89a", 6) == 0);
        CHECK(gif.back() == 0x3b);
    }
}
