#include "host_candidate.hpp"
#include "capture_route.hpp"
#include <cuexis_internal/sha256.hpp>
namespace cuexis::playback::detail {
namespace {
auto matchingStep(const ControlReceipt& proof, Participant participant, StepPhase phase)
    -> const StepReceipt* {
    const auto& step =
        proof.steps[static_cast<unsigned>(participant)][static_cast<unsigned>(phase)];
    if (!step || step->status != StepStatus::applied ||
        step->request.target != proof.request.target ||
        step->request.key.operationId != proof.request.operationId ||
        step->request.key.participant != participant || step->request.key.phase != phase ||
        step->request.key.sessionScope != proof.request.context.sessionScope)
        return nullptr;
    return &*step;
}
auto softwareReceipt(PlaybackSession& session, OwnerStepLedger& ledger, const StepRequest& request,
                     const core::Result<void>& result) -> core::Result<StepReceipt> {
    StepReceipt receipt{request,
                        result ? StepStatus::applied : StepStatus::failed,
                        {},
                        result ? std::optional<core::Error>{} : result.error()};
    auto effects = session.executionStepEffects();
    if (!effects)
        return core::unexpected(effects.error());
    receipt.effects = *effects;
    auto completed = ledger.complete(receipt);
    if (!completed)
        return core::unexpected(completed.error());
    return receipt;
}
} // namespace
auto HostCoordinationAccess::materialize(PreparedPlayback& prepared,
                                         const PresentationCapabilities& capabilities,
                                         const PresentationRequest& request,
                                         std::uint64_t rendererInstance,
                                         std::uint64_t rendererGeneration, std::uint64_t sequence)
    -> core::Result<PresentationCandidateInput> {
    return prepared.materializePresentation(capabilities, request, rendererInstance,
                                            rendererGeneration, sequence);
}
auto HostCoordinationAccess::enterFence(PlaybackSession& session,
                                        const PresentationCandidateInput& input)
    -> core::Result<CommitFence> {
    return session.enterExecutionFence(input);
}
auto HostCoordinationAccess::commit(PlaybackSession& session, PreparedPlayback&& prepared,
                                    CommitFence fence, const ControlReceipt& ready)
    -> core::Result<void> {
    return session.commitExecutionCandidate(std::move(prepared), fence, ready);
}
auto HostCoordinationAccess::leaveFence(PlaybackSession& session, CommitFence fence,
                                        const ControlReceipt& confirmed) -> core::Result<void> {
    return session.releaseExecutionFence(fence, confirmed);
}
auto HostCoordinationAccess::validateControlBinding(const PlaybackSession& session,
                                                    const StepRequest& request)
    -> core::Result<void> {
    return session.validateExecutionControlBinding(request);
}
auto HostCoordinationAccess::validateFrameContext(const PlaybackSession& session,
                                                  const HostContext& context)
    -> core::Result<void> {
    return session.validateExecutionFrameContext(context);
}
auto HostCoordinationAccess::acceptControlAtOwner(PlaybackSession& session,
                                                  const ControlRequest& request)
    -> core::Result<std::uint64_t> {
    return session.acceptExecutionControl(request);
}
auto HostCoordinationAccess::enterControlFence(PlaybackSession& session,
                                               const ControlReceipt& proof)
    -> core::Result<CommitFence> {
    return session.enterExecutionFence(proof);
}
auto HostCoordinationAccess::currentFence(const PlaybackSession& session)
    -> core::Result<CommitFence> {
    return session.executionFence();
}
auto HostCoordinationAccess::completeClose(PlaybackSession& session, CommitFence fence,
                                           const ControlReceipt& proof) -> core::Result<void> {
    return session.completeExecutionClose(fence, proof);
}
auto HostCoordinationAccess::revokeRealtimePermission(PlaybackSession& session)
    -> core::Result<void> {
    return session.invalidateGameplayClock();
}
auto HostCoordinationAccess::mapRealtimeClock(PlaybackSession& session,
                                              const ClockObservation& clock, std::int64_t now)
    -> core::Result<ClockTicks> {
    return session.mapGameplayClock(clock, now);
}
auto HostCoordinationAccess::archiveCut(const PlaybackSession& session,
                                        const GameplayReplay& replay, GameplayTick horizon)
    -> core::Result<GameplayReplayCut> {
    return session.gameplayReplayCut(replay, horizon);
}
auto HostCoordinationAccess::feedbackMembership(const PlaybackSession& session, std::uint64_t scope)
    -> core::Result<std::vector<FeedbackObservation>> {
    return session.gameplayFeedback(scope);
}
auto HostCoordinationAccess::samplePresentation(PlaybackSession& session,
                                                GameplayPresentationTick tick,
                                                const RuntimeFrame& frame) -> core::Result<void> {
    return session.sampleGameplayPresentation(tick, frame);
}
auto HostCoordinationAccess::generation(const PlaybackSession& session)
    -> core::Result<std::uint64_t> {
    auto owner = session.executionOwnerState();
    if (!owner)
        return core::unexpected(owner.error());
    return owner->generation;
}
auto HostCoordinationAccess::sessionScope(const PlaybackSession& session)
    -> core::Result<std::uint64_t> {
    auto owner = session.executionOwnerState();
    if (!owner)
        return core::unexpected(owner.error());
    return owner->sessionScope;
}
auto HostCoordinationAccess::currentRuntimeFrame(const PlaybackSession& session)
    -> core::Result<RuntimeFrame> {
    auto owner = session.executionOwnerState();
    if (!owner)
        return core::unexpected(owner.error());
    if (!owner->runtimeFrame)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "No committed Runtime frame"});
    return *owner->runtimeFrame;
}
auto HostCoordinationAccess::reconciliationOrdinal(const PlaybackSession& session)
    -> core::Result<std::uint64_t> {
    auto owner = session.executionOwnerState();
    if (!owner)
        return core::unexpected(owner.error());
    if (!owner->reconciliationOrdinal)
        return core::unexpected(core::Error{"playback.gameplay.invalid", "RPA profile is absent"});
    return *owner->reconciliationOrdinal;
}
auto HostCoordinationAccess::resumeIdentity(const PlaybackSession& session,
                                            const GameplayReconciliation& r)
    -> core::Result<PreparedSemanticIdentity> {
    auto owner = session.executionOwnerState();
    if (!owner)
        return core::unexpected(owner.error());
    auto gameplay = session.queryGameplay();
    if (!gameplay)
        return core::unexpected(gameplay.error());
    return reconciliationIdentity(r);
}
auto HostCoordinationAccess::acceptControl(PlaybackSession& session, HostCoordinator& host,
                                           const ControlRequest& request)
    -> core::Result<ControlReceipt> {
    auto valid = host.preflight(request);
    if (!valid)
        return core::unexpected(valid.error());
    if (!*valid)
        return host.accept(request, 0);
    auto decision = acceptControlAtOwner(session, request);
    if (!decision)
        return core::unexpected(decision.error());
    return host.accept(request, *decision);
}
auto HostCoordinationAccess::commitOnce(PlaybackSession& session, OwnerStepLedger& ledger,
                                        const StepRequest& request, PreparedPlayback&& prepared,
                                        CommitFence fence, const ControlReceipt& proof)
    -> core::Result<StepReceipt> try {
    auto snapshot = session.executionOwnerState();
    if (!snapshot)
        return core::unexpected(snapshot.error());
    const auto& s = *snapshot;
    auto old = ledger.query(request.key);
    if (old && old->status != StepStatus::pending)
        return ledger.apply(request);
    if (!s.acceptedControl || *s.acceptedControl != proof.request ||
        (proof.request.kind != ControlKind::load && proof.request.kind != ControlKind::reload) ||
        request.key.participant != Participant::software ||
        request.key.phase != StepPhase::softwareCommit ||
        proof.request.operationId != request.key.operationId ||
        proof.request.target != request.target || !controlReadyForCommit(proof))
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Owners are not prepared and fenced"});
    auto started = ledger.start(request);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return ledger.query(request.key);
    auto result = commit(session, std::move(prepared), fence, proof);
    StepReceipt receipt{request,
                        result ? StepStatus::applied : StepStatus::failed,
                        {},
                        result ? std::optional<core::Error>{} : result.error()};
    auto actualGeneration = generation(session);
    if (!actualGeneration)
        return core::unexpected(actualGeneration.error());
    receipt.effects.generation = *actualGeneration;
    auto published = ledger.complete(receipt);
    if (!published)
        return core::unexpected(published.error());
    return receipt;
} catch (...) {
    ledger.reportAllocationFailure(request.key);
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Software commit receipt reservation failed"});
}
auto HostCoordinationAccess::retireSoftware(PlaybackSession& session, OwnerStepLedger& ledger,
                                            const StepRequest& request, const ControlReceipt& proof)
    -> core::Result<StepReceipt> try {
    auto snapshot = session.executionOwnerState();
    if (!snapshot)
        return core::unexpected(snapshot.error());
    const auto& s = *snapshot;
    auto old = ledger.query(request.key);
    if (old && old->status != StepStatus::pending)
        return ledger.apply(request);
    const bool directedCleanup = proof.request.kind == ControlKind::close ||
                                 proof.status == ControlStatus::suspended ||
                                 proof.status == ControlStatus::failed;
    if (!s.acceptedControl || !directedCleanup || *s.acceptedControl != proof.request ||
        request.target != proof.request.target ||
        request.key.operationId != proof.request.operationId ||
        request.key.participant != Participant::software || request.key.phase != StepPhase::retire)
        return core::unexpected(core::Error{"host.rpa.context_stale",
                                            "Software retirement lacks accepted directed cleanup"});
    if (proof.request.participants[0] && !matchingStep(proof, Participant::capture, StepPhase::cut))
        return core::unexpected(
            core::Error{"host.rpa.device_pending", "Capture route remains active"});
    if (proof.request.participants[2]) {
        const auto* isolation = matchingStep(proof, Participant::audio, StepPhase::isolate);
        if (!matchingStep(proof, Participant::audio, StepPhase::stop) || !isolation ||
            !isolation->effects.callbackIsolated)
            return core::unexpected(core::Error{"host.rpa.device_pending",
                                                "Audio callback still retains software resources"});
    }
    auto started = ledger.start(request);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return ledger.query(request.key);
    auto result = session.retireExecutionStep(request, proof);
    StepReceipt receipt{request,
                        result ? StepStatus::applied : StepStatus::failed,
                        {},
                        result ? std::optional<core::Error>{} : result.error()};
    auto actualGeneration = generation(session);
    if (!actualGeneration)
        return core::unexpected(actualGeneration.error());
    receipt.effects.generation = *actualGeneration;
    receipt.effects.referencesReleased = bool(result);
    receipt.effects.callbackIsolated = true;
    auto published = ledger.complete(receipt);
    if (!published)
        return core::unexpected(published.error());
    return receipt;
} catch (...) {
    ledger.reportAllocationFailure(request.key);
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Software retirement publication failed"});
}
auto HostCoordinationAccess::applyPause(PlaybackSession& session, OwnerStepLedger& ledger,
                                        const StepRequest& request, std::int64_t now)
    -> core::Result<StepReceipt> try {
    if (request.key.participant != Participant::software ||
        request.key.phase != StepPhase::softwareCommit)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Invalid software Pause step"});
    auto snapshot = session.executionOwnerState();
    if (!snapshot)
        return core::unexpected(snapshot.error());
    const auto& s = *snapshot;
    auto previous = ledger.query(request.key);
    if (previous && previous->status != StepStatus::pending)
        return ledger.apply(request);
    if (!s.acceptedControl || s.acceptedControl->kind != ControlKind::pause ||
        s.acceptedControl->operationId != request.key.operationId ||
        s.acceptedControl->target != request.target || !s.pendingEpochEnd ||
        request.target.tick != s.pendingEpochEnd->inclusiveTick.value ||
        request.target.pauseEpoch != s.pendingEpochEnd->pauseEpoch)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Software Pause lacks the accepted cut"});
    if (!s.fence || s.fence->scope() != request.key.sessionScope ||
        s.fence->controlSequence() != request.target.context.controlSequence ||
        s.fence->candidateId() != request.target.candidateId)
        return core::unexpected(
            core::Error{"host.rpa.device_pending", "Software Pause requires the control fence"});
    auto started = ledger.start(request);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return ledger.query(request.key);
    if (request.target.tailPauseAt && *request.target.tailPauseAt > now)
        return core::unexpected(
            core::Error{"host.rpa.clock_invalid", "Tail pause action is in the future"});
    return softwareReceipt(
        session, ledger, request,
        session.pauseExecutionStep(request, request.target.tailPauseAt.value_or(now)));
} catch (...) {
    ledger.reportAllocationFailure(request.key);
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Software Pause publication failed"});
}
auto HostCoordinationAccess::applyResume(PlaybackSession& session, OwnerStepLedger& ledger,
                                         const StepRequest& request,
                                         const GameplayReconciliation& reconciliation,
                                         GameplayExecutionContext context,
                                         const ClockObservation& clock, std::int64_t now,
                                         const StepReceipt* routeReceipt)
    -> core::Result<StepReceipt> try {
    if (!request.target.nextContext ||
        request.target.nextContext->sessionScope != context.sessionScope ||
        request.target.nextContext->clockSegment != context.clockSegment ||
        request.target.nextContext->runEpoch != context.runEpoch ||
        request.target.tick != reconciliation.cutTick.value ||
        request.target.pauseEpoch != reconciliation.pauseEpoch ||
        (request.target.cut && request.target.cut != reconciliation.finalTraceCut) ||
        (request.target.reconciliationOrdinal &&
         request.target.reconciliationOrdinal != reconciliation.controlOrdinal) ||
        (request.resumeTrace && request.resumeTrace->finalCut != reconciliation.finalTraceCut) ||
        request.key.participant != Participant::software ||
        request.key.phase != StepPhase::softwareCommit)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Invalid software Resume target"});
    auto snapshot = session.executionOwnerState();
    if (!snapshot)
        return core::unexpected(snapshot.error());
    const auto& s = *snapshot;
    auto previous = ledger.query(request.key);
    if (previous && previous->status != StepStatus::pending) {
        auto exact = resumeIdentity(session, reconciliation);
        if (!exact || (request.resumeTrace ? std::optional{request.resumeTrace->identity}
                                           : request.target.payloadIdentity) != *exact) {
            ledger.reportConflict(request.key);
            return core::unexpected(
                core::Error{"host.rpa.message_conflict", "Repeated Resume trace differs"});
        }
        return ledger.apply(request);
    }
    if (!s.acceptedControl || s.acceptedControl->kind != ControlKind::resume ||
        s.acceptedControl->operationId != request.key.operationId ||
        s.acceptedControl->target != request.target)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Software Resume lacks accepted control"});
    if (!routeReceipt || routeReceipt->status != StepStatus::applied ||
        reconciliation.trace.size() < reconciliation.finalTraceCut)
        return ledger.apply(request);
    if (routeReceipt->request.key.participant != Participant::capture ||
        routeReceipt->request.key.phase != StepPhase::resumeRoute ||
        routeReceipt->request.key.operationId != request.key.operationId ||
        routeReceipt->request.key.sessionScope != request.key.sessionScope ||
        routeReceipt->request.target != request.target ||
        routeReceipt->effects.cut != reconciliation.finalTraceCut ||
        routeReceipt->effects.pauseEpoch != reconciliation.pauseEpoch ||
        (request.resumeTrace &&
         routeReceipt->effects.payloadIdentity != request.resumeTrace->identity))
        return core::unexpected(core::Error{"host.rpa.message_conflict",
                                            "Final route proof differs from software trace"});
    auto identity = resumeIdentity(session, reconciliation);
    if (!identity)
        return core::unexpected(identity.error());
    if ((request.resumeTrace ? std::optional{request.resumeTrace->identity}
                             : request.target.payloadIdentity) != *identity) {
        ledger.reportConflict(request.key);
        return core::unexpected(
            core::Error{"host.rpa.message_conflict", "Software Resume trace identity differs"});
    }
    if (!s.fence || s.fence->scope() != request.key.sessionScope ||
        s.fence->controlSequence() != request.target.context.controlSequence ||
        s.fence->candidateId() != request.target.candidateId)
        return core::unexpected(
            core::Error{"host.rpa.device_pending", "Software Resume requires the control fence"});
    auto started = ledger.start(request);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return ledger.query(request.key);
    auto result =
        session.resumeExecutionStep(request, reconciliation, context, clock, now, routeReceipt);
    if (!result && result.error().code() == "host.rpa.control_pending")
        return ledger.query(request.key);
    return softwareReceipt(session, ledger, request, result);
} catch (...) {
    ledger.reportAllocationFailure(request.key);
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Software Resume publication failed"});
}
auto HostCoordinationAccess::applySeek(PlaybackSession& session, OwnerStepLedger& ledger,
                                       const StepRequest& request, const ControlReceipt& proof,
                                       const GameplayReplay& archive, GameplayTick horizon,
                                       const GameplayReplayCut& cut,
                                       std::span<const GameplayCheckpoint> checkpoints,
                                       GameplayPresentationTick tick, const RuntimeFrame& frame,
                                       SchedulingProfile profile, ClockMappingConfiguration mapping,
                                       const std::string& domain) -> core::Result<StepReceipt> try {
    auto snapshot = session.executionOwnerState();
    if (!snapshot)
        return core::unexpected(snapshot.error());
    const auto& s = *snapshot;
    auto bytes = archive.toBytes();
    if (!bytes)
        return core::unexpected(bytes.error());
    const PreparedSemanticIdentity identity{core::detail::sha256(*bytes)};
    if (request.target.archiveIdentity != identity || request.target.seekHorizon != horizon.value ||
        request.target.seekCompleteRecords != cut.completeRecords ||
        request.target.seekPartialHorizon !=
            (cut.partialHorizon ? std::optional{cut.partialHorizon->value} : std::nullopt))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Seek archive or cut identity mismatch"});
    auto previous = ledger.query(request.key);
    if (previous && previous->status != StepStatus::pending)
        return ledger.apply(request);
    if (!s.acceptedControl || *s.acceptedControl != proof.request ||
        proof.request.kind != ControlKind::seek || request.target != proof.request.target ||
        request.key.operationId != proof.request.operationId ||
        request.key.participant != Participant::software ||
        request.key.phase != StepPhase::softwareCommit ||
        request.target.archiveIdentity != identity || request.target.seekHorizon != horizon.value ||
        request.target.seekCompleteRecords != cut.completeRecords ||
        request.target.seekPartialHorizon !=
            (cut.partialHorizon ? std::optional{cut.partialHorizon->value} : std::nullopt) ||
        !request.target.nextContext || !s.fence || !controlReadyForCommit(proof))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Seek target or owning archive mismatch"});
    auto started = ledger.start(request);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return ledger.query(request.key);
    auto result = session.seekExecutionStep(request, proof, archive, horizon, cut, checkpoints,
                                            tick, frame, profile, mapping, domain);
    auto receipt = softwareReceipt(session, ledger, request, result);
    return receipt;
} catch (...) {
    ledger.reportAllocationFailure(request.key);
    return core::unexpected(core::Error{"host.rpa.capacity_exceeded", "Seek publication failed"});
}
} // namespace cuexis::playback::detail
