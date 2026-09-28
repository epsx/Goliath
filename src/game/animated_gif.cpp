#include "game/animated_gif.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <utility>

namespace goliath {

namespace {
// Ten seconds at eight frames per second; keep the in-memory GIF bounded.
constexpr std::size_t kMaxFrames = 80;
constexpr std::size_t kMaxEncodedBytes = 64 * 1024 * 1024;
} // namespace

void AnimatedGifEncoder::byte(std::uint8_t value) {
    m_output.push_back(value);
}

void AnimatedGifEncoder::word(int value) {
    byte(static_cast<std::uint8_t>(value & 255));
    byte(static_cast<std::uint8_t>((value >> 8) & 255));
}

void AnimatedGifEncoder::block(const char* bytes, std::size_t count) {
    m_output.insert(m_output.end(), bytes, bytes + count);
}

bool AnimatedGifEncoder::begin(int width, int height) {
    if (width < 2 || width > 640 || height < 2 || height > 640) return false;
    m_output.clear();
    m_width = width;
    m_height = height;
    m_frames = 0;
    m_open = true;
    block("GIF89a", 6);
    word(width);
    word(height);
    byte(0x70); // Color resolution: 8 bits; frames carry local palettes.
    byte(0);
    byte(0);
    // Repeat in viewers supporting the Netscape application extension.
    const std::array<std::uint8_t, 19> repeat = {
        0x21, 0xff, 0x0b, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E',
        '2', '.', '0', 3, 1, 0, 0, 0
    };
    m_output.insert(m_output.end(), repeat.begin(), repeat.end());
    return true;
}

bool AnimatedGifEncoder::addFrame(const std::uint8_t* pixels, int stride,
                                   const std::uint32_t* colors,
                                   std::size_t colorCount,
                                   int delayCentiseconds) {
    if (!m_open || !pixels || !colors || colorCount == 0 ||
        colorCount > 256 || stride < m_width || m_frames >= kMaxFrames ||
        delayCentiseconds < 2 || delayCentiseconds > 100 ||
        m_output.size() > kMaxEncodedBytes) return false;

    byte(0x21); byte(0xf9); byte(4); byte(0); // Frame delay, no transparency.
    word(delayCentiseconds);
    byte(0); byte(0);
    byte(0x2c); word(0); word(0);
    word(m_width); word(m_height);
    byte(0x87); // 256-color local table; preserve each frame's colors.
    for (std::size_t i = 0; i < 256; ++i) {
        const std::uint32_t rgb = i < colorCount ? colors[i] : 0;
        byte(static_cast<std::uint8_t>((rgb >> 16) & 255));
        byte(static_cast<std::uint8_t>((rgb >> 8) & 255));
        byte(static_cast<std::uint8_t>(rgb & 255));
    }

    byte(8); // 8-bit indices and 9-bit initial LZW codes.
    std::vector<std::uint8_t> compressed;
    compressed.reserve(static_cast<std::size_t>(m_width * m_height));
    std::uint32_t bitBuffer = 0;
    int bitCount = 0;
    int codeWidth = 9;
    int nextCode = 258;
    const auto emit = [&](int code) {
        bitBuffer |= static_cast<std::uint32_t>(code) << bitCount;
        bitCount += codeWidth;
        while (bitCount >= 8) {
            compressed.push_back(static_cast<std::uint8_t>(bitBuffer & 255));
            bitBuffer >>= 8;
            bitCount -= 8;
        }
    };
    std::unordered_map<std::uint32_t, int> dictionary;
    dictionary.reserve(4096);
    emit(256); // Clear code.
    int prefix = pixels[0];
    for (int y = 0; y < m_height; ++y) {
        const std::uint8_t* row = pixels + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < m_width; ++x) {
            if (x == 0 && y == 0) continue;
            const int symbol = row[x];
            const std::uint32_t key =
                (static_cast<std::uint32_t>(prefix) << 8) |
                static_cast<std::uint32_t>(symbol);
            const auto found = dictionary.find(key);
            if (found != dictionary.end()) {
                prefix = found->second;
                continue;
            }
            emit(prefix);
            if (nextCode < 4096) {
                dictionary.emplace(key, nextCode++);
                // GIF decoders learn a dictionary entry after receiving the
                // next code, one entry later than the encoder does.
                if (nextCode > (1 << codeWidth) && codeWidth < 12)
                    ++codeWidth;
            } else {
                emit(256);
                dictionary.clear();
                codeWidth = 9;
                nextCode = 258;
            }
            prefix = symbol;
        }
    }
    emit(prefix);
    emit(257); // End of image.
    if (bitCount) compressed.push_back(static_cast<std::uint8_t>(bitBuffer & 255));

    for (std::size_t pos = 0; pos < compressed.size();) {
        const auto length = std::min<std::size_t>(255, compressed.size() - pos);
        byte(static_cast<std::uint8_t>(length));
        m_output.insert(m_output.end(), compressed.begin() + pos,
                        compressed.begin() + pos + length);
        pos += length;
    }
    byte(0); // End of frame data sub-blocks.
    ++m_frames;
    return m_output.size() <= kMaxEncodedBytes;
}

std::vector<std::uint8_t> AnimatedGifEncoder::finish() {
    if (!m_open || m_frames < 2) return {};
    byte(0x3b);
    m_open = false;
    return std::move(m_output);
}

} // namespace goliath
