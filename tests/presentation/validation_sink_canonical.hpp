#pragma once

// Canonical hashing and per-resource validation.

#include "validation_sink_digest.hpp"

#include <cmath>

namespace cuexis::test_support {
namespace validation_detail {

class CanonicalHash final {
  public:
    explicit CanonicalHash(playback::PresentationResourceType type) noexcept {
        static constexpr char domain[] = "cuexis.portable.presentation.v1";
        sha_.update(std::as_bytes(std::span{domain, sizeof(domain)}));
        writeU32(static_cast<std::uint32_t>(type));
    }

    void writeU32(std::uint32_t value) noexcept {
        const std::array bytes{static_cast<std::byte>(value & 0xFFU),
                               static_cast<std::byte>((value >> 8U) & 0xFFU),
                               static_cast<std::byte>((value >> 16U) & 0xFFU),
                               static_cast<std::byte>((value >> 24U) & 0xFFU)};
        sha_.update(bytes);
    }

    void writeFloat(float value) noexcept {
        if (value == 0.0F) {
            value = 0.0F;
        }
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeString(std::string_view value) noexcept {
        writeU32(static_cast<std::uint32_t>(value.size()));
        sha_.update(std::as_bytes(std::span{value.data(), value.size()}));
    }

    void writeIdentity(const playback::PresentationContentIdentity& identity) noexcept {
        sha_.update(std::as_bytes(std::span{identity.sha256}));
    }

    void writeBytes(std::span<const std::byte> bytes) noexcept {
        sha_.update(bytes);
    }

    [[nodiscard]] auto finish() const noexcept -> playback::PresentationContentIdentity {
        return sha_.finish();
    }

  private:
    Sha256 sha_;
};

[[nodiscard]] inline auto meshIdentity(const playback::PortableMesh& mesh) noexcept
    -> playback::PresentationContentIdentity {
    CanonicalHash hash{playback::PresentationResourceType::Mesh};
    hash.writeU32(static_cast<std::uint32_t>(mesh.positions.size() / 3U));
    hash.writeU32(mesh.uv0.empty() ? 0U : 1U);
    for (const auto value : mesh.positions) {
        hash.writeFloat(value);
    }
    for (const auto value : mesh.uv0) {
        hash.writeFloat(value);
    }
    hash.writeU32(static_cast<std::uint32_t>(mesh.indices.size()));
    for (const auto value : mesh.indices) {
        hash.writeU32(value);
    }
    return hash.finish();
}

[[nodiscard]] inline auto textureIdentity(const playback::PortableTexture2D& texture) noexcept
    -> playback::PresentationContentIdentity {
    CanonicalHash hash{playback::PresentationResourceType::Texture2D};
    hash.writeU32(texture.width);
    hash.writeU32(texture.height);
    hash.writeU32(static_cast<std::uint32_t>(texture.colorSpace));
    hash.writeBytes(texture.pixelsRgba8);
    return hash.finish();
}

[[nodiscard]] inline auto materialIdentity(const playback::PortableUnlitMaterial& material) noexcept
    -> playback::PresentationContentIdentity {
    CanonicalHash hash{playback::PresentationResourceType::UnlitMaterial};
    hash.writeU32(static_cast<std::uint32_t>(material.alphaMode));
    hash.writeU32(material.doubleSided ? 1U : 0U);
    for (const auto value : material.baseColor) {
        hash.writeFloat(value);
    }
    hash.writeU32(material.baseColorTexture ? 1U : 0U);
    if (material.baseColorTexture) {
        hash.writeString(material.baseColorTexture->assetId);
        hash.writeIdentity(material.baseColorTexture->identity);
    }
    return hash.finish();
}

[[nodiscard]] inline auto validateMesh(const playback::PresentationResourceRef& reference,
                                       const playback::PortableMesh& mesh,
                                       std::uint64_t& decodedBytes) -> core::Result<void> {
    if (mesh.positions.empty() || mesh.positions.size() % 3U != 0) {
        return core::unexpected(
            referenceError("Portable Mesh position array is invalid", &reference));
    }
    const auto vertexCount = mesh.positions.size() / 3U;
    if (vertexCount > maxMeshVertices || mesh.indices.empty() || mesh.indices.size() % 3U != 0 ||
        mesh.indices.size() > maxMeshIndices ||
        (!mesh.uv0.empty() && mesh.uv0.size() != vertexCount * 2U)) {
        return core::unexpected(referenceError("Portable Mesh counts are invalid", &reference));
    }
    for (const auto value : mesh.positions) {
        if (!std::isfinite(value) || std::abs(value) > maxPositionMagnitude) {
            return core::unexpected(
                referenceError("Portable Mesh position is outside the v1 range", &reference));
        }
    }
    for (const auto value : mesh.uv0) {
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
            return core::unexpected(
                referenceError("Portable Mesh UV0 is outside [0, 1]", &reference));
        }
    }
    if (!std::all_of(mesh.indices.begin(), mesh.indices.end(),
                     [&](std::uint32_t index) { return index < vertexCount; })) {
        return core::unexpected(referenceError("Portable Mesh index is out of range", &reference));
    }
    for (std::size_t index = 0; index < mesh.indices.size(); index += 3U) {
        const auto index0 = mesh.indices[index];
        const auto index1 = mesh.indices[index + 1U];
        const auto index2 = mesh.indices[index + 2U];
        if (index0 == index1 || index0 == index2 || index1 == index2) {
            return core::unexpected(
                referenceError("Portable Mesh triangle repeats a vertex index", &reference));
        }
        const auto coordinate = [&](std::uint32_t vertex, std::size_t axis) {
            return static_cast<double>(mesh.positions[vertex * 3U + axis]);
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
            return core::unexpected(
                referenceError("Portable Mesh triangle has zero area", &reference));
        }
    }

    std::array<float, 3> minimum{mesh.positions[0], mesh.positions[1], mesh.positions[2]};
    auto maximum = minimum;
    for (std::size_t index = 0; index < vertexCount; ++index) {
        for (std::size_t component = 0; component < 3; ++component) {
            const auto value = mesh.positions[index * 3U + component];
            minimum[component] = std::min(minimum[component], value);
            maximum[component] = std::max(maximum[component], value);
        }
    }
    for (std::size_t component = 0; component < 3; ++component) {
        if (!std::isfinite(mesh.boundsMin[component]) ||
            !std::isfinite(mesh.boundsMax[component]) ||
            mesh.boundsMin[component] != minimum[component] ||
            mesh.boundsMax[component] != maximum[component]) {
            return core::unexpected(
                referenceError("Portable Mesh derived bounds are invalid", &reference));
        }
    }

    std::uint64_t positionBytes{};
    std::uint64_t uvBytes{};
    std::uint64_t indexBytes{};
    if (!checkedMultiply(mesh.positions.size(), sizeof(float), positionBytes) ||
        !checkedMultiply(mesh.uv0.size(), sizeof(float), uvBytes) ||
        !checkedMultiply(mesh.indices.size(), sizeof(std::uint32_t), indexBytes) ||
        !checkedAdd(positionBytes, uvBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, indexBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, 24ULL, decodedBytes)) {
        return core::unexpected(referenceError("Portable Mesh byte count overflowed", &reference));
    }
    return {};
}

[[nodiscard]] inline auto validateTexture(const playback::PresentationResourceRef& reference,
                                          const playback::PortableTexture2D& texture,
                                          std::uint64_t& decodedBytes) -> core::Result<void> {
    if (texture.width == 0 || texture.height == 0 || texture.width > maxTextureDimension ||
        texture.height > maxTextureDimension ||
        (texture.colorSpace != playback::PresentationColorSpace::Linear &&
         texture.colorSpace != playback::PresentationColorSpace::Srgb)) {
        return core::unexpected(
            referenceError("Portable Texture2D metadata is invalid", &reference));
    }
    std::uint64_t pixelCount{};
    std::uint64_t pixelBytes{};
    if (!checkedMultiply(texture.width, texture.height, pixelCount) ||
        !checkedMultiply(pixelCount, 4ULL, pixelBytes) ||
        pixelBytes != texture.pixelsRgba8.size() || !checkedAdd(pixelBytes, 12ULL, decodedBytes)) {
        return core::unexpected(
            referenceError("Portable Texture2D byte count is invalid", &reference));
    }
    return {};
}

[[nodiscard]] inline auto validateMaterial(const playback::PresentationResourceRef& reference,
                                           const playback::PortableUnlitMaterial& material,
                                           std::uint64_t& decodedBytes) -> core::Result<void> {
    if (material.alphaMode != playback::PresentationAlphaMode::Opaque &&
        material.alphaMode != playback::PresentationAlphaMode::Blend) {
        return core::unexpected(
            referenceError("Portable Material alpha mode is invalid", &reference));
    }
    for (const auto value : material.baseColor) {
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
            return core::unexpected(
                referenceError("Portable Material color is invalid", &reference));
        }
    }
    if (material.alphaMode == playback::PresentationAlphaMode::Opaque &&
        material.baseColor[3] != 1.0F) {
        return core::unexpected(
            referenceError("Portable Opaque Material alpha must equal 1", &reference));
    }
    decodedBytes = 32;
    if (material.baseColorTexture) {
        if (material.baseColorTexture->type != playback::PresentationResourceType::Texture2D ||
            !isPortableAssetId(material.baseColorTexture->assetId)) {
            return core::unexpected(
                referenceError("Portable Material texture reference is invalid", &reference));
        }
        if (!checkedAdd(decodedBytes, 37ULL + material.baseColorTexture->assetId.size(),
                        decodedBytes)) {
            return core::unexpected(
                referenceError("Portable Material byte count overflowed", &reference));
        }
    }
    return {};
}

[[nodiscard]] inline auto validateShader(const playback::PresentationResourceRef& reference,
                                         const playback::PortableShader& shader,
                                         std::uint64_t& decodedBytes) -> core::Result<void> {
    decodedBytes = 48;
    const auto addString = [&](std::string_view value) -> bool {
        return checkedAdd(decodedBytes, 4ULL + value.size(), decodedBytes);
    };
    if (!addString(shader.vertexEntry) || !addString(shader.fragmentEntry) ||
        !addString(shader.requiredRendererProfile)) {
        return core::unexpected(
            referenceError("Portable Shader byte count overflowed", &reference));
    }
    for (const auto& keyword : shader.variantKeywords) {
        if (!addString(keyword)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    for (const auto& parameter : shader.parameters) {
        if (!addString(parameter.name)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    for (const auto& binding : shader.bindings) {
        if (!addString(binding.name)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    for (const auto& extension : shader.requiredHostExtensions) {
        if (!addString(extension)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    if (!checkedAdd(decodedBytes, shader.vertexSource.size() + shader.fragmentSource.size(),
                    decodedBytes)) {
        return core::unexpected(
            referenceError("Portable Shader byte count overflowed", &reference));
    }
    return {};
}

[[nodiscard]] inline auto
validateParameterizedMaterial(const playback::PresentationResourceRef& reference,
                              const playback::PortableParameterizedMaterial& material,
                              std::uint64_t& decodedBytes) -> core::Result<void> {
    if (material.alphaMode != playback::PresentationAlphaMode::Opaque &&
        material.alphaMode != playback::PresentationAlphaMode::Blend) {
        return core::unexpected(
            referenceError("Portable Material alpha mode is invalid", &reference));
    }
    if (material.shader.type != playback::PresentationResourceType::Shader ||
        !isPortableAssetId(material.shader.assetId)) {
        return core::unexpected(
            referenceError("Parameterized Material shader reference is invalid", &reference));
    }
    decodedBytes = 24ULL + 37ULL + material.shader.assetId.size();
    for (const auto& keyword : material.selectedKeywords) {
        if (!checkedAdd(decodedBytes, 4ULL + keyword.size(), decodedBytes)) {
            return core::unexpected(
                referenceError("Parameterized Material byte count overflowed", &reference));
        }
    }
    for (const auto& parameter : material.parameters) {
        if (parameter.type == playback::ShaderParameterType::Texture2D) {
            if (!parameter.texture ||
                parameter.texture->type != playback::PresentationResourceType::Texture2D ||
                !isPortableAssetId(parameter.texture->assetId)) {
                return core::unexpected(referenceError(
                    "Parameterized Material texture parameter is invalid", &reference));
            }
            if (!checkedAdd(decodedBytes,
                            8ULL + parameter.name.size() + 37ULL +
                                parameter.texture->assetId.size(),
                            decodedBytes)) {
                return core::unexpected(
                    referenceError("Parameterized Material byte count overflowed", &reference));
            }
        } else if (!checkedAdd(decodedBytes, 20ULL + parameter.name.size(), decodedBytes)) {
            return core::unexpected(
                referenceError("Parameterized Material byte count overflowed", &reference));
        }
    }
    return {};
}

[[nodiscard]] inline auto
expectedParameterizedDependencies(const playback::PortableParameterizedMaterial& material)
    -> std::vector<playback::PresentationResourceRef> {
    std::vector<playback::PresentationResourceRef> dependencies;
    dependencies.push_back(material.shader);
    for (const auto& parameter : material.parameters) {
        if (parameter.type == playback::ShaderParameterType::Texture2D && parameter.texture) {
            dependencies.push_back(*parameter.texture);
        }
    }
    return dependencies;
}

} // namespace validation_detail
} // namespace cuexis::test_support
