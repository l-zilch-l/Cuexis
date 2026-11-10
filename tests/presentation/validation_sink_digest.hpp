#pragma once

// The streaming SHA-256 used for identity bytes.

#include "validation_sink_common.hpp"

namespace cuexis::test_support {
namespace validation_detail {

class Sha256 final {
  public:
    void update(std::span<const std::byte> bytes) noexcept {
        totalBytes_ += bytes.size();
        for (const auto value : bytes) {
            block_[blockSize_++] = std::to_integer<std::uint8_t>(value);
            if (blockSize_ == block_.size()) {
                transform(block_);
                blockSize_ = 0;
            }
        }
    }

    [[nodiscard]] auto finish() const noexcept -> playback::PresentationContentIdentity {
        auto copy = *this;
        const auto bitCount = copy.totalBytes_ * 8ULL;
        copy.block_[copy.blockSize_++] = 0x80U;
        if (copy.blockSize_ > 56) {
            std::fill(copy.block_.begin() + static_cast<std::ptrdiff_t>(copy.blockSize_),
                      copy.block_.end(), 0U);
            copy.transform(copy.block_);
            copy.blockSize_ = 0;
        }
        std::fill(copy.block_.begin() + static_cast<std::ptrdiff_t>(copy.blockSize_),
                  copy.block_.begin() + 56, 0U);
        for (std::size_t index = 0; index < 8; ++index) {
            copy.block_[56 + index] = static_cast<std::uint8_t>(bitCount >> ((7U - index) * 8U));
        }
        copy.transform(copy.block_);

        playback::PresentationContentIdentity identity;
        for (std::size_t word = 0; word < copy.state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4; ++byte) {
                identity.sha256[word * 4 + byte] =
                    static_cast<std::uint8_t>(copy.state_[word] >> ((3U - byte) * 8U));
            }
        }
        return identity;
    }

  private:
    [[nodiscard]] static auto choose(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
        -> std::uint32_t {
        return (x & y) ^ (~x & z);
    }

    [[nodiscard]] static auto majority(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
        -> std::uint32_t {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    void transform(const std::array<std::uint8_t, 64>& block) noexcept {
        static constexpr std::array<std::uint32_t, 64> constants{
            0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U,
            0x923F82A4U, 0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
            0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U,
            0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
            0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U, 0xC6E00BF3U, 0xD5A79147U,
            0x06CA6351U, 0x14292967U, 0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
            0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU,
            0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
            0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU,
            0x5B9CCA4FU, 0x682E6FF3U, 0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
            0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
        };

        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            words[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24U) |
                           (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16U) |
                           (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8U) |
                           static_cast<std::uint32_t>(block[index * 4 + 3]);
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
            const auto small0 = std::rotr(words[index - 15], 7) ^ std::rotr(words[index - 15], 18) ^
                                (words[index - 15] >> 3U);
            const auto small1 = std::rotr(words[index - 2], 17) ^ std::rotr(words[index - 2], 19) ^
                                (words[index - 2] >> 10U);
            words[index] = words[index - 16] + small0 + words[index - 7] + small1;
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        auto f = state_[5];
        auto g = state_[6];
        auto h = state_[7];
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto large1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto temporary1 = h + large1 + choose(e, f, g) + constants[index] + words[index];
            const auto large0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto temporary2 = large0 + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
                                        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
    std::array<std::uint8_t, 64> block_{};
    std::size_t blockSize_{};
    std::uint64_t totalBytes_{};
};

} // namespace validation_detail
} // namespace cuexis::test_support
