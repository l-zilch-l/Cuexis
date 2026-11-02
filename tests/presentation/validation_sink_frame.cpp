#include "validation_sink.hpp"

#include "validation_sink_model.hpp"

// The ValidationSink activation and frame validation path.

namespace cuexis::test_support {

using namespace validation_detail;

void ValidationSink::activate(ValidationCandidate&& candidate) noexcept {
    active_.emplace(std::move(candidate));
}

bool ValidationSink::active() const noexcept {
    return active_.has_value();
}

auto ValidationSink::activeToken() const noexcept -> const playback::PresentationCandidateToken* {
    return active_ ? &active_->token() : nullptr;
}

auto ValidationSink::validateFrame(const playback::FrameSnapshot& snapshot,
                                   ValidationSummary& destination) const -> core::Result<void> {
    const auto fail = [&](core::Error error) -> core::Result<void> {
        destination.clear();
        return core::unexpected(std::move(error));
    };
    try {
        if (!active_) {
            return fail(core::Error{"playback.presentation.resource.missing",
                                    "Validation Sink has no active presentation candidate"});
        }
        if (snapshot.objects.size() > maxNormalizedRecords) {
            return fail(core::Error{"playback.presentation.frame.command_budget_exceeded",
                                    "Validation Sink command count exceeds the v1 limit"}
                            .withContext("limit", std::to_string(maxNormalizedRecords))
                            .withContext("actual", std::to_string(snapshot.objects.size())));
        }
        const std::array clearColor{snapshot.clearRed, snapshot.clearGreen, snapshot.clearBlue,
                                    snapshot.clearAlpha};
        if (!std::all_of(clearColor.begin(), clearColor.end(),
                         [](float value) { return std::isfinite(value); })) {
            return fail(nonFiniteError({}, "clear_color"));
        }

        destination.version = validationSummaryVersion;
        destination.viewportWidth = snapshot.viewportWidth;
        destination.viewportHeight = snapshot.viewportHeight;
        destination.clearColor = clearColor;
        destination.cameraActive = snapshot.camera.active;
        std::copy(std::begin(snapshot.camera.viewMatrix), std::end(snapshot.camera.viewMatrix),
                  destination.viewMatrix.begin());
        std::copy(std::begin(snapshot.camera.projectionMatrix),
                  std::end(snapshot.camera.projectionMatrix), destination.projectionMatrix.begin());
        destination.debugPassEnabled = active_->settings().debugPassEnabled;
        destination.digest = 0;
        if (destination.opaque.capacity() < snapshot.objects.size()) {
            destination.opaque.reserve(snapshot.objects.size());
        }
        if (destination.transparent.capacity() < snapshot.objects.size()) {
            destination.transparent.reserve(snapshot.objects.size());
        }

        std::size_t opaqueCount = 0;
        std::size_t transparentCount = 0;
        bool cameraValidated = false;
        for (std::size_t objectIndex = 0; objectIndex < snapshot.objects.size(); ++objectIndex) {
            const auto& object = snapshot.objects[objectIndex];
            if (object.mesh.has_value() != object.material.has_value()) {
                const auto* reference = object.mesh ? &*object.mesh : &*object.material;
                return fail(frameResourceError("Renderable Mesh and Material refs must be paired",
                                               object.id, reference));
            }
            if (!object.mesh) {
                if (!object.materialAssetId.empty()) {
                    return fail(frameResourceError(
                        "Renderable snapshot is missing portable resource refs", object.id));
                }
                continue;
            }
            if (object.mesh->type != playback::PresentationResourceType::Mesh ||
                (object.material->type != playback::PresentationResourceType::UnlitMaterial &&
                 object.material->type !=
                     playback::PresentationResourceType::ParameterizedMaterial) ||
                object.materialAssetId != object.material->assetId) {
                return fail(
                    frameResourceError("Snapshot portable refs have incompatible types or AssetIds",
                                       object.id, &*object.material));
            }

            const auto meshResource = locateResource(*active_, *object.mesh);
            const auto materialResource = locateResource(*active_, *object.material);
            if (!meshResource || !materialResource ||
                !std::holds_alternative<playback::PortableMesh>(meshResource->resource->value)) {
                return fail(frameResourceError(
                    "Snapshot ref is not backed by its active portable resource", object.id,
                    !meshResource ? &*object.mesh : &*object.material));
            }
            const auto& mesh = std::get<playback::PortableMesh>(meshResource->resource->value);
            const auto* unlit =
                std::get_if<playback::PortableUnlitMaterial>(&materialResource->resource->value);
            const auto* parameterized = std::get_if<playback::PortableParameterizedMaterial>(
                &materialResource->resource->value);
            if (unlit == nullptr && parameterized == nullptr) {
                return fail(
                    frameResourceError("Snapshot ref is not backed by its active portable resource",
                                       object.id, &*object.material));
            }
            if (unlit != nullptr) {
                if (unlit->baseColorTexture) {
                    const auto texture = locateResource(*active_, *unlit->baseColorTexture);
                    if (!texture ||
                        !std::holds_alternative<playback::PortableTexture2D>(
                            texture->resource->value) ||
                        materialResource->entry->dependencies.size() != 1 ||
                        materialResource->entry->dependencies.front() != *unlit->baseColorTexture) {
                        return fail(frameResourceError(
                            "Portable Material texture dependency is inconsistent", object.id,
                            &*unlit->baseColorTexture));
                    }
                } else if (!materialResource->entry->dependencies.empty()) {
                    return fail(frameResourceError("Portable Material has unexpected dependencies",
                                                   object.id, &*object.material));
                }
            } else {
                const auto expected = expectedParameterizedDependencies(*parameterized);
                if (materialResource->entry->dependencies != expected) {
                    return fail(
                        frameResourceError("Parameterized Material dependency is inconsistent",
                                           object.id, &*object.material));
                }
                for (const auto& dependency : expected) {
                    const auto located = locateResource(*active_, dependency);
                    if (!located) {
                        return fail(frameResourceError(
                            "Parameterized Material dependency is missing from the active cache",
                            object.id, &dependency));
                    }
                }
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
                    return fail(nonFiniteError({}, "camera_matrix"));
                }
                cameraValidated = true;
            }
            if (!finiteMatrix(object.worldMatrix)) {
                return fail(nonFiniteError(object.id, "world_matrix"));
            }
            if (!std::isfinite(object.materialOpacity)) {
                return fail(nonFiniteError(object.id, "material_opacity"));
            }
            if (object.materialOpacity < 0.0 || object.materialOpacity > 1.0) {
                return fail(frameValueError(object.id, "material_opacity"));
            }

            const float* baseColor = unlit != nullptr ? unlit->baseColor : nullptr;
            const float parameterizedBase[4]{1.0F, 1.0F, 1.0F, 1.0F};
            if (baseColor == nullptr) {
                baseColor = parameterizedBase;
            }
            const auto alphaMode = unlit != nullptr ? unlit->alphaMode : parameterized->alphaMode;
            const bool doubleSided =
                unlit != nullptr ? unlit->doubleSided : parameterized->doubleSided;

            std::array<double, 4> effectiveColor{};
            for (std::size_t component = 0; component < 3; ++component) {
                if (!std::isfinite(object.materialTint[component])) {
                    return fail(nonFiniteError(object.id, "material_tint"));
                }
                if (object.materialTint[component] < 0.0F ||
                    object.materialTint[component] > 1.0F) {
                    return fail(frameValueError(object.id, "material_tint"));
                }
                effectiveColor[component] = static_cast<double>(baseColor[component]) *
                                            static_cast<double>(object.materialTint[component]);
                if (!std::isfinite(effectiveColor[component])) {
                    return fail(nonFiniteError(object.id, "effective_rgb"));
                }
            }
            effectiveColor[3] = static_cast<double>(baseColor[3]) * object.materialOpacity;
            if (!std::isfinite(effectiveColor[3])) {
                return fail(nonFiniteError(object.id, "effective_alpha"));
            }

            const Point3 localCenter{
                (static_cast<double>(mesh.boundsMin[0]) + static_cast<double>(mesh.boundsMax[0])) /
                    2.0,
                (static_cast<double>(mesh.boundsMin[1]) + static_cast<double>(mesh.boundsMax[1])) /
                    2.0,
                (static_cast<double>(mesh.boundsMin[2]) + static_cast<double>(mesh.boundsMax[2])) /
                    2.0};
            const auto worldCenter = transformPoint(object.worldMatrix, localCenter);
            const auto viewCenter = transformPoint(snapshot.camera.viewMatrix, worldCenter);
            if (!finitePoint(localCenter) || !finitePoint(worldCenter) ||
                !finitePoint(viewCenter)) {
                return fail(nonFiniteError(object.id, "depth_transform"));
            }
            const auto depthMeters = -viewCenter.z;
            const auto scaledDepth = depthMeters * depthQuantization;
            const auto roundedDepth = std::round(scaledDepth);
            if (!std::isfinite(depthMeters) || !std::isfinite(scaledDepth) ||
                !std::isfinite(roundedDepth) || roundedDepth < -signedIntegerLimit ||
                roundedDepth >= signedIntegerLimit) {
                return fail(nonFiniteError(object.id, "depth"));
            }
            const auto depthKey = static_cast<std::int64_t>(roundedDepth);
            const auto pass =
                alphaMode == playback::PresentationAlphaMode::Blend || effectiveColor[3] < 1.0
                    ? ValidationPass::Transparent
                    : ValidationPass::Opaque;
            auto& command = pass == ValidationPass::Opaque
                                ? nextCommand(destination.opaque, opaqueCount)
                                : nextCommand(destination.transparent, transparentCount);
            command.objectIndex = objectIndex;
            command.objectId = object.id;
            std::copy(std::begin(object.worldMatrix), std::end(object.worldMatrix),
                      command.worldMatrix.begin());
            assignReference(command.mesh, *object.mesh);
            assignReference(command.material, *object.material);
            command.effectiveColor = effectiveColor;
            command.pass = pass;
            command.backFaceCulling = !doubleSided;
            command.depthTest = true;
            command.depthWrite = pass == ValidationPass::Opaque;
            command.sourceOverBlend = pass == ValidationPass::Transparent;
            command.depthMeters = depthMeters;
            command.transparentDepthKey = depthKey;
        }

        std::sort(destination.opaque.begin(),
                  destination.opaque.begin() + static_cast<std::ptrdiff_t>(opaqueCount),
                  [](const auto& left, const auto& right) {
                      return std::tie(left.objectId, left.objectIndex) <
                             std::tie(right.objectId, right.objectIndex);
                  });
        std::sort(destination.transparent.begin(),
                  destination.transparent.begin() + static_cast<std::ptrdiff_t>(transparentCount),
                  [](const auto& left, const auto& right) {
                      if (left.transparentDepthKey != right.transparentDepthKey) {
                          return left.transparentDepthKey > right.transparentDepthKey;
                      }
                      return std::tie(left.objectId, left.objectIndex) <
                             std::tie(right.objectId, right.objectIndex);
                  });
        destination.opaque.resize(opaqueCount);
        destination.transparent.resize(transparentCount);
        destination.digest = summaryDigest(destination);
        return {};
    } catch (const std::bad_alloc&) {
        return fail(core::Error{"playback.presentation.frame.command_budget_exceeded",
                                "Validation Sink command allocation could not be satisfied"}
                        .withContext("limit", std::to_string(maxNormalizedRecords))
                        .withContext("actual", "allocation_failed"));
    } catch (const std::exception& exception) {
        return fail(frameResourceError("Validation Sink frame validation failed", {})
                        .withContext("exception", exception.what()));
    } catch (...) {
        return fail(frameResourceError("Validation Sink frame validation failed", {}));
    }
}

static_assert(std::is_nothrow_move_constructible_v<ValidationCandidate>);
static_assert(std::is_nothrow_move_assignable_v<ValidationCandidate>);
} // namespace cuexis::test_support
