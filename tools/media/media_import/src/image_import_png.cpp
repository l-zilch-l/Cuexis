#include "image_import_units_internal.hpp"

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
            // IHDR byte 10 (compression) and byte 11 (filter) are fixed by the PNG
            // specification for the permitted bit depth / colour type combinations, and the v1
            // profile freezes one layout. They are checked rather than discarded so a file that
            // relies on a method libpng would accept but the profile never described is refused
            // here, and the [[nodiscard]] reader result is not dropped silently.
            if (header.readU8() != pngCompressionDeflate) {
                return core::unexpected(detail::mediaError(
                    "media.image.header_invalid",
                    "PNG compression method is not part of the v1 image profile"));
            }
            if (header.readU8() != pngFilterAdaptive) {
                return core::unexpected(
                    detail::mediaError("media.image.header_invalid",
                                       "PNG filter method is not part of the v1 image profile"));
            }
            // IHDR byte 12 is the interlace method. The v1 profile freezes one layout, so an
            // interlaced image is refused here rather than de-interlaced by libpng and published
            // under a profile that never described it.
            if (header.readU8() != pngInterlaceNone) {
                return core::unexpected(
                    detail::mediaError("media.image.interlace_unsupported",
                                       "PNG interlace is not part of the v1 image profile"));
            }
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
    // The v1 profile has exactly one supported gamma: the sRGB transfer function. A file that
    // declares another gamma is encoding linear-light or otherwise non-sRGB samples, and this
    // importer performs no colour conversion, so accepting it would publish the samples under a
    // colour space they do not use. ADR 0042 forbids silently approximating colour, so any other
    // gAMA is refused instead of being treated as sRGB.
    if (sawGammaChunk && !sawSrgbChunk && gammaValue != 45455U) {
        return core::unexpected(detail::budgetError("media.image.gamma_unsupported",
                                                    "PNG gAMA is not the sRGB transfer function",
                                                    45455U, gammaValue));
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
    // The profile refused an interlaced IHDR before this point, so libpng must be looking at a
    // non-interlaced image. Keep the check as a decode-time guard rather than silently
    // de-interlacing an image the profile never admitted.
    if (png_get_interlace_type(png, info) != PNG_INTERLACE_NONE) {
        png_destroy_read_struct(&png, &info, nullptr);
        return PngDecodeStatus::layoutUnsupported;
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

} // namespace cuexis::media_import
