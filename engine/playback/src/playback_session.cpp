// PlaybackSession host facade: load, absolute-time update, owning headless snapshots and reload.

#include <cuexis/playback/playback_session.hpp>

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::playback {
namespace {

void copyMatrix(const core::Mat4& source, float (&destination)[16]) noexcept {
    std::copy(source.values.begin(), source.values.end(), destination);
}

[[nodiscard]] auto runtimeFrame(const RuntimeFrame& frame) noexcept -> runtime::RuntimeFrame {
    return runtime::RuntimeFrame{.chartTimeMs = frame.chartTimeMs,
                                 .simulationDeltaTimeMs = frame.simulationDeltaTimeMs,
                                 .timeDiscontinuityId = frame.timeDiscontinuityId};
}

[[nodiscard]] auto runtimeReloadPolicy(ReloadPolicy policy) noexcept -> runtime::ReloadPolicy {
    return policy == ReloadPolicy::RestartAtZero ? runtime::ReloadPolicy::RestartAtZero
                                                 : runtime::ReloadPolicy::KeepChartTime;
}

[[nodiscard]] auto operationError(std::string code, std::string message,
                                  const core::Diagnostics& diagnostics) -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    if (!diagnostics.items().empty()) {
        error.withContext("diagnostic_code", std::string{diagnostics.items().front().code()});
        if (!diagnostics.items().front().fieldPath().empty()) {
            error.withContext("field_path", std::string{diagnostics.items().front().fieldPath()});
        }
    }
    return error;
}

struct SnapshotEntity final {
    chart::ChartObjectId id;
    entt::entity entity{entt::null};
};

struct SnapshotLayout final {
    std::vector<SnapshotEntity> entities;
    std::optional<entt::entity> activeCamera;
};

[[nodiscard]] auto ownerError(std::string_view operation) -> core::Error {
    return core::Error{"playback.session.not_owner_thread",
                       "PlaybackSession belongs to another thread"}
        .withContext("operation", std::string{operation});
}

[[nodiscard]] auto buildSnapshotLayout(runtime::RuntimeSession& session,
                                       const chart::ChartRuntime& chartRuntime)
    -> core::Result<SnapshotLayout> {
    SnapshotLayout layout;
    layout.entities.reserve(chartRuntime.objects.size());
    for (const auto& object : chartRuntime.objects) {
        auto entity = session.findEntity(object.id);
        if (!entity) {
            return core::unexpected(std::move(entity.error()));
        }
        if (!entity->has_value()) {
            return core::unexpected(core::Error{"playback.snapshot.object_missing",
                                                "Runtime object mapping is incomplete"}
                                        .withContext("object_id", object.id.value));
        }
        layout.entities.push_back(SnapshotEntity{.id = object.id, .entity = **entity});
    }

    auto camera = session.withWorld(
        [&](const world::World& world) -> core::Result<std::optional<entt::entity>> {
            return world.withRegistry([&](const entt::registry& registry)
                                          -> core::Result<std::optional<entt::entity>> {
                for (std::size_t index = 0; index < chartRuntime.objects.size(); ++index) {
                    if (chartRuntime.objects[index].components.camera.has_value() &&
                        registry.all_of<render::CameraComponent>(layout.entities[index].entity)) {
                        return layout.entities[index].entity;
                    }
                }

                std::optional<entt::entity> selected;
                const auto view = registry.view<const render::CameraComponent>();
                for (const entt::entity entity : view) {
                    if (!selected.has_value() ||
                        entt::to_integral(entity) < entt::to_integral(*selected)) {
                        selected = entity;
                    }
                }
                return selected;
            });
        });
    if (!camera) {
        return core::unexpected(std::move(camera.error()));
    }
    if (!*camera) {
        return core::unexpected(std::move(camera->error()));
    }
    layout.activeCamera = **camera;
    return layout;
}

} // namespace

struct PlaybackSession::State final {
    State() = default;
    explicit State(assets::AssetDatabase database)
        : resourceManager(std::in_place, std::move(database)) {}

    core::ThreadChecker ownerThread;
    // ResourceManager must outlive RuntimeSession and its ResourceScope.
    std::optional<assets::ResourceManager> resourceManager;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    std::optional<chart::ChartRuntime> chartRuntime;
    SnapshotLayout snapshotLayout;
    std::optional<RuntimeFrame> lastFrame;
    core::Diagnostics diagnostics;
    SessionState sessionState{SessionState::Empty};
};

PlaybackSession::PlaybackSession() noexcept : state_(std::make_unique<State>()) {}

PlaybackSession::PlaybackSession(assets::AssetDatabase database)
    : state_(std::make_unique<State>(std::move(database))) {}

PlaybackSession::~PlaybackSession() {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

auto PlaybackSession::state() const -> core::Result<SessionState> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("state"));
    }
    return state_->sessionState;
}

auto PlaybackSession::loadChart(std::string_view jsonText) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("load_chart"));
    }
    if (state_->sessionState != SessionState::Empty) {
        return core::unexpected(core::Error{"playback.session.not_empty",
                                            "PlaybackSession must be Empty before loading"});
    }
    state_->diagnostics.clear();

    const chart::ChartLimits limits;
    auto documentResult = chart::ChartLoader::load(jsonText, limits);
    const bool documentValid = documentResult.hasValue();
    state_->diagnostics.append(std::move(documentResult.diagnostics));
    if (!documentValid) {
        return core::unexpected(operationError(
            "playback.chart.load_failed", "Chart loading produced errors", state_->diagnostics));
    }

    auto runtimeResult = chart::ChartCompiler::compile(*documentResult.document, limits);
    const bool runtimeValid = runtimeResult.hasValue();
    state_->diagnostics.append(std::move(runtimeResult.diagnostics));
    if (!runtimeValid) {
        return core::unexpected(operationError("playback.chart.compile_failed",
                                               "Chart compilation produced errors",
                                               state_->diagnostics));
    }

    auto session = state_->resourceManager.has_value()
                       ? std::make_unique<runtime::RuntimeSession>(*state_->resourceManager)
                       : std::make_unique<runtime::RuntimeSession>();
    auto prepared = session->prepare(*runtimeResult.runtime);
    const bool preparedValid = prepared.hasValue();
    state_->diagnostics.append(std::move(prepared.diagnostics));
    if (!preparedValid) {
        return core::unexpected(operationError("playback.session.prepare_failed",
                                               "RuntimeSession preparation produced errors",
                                               state_->diagnostics));
    }
    if (auto committed = session->commit(std::move(*prepared.prepared)); !committed) {
        return core::unexpected(std::move(committed.error()));
    }

    auto snapshotLayout = buildSnapshotLayout(*session, *runtimeResult.runtime);
    if (!snapshotLayout) {
        return core::unexpected(std::move(snapshotLayout.error()));
    }

    state_->runtimeSession = std::move(session);
    state_->chartRuntime = std::move(*runtimeResult.runtime);
    state_->snapshotLayout = std::move(*snapshotLayout);
    state_->lastFrame.reset();
    state_->diagnostics.sortDeterministically();
    state_->sessionState = SessionState::Ready;
    return {};
}

auto PlaybackSession::update(const RuntimeFrame& frame) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("update"));
    }
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running &&
        state_->sessionState != SessionState::Paused) {
        return core::unexpected(core::Error{"playback.session.not_ready",
                                            "PlaybackSession must be active to receive updates"});
    }
    auto updated = state_->runtimeSession->update(runtimeFrame(frame));
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
    }
    state_->lastFrame = frame;
    state_->sessionState = SessionState::Running;
    return {};
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport) const
    -> core::Result<FrameSnapshot> {
    FrameSnapshot snapshot;
    if (auto extracted = extractFrame(viewport, snapshot); !extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    return snapshot;
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport, FrameSnapshot& snapshot) const
    -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("extract_frame"));
    }
    if (state_->sessionState == SessionState::Empty || !state_->runtimeSession ||
        !state_->chartRuntime.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    if (viewport.width == 0 || viewport.height == 0) {
        return core::unexpected(core::Error{"playback.viewport.invalid",
                                            "FrameViewport width and height must be positive"});
    }

    auto extracted = state_->runtimeSession->withWorld([&](const world::World& world)
                                                           -> core::Result<void> {
        return world.withRegistry([&](const entt::registry& registry) -> core::Result<void> {
            snapshot.camera = {};
            snapshot.clearRed = 0.055F;
            snapshot.clearGreen = 0.063F;
            snapshot.clearBlue = 0.071F;
            snapshot.clearAlpha = 1.0F;
            snapshot.viewportWidth = viewport.width;
            snapshot.viewportHeight = viewport.height;
            if (snapshot.objects.size() < state_->snapshotLayout.entities.size()) {
                snapshot.objects.resize(state_->snapshotLayout.entities.size());
            }

            std::size_t objectCount = 0;
            for (const auto& entry : state_->snapshotLayout.entities) {
                if (registry.all_of<world::WorldTransformComponent>(entry.entity)) {
                    auto& object = snapshot.objects[objectCount++];
                    if (object.id != entry.id.value) {
                        object.id = entry.id.value;
                    }
                    copyMatrix(registry.get<world::WorldTransformComponent>(entry.entity).matrix,
                               object.worldMatrix);
                    object.visible = true;
                }
            }
            snapshot.objects.resize(objectCount);

            if (!state_->snapshotLayout.activeCamera.has_value()) {
                return {};
            }
            const auto activeCamera = *state_->snapshotLayout.activeCamera;
            if (!registry.all_of<render::CameraComponent>(activeCamera)) {
                return core::unexpected(core::Error{"playback.snapshot.camera_missing",
                                                    "Active camera is unavailable"});
            }
            const auto& camera = registry.get<render::CameraComponent>(activeCamera);
            if (!std::isfinite(camera.fovY) || camera.fovY <= 0.0 || camera.fovY >= 179.0 ||
                !std::isfinite(camera.nearPlane) || !std::isfinite(camera.farPlane) ||
                camera.nearPlane <= 0.0 || camera.farPlane <= camera.nearPlane) {
                return core::unexpected(core::Error{"playback.snapshot.camera_invalid",
                                                    "Active camera parameters are invalid"});
            }

            snapshot.camera.active = true;
            snapshot.camera.fovY = camera.fovY;
            snapshot.camera.nearPlane = camera.nearPlane;
            snapshot.camera.farPlane = camera.farPlane;
            snapshot.camera.pitch = camera.pitch;
            snapshot.camera.yaw = camera.yaw;
            snapshot.camera.roll = camera.roll;
            const auto projection =
                core::makePerspective(camera.fovY * 3.14159265358979323846 / 180.0,
                                      static_cast<double>(viewport.width) / viewport.height,
                                      camera.nearPlane, camera.farPlane);
            copyMatrix(projection, snapshot.camera.projectionMatrix);

            core::Mat4 viewMatrix{};
            if (registry.all_of<world::WorldTransformComponent>(activeCamera)) {
                auto inverse = core::inverse(
                    registry.get<world::WorldTransformComponent>(activeCamera).matrix);
                if (!inverse) {
                    return core::unexpected(core::Error{"playback.snapshot.camera_not_invertible",
                                                        "Active camera transform is not invertible"}
                                                .withCause(inverse.error()));
                }
                viewMatrix = *inverse;
            }
            copyMatrix(viewMatrix, snapshot.camera.viewMatrix);
            return {};
        });
    });
    if (!extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    if (!*extracted) {
        return core::unexpected(std::move(extracted->error()));
    }
    return {};
}

auto PlaybackSession::reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("reload"));
    }
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running &&
        state_->sessionState != SessionState::Paused) {
        return core::unexpected(
            core::Error{"playback.session.not_ready", "PlaybackSession must be active to reload"});
    }

    const chart::ChartLimits limits;
    core::Diagnostics replacementDiagnostics;
    auto documentResult = chart::ChartLoader::load(replacementJson, limits);
    const bool documentValid = documentResult.hasValue();
    replacementDiagnostics.append(std::move(documentResult.diagnostics));
    if (!documentValid) {
        return core::unexpected(operationError("playback.chart.reload_load_failed",
                                               "Reload chart loading produced errors",
                                               replacementDiagnostics));
    }
    auto runtimeResult = chart::ChartCompiler::compile(*documentResult.document, limits);
    const bool runtimeValid = runtimeResult.hasValue();
    replacementDiagnostics.append(std::move(runtimeResult.diagnostics));
    if (!runtimeValid) {
        return core::unexpected(operationError("playback.chart.reload_compile_failed",
                                               "Reload chart compilation produced errors",
                                               replacementDiagnostics));
    }

    auto reloadResult = state_->runtimeSession->reload(
        *runtimeResult.runtime, runtimeFrame(targetFrame), runtimeReloadPolicy(policy));
    if (!reloadResult.reloaded) {
        return core::unexpected(operationError("playback.session.reload_failed",
                                               "RuntimeSession reload produced errors",
                                               reloadResult.diagnostics));
    }
    replacementDiagnostics.append(std::move(reloadResult.diagnostics));
    replacementDiagnostics.sortDeterministically();
    auto snapshotLayout = buildSnapshotLayout(*state_->runtimeSession, *runtimeResult.runtime);
    if (!snapshotLayout) {
        state_->sessionState = SessionState::Failed;
        return core::unexpected(std::move(snapshotLayout.error()));
    }
    state_->diagnostics = std::move(replacementDiagnostics);
    state_->chartRuntime = std::move(*runtimeResult.runtime);
    state_->snapshotLayout = std::move(*snapshotLayout);
    state_->lastFrame = targetFrame;
    state_->lastFrame->simulationDeltaTimeMs = 0.0;
    if (policy == ReloadPolicy::RestartAtZero) {
        state_->lastFrame->chartTimeMs = 0.0;
    }
    return {};
}

auto PlaybackSession::unload() -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("unload"));
    }
    if (state_->sessionState == SessionState::Empty) {
        return {};
    }
    if (state_->runtimeSession) {
        if (auto result = state_->runtimeSession->unload(); !result) {
            return result;
        }
    }
    state_->runtimeSession.reset();
    state_->chartRuntime.reset();
    state_->snapshotLayout = {};
    state_->lastFrame.reset();
    state_->diagnostics.clear();
    state_->sessionState = SessionState::Empty;
    return {};
}

auto PlaybackSession::chartInfo() const -> core::Result<ChartInfo> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("chart_info"));
    }
    if (!state_->runtimeSession || state_->runtimeSession->empty() ||
        !state_->chartRuntime.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed data"});
    }
    const auto renderableCount = static_cast<std::size_t>(
        std::count_if(state_->chartRuntime->objects.begin(), state_->chartRuntime->objects.end(),
                      [](const chart::RuntimeObject& object) {
                          return object.components.renderable.has_value();
                      }));
    return ChartInfo{.objectCount = state_->chartRuntime->objects.size(),
                     .behaviorCount = state_->chartRuntime->behaviors.size(),
                     .renderableCount = renderableCount,
                     .resourceCount = state_->runtimeSession->resourceCount()};
}

auto PlaybackSession::diagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("diagnostics"));
    }
    return state_->diagnostics;
}

} // namespace cuexis::playback
