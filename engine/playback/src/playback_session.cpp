// PlaybackSession host facade: load, absolute-time update, owning headless snapshots and reload.

#include <cuexis/playback/playback_session.hpp>

#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
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
    bool camera{};
};

} // namespace

struct PlaybackSession::State final {
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    std::optional<chart::ChartRuntime> chartRuntime;
    std::optional<RuntimeFrame> lastFrame;
    core::Diagnostics diagnostics;
    SessionState sessionState{SessionState::Empty};
};

PlaybackSession::PlaybackSession() noexcept : state_(std::make_unique<State>()) {}

PlaybackSession::~PlaybackSession() = default;

PlaybackSession::PlaybackSession(PlaybackSession&& other) noexcept
    : state_(std::move(other.state_)) {}

auto PlaybackSession::operator=(PlaybackSession&& other) noexcept -> PlaybackSession& {
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

auto PlaybackSession::state() const noexcept -> SessionState {
    return state_->sessionState;
}

auto PlaybackSession::loadChart(std::string_view jsonText) -> core::Result<void> {
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

    auto session = std::make_unique<runtime::RuntimeSession>();
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

    state_->runtimeSession = std::move(session);
    state_->chartRuntime = std::move(*runtimeResult.runtime);
    state_->lastFrame.reset();
    state_->diagnostics.sortDeterministically();
    state_->sessionState = SessionState::Ready;
    return {};
}

auto PlaybackSession::update(const RuntimeFrame& frame) -> core::Result<void> {
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
    if (state_->sessionState == SessionState::Empty || !state_->runtimeSession ||
        !state_->chartRuntime.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    if (viewport.width == 0 || viewport.height == 0) {
        return core::unexpected(core::Error{"playback.viewport.invalid",
                                            "FrameViewport width and height must be positive"});
    }

    std::vector<SnapshotEntity> entities;
    entities.reserve(state_->chartRuntime->objects.size());
    for (const auto& object : state_->chartRuntime->objects) {
        auto entity = state_->runtimeSession->findEntity(object.id);
        if (!entity) {
            return core::unexpected(std::move(entity.error()));
        }
        if (!entity->has_value()) {
            return core::unexpected(core::Error{"playback.snapshot.object_missing",
                                                "Runtime object mapping is incomplete"}
                                        .withContext("object_id", object.id.value));
        }
        entities.push_back(SnapshotEntity{
            .id = object.id, .entity = **entity, .camera = object.components.camera.has_value()});
    }

    auto extracted = state_->runtimeSession->withWorld([&](const world::World& world)
                                                           -> core::Result<FrameSnapshot> {
        return world.withRegistry([&](const entt::registry& registry)
                                      -> core::Result<FrameSnapshot> {
            FrameSnapshot snapshot;
            snapshot.viewportWidth = viewport.width;
            snapshot.viewportHeight = viewport.height;
            snapshot.objects.reserve(entities.size());

            std::optional<entt::entity> activeCamera;
            for (const auto& entry : entities) {
                if (registry.all_of<world::WorldTransformComponent>(entry.entity)) {
                    FrameSnapshot::ObjectSnapshot object;
                    object.id = entry.id.value;
                    copyMatrix(registry.get<world::WorldTransformComponent>(entry.entity).matrix,
                               object.worldMatrix);
                    snapshot.objects.push_back(std::move(object));
                }
                if (!activeCamera.has_value() && entry.camera &&
                    registry.all_of<render::CameraComponent>(entry.entity)) {
                    activeCamera = entry.entity;
                }
            }

            if (!activeCamera.has_value()) {
                std::vector<entt::entity> cameras;
                const auto view = registry.view<const render::CameraComponent>();
                for (const entt::entity entity : view) {
                    cameras.push_back(entity);
                }
                std::sort(cameras.begin(), cameras.end(),
                          [](entt::entity left, entt::entity right) {
                              return entt::to_integral(left) < entt::to_integral(right);
                          });
                if (!cameras.empty()) {
                    activeCamera = cameras.front();
                }
            }

            if (!activeCamera.has_value()) {
                return snapshot;
            }
            const auto& camera = registry.get<render::CameraComponent>(*activeCamera);
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
            if (registry.all_of<world::WorldTransformComponent>(*activeCamera)) {
                auto inverse = core::inverse(
                    registry.get<world::WorldTransformComponent>(*activeCamera).matrix);
                if (!inverse) {
                    return core::unexpected(core::Error{"playback.snapshot.camera_not_invertible",
                                                        "Active camera transform is not invertible"}
                                                .withCause(inverse.error()));
                }
                viewMatrix = *inverse;
            }
            copyMatrix(viewMatrix, snapshot.camera.viewMatrix);
            return snapshot;
        });
    });
    if (!extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    if (!*extracted) {
        return core::unexpected(std::move(extracted->error()));
    }
    return std::move(**extracted);
}

auto PlaybackSession::reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
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
    state_->diagnostics = std::move(replacementDiagnostics);
    state_->chartRuntime = std::move(*runtimeResult.runtime);
    state_->lastFrame = targetFrame;
    state_->lastFrame->simulationDeltaTimeMs = 0.0;
    if (policy == ReloadPolicy::RestartAtZero) {
        state_->lastFrame->chartTimeMs = 0.0;
    }
    return {};
}

auto PlaybackSession::unload() -> core::Result<void> {
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
    state_->lastFrame.reset();
    state_->diagnostics.clear();
    state_->sessionState = SessionState::Empty;
    return {};
}

auto PlaybackSession::chartInfo() const noexcept -> core::Result<ChartInfo> {
    if (!state_->runtimeSession || state_->runtimeSession->empty() ||
        !state_->chartRuntime.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed data"});
    }
    return ChartInfo{.objectCount = state_->chartRuntime->objects.size(),
                     .behaviorCount = state_->chartRuntime->behaviors.size()};
}

auto PlaybackSession::diagnostics() const noexcept -> const core::Diagnostics& {
    return state_->diagnostics;
}

} // namespace cuexis::playback
