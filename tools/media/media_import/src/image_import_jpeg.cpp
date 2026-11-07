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

} // namespace cuexis::media_import
