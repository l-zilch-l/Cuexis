#pragma once

// Shared internals for the offline media importer. Not installed.

#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::media_import::detail {

// Checked unsigned arithmetic. Every count, product and offset in the importer goes through these
// helpers before it reaches a reserve, resize or decoder call.
[[nodiscard]] constexpr auto checkedAdd(std::uint64_t left, std::uint64_t right,
                                        std::uint64_t& result) noexcept -> bool {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] constexpr auto checkedMul(std::uint64_t left, std::uint64_t right,
                                        std::uint64_t& result) noexcept -> bool {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return false;
    }
    result = left * right;
    return true;
}

// Tracks the conversion/scratch memory one import may hold at the same time. The limit is a hard
// limit: a charge that would exceed it is refused before the allocation happens.
class WorkingBudget final {
  public:
    explicit WorkingBudget(std::uint64_t limit) noexcept : limit_(limit) {}

    [[nodiscard]] auto charge(std::uint64_t bytes) noexcept -> bool {
        std::uint64_t next = 0;
        if (!checkedAdd(current_, bytes, next) || next > limit_) {
            return false;
        }
        current_ = next;
        if (current_ > peak_) {
            peak_ = current_;
        }
        return true;
    }

    void release(std::uint64_t bytes) noexcept {
        current_ = bytes > current_ ? 0 : current_ - bytes;
    }

    [[nodiscard]] auto current() const noexcept -> std::uint64_t {
        return current_;
    }
    [[nodiscard]] auto peak() const noexcept -> std::uint64_t {
        return peak_;
    }
    [[nodiscard]] auto limit() const noexcept -> std::uint64_t {
        return limit_;
    }

  private:
    std::uint64_t limit_{};
    std::uint64_t current_{};
    std::uint64_t peak_{};
};

[[nodiscard]] auto mediaError(std::string code, std::string message) -> core::Error;

[[nodiscard]] auto budgetError(std::string code, std::string message, std::uint64_t limit,
                               std::uint64_t actual) -> core::Error;

// A bounds-checked reader over an in-memory source. A short read fails instead of returning zero.
class ByteReader final {
  public:
    explicit ByteReader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] auto remaining() const noexcept -> std::size_t {
        return bytes_.size() - offset_;
    }
    [[nodiscard]] auto offset() const noexcept -> std::size_t {
        return offset_;
    }
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return bytes_.size();
    }
    [[nodiscard]] auto at(std::size_t offset) const noexcept -> std::uint8_t;

    void seek(std::size_t offset) noexcept {
        offset_ = offset > bytes_.size() ? bytes_.size() : offset;
    }
    void skip(std::size_t count) noexcept {
        seek(offset_ + count);
    }

    [[nodiscard]] auto readU8() noexcept -> std::uint8_t;
    [[nodiscard]] auto readU16Le() noexcept -> std::uint16_t;
    [[nodiscard]] auto readU24Le() noexcept -> std::uint32_t;
    [[nodiscard]] auto readU32Le() noexcept -> std::uint32_t;
    [[nodiscard]] auto readU64Le() noexcept -> std::uint64_t;
    [[nodiscard]] auto readU16Be() noexcept -> std::uint16_t;
    [[nodiscard]] auto readU32Be() noexcept -> std::uint32_t;
    [[nodiscard]] auto slice(std::size_t count) noexcept -> std::span<const std::byte>;

  private:
    std::span<const std::byte> bytes_;
    std::size_t offset_{};
};

void appendU8(std::vector<std::byte>& out, std::uint8_t value);
void appendU16Le(std::vector<std::byte>& out, std::uint16_t value);
void appendU32Le(std::vector<std::byte>& out, std::uint32_t value);
void appendU64Le(std::vector<std::byte>& out, std::uint64_t value);
void appendBytes(std::vector<std::byte>& out, std::span<const std::byte> bytes);

// Lowercase hexadecimal rendering of a digest, appended to the string.
void appendHex(std::string& out, std::span<const std::uint8_t> bytes);

// Portable Presentation Texture2D v1 payload (CXPRES01) with sRGB, straight alpha and tightly
// packed top-left RGBA8 rows.
[[nodiscard]] auto writePortableTexture(std::uint32_t width, std::uint32_t height,
                                        std::span<const std::byte> pixelsRgba8)
    -> std::vector<std::byte>;

// Applies EXIF Orientation 1-8 once. Returns the normalized top-left RGBA8 pixels; width and
// height are swapped for the transposing orientations.
[[nodiscard]] auto applyOrientation(std::uint16_t orientation, std::uint32_t width,
                                    std::uint32_t height, std::span<const std::byte> pixelsRgba8)
    -> core::Result<std::vector<std::byte>>;

// Canonical RIFF/WAVE PCM S16LE with only the fmt and data chunks.
[[nodiscard]] auto writeCanonicalWav(std::uint32_t sampleRate, std::uint32_t channels,
                                     std::span<const std::int16_t> samples)
    -> std::vector<std::byte>;

// Fixed float-to-S16 rule: round half away from zero on the scaled value, then saturate.
// Independent of the process rounding mode. NaN and infinity are rejected by the caller.
[[nodiscard]] auto quantizeToS16(double sample) noexcept -> std::int16_t;

// Fixed integer conversion of a decoded FLAC sample of the given bit depth to S16.
[[nodiscard]] auto narrowToS16(std::int64_t sample, std::uint32_t bitsPerSample) noexcept
    -> std::int16_t;

} // namespace cuexis::media_import::detail
