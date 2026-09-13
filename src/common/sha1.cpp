#include "sha1.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace goliath {
namespace {

class Sha1 {
public:
    void update(const unsigned char* data, std::size_t size) {
        total_bytes_ += size;
        while (size > 0) {
            const std::size_t room = block_.size() - block_size_;
            const std::size_t take = size < room ? size : room;
            for (std::size_t i = 0; i < take; ++i)
                block_[block_size_ + i] = data[i];
            block_size_ += take;
            data += take;
            size -= take;

            if (block_size_ == block_.size()) {
                transform(block_.data());
                block_size_ = 0;
            }
        }
    }

    std::array<std::uint32_t, 5> finish() {
        const std::uint64_t bit_count = static_cast<std::uint64_t>(total_bytes_) * 8ULL;

        block_[block_size_++] = 0x80;
        if (block_size_ > 56) {
            while (block_size_ < 64) block_[block_size_++] = 0;
            transform(block_.data());
            block_size_ = 0;
        }

        while (block_size_ < 56) block_[block_size_++] = 0;
        for (int i = 7; i >= 0; --i)
            block_[block_size_++] = static_cast<unsigned char>((bit_count >> (i * 8)) & 0xffU);

        transform(block_.data());
        block_size_ = 0;
        return state_;
    }

private:
    static std::uint32_t rol(std::uint32_t value, unsigned bits) {
        return (value << bits) | (value >> (32U - bits));
    }

    void transform(const unsigned char* block) {
        std::array<std::uint32_t, 80> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t p = i * 4;
            w[i] = (static_cast<std::uint32_t>(block[p]) << 24) |
                   (static_cast<std::uint32_t>(block[p + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[p + 2]) << 8) |
                   static_cast<std::uint32_t>(block[p + 3]);
        }
        for (std::size_t i = 16; i < 80; ++i)
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];

        for (std::size_t i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999U;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1U;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdcU;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6U;
            }

            const std::uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
    }

    std::array<std::uint32_t, 5> state_{
        0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U, 0xc3d2e1f0U};
    std::array<unsigned char, 64> block_{};
    std::size_t block_size_ = 0;
    std::uint64_t total_bytes_ = 0;
};

std::string digest_to_hex(const std::array<std::uint32_t, 5>& digest) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::uint32_t word : digest)
        out << std::setw(8) << word;
    return out.str();
}

} // namespace

std::string sha1_hex(std::string_view data) {
    Sha1 sha1;
    sha1.update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    return digest_to_hex(sha1.finish());
}

std::optional<std::string> sha1_file_hex(const std::filesystem::path& path,
                                         const std::atomic<bool>* cancel) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;

    Sha1 sha1;
    std::array<char, 1024 * 1024> buffer{};
    while (in) {
        if (cancel && cancel->load(std::memory_order_acquire))
            return std::nullopt;

        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) {
            sha1.update(reinterpret_cast<const unsigned char*>(buffer.data()),
                        static_cast<std::size_t>(got));
        }
    }

    if (!in.eof())
        return std::nullopt;
    if (cancel && cancel->load(std::memory_order_acquire))
        return std::nullopt;

    return digest_to_hex(sha1.finish());
}

} // namespace goliath
