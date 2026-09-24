#include "media_internal.hpp"

#include <cuexis_internal/sha256.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <string>

namespace cuexis::media_import::detail {
namespace {

constexpr std::array<std::byte, 8> textureMagic{
    std::byte{'C'}, std::byte{'X'}, std::byte{'P'}, std::byte{'R'},
    std::byte{'E'}, std::byte{'S'}, std::byte{'0'}, std::byte{'1'},
};

// Portable Presentation Texture2D v1: 24-byte envelope plus a 16-byte fixed body.
constexpr std::uint64_t textureEnvelopeBytes = 24;
constexpr std::uint64_t textureBodyBytes = 16;
constexpr std::uint32_t textureResourceKind = 2;
constexpr std::uint32_t texturePayloadVersion = 1;
constexpr std::uint32_t presentationColorSpaceSrgb = 2;

} // namespace

void appendHex(std::string& out, std::span<const std::uint8_t> bytes) {
    constexpr std::string_view digits = "0123456789abcdef";
    for (const auto value : bytes) {
        out.push_back(digits[value >> 4U]);
        out.push_back(digits[value & 0x0FU]);
    }
}

auto mediaError(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

auto budgetError(std::string code, std::string message, std::uint64_t limit, std::uint64_t actual)
    -> core::Error {
    return core::Error{std::move(code), std::move(message)}
        .withContext("limit", std::to_string(limit))
        .withContext("actual", std::to_string(actual));
}

auto ByteReader::at(std::size_t offset) const noexcept -> std::uint8_t {
    if (offset >= bytes_.size()) {
        return 0;
    }
    return std::to_integer<std::uint8_t>(bytes_[offset]);
}

auto ByteReader::readU8() noexcept -> std::uint8_t {
    if (remaining() < 1) {
        return 0;
    }
    return std::to_integer<std::uint8_t>(bytes_[offset_++]);
}

auto ByteReader::readU16Le() noexcept -> std::uint16_t {
    if (remaining() < 2) {
        offset_ = bytes_.size();
        return 0;
    }
    const std::uint16_t value =
        static_cast<std::uint16_t>(static_cast<std::uint32_t>(at(offset_)) |
                                   static_cast<std::uint32_t>(at(offset_ + 1)) << 8U);
    offset_ += 2;
    return value;
}

auto ByteReader::readU24Le() noexcept -> std::uint32_t {
    if (remaining() < 3) {
        offset_ = bytes_.size();
        return 0;
    }
    const auto value = static_cast<std::uint32_t>(at(offset_)) |
                       static_cast<std::uint32_t>(at(offset_ + 1)) << 8U |
                       static_cast<std::uint32_t>(at(offset_ + 2)) << 16U;
    offset_ += 3;
    return value;
}

auto ByteReader::readU32Le() noexcept -> std::uint32_t {
    if (remaining() < 4) {
        offset_ = bytes_.size();
        return 0;
    }
    const auto value = static_cast<std::uint32_t>(at(offset_)) |
                       static_cast<std::uint32_t>(at(offset_ + 1)) << 8U |
                       static_cast<std::uint32_t>(at(offset_ + 2)) << 16U |
                       static_cast<std::uint32_t>(at(offset_ + 3)) << 24U;
    offset_ += 4;
    return value;
}

auto ByteReader::readU64Le() noexcept -> std::uint64_t {
    if (remaining() < 8) {
        offset_ = bytes_.size();
        return 0;
    }
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(at(offset_ + index)) << (8U * index);
    }
    offset_ += 8;
    return value;
}

auto ByteReader::readU16Be() noexcept -> std::uint16_t {
    if (remaining() < 2) {
        offset_ = bytes_.size();
        return 0;
    }
    const std::uint16_t value =
        static_cast<std::uint16_t>(static_cast<std::uint32_t>(at(offset_)) << 8U |
                                   static_cast<std::uint32_t>(at(offset_ + 1)));
    offset_ += 2;
    return value;
}

auto ByteReader::readU32Be() noexcept -> std::uint32_t {
    if (remaining() < 4) {
        offset_ = bytes_.size();
        return 0;
    }
    const auto value = static_cast<std::uint32_t>(at(offset_)) << 24U |
                       static_cast<std::uint32_t>(at(offset_ + 1)) << 16U |
                       static_cast<std::uint32_t>(at(offset_ + 2)) << 8U |
                       static_cast<std::uint32_t>(at(offset_ + 3));
    offset_ += 4;
    return value;
}

auto ByteReader::slice(std::size_t count) noexcept -> std::span<const std::byte> {
    if (count > remaining()) {
        offset_ = bytes_.size();
        return {};
    }
    const auto value = bytes_.subspan(offset_, count);
    offset_ += count;
    return value;
}

void appendU8(std::vector<std::byte>& out, std::uint8_t value) {
    out.push_back(static_cast<std::byte>(value));
}

void appendU16Le(std::vector<std::byte>& out, std::uint16_t value) {
    appendU8(out, static_cast<std::uint8_t>(value & 0xFFU));
    appendU8(out, static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void appendU32Le(std::vector<std::byte>& out, std::uint32_t value) {
    appendU16Le(out, static_cast<std::uint16_t>(value & 0xFFFFU));
    appendU16Le(out, static_cast<std::uint16_t>((value >> 16U) & 0xFFFFU));
}

void appendU64Le(std::vector<std::byte>& out, std::uint64_t value) {
    appendU32Le(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
    appendU32Le(out, static_cast<std::uint32_t>((value >> 32U) & 0xFFFFFFFFULL));
}

void appendBytes(std::vector<std::byte>& out, std::span<const std::byte> bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}

auto writePortableTexture(std::uint32_t width, std::uint32_t height,
                          std::span<const std::byte> pixelsRgba8) -> std::vector<std::byte> {
    const auto total = textureEnvelopeBytes + textureBodyBytes + pixelsRgba8.size();
    std::vector<std::byte> out;
    out.reserve(static_cast<std::size_t>(total));
    appendBytes(out, textureMagic);
    appendU32Le(out, textureResourceKind);
    appendU32Le(out, texturePayloadVersion);
    appendU64Le(out, total);
    appendU32Le(out, width);
    appendU32Le(out, height);
    appendU32Le(out, presentationColorSpaceSrgb);
    appendU32Le(out, 0);
    appendBytes(out, pixelsRgba8);
    return out;
}

auto applyOrientation(std::uint16_t orientation, std::uint32_t width, std::uint32_t height,
                      std::span<const std::byte> pixelsRgba8)
    -> core::Result<std::vector<std::byte>> {
    if (orientation < 1 || orientation > 8) {
        return core::unexpected(mediaError("media.image.orientation_invalid",
                                           "EXIF orientation is outside the supported range 1-8"));
    }
    std::uint64_t expected = 0;
    if (!checkedMul(width, height, expected) || !checkedMul(expected, 4, expected)) {
        return core::unexpected(
            mediaError("media.image.dimension_invalid", "Image dimension product overflowed"));
    }
    if (pixelsRgba8.size() != expected) {
        return core::unexpected(mediaError("media.image.decode_incomplete",
                                           "Decoded pixel buffer does not match the dimensions"));
    }
    if (orientation == 1) {
        return std::vector<std::byte>{pixelsRgba8.begin(), pixelsRgba8.end()};
    }

    const bool transposed = orientation >= 5;
    const std::uint32_t outWidth = transposed ? height : width;
    std::vector<std::byte> out(static_cast<std::size_t>(expected), std::byte{});
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint32_t outX = 0;
            std::uint32_t outY = 0;
            switch (orientation) {
            case 2:
                outX = width - 1 - x;
                outY = y;
                break;
            case 3:
                outX = width - 1 - x;
                outY = height - 1 - y;
                break;
            case 4:
                outX = x;
                outY = height - 1 - y;
                break;
            case 5:
                outX = y;
                outY = x;
                break;
            case 6:
                outX = height - 1 - y;
                outY = x;
                break;
            case 7:
                outX = height - 1 - y;
                outY = width - 1 - x;
                break;
            case 8:
                outX = y;
                outY = width - 1 - x;
                break;
            default:
                break;
            }
            const auto source = (static_cast<std::size_t>(y) * width + x) * 4;
            const auto target = (static_cast<std::size_t>(outY) * outWidth + outX) * 4;
            for (std::size_t index = 0; index < 4; ++index) {
                out[target + index] = pixelsRgba8[source + index];
            }
        }
    }
    return out;
}

auto writeCanonicalWav(std::uint32_t sampleRate, std::uint32_t channels,
                       std::span<const std::int16_t> samples) -> std::vector<std::byte> {
    const std::uint64_t dataBytes = samples.size() * 2ULL;
    const std::uint64_t totalBytes = 44ULL + dataBytes;
    std::vector<std::byte> out;
    out.reserve(static_cast<std::size_t>(totalBytes));
    appendBytes(out, std::span<const std::byte>{reinterpret_cast<const std::byte*>("RIFF"), 4});
    appendU32Le(out, static_cast<std::uint32_t>(totalBytes - 8ULL));
    appendBytes(out, std::span<const std::byte>{reinterpret_cast<const std::byte*>("WAVE"), 4});
    appendBytes(out, std::span<const std::byte>{reinterpret_cast<const std::byte*>("fmt "), 4});
    appendU32Le(out, 16);
    appendU16Le(out, 1); // PCM
    appendU16Le(out, static_cast<std::uint16_t>(channels));
    appendU32Le(out, sampleRate);
    appendU32Le(out, sampleRate * channels * 2U);
    appendU16Le(out, static_cast<std::uint16_t>(channels * 2U));
    appendU16Le(out, 16);
    appendBytes(out, std::span<const std::byte>{reinterpret_cast<const std::byte*>("data"), 4});
    appendU32Le(out, static_cast<std::uint32_t>(dataBytes));
    for (const auto sample : samples) {
        appendU16Le(out, static_cast<std::uint16_t>(sample));
    }
    return out;
}

auto quantizeToS16(double sample) noexcept -> std::int16_t {
    // Round half away from zero, then saturate. std::floor is exact and independent of the
    // process rounding mode, so the same float input always produces the same S16 output.
    const double scaled = sample * 32768.0;
    const double rounded = scaled >= 0.0 ? std::floor(scaled + 0.5) : std::ceil(scaled - 0.5);
    if (rounded >= 32767.0) {
        return 32767;
    }
    if (rounded <= -32768.0) {
        return -32768;
    }
    return static_cast<std::int16_t>(rounded);
}

auto narrowToS16(std::int64_t sample, std::uint32_t bitsPerSample) noexcept -> std::int16_t {
    if (bitsPerSample < 16) {
        // Low-depth FLAC keeps its full amplitude: the sample is scaled into the S16 range exactly.
        const auto shift = static_cast<unsigned>(16U - bitsPerSample);
        return static_cast<std::int16_t>(sample << shift);
    }
    if (bitsPerSample == 16) {
        return static_cast<std::int16_t>(sample);
    }
    const auto shift = static_cast<unsigned>(bitsPerSample - 16);
    const std::int64_t half = static_cast<std::int64_t>(1) << (shift - 1);
    std::int64_t rounded = 0;
    if (sample >= 0) {
        rounded = (sample + half) >> shift;
    } else {
        rounded = -(((-sample) + half) >> shift);
    }
    if (rounded > 32767) {
        return 32767;
    }
    if (rounded < -32768) {
        return -32768;
    }
    return static_cast<std::int16_t>(rounded);
}

} // namespace cuexis::media_import::detail

namespace cuexis::media_import {
namespace {

// The conversion profile identity is a versioned preimage over every input that can change
// canonical bytes: the profile version and each fixed decoder build.
constexpr std::string_view profileDomain = "cuexis.media.import.profile.v1";
constexpr std::string_view profileVersion = "1";
constexpr std::string_view pngDecoderVersion = "libpng-1.6.58";
constexpr std::string_view jpegDecoderVersion = "libjpeg-turbo-3.2.0-idct-islow-fancy-upsample";
constexpr std::string_view mp3DecoderVersion = "minimp3-2021-11-30-no-simd";
constexpr std::string_view vorbisDecoderVersion = "libvorbis-1.3.7-libogg-1.3.6";
constexpr std::string_view flacDecoderVersion = "libflac-1.5.0-native";

} // namespace

auto mediaProfileIdentity() -> std::string {
    std::string preimage{profileDomain};
    preimage.push_back('\0');
    preimage.append(profileVersion);
    preimage.push_back('\0');
    preimage.append(pngDecoderVersion);
    preimage.push_back('\0');
    preimage.append(jpegDecoderVersion);
    preimage.push_back('\0');
    preimage.append(mp3DecoderVersion);
    preimage.push_back('\0');
    preimage.append(vorbisDecoderVersion);
    preimage.push_back('\0');
    preimage.append(flacDecoderVersion);

    std::string out;
    out.reserve(64);
    detail::appendHex(out, core::detail::sha256(std::string_view{preimage}));
    return out;
}

auto contentIdentity(std::span<const std::byte> bytes) -> std::string {
    std::string out;
    out.reserve(64);
    detail::appendHex(out, core::detail::sha256(bytes));
    return out;
}

auto isPngSource(std::span<const std::byte> source) noexcept -> bool {
    constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (source.size() < signature.size()) {
        return false;
    }
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (std::to_integer<std::uint8_t>(source[index]) != signature[index]) {
            return false;
        }
    }
    return true;
}

auto isJpegSource(std::span<const std::byte> source) noexcept -> bool {
    return source.size() >= 2 && std::to_integer<std::uint8_t>(source[0]) == 0xFF &&
           std::to_integer<std::uint8_t>(source[1]) == 0xD8;
}

auto isMp3Source(std::span<const std::byte> source) noexcept -> bool {
    if (source.size() < 3) {
        return false;
    }
    const auto first = std::to_integer<std::uint8_t>(source[0]);
    const auto second = std::to_integer<std::uint8_t>(source[1]);
    const auto third = std::to_integer<std::uint8_t>(source[2]);
    if (first == 0x49 && second == 0x44 && third == 0x33) {
        return true; // "ID3"
    }
    return first == 0xFF && (second & 0xE0U) == 0xE0U;
}

auto isOggVorbisSource(std::span<const std::byte> source) noexcept -> bool {
    constexpr std::string_view capture = "OggS";
    if (source.size() < capture.size()) {
        return false;
    }
    for (std::size_t index = 0; index < capture.size(); ++index) {
        if (std::to_integer<char>(source[index]) != capture[index]) {
            return false;
        }
    }
    return true;
}

auto isFlacSource(std::span<const std::byte> source) noexcept -> bool {
    constexpr std::string_view capture = "fLaC";
    if (source.size() < capture.size()) {
        return false;
    }
    for (std::size_t index = 0; index < capture.size(); ++index) {
        if (std::to_integer<char>(source[index]) != capture[index]) {
            return false;
        }
    }
    return true;
}

} // namespace cuexis::media_import
