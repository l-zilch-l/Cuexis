// RuntimeSession transactional lifecycle and absolute-time Behavior evaluation.

#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/runtime/transaction_access.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <cuexis_internal/allocation_observer.hpp>

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

class RuntimeEvaluationState final {
  public:
    explicit RuntimeEvaluationState(std::size_t requiredWrites, std::size_t animationWrites)
        : writes(requiredWrites), animationWrites(animationWrites) {}

    struct CameraEntry final {
        entt::entity entity{entt::null};
        double baselineFovY{60.0};
        double candidateFovY{60.0};
        render::CameraComponent* slot{};
    };

    struct AppearanceEntry final {
        entt::entity entity{entt::null};
        render::AppearanceComponent baseline;
        render::AppearanceComponent candidate;
        render::AppearanceComponent* slot{};
    };

    behavior::BehaviorProgram program;
    world::PropertyWriteBuffer writes;
    world::PropertyWriteBuffer animationWrites;
    world::PropertyResolver resolver;
    std::optional<animation::AnimationProgram> animation;
    std::vector<animation::AnimationObjectBinding> animationBindings;
    std::vector<animation::AnimationObjectBaseline> animationBaselines;
    std::vector<animation::AnimationLayerContribution> animationLayerContributions;
    std::vector<world::OverrideToken> hostOverrides;
    std::vector<world::OverrideToken> previewOverrides;
    std::vector<world::PropertyWrite> gameplayWrites;
    std::uint64_t nextOverrideId{1};
    std::vector<CameraEntry> cameras;
    std::vector<AppearanceEntry> appearances;
    std::vector<world::detail::TransformWrite> transformWrites;
};

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

constexpr std::array<world::PropertyId, world::propertyCount> allPropertyIds{
    world::PropertyId::TransformPositionX, world::PropertyId::TransformPositionY,
    world::PropertyId::TransformPositionZ, world::PropertyId::TransformRotation,
    world::PropertyId::TransformScale,     world::PropertyId::CameraFovY,
    world::PropertyId::RenderVisible,      world::PropertyId::RenderMaterial,
    world::PropertyId::MaterialOpacity,    world::PropertyId::MaterialTint,
};

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
        if (const auto* visible = state.resolver.resolvedValuePtr(
                appearance.entity, world::PropertyId::RenderVisible)) {
            if (const auto* value = std::get_if<bool>(visible)) {
                appearance.candidate.visible = *value;
            }
        }
        if (const auto* material = state.resolver.resolvedValuePtr(
                appearance.entity, world::PropertyId::RenderMaterial)) {
            if (const auto* value = std::get_if<std::string>(material)) {
                appearance.candidate.materialAssetId.assign(value->data(), value->size());
            }
        }
        if (const auto* opacity = state.resolver.resolvedValuePtr(
                appearance.entity, world::PropertyId::MaterialOpacity)) {
            if (const auto* value = std::get_if<double>(opacity)) {
                appearance.candidate.opacity = *value;
            }
        }
        if (const auto* tint = state.resolver.resolvedValuePtr(appearance.entity,
                                                               world::PropertyId::MaterialTint)) {
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
    structure("candidate.animation-bindings", state.animationBindings.size(), state.animationBindings.capacity());
    structure("candidate.animation-baselines", state.animationBaselines.size(), state.animationBaselines.capacity());
    structure("candidate.animation-contributions", state.animationLayerContributions.size(), state.animationLayerContributions.capacity());
    structure("candidate.host-overrides", state.hostOverrides.size(), state.hostOverrides.capacity());
    structure("candidate.preview-overrides", state.previewOverrides.size(), state.previewOverrides.capacity());
    structure("candidate.gameplay-writes", state.gameplayWrites.size(), state.gameplayWrites.capacity());
    structure("scratch.runtime-transform-writes", state.transformWrites.size(), state.transformWrites.capacity());
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
      resourceScope_(std::move(other.resourceScope_)), world_(std::move(other.world_)),
      presentationCamera_(other.presentationCamera_) {
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
    presentationCamera_ = other.presentationCamera_;
    other.owner_ = nullptr;
    other.ownerToken_ = 0;
    other.managerToken_ = 0;
    return *this;
}

RuntimeSession::RuntimeSession() noexcept : RuntimeSession(RuntimeSessionKind::Playback) {}

RuntimeSession::RuntimeSession(RuntimeSessionKind kind) noexcept
    : sessionKind_(kind), sessionToken_(allocateSessionToken()) {}

RuntimeSession::RuntimeSession(assets::ResourceManager& resourceManager) noexcept
    : RuntimeSession(RuntimeSessionKind::Playback, resourceManager) {}

RuntimeSession::RuntimeSession(RuntimeSessionKind kind,
                               assets::ResourceManager& resourceManager) noexcept
    : RuntimeSession(kind) {
    resourceManager_ = &resourceManager;
    managerToken_ = resourceManager.managerToken();
}

RuntimeSession::~RuntimeSession() {
    threadChecker_.assertCurrent();
}

auto RuntimeSession::prepare(chart::ChartRuntime chartRuntime) const
    -> PreparedRuntimeSessionResult {
    return prepare(std::move(chartRuntime), animation::AnimationProgram{});
}

auto RuntimeSession::prepare(chart::ChartRuntime chartRuntime,
                             animation::AnimationProgram&& animation) const
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

    auto evaluation = buildEvaluationState(chartRuntime, instantiated.value->objects,
                                           *instantiated.value->world, std::move(animation));
    if (!evaluation) {
        addSessionError(result.diagnostics, evaluation.error());
        result.diagnostics.sortDeterministically();
        return result;
    }

    auto camera =
        std::as_const(*instantiated.value->world)
            .withRegistry([&](const entt::registry& registry) -> std::optional<entt::entity> {
                for (std::size_t index = 0; index < chartRuntime.objects.size(); ++index) {
                    const auto entity = instantiated.value->objects.entries()[index].entity;
                    if (chartRuntime.objects[index].components.camera &&
                        registry.all_of<render::CameraComponent>(entity))
                        return entity;
                }
                std::optional<entt::entity> selected;
                for (const auto entity : registry.view<const render::CameraComponent>())
                    if (!selected || entt::to_integral(entity) < entt::to_integral(*selected))
                        selected = entity;
                return selected;
            });
    if (!camera) {
        addSessionError(result.diagnostics, camera.error());
        return result;
    }
    result.diagnostics.sortDeterministically();
    auto world = std::move(instantiated.value->world);
    auto objects = std::move(instantiated.value->objects);
    result.prepared.emplace(PreparedRuntimeSession{
        *this, sessionToken_, managerToken_, std::move(chartRuntime), std::move(objects),
        result.diagnostics, std::move(*evaluation), std::move(resourceScope), std::move(world)});
    result.prepared->presentationCamera_ = *camera;
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

    auto mutation = internal::presentation::Mutation::reserve(presentationClock_, true);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    chartRuntime_.emplace(std::move(prepared.chartRuntime_));
    objects_ = std::move(prepared.objects_);
    activeDiagnostics_ = std::move(prepared.diagnostics_);
    evaluation_ = std::move(prepared.evaluation_);
    resourceScope_ = std::move(prepared.resourceScope_);
    world_ = std::move(prepared.world_);
    presentationCamera_ = prepared.presentationCamera_;
    lastFrame_.reset();
    publishPresentation(*mutation, internal::presentation::Source::Prepared);
    return {};
}

auto RuntimeSession::updatePrepared(world::World& targetWorld, RuntimeEvaluationState& state,
                                    const chart::TimingMap& timingMap, const RuntimeFrame& frame,
                                    bool tickLifetimes, std::vector<RuntimeDebugRecord>* records,
                                    bool* truncated) -> core::Result<void> {
    internal::pd::Timer preparation{internal::pd::Stage::runtimePrepare};
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

    state.animationLayerContributions.clear();
    state.resolver.beginFrame();
    auto behaviorApplied =
        state.resolver.applyLayer(state.writes.writes(), world::PropertyLayer::Behavior, true);
    if (!behaviorApplied) {
        return core::unexpected(std::move(behaviorApplied.error()));
    }

    if (state.animation.has_value()) {
        auto baselines = rebuildAnimationBaselines(state);
        if (!baselines) {
            return core::unexpected(std::move(baselines.error()));
        }
        auto mixed = animation::AnimationSystem::evaluate(
            *state.animation, beatSample->rationalBeat, state.animationBindings,
            state.animationBaselines, state.animationWrites, debugOptions_.enabled);
        if (!mixed) {
            return core::unexpected(std::move(mixed.error()));
        }
        if (debugOptions_.enabled) {
            state.animationLayerContributions = std::move(mixed->layerContributions);
        }
        auto animationApplied = state.resolver.applyLayer(state.animationWrites.writes(),
                                                          world::PropertyLayer::Animation, true);
        if (!animationApplied) {
            return core::unexpected(std::move(animationApplied.error()));
        }
    }

    if (auto gameplayApplied = state.resolver.applyLayer(
            state.gameplayWrites, world::PropertyLayer::GameplayOverride, true);
        !gameplayApplied)
        return core::unexpected(std::move(gameplayApplied.error()));
    if (!state.hostOverrides.empty()) {
        std::vector<world::OverrideToken> activeHost;
        activeHost.reserve(state.hostOverrides.size());
        for (const auto& token : state.hostOverrides) {
            if (overrideIsActive(token, frame)) {
                activeHost.push_back(token);
            }
        }
        auto hostApplied =
            state.resolver.applyOverrides(activeHost, world::PropertyLayer::HostOverride);
        if (!hostApplied) {
            return core::unexpected(std::move(hostApplied.error()));
        }
    }
    if (!state.previewOverrides.empty()) {
        std::vector<world::OverrideToken> activePreview;
        activePreview.reserve(state.previewOverrides.size());
        for (const auto& token : state.previewOverrides) {
            if (overrideIsActive(token, frame)) {
                activePreview.push_back(token);
            }
        }
        auto previewApplied = state.resolver.applyOverrides(
            activePreview, world::PropertyLayer::StudioPreviewOverride);
        if (!previewApplied) {
            return core::unexpected(std::move(previewApplied.error()));
        }
    }
    auto finalized = state.resolver.finalize();
    if (!finalized) {
        return core::unexpected(std::move(finalized.error()));
    }
    auto presented = applyResolvedPresentation(state);
    if (!presented) {
        return core::unexpected(std::move(presented.error()));
    }
    if (records && truncated) {
        if (debugOptions_.enabled) {
            auto captured = captureDebug(state, objects_, beatSample->beat, *records, *truncated);
            if (!captured)
                return captured;
        } else {
            records->clear();
            *truncated = false;
        }
    }
    if (!transformUpdate_)
        transformUpdate_ = std::make_unique<world::detail::TransformUpdate>();
    auto transforms =
        world::detail::ResolverAccess::prepareTransforms(state.resolver, state.transformWrites);
    if (!transforms)
        return transforms;
    auto planned = transformUpdate_->prepare(targetWorld, state.transformWrites);
    if (!planned)
        return planned;
    struct PreparedWindow final {
        world::detail::TransformUpdate& plan;
        RuntimeEvaluationState& state;
        ~PreparedWindow() {
            for (auto& camera : state.cameras)
                camera.slot = nullptr;
            for (auto& appearance : state.appearances)
                appearance.slot = nullptr;
            plan.cancel();
        }
    } window{*transformUpdate_, state};
    auto slots = preparePresentationSlots(state, *transformUpdate_);
    if (!slots)
        return slots;
    internal::pd::Timer commit{internal::pd::Stage::runtimeCommit};
    installRuntimeState(state, *transformUpdate_, frame, tickLifetimes);

    return {};
}

auto detail::RuntimeTestAccess::sampleReload(RuntimeSession& session,
                                             PreparedRuntimeSession& prepared,
                                             const RuntimeFrame& frame) -> core::Result<void> {
    return session.updatePrepared(*prepared.world_, *prepared.evaluation_,
                                  prepared.chartRuntime_.timingMap, frame);
}

auto RuntimeSession::captureDebug(const RuntimeEvaluationState& state,
                                  const ObjectEntityMap& objects, double beatValue,
                                  std::vector<RuntimeDebugRecord>& records, bool& truncated) const
    -> core::Result<void> {
    records.clear();
    truncated = false;
    if (!debugOptions_.enabled) {
        return {};
    }

    try {
        std::size_t writeIndex = 0;
        const auto writes = state.writes.writes();
        const auto append = [&](const behavior::BehaviorBinding& binding,
                                world::PropertyId property, std::optional<std::size_t> eventIndex,
                                double progress, const world::PropertyWriteValue& output) {
            if (records.size() >= debugOptions_.capacity) {
                truncated = true;
                return;
            }
            const auto initial = baselineFor(binding, property);
            auto behaviorValue = owningValue(output);
            auto animationValue = state.resolver.layerValue(binding.entity, property,
                                                            world::PropertyLayer::Animation);
            auto hostValue = state.resolver.layerValue(binding.entity, property,
                                                       world::PropertyLayer::HostOverride);
            auto previewValue = state.resolver.layerValue(
                binding.entity, property, world::PropertyLayer::StudioPreviewOverride);
            auto finalValue = resolvedValue(state, binding.entity, property);
            records.push_back(RuntimeDebugRecord{
                .objectId = objectIdFor(objects, binding.entity),
                .property = property,
                .initialValue = initial,
                .eventIndex = eventIndex,
                .normalizedProgress = progress,
                .behaviorValue = behaviorValue,
                .animationValue = animationValue.value_or(world::PropertyValue{}),
                .hostOverrideValue = hostValue.value_or(world::PropertyValue{}),
                .previewOverrideValue = previewValue.value_or(world::PropertyValue{}),
                .finalValue = finalValue ? std::move(*finalValue) : std::move(behaviorValue),
                .sourceLayer = state.resolver.sourceLayer(binding.entity, property),
                .conflict = state.resolver.hadConflict(binding.entity, property),
                .animationLayers = {},
            });
            auto& record = records.back();
            for (const auto& contribution : state.animationLayerContributions) {
                if (contribution.objectId.value != record.objectId.value ||
                    contribution.property != record.property) {
                    continue;
                }
                RuntimeDebugAnimationLayer layer;
                layer.identity = identityLabel(contribution.layerIdentity);
                layer.priority = contribution.priority;
                layer.weight = contribution.weight;
                layer.mask = contribution.propertyMask.properties;
                layer.mask.insert(layer.mask.end(), contribution.propertyMask.prefixes.begin(),
                                  contribution.propertyMask.prefixes.end());
                layer.value = contribution.value;
                record.animationLayers.push_back(std::move(layer));
            }
        };

        for (const auto& binding : state.program.bindings) {
            const auto& definition = state.program.definitions[binding.behavior.value];
            for (const auto& track : definition.tracks) {
                if (writeIndex >= writes.size()) {
                    truncated = true;
                    return {};
                }
                append(binding, track.property, std::nullopt, 0.0, writes[writeIndex].value);
                ++writeIndex;
            }
            for (const auto& track : definition.eventTracks) {
                if (writeIndex >= writes.size()) {
                    truncated = true;
                    return {};
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
                        progress =
                            (beatValue - event->startBeat) / (event->endBeat - event->startBeat);
                    }
                }
                append(binding, track.property, eventIndex, progress, writes[writeIndex].value);
                ++writeIndex;
            }
            for (const auto& track : definition.stepTracks) {
                if (writeIndex >= writes.size()) {
                    truncated = true;
                    return {};
                }
                std::optional<std::size_t> eventIndex;
                if (!track.events.empty() && beatValue >= track.events.front().beat) {
                    const auto next = std::upper_bound(
                        track.events.begin(), track.events.end(), beatValue,
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
        return {};
    } catch (const std::bad_alloc&) {
        records.clear();
        truncated = false;
        return core::unexpected(core::Error{"runtime.debug.capture_allocation_failed",
                                            "Runtime debug capture could not be allocated"});
    } catch (...) {
        records.clear();
        truncated = false;
        return core::unexpected(
            core::Error{"runtime.debug.capture_failed", "Runtime debug capture failed"});
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

auto RuntimeSession::update(const RuntimeFrame& frame) -> core::Result<void> try {
    internal::pd::Timer measurement{internal::pd::Stage::runtime};
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession update must run on its owner thread"});
    }
    if (!world_ || !evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "Cannot update an empty RuntimeSession"});
    }
    if (callbackActive_)
        return core::unexpected(
            core::Error{"runtime.session.callback_reentrant", "Runtime callback reentry"});
    auto valid = validateFrame(frame, lastFrame_);
    if (!valid) {
        return core::unexpected(std::move(valid.error()));
    }
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_, false, true);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    bool truncated{};
    auto updated = updatePrepared(*world_, *evaluation_, chartRuntime_->timingMap, frame, true,
                                  &pendingDebugRecords_, &truncated);
    if (!updated)
        return updated;
    debugRecords_.swap(pendingDebugRecords_);
    debugTruncated_ = truncated;
    lastFrame_ = frame;
    publishPresentation(*mutation, internal::presentation::Source::RuntimeUpdate, frame);
    return {};
} catch (...) {
    if (transformUpdate_)
        transformUpdate_->cancel();
    return core::unexpected(
        core::Error{"runtime.session.update_exception", "Runtime preparation failed"});
}

auto RuntimeSession::acquireOverride(world::OverrideKind kind, std::string ownerId,
                                     std::int64_t priority, std::uint16_t propertyMask,
                                     world::OverrideLifetime lifetime,
                                     std::span<const PropertyOverrideWrite> writes)
    -> core::Result<world::OverrideTokenId> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (callbackActive_) {
        return core::unexpected(core::Error{"runtime.session.callback_reentrant",
                                            "RuntimeSession World callback must not be reentrant"});
    }
    if (!world_ || !evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    if (kind == world::OverrideKind::StudioPreview &&
        sessionKind_ != RuntimeSessionKind::StudioPreview) {
        return core::unexpected(
            core::Error{"runtime.override.preview_session_required",
                        "StudioPreviewOverride is only valid on a Studio preview RuntimeSession"});
    }
    if (writes.empty()) {
        return core::unexpected(core::Error{"runtime.override.empty",
                                            "Override tokens require at least one property write"});
    }

    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    world::OverrideToken token;
    token.id.value = evaluation_->nextOverrideId++;
    token.kind = kind;
    token.ownerId = std::move(ownerId);
    token.priority = priority;
    token.propertyMask = propertyMask;
    token.lifetime = lifetime;
    token.writes.reserve(writes.size());
    for (const auto& write : writes) {
        const auto entity = objects_.find(write.objectId);
        if (!entity.has_value() || *entity == entt::null) {
            return core::unexpected(core::Error{"runtime.override.object_missing",
                                                "Override target object was not found"}
                                        .withContext("object_id", write.objectId.value));
        }
        if ((propertyMask & world::propertyBit(write.property)) == 0U) {
            return core::unexpected(core::Error{"world.property.override_mask",
                                                "Override write is outside the token property mask"}
                                        .withContext("object_id", write.objectId.value)
                                        .withContext("owner", token.ownerId));
        }
        token.writes.push_back(world::OverrideWrite{
            .entity = *entity, .property = write.property, .value = write.value});
    }

    auto& store = kind == world::OverrideKind::StudioPreview ? evaluation_->previewOverrides
                                                             : evaluation_->hostOverrides;
    store.push_back(std::move(token));
    publishPresentation(*mutation, internal::presentation::Source::Mutation);
    return store.back().id;
}

auto RuntimeSession::releaseOverride(world::OverrideTokenId id) -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (callbackActive_) {
        return core::unexpected(core::Error{"runtime.session.callback_reentrant",
                                            "RuntimeSession World callback must not be reentrant"});
    }
    if (!evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto eraseFrom = [&](std::vector<world::OverrideToken>& tokens) {
        const auto found =
            std::find_if(tokens.begin(), tokens.end(),
                         [id](const world::OverrideToken& token) { return token.id == id; });
        if (found == tokens.end()) {
            return false;
        }
        tokens.erase(found);
        return true;
    };
    if (eraseFrom(evaluation_->hostOverrides) || eraseFrom(evaluation_->previewOverrides)) {
        publishPresentation(*mutation, internal::presentation::Source::Mutation);
        return {};
    }
    return core::unexpected(
        core::Error{"runtime.override.token_missing", "Override token was not found"}.withContext(
            "token", std::to_string(id.value)));
}

auto RuntimeSession::applyBaseProperty(const chart::ChartObjectId& objectId,
                                       world::PropertyId property, world::PropertyValue value)
    -> core::Result<void> try {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (callbackActive_) {
        return core::unexpected(core::Error{"runtime.session.callback_reentrant",
                                            "RuntimeSession World callback must not be reentrant"});
    }
    if (!world_ || !evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto candidate = std::make_unique<RuntimeEvaluationState>(*evaluation_);
    const auto entity = objects_.find(objectId);
    if (!entity.has_value() || *entity == entt::null) {
        return core::unexpected(
            core::Error{"runtime.property.object_missing", "Base property target was not found"}
                .withContext("object_id", objectId.value));
    }
    auto applied = candidate->resolver.applyBaseProperty(*entity, property, value);
    if (!applied) {
        return applied;
    }
    for (auto& binding : candidate->program.bindings) {
        if (binding.entity != *entity) {
            continue;
        }
        for (auto& baseline : binding.baselines) {
            if (baseline.property == property) {
                baseline.value = value;
            }
        }
    }
    if (property == world::PropertyId::CameraFovY) {
        if (const auto* fov = std::get_if<double>(&value)) {
            for (auto& camera : candidate->cameras) {
                if (camera.entity == *entity) {
                    camera.baselineFovY = *fov;
                }
            }
        }
    }
    for (auto& appearance : candidate->appearances) {
        if (appearance.entity != *entity) {
            continue;
        }
        switch (property) {
        case world::PropertyId::RenderVisible:
            if (const auto* visible = std::get_if<bool>(&value)) {
                appearance.baseline.visible = *visible;
            }
            break;
        case world::PropertyId::RenderMaterial:
            if (const auto* material = std::get_if<std::string>(&value)) {
                appearance.baseline.materialAssetId = *material;
            }
            break;
        case world::PropertyId::MaterialOpacity:
            if (const auto* opacity = std::get_if<double>(&value)) {
                appearance.baseline.opacity = *opacity;
            }
            break;
        case world::PropertyId::MaterialTint:
            if (const auto* tint = std::get_if<core::Vec3>(&value)) {
                appearance.baseline.tint = *tint;
            }
            break;
        default:
            break;
        }
    }
    std::vector<RuntimeDebugRecord> records;
    bool truncated{};
    if (lastFrame_) {
        auto updated = updatePrepared(*world_, *candidate, chartRuntime_->timingMap, *lastFrame_,
                                      true, &records, &truncated);
        if (!updated)
            return updated;
    }
    evaluation_.swap(candidate);
    if (lastFrame_) {
        debugRecords_.swap(records);
        debugTruncated_ = truncated;
    }
    publishPresentation(*mutation, internal::presentation::Source::Mutation);
    return {};
} catch (...) {
    if (transformUpdate_)
        transformUpdate_->cancel();
    return core::unexpected(
        core::Error{"runtime.session.update_exception", "Base property preparation failed"});
}

auto RuntimeSession::applyBaseProperty(const BasePropertyCommand& command) -> core::Result<void> {
    return applyBaseProperty(command.objectId, command.property, command.value);
}

auto RuntimeSession::baseRevision() const noexcept -> std::uint64_t {
    threadChecker_.assertCurrent();
    return evaluation_ ? evaluation_->resolver.baseRevision() : 0;
}

auto RuntimeSession::reload(chart::ChartRuntime replacement) -> RuntimeSessionReloadResult {
    const RuntimeFrame target = lastFrame_.value_or(RuntimeFrame{});
    return reload(std::move(replacement), target, ReloadPolicy::KeepChartTime);
}

auto RuntimeSession::reload(chart::ChartRuntime replacement, const RuntimeFrame& targetFrame,
                            ReloadPolicy policy) -> RuntimeSessionReloadResult try {
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

    if (callbackActive_) {
        addSessionError(result.diagnostics, "runtime.session.callback_reentrant",
                        "Runtime callback reentry");
        return result;
    }
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_, true);
    if (!mutation) {
        addSessionError(result.diagnostics, mutation.error());
        return result;
    }
    auto preparedResult =
        evaluation_->animation.has_value()
            ? prepare(std::move(replacement), animation::AnimationProgram{*evaluation_->animation})
            : prepare(std::move(replacement));
    result.diagnostics = std::move(preparedResult.diagnostics);
    if (!preparedResult.hasValue()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    // Evaluate the candidate directly; the active World is never exchanged during prepare.
    auto sampled =
        updatePrepared(*preparedResult.prepared->world_, *preparedResult.prepared->evaluation_,
                       preparedResult.prepared->chartRuntime_.timingMap, reloadFrame);
    if (!sampled) {
        addSessionError(result.diagnostics, sampled.error());
        result.diagnostics.sortDeterministically();
        return result;
    }

    std::vector<RuntimeDebugRecord> candidateDebugRecords;
    bool candidateDebugTruncated = false;
    if (debugOptions_.enabled) {
        const auto beatSample = preparedResult.prepared->chartRuntime_.timingMap.sampleChartTimeMs(
            reloadFrame.chartTimeMs);
        if (!beatSample) {
            addSessionError(result.diagnostics, beatSample.error());
            result.diagnostics.sortDeterministically();
            return result;
        }
        auto captured =
            captureDebug(*preparedResult.prepared->evaluation_, preparedResult.prepared->objects_,
                         beatSample->beat, candidateDebugRecords, candidateDebugTruncated);
        if (!captured) {
            addSessionError(result.diagnostics, captured.error());
            result.diagnostics.sortDeterministically();
            return result;
        }
    }
    replaceWith(std::move(*preparedResult.prepared));
    lastFrame_ = reloadFrame;
    if (debugOptions_.enabled) {
        debugRecords_ = std::move(candidateDebugRecords);
        debugTruncated_ = candidateDebugTruncated;
    } else {
        debugRecords_.clear();
        debugTruncated_ = false;
    }
    publishPresentation(*mutation, internal::presentation::Source::Restore, reloadFrame);
    result.reloaded = true;
    return result;
} catch (...) {
    if (transformUpdate_)
        transformUpdate_->cancel();
    RuntimeSessionReloadResult result;
    addSessionError(result.diagnostics, "runtime.session.update_exception",
                    "Reload preparation failed");
    return result;
}

auto RuntimeSession::unload() -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession unload must run on its owner thread"});
    }
    if (callbackActive_)
        return core::unexpected(
            core::Error{"runtime.session.callback_reentrant", "Runtime callback reentry"});
    if (!world_)
        return {};
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    evaluation_.reset();
    world_.reset();
    objects_.clear();
    chartRuntime_.reset();
    activeDiagnostics_.clear();
    resourceScope_.reset();
    lastFrame_.reset();
    debugRecords_.clear();
    debugTruncated_ = false;
    presentationCamera_.reset();
    publishPresentation(*mutation, internal::presentation::Source::Mutation);
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
    presentationCamera_ = prepared.presentationCamera_;

    previousEvaluation.reset();
    previousWorld.reset();
    previousScope.reset();
    previousObjects.clear();
    previousDiagnostics.clear();
    previousRuntime.reset();
}

auto RuntimeSession::updateGameplay(const RuntimeFrame& frame,
                                    std::span<const PropertyOverrideWrite> writes,
                                    bool tickLifetimes) -> core::Result<void> {
    internal::pd::Timer measurement{internal::pd::Stage::runtime};
    measurement.items(writes.size());
    if (!threadChecker_.isCurrent())
        return core::unexpected(
            core::Error{"runtime.session.not_owner_thread", "Wrong owner thread"});
    if (!evaluation_ || !world_)
        return core::unexpected(core::Error{"runtime.session.empty", "Runtime inactive"});
    if (callbackActive_)
        return core::unexpected(
            core::Error{"runtime.session.callback_reentrant", "Runtime callback reentry"});
    auto valid = validateFrame(frame, lastFrame_);
    if (!valid)
        return valid;
    auto mutation =
        internal::presentation::Mutation::reserve(presentationClock_, false, tickLifetimes);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    std::unique_ptr<RuntimeEvaluationState> candidate;
    try {
        {
            internal::pd::Timer copy{internal::pd::Stage::runtimeCopy};
            copy.items(1);
            copy.logicalCopy(sizeof(RuntimeEvaluationState));
            candidate = std::make_unique<RuntimeEvaluationState>(*evaluation_);
        }
        candidate->resolver.beginFrame();
        candidate->gameplayWrites.clear();
        candidate->gameplayWrites.reserve(writes.size());
        for (const auto& write : writes) {
            if (write.property != world::PropertyId::RenderVisible ||
                !std::holds_alternative<bool>(write.value))
                return core::unexpected(core::Error{"world.property.invalid_value",
                                                    "Gameplay requires typed render.visible"});
            auto entity = findEntity(write.objectId);
            if (!entity)
                return core::unexpected(std::move(entity.error()));
            if (!*entity)
                return core::unexpected(
                    core::Error{"presentation-target-missing", "Gameplay target missing"});
            candidate->gameplayWrites.push_back(
                {**entity, write.property, std::get<bool>(write.value)});
        }
        std::vector<RuntimeDebugRecord> records;
        bool truncated{};
        auto updated = updatePrepared(*world_, *candidate, chartRuntime_->timingMap, frame,
                                      tickLifetimes, &records, &truncated);
        if (!updated) {
            return updated;
        }
        evaluation_.swap(candidate);
        internal::pd::memory::structure("current.runtime", 1, 1);
        debugRecords_.swap(records);
        debugTruncated_ = truncated;
        lastFrame_ = frame;
        publishPresentation(*mutation,
                            tickLifetimes ? internal::presentation::Source::RuntimeUpdate
                                          : internal::presentation::Source::Restore,
                            frame);
        return {};
    } catch (...) {
        if (transformUpdate_)
            transformUpdate_->cancel();
        return core::unexpected(core::Error{"runtime.session.update_exception",
                                            "Gameplay projection allocation failed"});
    }
}

void RuntimeSession::publishPresentation(internal::presentation::Mutation& mutation,
                                         internal::presentation::Source source,
                                         std::optional<RuntimeFrame> frame) noexcept {
    mutation.publish();
    presentationSource_ = source;
    presentationFrame_ = frame;
}

} // namespace cuexis::runtime
