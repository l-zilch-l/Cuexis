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
