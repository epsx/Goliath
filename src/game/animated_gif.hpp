// Small bounded GIF89a encoder for short, indexed gameplay captures.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace goliath {

class AnimatedGifEncoder {
public:
    bool begin(int width, int height);
    bool addFrame(const std::uint8_t* pixels, int stride,
                  const std::uint32_t* colors, std::size_t colorCount,
                  int delayCentiseconds);
    std::vector<std::uint8_t> finish();
    std::size_t frameCount() const { return m_frames; }

private:
    void byte(std::uint8_t value);
    void word(int value);
    void block(const char* bytes, std::size_t count);

    std::vector<std::uint8_t> m_output;
    int m_width = 0;
    int m_height = 0;
    std::size_t m_frames = 0;
    bool m_open = false;
};

} // namespace goliath
