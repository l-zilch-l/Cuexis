#pragma once

// The image importer reads the EXIF orientation and the PNG/JPEG metadata first, then decodes the
// pixels under one working budget. The metadata, PNG, JPEG and tail units share these shapes, so
// they live here.

#include "media_internal.hpp"

#include <cuexis/core/result.hpp>
#include <cuexis/media_import/media_import.hpp>

#include <jpeglib.h>
#include <png.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::media_import {

using detail::ByteReader;
using detail::WorkingBudget;

constexpr std::uint32_t maxImageDimension = 8192;
constexpr std::uint64_t maxImageBytes = 32ULL * 1024ULL * 1024ULL;
constexpr std::string_view pngDecoderName = "libpng-1.6.58";
constexpr std::string_view jpegDecoderName = "libjpeg-turbo-3.2.0";
// IHDR interlace method 0: no interlacing. Method 1 (Adam7) is the only other defined value.
constexpr std::uint8_t pngInterlaceNone = 0U;
// IHDR compression and filter methods: the only values the PNG specification
// permits, and therefore the only ones the v1 image profile describes.
constexpr std::uint8_t pngCompressionDeflate = 0U;
constexpr std::uint8_t pngFilterAdaptive = 0U;

struct TiffOrientation final {
    std::optional<std::uint16_t> value;
};

struct PngProfile final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint8_t bitDepth{};
    std::uint8_t colorType{};
    std::uint16_t orientation{1};
};

// libpng and libjpeg report a fatal error by longjmp. MSVC warns (C4611) whenever setjmp appears in
// a C++ function, even when, as in these two bridges, every live object is a C structure or a raw
// pointer and no destructor can be skipped. The diagnostic is suppressed for exactly those calls.
#if defined(_MSC_VER)
#define CUEXIS_MEDIA_SETJMP_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4611))
#define CUEXIS_MEDIA_SETJMP_END __pragma(warning(pop))
#else
#define CUEXIS_MEDIA_SETJMP_BEGIN
#define CUEXIS_MEDIA_SETJMP_END
#endif

struct JpegProfile final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint8_t components{};
    std::uint16_t orientation{1};
};

// libjpeg keeps the jump target in its application data field, which avoids an over-aligned
// member in the error manager.
struct JpegErrorManager final {
    jpeg_error_mgr base{};
};

struct DecodedImage final {
    std::vector<std::byte> pixelsRgba8;
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint16_t orientation{1};
    std::string decoder;
};

[[nodiscard]] auto parseTiffOrientation(std::span<const std::byte> tiff)
    -> core::Result<TiffOrientation>;

[[nodiscard]] auto iccProfileIsSrgb(std::span<const std::byte> profile) -> bool;

[[nodiscard]] auto matchesAscii(std::span<const std::byte> chunk, std::string_view type) -> bool;

[[nodiscard]] auto scanPng(std::span<const std::byte> source, WorkingBudget& working)
    -> core::Result<PngProfile>;

[[nodiscard]] auto scanJpeg(std::span<const std::byte> source) -> core::Result<JpegProfile>;

[[nodiscard]] auto decodePngPixels(std::span<const std::byte> source, const PngProfile& profile,
                                   WorkingBudget& working) -> core::Result<std::vector<std::byte>>;

[[nodiscard]] auto decodeJpegPixels(std::span<const std::byte> source, const JpegProfile& profile,
                                    WorkingBudget& working) -> core::Result<std::vector<std::byte>>;

} // namespace cuexis::media_import
