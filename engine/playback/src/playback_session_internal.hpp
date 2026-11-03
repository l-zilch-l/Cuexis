#pragma once

// State and helpers shared by the PlaybackSession units. The session state is one object that
// every unit touches, and the error, guard and projection helpers are used by several of them,
// so both live here instead of in whichever unit happened to need them first.

#include <cuexis/playback/playback_session.hpp>

#include "playback_candidate_internal.hpp"
#include "playback_source_state.hpp"
#include "presentation_internal.hpp"
#include "presentation_materializer.hpp"
#include "presentation_state.hpp"
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/execution_diagnostics.hpp>
#include <cuexis_internal/prepare_trace.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <cuexis_internal/sha256.hpp>
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include "gameplay_internal.hpp"
#include "presentation_projection.hpp"
#include "replay_identity.hpp"
#include <cuexis/judgement/execution_scheduler.hpp>
#endif

#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/assets/asset_database.hpp>
#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/candidate_lowering.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/chart_writer.hpp>
#include <cuexis/chart/detail/chart_dispatch_internal.hpp>
#include <cuexis/chart/detail/chart_v4_resolver_internal.hpp>
#include <cuexis/chart/prepared_semantic_identity.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/runtime/runtime_session.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::playback {

using detail::SnapshotEntity;
using detail::SnapshotLayout;

inline std::atomic<std::uint64_t> nextPlaybackSessionToken{1};

[[nodiscard]] inline auto allocatePlaybackSessionToken() noexcept -> std::uint64_t {
    const auto token = nextPlaybackSessionToken.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        std::terminate();
    }
    return token;
}

[[nodiscard]] inline auto runtimeFrame(const RuntimeFrame& frame) noexcept
    -> runtime::RuntimeFrame {
    return runtime::RuntimeFrame{.chartTimeMs = frame.chartTimeMs,
                                 .simulationDeltaTimeMs = frame.simulationDeltaTimeMs,
                                 .timeDiscontinuityId = frame.timeDiscontinuityId};
}

[[nodiscard]] inline auto ownerError(std::string_view operation) -> core::Error {
    return core::Error{"playback.session.not_owner_thread",
                       "PlaybackSession belongs to another thread"}
        .withContext("operation", std::string{operation});
}

[[nodiscard]] inline auto reentryError(std::string_view operation) -> core::Error {
    return core::Error{"playback.session.reentrant",
                       "PlaybackSession cannot be re-entered while an operation is active"}
        .withContext("operation", std::string{operation});
}

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
inline auto gameplayError(std::string_view detail) -> core::Error {
    return core::Error{"playback.gameplay.invalid", std::string{detail}}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}

inline auto prepareProjection(const detail::GameplayContentStorage& content,
                              const SnapshotLayout& layout)
    -> core::Result<detail::PreparedProjection> {
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
    // Product tag: the projection index maps are session-scoped and capped as state.index.
    const internal::pd::memory::TagScope tag{internal::pd::memory::ownerMain,
                                             internal::pd::memory::domainIndex};
#endif
    for (const auto& requirement :
         content.capsule.gameplay.assembled().graph.derivedCapabilities.capabilities) {
        if (std::ranges::none_of(detail::PresentationProjectionAdapter::capabilities,
                                 [&](const auto& supported) {
                                     return supported.first == requirement.capabilityId &&
                                            supported.second == requirement.revision;
                                 }))
            return core::unexpected(
                core::Error{"capability.unknown", "Unsupported presentation binding capability"}
                    .withContext("category", "unknown_capability")
                    .withContext("severity", "error")
                    .withContext("faulted", "false")
                    .withContext("capabilityId", requirement.capabilityId)
                    .withContext("fieldPath", "derivedCapabilities.capabilityId"));
    }
    std::map<std::string, std::size_t, std::less<>> targets;
    for (std::size_t i = 0; i < layout.entities.size(); ++i)
        if (layout.entities[i].mesh)
            targets.emplace(layout.entities[i].id.value, i);
    return detail::PresentationProjectionAdapter::prepare(content.bindings, std::move(targets));
}
#endif

class SessionOperation final {
  public:
    explicit SessionOperation(bool& active) noexcept : active_(active) {
        active_ = true;
    }

    ~SessionOperation() noexcept {
        active_ = false;
    }

    SessionOperation(const SessionOperation&) = delete;
    auto operator=(const SessionOperation&) -> SessionOperation& = delete;

  private:
    bool& active_;
};

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
[[nodiscard]] auto makeGameplaySession(const GameplayContent& content)
    -> core::Result<judgement::JudgementSession>;

[[nodiscard]] auto internalContext(GameplayExecutionContext context) -> judgement::RpaContext;

[[nodiscard]] auto canonicalOccurrence(const GameplayOccurrence& occurrence,
                                       const judgement::SessionConfiguration& config)
    -> core::Result<judgement::CanonicalInput>;

[[nodiscard]] auto publicDecision(const judgement::AdvanceDecision& decision)
    -> GameplayAdvanceDecision;

// Shared by the gameplay and gameplay Seek translation units.
[[nodiscard]] auto publicTick(std::optional<judgement::Tick> tick) -> std::optional<GameplayTick>;
#endif

struct PlaybackSession::State final {
    State() = default;

    core::ThreadChecker ownerThread;
    // ResourceManager must outlive RuntimeSession and its ResourceScope.
    std::shared_ptr<content::IContentProvider> contentProvider;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::string activeChartJson;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    SnapshotLayout snapshotLayout;
    internal::presentation::RevisionClock presentationClock;
    const std::uint64_t presentationSessionScope{allocatePlaybackSessionToken()};
    std::optional<detail::CommittedPresentationState> committedPresentation;
    void publishPresentation(internal::presentation::Mutation& mutation,
                             detail::CommittedPresentationState descriptor) noexcept {
        mutation.publish();
        if (!runtimeSession) {
            committedPresentation.reset();
            return;
        }
        const auto runtime = runtime::detail::PresentationAccess::installedState(*runtimeSession);
        descriptor.sessionScope = presentationSessionScope;
        descriptor.resultVersion = presentationClock.version;
        descriptor.contentRevision = presentationClock.layout;
        descriptor.layoutRevision = presentationClock.layout;
        descriptor.worldRevision = runtime.stamp.version;
        descriptor.updateOrdinal = presentationClock.updates;
        descriptor.runtime = runtime.stamp;
        committedPresentation = descriptor;
    }
    std::optional<RuntimeFrame> lastFrame;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<ChartInfo> activeChartInfo;
    std::optional<PlaybackContentInfo> activeContentInfo;
    std::optional<PlaybackMode> activeMode;
    std::optional<PreparedSemanticIdentity> semanticIdentity;
    std::optional<chart::CandidateRuntimeMetadata> candidateMetadata;
    ChartParameterSet parameters;
    std::uint64_t sessionToken{allocatePlaybackSessionToken()};
    std::uint64_t generation{1};
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    std::optional<ExecutionFence> hostFence;
    std::optional<ControlRequest> acceptedHostControl;
    std::uint64_t hostDecision{};
    std::optional<std::uint64_t> acceptedHostDecision;
    std::optional<HostContext> confirmedHostContext;
    bool hostCommitAuthorized{};
#endif
    void bumpGeneration() noexcept {
        if (++generation == 0) {
            sessionToken = allocatePlaybackSessionToken();
            generation = 1;
        }
    }

    SessionState sessionState{SessionState::Empty};
    bool operationActive{};
    PlaybackCapabilitySet capabilities;
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t nextCandidateGeneration{1};
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    GameplayContent gameplayContent;
    std::unique_ptr<judgement::JudgementSession> gameplay;
    GameplayState gameplayState{GameplayState::Inactive};
    std::unique_ptr<judgement::ExecutionScheduler> executionScheduler;
    // Transfer only across explicit state replacement; the active scheduler owns the frontier.
    std::optional<std::uint64_t> pendingRetiredRequestFloor;
    std::optional<RealtimeClock> realtimeClock;
    std::optional<GameplayExecutionContext> executionContext;
    std::optional<ClockMappingConfiguration> executionMapping;
    std::optional<TailClockTransition> tailTransition;
    bool realtimeWasStarted{};
    auto syncExecutionState() -> core::Result<void> {
        auto query = gameplay->query();
        if (!query)
            return core::unexpected(query.error());
        const auto previous = gameplayState;
        if (query->kernelView().state == judgement::KernelSessionState::Faulted)
            gameplayState = GameplayState::Faulted;
        else if (executionScheduler->suspended() && gameplayState != GameplayState::ResumeAborted)
            gameplayState = GameplayState::Suspended;
        if (gameplayState != previous)
            bumpGeneration();
        return {};
    }
    std::optional<GameplayAdvanceReceipt> gameplayReceipt;
    detail::PreparedProjection gameplayProjectionIndex;
    detail::ProjectionState gameplayProjection;
#endif
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
    ChartParameterSet parameters;
    PreparedSemanticIdentity semanticIdentity;
    std::optional<chart::CandidateRuntimeMetadata> candidateMetadata;
    std::optional<assets::AudioSourceLease> audioSourceLease;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<RuntimeFrame> targetFrame;
    SessionState committedState{SessionState::Ready};
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t candidateGeneration{};
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    GameplayContent gameplayContent;
    std::unique_ptr<judgement::JudgementSession> gameplay;
    detail::PreparedProjection gameplayProjectionIndex;
#endif
};

} // namespace cuexis::playback
