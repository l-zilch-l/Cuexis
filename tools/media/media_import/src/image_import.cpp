#include "image_import_units_internal.hpp"

namespace cuexis::media_import {

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

} // namespace cuexis::media_import
