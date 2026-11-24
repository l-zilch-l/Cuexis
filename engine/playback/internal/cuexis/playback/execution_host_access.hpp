#pragma once

// Build-only execution-host coordination entry points. This header is not installed and carries no
// SDK stability promise. Each function forwards to the same-version candidate PlaybackSession
// member named by the RR-4 compatibility table; the declarations in the installed public header
// keep their own compatibility windows. Only named in-repo host and test targets receive the
// private include root that exposes this file.
#include <cuexis/core/result.hpp>
#include <cuexis/playback/execution_control.hpp>
#include <cuexis/playback/execution_coordination.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_types.hpp>
#include <cuexis/playback/realtime_clock.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace cuexis::playback {
namespace detail {

struct ExecutionHostAccess final {
    // Execution fence and candidate commit.
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    enterExecutionFence(PlaybackSession& session, const PresentationCandidateInput& input)
        -> core::Result<ExecutionFence>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto enterExecutionFence(PlaybackSession& session,
                                                                      const ControlReceipt& proof)
        -> core::Result<ExecutionFence>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    commitExecutionCandidate(PlaybackSession& session, PreparedPlayback&& prepared,
                             ExecutionFence fence, const ControlReceipt& ready)
        -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    releaseExecutionFence(PlaybackSession& session, ExecutionFence fence,
                          const ControlReceipt& confirmed) -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto executionFence(const PlaybackSession& session)
        -> core::Result<ExecutionFence>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    completeExecutionClose(PlaybackSession& session, ExecutionFence fence,
                           const ControlReceipt& proof) -> core::Result<void>;

    // Control binding, proof and acceptance.
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    validateExecutionControlBinding(const PlaybackSession& session, const StepRequest& request)
        -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    validateExecutionControlProof(const PlaybackSession& session, const StepRequest& request,
                                  const ControlReceipt& proof) -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    validateExecutionFrameContext(const PlaybackSession& session, const HostContext& context)
        -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    acceptExecutionControl(PlaybackSession& session, const ControlRequest& request)
        -> core::Result<std::uint64_t>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    executionOwnerState(const PlaybackSession& session) -> core::Result<ExecutionOwnerState>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    executionStepEffects(const PlaybackSession& session) -> core::Result<StepEffects>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    executionAdvanceRetention(const PlaybackSession& session,
                              GameplayExecutionContext& identityContext,
                              std::optional<std::uint64_t>& lastAcceptedId,
                              std::optional<std::uint64_t>& reclaimedThrough) -> core::Result<void>;

    // Owner steps.
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto retireExecutionStep(PlaybackSession& session,
                                                                      const StepRequest& request,
                                                                      const ControlReceipt& proof)
        -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    pauseExecutionStep(PlaybackSession& session, const StepRequest& request, std::int64_t now)
        -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto
    resumeExecutionStep(PlaybackSession& session, const StepRequest& request,
                        const GameplayReconciliation& reconciliation,
                        GameplayExecutionContext context, const ClockObservation& clock,
                        std::int64_t now, const StepReceipt* routeReceipt) -> core::Result<void>;
    [[nodiscard]] static CUEXIS_PLAYBACK_API auto seekExecutionStep(
        PlaybackSession& session, const StepRequest& request, const ControlReceipt& proof,
        const GameplayReplay& archive, GameplayTick horizon, const GameplayReplayCut& cut,
        std::span<const GameplayCheckpoint> checkpoints, GameplayPresentationTick tick,
        const RuntimeFrame& frame, SchedulingProfile profile, ClockMappingConfiguration mapping,
        const std::string& domain) -> core::Result<void>;
};

} // namespace detail
} // namespace cuexis::playback
