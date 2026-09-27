#include "media_internal.hpp"

#include <cuexis/media_import/media_import.hpp>

#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <jpeglib.h>
#include <png.h>
#include <zlib.h>

namespace cuexis::media_import {
namespace {

using detail::ByteReader;
using detail::WorkingBudget;

constexpr std::uint32_t maxImageDimension = 8192;
constexpr std::uint64_t maxImageBytes = 32ULL * 1024ULL * 1024ULL;
constexpr std::string_view pngDecoderName = "libpng-1.6.58";
constexpr std::string_view jpegDecoderName = "libjpeg-turbo-3.2.0";

// ---------------------------------------------------------------------------------------------
// EXIF / TIFF orientation
// ---------------------------------------------------------------------------------------------

struct TiffOrientation final {
    std::optional<std::uint16_t> value;
};

// Parses the IFD chain of a TIFF stream (the payload of an EXIF APP1 segment or a PNG eXIf
// chunk) and returns the single Orientation value. Two different values are contradictory and
// are rejected, as is any structurally invalid stream.
[[nodiscard]] auto parseTiffOrientation(std::span<const std::byte> tiff)
    -> core::Result<TiffOrientation> {
    if (tiff.size() < 8) {
        return core::unexpected(detail::mediaError(
            "media.image.exif_invalid", "EXIF stream is too short to hold a TIFF header"));
    }
    const auto first = std::to_integer<char>(tiff[0]);
    const auto second = std::to_integer<char>(tiff[1]);
    const bool littleEndian = first == 'I' && second == 'I';
    const bool bigEndian = first == 'M' && second == 'M';
    if (!littleEndian && !bigEndian) {
        return core::unexpected(
            detail::mediaError("media.image.exif_invalid", "EXIF byte order marker is invalid"));
    }

    const auto read16 = [&](std::size_t offset) -> std::uint16_t {
        if (littleEndian) {
            return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(tiff[offset])) |
                   static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(tiff[offset + 1]))
                       << 8U;
        }
        return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(tiff[offset])) << 8U |
               static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(tiff[offset + 1]));
    };
    const auto read32 = [&](std::size_t offset) -> std::uint32_t {
        if (littleEndian) {
            return static_cast<std::uint32_t>(read16(offset)) |
                   static_cast<std::uint32_t>(read16(offset + 2)) << 16U;
        }
        return static_cast<std::uint32_t>(read16(offset)) << 16U |
               static_cast<std::uint32_t>(read16(offset + 2));
    };

    if (read16(2) != 42) {
        return core::unexpected(
            detail::mediaError("media.image.exif_invalid", "EXIF TIFF magic is not 42"));
    }

    TiffOrientation result;
    std::uint32_t ifdOffset = read32(4);
    // Walk IFD0 and its successor chain; a malformed link is rejected instead of ignored.
    for (int depth = 0; depth < 8; ++depth) {
        if (ifdOffset == 0) {
            break;
        }
        const std::uint64_t entriesOffset = static_cast<std::uint64_t>(ifdOffset) + 2ULL;
        if (entriesOffset + 2ULL > tiff.size()) {
            return core::unexpected(detail::mediaError(
                "media.image.exif_invalid", "EXIF directory offset is outside the stream"));
        }
        const std::uint16_t entryCount = read16(static_cast<std::size_t>(ifdOffset));
        const std::uint64_t entriesBytes = static_cast<std::uint64_t>(entryCount) * 12ULL;
        const std::uint64_t nextOffset = entriesOffset + entriesBytes;
        if (nextOffset + 4ULL > tiff.size()) {
            return core::unexpected(detail::mediaError(
                "media.image.exif_invalid", "EXIF directory entry count exceeds the stream"));
        }
        for (std::uint16_t index = 0; index < entryCount; ++index) {
            const auto entry = static_cast<std::size_t>(entriesOffset) + index * 12U;
            if (read16(entry) != 0x0112U) {
                continue;
            }
            const std::uint16_t type = read16(entry + 2);
            const std::uint32_t count = read32(entry + 4);
            if (type != 3 || count == 0) {
                return core::unexpected(detail::mediaError(
                    "media.image.exif_invalid", "EXIF orientation entry is not a SHORT"));
            }
            if (count != 1) {
                return core::unexpected(detail::mediaError(
                    "media.image.orientation_invalid", "EXIF orientation carries multiple values"));
            }
            const auto value = read16(entry + 8);
            if (value < 1 || value > 8) {
                return core::unexpected(detail::mediaError("media.image.orientation_invalid",
                                                           "EXIF orientation is outside 1-8"));
            }
            if (result.value.has_value() && *result.value != value) {
                return core::unexpected(
                    detail::mediaError("media.image.orientation_invalid",
                                       "EXIF orientation metadata is contradictory"));
            }
            result.value = value;
        }
        ifdOffset = read32(static_cast<std::size_t>(nextOffset));
    }
    return result;
}

// ---------------------------------------------------------------------------------------------
// ICC color profile validation
// ---------------------------------------------------------------------------------------------

// A profile is accepted only when it is structurally an ICC RGB profile whose description names
// sRGB. Anything else is unsupported color metadata for this profile.
[[nodiscard]] auto iccProfileIsSrgb(std::span<const std::byte> profile) -> bool {
    if (profile.size() < 132) {
        return false;
    }
    const auto tag = [&](std::size_t offset, std::string_view expected) {
        if (offset + expected.size() > profile.size()) {
            return false;
        }
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (std::to_integer<char>(profile[offset + index]) != expected[index]) {
                return false;
            }
        }
        return true;
    };
    if (!tag(36, "acsp")) {
        return false;
    }
    const bool rgbSpace = tag(16, "RGB ");
    const bool graySpace = tag(16, "GRAY");
    if (!rgbSpace && !graySpace) {
        return false;
    }

    const auto containsSrgb = [&]() {
        constexpr std::string_view needle = "sRGB";
        if (profile.size() < needle.size()) {
            return false;
        }
        for (std::size_t offset = 0; offset + needle.size() <= profile.size(); ++offset) {
            if (tag(offset, needle)) {
                return true;
            }
        }
        return false;
    };
    return containsSrgb();
}

// ---------------------------------------------------------------------------------------------
// PNG
// ---------------------------------------------------------------------------------------------

struct PngProfile final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint8_t bitDepth{};
    std::uint8_t colorType{};
    std::uint16_t orientation{1};
};

[[nodiscard]] auto matchesAscii(std::span<const std::byte> chunk, std::string_view type) -> bool {
    if (chunk.size() < type.size()) {
        return false;
    }
    for (std::size_t index = 0; index < type.size(); ++index) {
        if (std::to_integer<char>(chunk[index]) != type[index]) {
            return false;
        }
    }
    return true;
}

// Walks the PNG chunk list to fix every profile decision before any decoder allocation: header
// fields, animation, color metadata and the EXIF orientation.
[[nodiscard]] auto scanPng(std::span<const std::byte> source, WorkingBudget& working)
    -> core::Result<PngProfile> {
    ByteReader reader{source};
    reader.seek(8);
    PngProfile profile;
    bool sawHeader = false;
    bool sawSrgbChunk = false;
    bool sawGammaChunk = false;
    std::uint32_t gammaValue = 0;
    bool sawIccChunk = false;
    bool sawExifChunk = false;
    std::optional<std::uint16_t> exifOrientation;

    while (reader.remaining() >= 12) {
        const std::uint32_t length = reader.readU32Be();
        const auto typeOffset = reader.offset();
        const auto type = source.subspan(typeOffset, 4);
        reader.skip(4);
        if (length > reader.remaining() || reader.remaining() - length < 4) {
            return core::unexpected(
                detail::mediaError("media.image.truncated", "PNG chunk length exceeds the source"));
        }
        const auto payload = reader.slice(length);
        reader.skip(4); // CRC, validated by libpng

        if (matchesAscii(type, "IHDR")) {
            if (sawHeader || payload.size() != 13) {
                return core::unexpected(
                    detail::mediaError("media.image.header_invalid", "PNG IHDR chunk is invalid"));
            }
            ByteReader header{payload};
            profile.width = header.readU32Be();
            profile.height = header.readU32Be();
            profile.bitDepth = header.readU8();
            profile.colorType = header.readU8();
            sawHeader = true;
        } else if (matchesAscii(type, "acTL")) {
            return core::unexpected(
                detail::mediaError("media.image.format_unsupported",
                                   "APNG animation is not part of the v1 image profile"));
        } else if (matchesAscii(type, "sRGB")) {
            if (payload.size() != 1) {
                return core::unexpected(
                    detail::mediaError("media.image.header_invalid", "PNG sRGB chunk is invalid"));
            }
            sawSrgbChunk = true;
        } else if (matchesAscii(type, "gAMA")) {
            if (payload.size() != 4) {
                return core::unexpected(
                    detail::mediaError("media.image.header_invalid", "PNG gAMA chunk is invalid"));
            }
            gammaValue = ByteReader{payload}.readU32Be();
            sawGammaChunk = true;
        } else if (matchesAscii(type, "iCCP")) {
            if (sawIccChunk) {
                return core::unexpected(detail::mediaError("media.image.header_invalid",
                                                           "PNG carries more than one iCCP chunk"));
            }
            sawIccChunk = true;
            ByteReader iccp{payload};
            std::size_t nameBytes = 0;
            while (nameBytes < iccp.size() && iccp.at(nameBytes) != 0) {
                ++nameBytes;
            }
            if (nameBytes == 0 || nameBytes + 2 > iccp.size()) {
                return core::unexpected(detail::mediaError("media.image.header_invalid",
                                                           "PNG iCCP chunk has no profile name"));
            }
            const auto method = iccp.at(nameBytes + 1);
            if (method != 0) {
                return core::unexpected(detail::mediaError(
                    "media.image.icc_unsupported", "PNG iCCP compression method is unsupported"));
            }
            const auto compressed = payload.subspan(nameBytes + 2);
            if (compressed.size() > 1024ULL * 1024ULL) {
                return core::unexpected(
                    detail::mediaError("media.image.icc_unsupported",
                                       "PNG iCCP profile is larger than the profile allows"));
            }
            std::uint64_t iccBufferBytes = compressed.size() * 4ULL + 1024ULL;
            if (!working.charge(iccBufferBytes)) {
                return core::unexpected(detail::budgetError(
                    "media.budget.working_limit", "PNG color profile exceeds the conversion budget",
                    working.limit(), working.current() + iccBufferBytes));
            }
            std::vector<std::byte> profileBytes(static_cast<std::size_t>(iccBufferBytes));
            uLongf profileSize = static_cast<uLongf>(profileBytes.size());
            const int status =
                uncompress(reinterpret_cast<Bytef*>(profileBytes.data()), &profileSize,
                           reinterpret_cast<const Bytef*>(compressed.data()),
                           static_cast<uLong>(compressed.size()));
            working.release(iccBufferBytes);
            if (status != Z_OK) {
                return core::unexpected(detail::mediaError(
                    "media.image.icc_unsupported", "PNG iCCP profile is not a valid zlib stream"));
            }
            profileBytes.resize(profileSize);
            if (!iccProfileIsSrgb(profileBytes)) {
                return core::unexpected(detail::mediaError(
                    "media.image.icc_unsupported", "PNG iCCP profile is not an sRGB profile"));
            }
        } else if (matchesAscii(type, "eXIf")) {
            if (sawExifChunk) {
                return core::unexpected(detail::mediaError("media.image.header_invalid",
                                                           "PNG carries more than one eXIf chunk"));
            }
            sawExifChunk = true;
            auto orientation = parseTiffOrientation(payload);
            if (!orientation) {
                return core::unexpected(orientation.error());
            }
            exifOrientation = orientation->value;
        } else if (matchesAscii(type, "IEND")) {
            break;
        }
    }

    if (!sawHeader) {
        return core::unexpected(
            detail::mediaError("media.image.header_invalid", "PNG stream has no IHDR chunk"));
    }
    if (profile.width == 0 || profile.height == 0) {
        return core::unexpected(
            detail::mediaError("media.image.dimension_invalid", "PNG dimensions must be nonzero"));
    }
    if (profile.width > maxImageDimension || profile.height > maxImageDimension) {
        return core::unexpected(detail::budgetError(
            "media.image.dimension_invalid", "PNG dimension exceeds the image profile",
            maxImageDimension, profile.width > maxImageDimension ? profile.width : profile.height));
    }
    switch (profile.colorType) {
    case 0: // grayscale
    case 2: // truecolor
    case 3: // palette
    case 4: // grayscale + alpha
    case 6: // truecolor + alpha
        break;
    default:
        return core::unexpected(detail::mediaError("media.image.color_type_unsupported",
                                                   "PNG color type is not part of the v1 profile"));
    }
    if (profile.bitDepth == 16) {
        return core::unexpected(detail::mediaError(
            "media.image.bit_depth_unsupported", "16-bit PNG is not part of the v1 image profile"));
    }
    if (profile.bitDepth != 1 && profile.bitDepth != 2 && profile.bitDepth != 4 &&
        profile.bitDepth != 8) {
        return core::unexpected(
            detail::mediaError("media.image.bit_depth_unsupported",
                               "PNG bit depth is not part of the v1 image profile"));
    }
    if (sawSrgbChunk && sawGammaChunk && gammaValue != 45455U) {
        return core::unexpected(detail::mediaError("media.image.gamma_unsupported",
                                                   "PNG gAMA contradicts its sRGB chunk"));
    }
    if (sawGammaChunk && !sawSrgbChunk && gammaValue != 45455U && gammaValue != 100000U) {
        return core::unexpected(detail::budgetError(
            "media.image.gamma_unsupported", "PNG gAMA is not sRGB or linear", 45455U, gammaValue));
    }
    if (exifOrientation.has_value()) {
        profile.orientation = *exifOrientation;
    }
    return profile;
}

struct PngIoState final {
    const std::byte* data{};
    std::size_t size{};
    std::size_t offset{};
};

struct PngMemoryState final {
    WorkingBudget* budget{};
};

// Every libpng allocation is charged to the import working budget and refused when the budget is
// exhausted. The size header keeps the release exact so the peak stays meaningful.
void* pngAllocate(png_structp png, png_alloc_size_t size) {
    auto* state = static_cast<PngMemoryState*>(png_get_mem_ptr(png));
    if (state == nullptr || state->budget == nullptr || size == 0) {
        return nullptr;
    }
    std::uint64_t total = 0;
    if (!detail::checkedAdd(size, 16ULL, total) || total > state->budget->limit()) {
        return nullptr;
    }
    if (!state->budget->charge(total)) {
        return nullptr;
    }
    void* raw = std::malloc(static_cast<std::size_t>(total));
    if (raw == nullptr) {
        state->budget->release(total);
        return nullptr;
    }
    std::memcpy(raw, &total, sizeof(total));
    return static_cast<std::byte*>(raw) + 16;
}

void pngRelease(png_structp png, png_voidp pointer) {
    auto* state = static_cast<PngMemoryState*>(png_get_mem_ptr(png));
    if (pointer == nullptr) {
        return;
    }
    auto* raw = static_cast<std::byte*>(pointer) - 16;
    std::uint64_t total = 0;
    std::memcpy(&total, raw, sizeof(total));
    if (state != nullptr && state->budget != nullptr) {
        state->budget->release(total);
    }
    std::free(raw);
}

void pngRead(png_structp png, png_bytep out, png_size_t count) {
    auto* state = static_cast<PngIoState*>(png_get_io_ptr(png));
    if (state == nullptr || count > state->size - state->offset) {
        png_error(png, "unexpected end of PNG source");
        return;
    }
    std::memcpy(out, state->data + state->offset, count);
    state->offset += count;
}

void pngErrorCallback(png_structp, png_const_charp) {}
void pngWarningCallback(png_structp, png_const_charp) {}

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

enum class PngDecodeStatus : std::uint8_t {
    ok,
    structureFailed,
    rejected,
    dimensionsChanged,
    bitDepthUnsupported,
    layoutUnsupported,
    strideUnsupported,
};

// Runs the libpng pipeline for one image. libpng reports a fatal error by longjmp, so this function
// holds only C structures and raw pointers: a jump back here must not skip a C++ destructor.
[[nodiscard]] auto runPngDecode(const std::byte* sourceData, std::size_t sourceSize,
                                const PngProfile& profile, png_bytep* rows, WorkingBudget& working)
    -> PngDecodeStatus {
    PngIoState io{sourceData, sourceSize, 0};
    PngMemoryState memory{&working};
    png_structp png =
        png_create_read_struct_2(PNG_LIBPNG_VER_STRING, nullptr, pngErrorCallback,
                                 pngWarningCallback, &memory, pngAllocate, pngRelease);
    if (png == nullptr) {
        return PngDecodeStatus::structureFailed;
    }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return PngDecodeStatus::structureFailed;
    }
    CUEXIS_MEDIA_SETJMP_BEGIN
    if (setjmp(png_jmpbuf(png)) != 0) {
        png_destroy_read_struct(&png, &info, nullptr);
        return PngDecodeStatus::rejected;
    }
    CUEXIS_MEDIA_SETJMP_END

    png_set_read_fn(png, &io, pngRead);
    png_set_crc_action(png, PNG_CRC_ERROR_QUIT, PNG_CRC_ERROR_QUIT);
    png_set_benign_errors(png, 0);
    png_read_info(png, info);

    if (png_get_image_width(png, info) != profile.width ||
        png_get_image_height(png, info) != profile.height) {
        png_destroy_read_struct(&png, &info, nullptr);
        return PngDecodeStatus::dimensionsChanged;
    }
    if (png_get_bit_depth(png, info) == 16) {
        png_destroy_read_struct(&png, &info, nullptr);
        return PngDecodeStatus::bitDepthUnsupported;
    }

    png_set_expand(png);
    png_set_gray_to_rgb(png);
    const bool hasAlpha = png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY_ALPHA ||
                          png_get_color_type(png, info) == PNG_COLOR_TYPE_RGB_ALPHA ||
                          png_get_valid(png, info, PNG_INFO_tRNS) != 0;
    if (!hasAlpha) {
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    }
    if (png_get_interlace_type(png, info) != PNG_INTERLACE_NONE) {
        png_set_interlace_handling(png);
    }
    png_read_update_info(png, info);
    if (png_get_channels(png, info) != 4 || png_get_bit_depth(png, info) != 8) {
        png_destroy_read_struct(&png, &info, nullptr);
        return PngDecodeStatus::layoutUnsupported;
    }
    if (png_get_rowbytes(png, info) != static_cast<png_size_t>(profile.width) * 4U) {
        png_destroy_read_struct(&png, &info, nullptr);
        return PngDecodeStatus::strideUnsupported;
    }

    png_read_image(png, rows);
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    return PngDecodeStatus::ok;
}

// Decodes the PNG pixels into tightly packed top-left RGBA8.
[[nodiscard]] auto decodePngPixels(std::span<const std::byte> source, const PngProfile& profile,
                                   WorkingBudget& working) -> core::Result<std::vector<std::byte>> {
    std::uint64_t pixelCount = 0;
    std::uint64_t pixelBytes = 0;
    if (!detail::checkedMul(profile.width, profile.height, pixelCount) ||
        !detail::checkedMul(pixelCount, 4ULL, pixelBytes) || pixelBytes > maxImageBytes) {
        return core::unexpected(detail::budgetError("media.image.byte_limit",
                                                    "Decoded RGBA8 image exceeds the image budget",
                                                    maxImageBytes, pixelBytes));
    }
    std::vector<std::byte> pixels(static_cast<std::size_t>(pixelBytes));
    std::uint64_t rowPointerBytes = 0;
    if (!detail::checkedMul(profile.height, sizeof(png_bytep), rowPointerBytes)) {
        return core::unexpected(detail::mediaError("media.image.dimension_invalid",
                                                   "PNG row pointer count overflowed"));
    }
    if (!working.charge(rowPointerBytes)) {
        return core::unexpected(detail::budgetError(
            "media.budget.working_limit", "PNG row table exceeds the conversion budget",
            working.limit(), working.current() + rowPointerBytes));
    }
    std::vector<png_bytep> rows(profile.height);
    for (std::uint32_t row = 0; row < profile.height; ++row) {
        rows[row] = reinterpret_cast<png_bytep>(pixels.data() + static_cast<std::size_t>(row) *
                                                                    profile.width * 4ULL);
    }

    switch (runPngDecode(source.data(), source.size(), profile, rows.data(), working)) {
    case PngDecodeStatus::ok:
        return pixels;
    case PngDecodeStatus::structureFailed:
        return core::unexpected(detail::mediaError("media.image.decode_failed",
                                                   "libpng read structure could not be created"));
    case PngDecodeStatus::rejected:
        return core::unexpected(
            detail::mediaError("media.image.decode_failed", "libpng rejected the PNG stream"));
    case PngDecodeStatus::dimensionsChanged:
        return core::unexpected(detail::mediaError(
            "media.image.header_invalid", "PNG dimensions changed between scan and decode"));
    case PngDecodeStatus::bitDepthUnsupported:
        return core::unexpected(detail::mediaError(
            "media.image.bit_depth_unsupported", "16-bit PNG is not part of the v1 image profile"));
    case PngDecodeStatus::layoutUnsupported:
        return core::unexpected(detail::mediaError("media.image.color_type_unsupported",
                                                   "PNG did not expand to 8-bit RGBA"));
    case PngDecodeStatus::strideUnsupported:
        return core::unexpected(detail::mediaError("media.image.header_invalid",
                                                   "PNG row stride is not tightly packed RGBA8"));
    }
    return core::unexpected(detail::mediaError("media.image.decode_failed",
                                               "libpng returned an unknown decode status"));
}

// ---------------------------------------------------------------------------------------------
// JPEG
// ---------------------------------------------------------------------------------------------

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

void jpegErrorExit(j_common_ptr info) {
    auto* jump = static_cast<std::jmp_buf*>(info->client_data);
    std::longjmp(*jump, 1);
}

void jpegEmitMessage(j_common_ptr, int) {}

// Walks the JPEG marker segments to fix dimensions, component count, Adobe transform and the EXIF
// orientation before libjpeg allocates any coefficient or sample buffer.
[[nodiscard]] auto scanJpeg(std::span<const std::byte> source) -> core::Result<JpegProfile> {
    JpegProfile profile;
    bool sawFrame = false;
    bool sawAdobe = false;
    std::uint8_t adobeTransform = 0;
    std::optional<std::uint16_t> exifOrientation;
    bool sawIcc = false;
    std::size_t offset = 2;
    while (offset + 1 < source.size()) {
        if (std::to_integer<std::uint8_t>(source[offset]) != 0xFF) {
            return core::unexpected(detail::mediaError("media.image.header_invalid",
                                                       "JPEG marker stream is not aligned"));
        }
        while (offset < source.size() && std::to_integer<std::uint8_t>(source[offset]) == 0xFF) {
            ++offset;
        }
        if (offset >= source.size()) {
            return core::unexpected(
                detail::mediaError("media.image.truncated", "JPEG ends inside a marker"));
        }
        const auto marker = std::to_integer<std::uint8_t>(source[offset]);
        ++offset;
        if (marker == 0x00 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD9)) {
            continue; // standalone marker
        }
        if (offset + 2 > source.size()) {
            return core::unexpected(
                detail::mediaError("media.image.truncated", "JPEG segment length is missing"));
        }
        const std::uint32_t length =
            static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(source[offset])) << 8U |
            static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(source[offset + 1]));
        if (length < 2 || offset + length > source.size()) {
            return core::unexpected(detail::mediaError("media.image.truncated",
                                                       "JPEG segment length exceeds the source"));
        }
        const auto payload = source.subspan(offset + 2, length - 2);
        offset += length;

        if (marker == 0xDA) {
            break; // start of scan
        }
        if (marker == 0xE1 && payload.size() >= 6 && matchesAscii(payload, "Exif")) {
            auto orientation = parseTiffOrientation(payload.subspan(6));
            if (!orientation) {
                return core::unexpected(orientation.error());
            }
            if (orientation->value.has_value()) {
                exifOrientation = orientation->value;
            }
        } else if (marker == 0xEE) {
            sawAdobe = true;
            if (payload.size() >= 12) {
                adobeTransform = std::to_integer<std::uint8_t>(payload[11]);
            }
        } else if (marker == 0xE2 && payload.size() > 14 && matchesAscii(payload, "ICC_PROFILE")) {
            const auto sequence = std::to_integer<std::uint8_t>(payload[12]);
            const auto total = std::to_integer<std::uint8_t>(payload[13]);
            if (total != 1 || sequence != 1) {
                return core::unexpected(detail::mediaError(
                    "media.image.icc_unsupported", "Split JPEG ICC profiles are not supported"));
            }
            sawIcc = true;
            if (!iccProfileIsSrgb(payload.subspan(14))) {
                return core::unexpected(detail::mediaError(
                    "media.image.icc_unsupported", "JPEG ICC profile is not an sRGB profile"));
            }
        } else if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            if (payload.size() < 6) {
                return core::unexpected(detail::mediaError("media.image.header_invalid",
                                                           "JPEG frame header is truncated"));
            }
            const auto precision = std::to_integer<std::uint8_t>(payload[0]);
            if (precision != 8) {
                return core::unexpected(detail::mediaError("media.image.bit_depth_unsupported",
                                                           "JPEG sample precision is not 8-bit"));
            }
            profile.height = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[1]))
                                 << 8U |
                             static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[2]));
            profile.width = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[3]))
                                << 8U |
                            static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[4]));
            profile.components = std::to_integer<std::uint8_t>(payload[5]);
            sawFrame = true;
            break;
        } else if ((marker >= 0xC3 && marker <= 0xCF) && marker != 0xC4 && marker != 0xC8 &&
                   marker != 0xCC) {
            return core::unexpected(
                detail::mediaError("media.image.format_unsupported",
                                   "JPEG frame type is not baseline or progressive"));
        }
    }

    if (!sawFrame) {
        return core::unexpected(
            detail::mediaError("media.image.header_invalid", "JPEG stream has no frame header"));
    }
    if (profile.width == 0 || profile.height == 0) {
        return core::unexpected(
            detail::mediaError("media.image.dimension_invalid", "JPEG dimensions must be nonzero"));
    }
    // One grayscale component or three colour components only. A four-component frame is CMYK
    // (Adobe transform 0) or YCCK (transform 2) and is outside the v1 profile; the Adobe marker is
    // only used to name the reason, because a three-component Adobe-marked RGB stream is a normal
    // JPEG that libjpeg-turbo converts deterministically.
    if (profile.components != 1 && profile.components != 3) {
        return core::unexpected(detail::mediaError(
            "media.image.color_type_unsupported", sawAdobe && adobeTransform == 2
                                                      ? "YCCK JPEG is not part of the v1 profile"
                                                      : "CMYK JPEG is not part of the v1 profile"));
    }
    if (profile.width > maxImageDimension || profile.height > maxImageDimension) {
        return core::unexpected(detail::budgetError(
            "media.image.dimension_invalid", "JPEG dimension exceeds the image profile",
            maxImageDimension, profile.width > maxImageDimension ? profile.width : profile.height));
    }
    (void)sawIcc;
    if (exifOrientation.has_value()) {
        profile.orientation = *exifOrientation;
    }
    return profile;
}

enum class JpegDecodeStatus : std::uint8_t {
    ok,
    rejected,
    headerChanged,
    colorTypeUnsupported,
    layoutUnexpected,
    incomplete,
};

// Runs the libjpeg pipeline for one image. libjpeg reports a fatal error by longjmp, so this
// function holds only C structures and raw pointers: a jump back here must not skip a C++
// destructor.
[[nodiscard]] auto runJpegDecode(std::span<const std::byte> source, const JpegProfile& profile,
                                 std::byte* pixels) -> JpegDecodeStatus {
    jpeg_decompress_struct info{};
    JpegErrorManager manager{};
    std::jmp_buf jump{};
    info.err = jpeg_std_error(&manager.base);
    manager.base.error_exit = jpegErrorExit;
    manager.base.emit_message = jpegEmitMessage;
    manager.base.trace_level = 0;
    info.client_data = &jump;
    CUEXIS_MEDIA_SETJMP_BEGIN
    if (setjmp(jump) != 0) {
        jpeg_destroy_decompress(&info);
        return JpegDecodeStatus::rejected;
    }
    CUEXIS_MEDIA_SETJMP_END

    jpeg_create_decompress(&info);
    jpeg_mem_src(&info, reinterpret_cast<const unsigned char*>(source.data()),
                 static_cast<unsigned long>(source.size()));
    jpeg_read_header(&info, TRUE);

    if (info.image_width != profile.width || info.image_height != profile.height ||
        info.num_components != profile.components) {
        jpeg_destroy_decompress(&info);
        return JpegDecodeStatus::headerChanged;
    }
    if (info.jpeg_color_space == JCS_CMYK || info.jpeg_color_space == JCS_YCCK) {
        jpeg_destroy_decompress(&info);
        return JpegDecodeStatus::colorTypeUnsupported;
    }

    info.out_color_space = JCS_EXT_RGBA;
    info.dct_method = JDCT_ISLOW;
    info.do_fancy_upsampling = TRUE;
    info.do_block_smoothing = TRUE;
    info.two_pass_quantize = FALSE;
    info.quantize_colors = FALSE;
    info.dither_mode = JDITHER_NONE;
    info.buffered_image = FALSE;
    jpeg_start_decompress(&info);

    if (info.output_width != profile.width || info.output_height != profile.height ||
        info.output_components != 4) {
        jpeg_destroy_decompress(&info);
        return JpegDecodeStatus::layoutUnexpected;
    }

    while (info.output_scanline < info.output_height) {
        const auto row = static_cast<std::size_t>(info.output_scanline);
        JSAMPROW rowPointer = reinterpret_cast<JSAMPROW>(
            pixels + row * static_cast<std::size_t>(profile.width) * 4ULL);
        if (jpeg_read_scanlines(&info, &rowPointer, 1) != 1) {
            jpeg_destroy_decompress(&info);
            return JpegDecodeStatus::incomplete;
        }
    }
    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    return JpegDecodeStatus::ok;
}

// Decodes the JPEG into tightly packed top-left RGBA8 with fixed integer IDCT and fixed
// upsampling.
[[nodiscard]] auto decodeJpegPixels(std::span<const std::byte> source, const JpegProfile& profile,
                                    WorkingBudget& working)
    -> core::Result<std::vector<std::byte>> {
    std::uint64_t pixelCount = 0;
    std::uint64_t pixelBytes = 0;
    if (!detail::checkedMul(profile.width, profile.height, pixelCount) ||
        !detail::checkedMul(pixelCount, 4ULL, pixelBytes) || pixelBytes > maxImageBytes) {
        return core::unexpected(detail::budgetError("media.image.byte_limit",
                                                    "Decoded RGBA8 image exceeds the image budget",
                                                    maxImageBytes, pixelBytes));
    }
    std::vector<std::byte> pixels(static_cast<std::size_t>(pixelBytes));
    // libjpeg allocates one output row plus its internal coefficient and sample buffers. The row is
    // charged here; the internal allocations are covered by the bounded worker process.
    const std::uint64_t rowBytes = static_cast<std::uint64_t>(profile.width) * 4ULL;
    if (!working.charge(rowBytes)) {
        return core::unexpected(detail::budgetError("media.budget.working_limit",
                                                    "JPEG row buffer exceeds the conversion budget",
                                                    working.limit(), working.current() + rowBytes));
    }

    switch (runJpegDecode(source, profile, pixels.data())) {
    case JpegDecodeStatus::ok:
        return pixels;
    case JpegDecodeStatus::rejected:
        return core::unexpected(
            detail::mediaError("media.image.decode_failed", "libjpeg rejected the JPEG stream"));
    case JpegDecodeStatus::headerChanged:
        return core::unexpected(detail::mediaError("media.image.header_invalid",
                                                   "JPEG header changed between scan and decode"));
    case JpegDecodeStatus::colorTypeUnsupported:
        return core::unexpected(
            detail::mediaError("media.image.color_type_unsupported",
                               "CMYK and YCCK JPEG are not part of the v1 profile"));
    case JpegDecodeStatus::layoutUnexpected:
        return core::unexpected(detail::mediaError("media.image.decode_failed",
                                                   "libjpeg produced an unexpected output layout"));
    case JpegDecodeStatus::incomplete:
        return core::unexpected(detail::mediaError("media.image.decode_incomplete",
                                                   "libjpeg stopped before the last row"));
    }
    return core::unexpected(detail::mediaError("media.image.decode_failed",
                                               "libjpeg returned an unknown decode status"));
}

// ---------------------------------------------------------------------------------------------
// Shared import tail
// ---------------------------------------------------------------------------------------------

struct DecodedImage final {
    std::vector<std::byte> pixelsRgba8;
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint16_t orientation{1};
    std::string decoder;
};

// Applies the orientation once, charges the pre-orientation buffer as working memory while the
// second buffer exists, and builds the portable payload.
[[nodiscard]] auto finalizeImage(DecodedImage decoded, WorkingBudget& working,
                                 const MediaBudget& budget) -> core::Result<ImageImportResult> {
    const bool transposed = decoded.orientation >= 5;
    ImageImportResult result;
    result.info.sourceWidth = decoded.width;
    result.info.sourceHeight = decoded.height;
    result.info.orientation = decoded.orientation;
    result.info.decoder = std::move(decoded.decoder);
    result.info.width = transposed ? decoded.height : decoded.width;
    result.info.height = transposed ? decoded.width : decoded.height;

    if (decoded.orientation == 1) {
        result.usage.decodedBytes = decoded.pixelsRgba8.size();
        result.portableTexture = detail::writePortableTexture(result.info.width, result.info.height,
                                                              decoded.pixelsRgba8);
    } else {
        const auto sourceBytes = static_cast<std::uint64_t>(decoded.pixelsRgba8.size());
        if (!working.charge(sourceBytes)) {
            return core::unexpected(
                detail::budgetError("media.budget.working_limit",
                                    "Orientation normalization exceeds the conversion budget",
                                    working.limit(), working.current() + sourceBytes));
        }
        auto oriented = detail::applyOrientation(decoded.orientation, decoded.width, decoded.height,
                                                 decoded.pixelsRgba8);
        working.release(sourceBytes);
        if (!oriented) {
            return core::unexpected(oriented.error());
        }
        result.usage.decodedBytes = oriented->size();
        result.portableTexture =
            detail::writePortableTexture(result.info.width, result.info.height, *oriented);
    }

    std::uint64_t envelopeBytes = 0;
    if (!detail::checkedAdd(result.usage.decodedBytes, 40ULL, envelopeBytes) ||
        envelopeBytes != result.portableTexture.size() ||
        result.usage.decodedBytes > budget.maxImageBytes) {
        return core::unexpected(detail::budgetError("media.image.byte_limit",
                                                    "Portable texture exceeds the image budget",
                                                    budget.maxImageBytes, envelopeBytes));
    }
    result.usage.peakWorkingBytes = working.peak();
    return result;
}

[[nodiscard]] auto validatePixelBudget(std::uint32_t width, std::uint32_t height,
                                       const MediaBudget& budget) -> core::Result<std::uint64_t> {
    std::uint64_t pixels = 0;
    std::uint64_t bytes = 0;
    if (!detail::checkedMul(width, height, pixels) || !detail::checkedMul(pixels, 4ULL, bytes)) {
        return core::unexpected(detail::mediaError("media.image.dimension_invalid",
                                                   "Image dimension product overflowed"));
    }
    if (pixels > budget.maxImagePixels) {
        return core::unexpected(detail::budgetError("media.image.pixel_limit",
                                                    "Image pixel count exceeds the image profile",
                                                    budget.maxImagePixels, pixels));
    }
    if (bytes > budget.maxImageBytes) {
        return core::unexpected(detail::budgetError("media.image.byte_limit",
                                                    "Decoded RGBA8 image exceeds the image budget",
                                                    budget.maxImageBytes, bytes));
    }
    return bytes;
}

} // namespace

auto importImage(std::span<const std::byte> source, const MediaBudget& budget)
    -> core::Result<ImageImportResult> {
    if (source.empty()) {
        return core::unexpected(detail::mediaError("media.source.empty", "Image source is empty"));
    }
    if (source.size() > budget.maxEncodedBytes) {
        return core::unexpected(detail::budgetError("media.source.limit",
                                                    "Encoded image exceeds the source budget",
                                                    budget.maxEncodedBytes, source.size()));
    }

    WorkingBudget working{budget.maxWorkingBytes};

    if (isPngSource(source)) {
        auto profile = scanPng(source, working);
        if (!profile) {
            return core::unexpected(profile.error());
        }
        auto budgetCheck = validatePixelBudget(profile->width, profile->height, budget);
        if (!budgetCheck) {
            return core::unexpected(budgetCheck.error());
        }
        auto pixels = decodePngPixels(source, *profile, working);
        if (!pixels) {
            return core::unexpected(pixels.error());
        }
        auto result =
            finalizeImage(DecodedImage{std::move(*pixels), profile->width, profile->height,
                                       profile->orientation, std::string{pngDecoderName}},
                          working, budget);
        if (result) {
            result->usage.sourceBytes = source.size();
        }
        return result;
    }
    if (isJpegSource(source)) {
        auto profile = scanJpeg(source);
        if (!profile) {
            return core::unexpected(profile.error());
        }
        auto budgetCheck = validatePixelBudget(profile->width, profile->height, budget);
        if (!budgetCheck) {
            return core::unexpected(budgetCheck.error());
        }
        auto pixels = decodeJpegPixels(source, *profile, working);
        if (!pixels) {
            return core::unexpected(pixels.error());
        }
        auto result =
            finalizeImage(DecodedImage{std::move(*pixels), profile->width, profile->height,
                                       profile->orientation, std::string{jpegDecoderName}},
                          working, budget);
        if (result) {
            result->usage.sourceBytes = source.size();
        }
        return result;
    }
    return core::unexpected(detail::mediaError(
        "media.image.format_unsupported",
        "Source is neither a PNG nor a JPEG stream; no fallback decoder is used"));
}

} // namespace cuexis::media_import