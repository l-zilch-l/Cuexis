// PlaybackSession execution control: the execution fence, the host control proof and the step
// commands that retire, pause, resume and seek through it.

#include "playback_session_internal.hpp"

namespace cuexis::playback {

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
namespace {
struct HostMutationPermission final {
    bool& flag;
    bool previous;
    explicit HostMutationPermission(bool& value) : flag(value), previous(value) {
        flag = true;
    }
    ~HostMutationPermission() {
        flag = previous;
    }
};
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
} // namespace
auto computeGameplayReconciliationIdentity(const GameplayReconciliation& r)
    -> core::Result<PreparedSemanticIdentity> try {
    core::detail::Sha256 hash;
    const auto integer = [&](std::uint64_t n) {
        std::array<std::byte, 8> b{};
        for (std::size_t i = 0; i < 8; ++i)
            b[i] = std::byte((n >> (i * 8)) & 255);
        hash.update(b);
    };
    const auto text = [&](std::string_view value) {
        integer(value.size());
        hash.update(std::as_bytes(std::span{value.data(), value.size()}));
    };
    text("cuexis.host.resume.target.v1");
    integer(r.cutTick.value);
    integer(r.controlOrdinal);
    integer(r.pauseEpoch);
    integer(r.finalTraceCut);
    for (const auto& t : r.trace) {
        const auto& in = t.occurrence.input;
        integer(t.sequence);
        integer(in.observationTick.value);
        integer(t.occurrence.sourceLocalPosition);
        text(in.domain);
        text(in.sourceClass);
        text(in.channel);
        integer(static_cast<std::uint64_t>(in.action));
        integer(in.amount.has_value());
        if (in.amount) {
            integer(in.amount->numerator);
            integer(in.amount->denominator);
        }
        integer(in.sequence);
        integer(in.crossedSamplingGap);
        integer(in.reconnected);
        integer(in.droppedSamples);
        integer(in.rawTimestamps.device);
        integer(in.rawTimestamps.hostArrival);
        integer(in.rawTimestamps.audioFrame);
        integer(in.rawTimestamps.renderFrame);
    }
    return PreparedSemanticIdentity{hash.finish()};
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Resume identity reservation failed"});
}

auto PlaybackSession::enterExecutionFence(const PresentationCandidateInput& input)
    -> core::Result<ExecutionFence> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("enter_fence"));
    if (s.operationActive || s.hostFence || input.playbackOwnerToken != s.sessionToken ||
        input.mutationGeneration != s.generation || !input.candidateId ||
        !input.context.controlSequence || s.generation == UINT64_MAX)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Prepared generation changed before fence"});
    s.hostFence = ExecutionFence{input.context.sessionScope, s.generation, input.candidateId,
                                 input.context.controlSequence};
    return *s.hostFence;
}
auto PlaybackSession::commitExecutionCandidate(PreparedPlayback&& prepared, ExecutionFence fence,
                                               const ControlReceipt& ready) -> core::Result<void> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("host_commit"));
    if (!s.hostFence || *s.hostFence != fence || !prepared.valid() ||
        prepared.state_->candidateGeneration != fence.candidateId_ ||
        ready.request.context.sessionScope != fence.scope_ ||
        ready.request.context.controlSequence != fence.controlSequence_ ||
        ready.request.target.candidateId != fence.candidateId_ || !controlReadyForCommit(ready) ||
        generationFor(ready.request.target, Participant::software) != fence.generation_ ||
        ready.request.target.nextContext.value_or(ready.request.target.context).contentIdentity !=
            prepared.state_->semanticIdentity ||
        ready.request.target.nextContext.value_or(ready.request.target.context)
                .resourceGeneration != prepared.state_->candidateGeneration)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Commit fence identity mismatch"});
    struct Permission final {
        bool& flag;
        ~Permission() {
            flag = false;
        }
    } permission{s.hostCommitAuthorized};
    s.hostCommitAuthorized = true;
    return session.commit(std::move(prepared));
}
auto PlaybackSession::releaseExecutionFence(ExecutionFence fence, const ControlReceipt& confirmed)
    -> core::Result<void> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("leave_fence"));
    if (!s.hostFence || *s.hostFence != fence)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Old receipt cannot release fence"});
    if (confirmed.request.context.sessionScope != fence.scope_ ||
        confirmed.request.context.controlSequence != fence.controlSequence_ ||
        confirmed.request.target.candidateId != fence.candidateId_ ||
        !controlFullyApplied(confirmed))
        return core::unexpected(
            core::Error{"host.rpa.device_pending", "Participating owners not confirmed"});
    s.confirmedHostContext = confirmed.finalizedContext.value_or(
        confirmed.request.target.nextContext.value_or(confirmed.request.context));
    s.hostFence.reset();
    return {};
}
auto PlaybackSession::validateExecutionControlBinding(const StepRequest& request) const
    -> core::Result<void> {
    auto& session = *this;
    const auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("control_binding"));
    const bool participant = request.key.participant == Participant::software;
    const bool scope = s.acceptedHostControl &&
                       request.key.sessionScope == s.acceptedHostControl->context.sessionScope;
    const bool operation =
        s.acceptedHostControl && request.key.operationId == s.acceptedHostControl->operationId;
    const bool target = s.acceptedHostControl && request.target == s.acceptedHostControl->target;
    const bool accepted = s.acceptedHostControl.has_value();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink) {
        sink->bindingChecked = true;
        sink->bindingOwner = true;
        sink->bindingParticipant = participant;
        sink->bindingScope = scope;
        sink->bindingOperation = operation;
        sink->bindingTarget = target;
        if (!accepted) {
            sink->bindingChecked = true;
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingMissing);
        } else if (!participant)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingParticipant);
        else if (!scope)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingScope);
        else if (!operation)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingOperation);
        else if (!target)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingTarget);
    }
#endif
    if (!accepted || !participant || !scope || !operation || !target)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Software step lacks accepted target"});
    return {};
}
auto PlaybackSession::validateExecutionControlProof(const StepRequest& request,
                                                    const ControlReceipt& proof) const
    -> core::Result<void> {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    if (!state_->acceptedHostDecision || proof.decisionOrdinal != *state_->acceptedHostDecision ||
        proof.request != *state_->acceptedHostControl)
        return core::unexpected(core::Error{
            "host.rpa.context_stale", "Control proof differs from the accepted owner decision"});
    return {};
}

auto PlaybackSession::validateExecutionFrameContext(const HostContext& context) const
    -> core::Result<void> {
    auto& session = *this;
    const auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("frame_context"));
    if (s.hostFence || !s.confirmedHostContext || *s.confirmedHostContext != context)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Frame lacks a confirmed full context"});
    return {};
}
auto PlaybackSession::acceptExecutionControl(const ControlRequest& request)
    -> core::Result<std::uint64_t> try {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("host_control"));
    if (s.operationActive)
        return core::unexpected(reentryError("host_control"));
    if (s.acceptedHostControl && s.acceptedHostDecision &&
        request.context.sessionScope == s.acceptedHostControl->context.sessionScope &&
        request.operationId == s.acceptedHostControl->operationId) {
        if (request != *s.acceptedHostControl)
            return core::unexpected(
                core::Error{"host.rpa.message_conflict", "Control identity conflict"});
        return *s.acceptedHostDecision;
    }
    if (request.context != request.target.context || !request.context.sessionScope ||
        !request.operationId || request.commandOrdinal != request.target.commandOrdinal ||
        request.commandOrdinal == UINT64_MAX ||
        (s.acceptedHostControl && request.commandOrdinal <= s.acceptedHostControl->commandOrdinal))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Control target or ordinal invalid"});
    if (s.confirmedHostContext &&
        (request.context.sessionScope != s.confirmedHostContext->sessionScope ||
         request.context.resourceGeneration != s.confirmedHostContext->resourceGeneration ||
         request.context.rendererGeneration != s.confirmedHostContext->rendererGeneration ||
         request.context.contentIdentity != s.confirmedHostContext->contentIdentity ||
         request.context.controlSequence <= s.confirmedHostContext->controlSequence))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Confirmed owner identity mismatch"});
    // Validate a new Close against its admission generation. Legitimate post-cut delivery can
    // advance the owner before retire; a retransmitted accepted request returned above keeps
    // its original binding and decision.
    if (request.kind == ControlKind::close &&
        generationFor(request.target, Participant::software) != s.generation)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Close admission generation is stale"});
    if (request.kind == ControlKind::close && s.hostFence &&
        request.context.controlSequence <= s.hostFence->controlSequence())
        return core::unexpected(core::Error{
            "host.rpa.context_stale", "A new Close cannot reuse an older control fence identity"});
    // Reserve the complete binding before K1 changes any execution state.
    // GCC inlines the copy of the engaged optional<ReplaySourceIdentity> (a shared_ptr holder) and
    // then reports reads of its control block as possibly uninitialised. The value is a complete
    // copy of a const request that was fully constructed by the caller, so this is a false
    // positive; the diagnostic is disabled around this one statement instead of the whole file.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    auto binding = request;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (s.hostFence && request.kind != ControlKind::close)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Only Close can retire an unresolved fence"});
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (s.executionScheduler && s.executionContext) {
        const auto& current = *s.executionContext;
        const bool exact = request.context.sessionScope == current.sessionScope &&
                           request.context.clockSegment == current.clockSegment &&
                           request.context.runEpoch == current.runEpoch;
        const bool abortedRoute =
            request.kind == ControlKind::close && s.acceptedHostControl &&
            s.acceptedHostControl->kind == ControlKind::resume &&
            s.acceptedHostControl->target.nextContext &&
            request.context.sessionScope ==
                s.acceptedHostControl->target.nextContext->sessionScope &&
            request.context.clockSegment ==
                s.acceptedHostControl->target.nextContext->clockSegment &&
            request.context.runEpoch == s.acceptedHostControl->target.nextContext->runEpoch;
        if (!exact && !abortedRoute)
            return core::unexpected(
                core::Error{"host.rpa.context_stale", "Control execution context mismatch"});
        if ((request.kind == ControlKind::pause &&
             (!request.target.tick || !request.target.pauseEpoch)) ||
            (request.kind == ControlKind::resume &&
             (!request.target.tick || !request.target.pauseEpoch || !request.target.nextContext ||
              (!request.target.reconciliationOrdinal &&
               (!request.target.payloadIdentity || !request.target.cut)))))
            return core::unexpected(
                core::Error{"host.rpa.context_stale", "Control target is incomplete"});
        // Close cancels outstanding work even when ordinary Gameplay is faulted or fenced.
        auto accepted = request.kind == ControlKind::close
                            ? s.executionScheduler->acceptControl(
                                  judgement::RpaContext{current.sessionScope, current.clockSegment,
                                                        current.runEpoch},
                                  request.commandOrdinal)
                            : session.acceptGameplayControl(current, request.commandOrdinal);
        if (!accepted)
            return core::unexpected(accepted.error());
        const auto decision = *s.executionScheduler->lastDecisionOrdinal();
        if (decision == UINT64_MAX)
            return core::unexpected(
                core::Error{"host.rpa.capacity_exceeded", "Host decision exhausted"});
        s.hostDecision = decision + 1;
        s.acceptedHostControl = std::move(binding);
        s.acceptedHostDecision = decision;
        return decision;
    }
#endif
    if (request.context.sessionScope != s.sessionToken || s.hostDecision == UINT64_MAX)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Software owner scope differs"});
    const auto decision = s.hostDecision++;
    s.acceptedHostControl = std::move(binding);
    s.acceptedHostDecision = decision;
    return decision;
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Host control publication failed"});
}
auto PlaybackSession::enterExecutionFence(const ControlReceipt& proof)
    -> core::Result<ExecutionFence> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("control_fence"));
    const bool operationActive = s.operationActive;
    const bool existingFence = s.hostFence.has_value();
    const bool noAcceptedControl = !s.acceptedHostControl;
    const bool requestMismatch = !noAcceptedControl && *s.acceptedHostControl != proof.request;
    const bool statusMismatch = proof.status != ControlStatus::preparing;
    const bool typeMismatch =
        proof.request.kind == ControlKind::load || proof.request.kind == ControlKind::reload;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink) {
        using Predicate = internal::execution_diagnostics::rpa::FencePredicate;
        sink->identity.valid = true;
        sink->identity.scope = proof.request.context.sessionScope;
        sink->identity.candidateId = proof.request.target.candidateId;
        sink->identity.controlSequence = proof.request.context.controlSequence;
        sink->identity.operationId = proof.request.operationId;
        sink->identity.kind = static_cast<std::uint8_t>(proof.request.kind);
        sink->identity.status = static_cast<std::uint8_t>(proof.status);
        if (operationActive)
            internal::execution_diagnostics::rpa::reject(Predicate::operationActive);
        if (existingFence)
            internal::execution_diagnostics::rpa::reject(Predicate::existingFence);
        if (noAcceptedControl)
            internal::execution_diagnostics::rpa::reject(Predicate::noAcceptedControl);
        if (requestMismatch)
            internal::execution_diagnostics::rpa::reject(Predicate::requestMismatch);
        if (statusMismatch)
            internal::execution_diagnostics::rpa::reject(Predicate::statusMismatch);
        if (typeMismatch)
            internal::execution_diagnostics::rpa::reject(Predicate::typeMismatch);
        if (operationActive || existingFence || noAcceptedControl || requestMismatch ||
            statusMismatch || typeMismatch)
            internal::execution_diagnostics::rpa::noteReason(
                operationActive ? internal::execution_diagnostics::rpa::Reason::operationActive
                : existingFence ? internal::execution_diagnostics::rpa::Reason::existingFence
                : noAcceptedControl
                    ? internal::execution_diagnostics::rpa::Reason::noAcceptedControl
                : requestMismatch ? internal::execution_diagnostics::rpa::Reason::requestMismatch
                : statusMismatch  ? internal::execution_diagnostics::rpa::Reason::statusMismatch
                                  : internal::execution_diagnostics::rpa::Reason::typeMismatch);
    }
#endif
    if (operationActive || existingFence || noAcceptedControl || requestMismatch ||
        statusMismatch || typeMismatch)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Control cannot enter its software fence"});
    const auto phase =
        proof.request.kind == ControlKind::resume || proof.request.kind == ControlKind::seek
            ? StepPhase::resumeRoute
            : StepPhase::cut;
    if (proof.request.participants[0] && !matchingStep(proof, Participant::capture, phase)) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        if (internal::execution_diagnostics::rpa::fenceSink) {
            internal::execution_diagnostics::rpa::reject(
                internal::execution_diagnostics::rpa::FencePredicate::cutMissing);
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::cutMissing);
        }
#endif
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Final capture boundary is unconfirmed"});
    }
    s.hostFence =
        ExecutionFence{proof.request.context.sessionScope, s.generation,
                       proof.request.target.candidateId, proof.request.context.controlSequence};
    return *s.hostFence;
}
auto PlaybackSession::executionFence() const -> core::Result<ExecutionFence> {
    auto& session = *this;
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("control_fence"));
    if (!session.state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "No active software fence"});
    return *session.state_->hostFence;
}
auto PlaybackSession::completeExecutionClose(ExecutionFence fence, const ControlReceipt& proof)
    -> core::Result<void> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("host_close"));
    if (!s.hostFence || *s.hostFence != fence || proof.request.kind != ControlKind::close ||
        proof.request.context.sessionScope != fence.scope_ ||
        proof.request.context.controlSequence <= fence.controlSequence_ ||
        !controlFullyApplied(proof) || s.sessionState != SessionState::Empty)
        return core::unexpected(
            core::Error{"host.rpa.device_pending", "Close has not retired every fenced owner"});
    s.hostFence.reset();
    return {};
}
auto PlaybackSession::invalidateGameplayClock() -> core::Result<void> {
    auto& session = *this;
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("execution_revoke"));
    auto& s = *session.state_;
    if (!s.executionScheduler)
        return core::unexpected(gameplayError("Realtime session is not started"));
    auto revoked = s.executionScheduler->clockValid(false);
    auto synced = s.syncExecutionState();
    return synced ? revoked : synced;
}
auto PlaybackSession::mapGameplayClock(const ClockObservation& clock, std::int64_t now)
    -> core::Result<ClockTicks> {
    auto& session = *this;
    if (auto guard = session.gameplayWriteGuard(); !guard)
        return core::unexpected(guard.error());
    auto& s = *session.state_;
    if (!s.realtimeClock || !s.executionScheduler)
        return core::unexpected(gameplayError("Realtime session is not started"));
    auto mapped = s.tailTransition && clock == s.tailTransition->eof
                      ? s.realtimeClock->sampleTail(now)
                      : s.realtimeClock->sample(clock, now);
    if (!mapped) {
        auto invalid = s.executionScheduler->clockValid(false);
        if (!invalid)
            return core::unexpected(invalid.error());
        auto synced = s.syncExecutionState();
        if (!synced)
            return core::unexpected(synced.error());
    }
    return mapped;
}
auto PlaybackSession::gameplayReplayCut(const GameplayReplay& replay, GameplayTick horizon) const
    -> core::Result<GameplayReplayCut> {
    auto& session = *this;
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("archive_cut"));
    const auto* archive = detail::GameplayAccess::replay(replay);
    if (!archive)
        return core::unexpected(gameplayError("Owning archive is absent"));
    auto cut = judgement::replayCutAt(archive->archive, judgement::Tick{horizon.value});
    return GameplayReplayCut{
        cut.completeRecords,
        cut.partialHorizon ? std::optional<GameplayTick>{GameplayTick{cut.partialHorizon->value()}}
                           : std::nullopt};
}
auto PlaybackSession::gameplayFeedback(const std::uint64_t scope) const
    -> core::Result<std::vector<FeedbackObservation>> try {
    internal::pd::Timer measurement{internal::pd::Stage::feedback};
    auto& session = *this;
    const auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("feedback_membership"));
    if (!s.gameplay || !scope)
        return core::unexpected(gameplayError("Feedback scope absent"));
    std::vector<FeedbackObservation> members;
    for (const auto& scheduled : s.gameplayProjection.active) {
        const auto& b = s.gameplayProjectionIndex.rows[scheduled.row].binding;
        if (!scheduled.published || !s.gameplayProjection.lastTick ||
            b.start.value > s.gameplayProjection.lastTick->value ||
            (b.end && b.end->value <= s.gameplayProjection.lastTick->value))
            continue;
        members.push_back({{scope, scheduled.factId.commitId, scheduled.factId.localOrdinal, 0,
                            b.target, b.bindingId},
                           b.start.value,
                           b.end,
                           FeedbackStatus::created,
                           false});
    }
    return members;
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Feedback membership failed"});
}
auto PlaybackSession::sampleGameplayPresentation(GameplayPresentationTick tick,
                                                 const RuntimeFrame& frame)
    -> core::Result<void> try {
    auto& session = *this;
    if (auto guard = session.gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{session.state_->operationActive};
    // Obtain the immutable kernel result without advancing the scheduler.
    if (!session.state_->gameplay)
        return core::unexpected(gameplayError("Gameplay presentation is not prepared"));
    if (session.state_->gameplayState == GameplayState::Paused)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Paused presentation must stay frozen"});
    return detail::GameplayPublicationAccess::publish(session, *session.state_->gameplay, tick,
                                                      frame, true);
} catch (...) {
    return core::unexpected(gameplayError("Presentation sampling failed"));
}
auto PlaybackSession::executionOwnerState() const -> core::Result<ExecutionOwnerState> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("execution_owner"));
    ExecutionOwnerState value{state_->sessionToken,
                              state_->generation,
                              state_->lastFrame,
                              state_->acceptedHostControl,
                              state_->hostFence,
                              {},
                              {}};
    if (state_->executionScheduler && state_->executionScheduler->pendingEpochEnd()) {
        const auto& end = *state_->executionScheduler->pendingEpochEnd();
        value.pendingEpochEnd = GameplayEpochEnd{{end.inclusiveTick.value()}, end.pauseEpoch};
    }
    if (state_->gameplay) {
        auto query = state_->gameplay->query();
        if (!query)
            return core::unexpected(query.error());
        if (query->kernelView().rpa)
            value.reconciliationOrdinal = query->kernelView().rpa->nextControlOrdinal;
    }
    return value;
} catch (...) {
    return core::unexpected(core::Error{"host.rpa.capacity_exceeded", "Owner status copy failed"});
}
auto PlaybackSession::executionStepEffects() const -> core::Result<StepEffects> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("execution_effects"));
    auto query = queryGameplay();
    if (!query)
        return core::unexpected(query.error());
    const auto& p = detail::GameplayAccess::result(*query)->kernel;
    StepEffects effects;
    effects.generation = state_->generation;
    effects.sealedFactCursor = p.sealedFactCursor;
    effects.foldedFactCursor = p.fold ? p.fold->factCursor : p.sealedFactCursor;
    if (p.rpa && p.rpa->cutTick)
        effects.position = p.rpa->cutTick->value();
    return effects;
}

auto PlaybackSession::retireExecutionStep(const StepRequest& request, const ControlReceipt& proof)
    -> core::Result<void> {
    if (auto valid = validateExecutionControlProof(request, proof); !valid)
        return valid;
    if (proof.request.kind == ControlKind::close && proof.status == ControlStatus::failed)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Failed Close cannot retire software"});
    const bool directedPhase = request.key.phase == StepPhase::retire;
    const bool directedTarget = request.target == proof.request.target;
    const bool directedRequest = *state_->acceptedHostControl == proof.request;
    const bool directedKind = proof.request.kind == ControlKind::close ||
                              proof.status == ControlStatus::suspended ||
                              proof.status == ControlStatus::failed;
    const bool captureCleared =
        !proof.request.participants[0] ||
        matchingStep(proof, Participant::capture, StepPhase::cut) != nullptr;
    const bool audioCleared = [&] {
        if (!proof.request.participants[2])
            return true;
        const auto* isolation = matchingStep(proof, Participant::audio, StepPhase::isolate);
        return matchingStep(proof, Participant::audio, StepPhase::stop) != nullptr && isolation &&
               isolation->effects.callbackIsolated;
    }();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink) {
        sink->permitAttempted = true;
        sink->permitDirected = directedPhase && directedTarget && directedRequest && directedKind;
        sink->permitRequestMatch = directedRequest;
        sink->permitTargetMatch = directedTarget;
        sink->permitOperationMatch = request.key.operationId == proof.request.operationId;
        sink->permitParticipantMatch = request.key.participant == Participant::software;
        sink->permitCaptureCut = captureCleared;
        sink->permitAudioIsolated = audioCleared;
        if (!directedPhase)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::retirePhase);
        else if (!directedTarget)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::retireTarget);
        else if (!directedRequest)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::requestMismatch);
        else if (!directedKind)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::retireKind);
        else if (!captureCleared)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::captureActive);
        else if (!audioCleared)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::audioActive);
    }
#endif
    if (!directedPhase || !directedTarget || !directedRequest || !directedKind)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Retirement lacks directed cleanup"});
    if (!captureCleared)
        return core::unexpected(core::Error{"host.rpa.device_pending", "Capture is active"});
    if (!audioCleared)
        return core::unexpected(core::Error{"host.rpa.device_pending", "Audio is active"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    auto unloaded = unload();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink)
        sink->retireApplied = bool(unloaded);
#endif
    return unloaded;
}
auto PlaybackSession::pauseExecutionStep(const StepRequest& request, std::int64_t now)
    -> core::Result<void> {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    if (state_->acceptedHostControl->kind != ControlKind::pause ||
        request.key.phase != StepPhase::softwareCommit || !state_->executionScheduler ||
        !state_->executionScheduler->pendingEpochEnd() ||
        request.target.tick !=
            state_->executionScheduler->pendingEpochEnd()->inclusiveTick.value() ||
        request.target.pauseEpoch != state_->executionScheduler->pendingEpochEnd()->pauseEpoch ||
        !state_->hostFence || state_->hostFence->scope_ != request.key.sessionScope ||
        state_->hostFence->candidateId_ != request.target.candidateId ||
        state_->hostFence->controlSequence_ != request.target.context.controlSequence ||
        (request.target.tailPauseAt && *request.target.tailPauseAt > now))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Pause lacks accepted cut and fence"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    return pauseRealtimeGameplay(now);
}
auto PlaybackSession::resumeExecutionStep(const StepRequest& request,
                                          const GameplayReconciliation& reconciliation,
                                          GameplayExecutionContext context,
                                          const ClockObservation& clock, std::int64_t now,
                                          const StepReceipt* routeReceipt) -> core::Result<void> {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    if (state_->acceptedHostControl->kind != ControlKind::resume ||
        request.key.phase != StepPhase::softwareCommit || !state_->hostFence ||
        !request.target.nextContext || state_->hostFence->scope_ != request.key.sessionScope ||
        state_->hostFence->candidateId_ != request.target.candidateId ||
        state_->hostFence->controlSequence_ != request.target.context.controlSequence ||
        request.target.tick != reconciliation.cutTick.value ||
        request.target.pauseEpoch != reconciliation.pauseEpoch ||
        (request.target.cut && request.target.cut != reconciliation.finalTraceCut) ||
        (request.target.reconciliationOrdinal &&
         request.target.reconciliationOrdinal != reconciliation.controlOrdinal) ||
        request.target.nextContext->sessionScope != context.sessionScope ||
        request.target.nextContext->clockSegment != context.clockSegment ||
        request.target.nextContext->runEpoch != context.runEpoch)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Resume lacks accepted route and fence"});
    if (!routeReceipt || routeReceipt->status != StepStatus::applied ||
        routeReceipt->request.key.participant != Participant::capture ||
        routeReceipt->request.key.phase != StepPhase::resumeRoute ||
        routeReceipt->request.key.operationId != request.key.operationId ||
        routeReceipt->request.key.sessionScope != request.key.sessionScope ||
        routeReceipt->request.target != request.target ||
        routeReceipt->effects.cut != reconciliation.finalTraceCut ||
        routeReceipt->effects.pauseEpoch != reconciliation.pauseEpoch ||
        reconciliation.trace.size() < reconciliation.finalTraceCut)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Final route is unconfirmed"});
    auto identity = computeGameplayReconciliationIdentity(reconciliation);
    if (!identity)
        return core::unexpected(identity.error());
    if ((request.resumeTrace ? std::optional{request.resumeTrace->identity}
                             : request.target.payloadIdentity) != *identity ||
        (request.resumeTrace &&
         (request.resumeTrace->finalCut != reconciliation.finalTraceCut ||
          routeReceipt->effects.payloadIdentity != request.resumeTrace->identity)))
        return core::unexpected(
            core::Error{"host.rpa.message_conflict", "Resume trace identity differs"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    return resumeRealtimeGameplay(reconciliation, context, clock, now, true, request.tailResumeAt);
}
auto PlaybackSession::seekExecutionStep(const StepRequest& request, const ControlReceipt& proof,
                                        const GameplayReplay& archive, GameplayTick horizon,
                                        const GameplayReplayCut& cut,
                                        std::span<const GameplayCheckpoint> checkpoints,
                                        GameplayPresentationTick tick, const RuntimeFrame& frame,
                                        SchedulingProfile profile,
                                        ClockMappingConfiguration mapping,
                                        const std::string& domain) -> core::Result<void> try {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    const auto binding = detail::replayBindingMatches(archive, request.target.archiveIdentity,
                                                      request.target.replaySourceIdentity);
    if (!binding)
        return core::unexpected(binding.error());
    if (*state_->acceptedHostControl != proof.request || proof.request.kind != ControlKind::seek ||
        request.key.phase != StepPhase::softwareCommit || !request.target.nextContext ||
        !state_->hostFence || !controlReadyForCommit(proof) || !*binding ||
        request.target.seekHorizon != horizon.value ||
        request.target.seekCompleteRecords != cut.completeRecords ||
        request.target.seekPartialHorizon !=
            (cut.partialHorizon ? std::optional{cut.partialHorizon->value} : std::nullopt))
        return core::unexpected(core::Error{"host.rpa.context_stale", "Seek proof differs"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    auto result = seekGameplay(archive, horizon, cut, checkpoints, tick, frame);
    if (!result)
        return result;
    const auto& next = *request.target.nextContext;
    return beginRealtimeGameplay(profile, mapping,
                                 {next.sessionScope, next.clockSegment, next.runEpoch}, domain);
} catch (...) {
    return core::unexpected(core::Error{"host.rpa.capacity_exceeded", "Seek reservation failed"});
}

#endif

} // namespace cuexis::playback
