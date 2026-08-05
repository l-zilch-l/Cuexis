//  ChartWorldInstantiator 实现 — ChartRuntime → EnTT World 实例化
//  validate(): 无副作用检查排序、parent 引用、Behavior 引用、组件一致性
//  instantiate(): 创建 Entity → 设置 Transform/Hierarchy/Renderable/Behavior/GameplayTag
//  → 建立层级 → 更新世界矩阵 → 构建 ObjectEntityMap
//  阶段 1A 无资源时 Renderable 返回明确错误；阶段 1B 从 Scope 注入 Handle

#include <cuexis/runtime/chart_world_instantiator.hpp>

#include <cuexis/behavior/behavior_component.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/gameplay/tags.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/property.hpp>
#include <cuexis/world/transform_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>

namespace cuexis::runtime {
namespace {

void addObjectError(core::Diagnostics& diagnostics, std::string code, std::string message,
                    std::size_t objectIndex, const chart::ChartObjectId& objectId) {
    static_cast<void>(diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                                       std::move(code), std::move(message),
                                                       "/objects/" + std::to_string(objectIndex)}
                                          .withContext("object_id", objectId.value)));
}

[[nodiscard]] auto behaviorIndex(const chart::ChartRuntime& runtime,
                                 const chart::BehaviorId& behaviorId)
    -> std::optional<std::uint32_t> {
    const auto behavior =
        std::lower_bound(runtime.behaviors.begin(), runtime.behaviors.end(), behaviorId,
                         [](const chart::RuntimeBehavior& candidate, const chart::BehaviorId& id) {
                             return candidate.id < id;
                         });
    if (behavior == runtime.behaviors.end() || behavior->id != behaviorId) {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(
        static_cast<std::size_t>(behavior - runtime.behaviors.begin()));
}

[[nodiscard]] auto isTransformProperty(chart::BehaviorProperty property) noexcept -> bool {
    return property == chart::BehaviorProperty::TransformPositionX ||
           property == chart::BehaviorProperty::TransformPositionY ||
           property == chart::BehaviorProperty::TransformPositionZ ||
           property == chart::BehaviorProperty::TransformRotation ||
           property == chart::BehaviorProperty::TransformScale;
}

void addWorldError(core::Diagnostics& diagnostics, const core::Error& error) {
    core::Diagnostic diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                std::string{error.message()}};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

} // namespace

auto ObjectEntityMap::find(const chart::ChartObjectId& objectId) const noexcept
    -> std::optional<entt::entity> {
    const auto entry =
        std::lower_bound(entries_.begin(), entries_.end(), objectId,
                         [](const ObjectEntityEntry& candidate, const chart::ChartObjectId& id) {
                             return candidate.objectId < id;
                         });
    if (entry == entries_.end() || entry->objectId != objectId) {
        return std::nullopt;
    }
    return entry->entity;
}

auto ObjectEntityMap::size() const noexcept -> std::size_t {
    return entries_.size();
}

auto ObjectEntityMap::entries() const noexcept -> const std::vector<ObjectEntityEntry>& {
    return entries_;
}

void ObjectEntityMap::clear() noexcept {
    entries_.clear();
}

auto ChartWorldInstantiator::validate(const chart::ChartRuntime& runtime) -> core::Diagnostics {
    core::Diagnostics diagnostics{
        runtimeDiagnosticLimit,
        core::Diagnostic{core::DiagnosticSeverity::Error, "runtime.chart.diagnostic_limit",
                         "Runtime Chart diagnostic limit was reached", "/objects"}};

    if (runtime.behaviors.size() > std::numeric_limits<std::uint32_t>::max()) {
        static_cast<void>(diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "runtime.chart.too_many_behaviors",
            "Runtime behavior count exceeds the stable behavior index capacity", "/behaviors"}));
    }
    for (std::size_t index = 1; index < runtime.behaviors.size(); ++index) {
        if (!(runtime.behaviors[index - 1].id < runtime.behaviors[index].id)) {
            static_cast<void>(diagnostics.add(core::Diagnostic{
                core::DiagnosticSeverity::Error, "runtime.chart.behaviors_not_strictly_sorted",
                "Runtime behaviors must be unique and sorted by BehaviorId", "/behaviors"}));
            break;
        }
    }
    for (std::size_t behaviorIndexValue = 0; behaviorIndexValue < runtime.behaviors.size();
         ++behaviorIndexValue) {
        const auto& behavior = runtime.behaviors[behaviorIndexValue];
        for (std::size_t trackIndex = 0; trackIndex < behavior.tracks.size(); ++trackIndex) {
            const auto& track = behavior.tracks[trackIndex];
            if (trackIndex != 0 && !(behavior.tracks[trackIndex - 1].property < track.property)) {
                static_cast<void>(diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "runtime.chart.tracks_not_strictly_sorted",
                    "Runtime Behavior Tracks must be unique and sorted by Property",
                    "/behaviors/" + std::to_string(behaviorIndexValue) + "/tracks"}));
                break;
            }
            if (track.keys.empty()) {
                static_cast<void>(diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "runtime.chart.behavior_track_empty",
                    "Runtime Behavior Track must contain at least one key",
                    "/behaviors/" + std::to_string(behaviorIndexValue) + "/tracks/" +
                        std::to_string(trackIndex) + "/keys"}));
                continue;
            }
            for (std::size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex) {
                if (!std::isfinite(track.keys[keyIndex].chartTimeMs) ||
                    (keyIndex != 0 &&
                     !(track.keys[keyIndex - 1].chartTimeMs < track.keys[keyIndex].chartTimeMs))) {
                    static_cast<void>(diagnostics.add(core::Diagnostic{
                        core::DiagnosticSeverity::Error,
                        "runtime.chart.behavior_keys_not_strictly_sorted",
                        "Runtime Behavior keys must have finite strictly increasing times",
                        "/behaviors/" + std::to_string(behaviorIndexValue) + "/tracks/" +
                            std::to_string(trackIndex) + "/keys"}));
                    break;
                }
            }
        }
        for (std::size_t trackIndex = 0; trackIndex < behavior.eventTracks.size(); ++trackIndex) {
            const auto& track = behavior.eventTracks[trackIndex];
            if (trackIndex != 0 &&
                !(behavior.eventTracks[trackIndex - 1].property < track.property)) {
                static_cast<void>(diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error,
                    "runtime.chart.event_tracks_not_strictly_sorted",
                    "Runtime Event Tracks must be unique and sorted by Property",
                    "/behaviors/" + std::to_string(behaviorIndexValue) + "/eventTracks"}));
                break;
            }
            if (track.events.empty()) {
                static_cast<void>(diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "runtime.chart.event_track_empty",
                    "Runtime Event Track must contain at least one event",
                    "/behaviors/" + std::to_string(behaviorIndexValue) + "/eventTracks/" +
                        std::to_string(trackIndex)}));
                continue;
            }
            for (std::size_t eventIndex = 0; eventIndex < track.events.size(); ++eventIndex) {
                const auto& event = track.events[eventIndex];
                if (!std::isfinite(event.startBeat) || !std::isfinite(event.endBeat) ||
                    event.endBeat < event.startBeat ||
                    (eventIndex != 0 && track.events[eventIndex - 1].endBeat > event.startBeat)) {
                    static_cast<void>(diagnostics.add(core::Diagnostic{
                        core::DiagnosticSeverity::Error, "runtime.chart.events_invalid",
                        "Runtime Events must be finite, sorted, and non-overlapping",
                        "/behaviors/" + std::to_string(behaviorIndexValue) + "/eventTracks/" +
                            std::to_string(trackIndex)}));
                    break;
                }
            }
        }
        for (std::size_t trackIndex = 0; trackIndex < behavior.stepTracks.size(); ++trackIndex) {
            const auto& track = behavior.stepTracks[trackIndex];
            if (trackIndex != 0 &&
                !(behavior.stepTracks[trackIndex - 1].property < track.property)) {
                static_cast<void>(diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error,
                    "runtime.chart.step_tracks_not_strictly_sorted",
                    "Runtime Step Tracks must be unique and sorted by Property",
                    "/behaviors/" + std::to_string(behaviorIndexValue) + "/stepTracks"}));
                break;
            }
            for (std::size_t eventIndex = 0; eventIndex < track.events.size(); ++eventIndex) {
                if (!std::isfinite(track.events[eventIndex].beat) ||
                    (eventIndex != 0 &&
                     !(track.events[eventIndex - 1].beat < track.events[eventIndex].beat))) {
                    static_cast<void>(diagnostics.add(core::Diagnostic{
                        core::DiagnosticSeverity::Error, "runtime.chart.step_events_invalid",
                        "Runtime Step Events must have finite strictly increasing Beats",
                        "/behaviors/" + std::to_string(behaviorIndexValue) + "/stepTracks/" +
                            std::to_string(trackIndex)}));
                    break;
                }
            }
        }
    }

    std::size_t propertyWrites = 0;
    for (std::size_t index = 0; index < runtime.objects.size(); ++index) {
        const auto& object = runtime.objects[index];
        if (index != 0 && !(runtime.objects[index - 1].id < object.id)) {
            addObjectError(diagnostics, "runtime.chart.objects_not_strictly_sorted",
                           "Runtime objects must be unique and sorted by ChartObjectId", index,
                           object.id);
        }
        if (object.parentIndex.has_value() &&
            (*object.parentIndex >= runtime.objects.size() || *object.parentIndex == index)) {
            addObjectError(diagnostics, "runtime.chart.invalid_parent_index",
                           "Runtime object parent index is invalid", index, object.id);
        }
        if (object.components.behavior.has_value()) {
            const auto resolvedBehavior =
                behaviorIndex(runtime, object.components.behavior->behavior);
            if (!resolvedBehavior.has_value()) {
                addObjectError(diagnostics, "runtime.chart.behavior_not_found",
                               "Runtime object references an unavailable behavior", index,
                               object.id);
                continue;
            }
            const auto& behavior = runtime.behaviors[*resolvedBehavior];
            const auto trackCount =
                behavior.tracks.size() + behavior.eventTracks.size() + behavior.stepTracks.size();
            if (trackCount > world::maxPropertyWritesPerFrame ||
                propertyWrites > world::maxPropertyWritesPerFrame - trackCount) {
                addObjectError(diagnostics, "runtime.chart.property_write_limit",
                               "Runtime behavior bindings exceed the per-frame write limit", index,
                               object.id);
                continue;
            }
            propertyWrites += trackCount;
            for (const auto& track : behavior.tracks) {
                if (isTransformProperty(track.property) &&
                    !object.components.transform.has_value()) {
                    addObjectError(diagnostics, "runtime.chart.behavior_transform_missing",
                                   "Transform Behavior target has no cuexis.transform component",
                                   index, object.id);
                }
                if (track.property == chart::BehaviorProperty::CameraFovY &&
                    !object.components.camera.has_value()) {
                    addObjectError(diagnostics, "runtime.chart.behavior_camera_missing",
                                   "camera.fovY target has no cuexis.camera component", index,
                                   object.id);
                }
            }
            for (const auto& track : behavior.eventTracks) {
                if (isTransformProperty(track.property) &&
                    !object.components.transform.has_value()) {
                    addObjectError(diagnostics, "runtime.chart.behavior_transform_missing",
                                   "Transform Behavior target has no cuexis.transform component",
                                   index, object.id);
                }
                if (track.property == chart::BehaviorProperty::CameraFovY &&
                    !object.components.camera.has_value()) {
                    addObjectError(diagnostics, "runtime.chart.behavior_camera_missing",
                                   "camera.fovY target has no cuexis.camera component", index,
                                   object.id);
                }
                if ((track.property == chart::BehaviorProperty::MaterialOpacity ||
                     track.property == chart::BehaviorProperty::MaterialTint) &&
                    !object.components.renderable.has_value()) {
                    addObjectError(diagnostics, "runtime.chart.behavior_renderable_missing",
                                   "Material Behavior target has no cuexis.renderable component",
                                   index, object.id);
                }
            }
            for (const auto& track : behavior.stepTracks) {
                static_cast<void>(track);
                if (!object.components.renderable.has_value()) {
                    addObjectError(diagnostics, "runtime.chart.behavior_renderable_missing",
                                   "Render Step Event target has no cuexis.renderable component",
                                   index, object.id);
                }
            }
        }
    }

    diagnostics.sortDeterministically();
    return diagnostics;
}

auto ChartWorldInstantiator::instantiate(const chart::ChartRuntime& runtime)
    -> ChartWorldInstantiationResult {
    return instantiate(runtime, {}, 0);
}

auto ChartWorldInstantiator::instantiate(
    const chart::ChartRuntime& runtime,
    std::span<const std::optional<ResolvedRenderableResources>> renderableResources,
    std::uint64_t expectedManagerToken) -> ChartWorldInstantiationResult {
    ChartWorldInstantiationResult result;

    result.diagnostics.append(validate(runtime));
    if (result.diagnostics.hasErrors()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    if (!renderableResources.empty() && renderableResources.size() != runtime.objects.size()) {
        static_cast<void>(result.diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "runtime.chart.resource_binding_count_mismatch",
            "Renderable resource bindings must match the Runtime object count", "/objects"}));
    }
    if (!renderableResources.empty() && expectedManagerToken == 0) {
        static_cast<void>(result.diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "runtime.chart.resource_manager_token_invalid",
            "Renderable resource bindings require a valid ResourceManager token", "/objects"}));
    }

    for (std::size_t index = 0; index < runtime.objects.size(); ++index) {
        const auto& object = runtime.objects[index];
        if (object.components.renderable.has_value()) {
            if (renderableResources.empty()) {
                addObjectError(result.diagnostics, "runtime.chart.renderable_resources_unsupported",
                               "Renderable resources require an injected ResourceManager", index,
                               object.id);
            } else if (renderableResources.size() == runtime.objects.size() &&
                       !renderableResources[index].has_value()) {
                addObjectError(result.diagnostics, "runtime.chart.renderable_binding_missing",
                               "Renderable object has no resolved resource binding", index,
                               object.id);
            } else if (renderableResources.size() == runtime.objects.size() &&
                       (!renderableResources[index]->mesh.belongsTo(expectedManagerToken) ||
                        !renderableResources[index]->material.belongsTo(expectedManagerToken))) {
                addObjectError(result.diagnostics, "runtime.chart.renderable_handle_invalid",
                               "Renderable object received an invalid or foreign resource Handle",
                               index, object.id);
            }
        } else if (renderableResources.size() == runtime.objects.size() &&
                   renderableResources[index].has_value()) {
            addObjectError(result.diagnostics, "runtime.chart.renderable_binding_unexpected",
                           "Non-renderable object received a resource binding", index, object.id);
        }
    }

    if (result.diagnostics.hasErrors()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    try {
        auto world = std::make_unique<world::World>();
        ObjectEntityMap objects;
        objects.entries_.reserve(runtime.objects.size());

        auto populated = world->withRegistry([&](entt::registry& registry) {
            for (const auto& object : runtime.objects) {
                objects.entries_.push_back(
                    ObjectEntityEntry{.objectId = object.id, .entity = registry.create()});
            }

            for (std::size_t index = 0; index < runtime.objects.size(); ++index) {
                const auto& object = runtime.objects[index];
                const entt::entity entity = objects.entries_[index].entity;

                if (object.components.transform.has_value()) {
                    const auto& transform = *object.components.transform;
                    registry.emplace<world::TransformComponent>(
                        entity, transform.position, transform.rotation, transform.scale);
                }
                if (object.parentIndex.has_value()) {
                    registry.emplace<world::HierarchyComponent>(
                        entity, objects.entries_[*object.parentIndex].entity);
                }
                if (object.components.behavior.has_value()) {
                    registry.emplace<behavior::BehaviorComponent>(
                        entity, behavior::RuntimeBehaviorIndex{
                                    *behaviorIndex(runtime, object.components.behavior->behavior)});
                }
                if (object.components.note.has_value()) {
                    registry.emplace<gameplay::NoteTag>(entity);
                }
                if (object.components.element) {
                    registry.emplace<gameplay::ElementTag>(entity);
                }
                if (object.components.renderable.has_value()) {
                    const auto& resources = *renderableResources[index];
                    registry.emplace<render::RenderableComponent>(entity, resources.mesh,
                                                                  resources.material);
                    registry.emplace<render::AppearanceComponent>(
                        entity, true, object.components.renderable->material.value, 1.0,
                        core::Vec3{1.0F, 1.0F, 1.0F});
                }
                if (object.components.camera.has_value()) {
                    const auto& cam = *object.components.camera;
                    registry.emplace<render::CameraComponent>(
                        entity, cam.type, cam.fovY, cam.nearPlane, cam.farPlane, 0.0, 0.0, 0.0);
                }
            }

            const auto cameraEntity = registry.create();
            core::Vec3 cameraPosition{};
            if (runtime.camera.defaultTransform.has_value()) {
                cameraPosition = runtime.camera.defaultTransform->position;
            }
            const auto pitchRad =
                static_cast<float>(runtime.camera.pitch * 3.14159265358979323846 / 180.0);
            const auto yawRad =
                static_cast<float>(runtime.camera.yaw * 3.14159265358979323846 / 180.0);
            const auto rollRad =
                static_cast<float>(runtime.camera.roll * 3.14159265358979323846 / 180.0);
            const auto halfPitch = pitchRad * 0.5F;
            const auto halfYaw = yawRad * 0.5F;
            const auto halfRoll = rollRad * 0.5F;
            const auto cosPitch = std::cos(halfPitch);
            const auto sinPitch = std::sin(halfPitch);
            const auto cosYaw = std::cos(halfYaw);
            const auto sinYaw = std::sin(halfYaw);
            const auto cosRoll = std::cos(halfRoll);
            const auto sinRoll = std::sin(halfRoll);
            const core::Quat cameraRotation{
                sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw,
                cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw,
                cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw,
                cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw,
            };
            registry.emplace<world::TransformComponent>(
                cameraEntity, cameraPosition, cameraRotation, core::Vec3{1.0F, 1.0F, 1.0F});
            registry.emplace<render::CameraComponent>(cameraEntity, runtime.camera.type,
                                                      runtime.camera.fovY, runtime.camera.nearPlane,
                                                      runtime.camera.farPlane, runtime.camera.pitch,
                                                      runtime.camera.yaw, runtime.camera.roll);
        });
        if (!populated) {
            addWorldError(result.diagnostics, populated.error());
            result.diagnostics.sortDeterministically();
            return result;
        }

        auto transforms = world::updateWorldTransforms(*world);
        if (!transforms) {
            addWorldError(result.diagnostics, transforms.error());
            result.diagnostics.sortDeterministically();
            return result;
        }

        result.value.emplace(
            ChartWorldInstantiation{.world = std::move(world), .objects = std::move(objects)});
        return result;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& exception) {
        static_cast<void>(result.diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "runtime.chart.instantiation_exception",
            std::string{"World instantiation failed: "} + exception.what()}));
    } catch (...) {
        static_cast<void>(result.diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "runtime.chart.instantiation_exception",
            "World instantiation failed with a non-standard exception"}));
    }

    result.diagnostics.sortDeterministically();
    return result;
}

} // namespace cuexis::runtime
