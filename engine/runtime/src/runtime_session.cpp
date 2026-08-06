// RuntimeSession transactional lifecycle and absolute-time Behavior evaluation.

#include <cuexis/runtime/runtime_session.hpp>

#include <cuexis/behavior/behavior_program.hpp>
#include <cuexis/behavior/behavior_system.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/world/property.hpp>
#include <cuexis/world/transform_system.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cuexis::runtime {

class RuntimeEvaluationState final {
  public:
    explicit RuntimeEvaluationState(std::size_t requiredWrites) : writes(requiredWrites) {}

    struct CameraEntry final {
        entt::entity entity{entt::null};
        double baselineFovY{60.0};
        double candidateFovY{60.0};
        render::CameraComponent previous{};
        bool seen{};
    };

    struct AppearanceEntry final {
        entt::entity entity{entt::null};
        render::AppearanceComponent baseline;
        render::AppearanceComponent candidate;
        render::AppearanceComponent previous;
    };

    behavior::BehaviorProgram program;
    world::PropertyWriteBuffer writes;
    world::TransformPropertyResolver transformResolver;
    std::vector<CameraEntry> cameras;
    std::vector<AppearanceEntry> appearances;
    bool camerasCommitted{};
    bool appearancesCommitted{};
};

namespace {

static_assert(std::is_nothrow_move_constructible_v<chart::ChartRuntime>);
static_assert(std::is_nothrow_move_assignable_v<chart::ChartRuntime>);
static_assert(std::is_nothrow_move_assignable_v<ObjectEntityMap>);
static_assert(std::is_nothrow_move_assignable_v<core::Diagnostics>);
static_assert(std::is_nothrow_move_assignable_v<assets::ResourceScope>);

std::atomic<std::uint64_t> nextSessionToken{1};

[[nodiscard]] auto allocateSessionToken() noexcept -> std::uint64_t {
    const auto token = nextSessionToken.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        std::terminate();
    }
    return token;
}

void addSessionError(core::Diagnostics& diagnostics, std::string code, std::string message) {
    diagnostics.add(
        core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code), std::move(message)});
}

void addSessionError(core::Diagnostics& diagnostics, const core::Error& error) {
    core::Diagnostic diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                std::string{error.message()}};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto toPropertyId(chart::BehaviorProperty property) noexcept -> world::PropertyId {
    switch (property) {
    case chart::BehaviorProperty::TransformPositionX:
        return world::PropertyId::TransformPositionX;
    case chart::BehaviorProperty::TransformPositionY:
        return world::PropertyId::TransformPositionY;
    case chart::BehaviorProperty::TransformPositionZ:
        return world::PropertyId::TransformPositionZ;
    case chart::BehaviorProperty::TransformRotation:
        return world::PropertyId::TransformRotation;
    case chart::BehaviorProperty::TransformScale:
        return world::PropertyId::TransformScale;
    case chart::BehaviorProperty::CameraFovY:
        return world::PropertyId::CameraFovY;
    case chart::BehaviorProperty::MaterialOpacity:
        return world::PropertyId::MaterialOpacity;
    case chart::BehaviorProperty::MaterialTint:
        return world::PropertyId::MaterialTint;
    }
    return world::PropertyId::TransformPositionX;
}

[[nodiscard]] auto toPropertyId(chart::BehaviorStepProperty property) noexcept
    -> world::PropertyId {
    switch (property) {
    case chart::BehaviorStepProperty::RenderVisible:
        return world::PropertyId::RenderVisible;
    case chart::BehaviorStepProperty::RenderMaterial:
        return world::PropertyId::RenderMaterial;
    }
    return world::PropertyId::RenderVisible;
}

[[nodiscard]] auto toPropertyValue(chart::BehaviorValue value) -> world::PropertyValue {
    return std::visit(
        [](auto&& item) -> world::PropertyValue {
            return world::PropertyValue{std::forward<decltype(item)>(item)};
        },
        std::move(value));
}

[[nodiscard]] auto toPropertyValue(chart::BehaviorStepValue&& value) -> world::PropertyValue {
    if (const auto* visible = std::get_if<bool>(&value)) {
        return world::PropertyValue{std::in_place_type<bool>, *visible};
    }
    auto material = std::get<chart::AssetId>(std::move(value));
    return world::PropertyValue{std::in_place_type<std::string>, std::move(material.value)};
}

[[nodiscard]] auto baselineFor(const chart::RuntimeObject& object, world::PropertyId property)
    -> core::Result<world::PropertyValue> {
    switch (property) {
    case world::PropertyId::TransformPositionX:
    case world::PropertyId::TransformPositionY:
    case world::PropertyId::TransformPositionZ:
        if (object.components.transform) {
            const auto& position = object.components.transform->position;
            const double value =
                property == world::PropertyId::TransformPositionX
                    ? position.x
                    : (property == world::PropertyId::TransformPositionY ? position.y : position.z);
            return world::PropertyValue{value};
        }
        break;
    case world::PropertyId::TransformRotation:
        if (object.components.transform) {
            return world::PropertyValue{object.components.transform->rotation};
        }
        break;
    case world::PropertyId::TransformScale:
        if (object.components.transform) {
            return world::PropertyValue{object.components.transform->scale};
        }
        break;
    case world::PropertyId::CameraFovY:
        if (object.components.camera) {
            return world::PropertyValue{object.components.camera->fovY};
        }
        break;
    case world::PropertyId::RenderVisible:
        if (object.components.renderable) {
            return world::PropertyValue{true};
        }
        break;
    case world::PropertyId::RenderMaterial:
        if (object.components.renderable) {
            return world::PropertyValue{object.components.renderable->material.value};
        }
        break;
    case world::PropertyId::MaterialOpacity:
        if (object.components.renderable) {
            return world::PropertyValue{1.0};
        }
        break;
    case world::PropertyId::MaterialTint:
        if (object.components.renderable) {
            return world::PropertyValue{core::Vec3{1.0F, 1.0F, 1.0F}};
        }
        break;
    }
    return core::unexpected(core::Error{"runtime.program.baseline_missing",
                                        "Behavior property target has no compatible component"});
}

[[nodiscard]] auto toEasing(chart::BehaviorEasing easing) noexcept -> behavior::Easing {
    switch (easing) {
    case chart::BehaviorEasing::Linear:
        return behavior::Easing::Linear;
    case chart::BehaviorEasing::InCubic:
        return behavior::Easing::InCubic;
    case chart::BehaviorEasing::OutCubic:
        return behavior::Easing::OutCubic;
    case chart::BehaviorEasing::InOutCubic:
        return behavior::Easing::InOutCubic;
    }
    return behavior::Easing::Linear;
}

[[nodiscard]] auto buildEvaluationState(chart::ChartRuntime& runtime,
                                        const ObjectEntityMap& objects, world::World& world)
    -> core::Result<std::unique_ptr<RuntimeEvaluationState>> {
    std::size_t requiredWrites = 0;
    for (std::size_t objectIndex = 0; objectIndex < runtime.objects.size(); ++objectIndex) {
        const auto& reference = runtime.objects[objectIndex].components.behavior;
        if (!reference.has_value()) {
            continue;
        }
        const auto behavior = std::lower_bound(
            runtime.behaviors.begin(), runtime.behaviors.end(), reference->behavior,
            [](const chart::RuntimeBehavior& candidate, const chart::BehaviorId& id) {
                return candidate.id < id;
            });
        if (behavior == runtime.behaviors.end() || behavior->id != reference->behavior) {
            return core::unexpected(core::Error{"runtime.program.behavior_missing",
                                                "Behavior binding was not validated"});
        }
        const auto trackCount =
            behavior->tracks.size() + behavior->eventTracks.size() + behavior->stepTracks.size();
        if (trackCount > world::maxPropertyWritesPerFrame ||
            requiredWrites > world::maxPropertyWritesPerFrame - trackCount) {
            return core::unexpected(core::Error{"runtime.program.write_limit",
                                                "Behavior program exceeds the write budget"});
        }
        requiredWrites += trackCount;
    }

    auto state = std::make_unique<RuntimeEvaluationState>(requiredWrites);
    state->program.definitions.reserve(runtime.behaviors.size());
    for (auto& runtimeBehavior : runtime.behaviors) {
        behavior::BehaviorDefinition definition;
        definition.tracks.reserve(runtimeBehavior.tracks.size());
        for (auto& runtimeTrack : runtimeBehavior.tracks) {
            behavior::BehaviorTrack track{.property = toPropertyId(runtimeTrack.property),
                                          .keys = {}};
            track.keys.reserve(runtimeTrack.keys.size());
            for (auto& runtimeKey : runtimeTrack.keys) {
                track.keys.push_back(behavior::BehaviorKey{
                    .chartTimeMs = runtimeKey.chartTimeMs,
                    .value = toPropertyValue(std::move(runtimeKey.value)),
                    .easing = toEasing(runtimeKey.easing),
                });
            }
            definition.tracks.push_back(std::move(track));
        }
        definition.eventTracks.reserve(runtimeBehavior.eventTracks.size());
        for (auto& runtimeTrack : runtimeBehavior.eventTracks) {
            behavior::BehaviorEventTrack track{.property = toPropertyId(runtimeTrack.property),
                                               .events = {}};
            track.events.reserve(runtimeTrack.events.size());
            for (auto& runtimeEvent : runtimeTrack.events) {
                track.events.push_back(behavior::BehaviorEvent{
                    .startBeat = runtimeEvent.startBeat,
                    .endBeat = runtimeEvent.endBeat,
                    .startValue = toPropertyValue(std::move(runtimeEvent.startValue)),
                    .endValue = toPropertyValue(std::move(runtimeEvent.endValue)),
                    .startSlope = runtimeEvent.startSlope,
                    .endSlope = runtimeEvent.endSlope,
                    .instantaneous = runtimeEvent.instantaneous,
                });
            }
            definition.eventTracks.push_back(std::move(track));
        }
        definition.stepTracks.reserve(runtimeBehavior.stepTracks.size());
        for (auto& runtimeTrack : runtimeBehavior.stepTracks) {
            behavior::BehaviorStepTrack track{.property = toPropertyId(runtimeTrack.property),
                                              .events = {}};
            track.events.reserve(runtimeTrack.events.size());
            for (auto& runtimeEvent : runtimeTrack.events) {
                track.events.push_back(behavior::BehaviorStepEvent{
                    .beat = runtimeEvent.beat,
                    .value = toPropertyValue(std::move(runtimeEvent.value)),
                });
            }
            definition.stepTracks.push_back(std::move(track));
        }
        state->program.definitions.push_back(std::move(definition));
    }

    if (objects.entries().size() != runtime.objects.size()) {
        return core::unexpected(core::Error{"runtime.program.object_map_mismatch",
                                            "Runtime object map does not match ChartRuntime"});
    }
    state->program.bindings.reserve(runtime.objects.size());
    for (std::size_t index = 0; index < runtime.objects.size(); ++index) {
        const auto& reference = runtime.objects[index].components.behavior;
        if (!reference.has_value()) {
            continue;
        }
        const auto behavior = std::lower_bound(
            runtime.behaviors.begin(), runtime.behaviors.end(), reference->behavior,
            [](const chart::RuntimeBehavior& candidate, const chart::BehaviorId& id) {
                return candidate.id < id;
            });
        const auto behaviorIndex = static_cast<std::size_t>(behavior - runtime.behaviors.begin());
        behavior::BehaviorBinding binding{
            .entity = objects.entries()[index].entity,
            .behavior = behavior::RuntimeBehaviorIndex{static_cast<std::uint32_t>(behaviorIndex)},
            .baselines = {},
        };
        const auto& definition = state->program.definitions[behaviorIndex];
        binding.baselines.reserve(definition.eventTracks.size() + definition.stepTracks.size());
        for (const auto& track : definition.eventTracks) {
            auto baseline = baselineFor(runtime.objects[index], track.property);
            if (!baseline) {
                return core::unexpected(std::move(baseline.error()));
            }
            binding.baselines.push_back(
                behavior::PropertyBaseline{track.property, std::move(*baseline)});
        }
        for (const auto& track : definition.stepTracks) {
            auto baseline = baselineFor(runtime.objects[index], track.property);
            if (!baseline) {
                return core::unexpected(std::move(baseline.error()));
            }
            binding.baselines.push_back(
                behavior::PropertyBaseline{track.property, std::move(*baseline)});
        }
        state->program.bindings.push_back(std::move(binding));
    }

    // The behavior evaluator owns its normalized program after preparation.  Do not retain a
    // second copy of every key in the long-lived ChartRuntime.
    runtime.behaviors.clear();

    auto resolver = world::TransformPropertyResolver::capture(world);
    if (!resolver) {
        return core::unexpected(std::move(resolver.error()));
    }
    state->transformResolver = std::move(*resolver);

    auto cameras = world.withRegistry([&](const entt::registry& registry) {
        const auto cameraView = registry.view<const render::CameraComponent>();
        for (const entt::entity entity : cameraView) {
            const auto& camera = cameraView.get<const render::CameraComponent>(entity);
            state->cameras.push_back(RuntimeEvaluationState::CameraEntry{
                .entity = entity,
                .baselineFovY = camera.fovY,
                .candidateFovY = camera.fovY,
                .previous = camera,
            });
        }
    });
    if (!cameras) {
        return core::unexpected(std::move(cameras.error()));
    }
    std::sort(state->cameras.begin(), state->cameras.end(),
              [](const auto& left, const auto& right) {
                  return entt::to_integral(left.entity) < entt::to_integral(right.entity);
              });
    const auto maximumMaterialSize = [&](entt::entity entity, std::size_t baselineSize) {
        std::size_t result = baselineSize;
        for (const auto& binding : state->program.bindings) {
            if (binding.entity != entity) {
                continue;
            }
            const auto& definition = state->program.definitions[binding.behavior.value];
            for (const auto& track : definition.stepTracks) {
                if (track.property != world::PropertyId::RenderMaterial) {
                    continue;
                }
                for (const auto& event : track.events) {
                    if (const auto* value = std::get_if<std::string>(&event.value)) {
                        result = std::max(result, value->size());
                    }
                }
            }
        }
        return result;
    };
    auto appearances = world.withRegistry([&](entt::registry& registry) {
        const auto view = registry.view<render::AppearanceComponent>();
        for (const entt::entity entity : view) {
            auto& appearance = view.get<render::AppearanceComponent>(entity);
            const auto materialSize =
                maximumMaterialSize(entity, appearance.materialAssetId.size());
            appearance.materialAssetId.reserve(materialSize);
            RuntimeEvaluationState::AppearanceEntry entry{
                .entity = entity,
                .baseline = appearance,
                .candidate = appearance,
                .previous = appearance,
            };
            entry.candidate.materialAssetId.reserve(materialSize);
            entry.previous.materialAssetId.reserve(materialSize);
            state->appearances.push_back(std::move(entry));
        }
    });
    if (!appearances) {
        return core::unexpected(std::move(appearances.error()));
    }
    std::sort(state->appearances.begin(), state->appearances.end(),
              [](const auto& left, const auto& right) {
                  return entt::to_integral(left.entity) < entt::to_integral(right.entity);
              });
    return state;
}

[[nodiscard]] auto prepareCameras(RuntimeEvaluationState& state) -> core::Result<void> {
    state.camerasCommitted = false;
    for (auto& camera : state.cameras) {
        camera.candidateFovY = camera.baselineFovY;
        camera.seen = false;
    }
    for (const auto& write : state.writes.writes()) {
        if (write.property != world::PropertyId::CameraFovY) {
            continue;
        }
        const auto camera = std::lower_bound(
            state.cameras.begin(), state.cameras.end(), write.entity,
            [](const RuntimeEvaluationState::CameraEntry& candidate, entt::entity entity) {
                return entt::to_integral(candidate.entity) < entt::to_integral(entity);
            });
        if (camera == state.cameras.end() || camera->entity != write.entity) {
            return core::unexpected(core::Error{"runtime.camera.binding_missing",
                                                "camera.fovY target has no CameraComponent"});
        }
        if (camera->seen) {
            return core::unexpected(core::Error{"runtime.camera.write_conflict",
                                                "camera.fovY was written more than once"});
        }
        const auto* value = std::get_if<double>(&write.value);
        if (value == nullptr || !std::isfinite(*value) || *value <= 0.0 || *value >= 179.0) {
            return core::unexpected(core::Error{"runtime.camera.fov_out_of_range",
                                                "camera.fovY must be between 0 and 179 degrees"});
        }
        camera->candidateFovY = *value;
        camera->seen = true;
    }
    return {};
}

[[nodiscard]] auto commitCameras(RuntimeEvaluationState& state, world::World& world)
    -> core::Result<void> {
    auto result = world.withRegistry([&](entt::registry& registry) -> core::Result<void> {
        for (const auto& camera : state.cameras) {
            if (!registry.valid(camera.entity) ||
                !registry.all_of<render::CameraComponent>(camera.entity)) {
                return core::unexpected(core::Error{"runtime.camera.baseline_missing",
                                                    "A captured CameraComponent is unavailable"});
            }
        }
        for (auto& camera : state.cameras) {
            auto& component = registry.get<render::CameraComponent>(camera.entity);
            camera.previous = component;
            component.fovY = camera.candidateFovY;
        }
        return {};
    });
    if (result) {
        state.camerasCommitted = true;
    }
    return result;
}

void rollbackCameras(RuntimeEvaluationState& state, world::World& world) noexcept {
    if (!state.camerasCommitted) {
        return;
    }
    const auto rolledBack = world.withRegistry([&](entt::registry& registry) {
        for (const auto& camera : state.cameras) {
            if (registry.valid(camera.entity) &&
                registry.all_of<render::CameraComponent>(camera.entity)) {
                registry.replace<render::CameraComponent>(camera.entity, camera.previous);
            }
        }
    });
    if (!rolledBack) {
        std::terminate();
    }
    state.camerasCommitted = false;
}

[[nodiscard]] auto prepareAppearances(RuntimeEvaluationState& state) -> core::Result<void> {
    state.appearancesCommitted = false;
    for (auto& appearance : state.appearances) {
        appearance.candidate.visible = appearance.baseline.visible;
        appearance.candidate.materialAssetId = appearance.baseline.materialAssetId;
        appearance.candidate.opacity = appearance.baseline.opacity;
        appearance.candidate.tint = appearance.baseline.tint;
    }
    for (const auto& write : state.writes.writes()) {
        if (write.property != world::PropertyId::RenderVisible &&
            write.property != world::PropertyId::RenderMaterial &&
            write.property != world::PropertyId::MaterialOpacity &&
            write.property != world::PropertyId::MaterialTint) {
            continue;
        }
        const auto appearance = std::lower_bound(
            state.appearances.begin(), state.appearances.end(), write.entity,
            [](const RuntimeEvaluationState::AppearanceEntry& candidate, entt::entity entity) {
                return entt::to_integral(candidate.entity) < entt::to_integral(entity);
            });
        if (appearance == state.appearances.end() || appearance->entity != write.entity) {
            return core::unexpected(core::Error{"runtime.appearance.binding_missing",
                                                "Appearance target has no Renderable component"});
        }
        switch (write.property) {
        case world::PropertyId::RenderVisible: {
            const auto* value = std::get_if<bool>(&write.value);
            if (value == nullptr) {
                return core::unexpected(core::Error{"runtime.appearance.value_invalid",
                                                    "render.visible requires a Boolean value"});
            }
            appearance->candidate.visible = *value;
            break;
        }
        case world::PropertyId::RenderMaterial: {
            const auto* value = std::get_if<std::string_view>(&write.value);
            if (value == nullptr || value->empty()) {
                return core::unexpected(
                    core::Error{"runtime.appearance.value_invalid",
                                "render.material requires a non-empty Material AssetId"});
            }
            appearance->candidate.materialAssetId = *value;
            break;
        }
        case world::PropertyId::MaterialOpacity: {
            const auto* value = std::get_if<double>(&write.value);
            if (value == nullptr || !std::isfinite(*value) || *value < 0.0 || *value > 1.0) {
                return core::unexpected(core::Error{"runtime.appearance.value_invalid",
                                                    "material.opacity must be in [0, 1]"});
            }
            appearance->candidate.opacity = *value;
            break;
        }
        case world::PropertyId::MaterialTint: {
            const auto* value = std::get_if<core::Vec3>(&write.value);
            if (value == nullptr || !core::isFinite(*value) || value->x < 0.0F || value->x > 1.0F ||
                value->y < 0.0F || value->y > 1.0F || value->z < 0.0F || value->z > 1.0F) {
                return core::unexpected(
                    core::Error{"runtime.appearance.value_invalid",
                                "material.tint components must be finite and in [0, 1]"});
            }
            appearance->candidate.tint = *value;
            break;
        }
        default:
            break;
        }
    }
    return {};
}

[[nodiscard]] auto commitAppearances(RuntimeEvaluationState& state, world::World& world)
    -> core::Result<void> {
    auto result = world.withRegistry([&](entt::registry& registry) -> core::Result<void> {
        for (const auto& appearance : state.appearances) {
            if (!registry.valid(appearance.entity) ||
                !registry.all_of<render::AppearanceComponent>(appearance.entity)) {
                return core::unexpected(
                    core::Error{"runtime.appearance.baseline_missing",
                                "A captured AppearanceComponent is unavailable"});
            }
        }
        for (auto& appearance : state.appearances) {
            auto& component = registry.get<render::AppearanceComponent>(appearance.entity);
            appearance.previous = component;
            component = appearance.candidate;
        }
        return {};
    });
    if (result) {
        state.appearancesCommitted = true;
    }
    return result;
}

void rollbackAppearances(RuntimeEvaluationState& state, world::World& world) noexcept {
    if (!state.appearancesCommitted) {
        return;
    }
    const auto rolledBack = world.withRegistry([&](entt::registry& registry) {
        for (const auto& appearance : state.appearances) {
            if (registry.valid(appearance.entity) &&
                registry.all_of<render::AppearanceComponent>(appearance.entity)) {
                registry.replace<render::AppearanceComponent>(appearance.entity,
                                                              appearance.previous);
            }
        }
    });
    if (!rolledBack) {
        std::terminate();
    }
    state.appearancesCommitted = false;
}

[[nodiscard]] auto validateFrame(const RuntimeFrame& frame,
                                 const std::optional<RuntimeFrame>& previous)
    -> core::Result<void> {
    if (!std::isfinite(frame.chartTimeMs)) {
        return core::unexpected(core::Error{"runtime.frame.chart_time_non_finite",
                                            "RuntimeFrame chartTimeMs must be finite"});
    }
    if (!std::isfinite(frame.simulationDeltaTimeMs) || frame.simulationDeltaTimeMs < 0.0) {
        return core::unexpected(core::Error{"runtime.frame.delta_invalid",
                                            "RuntimeFrame delta must be finite and non-negative"});
    }
    if (!previous.has_value()) {
        return {};
    }
    if (frame.timeDiscontinuityId == previous->timeDiscontinuityId &&
        frame.chartTimeMs < previous->chartTimeMs) {
        return core::unexpected(core::Error{"runtime.frame.backward_seek_undeclared",
                                            "Backward chart time requires a new discontinuity ID"});
    }
    if (frame.timeDiscontinuityId != previous->timeDiscontinuityId &&
        frame.simulationDeltaTimeMs != 0.0) {
        return core::unexpected(core::Error{"runtime.frame.discontinuity_delta_nonzero",
                                            "The first discontinuity frame must use zero delta"});
    }
    return {};
}

[[nodiscard]] auto objectIdFor(const ObjectEntityMap& objects, entt::entity entity)
    -> chart::ChartObjectId {
    const auto entry = std::find_if(
        objects.entries().begin(), objects.entries().end(),
        [entity](const ObjectEntityEntry& candidate) { return candidate.entity == entity; });
    return entry == objects.entries().end() ? chart::ChartObjectId{} : entry->objectId;
}

[[nodiscard]] auto baselineFor(const behavior::BehaviorBinding& binding, world::PropertyId property)
    -> world::PropertyValue {
    const auto baseline = std::find_if(binding.baselines.begin(), binding.baselines.end(),
                                       [property](const behavior::PropertyBaseline& candidate) {
                                           return candidate.property == property;
                                       });
    return baseline == binding.baselines.end() ? world::PropertyValue{} : baseline->value;
}

[[nodiscard]] auto owningValue(const world::PropertyWriteValue& value) -> world::PropertyValue {
    return std::visit(
        [](const auto& item) -> world::PropertyValue {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::string_view>) {
                return std::string{item};
            } else {
                return item;
            }
        },
        value);
}

[[nodiscard]] auto resolvedValue(const RuntimeEvaluationState& state, entt::entity entity,
                                 world::PropertyId property)
    -> std::optional<world::PropertyValue> {
    if (const auto transform = state.transformResolver.resolvedValue(entity, property)) {
        return transform;
    }
    if (property == world::PropertyId::CameraFovY) {
        const auto camera = std::lower_bound(
            state.cameras.begin(), state.cameras.end(), entity,
            [](const RuntimeEvaluationState::CameraEntry& candidate, entt::entity target) {
                return entt::to_integral(candidate.entity) < entt::to_integral(target);
            });
        if (camera != state.cameras.end() && camera->entity == entity) {
            return world::PropertyValue{camera->candidateFovY};
        }
        return std::nullopt;
    }
    const auto appearance = std::lower_bound(
        state.appearances.begin(), state.appearances.end(), entity,
        [](const RuntimeEvaluationState::AppearanceEntry& candidate, entt::entity target) {
            return entt::to_integral(candidate.entity) < entt::to_integral(target);
        });
    if (appearance == state.appearances.end() || appearance->entity != entity) {
        return std::nullopt;
    }
    switch (property) {
    case world::PropertyId::RenderVisible:
        return world::PropertyValue{appearance->candidate.visible};
    case world::PropertyId::RenderMaterial:
        return world::PropertyValue{appearance->candidate.materialAssetId};
    case world::PropertyId::MaterialOpacity:
        return world::PropertyValue{appearance->candidate.opacity};
    case world::PropertyId::MaterialTint:
        return world::PropertyValue{appearance->candidate.tint};
    case world::PropertyId::TransformPositionX:
    case world::PropertyId::TransformPositionY:
    case world::PropertyId::TransformPositionZ:
    case world::PropertyId::TransformRotation:
    case world::PropertyId::TransformScale:
    case world::PropertyId::CameraFovY:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

PreparedRuntimeSession::PreparedRuntimeSession(const RuntimeSession& owner,
                                               std::uint64_t ownerToken, std::uint64_t managerToken,
                                               chart::ChartRuntime chartRuntime,
                                               ObjectEntityMap objects,
                                               core::Diagnostics diagnostics,
                                               std::unique_ptr<RuntimeEvaluationState> evaluation,
                                               std::optional<assets::ResourceScope> resourceScope,
                                               std::unique_ptr<world::World> world) noexcept
    : owner_(&owner), ownerToken_(ownerToken), managerToken_(managerToken),
      chartRuntime_(std::move(chartRuntime)), objects_(std::move(objects)),
      diagnostics_(std::move(diagnostics)), evaluation_(std::move(evaluation)),
      resourceScope_(std::move(resourceScope)), world_(std::move(world)) {}

PreparedRuntimeSession::~PreparedRuntimeSession() = default;

PreparedRuntimeSession::PreparedRuntimeSession(PreparedRuntimeSession&& other) noexcept
    : owner_(other.owner_), ownerToken_(other.ownerToken_), managerToken_(other.managerToken_),
      chartRuntime_(std::move(other.chartRuntime_)), objects_(std::move(other.objects_)),
      diagnostics_(std::move(other.diagnostics_)), evaluation_(std::move(other.evaluation_)),
      resourceScope_(std::move(other.resourceScope_)), world_(std::move(other.world_)) {
    other.owner_ = nullptr;
    other.ownerToken_ = 0;
    other.managerToken_ = 0;
}

auto PreparedRuntimeSession::operator=(PreparedRuntimeSession&& other) noexcept
    -> PreparedRuntimeSession& {
    if (this == &other) {
        return *this;
    }
    world_.reset();
    resourceScope_.reset();
    evaluation_.reset();
    owner_ = other.owner_;
    ownerToken_ = other.ownerToken_;
    managerToken_ = other.managerToken_;
    chartRuntime_ = std::move(other.chartRuntime_);
    objects_ = std::move(other.objects_);
    diagnostics_ = std::move(other.diagnostics_);
    evaluation_ = std::move(other.evaluation_);
    resourceScope_ = std::move(other.resourceScope_);
    world_ = std::move(other.world_);
    other.owner_ = nullptr;
    other.ownerToken_ = 0;
    other.managerToken_ = 0;
    return *this;
}

RuntimeSession::RuntimeSession() noexcept : sessionToken_(allocateSessionToken()) {}

RuntimeSession::RuntimeSession(assets::ResourceManager& resourceManager) noexcept
    : RuntimeSession() {
    resourceManager_ = &resourceManager;
    managerToken_ = resourceManager.managerToken();
}

RuntimeSession::~RuntimeSession() {
    threadChecker_.assertCurrent();
}

auto RuntimeSession::prepare(chart::ChartRuntime chartRuntime) const
    -> PreparedRuntimeSessionResult {
    PreparedRuntimeSessionResult result;
    if (!threadChecker_.isCurrent()) {
        addSessionError(result.diagnostics, "runtime.session.not_owner_thread",
                        "RuntimeSession preparation must run on its owner thread");
        return result;
    }

    result.diagnostics.append(ChartWorldInstantiator::validate(chartRuntime));
    if (result.diagnostics.hasErrors()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    std::optional<assets::ResourceScope> resourceScope;
    std::vector<std::optional<ResolvedRenderableResources>> renderableResources;
    if (resourceManager_ != nullptr) {
        resourceScope.emplace(*resourceManager_);
        renderableResources.resize(chartRuntime.objects.size());
        for (std::size_t index = 0; index < chartRuntime.objects.size(); ++index) {
            const auto& renderable = chartRuntime.objects[index].components.renderable;
            if (!renderable.has_value()) {
                continue;
            }
            auto mesh = resourceScope->requestMesh(assets::AssetId{renderable->mesh.value},
                                                   assets::ResourcePolicy::Fallback);
            result.diagnostics.append(std::move(mesh.diagnostics));
            if (result.diagnostics.hasErrors() || result.diagnostics.limitReached()) {
                break;
            }
            auto material = resourceScope->requestMaterial(
                assets::AssetId{renderable->material.value}, assets::ResourcePolicy::Fallback);
            result.diagnostics.append(std::move(material.diagnostics));
            if (result.diagnostics.hasErrors() || result.diagnostics.limitReached()) {
                break;
            }
            if (mesh.hasValue() && material.hasValue()) {
                renderableResources[index] = ResolvedRenderableResources{
                    .mesh = *mesh.handle,
                    .material = *material.handle,
                };
            }
            const auto& behaviorReference = chartRuntime.objects[index].components.behavior;
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
                    const auto* materialId = std::get_if<chart::AssetId>(&event.value);
                    if (materialId == nullptr) {
                        continue;
                    }
                    auto dynamicMaterial = resourceScope->requestMaterial(
                        assets::AssetId{materialId->value}, assets::ResourcePolicy::Required);
                    result.diagnostics.append(std::move(dynamicMaterial.diagnostics));
                    if (!dynamicMaterial.hasValue() || result.diagnostics.hasErrors() ||
                        result.diagnostics.limitReached()) {
                        break;
                    }
                }
                if (result.diagnostics.hasErrors() || result.diagnostics.limitReached()) {
                    break;
                }
            }
        }
        if (result.diagnostics.hasErrors()) {
            result.diagnostics.sortDeterministically();
            return result;
        }
    }

    auto instantiated =
        resourceManager_ == nullptr
            ? ChartWorldInstantiator::instantiate(chartRuntime)
            : ChartWorldInstantiator::instantiate(chartRuntime, renderableResources, managerToken_);
    result.diagnostics.append(std::move(instantiated.diagnostics));
    if (!instantiated.hasValue()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    auto evaluation =
        buildEvaluationState(chartRuntime, instantiated.value->objects, *instantiated.value->world);
    if (!evaluation) {
        addSessionError(result.diagnostics, evaluation.error());
        result.diagnostics.sortDeterministically();
        return result;
    }

    result.diagnostics.sortDeterministically();
    auto world = std::move(instantiated.value->world);
    auto objects = std::move(instantiated.value->objects);
    result.prepared.emplace(PreparedRuntimeSession{
        *this, sessionToken_, managerToken_, std::move(chartRuntime), std::move(objects),
        result.diagnostics, std::move(*evaluation), std::move(resourceScope), std::move(world)});
    return result;
}

auto RuntimeSession::commit(PreparedRuntimeSession&& prepared) -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession commit must run on its owner thread"});
    }
    if (world_) {
        return core::unexpected(core::Error{"runtime.session.already_committed",
                                            "Use reload to replace an active RuntimeSession"});
    }
    if (!prepared.world_ || !prepared.evaluation_) {
        return core::unexpected(core::Error{"runtime.session.prepared_unavailable",
                                            "PreparedRuntimeSession has already been consumed"});
    }
    if (prepared.owner_ != this || prepared.ownerToken_ != sessionToken_) {
        return core::unexpected(
            core::Error{"runtime.session.prepared_owner_mismatch",
                        "PreparedRuntimeSession belongs to a different RuntimeSession"});
    }
    if (prepared.managerToken_ != managerToken_ ||
        (prepared.resourceScope_.has_value() &&
         prepared.resourceScope_->managerToken() != managerToken_)) {
        return core::unexpected(
            core::Error{"runtime.session.prepared_manager_mismatch",
                        "PreparedRuntimeSession belongs to a different ResourceManager"});
    }

    chartRuntime_.emplace(std::move(prepared.chartRuntime_));
    objects_ = std::move(prepared.objects_);
    activeDiagnostics_ = std::move(prepared.diagnostics_);
    evaluation_ = std::move(prepared.evaluation_);
    resourceScope_ = std::move(prepared.resourceScope_);
    world_ = std::move(prepared.world_);
    lastFrame_.reset();
    return {};
}

auto RuntimeSession::updatePrepared(RuntimeEvaluationState& state,
                                    const chart::TimingMap& timingMap, const RuntimeFrame& frame)
    -> core::Result<void> {
    auto beatSample = timingMap.sampleChartTimeMs(frame.chartTimeMs);
    if (!beatSample) {
        return core::unexpected(std::move(beatSample.error()));
    }
    auto evaluated = behavior::BehaviorSystem::evaluate(
        state.program,
        behavior::BehaviorSample{frame.chartTimeMs, beatSample->beat, beatSample->inStop,
                                 beatSample->stopProgress},
        state.writes);
    if (!evaluated) {
        return core::unexpected(std::move(evaluated.error()));
    }
    auto camerasPrepared = prepareCameras(state);
    if (!camerasPrepared) {
        return core::unexpected(std::move(camerasPrepared.error()));
    }
    auto transformsPrepared = state.transformResolver.prepare(state.writes.writes());
    if (!transformsPrepared) {
        return core::unexpected(std::move(transformsPrepared.error()));
    }
    auto appearancesPrepared = prepareAppearances(state);
    if (!appearancesPrepared) {
        return core::unexpected(std::move(appearancesPrepared.error()));
    }
    auto transformsCommitted = state.transformResolver.commit(*world_);
    if (!transformsCommitted) {
        return core::unexpected(std::move(transformsCommitted.error()));
    }
    auto camerasCommitted = commitCameras(state, *world_);
    if (!camerasCommitted) {
        state.transformResolver.rollback(*world_);
        return core::unexpected(std::move(camerasCommitted.error()));
    }
    auto appearancesCommitted = commitAppearances(state, *world_);
    if (!appearancesCommitted) {
        rollbackCameras(state, *world_);
        state.transformResolver.rollback(*world_);
        return core::unexpected(std::move(appearancesCommitted.error()));
    }
    auto transformsUpdated = world::updateWorldTransforms(*world_);
    if (!transformsUpdated) {
        rollbackAppearances(state, *world_);
        rollbackCameras(state, *world_);
        state.transformResolver.rollback(*world_);
        return core::unexpected(std::move(transformsUpdated.error()));
    }
    return {};
}

void RuntimeSession::captureDebug(const RuntimeEvaluationState& state, double beatValue) {
    debugRecords_.clear();
    debugTruncated_ = false;
    if (!debugOptions_.enabled) {
        return;
    }

    std::size_t writeIndex = 0;
    const auto writes = state.writes.writes();
    const auto append = [&](const behavior::BehaviorBinding& binding, world::PropertyId property,
                            std::optional<std::size_t> eventIndex, double progress,
                            const world::PropertyWriteValue& output) {
        if (debugRecords_.size() >= debugOptions_.capacity) {
            debugTruncated_ = true;
            return;
        }
        const auto initial = baselineFor(binding, property);
        auto behaviorValue = owningValue(output);
        auto finalValue = resolvedValue(state, binding.entity, property);
        debugRecords_.push_back(RuntimeDebugRecord{
            .objectId = objectIdFor(objects_, binding.entity),
            .property = property,
            .initialValue = initial,
            .eventIndex = eventIndex,
            .normalizedProgress = progress,
            .behaviorValue = behaviorValue,
            .finalValue = finalValue ? std::move(*finalValue) : std::move(behaviorValue),
        });
    };

    for (const auto& binding : state.program.bindings) {
        const auto& definition = state.program.definitions[binding.behavior.value];
        writeIndex += definition.tracks.size();
        for (const auto& track : definition.eventTracks) {
            if (writeIndex >= writes.size()) {
                debugTruncated_ = true;
                return;
            }
            std::optional<std::size_t> eventIndex;
            double progress = 0.0;
            if (!track.events.empty() && beatValue >= track.events.front().startBeat) {
                const auto next =
                    std::upper_bound(track.events.begin(), track.events.end(), beatValue,
                                     [](double value, const behavior::BehaviorEvent& event) {
                                         return value < event.startBeat;
                                     });
                const auto event = next - 1;
                eventIndex = static_cast<std::size_t>(event - track.events.begin());
                if (event->instantaneous || beatValue >= event->endBeat) {
                    progress = 1.0;
                } else {
                    progress = (beatValue - event->startBeat) / (event->endBeat - event->startBeat);
                }
            }
            append(binding, track.property, eventIndex, progress, writes[writeIndex].value);
            ++writeIndex;
        }
        for (const auto& track : definition.stepTracks) {
            if (writeIndex >= writes.size()) {
                debugTruncated_ = true;
                return;
            }
            std::optional<std::size_t> eventIndex;
            if (!track.events.empty() && beatValue >= track.events.front().beat) {
                const auto next =
                    std::upper_bound(track.events.begin(), track.events.end(), beatValue,
                                     [](double value, const behavior::BehaviorStepEvent& event) {
                                         return value < event.beat;
                                     });
                eventIndex = static_cast<std::size_t>((next - 1) - track.events.begin());
            }
            append(binding, track.property, eventIndex, eventIndex ? 1.0 : 0.0,
                   writes[writeIndex].value);
            ++writeIndex;
        }
    }
}

auto RuntimeSession::configureDebug(RuntimeDebugOptions options) -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(
            core::Error{"runtime.session.not_owner_thread",
                        "RuntimeSession debug configuration must run on its owner thread"});
    }
    if (options.enabled && (options.capacity == 0 || options.capacity > maxRuntimeDebugRecords)) {
        return core::unexpected(
            core::Error{"runtime.debug.capacity_invalid",
                        "Runtime debug capacity is outside the supported range"}
                .withContext("maximum", std::to_string(maxRuntimeDebugRecords)));
    }
    try {
        if (options.enabled && debugRecords_.capacity() < options.capacity) {
            debugRecords_.reserve(options.capacity);
        }
    } catch (const std::bad_alloc&) {
        return core::unexpected(core::Error{"runtime.debug.allocation_failed",
                                            "Runtime debug storage could not be allocated"});
    } catch (...) {
        return core::unexpected(core::Error{"runtime.debug.configuration_failed",
                                            "Runtime debug configuration failed"});
    }
    debugOptions_ = options;
    debugRecords_.clear();
    debugTruncated_ = false;
    return {};
}

auto RuntimeSession::debugSnapshot() const -> core::Result<RuntimeDebugSnapshot> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(
            core::Error{"runtime.session.not_owner_thread",
                        "RuntimeSession debug snapshot must run on its owner thread"});
    }
    try {
        return RuntimeDebugSnapshot{.records = debugRecords_, .truncated = debugTruncated_};
    } catch (const std::bad_alloc&) {
        return core::unexpected(core::Error{"runtime.debug.snapshot_allocation_failed",
                                            "Runtime debug snapshot could not be copied"});
    } catch (...) {
        return core::unexpected(
            core::Error{"runtime.debug.snapshot_failed", "Runtime debug snapshot failed"});
    }
}

auto RuntimeSession::update(const RuntimeFrame& frame) -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession update must run on its owner thread"});
    }
    if (!world_ || !evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "Cannot update an empty RuntimeSession"});
    }
    auto valid = validateFrame(frame, lastFrame_);
    if (!valid) {
        return core::unexpected(std::move(valid.error()));
    }
    auto updated = updatePrepared(*evaluation_, chartRuntime_->timingMap, frame);
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
    }
    if (debugOptions_.enabled) {
        const auto beatSample = chartRuntime_->timingMap.sampleChartTimeMs(frame.chartTimeMs);
        if (!beatSample) {
            return core::unexpected(std::move(beatSample.error()));
        }
        captureDebug(*evaluation_, beatSample->beat);
    } else {
        debugRecords_.clear();
        debugTruncated_ = false;
    }
    lastFrame_ = frame;
    return {};
}

auto RuntimeSession::reload(chart::ChartRuntime replacement) -> RuntimeSessionReloadResult {
    const RuntimeFrame target = lastFrame_.value_or(RuntimeFrame{});
    return reload(std::move(replacement), target, ReloadPolicy::KeepChartTime);
}

auto RuntimeSession::reload(chart::ChartRuntime replacement, const RuntimeFrame& targetFrame,
                            ReloadPolicy policy) -> RuntimeSessionReloadResult {
    RuntimeSessionReloadResult result;
    if (!threadChecker_.isCurrent()) {
        addSessionError(result.diagnostics, "runtime.session.not_owner_thread",
                        "RuntimeSession reload must run on its owner thread");
        return result;
    }
    if (!world_ || !evaluation_) {
        addSessionError(result.diagnostics, "runtime.session.empty",
                        "Cannot reload an empty RuntimeSession");
        return result;
    }

    RuntimeFrame reloadFrame = targetFrame;
    reloadFrame.simulationDeltaTimeMs = 0.0;
    if (policy == ReloadPolicy::RestartAtZero) {
        reloadFrame.chartTimeMs = 0.0;
    }
    auto frameValidation = validateFrame(reloadFrame, std::nullopt);
    if (!frameValidation) {
        addSessionError(result.diagnostics, frameValidation.error());
        return result;
    }

    auto preparedResult = prepare(std::move(replacement));
    result.diagnostics = std::move(preparedResult.diagnostics);
    if (!preparedResult.hasValue()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    auto previousWorld = std::move(world_);
    world_ = std::move(preparedResult.prepared->world_);
    auto sampled = updatePrepared(*preparedResult.prepared->evaluation_,
                                  preparedResult.prepared->chartRuntime_.timingMap, reloadFrame);
    preparedResult.prepared->world_ = std::move(world_);
    world_ = std::move(previousWorld);
    if (!sampled) {
        addSessionError(result.diagnostics, sampled.error());
        result.diagnostics.sortDeterministically();
        return result;
    }

    replaceWith(std::move(*preparedResult.prepared));
    lastFrame_ = reloadFrame;
    if (debugOptions_.enabled) {
        const auto beatSample = chartRuntime_->timingMap.sampleChartTimeMs(reloadFrame.chartTimeMs);
        if (!beatSample) {
            addSessionError(result.diagnostics, beatSample.error());
            return result;
        }
        captureDebug(*evaluation_, beatSample->beat);
    }
    result.reloaded = true;
    return result;
}

auto RuntimeSession::unload() -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession unload must run on its owner thread"});
    }
    evaluation_.reset();
    world_.reset();
    objects_.clear();
    chartRuntime_.reset();
    activeDiagnostics_.clear();
    resourceScope_.reset();
    lastFrame_.reset();
    debugRecords_.clear();
    debugTruncated_ = false;
    return {};
}

auto RuntimeSession::empty() const noexcept -> bool {
    threadChecker_.assertCurrent();
    return !world_;
}

auto RuntimeSession::objectCount() const noexcept -> std::size_t {
    threadChecker_.assertCurrent();
    return objects_.size();
}

auto RuntimeSession::resourceCount() const noexcept -> std::size_t {
    threadChecker_.assertCurrent();
    return resourceScope_.has_value() ? resourceScope_->size() : 0;
}

auto RuntimeSession::activeDiagnostics() const noexcept -> const core::Diagnostics& {
    threadChecker_.assertCurrent();
    return activeDiagnostics_;
}

auto RuntimeSession::findEntity(const chart::ChartObjectId& objectId) const
    -> core::Result<std::optional<entt::entity>> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (!world_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    return objects_.find(objectId);
}

void RuntimeSession::replaceWith(PreparedRuntimeSession&& prepared) noexcept {
    auto previousRuntime = std::move(chartRuntime_);
    auto previousObjects = std::move(objects_);
    auto previousDiagnostics = std::move(activeDiagnostics_);
    auto previousEvaluation = std::move(evaluation_);
    auto previousScope = std::move(resourceScope_);
    auto previousWorld = std::move(world_);

    chartRuntime_.emplace(std::move(prepared.chartRuntime_));
    objects_ = std::move(prepared.objects_);
    activeDiagnostics_ = std::move(prepared.diagnostics_);
    evaluation_ = std::move(prepared.evaluation_);
    resourceScope_ = std::move(prepared.resourceScope_);
    world_ = std::move(prepared.world_);

    previousEvaluation.reset();
    previousWorld.reset();
    previousScope.reset();
    previousObjects.clear();
    previousDiagnostics.clear();
    previousRuntime.reset();
}

} // namespace cuexis::runtime
