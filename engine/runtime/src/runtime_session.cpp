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
    auto planned = transformUpdate_->prepare(targetWorld, state.transformWrites, false, true);
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

} // namespace cuexis::runtime
