// PlaybackSession host facade: load, absolute-time update, owning headless snapshots and reload.

#include <cuexis/playback/playback_session.hpp>

#include "playback_source_state.hpp"

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/content/content_provider.hpp>
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
#include <atomic>
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

std::atomic<std::uint64_t> nextPlaybackSessionToken{1};

[[nodiscard]] auto allocatePlaybackSessionToken() noexcept -> std::uint64_t {
    const auto token = nextPlaybackSessionToken.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        std::terminate();
    }
    return token;
}

void copyMatrix(const core::Mat4& source, float (&destination)[16]) noexcept {
    std::copy(source.values.begin(), source.values.end(), destination);
}

[[nodiscard]] auto runtimeFrame(const RuntimeFrame& frame) noexcept -> runtime::RuntimeFrame {
    return runtime::RuntimeFrame{.chartTimeMs = frame.chartTimeMs,
                                 .simulationDeltaTimeMs = frame.simulationDeltaTimeMs,
                                 .timeDiscontinuityId = frame.timeDiscontinuityId};
}

[[nodiscard]] auto operationError(std::string code, std::string message,
                                  const core::Diagnostics& diagnostics) -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    const auto firstError = std::find_if(
        diagnostics.items().begin(), diagnostics.items().end(), [](const core::Diagnostic& item) {
            return item.severity() == core::DiagnosticSeverity::Error;
        });
    if (firstError != diagnostics.items().end()) {
        error.withContext("diagnostic_code", std::string{firstError->code()});
        if (!firstError->fieldPath().empty()) {
            error.withContext("field_path", std::string{firstError->fieldPath()});
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

[[nodiscard]] auto chartInfoFor(const chart::ChartRuntime& chartRuntime, std::size_t resourceCount)
    -> ChartInfo {
    return ChartInfo{
        .objectCount = chartRuntime.objects.size(),
        .behaviorCount = chartRuntime.behaviors.size(),
        .renderableCount = static_cast<std::size_t>(
            std::count_if(chartRuntime.objects.begin(), chartRuntime.objects.end(),
                          [](const chart::RuntimeObject& object) {
                              return object.components.renderable.has_value();
                          })),
        .resourceCount = resourceCount,
    };
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
    layout.activeCamera = *camera;
    return layout;
}

} // namespace

struct PlaybackSession::State final {
    State() = default;

    core::ThreadChecker ownerThread;
    // ResourceManager must outlive RuntimeSession and its ResourceScope.
    std::shared_ptr<content::IContentProvider> contentProvider;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::string activeChartJson;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    SnapshotLayout snapshotLayout;
    std::optional<RuntimeFrame> lastFrame;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<ChartInfo> activeChartInfo;
    std::optional<PlaybackContentInfo> activeContentInfo;
    std::optional<PlaybackMode> activeMode;
    std::uint64_t sessionToken{allocatePlaybackSessionToken()};
    std::uint64_t generation{1};
    SessionState sessionState{SessionState::Empty};
};

struct PreparedPlayback::State final {
    PlaybackSession* owner{};
    std::uint64_t ownerToken{};
    std::uint64_t expectedGeneration{};
    bool replacement{};
    core::ThreadChecker ownerThread;
    std::shared_ptr<content::IContentProvider> contentProvider;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::string chartJson;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    SnapshotLayout snapshotLayout;
    ChartInfo chartInfo;
    PlaybackContentInfo contentInfo;
    std::optional<assets::AudioSourceLease> audioSourceLease;
    core::Diagnostics diagnostics;
    std::optional<RuntimeFrame> targetFrame;
    SessionState committedState{SessionState::Ready};
};

PreparedPlayback::PreparedPlayback() noexcept = default;

PreparedPlayback::PreparedPlayback(std::unique_ptr<State> state) noexcept
    : state_(std::move(state)) {}

PreparedPlayback::~PreparedPlayback() {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

PreparedPlayback::PreparedPlayback(PreparedPlayback&& other) noexcept
    : state_(std::move(other.state_)) {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

auto PreparedPlayback::operator=(PreparedPlayback&& other) noexcept -> PreparedPlayback& {
    if ((state_ && !state_->ownerThread.isCurrent()) ||
        (other.state_ && !other.state_->ownerThread.isCurrent())) {
        std::terminate();
    }
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

bool PreparedPlayback::valid() const noexcept {
    return state_ && state_->ownerThread.isCurrent() && state_->runtimeSession != nullptr;
}

const PlaybackContentInfo* PreparedPlayback::contentInfo() const noexcept {
    return valid() ? &state_->contentInfo : nullptr;
}

std::optional<MainMusicSourceView> PreparedPlayback::mainMusicSource() const noexcept {
    if (!valid() || !state_->audioSourceLease || !state_->audioSourceLease->valid()) {
        return std::nullopt;
    }
    const auto& source = state_->audioSourceLease->resource();
    return MainMusicSourceView{source.id.value, state_->contentInfo.timingOffsetMs,
                               source.blob->providerRevision, source.bytes()};
}

PlaybackSession::PlaybackSession() noexcept : state_(std::make_unique<State>()) {}

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

auto PlaybackSession::prepareLoad(std::string_view jsonText, PlaybackMode mode)
    -> core::Result<PreparedPlayback> {
    auto source = PlaybackSource::fromChartText(std::string{jsonText});
    if (!source) {
        return core::unexpected(std::move(source.error()));
    }
    return prepare(std::move(*source), mode, nullptr, ReloadPolicy::KeepChartTime, false);
}

auto PlaybackSession::prepareLoad(PlaybackSource&& source, PlaybackMode mode)
    -> core::Result<PreparedPlayback> {
    return prepare(std::move(source), mode, nullptr, ReloadPolicy::KeepChartTime, false);
}

auto PlaybackSession::prepareReload(std::string_view replacementJson,
                                    const RuntimeFrame& targetFrame, ReloadPolicy policy)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_reload"));
    }
    if (!state_->activeMode) {
        return core::unexpected(
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"});
    }
    auto source = PlaybackSource::fromChartText(std::string{replacementJson});
    if (!source) {
        return core::unexpected(std::move(source.error()));
    }
    if (state_->resourceManager) {
        auto sourceState = std::move(source->state_);
        sourceState->provider = state_->contentProvider;
        sourceState->database.emplace(state_->resourceManager->database());
        source->state_ = std::move(sourceState);
    }
    return prepare(std::move(*source), *state_->activeMode, &targetFrame, policy, true);
}

auto PlaybackSession::prepareReload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                                    ReloadPolicy policy) -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_reload"));
    }
    if (!state_->activeMode) {
        return core::unexpected(
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"});
    }
    return prepare(std::move(replacement), *state_->activeMode, &targetFrame, policy, true);
}

auto PlaybackSession::prepare(PlaybackSource&& source, PlaybackMode mode,
                              const RuntimeFrame* targetFrame, ReloadPolicy policy,
                              bool replacement) -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError(replacement ? "prepare_reload" : "prepare_load"));
    }
    if ((!replacement && state_->sessionState != SessionState::Empty) ||
        (replacement && state_->sessionState != SessionState::Ready &&
         state_->sessionState != SessionState::Running)) {
        return core::unexpected(
            core::Error{replacement ? "playback.session.not_ready" : "playback.session.not_empty",
                        replacement ? "PlaybackSession must be active before preparing a reload"
                                    : "PlaybackSession must be Empty before preparing a load"});
    }

    if (!source.state_) {
        return core::unexpected(core::Error{"playback.source.invalid", "PlaybackSource is empty"});
    }
    core::Diagnostics diagnostics;
    auto& sourceState = *source.state_;
    const auto& jsonText = sourceState.chartJson;
    const chart::ChartLimits limits;
    auto documentResult = chart::ChartLoader::load(jsonText, limits);
    const bool documentValid = documentResult.hasValue();
    diagnostics.append(std::move(documentResult.diagnostics));
    if (!documentValid) {
        state_->lastOperationDiagnostics = diagnostics;
        return core::unexpected(operationError(replacement ? "playback.chart.reload_load_failed"
                                                           : "playback.chart.load_failed",
                                               "Chart loading produced errors", diagnostics));
    }

    auto runtimeResult = chart::ChartCompiler::compile(*documentResult.document, limits);
    const bool runtimeValid = runtimeResult.hasValue();
    diagnostics.append(std::move(runtimeResult.diagnostics));
    if (!runtimeValid) {
        state_->lastOperationDiagnostics = diagnostics;
        return core::unexpected(operationError(replacement ? "playback.chart.reload_compile_failed"
                                                           : "playback.chart.compile_failed",
                                               "Chart compilation produced errors", diagnostics));
    }
    auto& chartRuntime = *runtimeResult.runtime;
    const bool hasMainMusic = chartRuntime.mainMusic.has_value();
    if ((mode == PlaybackMode::ChartClock && hasMainMusic) ||
        (mode != PlaybackMode::ChartClock && !hasMainMusic)) {
        return core::unexpected(
            core::Error{"playback.mode.content_mismatch",
                        mode == PlaybackMode::ChartClock
                            ? "ChartClock requires a chart without main music"
                            : "HostClock and CuexisAudio require a chart with main music"});
    }

    std::unique_ptr<assets::ResourceManager> resourceManager;
    if (sourceState.database) {
        resourceManager = std::make_unique<assets::ResourceManager>(
            std::move(*sourceState.database), sourceState.provider);
    }

    std::optional<assets::AudioSourceLease> audioSourceLease;
    if (chartRuntime.mainMusic) {
        if (!resourceManager) {
            return core::unexpected(core::Error{
                "playback.content.asset_database_missing",
                "A chart with main music requires an AssetDatabase and ContentProvider"});
        }
        auto sourceResult = resourceManager->requestAudioSource(
            assets::AssetId{chartRuntime.mainMusic->value}, assets::ResourcePolicy::Required);
        const bool sourceValid = sourceResult.hasValue();
        diagnostics.append(std::move(sourceResult.diagnostics));
        if (!sourceValid) {
            state_->lastOperationDiagnostics = diagnostics;
            return core::unexpected(
                operationError("playback.content.main_music_failed",
                               "Required main music source could not be prepared", diagnostics));
        }
        audioSourceLease.emplace(std::move(*sourceResult.lease));
    }

    auto session = resourceManager ? std::make_unique<runtime::RuntimeSession>(*resourceManager)
                                   : std::make_unique<runtime::RuntimeSession>();
    auto runtimePrepared = session->prepare(chartRuntime);
    const bool preparedValid = runtimePrepared.hasValue();
    diagnostics.append(std::move(runtimePrepared.diagnostics));
    if (!preparedValid) {
        state_->lastOperationDiagnostics = diagnostics;
        return core::unexpected(operationError("playback.session.prepare_failed",
                                               "RuntimeSession preparation produced errors",
                                               diagnostics));
    }
    if (auto committed = session->commit(std::move(*runtimePrepared.prepared)); !committed) {
        return core::unexpected(std::move(committed.error()));
    }

    std::optional<RuntimeFrame> committedFrame;
    if (replacement && targetFrame != nullptr) {
        committedFrame = *targetFrame;
        committedFrame->simulationDeltaTimeMs = 0.0;
        if (policy == ReloadPolicy::RestartAtZero) {
            committedFrame->chartTimeMs = 0.0;
        }
        if (auto updated = session->update(runtimeFrame(*committedFrame)); !updated) {
            return core::unexpected(std::move(updated.error()));
        }
    }

    auto snapshotLayout = buildSnapshotLayout(*session, chartRuntime);
    if (!snapshotLayout) {
        return core::unexpected(std::move(snapshotLayout.error()));
    }

    diagnostics.sortDeterministically();
    auto prepared = std::make_unique<PreparedPlayback::State>();
    prepared->owner = this;
    prepared->ownerToken = state_->sessionToken;
    prepared->expectedGeneration = state_->generation;
    prepared->replacement = replacement;
    prepared->contentProvider = std::move(sourceState.provider);
    prepared->resourceManager = std::move(resourceManager);
    prepared->chartJson = sourceState.chartJson;
    prepared->runtimeSession = std::move(session);
    prepared->snapshotLayout = std::move(*snapshotLayout);
    prepared->chartInfo = chartInfoFor(chartRuntime, prepared->runtimeSession->resourceCount());
    prepared->contentInfo = PlaybackContentInfo{
        chartRuntime.chartId.value, chartRuntime.version, chartRuntime.timingMap.offsetMs(), mode,
        chartRuntime.mainMusic ? std::optional<std::string>{chartRuntime.mainMusic->value}
                               : std::nullopt};
    prepared->audioSourceLease = std::move(audioSourceLease);
    prepared->diagnostics = std::move(diagnostics);
    prepared->targetFrame = committedFrame;
    prepared->committedState = replacement ? state_->sessionState : SessionState::Ready;
    return PreparedPlayback{std::move(prepared)};
}

auto PlaybackSession::commit(PreparedPlayback&& prepared) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("commit"));
    }
    if (!prepared.state_ || !prepared.state_->ownerThread.isCurrent()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    auto& candidate = *prepared.state_;
    if (candidate.owner != this || candidate.ownerToken != state_->sessionToken) {
        return core::unexpected(core::Error{"playback.prepared.wrong_session",
                                            "PreparedPlayback belongs to another PlaybackSession"});
    }
    if (candidate.expectedGeneration != state_->generation) {
        return core::unexpected(
            core::Error{"playback.prepared.stale", "PlaybackSession changed after preparation"});
    }
    if ((!candidate.replacement && state_->sessionState != SessionState::Empty) ||
        (candidate.replacement && state_->sessionState != SessionState::Ready &&
         state_->sessionState != SessionState::Running)) {
        return core::unexpected(core::Error{"playback.prepared.lifecycle_changed",
                                            "PlaybackSession lifecycle changed after preparation"});
    }

    state_->contentProvider = std::move(candidate.contentProvider);
    state_->resourceManager = std::move(candidate.resourceManager);
    state_->activeChartJson = std::move(candidate.chartJson);
    state_->runtimeSession = std::move(candidate.runtimeSession);
    state_->snapshotLayout = std::move(candidate.snapshotLayout);
    state_->activeChartInfo = candidate.chartInfo;
    state_->activeContentInfo = std::move(candidate.contentInfo);
    state_->activeMode = state_->activeContentInfo->mode;
    state_->lastFrame = candidate.targetFrame;
    state_->diagnostics = std::move(candidate.diagnostics);
    state_->lastOperationDiagnostics = state_->diagnostics;
    state_->sessionState = candidate.committedState;
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
    prepared.state_.reset();
    return {};
}

auto PlaybackSession::loadChart(std::string_view jsonText) -> core::Result<void> {
    auto prepared = prepareLoad(jsonText, PlaybackMode::ChartClock);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::load(PlaybackSource&& source, PlaybackMode mode) -> core::Result<void> {
    auto prepared = prepareLoad(std::move(source), mode);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::update(const RuntimeFrame& frame) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("update"));
    }
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running) {
        return core::unexpected(core::Error{"playback.session.not_ready",
                                            "PlaybackSession must be active to receive updates"});
    }
    auto updated = state_->runtimeSession->update(runtimeFrame(frame));
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
    }
    state_->lastFrame = frame;
    state_->sessionState = SessionState::Running;
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
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
        !state_->activeChartInfo.has_value()) {
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

            const core::Mat4 identity;
            for (std::size_t index = 0; index < state_->snapshotLayout.entities.size(); ++index) {
                const auto& entry = state_->snapshotLayout.entities[index];
                auto& object = snapshot.objects[index];
                if (object.id != entry.id.value) {
                    object.id = entry.id.value;
                }
                object.hasTransform = registry.all_of<world::WorldTransformComponent>(entry.entity);
                if (object.hasTransform) {
                    copyMatrix(registry.get<world::WorldTransformComponent>(entry.entity).matrix,
                               object.worldMatrix);
                } else {
                    copyMatrix(identity, object.worldMatrix);
                }
                object.visible = true;
            }
            snapshot.objects.resize(state_->snapshotLayout.entities.size());

            if (!state_->snapshotLayout.activeCamera.has_value()) {
                return {};
            }
            const auto activeCamera = *state_->snapshotLayout.activeCamera;
            if (!registry.all_of<render::CameraComponent>(activeCamera)) {
                return core::unexpected(core::Error{"playback.snapshot.camera_missing",
                                                    "Active camera is unavailable"});
            }
            const auto& camera = registry.get<render::CameraComponent>(activeCamera);
            const auto projection =
                core::makePerspective(camera.fovY * 3.14159265358979323846 / 180.0,
                                      static_cast<double>(viewport.width) / viewport.height,
                                      camera.nearPlane, camera.farPlane);
            if (!projection) {
                return core::unexpected(core::Error{"playback.snapshot.camera_invalid",
                                                    "Active camera parameters are invalid"}
                                            .withCause(projection.error()));
            }

            snapshot.camera.active = true;
            snapshot.camera.fovY = camera.fovY;
            snapshot.camera.nearPlane = camera.nearPlane;
            snapshot.camera.farPlane = camera.farPlane;
            snapshot.camera.pitch = camera.pitch;
            snapshot.camera.yaw = camera.yaw;
            snapshot.camera.roll = camera.roll;
            copyMatrix(*projection, snapshot.camera.projectionMatrix);

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
    return {};
}

auto PlaybackSession::reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
    auto prepared = prepareReload(replacementJson, targetFrame, policy);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::reload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
    auto prepared = prepareReload(std::move(replacement), targetFrame, policy);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
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
    state_->resourceManager.reset();
    state_->contentProvider.reset();
    state_->activeChartJson.clear();
    state_->snapshotLayout = {};
    state_->lastFrame.reset();
    state_->diagnostics.clear();
    state_->lastOperationDiagnostics.clear();
    state_->activeChartInfo.reset();
    state_->activeContentInfo.reset();
    state_->activeMode.reset();
    state_->sessionState = SessionState::Empty;
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
    return {};
}

auto PlaybackSession::chartInfo() const -> core::Result<ChartInfo> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("chart_info"));
    }
    if (!state_->runtimeSession || state_->runtimeSession->empty() ||
        !state_->activeChartInfo.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed data"});
    }
    auto info = *state_->activeChartInfo;
    info.resourceCount = state_->runtimeSession->resourceCount();
    return info;
}

auto PlaybackSession::contentInfo() const -> core::Result<PlaybackContentInfo> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("content_info"));
    }
    if (!state_->activeContentInfo) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed content"});
    }
    return *state_->activeContentInfo;
}

auto PlaybackSession::diagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("diagnostics"));
    }
    return state_->diagnostics;
}

auto PlaybackSession::lastOperationDiagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("last_operation_diagnostics"));
    }
    return state_->lastOperationDiagnostics;
}

} // namespace cuexis::playback
