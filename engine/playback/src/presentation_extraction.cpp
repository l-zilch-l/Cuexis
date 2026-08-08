#include "presentation_extraction.hpp"

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace cuexis::playback::detail {
namespace {

constexpr std::size_t maxNormalizedRecords = 100'000;
constexpr double depthQuantization = 4096.0;
constexpr double signedIntegerLimit = 0x1p63;

[[nodiscard]] auto resourceTypeName(PresentationResourceType type) noexcept -> std::string_view {
    switch (type) {
    case PresentationResourceType::Mesh:
        return "mesh";
    case PresentationResourceType::Texture2D:
        return "texture2d";
    case PresentationResourceType::UnlitMaterial:
        return "unlit_material";
    }
    return "unknown";
}

[[nodiscard]] auto resourceMismatch(std::string message, std::string_view objectId,
                                    const PresentationResourceRef* reference = nullptr)
    -> core::Error {
    auto error = core::Error{"playback.presentation.frame.resource_mismatch", std::move(message)};
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId)
            .withContext("resource_type", std::string{resourceTypeName(reference->type)});
    }
    return error;
}

[[nodiscard]] auto nonFinite(std::string_view objectId, std::string_view field) -> core::Error {
    auto error = core::Error{"playback.presentation.frame.non_finite",
                             "Presentation frame calculation contains a non-finite value"}
                     .withContext("field", std::string{field});
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] auto referenceKey(const PresentationResourceRef& reference) noexcept {
    return std::tie(reference.assetId, reference.type);
}

struct LocatedResource final {
    const PresentationManifestEntry* manifestEntry{};
    const PortableResource* resource{};
};

[[nodiscard]] auto locateResource(const PresentationResourceManifest& manifest,
                                  std::span<const PortableResourcePtr> resources,
                                  const PresentationResourceRef& reference) noexcept
    -> std::optional<LocatedResource> {
    const auto found = std::lower_bound(
        manifest.entries.begin(), manifest.entries.end(), reference,
        [](const PresentationManifestEntry& entry, const PresentationResourceRef& candidate) {
            return referenceKey(entry.reference) < referenceKey(candidate);
        });
    if (found == manifest.entries.end() || found->reference != reference) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(found - manifest.entries.begin());
    if (index >= resources.size() || resources[index] == nullptr ||
        resources[index]->reference != reference) {
        return std::nullopt;
    }
    return LocatedResource{&*found, resources[index].get()};
}

[[nodiscard]] auto resourceValueMatchesType(const PortableResource& resource) noexcept -> bool {
    switch (resource.reference.type) {
    case PresentationResourceType::Mesh:
        return std::holds_alternative<PortableMesh>(resource.value);
    case PresentationResourceType::Texture2D:
        return std::holds_alternative<PortableTexture2D>(resource.value);
    case PresentationResourceType::UnlitMaterial:
        return std::holds_alternative<PortableUnlitMaterial>(resource.value);
    }
    return false;
}

[[nodiscard]] auto finiteMatrix(const float (&matrix)[16]) noexcept -> bool {
    return std::all_of(std::begin(matrix), std::end(matrix),
                       [](float value) { return std::isfinite(value); });
}

struct Point3 final {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] auto transformPoint(const float (&matrix)[16], const Point3& point) noexcept
    -> Point3 {
    return Point3{
        static_cast<double>(matrix[0]) * point.x + static_cast<double>(matrix[4]) * point.y +
            static_cast<double>(matrix[8]) * point.z + static_cast<double>(matrix[12]),
        static_cast<double>(matrix[1]) * point.x + static_cast<double>(matrix[5]) * point.y +
            static_cast<double>(matrix[9]) * point.z + static_cast<double>(matrix[13]),
        static_cast<double>(matrix[2]) * point.x + static_cast<double>(matrix[6]) * point.y +
            static_cast<double>(matrix[10]) * point.z + static_cast<double>(matrix[14])};
}

[[nodiscard]] auto finitePoint(const Point3& point) noexcept -> bool {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

[[nodiscard]] auto validateResourceTable(const PresentationResourceManifest& manifest,
                                         std::span<const PortableResourcePtr> resources)
    -> core::Result<void> {
    if (manifest.entries.size() != resources.size()) {
        return core::unexpected(
            resourceMismatch("Presentation manifest and resource table sizes differ", {}));
    }
    for (std::size_t index = 0; index < manifest.entries.size(); ++index) {
        const auto& entry = manifest.entries[index];
        if (index != 0 && !(referenceKey(manifest.entries[index - 1].reference) <
                            referenceKey(entry.reference))) {
            return core::unexpected(resourceMismatch(
                "Presentation manifest entries are not in canonical order", {}, &entry.reference));
        }
        if (resources[index] == nullptr || resources[index]->reference != entry.reference ||
            !resourceValueMatchesType(*resources[index])) {
            return core::unexpected(resourceMismatch(
                "Presentation resource table does not match its manifest", {}, &entry.reference));
        }
    }
    return {};
}

} // namespace

auto normalizePresentationFrame(const FrameSnapshot& snapshot,
                                const PresentationResourceManifest& manifest,
                                std::span<const PortableResourcePtr> resources,
                                NormalizedPresentationFrame& destination) -> core::Result<void> {
    destination.opaque.clear();
    destination.transparent.clear();
    const auto fail = [&](core::Error error) -> core::Result<void> {
        destination.opaque.clear();
        destination.transparent.clear();
        return core::unexpected(std::move(error));
    };

    try {
        if (snapshot.objects.size() > maxNormalizedRecords) {
            return fail(core::Error{"playback.presentation.frame.command_budget_exceeded",
                                    "Normalized presentation record limit was exceeded"}
                            .withContext("limit", std::to_string(maxNormalizedRecords))
                            .withContext("actual", std::to_string(snapshot.objects.size())));
        }
        if (auto validated = validateResourceTable(manifest, resources); !validated) {
            return fail(std::move(validated.error()));
        }
        if (destination.opaque.capacity() < snapshot.objects.size()) {
            destination.opaque.reserve(snapshot.objects.size());
        }
        if (destination.transparent.capacity() < snapshot.objects.size()) {
            destination.transparent.reserve(snapshot.objects.size());
        }

        bool cameraValidated = false;
        for (std::size_t objectIndex = 0; objectIndex < snapshot.objects.size(); ++objectIndex) {
            const auto& object = snapshot.objects[objectIndex];
            if (object.mesh.has_value() != object.material.has_value()) {
                const auto* reference = object.mesh ? &*object.mesh : &*object.material;
                return fail(resourceMismatch("Renderable Mesh and Material refs must be paired",
                                             object.id, reference));
            }
            if (!object.mesh) {
                if (!object.materialAssetId.empty()) {
                    return fail(resourceMismatch("Renderable snapshot is missing portable refs",
                                                 object.id));
                }
                continue;
            }
            if (object.mesh->type != PresentationResourceType::Mesh ||
                object.material->type != PresentationResourceType::UnlitMaterial ||
                object.materialAssetId != object.material->assetId) {
                return fail(
                    resourceMismatch("Snapshot portable refs have incompatible types or IDs",
                                     object.id, &*object.material));
            }

            const auto meshResource = locateResource(manifest, resources, *object.mesh);
            const auto materialResource = locateResource(manifest, resources, *object.material);
            if (!meshResource || !materialResource ||
                !std::holds_alternative<PortableMesh>(meshResource->resource->value) ||
                !std::holds_alternative<PortableUnlitMaterial>(materialResource->resource->value)) {
                return fail(resourceMismatch("Snapshot ref is not backed by its declared resource",
                                             object.id,
                                             !meshResource ? &*object.mesh : &*object.material));
            }

            const auto& mesh = std::get<PortableMesh>(meshResource->resource->value);
            const auto& material =
                std::get<PortableUnlitMaterial>(materialResource->resource->value);
            if (material.baseColorTexture) {
                const auto textureResource =
                    locateResource(manifest, resources, *material.baseColorTexture);
                if (material.baseColorTexture->type != PresentationResourceType::Texture2D ||
                    !textureResource ||
                    !std::holds_alternative<PortableTexture2D>(textureResource->resource->value) ||
                    materialResource->manifestEntry->dependencies.size() != 1 ||
                    materialResource->manifestEntry->dependencies.front() !=
                        *material.baseColorTexture) {
                    return fail(
                        resourceMismatch("Portable Material texture dependency is inconsistent",
                                         object.id, &*material.baseColorTexture));
                }
            } else if (!materialResource->manifestEntry->dependencies.empty()) {
                return fail(resourceMismatch("Portable Material has unexpected dependencies",
                                             object.id, &*object.material));
            }

            if (!object.visible) {
                continue;
            }
            if (!snapshot.camera.active) {
                return fail(core::Error{"playback.presentation.frame.camera_required",
                                        "Visible renderables require an active camera"}
                                .withContext("object_id", object.id));
            }
            if (!cameraValidated) {
                if (!finiteMatrix(snapshot.camera.viewMatrix) ||
                    !finiteMatrix(snapshot.camera.projectionMatrix)) {
                    return fail(nonFinite({}, "camera_matrix"));
                }
                cameraValidated = true;
            }
            if (!finiteMatrix(object.worldMatrix)) {
                return fail(nonFinite(object.id, "world_matrix"));
            }
            for (std::size_t component = 0; component < 3; ++component) {
                if (!std::isfinite(mesh.boundsMin[component]) ||
                    !std::isfinite(mesh.boundsMax[component])) {
                    return fail(nonFinite(object.id, "mesh_bounds"));
                }
            }
            if (!std::isfinite(object.materialOpacity)) {
                return fail(nonFinite(object.id, "material_opacity"));
            }

            NormalizedPresentationRecord record;
            record.objectIndex = objectIndex;
            for (std::size_t component = 0; component < 3; ++component) {
                if (!std::isfinite(material.baseColor[component]) ||
                    !std::isfinite(object.materialTint[component])) {
                    return fail(nonFinite(object.id, "effective_rgb"));
                }
                record.effectiveRgb[component] =
                    static_cast<double>(material.baseColor[component]) *
                    static_cast<double>(object.materialTint[component]);
                if (!std::isfinite(record.effectiveRgb[component])) {
                    return fail(nonFinite(object.id, "effective_rgb"));
                }
            }
            if (!std::isfinite(material.baseColor[3])) {
                return fail(nonFinite(object.id, "effective_alpha"));
            }
            record.effectiveAlpha =
                static_cast<double>(material.baseColor[3]) * object.materialOpacity;
            if (!std::isfinite(record.effectiveAlpha)) {
                return fail(nonFinite(object.id, "effective_alpha"));
            }

            const Point3 localCenter{
                (static_cast<double>(mesh.boundsMin[0]) + static_cast<double>(mesh.boundsMax[0])) /
                    2.0,
                (static_cast<double>(mesh.boundsMin[1]) + static_cast<double>(mesh.boundsMax[1])) /
                    2.0,
                (static_cast<double>(mesh.boundsMin[2]) + static_cast<double>(mesh.boundsMax[2])) /
                    2.0};
            if (!finitePoint(localCenter)) {
                return fail(nonFinite(object.id, "mesh_center"));
            }
            const auto worldCenter = transformPoint(object.worldMatrix, localCenter);
            const auto viewCenter = transformPoint(snapshot.camera.viewMatrix, worldCenter);
            if (!finitePoint(worldCenter) || !finitePoint(viewCenter)) {
                return fail(nonFinite(object.id, "depth_transform"));
            }
            record.depthMeters = -viewCenter.z;
            const auto scaledDepth = record.depthMeters * depthQuantization;
            const auto roundedDepth = std::round(scaledDepth);
            if (!std::isfinite(record.depthMeters) || !std::isfinite(scaledDepth) ||
                !std::isfinite(roundedDepth) || roundedDepth < -signedIntegerLimit ||
                roundedDepth >= signedIntegerLimit) {
                return fail(nonFinite(object.id, "depth"));
            }
            record.transparentDepthKey = static_cast<std::int64_t>(roundedDepth);
            record.backFaceCulling = !material.doubleSided;
            record.pass =
                material.alphaMode == PresentationAlphaMode::Blend || record.effectiveAlpha < 1.0
                    ? NormalizedPresentationPass::Transparent
                    : NormalizedPresentationPass::Opaque;
            record.depthWrite = record.pass == NormalizedPresentationPass::Opaque;
            record.sourceOverBlend = record.pass == NormalizedPresentationPass::Transparent;
            if (record.pass == NormalizedPresentationPass::Opaque) {
                destination.opaque.push_back(record);
            } else {
                destination.transparent.push_back(record);
            }
        }

        std::sort(destination.opaque.begin(), destination.opaque.end(),
                  [&](const auto& left, const auto& right) {
                      return snapshot.objects[left.objectIndex].id <
                             snapshot.objects[right.objectIndex].id;
                  });
        std::sort(destination.transparent.begin(), destination.transparent.end(),
                  [&](const auto& left, const auto& right) {
                      if (left.transparentDepthKey != right.transparentDepthKey) {
                          return left.transparentDepthKey > right.transparentDepthKey;
                      }
                      return snapshot.objects[left.objectIndex].id <
                             snapshot.objects[right.objectIndex].id;
                  });
        return {};
    } catch (const std::bad_alloc&) {
        return fail(core::Error{"playback.presentation.frame.command_budget_exceeded",
                                "Normalized presentation allocation could not be satisfied"}
                        .withContext("limit", std::to_string(maxNormalizedRecords)));
    } catch (const std::exception& exception) {
        return fail(core::Error{"playback.presentation.frame.resource_mismatch",
                                "Presentation frame extraction failed"}
                        .withContext("exception", exception.what()));
    } catch (...) {
        return fail(core::Error{"playback.presentation.frame.resource_mismatch",
                                "Presentation frame extraction failed"});
    }
}

} // namespace cuexis::playback::detail
