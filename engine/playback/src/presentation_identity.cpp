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

[[nodiscard]] auto completeParameterizedMaterial(std::string_view assetId,
                                                 ParsedParameterizedMaterial& parsed,
                                                 const PortableShader& shader)
    -> core::Result<void> {
    const auto type = PresentationResourceType::ParameterizedMaterial;
    if (parsed.parameterCount != shader.parameters.size()) {
        return core::unexpected(
            resourceError("playback.presentation.material.parameter_mismatch",
                          "Parameterized Material parameter count does not match the shader schema",
                          assetId, type));
    }
    for (const auto& keyword : parsed.selectedKeywords) {
        if (std::find(shader.variantKeywords.begin(), shader.variantKeywords.end(), keyword) ==
            shader.variantKeywords.end()) {
            return core::unexpected(
                resourceError("playback.presentation.material.keyword_undeclared",
                              "Selected keyword is not declared by the shader", assetId, type)
                    .withContext("keyword", keyword));
        }
    }
    ByteReader reader{parsed.parameterBytes};
    parsed.value.selectedKeywords = parsed.selectedKeywords;
    parsed.value.parameters.reserve(shader.parameters.size());
    std::uint32_t textureParameters = 0;
    for (const auto& schema : shader.parameters) {
        ShaderParameterValue value;
        value.name = schema.name;
        value.type = schema.type;
        if (schema.type == ShaderParameterType::Texture2D) {
            if (reader.remaining() < 4) {
                return core::unexpected(payloadError(
                    "playback.presentation.payload.truncated",
                    "Parameterized Material texture parameter is truncated", assetId, type,
                    envelopeByteCount + 24 + parsed.shaderAssetId.size() +
                        (parsed.parameterBytes.size() - reader.remaining())));
            }
            const auto textureBytes = reader.readU32();
            if (textureBytes < 1 || textureBytes > 256 || reader.remaining() < textureBytes) {
                return core::unexpected(payloadError(
                    "playback.presentation.material.shader_reference_invalid",
                    "Parameterized Material texture AssetId length is outside the v1 range",
                    assetId, type, reader.offset()));
            }
            auto encoded = reader.readBytes(textureBytes);
            auto textureAssetId = bytesToString(encoded);
            if (!isPortableAssetId(textureAssetId)) {
                return core::unexpected(
                    payloadError("playback.presentation.material.shader_reference_invalid",
                                 "Parameterized Material texture AssetId is not portable", assetId,
                                 type, reader.offset() - textureBytes));
            }
            ++textureParameters;
            if (textureParameters > maxMaterialTextureParameters) {
                return core::unexpected(
                    budgetError(assetId, type, maxMaterialTextureParameters, textureParameters));
            }
            parsed.textureAssetIds.push_back(textureAssetId);
            value.texture = PresentationResourceRef{
                PresentationResourceType::Texture2D, std::move(textureAssetId), {}};
        } else if (schema.type == ShaderParameterType::Int) {
            if (reader.remaining() < 16) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.truncated",
                                 "Parameterized Material int parameter is truncated", assetId, type,
                                 reader.offset()));
            }
            value.integer = reader.readI32();
            const auto padding0 = reader.readU32();
            const auto padding1 = reader.readU32();
            const auto padding2 = reader.readU32();
            if (padding0 != 0 || padding1 != 0 || padding2 != 0) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.reserved_nonzero",
                                 "Parameterized Material int parameter padding must be zero",
                                 assetId, type, reader.offset() - 12));
            }
        } else if (schema.type == ShaderParameterType::Bool) {
            if (reader.remaining() < 16) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.truncated",
                                 "Parameterized Material bool parameter is truncated", assetId,
                                 type, reader.offset()));
            }
            const auto boolean = reader.readU32();
            const auto padding0 = reader.readU32();
            const auto padding1 = reader.readU32();
            const auto padding2 = reader.readU32();
            if (boolean > 1 || padding0 != 0 || padding1 != 0 || padding2 != 0) {
                return core::unexpected(
                    payloadError("playback.presentation.material.parameter_mismatch",
                                 "Parameterized Material bool parameter is invalid", assetId, type,
                                 reader.offset() - 16));
            }
            value.boolean = boolean != 0;
        } else {
            if (reader.remaining() < 16) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.truncated",
                                 "Parameterized Material numeric parameter is truncated", assetId,
                                 type, reader.offset()));
            }
            for (auto& lane : value.numeric) {
                lane = reader.readFloat();
            }
            if (!std::isfinite(value.numeric[0]) || !std::isfinite(value.numeric[1]) ||
                !std::isfinite(value.numeric[2]) || !std::isfinite(value.numeric[3]) ||
                !unusedNumericLanesZero(schema.type, value.numeric)) {
                return core::unexpected(
                    payloadError("playback.presentation.material.parameter_mismatch",
                                 "Parameterized Material numeric parameter is invalid", assetId,
                                 type, reader.offset() - 16));
            }
        }
        parsed.value.parameters.push_back(std::move(value));
    }
    if (reader.remaining() != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload contains trailing bytes", assetId,
                                             type, reader.offset()));
    }

    parsed.decodedByteCount = 24;
    if (!checkedAddU64(parsed.decodedByteCount, 37ULL + parsed.shaderAssetId.size(),
                       parsed.decodedByteCount)) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    for (const auto& keyword : parsed.selectedKeywords) {
        if (!checkedAddU64(parsed.decodedByteCount, 4ULL + keyword.size(),
                           parsed.decodedByteCount)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& parameter : parsed.value.parameters) {
        std::uint64_t extra = 20ULL + parameter.name.size();
        if (parameter.type == ShaderParameterType::Texture2D && parameter.texture) {
            extra = 8ULL + parameter.name.size() + 37ULL + parameter.texture->assetId.size();
        }
        if (!checkedAddU64(parsed.decodedByteCount, extra, parsed.decodedByteCount)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    if (parsed.decodedByteCount > maxResourceBytes) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    return {};
}

[[nodiscard]] auto meshIdentity(const PortableMesh& mesh) noexcept -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::Mesh};
    hash.writeU32(static_cast<std::uint32_t>(mesh.positions.size() / 3));
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

[[nodiscard]] auto textureIdentity(const PortableTexture2D& texture) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::Texture2D};
    hash.writeU32(texture.width);
    hash.writeU32(texture.height);
    hash.writeU32(static_cast<std::uint32_t>(texture.colorSpace));
    hash.writeBytes(texture.pixelsRgba8);
    return hash.finish();
}

[[nodiscard]] auto materialIdentity(const PortableUnlitMaterial& material) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::UnlitMaterial};
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

[[nodiscard]] auto shaderIdentity(const PortableShader& shader) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::Shader};
    hash.writeString(shader.vertexEntry);
    hash.writeString(shader.fragmentEntry);
    auto keywords = shader.variantKeywords;
    std::sort(keywords.begin(), keywords.end());
    hash.writeU32(static_cast<std::uint32_t>(keywords.size()));
    for (const auto& keyword : keywords) {
        hash.writeString(keyword);
    }
    hash.writeU32(static_cast<std::uint32_t>(shader.parameters.size()));
    for (const auto& parameter : shader.parameters) {
        hash.writeString(parameter.name);
        hash.writeU32(static_cast<std::uint32_t>(parameter.type));
        hash.writeU32(parameter.set);
        hash.writeU32(parameter.binding);
        for (const auto lane : parameter.defaultNumeric) {
            hash.writeFloat(lane);
        }
        hash.writeI32(parameter.defaultInt);
        hash.writeU32(parameter.defaultBool ? 1U : 0U);
    }
    hash.writeU32(static_cast<std::uint32_t>(shader.bindings.size()));
    for (const auto& binding : shader.bindings) {
        hash.writeU32(binding.set);
        hash.writeU32(binding.binding);
        hash.writeU32(static_cast<std::uint32_t>(binding.type));
        hash.writeString(binding.name);
    }
    hash.writeU32(static_cast<std::uint32_t>(shader.defaultAlphaMode));
    hash.writeU32(shader.defaultDoubleSided ? 1U : 0U);
    hash.writeString(shader.requiredRendererProfile);
    auto extensions = shader.requiredHostExtensions;
    std::sort(extensions.begin(), extensions.end());
    hash.writeU32(static_cast<std::uint32_t>(extensions.size()));
    for (const auto& extension : extensions) {
        hash.writeString(extension);
    }
    hash.writeString(shader.vertexSource);
    hash.writeString(shader.fragmentSource);
    return hash.finish();
}

[[nodiscard]] auto
parameterizedMaterialIdentity(const PortableParameterizedMaterial& material) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::ParameterizedMaterial};
    hash.writeU32(static_cast<std::uint32_t>(material.alphaMode));
    hash.writeU32(material.doubleSided ? 1U : 0U);
    hash.writeString(material.shader.assetId);
    hash.writeIdentity(material.shader.identity);
    auto keywords = material.selectedKeywords;
    std::sort(keywords.begin(), keywords.end());
    hash.writeU32(static_cast<std::uint32_t>(keywords.size()));
    for (const auto& keyword : keywords) {
        hash.writeString(keyword);
    }
    hash.writeU32(static_cast<std::uint32_t>(material.parameters.size()));
    for (const auto& parameter : material.parameters) {
        hash.writeString(parameter.name);
        hash.writeU32(static_cast<std::uint32_t>(parameter.type));
        if (parameter.type == ShaderParameterType::Texture2D) {
            hash.writeU32(parameter.texture ? 1U : 0U);
            if (parameter.texture) {
                hash.writeString(parameter.texture->assetId);
                hash.writeIdentity(parameter.texture->identity);
            }
        } else if (parameter.type == ShaderParameterType::Int) {
            hash.writeI32(parameter.integer);
        } else if (parameter.type == ShaderParameterType::Bool) {
            hash.writeU32(parameter.boolean ? 1U : 0U);
        } else {
            for (const auto lane : parameter.numeric) {
                hash.writeFloat(lane);
            }
        }
    }
    return hash.finish();
}

[[nodiscard]] auto resourceValuesEqual(const PortableResourceValue& left,
                                       const PortableResourceValue& right) noexcept -> bool {
    if (left.index() != right.index()) {
        return false;
    }
    if (const auto* leftMesh = std::get_if<PortableMesh>(&left)) {
        const auto& rightMesh = std::get<PortableMesh>(right);
        return leftMesh->positions == rightMesh.positions && leftMesh->uv0 == rightMesh.uv0 &&
               leftMesh->indices == rightMesh.indices;
    }
    if (const auto* leftTexture = std::get_if<PortableTexture2D>(&left)) {
        const auto& rightTexture = std::get<PortableTexture2D>(right);
        return leftTexture->width == rightTexture.width &&
               leftTexture->height == rightTexture.height &&
               leftTexture->colorSpace == rightTexture.colorSpace &&
               leftTexture->pixelsRgba8 == rightTexture.pixelsRgba8;
    }
    if (const auto* leftUnlit = std::get_if<PortableUnlitMaterial>(&left)) {
        const auto& rightUnlit = std::get<PortableUnlitMaterial>(right);
        return std::equal(std::begin(leftUnlit->baseColor), std::end(leftUnlit->baseColor),
                          std::begin(rightUnlit.baseColor)) &&
               leftUnlit->alphaMode == rightUnlit.alphaMode &&
               leftUnlit->doubleSided == rightUnlit.doubleSided &&
               leftUnlit->baseColorTexture == rightUnlit.baseColorTexture;
    }
    if (const auto* leftShader = std::get_if<PortableShader>(&left)) {
        const auto& rightShader = std::get<PortableShader>(right);
        return leftShader->vertexSource == rightShader.vertexSource &&
               leftShader->fragmentSource == rightShader.fragmentSource &&
               leftShader->vertexEntry == rightShader.vertexEntry &&
               leftShader->fragmentEntry == rightShader.fragmentEntry &&
               leftShader->variantKeywords == rightShader.variantKeywords &&
               leftShader->defaultAlphaMode == rightShader.defaultAlphaMode &&
               leftShader->defaultDoubleSided == rightShader.defaultDoubleSided &&
               leftShader->requiredRendererProfile == rightShader.requiredRendererProfile &&
               leftShader->requiredHostExtensions == rightShader.requiredHostExtensions &&
               leftShader->bindings.size() == rightShader.bindings.size() &&
               leftShader->parameters.size() == rightShader.parameters.size();
    }
    const auto& leftMaterial = std::get<PortableParameterizedMaterial>(left);
    const auto& rightMaterial = std::get<PortableParameterizedMaterial>(right);
    return leftMaterial.shader == rightMaterial.shader &&
           leftMaterial.alphaMode == rightMaterial.alphaMode &&
           leftMaterial.doubleSided == rightMaterial.doubleSided &&
           leftMaterial.selectedKeywords == rightMaterial.selectedKeywords &&
           leftMaterial.parameters.size() == rightMaterial.parameters.size();
}

[[nodiscard]] auto looksPortable(std::span<const std::byte> bytes) noexcept -> bool {
    if (bytes.size() >= payloadMagic.size() &&
        std::equal(payloadMagic.begin(), payloadMagic.end(), bytes.begin())) {
        return true;
    }
    static constexpr std::array<std::byte, 6> prefix{
        std::byte{'C'}, std::byte{'X'}, std::byte{'P'},
        std::byte{'R'}, std::byte{'E'}, std::byte{'S'},
    };
    if (bytes.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), bytes.begin())) {
        return true;
    }
    if (bytes.size() < envelopeByteCount) {
        return false;
    }
    ByteReader reader{bytes};
    reader.seek(8);
    const auto kind = reader.readU32();
    const auto version = reader.readU32();
    const auto byteCount = reader.readU64();
    return kind >= 1 && kind <= 5 && version == 1 && byteCount == bytes.size();
}

[[nodiscard]] auto collectRequiredResources(const chart::ChartRuntime& chartRuntime)
    -> core::Result<std::map<std::string, PresentationResourceType, std::less<>>> {
    std::map<std::string, PresentationResourceType, std::less<>> required;
    const auto add = [&](std::string_view assetId,
                         PresentationResourceType type) -> core::Result<void> {
        const auto [found, inserted] = required.emplace(std::string{assetId}, type);
        if (!inserted && found->second != type) {
            return core::unexpected(
                resourceError("playback.presentation.reference.invalid",
                              "One AssetId is referenced with multiple presentation resource types",
                              assetId, type));
        }
        return {};
    };
    for (const auto& object : chartRuntime.objects) {
        if (!object.components.renderable) {
            continue;
        }
        if (auto result =
                add(object.components.renderable->mesh.value, PresentationResourceType::Mesh);
            !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (auto result = add(object.components.renderable->material.value,
                              PresentationResourceType::UnlitMaterial);
            !result) {
            return core::unexpected(std::move(result.error()));
        }
        const auto& behaviorReference = object.components.behavior;
        if (!behaviorReference) {
            continue;
        }
        const auto behavior =
            std::lower_bound(chartRuntime.behaviors.begin(), chartRuntime.behaviors.end(),
                             behaviorReference->behavior,
                             [](const chart::RuntimeBehavior& candidate,
                                const chart::BehaviorId& id) { return candidate.id < id; });
        if (behavior == chartRuntime.behaviors.end() ||
            behavior->id != behaviorReference->behavior) {
            continue;
        }
        for (const auto& track : behavior->stepTracks) {
            if (track.property != chart::BehaviorStepProperty::RenderMaterial) {
                continue;
            }
            for (const auto& event : track.events) {
                if (const auto* assetId = std::get_if<chart::AssetId>(&event.value)) {
                    if (auto result = add(assetId->value, PresentationResourceType::UnlitMaterial);
                        !result) {
                        return core::unexpected(std::move(result.error()));
                    }
                }
            }
        }
    }
    return required;
}

[[nodiscard]] auto unavailableResource(std::string_view assetId, PresentationResourceType type,
                                       core::Error cause) -> core::Error {
    for (const core::Error* current = &cause; current != nullptr; current = current->cause()) {
        if (current->code() == "assets.resource.provider_too_large" ||
            current->code() == "content.provider.too_large" ||
            current->code() == "content.filesystem.too_large") {
            return resourceError("playback.presentation.resource.budget_exceeded",
                                 "Portable resource encoded byte budget was exceeded", assetId,
                                 type)
                .withContext("limit", std::to_string(maxResourceBytes))
                .withContext("actual", "greater_than_limit")
                .withCause(std::move(cause));
        }
    }
    return resourceError("playback.presentation.resource.missing",
                         "Required portable presentation resource is unavailable", assetId, type)
        .withCause(std::move(cause));
}

[[nodiscard]] auto presentationRefByteCount(const PresentationResourceRef& reference) noexcept
    -> std::uint64_t {
    return 37ULL + reference.assetId.size();
}

} // namespace cuexis::playback::detail
