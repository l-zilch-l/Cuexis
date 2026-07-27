// RuntimeSession transactional lifecycle and absolute-time Behavior evaluation.

#include <cuexis/runtime/runtime_session.hpp>

#include <cuexis/behavior/behavior_program.hpp>
#include <cuexis/behavior/behavior_system.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/world/property.hpp>
#include <cuexis/world/transform_system.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
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

    behavior::BehaviorProgram program;
    world::PropertyWriteBuffer writes;
    world::TransformPropertyResolver transformResolver;
    std::vector<CameraEntry> cameras;
    bool camerasCommitted{};
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
    }
    return world::PropertyId::TransformPositionX;
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
                                        const ObjectEntityMap& objects, const world::World& world)
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
        if (behavior->tracks.size() > world::maxPropertyWritesPerFrame ||
            requiredWrites > world::maxPropertyWritesPerFrame - behavior->tracks.size()) {
            return core::unexpected(core::Error{"runtime.program.write_limit",
                                                "Behavior program exceeds the write budget"});
        }
        requiredWrites += behavior->tracks.size();
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
                    .value = std::move(runtimeKey.value),
                    .easing = toEasing(runtimeKey.easing),
                });
            }
            definition.tracks.push_back(std::move(track));
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
        state->program.bindings.push_back(behavior::BehaviorBinding{
            .entity = objects.entries()[index].entity,
            .behavior = behavior::RuntimeBehaviorIndex{static_cast<std::uint32_t>(behaviorIndex)},
        });
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

auto RuntimeSession::updatePrepared(RuntimeEvaluationState& state, const RuntimeFrame& frame)
    -> core::Result<void> {
    auto evaluated =
        behavior::BehaviorSystem::evaluate(state.program, frame.chartTimeMs, state.writes);
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
    auto transformsCommitted = state.transformResolver.commit(*world_);
    if (!transformsCommitted) {
        return core::unexpected(std::move(transformsCommitted.error()));
    }
    auto camerasCommitted = commitCameras(state, *world_);
    if (!camerasCommitted) {
        state.transformResolver.rollback(*world_);
        return core::unexpected(std::move(camerasCommitted.error()));
    }
    auto transformsUpdated = world::updateWorldTransforms(*world_);
    if (!transformsUpdated) {
        rollbackCameras(state, *world_);
        state.transformResolver.rollback(*world_);
        return core::unexpected(std::move(transformsUpdated.error()));
    }
    return {};
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
    auto updated = updatePrepared(*evaluation_, frame);
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
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
    auto sampled = updatePrepared(*preparedResult.prepared->evaluation_, reloadFrame);
    preparedResult.prepared->world_ = std::move(world_);
    world_ = std::move(previousWorld);
    if (!sampled) {
        addSessionError(result.diagnostics, sampled.error());
        result.diagnostics.sortDeterministically();
        return result;
    }

    replaceWith(std::move(*preparedResult.prepared));
    lastFrame_ = reloadFrame;
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
