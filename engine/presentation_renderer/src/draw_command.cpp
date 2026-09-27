#include <cuexis/presentation_renderer/draw_command.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::presentation_renderer {
namespace {

constexpr std::size_t maxNormalizedRecords = 100'000;
constexpr double depthQuantization = 4096.0;
constexpr double signedIntegerLimit = 0x1p63;

struct OrderedDraw final {
    std::size_t objectIndex{};
    DrawCommand command;
};

struct Point3 final {
    double x{};
    double y{};
    double z{};
};

struct CachedMesh final {
    playback::PresentationResourceRef reference;
    std::array<double, 3> boundsCenter{};
};

struct CachedMaterial final {
    playback::PresentationResourceRef reference;
    playback::PortableUnlitMaterial material;
    bool parameterized{};
    playback::PortableParameterizedMaterial parameterizedMaterial;
};

struct CachedTexture final {
    playback::PresentationResourceRef reference;
};

struct CachedShader final {
    playback::PresentationResourceRef reference;
};

class SummaryHash final {
  public:
    SummaryHash() noexcept {
        static constexpr char domain[] = "cuexis.validation.summary.v1";
        writeBytes(std::as_bytes(std::span{domain, sizeof(domain)}));
    }

    void writeU8(std::uint8_t value) noexcept {
        value_ ^= value;
        value_ *= 1099511628211ULL;
    }

    void writeU32(std::uint32_t value) noexcept {
        for (std::size_t index = 0; index < 4; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeU64(std::uint64_t value) noexcept {
        for (std::size_t index = 0; index < 8; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeBool(bool value) noexcept {
        writeU8(value ? 1U : 0U);
    }

    void writeFloat(float value) noexcept {
        if (value == 0.0F) {
            value = 0.0F;
        }
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeDouble(double value) noexcept {
        if (value == 0.0) {
            value = 0.0;
        }
        writeU64(std::bit_cast<std::uint64_t>(value));
    }

    void writeBytes(std::span<const std::byte> bytes) noexcept {
        for (const auto value : bytes) {
            writeU8(std::to_integer<std::uint8_t>(value));
        }
    }

    void writeString(std::string_view value) noexcept {
        writeU32(static_cast<std::uint32_t>(value.size()));
        writeBytes(std::as_bytes(std::span{value.data(), value.size()}));
    }

    void writeReference(const playback::PresentationResourceRef& reference) noexcept {
        writeU32(static_cast<std::uint32_t>(reference.type));
        writeString(reference.assetId);
        writeBytes(std::as_bytes(std::span{reference.identity.sha256}));
    }

    [[nodiscard]] auto value() const noexcept -> std::uint64_t {
        return value_;
    }

  private:
    std::uint64_t value_{14695981039346656037ULL};
};

[[nodiscard]] auto referenceKey(const playback::PresentationResourceRef& reference) noexcept {
    return std::tie(reference.assetId, reference.type);
}

[[nodiscard]] auto resourceError(std::string code, std::string message,
                                 const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId);
    }
    return error;
}

[[nodiscard]] auto frameError(std::string message, std::string_view objectId,
                              const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error {
    auto error = resourceError("presentation.renderer.frame.resource_mismatch", std::move(message),
                               reference);
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] auto nonFiniteError(std::string_view objectId, std::string_view field)
    -> core::Error {
    auto error = core::Error{"presentation.renderer.frame.non_finite",
                             "Presentation calculation contains a non-finite value"}
                     .withContext("field", std::string{field});
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] auto finiteMatrix(const float (&matrix)[16]) noexcept -> bool {
    return std::all_of(std::begin(matrix), std::end(matrix),
                       [](float value) { return std::isfinite(value); });
}

[[nodiscard]] auto finiteMatrix(const std::array<float, 16>& matrix) noexcept -> bool {
    return std::all_of(matrix.begin(), matrix.end(),
                       [](float value) { return std::isfinite(value); });
}

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

template <typename Resource>
[[nodiscard]] auto findCached(const std::vector<Resource>& resources,
                              const playback::PresentationResourceRef& reference) noexcept
    -> const Resource* {
    const auto found = std::lower_bound(
        resources.begin(), resources.end(), reference,
        [](const Resource& resource, const playback::PresentationResourceRef& candidate) {
            return referenceKey(resource.reference) < referenceKey(candidate);
        });
    return found != resources.end() && found->reference == reference ? &*found : nullptr;
}

void hashCommand(SummaryHash& hash, const DrawCommand& command) noexcept {
    hash.writeString(command.objectId);
    for (const auto value : command.worldMatrix) {
        hash.writeFloat(value);
    }
    hash.writeReference(command.mesh);
    hash.writeReference(command.material);
    for (const auto value : command.effectiveColor) {
        hash.writeDouble(value);
    }
    hash.writeU8(static_cast<std::uint8_t>(command.pass));
    hash.writeBool(command.backFaceCulling);
    hash.writeBool(command.depthTest);
    hash.writeBool(command.depthWrite);
    hash.writeBool(command.sourceOverBlend);
    hash.writeDouble(command.depthMeters);
    hash.writeU64(std::bit_cast<std::uint64_t>(command.transparentDepthKey));
}

[[nodiscard]] auto summaryDigest(const DrawSummary& summary) noexcept -> std::uint64_t {
    SummaryHash hash;
    hash.writeU32(summary.version);
    hash.writeU32(summary.viewportWidth);
    hash.writeU32(summary.viewportHeight);
    for (const auto value : summary.clearColor) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.cameraActive);
    for (const auto value : summary.viewMatrix) {
        hash.writeFloat(value);
    }
    for (const auto value : summary.projectionMatrix) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.debugPassEnabled);
    hash.writeU32(static_cast<std::uint32_t>(summary.opaque.size()));
    for (const auto& command : summary.opaque) {
        hashCommand(hash, command);
    }
    hash.writeU32(static_cast<std::uint32_t>(summary.transparent.size()));
    for (const auto& command : summary.transparent) {
        hashCommand(hash, command);
    }
    return hash.value();
}

[[nodiscard]] auto validateDebugPass(bool debugPassEnabled, const render::RenderScene* debugScene,
                                     std::size_t& debugCommandCount) -> core::Result<void> {
    debugCommandCount = 0;
    if (!debugPassEnabled || debugScene == nullptr) {
        return {};
    }
    if (debugScene->size() > render::RenderScene::maxCommandCount) {
        return core::unexpected(core::Error{"presentation.renderer.debug.limit",
                                            "RenderScene command limit was exceeded"});
    }
    for (const auto& command : debugScene->commands()) {
        if (command.type != render::RenderCommandType::DebugLine ||
            !core::isFinite(command.start) || !core::isFinite(command.end) ||
            !render::isValidColor(command.color)) {
            return core::unexpected(core::Error{"presentation.renderer.debug.invalid",
                                                "RenderScene contains an invalid Debug command"});
        }
    }
    debugCommandCount = debugScene->size();
    return {};
}

} // namespace

auto buildPresentationCommands(const playback::FrameSnapshot& snapshot,
                               std::span<const playback::PortableResourcePtr> resources,
                               bool manifestEmpty, bool debugPassEnabled,
                               const render::RenderScene* debugScene) -> core::Result<DrawSummary> {
    DrawSummary summary;
    summary.viewportWidth = snapshot.viewportWidth;
    summary.viewportHeight = snapshot.viewportHeight;
    summary.clearColor = {snapshot.clearRed, snapshot.clearGreen, snapshot.clearBlue,
                          snapshot.clearAlpha};
    summary.cameraActive = snapshot.camera.active;
    std::copy(std::begin(snapshot.camera.viewMatrix), std::end(snapshot.camera.viewMatrix),
              summary.viewMatrix.begin());
    std::copy(std::begin(snapshot.camera.projectionMatrix),
              std::end(snapshot.camera.projectionMatrix), summary.projectionMatrix.begin());
    summary.debugPassEnabled = debugPassEnabled;
    if (!std::all_of(summary.clearColor.begin(), summary.clearColor.end(), [](float value) {
            return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
        })) {
        return core::unexpected(core::Error{"presentation.renderer.frame.clear_color",
                                            "Clear color must be finite and within [0, 1]"});
    }
    if (!finiteMatrix(summary.viewMatrix) || !finiteMatrix(summary.projectionMatrix)) {
        return core::unexpected(nonFiniteError({}, "camera_matrix"));
    }
    if (auto debug = validateDebugPass(debugPassEnabled, debugScene, summary.debugCommandCount);
        !debug) {
        return core::unexpected(std::move(debug.error()));
    }
    if (manifestEmpty) {
        summary.digest = summaryDigest(summary);
        return summary;
    }
    if (snapshot.objects.size() > maxNormalizedRecords) {
        return core::unexpected(
            core::Error{"presentation.renderer.frame.command_budget_exceeded",
                        "Presentation command count exceeds the Portable v1 limit"}
                .withContext("limit", std::to_string(maxNormalizedRecords))
                .withContext("actual", std::to_string(snapshot.objects.size())));
    }

    std::vector<CachedMesh> meshes;
    std::vector<CachedMaterial> materials;
    std::vector<CachedTexture> textures;
    std::vector<CachedShader> shaders;
    meshes.reserve(resources.size());
    materials.reserve(resources.size());
    textures.reserve(resources.size());
    shaders.reserve(resources.size());
    for (const auto& resource : resources) {
        if (!resource) {
            continue;
        }
        if (const auto* mesh = std::get_if<playback::PortableMesh>(&resource->value)) {
            CachedMesh cached;
            cached.reference = resource->reference;
            for (std::size_t component = 0; component < 3; ++component) {
                if (!std::isfinite(mesh->boundsMin[component]) ||
                    !std::isfinite(mesh->boundsMax[component])) {
                    return core::unexpected(nonFiniteError({}, "mesh_bounds"));
                }
            }
            cached.boundsCenter = {
                (static_cast<double>(mesh->boundsMin[0]) +
                 static_cast<double>(mesh->boundsMax[0])) /
                    2.0,
                (static_cast<double>(mesh->boundsMin[1]) +
                 static_cast<double>(mesh->boundsMax[1])) /
                    2.0,
                (static_cast<double>(mesh->boundsMin[2]) +
                 static_cast<double>(mesh->boundsMax[2])) /
                    2.0,
            };
            meshes.push_back(std::move(cached));
        } else if (const auto* unlit =
                       std::get_if<playback::PortableUnlitMaterial>(&resource->value)) {
            CachedMaterial cached;
            cached.reference = resource->reference;
            cached.material = *unlit;
            materials.push_back(std::move(cached));
        } else if (const auto* parameterized =
                       std::get_if<playback::PortableParameterizedMaterial>(&resource->value)) {
            CachedMaterial cached;
            cached.reference = resource->reference;
            cached.material.baseColor[0] = 1.0F;
            cached.material.baseColor[1] = 1.0F;
            cached.material.baseColor[2] = 1.0F;
            cached.material.baseColor[3] = 1.0F;
            cached.material.alphaMode = parameterized->alphaMode;
            cached.material.doubleSided = parameterized->doubleSided;
            cached.parameterized = true;
            cached.parameterizedMaterial = *parameterized;
            materials.push_back(std::move(cached));
        } else if (std::holds_alternative<playback::PortableTexture2D>(resource->value)) {
            textures.push_back(CachedTexture{resource->reference});
        } else if (std::holds_alternative<playback::PortableShader>(resource->value)) {
            shaders.push_back(CachedShader{resource->reference});
        }
    }
    const auto byReference = [](const auto& left, const auto& right) {
        return referenceKey(left.reference) < referenceKey(right.reference);
    };
    std::sort(meshes.begin(), meshes.end(), byReference);
    std::sort(materials.begin(), materials.end(), byReference);
    std::sort(textures.begin(), textures.end(), byReference);
    std::sort(shaders.begin(), shaders.end(), byReference);

    std::vector<OrderedDraw> opaque;
    std::vector<OrderedDraw> transparent;
    opaque.reserve(snapshot.objects.size());
    transparent.reserve(snapshot.objects.size());
    bool cameraValidated = false;
    for (std::size_t objectIndex = 0; objectIndex < snapshot.objects.size(); ++objectIndex) {
        const auto& object = snapshot.objects[objectIndex];
        if (object.mesh.has_value() != object.material.has_value()) {
            const auto* reference = object.mesh ? &*object.mesh : &*object.material;
            return core::unexpected(frameError("Renderable Mesh and Material refs must be paired",
                                               object.id, reference));
        }
        if (!object.mesh) {
            if (!object.materialAssetId.empty()) {
                return core::unexpected(
                    frameError("Renderable snapshot is missing portable refs", object.id));
            }
            continue;
        }
        if (object.mesh->type != playback::PresentationResourceType::Mesh ||
            (object.material->type != playback::PresentationResourceType::UnlitMaterial &&
             object.material->type != playback::PresentationResourceType::ParameterizedMaterial) ||
            object.materialAssetId != object.material->assetId) {
            return core::unexpected(
                frameError("Snapshot portable refs have incompatible types or IDs", object.id,
                           &*object.material));
        }
        const auto* mesh = findCached(meshes, *object.mesh);
        const auto* material = findCached(materials, *object.material);
        if (mesh == nullptr || material == nullptr) {
            return core::unexpected(
                frameError("Snapshot ref is not backed by the active presentation cache", object.id,
                           mesh == nullptr ? &*object.mesh : &*object.material));
        }
        if (material->parameterized) {
            if (findCached(shaders, material->parameterizedMaterial.shader) == nullptr) {
                return core::unexpected(
                    frameError("Parameterized material is missing its shader resource", object.id,
                               &material->parameterizedMaterial.shader));
            }
            for (const auto& parameter : material->parameterizedMaterial.parameters) {
                if (parameter.type != playback::ShaderParameterType::Texture2D) {
                    continue;
                }
                if (!parameter.texture || findCached(textures, *parameter.texture) == nullptr) {
                    return core::unexpected(frameError(
                        "Parameterized texture binding is not backed by the presentation cache",
                        object.id, parameter.texture ? &*parameter.texture : &*object.material));
                }
            }
        } else if (material->material.baseColorTexture) {
            if (findCached(textures, *material->material.baseColorTexture) == nullptr) {
                return core::unexpected(
                    frameError("Material texture ref is not backed by the presentation cache",
                               object.id, &*material->material.baseColorTexture));
            }
        }
        if (!object.visible) {
            continue;
        }
        if (!snapshot.camera.active) {
            return core::unexpected(core::Error{"presentation.renderer.frame.camera_required",
                                                "Visible renderables require an active camera"}
                                        .withContext("object_id", object.id));
        }
        if (!cameraValidated) {
            if (!finiteMatrix(snapshot.camera.viewMatrix) ||
                !finiteMatrix(snapshot.camera.projectionMatrix)) {
                return core::unexpected(nonFiniteError({}, "camera_matrix"));
            }
            cameraValidated = true;
        }
        if (!finiteMatrix(object.worldMatrix)) {
            return core::unexpected(nonFiniteError(object.id, "world_matrix"));
        }
        if (!std::isfinite(object.materialOpacity)) {
            return core::unexpected(nonFiniteError(object.id, "material_opacity"));
        }

        OrderedDraw draw;
        draw.objectIndex = objectIndex;
        draw.command.objectId = object.id;
        std::copy(std::begin(object.worldMatrix), std::end(object.worldMatrix),
                  draw.command.worldMatrix.begin());
        draw.command.mesh = *object.mesh;
        draw.command.material = *object.material;
        for (std::size_t component = 0; component < 3; ++component) {
            if (!std::isfinite(material->material.baseColor[component]) ||
                !std::isfinite(object.materialTint[component])) {
                return core::unexpected(nonFiniteError(object.id, "effective_rgb"));
            }
            draw.command.effectiveColor[component] =
                static_cast<double>(material->material.baseColor[component]) *
                static_cast<double>(object.materialTint[component]);
            if (!std::isfinite(draw.command.effectiveColor[component])) {
                return core::unexpected(nonFiniteError(object.id, "effective_rgb"));
            }
        }
        if (!std::isfinite(material->material.baseColor[3])) {
            return core::unexpected(nonFiniteError(object.id, "effective_alpha"));
        }
        draw.command.effectiveColor[3] =
            static_cast<double>(material->material.baseColor[3]) * object.materialOpacity;
        if (!std::isfinite(draw.command.effectiveColor[3])) {
            return core::unexpected(nonFiniteError(object.id, "effective_alpha"));
        }

        Point3 localCenter{mesh->boundsCenter[0], mesh->boundsCenter[1], mesh->boundsCenter[2]};
        const auto worldCenter = transformPoint(object.worldMatrix, localCenter);
        const auto viewCenter = transformPoint(snapshot.camera.viewMatrix, worldCenter);
        if (!finitePoint(localCenter) || !finitePoint(worldCenter) || !finitePoint(viewCenter)) {
            return core::unexpected(nonFiniteError(object.id, "depth_transform"));
        }
        draw.command.depthMeters = -viewCenter.z;
        const double scaledDepth = draw.command.depthMeters * depthQuantization;
        const double roundedDepth = std::round(scaledDepth);
        if (!std::isfinite(draw.command.depthMeters) || !std::isfinite(scaledDepth) ||
            !std::isfinite(roundedDepth) || roundedDepth < -signedIntegerLimit ||
            roundedDepth >= signedIntegerLimit) {
            return core::unexpected(nonFiniteError(object.id, "depth"));
        }
        draw.command.transparentDepthKey = static_cast<std::int64_t>(roundedDepth);
        draw.command.backFaceCulling = !material->material.doubleSided;
        draw.command.pass =
            material->material.alphaMode == playback::PresentationAlphaMode::Blend ||
                    draw.command.effectiveColor[3] < 1.0
                ? PresentationPass::Transparent
                : PresentationPass::Opaque;
        draw.command.depthWrite = draw.command.pass == PresentationPass::Opaque;
        draw.command.sourceOverBlend = draw.command.pass == PresentationPass::Transparent;
        if (draw.command.pass == PresentationPass::Opaque) {
            opaque.push_back(std::move(draw));
        } else {
            transparent.push_back(std::move(draw));
        }
    }

    std::sort(opaque.begin(), opaque.end(), [](const auto& left, const auto& right) {
        return std::tie(left.command.objectId, left.objectIndex) <
               std::tie(right.command.objectId, right.objectIndex);
    });
    std::sort(transparent.begin(), transparent.end(), [](const auto& left, const auto& right) {
        if (left.command.transparentDepthKey != right.command.transparentDepthKey) {
            return left.command.transparentDepthKey > right.command.transparentDepthKey;
        }
        return std::tie(left.command.objectId, left.objectIndex) <
               std::tie(right.command.objectId, right.objectIndex);
    });
    summary.opaque.reserve(opaque.size());
    summary.transparent.reserve(transparent.size());
    for (const auto& draw : opaque) {
        summary.opaque.push_back(draw.command);
    }
    for (const auto& draw : transparent) {
        summary.transparent.push_back(draw.command);
    }
    summary.digest = summaryDigest(summary);
    return summary;
}

} // namespace cuexis::presentation_renderer
