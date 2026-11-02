#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/runtime/transaction_access.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/presentation_metrics.hpp>

#include "runtime_session_units_internal.hpp"

#include <cuexis/animation/animation_program.hpp>
#include <cuexis/animation/animation_system.hpp>
#include <cuexis/behavior/behavior_program.hpp>
#include <cuexis/behavior/behavior_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/world/property.hpp>
#include <cuexis/world/resolver_access.hpp>
#include <cuexis/world/transform_system.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::runtime {

auto detail::RuntimeTestAccess::state(const RuntimeSession& session) -> RuntimeTransactionState {
    RuntimeTransactionState result{session.lastFrame_, session.baseRevision(), {}};
    if (session.evaluation_) {
        const auto append = [&](const auto& tokens) {
            result.tokenState.push_back(tokens.size());
            for (const auto& token : tokens)
                result.tokenState.insert(result.tokenState.end(),
                                         {token.id.value,
                                          static_cast<std::uint64_t>(token.lifetime.kind),
                                          token.lifetime.remainingFrames});
        };
        append(session.evaluation_->hostOverrides);
        append(session.evaluation_->previewOverrides);
    }
    return result;
}

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

[[nodiscard]] auto toPropertyValue(const chart::BehaviorValue& value) -> world::PropertyValue {
    return std::visit(
        [](const auto& item) -> world::PropertyValue { return world::PropertyValue{item}; }, value);
}

[[nodiscard]] auto toPropertyValue(const chart::BehaviorStepValue& value) -> world::PropertyValue {
    if (const auto* visible = std::get_if<bool>(&value)) {
        return world::PropertyValue{std::in_place_type<bool>, *visible};
    }
    const auto& material = std::get<chart::AssetId>(value);
    return world::PropertyValue{std::in_place_type<std::string>, material.value};
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

[[nodiscard]] auto overrideIsActive(const world::OverrideToken& token,
                                    const RuntimeFrame& frame) noexcept -> bool {
    switch (token.lifetime.kind) {
    case world::OverrideLifetimeKind::UntilReleased:
        return true;
    case world::OverrideLifetimeKind::RemainingFrames:
        return token.lifetime.remainingFrames > 0;
    case world::OverrideLifetimeKind::UntilChartTimeMs:
        return frame.chartTimeMs < token.lifetime.untilChartTimeMs;
    }
    return false;
}

void tickOverrideLifetimes(std::vector<world::OverrideToken>& tokens) noexcept {
    for (auto& token : tokens) {
        if (token.lifetime.kind == world::OverrideLifetimeKind::RemainingFrames &&
            token.lifetime.remainingFrames > 0) {
            --token.lifetime.remainingFrames;
        }
    }
}

[[nodiscard]] auto rebuildAnimationBaselines(RuntimeEvaluationState& state) -> core::Result<void> {
    state.animationBaselines.clear();
    state.animationBaselines.reserve(state.animationBindings.size());
    for (const auto& binding : state.animationBindings) {
        animation::AnimationObjectBaseline baseline{.objectId = binding.objectId, .properties = {}};
        for (const auto property : allPropertyIds) {
            auto value = state.resolver.resolvedValue(binding.entity, property);
            if (!value) {
                continue;
            }
            baseline.properties.push_back(animation::AnimationPropertyBaseline{
                .property = property, .value = std::move(*value)});
        }
        state.animationBaselines.push_back(std::move(baseline));
    }
    return {};
}

[[nodiscard]] auto applyResolvedPresentation(RuntimeEvaluationState& state) -> core::Result<void> {
    for (auto& camera : state.cameras) {
        camera.candidateFovY = camera.baselineFovY;
        const auto value =
            state.resolver.resolvedValue(camera.entity, world::PropertyId::CameraFovY);
        if (!value) {
            continue;
        }
        const auto* fov = std::get_if<double>(&*value);
        if (fov == nullptr) {
            return core::unexpected(core::Error{"runtime.camera.fov_out_of_range",
                                                "camera.fovY must be between 0 and 179 degrees"});
        }
        camera.candidateFovY = *fov;
    }
    for (auto& appearance : state.appearances) {
        appearance.candidate.visible = appearance.baseline.visible;
        appearance.candidate.opacity = appearance.baseline.opacity;
        appearance.candidate.tint = appearance.baseline.tint;
        appearance.candidate.materialAssetId.assign(appearance.baseline.materialAssetId);
        const auto [visible, material, opacity, tint] =
            world::detail::ResolverAccess::resolvedAppearance(state.resolver, appearance.entity);
        if (visible) {
            if (const auto* value = std::get_if<bool>(visible)) {
                appearance.candidate.visible = *value;
            }
        }
        if (material) {
            if (const auto* value = std::get_if<std::string>(material)) {
                appearance.candidate.materialAssetId.assign(value->data(), value->size());
            }
        }
        if (opacity) {
            if (const auto* value = std::get_if<double>(opacity)) {
                appearance.candidate.opacity = *value;
            }
        }
        if (tint) {
            if (const auto* value = std::get_if<core::Vec3>(tint)) {
                appearance.candidate.tint = *value;
            }
        }
    }
    return {};
}

[[nodiscard]] auto registerPresentationBaselines(RuntimeEvaluationState& state)
    -> core::Result<void> {
    for (const auto& camera : state.cameras) {
        auto registered =
            state.resolver.registerBaseline(camera.entity, world::PropertyId::CameraFovY,
                                            world::PropertyValue{camera.baselineFovY});
        if (!registered) {
            return registered;
        }
    }
    for (const auto& appearance : state.appearances) {
        auto visible =
            state.resolver.registerBaseline(appearance.entity, world::PropertyId::RenderVisible,
                                            world::PropertyValue{appearance.baseline.visible});
        if (!visible) {
            return visible;
        }
        auto material = state.resolver.registerBaseline(
            appearance.entity, world::PropertyId::RenderMaterial,
            world::PropertyValue{appearance.baseline.materialAssetId});
        if (!material) {
            return material;
        }
        auto opacity =
            state.resolver.registerBaseline(appearance.entity, world::PropertyId::MaterialOpacity,
                                            world::PropertyValue{appearance.baseline.opacity});
        if (!opacity) {
            return opacity;
        }
        auto tint =
            state.resolver.registerBaseline(appearance.entity, world::PropertyId::MaterialTint,
                                            world::PropertyValue{appearance.baseline.tint});
        if (!tint) {
            return tint;
        }
    }
    return {};
}

[[nodiscard]] auto attachAnimation(RuntimeEvaluationState& state, const ObjectEntityMap& objects,
                                   animation::AnimationProgram animation) -> core::Result<void> {
    if (animation.empty()) {
        return {};
    }
    state.animationBindings.reserve(animation.objectCount());
    for (const auto& object : animation.objects()) {
        const auto entity = objects.find(object.objectId);
        if (!entity.has_value() || *entity == entt::null) {
            return core::unexpected(core::Error{"runtime.animation.binding_missing",
                                                "Animation object has no bound entity"}
                                        .withContext("object_id", object.objectId.value));
        }
        state.animationBindings.push_back(
            animation::AnimationObjectBinding{.objectId = object.objectId, .entity = *entity});
    }
    state.animation = std::move(animation);
    return {};
}

[[nodiscard]] auto buildEvaluationState(chart::ChartRuntime& runtime,
                                        const ObjectEntityMap& objects, world::World& world,
                                        animation::AnimationProgram animation)
    -> core::Result<std::unique_ptr<RuntimeEvaluationState>> {
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
    // Product tag: candidate evaluation state is owned by the session, not by the caller thread.
    const internal::pd::memory::TagScope tag{internal::pd::memory::ownerMain,
                                             internal::pd::memory::domainCandidate};
#endif
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

    std::size_t animationWriteBudget = 0;
    if (!animation.empty()) {
        if (!world::animationWriteBudgetFits(animation.objectCount(),
                                             world::maxPropertyWritesPerFrame)) {
            return core::unexpected(core::Error{"runtime.animation.write_limit",
                                                "Animation program exceeds the write budget"});
        }
        animationWriteBudget = *world::requiredAnimationWrites(animation.objectCount());
    }

    auto state = std::make_unique<RuntimeEvaluationState>(requiredWrites, animationWriteBudget);
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
                    .startValue = toPropertyValue(runtimeEvent.startValue),
                    .endValue = toPropertyValue(runtimeEvent.endValue),
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
                    .value = toPropertyValue(runtimeEvent.value),
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

    auto resolver = world::PropertyResolver::capture(world);
    if (!resolver) {
        return core::unexpected(std::move(resolver.error()));
    }
    state->resolver = std::move(*resolver);

    auto cameras = world.withRegistry([&](const entt::registry& registry) {
        const auto cameraView = registry.view<const render::CameraComponent>();
        for (const entt::entity entity : cameraView) {
            const auto& camera = cameraView.get<const render::CameraComponent>(entity);
            state->cameras.push_back(RuntimeEvaluationState::CameraEntry{
                .entity = entity,
                .baselineFovY = camera.fovY,
                .candidateFovY = camera.fovY,
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
            };
            entry.candidate.materialAssetId.reserve(materialSize);
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
    auto registered = registerPresentationBaselines(*state);
    if (!registered) {
        return core::unexpected(std::move(registered.error()));
    }
    std::size_t maxMaterialBytes = 0;
    for (const auto& appearance : state->appearances) {
        maxMaterialBytes =
            std::max({maxMaterialBytes, appearance.baseline.materialAssetId.capacity(),
                      appearance.candidate.materialAssetId.capacity()});
        state->resolver.reserveStringCapacity(appearance.entity, world::PropertyId::RenderMaterial,
                                              appearance.candidate.materialAssetId.capacity());
    }
    auto attached = attachAnimation(*state, objects, std::move(animation));
    if (!attached) {
        return core::unexpected(std::move(attached.error()));
    }
    const auto ownedStringSlots =
        std::max({state->appearances.size(), state->animationBindings.size(), std::size_t{1}});
    state->writes.reserveOwnedStrings(ownedStringSlots, maxMaterialBytes);
    state->animationWrites.reserveOwnedStrings(ownedStringSlots, maxMaterialBytes);
    return state;
}

// All component existence checks and owning string preparation precede installation.
auto preparePresentationSlots(RuntimeEvaluationState& state, world::detail::TransformUpdate& plan)
    -> core::Result<void> {
    for (auto& camera : state.cameras) {
        camera.slot = plan.resolve<render::CameraComponent>(camera.entity);
        if (!camera.slot)
            return core::unexpected(core::Error{"runtime.camera.baseline_missing",
                                                "A captured CameraComponent is unavailable"});
    }
    for (auto& appearance : state.appearances) {
        appearance.slot = plan.resolve<render::AppearanceComponent>(appearance.entity);
        if (!appearance.slot)
            return core::unexpected(core::Error{"runtime.appearance.baseline_missing",
                                                "A captured AppearanceComponent is unavailable"});
    }
    return {};
}
void installPresentation(RuntimeEvaluationState& state) noexcept {
    for (auto& camera : state.cameras) {
        camera.slot->fovY = camera.candidateFovY;
        camera.slot = nullptr;
    }
    for (auto& appearance : state.appearances) {
        appearance.slot->visible = appearance.candidate.visible;
        appearance.slot->opacity = appearance.candidate.opacity;
        appearance.slot->tint = appearance.candidate.tint;
        appearance.slot->materialAssetId.swap(appearance.candidate.materialAssetId);
        appearance.slot = nullptr;
    }
}

void installRuntimeState(RuntimeEvaluationState& state, world::detail::TransformUpdate& plan,
                         const RuntimeFrame& frame, bool tickLifetimes) noexcept {
    using internal::pd::memory::structure;
    structure("candidate.runtime", 1, 1);
    structure("candidate.cameras", state.cameras.size(), state.cameras.capacity());
    structure("candidate.appearances", state.appearances.size(), state.appearances.capacity());
    structure("candidate.animation-bindings", state.animationBindings.size(),
              state.animationBindings.capacity());
    structure("candidate.animation-baselines", state.animationBaselines.size(),
              state.animationBaselines.capacity());
    structure("candidate.animation-contributions", state.animationLayerContributions.size(),
              state.animationLayerContributions.capacity());
    structure("candidate.host-overrides", state.hostOverrides.size(),
              state.hostOverrides.capacity());
    structure("candidate.preview-overrides", state.previewOverrides.size(),
              state.previewOverrides.capacity());
    structure("candidate.gameplay-writes", state.gameplayWrites.size(),
              state.gameplayWrites.capacity());
    structure("scratch.runtime-transform-writes", state.transformWrites.size(),
              state.transformWrites.capacity());
    static_assert(std::is_nothrow_move_assignable_v<world::OverrideToken>);
    plan.apply();
    installPresentation(state);
    world::detail::ResolverAccess::installed(state.resolver);
    if (tickLifetimes) {
        tickOverrideLifetimes(state.hostOverrides);
        tickOverrideLifetimes(state.previewOverrides);
    }
    const auto pruneInactive = [&](std::vector<world::OverrideToken>& tokens) {
        tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
                                    [&](const world::OverrideToken& token) {
                                        return !overrideIsActive(token, frame);
                                    }),
                     tokens.end());
    };
    pruneInactive(state.hostOverrides);
    pruneInactive(state.previewOverrides);

    plan.finish();
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

[[nodiscard]] auto identityLabel(const chart::AnimationRecordIdentity& identity) -> std::string {
    if (const auto* local = std::get_if<std::string>(&identity)) {
        return *local;
    }
    const auto& generated = std::get<chart::GeneratedAnimationIdentity>(identity);
    std::string label;
    label.reserve(generated.objectId.size() + generated.bindingId.size() +
                  generated.templateId.size() + 2);
    label.append(generated.objectId);
    label.push_back('/');
    label.append(generated.bindingId);
    label.push_back('/');
    label.append(generated.templateId);
    return label;
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
    return state.resolver.resolvedValue(entity, property);
}

} // namespace cuexis::runtime
