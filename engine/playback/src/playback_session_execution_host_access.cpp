// Execution-host coordination entry points defined in the experimental cuexis_playback library.
// The unit forward only; each forwarded member keeps its own validation order, state and
// diagnostics. It is compiled only by the candidate source table.

#include <cuexis/playback/execution_host_access.hpp>

namespace cuexis::playback::detail {

auto ExecutionHostAccess::enterExecutionFence(PlaybackSession& session,
                                              const PresentationCandidateInput& input)
    -> core::Result<ExecutionFence> {
    return session.enterExecutionFence(input);
}
auto ExecutionHostAccess::enterExecutionFence(PlaybackSession& session, const ControlReceipt& proof)
    -> core::Result<ExecutionFence> {
    return session.enterExecutionFence(proof);
}
auto ExecutionHostAccess::commitExecutionCandidate(PlaybackSession& session,
                                                   PreparedPlayback&& prepared,
                                                   ExecutionFence fence,
                                                   const ControlReceipt& ready)
    -> core::Result<void> {
    return session.commitExecutionCandidate(std::move(prepared), fence, ready);
}
auto ExecutionHostAccess::releaseExecutionFence(PlaybackSession& session, ExecutionFence fence,
                                                const ControlReceipt& confirmed)
    -> core::Result<void> {
    return session.releaseExecutionFence(fence, confirmed);
}
auto ExecutionHostAccess::executionFence(const PlaybackSession& session)
    -> core::Result<ExecutionFence> {
    return session.executionFence();
}
auto ExecutionHostAccess::completeExecutionClose(PlaybackSession& session, ExecutionFence fence,
                                                 const ControlReceipt& proof)
    -> core::Result<void> {
    return session.completeExecutionClose(fence, proof);
}
auto ExecutionHostAccess::validateExecutionControlBinding(const PlaybackSession& session,
                                                          const StepRequest& request)
    -> core::Result<void> {
    return session.validateExecutionControlBinding(request);
}
auto ExecutionHostAccess::validateExecutionControlProof(const PlaybackSession& session,
                                                        const StepRequest& request,
                                                        const ControlReceipt& proof)
    -> core::Result<void> {
    return session.validateExecutionControlProof(request, proof);
}
auto ExecutionHostAccess::validateExecutionFrameContext(const PlaybackSession& session,
                                                        const HostContext& context)
    -> core::Result<void> {
    return session.validateExecutionFrameContext(context);
}
auto ExecutionHostAccess::acceptExecutionControl(PlaybackSession& session,
                                                 const ControlRequest& request)
    -> core::Result<std::uint64_t> {
    return session.acceptExecutionControl(request);
}
auto ExecutionHostAccess::executionOwnerState(const PlaybackSession& session)
    -> core::Result<ExecutionOwnerState> {
    return session.executionOwnerState();
}
auto ExecutionHostAccess::executionStepEffects(const PlaybackSession& session)
    -> core::Result<StepEffects> {
    return session.executionStepEffects();
}
auto ExecutionHostAccess::executionAdvanceRetention(const PlaybackSession& session,
                                                    GameplayExecutionContext& identityContext,
                                                    std::optional<std::uint64_t>& lastAcceptedId,
                                                    std::optional<std::uint64_t>& reclaimedThrough)
    -> core::Result<void> {
    return session.executionAdvanceRetention(identityContext, lastAcceptedId, reclaimedThrough);
}
auto ExecutionHostAccess::retireExecutionStep(PlaybackSession& session, const StepRequest& request,
                                              const ControlReceipt& proof) -> core::Result<void> {
    return session.retireExecutionStep(request, proof);
}
auto ExecutionHostAccess::pauseExecutionStep(PlaybackSession& session, const StepRequest& request,
                                             std::int64_t now) -> core::Result<void> {
    return session.pauseExecutionStep(request, now);
}
auto ExecutionHostAccess::resumeExecutionStep(PlaybackSession& session, const StepRequest& request,
                                              const GameplayReconciliation& reconciliation,
                                              GameplayExecutionContext context,
                                              const ClockObservation& clock, std::int64_t now,
                                              const StepReceipt* routeReceipt)
    -> core::Result<void> {
    return session.resumeExecutionStep(request, reconciliation, context, clock, now, routeReceipt);
}
auto ExecutionHostAccess::seekExecutionStep(
    PlaybackSession& session, const StepRequest& request, const ControlReceipt& proof,
    const GameplayReplay& archive, GameplayTick horizon, const GameplayReplayCut& cut,
    std::span<const GameplayCheckpoint> checkpoints, GameplayPresentationTick tick,
    const RuntimeFrame& frame, SchedulingProfile profile, ClockMappingConfiguration mapping,
    const std::string& domain) -> core::Result<void> {
    return session.seekExecutionStep(request, proof, archive, horizon, cut, checkpoints, tick,
                                     frame, profile, mapping, domain);
}

} // namespace cuexis::playback::detail
