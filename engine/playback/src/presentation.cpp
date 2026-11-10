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

auto preparePresentation(const chart::ChartRuntime& chartRuntime,
                         assets::ResourceManager* resourceManager, bool rejectLegacy)
    -> core::Result<std::optional<PreparedPresentation>> {
    try {
        auto requiredResult = collectRequiredResources(chartRuntime);
        if (!requiredResult) {
            return core::unexpected(std::move(requiredResult.error()));
        }
        auto& required = *requiredResult;
        if (required.size() > maxManifestEntries) {
            return core::unexpected(
                core::Error{"playback.presentation.session.budget_exceeded",
                            "Portable presentation manifest entry limit was exceeded"}
                    .withContext("limit", std::to_string(maxManifestEntries))
                    .withContext("actual", std::to_string(required.size())));
        }
        if (required.empty()) {
            PreparedPresentation empty;
            empty.manifest.totalDecodedBytes = 24;
            return std::optional<PreparedPresentation>{std::move(empty)};
        }
        if (resourceManager == nullptr) {
            const auto& first = *required.begin();
            return core::unexpected(
                resourceError("playback.presentation.resource.missing",
                              "Renderable chart has no AssetDatabase or ContentProvider",
                              first.first, first.second));
        }

        std::map<std::string, assets::MeshLease, std::less<>> meshLeases;
        std::map<std::string, assets::MaterialLease, std::less<>> materialLeases;
        bool portableCandidate = false;
        const auto objectIdFor = [&](std::string_view assetId,
                                     PresentationResourceType type) -> std::string_view {
            for (const auto& object : chartRuntime.objects) {
                if (!object.components.renderable) {
                    continue;
                }
                const auto& renderable = *object.components.renderable;
                const auto& referencedAsset =
                    type == PresentationResourceType::Mesh ? renderable.mesh : renderable.material;
                if (referencedAsset.value == assetId) {
                    return object.id.value;
                }
            }
            return {};
        };
        for (const auto& [assetId, type] : required) {
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || record->type != indexedType(type)) {
                auto error = resourceError(
                    "playback.presentation.resource.missing",
                    "Required presentation AssetId has a missing or incompatible Asset Index entry",
                    assetId, type);
                if (record != nullptr) {
                    error.withContext("actual", std::string{assets::assetTypeName(record->type)});
                }
                return core::unexpected(std::move(error));
            }
            if (type == PresentationResourceType::Mesh) {
                auto loaded = resourceManager->loadMesh(assets::AssetId{assetId});
                if (!loaded) {
                    return core::unexpected(
                        unavailableResource(assetId, type, std::move(loaded.error())));
                }
                if (rejectLegacy && !looksPortable(loaded->resource().bytes())) {
                    auto error = resourceError(
                        "playback.chart.v4.requires_portable_presentation",
                        "Chart v4 renderable requires a portable CXPRES01 presentation payload",
                        assetId, type);
                    error.withContext("field_path", std::string{"$/resources/"} + assetId);
                    if (const auto objectId = objectIdFor(assetId, type); !objectId.empty()) {
                        error.withContext("object_id", std::string{objectId});
                    }
                    return core::unexpected(std::move(error));
                }
                portableCandidate = portableCandidate || looksPortable(loaded->resource().bytes());
                meshLeases.emplace(assetId, std::move(*loaded));
            } else {
                auto loaded = resourceManager->loadMaterial(assets::AssetId{assetId});
                if (!loaded) {
                    return core::unexpected(
                        unavailableResource(assetId, type, std::move(loaded.error())));
                }
                if (rejectLegacy && !looksPortable(loaded->resource().bytes())) {
                    auto error = resourceError(
                        "playback.chart.v4.requires_portable_presentation",
                        "Chart v4 renderable requires a portable CXPRES01 presentation payload",
                        assetId, type);
                    error.withContext("field_path", std::string{"$/resources/"} + assetId);
                    if (const auto objectId = objectIdFor(assetId, type); !objectId.empty()) {
                        error.withContext("object_id", std::string{objectId});
                    }
                    return core::unexpected(std::move(error));
                }
                portableCandidate = portableCandidate || looksPortable(loaded->resource().bytes());
                materialLeases.emplace(assetId, std::move(*loaded));
            }
        }
        if (!portableCandidate) {
            return std::optional<PreparedPresentation>{};
        }

        std::map<std::string, ParsedMesh, std::less<>> parsedMeshes;
        std::map<std::string, ParsedMaterial, std::less<>> parsedMaterials;
        std::map<std::string, ParsedParameterizedMaterial, std::less<>> parsedParameterized;
        std::map<std::string, PresentationResourceType, std::less<>> requiredTextures;
        std::map<std::string, PresentationResourceType, std::less<>> requiredShaders;
        for (auto& [assetId, lease] : meshLeases) {
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || !record->dependencies.empty()) {
                return core::unexpected(resourceError("playback.presentation.dependency.mismatch",
                                                      "Portable Mesh dependency list must be empty",
                                                      assetId, PresentationResourceType::Mesh));
            }
            auto parsed = parseMesh(assetId, lease.resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            parsedMeshes.emplace(assetId, std::move(*parsed));
        }
        for (auto& [assetId, lease] : materialLeases) {
            const auto kind = peekPayloadKind(lease.resource().bytes());
            if (!kind || (*kind != payloadKind(PresentationResourceType::UnlitMaterial) &&
                          *kind != payloadKind(PresentationResourceType::ParameterizedMaterial))) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.type_mismatch",
                                 "Portable payload kind does not match the Asset Index", assetId,
                                 PresentationResourceType::UnlitMaterial, 8));
            }
            if (*kind == payloadKind(PresentationResourceType::UnlitMaterial)) {
                auto parsed = parseMaterial(assetId, lease.resource().bytes());
                if (!parsed) {
                    return core::unexpected(std::move(parsed.error()));
                }
                const auto* record = resourceManager->database().find(assetId);
                const bool matchesNoTexture =
                    !parsed->textureAssetId && record != nullptr && record->dependencies.empty();
                const bool matchesTexture =
                    parsed->textureAssetId && record != nullptr &&
                    record->dependencies.size() == 1 &&
                    record->dependencies.front().value == *parsed->textureAssetId;
                if (!matchesNoTexture && !matchesTexture) {
                    return core::unexpected(resourceError(
                        "playback.presentation.dependency.mismatch",
                        "Portable Material payload and Asset Index dependencies differ", assetId,
                        PresentationResourceType::UnlitMaterial));
                }
                if (parsed->textureAssetId) {
                    const auto* textureRecord =
                        resourceManager->database().find(*parsed->textureAssetId);
                    if (textureRecord == nullptr ||
                        textureRecord->type != assets::AssetType::Texture) {
                        return core::unexpected(
                            resourceError("playback.presentation.dependency.mismatch",
                                          "Portable Material dependency is not a Texture2D AssetId",
                                          assetId, PresentationResourceType::UnlitMaterial)
                                .withContext("dependency", *parsed->textureAssetId));
                    }
                    requiredTextures.emplace(*parsed->textureAssetId,
                                             PresentationResourceType::Texture2D);
                }
                parsedMaterials.emplace(assetId, std::move(*parsed));
                continue;
            }

            auto parsed = parseParameterizedMaterial(assetId, lease.resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            required[assetId] = PresentationResourceType::ParameterizedMaterial;
            requiredShaders.emplace(parsed->shaderAssetId, PresentationResourceType::Shader);
            parsedParameterized.emplace(assetId, std::move(*parsed));
        }

        std::map<std::string, ParsedShader, std::less<>> parsedShaders;
        for (const auto& [assetId, type] : requiredShaders) {
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || record->type != assets::AssetType::Shader) {
                auto error = resourceError(
                    "playback.presentation.material.shader_reference_invalid",
                    "Parameterized Material shader AssetId is missing or not a Shader", assetId,
                    type);
                if (record != nullptr) {
                    error.withContext("actual", std::string{assets::assetTypeName(record->type)});
                }
                return core::unexpected(std::move(error));
            }
            if (!record->dependencies.empty()) {
                return core::unexpected(
                    resourceError("playback.presentation.dependency.mismatch",
                                  "Portable Shader dependency list must be empty", assetId, type));
            }
            auto loaded = resourceManager->loadShader(assets::AssetId{assetId});
            if (!loaded) {
                return core::unexpected(
                    unavailableResource(assetId, type, std::move(loaded.error())));
            }
            auto parsed = parseShader(assetId, loaded->resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            parsedShaders.emplace(assetId, std::move(*parsed));
        }

        for (auto& [assetId, parsed] : parsedParameterized) {
            const auto shader = parsedShaders.find(parsed.shaderAssetId);
            if (shader == parsedShaders.end()) {
                return core::unexpected(
                    resourceError("playback.presentation.material.shader_reference_invalid",
                                  "Parameterized Material shader was not prepared", assetId,
                                  PresentationResourceType::ParameterizedMaterial)
                        .withContext("dependency", parsed.shaderAssetId));
            }
            if (auto completed =
                    completeParameterizedMaterial(assetId, parsed, shader->second.value);
                !completed) {
                return core::unexpected(std::move(completed.error()));
            }
            const auto* record = resourceManager->database().find(assetId);
            std::vector<std::string> expected{parsed.shaderAssetId};
            expected.insert(expected.end(), parsed.textureAssetIds.begin(),
                            parsed.textureAssetIds.end());
            std::sort(expected.begin(), expected.end());
            expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
            std::vector<std::string> actual;
            if (record != nullptr) {
                actual.reserve(record->dependencies.size());
                for (const auto& dependency : record->dependencies) {
                    actual.push_back(dependency.value);
                }
            }
            if (record == nullptr || actual != expected) {
                return core::unexpected(
                    resourceError("playback.presentation.dependency.mismatch",
                                  "Portable Material payload and Asset Index dependencies differ",
                                  assetId, PresentationResourceType::ParameterizedMaterial));
            }
            for (const auto& textureAssetId : parsed.textureAssetIds) {
                const auto* textureRecord = resourceManager->database().find(textureAssetId);
                if (textureRecord == nullptr || textureRecord->type != assets::AssetType::Texture) {
                    return core::unexpected(
                        resourceError("playback.presentation.dependency.mismatch",
                                      "Portable Material dependency is not a Texture2D AssetId",
                                      assetId, PresentationResourceType::ParameterizedMaterial)
                            .withContext("dependency", textureAssetId));
                }
                requiredTextures.emplace(textureAssetId, PresentationResourceType::Texture2D);
            }
        }

        const auto totalEntries =
            required.size() + requiredTextures.size() + requiredShaders.size();
        if (totalEntries > maxManifestEntries) {
            return core::unexpected(
                core::Error{"playback.presentation.session.budget_exceeded",
                            "Portable presentation manifest entry limit was exceeded"}
                    .withContext("limit", std::to_string(maxManifestEntries))
                    .withContext("actual", std::to_string(totalEntries)));
        }

        std::map<std::string, ParsedTexture, std::less<>> parsedTextures;
        for (const auto& [assetId, type] : requiredTextures) {
            auto loaded = resourceManager->loadTexture(assets::AssetId{assetId});
            if (!loaded) {
                return core::unexpected(
                    unavailableResource(assetId, type, std::move(loaded.error())));
            }
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || !record->dependencies.empty()) {
                return core::unexpected(resourceError(
                    "playback.presentation.dependency.mismatch",
                    "Portable Texture2D dependency list must be empty", assetId, type));
            }
            auto parsed = parseTexture(assetId, loaded->resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            parsedTextures.emplace(assetId, std::move(*parsed));
        }

        std::map<PresentationResourceKey, BuiltResource, PresentationResourceKeyLess> built;
        std::map<std::array<std::uint8_t, 32>, PortableResourcePtr> identities;
        const auto addResource =
            [&](PortableResourcePtr resource, std::uint64_t encodedByteCount,
                std::uint64_t decodedByteCount,
                std::vector<PresentationResourceRef> dependencies) -> core::Result<void> {
            const auto identity = resource->reference.identity.sha256;
            const auto collision = identities.find(identity);
            if (collision != identities.end() &&
                !resourceValuesEqual(collision->second->value, resource->value)) {
                return core::unexpected(resourceError(
                    "playback.presentation.identity_collision",
                    "Different portable resource values produced the same semantic identity",
                    resource->reference.assetId, resource->reference.type));
            }
            identities.try_emplace(identity, resource);
            const PresentationResourceKey key{resource->reference.assetId,
                                              resource->reference.type};
            const auto [unused, inserted] =
                built.emplace(key, BuiltResource{std::move(resource), encodedByteCount,
                                                 decodedByteCount, std::move(dependencies)});
            if (!inserted) {
                return core::unexpected(
                    resourceError("playback.presentation.reference.invalid",
                                  "Portable presentation contains a duplicate AssetId and type",
                                  key.assetId, key.type));
            }
            return {};
        };

        for (auto& [assetId, parsed] : parsedTextures) {
            PresentationResourceRef reference{PresentationResourceType::Texture2D, assetId,
                                              textureIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, {});
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedMeshes) {
            PresentationResourceRef reference{PresentationResourceType::Mesh, assetId,
                                              meshIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, {});
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedShaders) {
            PresentationResourceRef reference{PresentationResourceType::Shader, assetId,
                                              shaderIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, {});
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedMaterials) {
            std::vector<PresentationResourceRef> dependencies;
            if (parsed.textureAssetId) {
                const auto found = built.find(PresentationResourceKeyView{
                    *parsed.textureAssetId, PresentationResourceType::Texture2D});
                if (found == built.end()) {
                    return core::unexpected(
                        resourceError("playback.presentation.resource.missing",
                                      "Portable Material texture dependency was not prepared",
                                      assetId, PresentationResourceType::UnlitMaterial));
                }
                parsed.value.baseColorTexture = found->second.resource->reference;
                dependencies.push_back(found->second.resource->reference);
            }
            PresentationResourceRef reference{PresentationResourceType::UnlitMaterial, assetId,
                                              materialIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, std::move(dependencies));
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedParameterized) {
            std::vector<PresentationResourceRef> dependencies;
            const auto shader = built.find(PresentationResourceKeyView{
                parsed.shaderAssetId, PresentationResourceType::Shader});
            if (shader == built.end()) {
                return core::unexpected(
                    resourceError("playback.presentation.material.shader_reference_invalid",
                                  "Parameterized Material shader dependency was not prepared",
                                  assetId, PresentationResourceType::ParameterizedMaterial));
            }
            parsed.value.shader = shader->second.resource->reference;
            dependencies.push_back(shader->second.resource->reference);
            for (auto& parameter : parsed.value.parameters) {
                if (!parameter.texture) {
                    continue;
                }
                const auto found = built.find(PresentationResourceKeyView{
                    parameter.texture->assetId, PresentationResourceType::Texture2D});
                if (found == built.end()) {
                    return core::unexpected(
                        resourceError("playback.presentation.resource.missing",
                                      "Portable Material texture dependency was not prepared",
                                      assetId, PresentationResourceType::ParameterizedMaterial));
                }
                parameter.texture = found->second.resource->reference;
                dependencies.push_back(found->second.resource->reference);
            }
            PresentationResourceRef reference{PresentationResourceType::ParameterizedMaterial,
                                              assetId, parameterizedMaterialIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, std::move(dependencies));
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }

        PreparedPresentation presentation;
        presentation.manifest.entries.reserve(built.size());
        presentation.orderedResources.reserve(built.size());
        std::uint64_t totalEncodedBytes = 0;
        std::uint64_t totalResourceDecodedBytes = 0;
        std::uint64_t manifestBytes = 24;
        for (const auto& [key, resource] : built) {
            if (resource.encodedByteCount > maxSessionBytes - totalEncodedBytes ||
                resource.decodedByteCount > maxSessionBytes - totalResourceDecodedBytes) {
                return core::unexpected(
                    core::Error{"playback.presentation.session.budget_exceeded",
                                "Portable presentation resource byte total was exceeded"}
                        .withContext("limit", std::to_string(maxSessionBytes)));
            }
            totalEncodedBytes += resource.encodedByteCount;
            totalResourceDecodedBytes += resource.decodedByteCount;
            auto entryBytes = 20ULL + presentationRefByteCount(resource.resource->reference);
            for (const auto& dependency : resource.dependencies) {
                entryBytes += presentationRefByteCount(dependency);
            }
            if (entryBytes > maxSessionBytes - manifestBytes) {
                return core::unexpected(
                    core::Error{"playback.presentation.session.budget_exceeded",
                                "Portable presentation manifest byte total was exceeded"}
                        .withContext("limit", std::to_string(maxSessionBytes)));
            }
            manifestBytes += entryBytes;
            presentation.manifest.entries.push_back(
                PresentationManifestEntry{resource.resource->reference, resource.encodedByteCount,
                                          resource.decodedByteCount, resource.dependencies});
            presentation.resources.emplace(key, resource.resource);
            presentation.orderedResources.push_back(resource.resource);
        }
        if (totalEncodedBytes > maxSessionBytes ||
            totalResourceDecodedBytes > maxSessionBytes - manifestBytes) {
            const auto actual = totalResourceDecodedBytes + manifestBytes;
            return core::unexpected(
                core::Error{"playback.presentation.session.budget_exceeded",
                            "Portable presentation session byte budget was exceeded"}
                    .withContext("limit", std::to_string(maxSessionBytes))
                    .withContext("actual", std::to_string(actual)));
        }
        presentation.manifest.totalEncodedBytes = totalEncodedBytes;
        presentation.manifest.totalDecodedBytes = totalResourceDecodedBytes + manifestBytes;
        return std::optional<PreparedPresentation>{std::move(presentation)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.budget_exceeded",
                        "Portable presentation allocation could not be satisfied"}
                .withContext("limit", std::to_string(maxSessionBytes))
                .withContext("actual", "allocation_failed"));
    } catch (const std::exception& exception) {
        return core::unexpected(core::Error{"playback.presentation.prepare_failed",
                                            "Portable presentation preparation failed"}
                                    .withContext("exception", exception.what()));
    } catch (...) {
        return core::unexpected(core::Error{"playback.presentation.prepare_failed",
                                            "Portable presentation preparation failed"});
    }
}

auto findPresentationResource(const PreparedPresentation& presentation,
                              const PresentationResourceRef& reference) noexcept
    -> const PortableResourcePtr* {
    const auto found =
        presentation.resources.find(PresentationResourceKeyView{reference.assetId, reference.type});
    if (found == presentation.resources.end() || found->second->reference != reference) {
        return nullptr;
    }
    return &found->second;
}

auto findPresentationResource(const PreparedPresentation& presentation, std::string_view assetId,
                              PresentationResourceType type) noexcept
    -> const PortableResourcePtr* {
    const auto found = presentation.resources.find(PresentationResourceKeyView{assetId, type});
    if (found == presentation.resources.end()) {
        return nullptr;
    }
    return &found->second;
}

} // namespace cuexis::playback::detail
