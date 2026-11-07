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

[[nodiscard]] auto parseMaterial(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedMaterial> {
    const auto type = PresentationResourceType::UnlitMaterial;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 56) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Unlit Material fixed body is truncated",
                                             assetId, type, bytes.size()));
    }
    auto reader = *readerResult;
    const auto alphaMode = reader.readU32();
    const auto doubleSided = reader.readU32();
    std::array<float, 4> baseColor{};
    for (auto& value : baseColor) {
        value = reader.readFloat();
    }
    const auto textureAssetIdByteCount = reader.readU32();
    const auto reserved = reader.readU32();
    if ((alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
         alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Blend)) ||
        doubleSided > 1) {
        return core::unexpected(
            payloadError("playback.presentation.material.value_invalid",
                         "Portable Unlit Material alpha mode or double-sided value is invalid",
                         assetId, type, alphaMode > 2 ? 24 : 28));
    }
    for (std::size_t index = 0; index < baseColor.size(); ++index) {
        if (!std::isfinite(baseColor[index]) || baseColor[index] < 0.0F ||
            baseColor[index] > 1.0F) {
            return core::unexpected(
                payloadError("playback.presentation.material.value_invalid",
                             "Portable Unlit Material base color is outside [0, 1]", assetId, type,
                             32 + index * sizeof(float)));
        }
    }
    if (alphaMode == static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
        baseColor[3] != 1.0F) {
        return core::unexpected(payloadError(
            "playback.presentation.material.value_invalid",
            "Opaque Portable Unlit Material base color alpha must be one", assetId, type, 44));
    }
    if (textureAssetIdByteCount > 256) {
        return core::unexpected(payloadError(
            "playback.presentation.material.texture_reference_invalid",
            "Portable Unlit Material texture AssetId exceeds the v1 limit", assetId, type, 48));
    }
    if (reserved != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Unlit Material reserved field is non-zero",
                                             assetId, type, 52));
    }

    std::size_t expectedBytes = 56;
    if (!checkedAdd(expectedBytes, textureAssetIdByteCount, expectedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Unlit Material byte calculation overflowed",
                                             assetId, type, 48));
    }
    if (auto exact = validateExactSize(expectedBytes, bytes.size(), assetId, type); !exact) {
        return core::unexpected(std::move(exact.error()));
    }

    ParsedMaterial parsed;
    parsed.value.alphaMode = static_cast<PresentationAlphaMode>(alphaMode);
    parsed.value.doubleSided = doubleSided != 0;
    std::copy(baseColor.begin(), baseColor.end(), parsed.value.baseColor);
    if (textureAssetIdByteCount != 0) {
        const auto encodedId = reader.readBytes(textureAssetIdByteCount);
        std::string textureAssetId;
        textureAssetId.reserve(textureAssetIdByteCount);
        for (const auto value : encodedId) {
            textureAssetId.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
        }
        if (!isPortableAssetId(textureAssetId)) {
            return core::unexpected(payloadError(
                "playback.presentation.material.texture_reference_invalid",
                "Portable Unlit Material texture AssetId is not portable", assetId, type, 56));
        }
        parsed.textureAssetId = std::move(textureAssetId);
    }
    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = 32 + (parsed.textureAssetId ? 37 + parsed.textureAssetId->size() : 0);
    if (parsed.decodedByteCount > maxResourceBytes) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    return parsed;
}

[[nodiscard]] auto parseShader(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedShader> {
    const auto type = PresentationResourceType::Shader;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 72) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Shader fixed body is truncated", assetId,
                                             type, bytes.size()));
    }
    auto reader = *readerResult;
    const auto flags = reader.readU32();
    const auto vertexEntryByteCount = reader.readU32();
    const auto fragmentEntryByteCount = reader.readU32();
    const auto keywordCount = reader.readU32();
    const auto parameterCount = reader.readU32();
    const auto bindingCount = reader.readU32();
    const auto hostExtensionCount = reader.readU32();
    const auto defaultAlphaMode = reader.readU32();
    const auto defaultDoubleSided = reader.readU32();
    const auto profileByteCount = reader.readU32();
    const auto vertexSourceByteCount = reader.readU32();
    const auto fragmentSourceByteCount = reader.readU32();
    if (flags != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Shader reserved flags are non-zero", assetId,
                                             type, 24));
    }
    if (vertexEntryByteCount < 1 || vertexEntryByteCount > maxEntryBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.entry_invalid",
                                             "Shader vertex entry length is outside the v1 range",
                                             assetId, type, 28));
    }
    if (fragmentEntryByteCount < 1 || fragmentEntryByteCount > maxEntryBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.entry_invalid",
                                             "Shader fragment entry length is outside the v1 range",
                                             assetId, type, 32));
    }
    if (keywordCount > maxShaderKeywords) {
        return core::unexpected(payloadError("playback.presentation.shader.keyword_invalid",
                                             "Shader keyword count exceeds the v1 limit", assetId,
                                             type, 36));
    }
    if (parameterCount > maxShaderParameters) {
        return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                             "Shader parameter count exceeds the v1 limit", assetId,
                                             type, 40));
    }
    if (bindingCount > maxShaderBindings) {
        return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                             "Shader binding count exceeds the v1 limit", assetId,
                                             type, 44));
    }
    if (hostExtensionCount > maxShaderHostExtensions) {
        return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                             "Shader host extension count exceeds the v1 limit",
                                             assetId, type, 48));
    }
    if ((defaultAlphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
         defaultAlphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Blend)) ||
        defaultDoubleSided > 1) {
        return core::unexpected(
            payloadError("playback.presentation.shader.schema_invalid",
                         "Shader default alpha mode or double-sided value is invalid", assetId,
                         type, defaultAlphaMode > 2 ? 52 : 56));
    }
    if (profileByteCount < 1 || profileByteCount > maxEntryBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.profile_unsupported",
                                             "Shader profile length is outside the v1 range",
                                             assetId, type, 60));
    }
    if (vertexSourceByteCount < 1 || vertexSourceByteCount > maxShaderSourceBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.subset_invalid",
                                             "Shader vertex source length is outside the v1 range",
                                             assetId, type, 64));
    }
    if (fragmentSourceByteCount < 1 || fragmentSourceByteCount > maxShaderSourceBytes) {
        return core::unexpected(payloadError(
            "playback.presentation.shader.subset_invalid",
            "Shader fragment source length is outside the v1 range", assetId, type, 68));
    }
    if (vertexSourceByteCount > maxShaderSourceTotalBytes - fragmentSourceByteCount) {
        return core::unexpected(budgetError(assetId, type, maxShaderSourceTotalBytes,
                                            static_cast<std::uint64_t>(vertexSourceByteCount) +
                                                fragmentSourceByteCount));
    }

    auto readName = [&](std::uint32_t byteCount) -> core::Result<std::string> {
        auto encoded = readCountedBytes(reader, byteCount, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        return bytesToString(*encoded);
    };
    auto vertexEntry = readName(vertexEntryByteCount);
    if (!vertexEntry) {
        return core::unexpected(std::move(vertexEntry.error()));
    }
    auto fragmentEntry = readName(fragmentEntryByteCount);
    if (!fragmentEntry) {
        return core::unexpected(std::move(fragmentEntry.error()));
    }
    auto profile = readName(profileByteCount);
    if (!profile) {
        return core::unexpected(std::move(profile.error()));
    }
    if (!isUserIdentifier(*vertexEntry) || !isUserIdentifier(*fragmentEntry)) {
        return core::unexpected(payloadError("playback.presentation.shader.entry_invalid",
                                             "Shader entry name is not a portable identifier",
                                             assetId, type, 72));
    }
    if (*profile != builtinRendererProfile && !isUserIdentifier(*profile)) {
        return core::unexpected(payloadError("playback.presentation.shader.profile_unsupported",
                                             "Shader required renderer profile is unsupported",
                                             assetId, type, 72));
    }

    ParsedShader parsed;
    parsed.value.vertexEntry = std::move(*vertexEntry);
    parsed.value.fragmentEntry = std::move(*fragmentEntry);
    parsed.value.requiredRendererProfile = std::move(*profile);
    parsed.value.defaultAlphaMode = static_cast<PresentationAlphaMode>(defaultAlphaMode);
    parsed.value.defaultDoubleSided = defaultDoubleSided != 0;
    parsed.value.variantKeywords.reserve(keywordCount);
    std::set<std::string, std::less<>> uniqueKeywords;
    for (std::uint32_t index = 0; index < keywordCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader keyword list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        auto name = bytesToString(*encoded);
        if (!isUserIdentifier(name)) {
            return core::unexpected(payloadError("playback.presentation.shader.keyword_invalid",
                                                 "Shader keyword is not a portable identifier",
                                                 assetId, type, reader.offset() - nameBytes));
        }
        if (!uniqueKeywords.insert(name).second) {
            return core::unexpected(payloadError("playback.presentation.shader.keyword_invalid",
                                                 "Shader keywords must be unique", assetId, type,
                                                 reader.offset() - nameBytes));
        }
        parsed.value.variantKeywords.push_back(std::move(name));
    }

    parsed.value.parameters.reserve(parameterCount);
    std::set<std::string, std::less<>> uniqueParameterNames;
    for (std::uint32_t index = 0; index < parameterCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader parameter list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        ShaderParameterSchemaEntry entry;
        entry.name = bytesToString(*encoded);
        if (reader.remaining() < 36) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader parameter record is truncated",
                                                 assetId, type, reader.offset()));
        }
        entry.type = static_cast<ShaderParameterType>(reader.readU32());
        entry.set = reader.readU32();
        entry.binding = reader.readU32();
        for (auto& lane : entry.defaultNumeric) {
            lane = reader.readFloat();
        }
        entry.defaultInt = reader.readI32();
        const auto defaultBool = reader.readU32();
        if (!isUserIdentifier(entry.name) || !uniqueParameterNames.insert(entry.name).second) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader parameter name is invalid or duplicated",
                                                 assetId, type, reader.offset()));
        }
        if (static_cast<std::uint32_t>(entry.type) < 1 ||
            static_cast<std::uint32_t>(entry.type) > 7) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader parameter type is unsupported", assetId,
                                                 type, reader.offset() - 32));
        }
        if (entry.set != 0 || entry.binding < 1 || entry.binding > maxShaderBindings) {
            return core::unexpected(
                payloadError(entry.set == 0 && entry.binding == 0
                                 ? "playback.presentation.shader.reserved_binding"
                                 : "playback.presentation.shader.schema_invalid",
                             "Shader parameter set or binding is outside the v1 range", assetId,
                             type, reader.offset() - 28));
        }
        if (!std::isfinite(entry.defaultNumeric[0]) || !std::isfinite(entry.defaultNumeric[1]) ||
            !std::isfinite(entry.defaultNumeric[2]) || !std::isfinite(entry.defaultNumeric[3]) ||
            !unusedNumericLanesZero(entry.type, entry.defaultNumeric) ||
            (entry.type != ShaderParameterType::Int && entry.defaultInt != 0) || defaultBool > 1 ||
            (entry.type != ShaderParameterType::Bool && defaultBool != 0) ||
            (entry.type == ShaderParameterType::Texture2D && entry.defaultInt != 0)) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader parameter default values are invalid",
                                                 assetId, type, reader.offset() - 20));
        }
        entry.defaultBool = defaultBool != 0;
        parsed.value.parameters.push_back(std::move(entry));
    }

    parsed.value.bindings.reserve(bindingCount);
    std::set<std::string, std::less<>> uniqueBindingNames;
    std::set<std::pair<std::uint32_t, std::uint32_t>> uniqueSlots;
    for (std::uint32_t index = 0; index < bindingCount; ++index) {
        if (reader.remaining() < 16) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader binding list is truncated",
                                                 assetId, type, reader.offset()));
        }
        ShaderBinding binding;
        binding.set = reader.readU32();
        binding.binding = reader.readU32();
        binding.type = static_cast<ShaderParameterType>(reader.readU32());
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        binding.name = bytesToString(*encoded);
        if (!isUserIdentifier(binding.name) || !uniqueBindingNames.insert(binding.name).second) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader binding name is invalid or duplicated",
                                                 assetId, type, reader.offset() - nameBytes));
        }
        if (static_cast<std::uint32_t>(binding.type) < 1 ||
            static_cast<std::uint32_t>(binding.type) > 7) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader binding type is unsupported", assetId,
                                                 type, reader.offset() - nameBytes - 12));
        }
        if (binding.set == 0 && binding.binding == 0) {
            return core::unexpected(
                payloadError("playback.presentation.shader.reserved_binding",
                             "User shader bindings must not occupy set 0 binding 0", assetId, type,
                             reader.offset() - nameBytes - 12));
        }
        if (binding.set != 0 || binding.binding < 1 || binding.binding > maxShaderBindings ||
            !uniqueSlots.insert({binding.set, binding.binding}).second) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader binding slot is invalid or duplicated",
                                                 assetId, type, reader.offset() - nameBytes - 12));
        }
        parsed.value.bindings.push_back(std::move(binding));
    }

    parsed.value.requiredHostExtensions.reserve(hostExtensionCount);
    std::set<std::string, std::less<>> uniqueExtensions;
    for (std::uint32_t index = 0; index < hostExtensionCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader host extension list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        auto name = bytesToString(*encoded);
        if (!isUserIdentifier(name) || !uniqueExtensions.insert(name).second) {
            return core::unexpected(
                payloadError("playback.presentation.shader.schema_invalid",
                             "Shader host extension ID is invalid or duplicated", assetId, type,
                             reader.offset() - nameBytes));
        }
        parsed.value.requiredHostExtensions.push_back(std::move(name));
    }
    if (parsed.value.requiredRendererProfile != builtinRendererProfile) {
        const auto found = std::find(parsed.value.requiredHostExtensions.begin(),
                                     parsed.value.requiredHostExtensions.end(),
                                     parsed.value.requiredRendererProfile);
        if (found == parsed.value.requiredHostExtensions.end()) {
            return core::unexpected(payloadError(
                "playback.presentation.shader.profile_unsupported",
                "Host-extension renderer profile must be listed in requiredHostExtensions", assetId,
                type, 60));
        }
    }

    auto vertexSourceBytes = readCountedBytes(reader, vertexSourceByteCount, assetId, type);
    if (!vertexSourceBytes) {
        return core::unexpected(std::move(vertexSourceBytes.error()));
    }
    auto fragmentSourceBytes = readCountedBytes(reader, fragmentSourceByteCount, assetId, type);
    if (!fragmentSourceBytes) {
        return core::unexpected(std::move(fragmentSourceBytes.error()));
    }
    parsed.value.vertexSource = bytesToString(*vertexSourceBytes);
    parsed.value.fragmentSource = bytesToString(*fragmentSourceBytes);
    if (auto validated =
            validateShaderSource(parsed.value.vertexSource, assetId, type,
                                 reader.offset() - vertexSourceByteCount - fragmentSourceByteCount);
        !validated) {
        return core::unexpected(std::move(validated.error()));
    }
    if (auto validated = validateShaderSource(parsed.value.fragmentSource, assetId, type,
                                              reader.offset() - fragmentSourceByteCount);
        !validated) {
        return core::unexpected(std::move(validated.error()));
    }
    if (reader.remaining() != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload contains trailing bytes", assetId,
                                             type, reader.offset()));
    }

    for (const auto& parameter : parsed.value.parameters) {
        const auto binding = std::find_if(
            parsed.value.bindings.begin(), parsed.value.bindings.end(),
            [&](const ShaderBinding& candidate) {
                return candidate.set == parameter.set && candidate.binding == parameter.binding;
            });
        if (binding == parsed.value.bindings.end() || binding->type != parameter.type) {
            return core::unexpected(payloadError(
                "playback.presentation.shader.schema_invalid",
                "Shader parameter set/binding must match the binding table", assetId, type, 40));
        }
    }

    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = 48;
    const auto addString = [&](std::string_view value) -> bool {
        const auto extra = 4ULL + static_cast<std::uint64_t>(value.size());
        return checkedAddU64(parsed.decodedByteCount, extra, parsed.decodedByteCount);
    };
    if (!addString(parsed.value.vertexEntry) || !addString(parsed.value.fragmentEntry) ||
        !addString(parsed.value.requiredRendererProfile)) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    for (const auto& keyword : parsed.value.variantKeywords) {
        if (!addString(keyword)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& parameter : parsed.value.parameters) {
        if (!addString(parameter.name)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& binding : parsed.value.bindings) {
        if (!addString(binding.name)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& extension : parsed.value.requiredHostExtensions) {
        if (!addString(extension)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    if (!checkedAddU64(parsed.decodedByteCount, parsed.value.vertexSource.size(),
                       parsed.decodedByteCount) ||
        !checkedAddU64(parsed.decodedByteCount, parsed.value.fragmentSource.size(),
                       parsed.decodedByteCount) ||
        parsed.decodedByteCount > maxResourceBytes) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    return parsed;
}

[[nodiscard]] auto parseParameterizedMaterial(std::string_view assetId,
                                              std::span<const std::byte> bytes)
    -> core::Result<ParsedParameterizedMaterial> {
    const auto type = PresentationResourceType::ParameterizedMaterial;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 48) {
        return core::unexpected(
            payloadError("playback.presentation.payload.truncated",
                         "Portable Parameterized Material fixed body is truncated", assetId, type,
                         bytes.size()));
    }
    auto reader = *readerResult;
    const auto alphaMode = reader.readU32();
    const auto doubleSided = reader.readU32();
    const auto keywordCount = reader.readU32();
    const auto parameterCount = reader.readU32();
    const auto shaderAssetIdByteCount = reader.readU32();
    const auto reserved = reader.readU32();
    if ((alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
         alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Blend)) ||
        doubleSided > 1) {
        return core::unexpected(payloadError(
            "playback.presentation.material.value_invalid",
            "Portable Parameterized Material alpha mode or double-sided value is invalid", assetId,
            type, alphaMode > 2 ? 24 : 28));
    }
    if (keywordCount > maxShaderKeywords) {
        return core::unexpected(payloadError(
            "playback.presentation.material.keyword_undeclared",
            "Parameterized Material keyword count exceeds the v1 limit", assetId, type, 32));
    }
    if (parameterCount > maxShaderParameters) {
        return core::unexpected(payloadError(
            "playback.presentation.material.parameter_mismatch",
            "Parameterized Material parameter count exceeds the v1 limit", assetId, type, 36));
    }
    if (shaderAssetIdByteCount < 1 || shaderAssetIdByteCount > 256) {
        return core::unexpected(
            payloadError("playback.presentation.material.shader_reference_invalid",
                         "Parameterized Material shader AssetId length is outside the v1 range",
                         assetId, type, 40));
    }
    if (reserved != 0) {
        return core::unexpected(payloadError(
            "playback.presentation.payload.reserved_nonzero",
            "Portable Parameterized Material reserved field is non-zero", assetId, type, 44));
    }

    auto shaderIdBytes = readCountedBytes(reader, shaderAssetIdByteCount, assetId, type);
    if (!shaderIdBytes) {
        return core::unexpected(std::move(shaderIdBytes.error()));
    }
    auto shaderAssetId = bytesToString(*shaderIdBytes);
    if (!isPortableAssetId(shaderAssetId)) {
        return core::unexpected(payloadError(
            "playback.presentation.material.shader_reference_invalid",
            "Parameterized Material shader AssetId is not portable", assetId, type, 48));
    }

    ParsedParameterizedMaterial parsed;
    parsed.value.alphaMode = static_cast<PresentationAlphaMode>(alphaMode);
    parsed.value.doubleSided = doubleSided != 0;
    parsed.shaderAssetId = std::move(shaderAssetId);
    parsed.parameterCount = parameterCount;
    parsed.selectedKeywords.reserve(keywordCount);
    std::set<std::string, std::less<>> uniqueKeywords;
    for (std::uint32_t index = 0; index < keywordCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Parameterized Material keyword list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        auto name = bytesToString(*encoded);
        if (!isUserIdentifier(name) || !uniqueKeywords.insert(name).second) {
            return core::unexpected(
                payloadError("playback.presentation.material.keyword_undeclared",
                             "Parameterized Material keyword is invalid or duplicated", assetId,
                             type, reader.offset() - nameBytes));
        }
        parsed.selectedKeywords.push_back(std::move(name));
    }
    parsed.parameterBytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(reader.offset()),
                                 bytes.end());
    parsed.encodedByteCount = bytes.size();
    return parsed;
}

} // namespace cuexis::playback::detail
