#include "presentation_materializer.hpp"
#include <algorithm>

namespace cuexis::playback::detail {
[[nodiscard]] auto presentationResourceTypeName(PresentationResourceType type) noexcept
    -> std::string_view {
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
namespace {
void copyMatrix(const core::Mat4& source, float (&destination)[16]) noexcept {
    std::copy(source.values.begin(), source.values.end(), destination);
}

} // namespace
[[nodiscard]] auto snapshotResourceMismatch(std::string_view objectId, std::string_view assetId,
                                            PresentationResourceType type) -> core::Error {
    return core::Error{"playback.presentation.frame.resource_mismatch",
                       "Snapshot presentation resource could not be resolved"}
        .withContext("object_id", std::string{objectId})
        .withContext("asset_id", std::string{assetId})
        .withContext("resource_type", std::string{presentationResourceTypeName(type)});
}

void assignPresentationReference(std::optional<PresentationResourceRef>& destination,
                                 const PresentationResourceRef& source,
                                 std::size_t assetIdCapacity) {
    if (!destination) {
        destination.emplace();
    }
    if (destination->assetId.capacity() < assetIdCapacity) {
        destination->assetId.reserve(assetIdCapacity);
    }
    destination->type = source.type;
    if (destination->assetId != source.assetId) {
        destination->assetId = source.assetId;
    }
    destination->identity = source.identity;
}

namespace {
auto fillObjects(const runtime::detail::PresentationRead& view, const SnapshotLayout& layout,
                 FrameSnapshot& snapshot) -> core::Result<void> {
    if (snapshot.objects.size() < layout.entities.size()) {
        snapshot.objects.resize(layout.entities.size());
    }
    const core::Mat4 identity;
    runtime::detail::ObjectVisualRead read;
    for (std::size_t index = 0; index < layout.entities.size(); ++index) {
        const auto& entry = layout.entities[index];
        auto row = view.readSnapshotObject(entry.runtimeIndex, read);
        if (!row)
            return core::unexpected(std::move(row.error()));
        // The layout stamp ties this layout to the runtime layout once per frame; the per-object
        // comparison only guards against a mapping change, so it is exact for the first object and
        // a length comparison afterwards. A full string compare here cost 10% of the read block.
        const bool mappingChanged =
            index == 0 ? read.id != entry.id.value : read.id.size() != entry.id.value.size();
        if (mappingChanged)
            return core::unexpected(core::Error{"runtime.presentation.layout_stale",
                                                "Presentation object mapping changed"});
        auto& object = snapshot.objects[index];
        if (object.materialAssetId.capacity() < entry.maximumMaterialAssetIdSize) {
            object.materialAssetId.reserve(entry.maximumMaterialAssetIdSize);
        }
        if (entry.mesh) {
            assignPresentationReference(object.mesh, *entry.mesh, entry.maximumMeshAssetIdSize);
        } else {
            object.mesh.reset();
        }
        if (object.id != entry.id.value) {
            object.id = entry.id.value;
        }
        object.hasTransform = read.matrix != nullptr;
        if (read.matrix != nullptr) {
            copyMatrix(*read.matrix, object.worldMatrix);
        } else {
            copyMatrix(identity, object.worldMatrix);
        }
        if (read.hasAppearance) {
            const auto& appearance = read;
            object.visible = appearance.visible;
            if (appearance.material.size() >
                std::max<std::size_t>(entry.maximumMaterialAssetIdSize, 256))
                return core::unexpected(core::Error{"playback.presentation.frame.resource_mismatch",
                                                    "Material exceeds prepared admission bound"});
            if (object.materialAssetId != appearance.material)
                object.materialAssetId.assign(appearance.material);
            if (entry.mesh) {
                const auto only = entry.materials.begin();
                const auto material =
                    entry.materials.size() == 1 && only->first == appearance.material
                        ? only
                        : entry.materials.find(appearance.material);
                if (material == entry.materials.end()) {
                    return core::unexpected(
                        snapshotResourceMismatch(entry.id.value, appearance.material,
                                                 PresentationResourceType::UnlitMaterial));
                }
                assignPresentationReference(object.material, material->second,
                                            entry.maximumMaterialAssetIdSize);
            } else {
                object.material.reset();
            }
            object.materialOpacity = appearance.opacity;
            object.materialTint[0] = appearance.tint.x;
            object.materialTint[1] = appearance.tint.y;
            object.materialTint[2] = appearance.tint.z;
        } else {
            if (entry.mesh) {
                return core::unexpected(snapshotResourceMismatch(
                    entry.id.value, "", PresentationResourceType::UnlitMaterial));
            }
            object.visible = true;
            object.materialAssetId.clear();
            object.material.reset();
            object.materialOpacity = 1.0;
            object.materialTint[0] = 1.0F;
            object.materialTint[1] = 1.0F;
            object.materialTint[2] = 1.0F;
        }
    }
    snapshot.objects.resize(layout.entities.size());
    return {};
}
auto fillCamera(const runtime::detail::CameraPresentation& camera, const FrameViewport& viewport,
                FrameSnapshot::CameraSnapshot& destination) -> core::Result<void> {
    if (!camera.selected) {
        return {};
    }
    if (!camera.present) {
        return core::unexpected(
            core::Error{"playback.snapshot.camera_missing", "Active camera is unavailable"});
    }
    const auto projection = core::makePerspective(
        camera.fovY * 3.14159265358979323846 / 180.0,
        static_cast<double>(viewport.width) / viewport.height, camera.nearPlane, camera.farPlane);
    if (!projection) {
        return core::unexpected(
            core::Error{"playback.snapshot.camera_invalid", "Active camera parameters are invalid"}
                .withCause(projection.error()));
    }

    destination.active = true;
    destination.fovY = camera.fovY;
    destination.nearPlane = camera.nearPlane;
    destination.farPlane = camera.farPlane;
    destination.pitch = camera.pitch;
    destination.yaw = camera.yaw;
    destination.roll = camera.roll;
    copyMatrix(*projection, destination.projectionMatrix);

    core::Mat4 viewMatrix{};
    if (camera.hasTransform) {
        auto inverse = core::inverse(camera.matrix);
        if (!inverse) {
            return core::unexpected(core::Error{"playback.snapshot.camera_not_invertible",
                                                "Active camera transform is not invertible"}
                                        .withCause(inverse.error()));
        }
        viewMatrix = *inverse;
    }
    copyMatrix(viewMatrix, destination.viewMatrix);
    return {};
}
} // namespace
auto materializeSnapshot(const runtime::detail::PresentationRead& view,
                         const SnapshotLayout& layout, const FrameViewport& viewport,
                         FrameSnapshot& snapshot) -> core::Result<void> {
    auto state = view.state();
    if (!state)
        return core::unexpected(std::move(state.error()));
    if (state->stamp.owner != layout.runtimeOwner || state->stamp.layout != layout.runtimeLayout)
        return core::unexpected(
            core::Error{"runtime.presentation.layout_stale", "Presentation layout changed"});
    snapshot.camera = {};
    snapshot.clearRed = 0.055F;
    snapshot.clearGreen = 0.063F;
    snapshot.clearBlue = 0.071F;
    snapshot.clearAlpha = 1.0F;
    snapshot.viewportWidth = viewport.width;
    snapshot.viewportHeight = viewport.height;
    auto objects = fillObjects(view, layout, snapshot);
    if (!objects)
        return objects;
    auto camera = view.readCamera();
    if (!camera)
        return core::unexpected(std::move(camera.error()));
    return fillCamera(*camera, viewport, snapshot.camera);
}
} // namespace cuexis::playback::detail
