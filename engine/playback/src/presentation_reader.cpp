#include "presentation_internal.hpp"
#include "presentation_units_internal.hpp"

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::playback::detail {

[[nodiscard]] auto resourceTypeName(PresentationResourceType type) noexcept -> std::string_view {
    switch (type) {
    case PresentationResourceType::Mesh:
        return "mesh";
    case PresentationResourceType::Texture2D:
        return "texture2d";
    case PresentationResourceType::UnlitMaterial:
        return "unlit_material";
    case PresentationResourceType::Shader:
        return "shader";
    case PresentationResourceType::ParameterizedMaterial:
        return "parameterized_material";
    }
    return "unknown";
}

[[nodiscard]] auto indexedType(PresentationResourceType type) noexcept -> assets::AssetType {
    switch (type) {
    case PresentationResourceType::Mesh:
        return assets::AssetType::Mesh;
    case PresentationResourceType::Texture2D:
        return assets::AssetType::Texture;
    case PresentationResourceType::UnlitMaterial:
        return assets::AssetType::Material;
    case PresentationResourceType::Shader:
        return assets::AssetType::Shader;
    case PresentationResourceType::ParameterizedMaterial:
        return assets::AssetType::Material;
    }
    return assets::AssetType::Mesh;
}

[[nodiscard]] auto payloadKind(PresentationResourceType type) noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(type);
}

[[nodiscard]] auto resourceError(std::string code, std::string message, std::string_view assetId,
                                 PresentationResourceType type) -> core::Error {
    return core::Error{std::move(code), std::move(message)}
        .withContext("asset_id", std::string{assetId})
        .withContext("resource_type", std::string{resourceTypeName(type)});
}

[[nodiscard]] auto payloadError(std::string code, std::string message, std::string_view assetId,
                                PresentationResourceType type, std::size_t byteOffset)
    -> core::Error {
    return resourceError(std::move(code), std::move(message), assetId, type)
        .withContext("byte_offset", std::to_string(byteOffset));
}

[[nodiscard]] auto budgetError(std::string_view assetId, PresentationResourceType type,
                               std::uint64_t limit, std::uint64_t actual) -> core::Error {
    return resourceError("playback.presentation.resource.budget_exceeded",
                         "Portable resource byte budget was exceeded", assetId, type)
        .withContext("limit", std::to_string(limit))
        .withContext("actual", std::to_string(actual));
}

[[nodiscard]] auto checkedAdd(std::size_t left, std::size_t right, std::size_t& result) noexcept
    -> bool {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] auto checkedAddU64(std::uint64_t left, std::uint64_t right,
                                 std::uint64_t& result) noexcept -> bool {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] auto checkedMultiply(std::size_t left, std::size_t right,
                                   std::size_t& result) noexcept -> bool {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
        return false;
    }
    result = left * right;
    return true;
}

[[nodiscard]] auto validateEnvelope(std::span<const std::byte> bytes, std::string_view assetId,
                                    PresentationResourceType type) -> core::Result<ByteReader> {
    if (bytes.size() > maxResourceBytes) {
        return core::unexpected(budgetError(assetId, type, maxResourceBytes, bytes.size()));
    }
    if (bytes.size() < envelopeByteCount) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload envelope is truncated", assetId,
                                             type, bytes.size()));
    }
    if (!std::equal(payloadMagic.begin(), payloadMagic.end(), bytes.begin())) {
        return core::unexpected(payloadError("playback.presentation.payload.magic_invalid",
                                             "Portable payload magic is invalid", assetId, type,
                                             0));
    }

    ByteReader reader{bytes};
    reader.seek(8);
    const auto kind = reader.readU32();
    if (kind != payloadKind(type)) {
        return core::unexpected(payloadError("playback.presentation.payload.type_mismatch",
                                             "Portable payload kind does not match the Asset Index",
                                             assetId, type, 8));
    }
    const auto version = reader.readU32();
    if (version != 1) {
        return core::unexpected(payloadError("playback.presentation.payload.version_unsupported",
                                             "Portable payload version is unsupported", assetId,
                                             type, 12));
    }
    const auto declaredByteCount = reader.readU64();
    if (declaredByteCount > bytes.size()) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload body is truncated", assetId, type,
                                             bytes.size()));
    }
    if (declaredByteCount != bytes.size()) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload size does not match its envelope",
                                             assetId, type,
                                             static_cast<std::size_t>(declaredByteCount)));
    }
    return reader;
}

[[nodiscard]] auto validateExactSize(std::size_t expected, std::size_t actual,
                                     std::string_view assetId, PresentationResourceType type)
    -> core::Result<void> {
    if (expected > actual) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload body is truncated", assetId, type,
                                             actual));
    }
    if (expected < actual) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload contains trailing bytes", assetId,
                                             type, expected));
    }
    return {};
}

[[nodiscard]] auto parseMesh(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedMesh> {
    const auto type = PresentationResourceType::Mesh;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 40) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Mesh fixed body is truncated", assetId, type,
                                             bytes.size()));
    }
    auto reader = *readerResult;
    const auto vertexCount = reader.readU32();
    const auto indexCount = reader.readU32();
    const auto flags = reader.readU32();
    const auto reserved = reader.readU32();
    if (vertexCount == 0 || vertexCount > maxMeshVertices) {
        return core::unexpected(payloadError("playback.presentation.mesh.vertex_count_invalid",
                                             "Portable Mesh vertex count is outside the v1 range",
                                             assetId, type, 24));
    }
    if (indexCount < 3 || indexCount > maxMeshIndices || indexCount % 3 != 0) {
        return core::unexpected(payloadError("playback.presentation.mesh.index_count_invalid",
                                             "Portable Mesh index count is outside the v1 range",
                                             assetId, type, 28));
    }
    if ((flags & ~1U) != 0 || reserved != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Mesh reserved flags or fields are non-zero",
                                             assetId, type, (flags & ~1U) != 0 ? 32 : 36));
    }

    const bool hasUv0 = (flags & 1U) != 0;
    std::size_t positionCount = 0;
    std::size_t positionBytes = 0;
    std::size_t uvCount = 0;
    std::size_t uvBytes = 0;
    std::size_t indexBytes = 0;
    std::size_t expectedBytes = 40;
    if (!checkedMultiply(vertexCount, 3, positionCount) ||
        !checkedMultiply(positionCount, sizeof(float), positionBytes) ||
        (hasUv0 && (!checkedMultiply(vertexCount, 2, uvCount) ||
                    !checkedMultiply(uvCount, sizeof(float), uvBytes))) ||
        !checkedMultiply(indexCount, sizeof(std::uint32_t), indexBytes) ||
        !checkedAdd(expectedBytes, positionBytes, expectedBytes) ||
        !checkedAdd(expectedBytes, uvBytes, expectedBytes) ||
        !checkedAdd(expectedBytes, indexBytes, expectedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Mesh byte calculation overflowed", assetId,
                                             type, 24));
    }
    if (auto exact = validateExactSize(expectedBytes, bytes.size(), assetId, type); !exact) {
        return core::unexpected(std::move(exact.error()));
    }

    std::size_t decodedBytes = 0;
    if (!checkedAdd(positionBytes, uvBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, indexBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, 24, decodedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Mesh decoded byte calculation overflowed",
                                             assetId, type, 24));
    }
    if (decodedBytes > maxResourceBytes) {
        return core::unexpected(budgetError(assetId, type, maxResourceBytes, decodedBytes));
    }

    ParsedMesh parsed;
    parsed.value.positions.resize(positionCount);
    parsed.value.uv0.resize(uvCount);
    parsed.value.indices.resize(indexCount);
    for (std::size_t index = 0; index < parsed.value.positions.size(); ++index) {
        const auto value = reader.readFloat();
        if (!std::isfinite(value) || std::abs(value) > 1'000'000.0F) {
            return core::unexpected(
                payloadError("playback.presentation.mesh.value_invalid",
                             "Portable Mesh position is non-finite or outside the v1 range",
                             assetId, type, reader.offset() - sizeof(float)));
        }
        parsed.value.positions[index] = value;
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        parsed.value.boundsMin[axis] = parsed.value.positions[axis];
        parsed.value.boundsMax[axis] = parsed.value.positions[axis];
    }
    for (std::size_t vertex = 1; vertex < vertexCount; ++vertex) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto value = parsed.value.positions[vertex * 3 + axis];
            parsed.value.boundsMin[axis] = std::min(parsed.value.boundsMin[axis], value);
            parsed.value.boundsMax[axis] = std::max(parsed.value.boundsMax[axis], value);
        }
    }
    for (std::size_t index = 0; index < parsed.value.uv0.size(); ++index) {
        const auto value = reader.readFloat();
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
            return core::unexpected(
                payloadError("playback.presentation.mesh.value_invalid",
                             "Portable Mesh UV0 value is non-finite or outside [0, 1]", assetId,
                             type, reader.offset() - sizeof(float)));
        }
        parsed.value.uv0[index] = value;
    }
    for (std::size_t index = 0; index < parsed.value.indices.size(); ++index) {
        const auto value = reader.readU32();
        if (value >= vertexCount) {
            return core::unexpected(payloadError("playback.presentation.mesh.index_out_of_range",
                                                 "Portable Mesh index does not name a vertex",
                                                 assetId, type,
                                                 reader.offset() - sizeof(std::uint32_t)));
        }
        parsed.value.indices[index] = value;
    }
    for (std::size_t index = 0; index < parsed.value.indices.size(); index += 3) {
        const auto index0 = parsed.value.indices[index];
        const auto index1 = parsed.value.indices[index + 1];
        const auto index2 = parsed.value.indices[index + 2];
        if (index0 == index1 || index0 == index2 || index1 == index2) {
            return core::unexpected(resourceError("playback.presentation.mesh.degenerate_triangle",
                                                  "Portable Mesh triangle repeats a vertex index",
                                                  assetId, type));
        }
        const auto coordinate = [&](std::uint32_t vertex, std::size_t axis) {
            return static_cast<double>(parsed.value.positions[vertex * 3 + axis]);
        };
        const auto edge10 = coordinate(index1, 0) - coordinate(index0, 0);
        const auto edge11 = coordinate(index1, 1) - coordinate(index0, 1);
        const auto edge12 = coordinate(index1, 2) - coordinate(index0, 2);
        const auto edge20 = coordinate(index2, 0) - coordinate(index0, 0);
        const auto edge21 = coordinate(index2, 1) - coordinate(index0, 1);
        const auto edge22 = coordinate(index2, 2) - coordinate(index0, 2);
        const auto cross0 = edge11 * edge22 - edge12 * edge21;
        const auto cross1 = edge12 * edge20 - edge10 * edge22;
        const auto cross2 = edge10 * edge21 - edge11 * edge20;
        if (cross0 == 0.0 && cross1 == 0.0 && cross2 == 0.0) {
            return core::unexpected(resourceError("playback.presentation.mesh.degenerate_triangle",
                                                  "Portable Mesh triangle has zero area", assetId,
                                                  type));
        }
    }

    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = decodedBytes;
    return parsed;
}

[[nodiscard]] auto parseTexture(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedTexture> {
    const auto type = PresentationResourceType::Texture2D;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 40) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Texture2D fixed body is truncated", assetId,
                                             type, bytes.size()));
    }
    auto reader = *readerResult;
    const auto width = reader.readU32();
    const auto height = reader.readU32();
    const auto colorSpace = reader.readU32();
    const auto reserved = reader.readU32();
    if (width == 0 || width > maxTextureDimension || height == 0 || height > maxTextureDimension) {
        return core::unexpected(payloadError(
            "playback.presentation.texture.dimension_invalid",
            "Portable Texture2D dimensions are outside the v1 range", assetId, type, 24));
    }
    if (colorSpace != static_cast<std::uint32_t>(PresentationColorSpace::Linear) &&
        colorSpace != static_cast<std::uint32_t>(PresentationColorSpace::Srgb)) {
        return core::unexpected(
            payloadError("playback.presentation.texture.color_space_unsupported",
                         "Portable Texture2D color space is unsupported", assetId, type, 32));
    }
    if (reserved != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Texture2D reserved field is non-zero",
                                             assetId, type, 36));
    }

    std::size_t pixelCount = 0;
    std::size_t pixelBytes = 0;
    std::size_t expectedBytes = 40;
    if (!checkedMultiply(width, height, pixelCount) ||
        !checkedMultiply(pixelCount, 4, pixelBytes) ||
        !checkedAdd(expectedBytes, pixelBytes, expectedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Texture2D byte calculation overflowed",
                                             assetId, type, 24));
    }
    std::size_t decodedBytes = 0;
    if (!checkedAdd(pixelBytes, 12, decodedBytes)) {
        return core::unexpected(payloadError(
            "playback.presentation.payload.integer_overflow",
            "Portable Texture2D decoded byte calculation overflowed", assetId, type, 24));
    }
    if (decodedBytes > maxResourceBytes) {
        return core::unexpected(budgetError(assetId, type, maxResourceBytes, decodedBytes));
    }
    if (auto exact = validateExactSize(expectedBytes, bytes.size(), assetId, type); !exact) {
        if (expectedBytes < bytes.size()) {
            return core::unexpected(payloadError("playback.presentation.texture.pixel_size_invalid",
                                                 "Portable Texture2D pixel size is invalid",
                                                 assetId, type, 40));
        }
        return core::unexpected(std::move(exact.error()));
    }

    ParsedTexture parsed;
    parsed.value.width = width;
    parsed.value.height = height;
    parsed.value.colorSpace = static_cast<PresentationColorSpace>(colorSpace);
    const auto pixels = reader.readBytes(pixelBytes);
    parsed.value.pixelsRgba8.assign(pixels.begin(), pixels.end());
    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = decodedBytes;
    return parsed;
}

[[nodiscard]] auto isPortableAssetId(std::string_view value) noexcept -> bool {
    const auto isAlphaNumeric = [](unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9');
    };
    if (value.empty() || value.size() > 256 ||
        !isAlphaNumeric(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](unsigned char character) {
        return isAlphaNumeric(character) || character == '.' || character == '_' ||
               character == '-' || character == '/';
    });
}

[[nodiscard]] auto bytesToString(std::span<const std::byte> bytes) -> std::string {
    std::string text;
    text.resize(bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        text[index] = static_cast<char>(std::to_integer<unsigned char>(bytes[index]));
    }
    return text;
}

[[nodiscard]] auto startsWithIgnoreCase(std::string_view value, std::string_view prefix) noexcept
    -> bool {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        const auto left = static_cast<unsigned char>(value[index]);
        const auto right = static_cast<unsigned char>(prefix[index]);
        const auto lowerLeft = left >= 'A' && left <= 'Z' ? static_cast<char>(left - 'A' + 'a')
                                                          : static_cast<char>(left);
        const auto lowerRight = right >= 'A' && right <= 'Z' ? static_cast<char>(right - 'A' + 'a')
                                                             : static_cast<char>(right);
        if (lowerLeft != lowerRight) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto isShaderIdentifier(std::string_view value) noexcept -> bool {
    if (value.empty() || value.size() > maxIdentifierBytes) {
        return false;
    }
    const auto first = static_cast<unsigned char>(value.front());
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z'))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '_';
    });
}

[[nodiscard]] auto isUserIdentifier(std::string_view value) noexcept -> bool {
    return isShaderIdentifier(value) && !startsWithIgnoreCase(value, "cuexis");
}

[[nodiscard]] auto isWellFormedUtf8(std::span<const std::byte> bytes) noexcept -> bool {
    std::size_t index = 0;
    while (index < bytes.size()) {
        const auto lead = std::to_integer<std::uint8_t>(bytes[index]);
        std::size_t extra = 0;
        std::uint32_t codepoint = 0;
        std::uint32_t minimum = 0;
        if (lead <= 0x7FU) {
            ++index;
            continue;
        }
        if ((lead & 0xE0U) == 0xC0U) {
            extra = 1;
            codepoint = lead & 0x1FU;
            minimum = 0x80U;
        } else if ((lead & 0xF0U) == 0xE0U) {
            extra = 2;
            codepoint = lead & 0x0FU;
            minimum = 0x800U;
        } else if ((lead & 0xF8U) == 0xF0U) {
            extra = 3;
            codepoint = lead & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (index + extra >= bytes.size()) {
            return false;
        }
        for (std::size_t follow = 1; follow <= extra; ++follow) {
            const auto next = std::to_integer<std::uint8_t>(bytes[index + follow]);
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            codepoint = (codepoint << 6U) | (next & 0x3FU);
        }
        if (codepoint < minimum || codepoint > 0x10FFFFU ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
            return false;
        }
        index += 1 + extra;
    }
    return true;
}

[[nodiscard]] auto peekPayloadKind(std::span<const std::byte> bytes) noexcept
    -> std::optional<std::uint32_t> {
    if (bytes.size() < envelopeByteCount ||
        !std::equal(payloadMagic.begin(), payloadMagic.end(), bytes.begin())) {
        return std::nullopt;
    }
    ByteReader reader{bytes};
    reader.seek(8);
    return reader.readU32();
}

[[nodiscard]] auto readCountedBytes(ByteReader& reader, std::uint32_t byteCount,
                                    std::string_view assetId, PresentationResourceType type)
    -> core::Result<std::span<const std::byte>> {
    if (reader.remaining() < byteCount) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload body is truncated", assetId, type,
                                             reader.offset()));
    }
    return reader.readBytes(byteCount);
}

[[nodiscard]] auto validateShaderSource(std::string_view source, std::string_view assetId,
                                        PresentationResourceType type, std::size_t byteOffset)
    -> core::Result<void> {
    const auto bytes = std::as_bytes(std::span{source.data(), source.size()});
    if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        return core::unexpected(payloadError("playback.presentation.shader.source_encoding_invalid",
                                             "Shader source must be UTF-8 without a BOM", assetId,
                                             type, byteOffset));
    }
    if (source.find('\r') != std::string_view::npos) {
        return core::unexpected(payloadError("playback.presentation.shader.source_encoding_invalid",
                                             "Shader source line endings must be LF", assetId, type,
                                             byteOffset));
    }
    if (!isWellFormedUtf8(bytes)) {
        return core::unexpected(payloadError("playback.presentation.shader.source_encoding_invalid",
                                             "Shader source must be well-formed UTF-8", assetId,
                                             type, byteOffset));
    }
    const auto newline = source.find('\n');
    const auto firstLine = newline == std::string_view::npos ? source : source.substr(0, newline);
    if (firstLine != "#version 450") {
        return core::unexpected(payloadError("playback.presentation.shader.subset_invalid",
                                             "Shader source must start with #version 450", assetId,
                                             type, byteOffset));
    }
    std::size_t lineStart = 0;
    while (lineStart < source.size()) {
        const auto lineEnd = source.find('\n', lineStart);
        const auto line = source.substr(
            lineStart, (lineEnd == std::string_view::npos ? source.size() : lineEnd) - lineStart);
        std::size_t cursor = 0;
        while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
            ++cursor;
        }
        if (line.substr(cursor).starts_with("#include")) {
            return core::unexpected(payloadError("playback.presentation.shader.subset_invalid",
                                                 "Shader source must not use #include", assetId,
                                                 type, byteOffset + lineStart));
        }
        if (lineEnd == std::string_view::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return {};
}

[[nodiscard]] auto unusedNumericLanesZero(ShaderParameterType type,
                                          const std::array<float, 4>& numeric) noexcept -> bool {
    switch (type) {
    case ShaderParameterType::Float:
        return numeric[1] == 0.0F && numeric[2] == 0.0F && numeric[3] == 0.0F;
    case ShaderParameterType::Vec2:
        return numeric[2] == 0.0F && numeric[3] == 0.0F;
    case ShaderParameterType::Vec3:
        return numeric[3] == 0.0F;
    case ShaderParameterType::Vec4:
        return true;
    case ShaderParameterType::Int:
    case ShaderParameterType::Bool:
    case ShaderParameterType::Texture2D:
        return numeric[0] == 0.0F && numeric[1] == 0.0F && numeric[2] == 0.0F && numeric[3] == 0.0F;
    }
    return false;
}

} // namespace cuexis::playback::detail
