// PlaybackSession realtime gameplay: the scheduling clock, capture delivery and advance requests.

#include "playback_session_internal.hpp"

namespace cuexis::playback {

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
auto PlaybackSession::beginRealtimeGameplay(SchedulingProfile profile,
                                            ClockMappingConfiguration config,
                                            GameplayExecutionContext context, std::string domain)
    -> core::Result<void> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{state_->operationActive};
    if (!state_->gameplay || state_->executionScheduler || state_->realtimeWasStarted)
        return core::unexpected(gameplayError("Begin requires a newly prepared realtime session"));
    if (state_->acceptedHostControl && state_->acceptedHostControl->target.nextContext) {
        const auto& expected = *state_->acceptedHostControl->target.nextContext;
        if (context.sessionScope != expected.sessionScope ||
            context.clockSegment != expected.clockSegment || context.runEpoch != expected.runEpoch)
            return core::unexpected(core::Error{"host.rpa.context_stale",
                                                "Realtime startup differs from accepted context"});
    }
    auto query = state_->gameplay->query();
    if (!query)
        return core::unexpected(query.error());
    if (!query->kernelView().rpa || context.sessionScope == 0 || context.runEpoch == 0)
        return core::unexpected(
            gameplayError("Explicit realtime execution profile and scope required"));
    if (!profile.testOnly)
        return core::unexpected(core::Error{"host.rpa.profile_incomplete",
                                            "Production candidate budgets await F3 acceptance"});
    const auto* content = detail::GameplayAccess::content(state_->gameplayContent);
    auto calibration = RealtimeClock::calibrationIdentity(config);
    if (!calibration)
        return core::unexpected(calibration.error());
    if (content->configuration.calibrationIdentityToken != *calibration)
        return core::unexpected(
            core::Error{"host.rpa.calibration_mismatch",
                        "Prepared calibration does not bind this clock mapping"});
    auto clock = RealtimeClock::create(profile, config, std::move(domain), context.clockSegment);
    if (!clock)
        return core::unexpected(clock.error());
    if (*profile.retainedMessageCapacity > SIZE_MAX)
        return core::unexpected(
            core::Error{"host.rpa.capacity_exceeded", "Capacity not representable"});
    auto scheduler = judgement::ExecutionScheduler::create(
        *state_->gameplay, internalContext(context),
        *content->configuration.latePolicy.windowCloseThreshold.measuredValue(),
        static_cast<std::size_t>(*profile.retainedMessageCapacity),
        state_->acceptedHostControl ? state_->acceptedHostControl->commandOrdinal + 1 : 0,
        state_->acceptedHostControl ? state_->hostDecision : 0, state_->pendingRetiredRequestFloor);
    if (!scheduler)
        return core::unexpected(scheduler.error());
    state_->executionScheduler =
        std::make_unique<judgement::ExecutionScheduler>(std::move(*scheduler));
    state_->tailTransition.reset();
    state_->executionMapping = config;
    state_->realtimeClock = std::move(*clock);
    state_->executionContext = context;
    state_->pendingRetiredRequestFloor.reset();
    state_->realtimeWasStarted = true;
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Realtime startup reservation failed"));
}
auto PlaybackSession::beginRealtimeTail(const ClockObservation& eof, std::uint64_t segment,
                                        TailProfile profile, std::int64_t now)
    -> core::Result<TailClockTransition> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return core::unexpected(guard.error());
    auto& s = *state_;
    if (!s.executionScheduler || !s.realtimeClock || !s.executionContext || s.tailTransition ||
        s.hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Tail transition not ready"});
    SessionOperation operation{s.operationActive};
    auto next = *s.executionContext;
    next.clockSegment = segment;
    if (!s.executionScheduler->canTransitionClock(internalContext(next)))
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Unconfirmed old clock delivery"});
    const auto* content = detail::GameplayAccess::content(s.gameplayContent);
    std::int64_t lastTimer = INT64_MIN;
    for (const auto& timer : content->capsule.gameplay.timers())
        lastTimer = std::max(lastTimer, timer.tick.value());
    for (const auto& requirement : content->capsule.gameplay.requirements())
        lastTimer = std::max(lastTimer, requirement.deadline.value());
    const auto close =
        content->configuration.latePolicy.windowCloseThreshold.measuredValue()->value();
    if (lastTimer == INT64_MIN)
        lastTimer = profile.inputDeadline;
    if (profile.inputDeadline < lastTimer)
        return core::unexpected(core::Error{"host.rpa.profile_incomplete",
                                            "Tail deadline excludes prepared obligations"});
    const auto latest = std::max(lastTimer, profile.inputDeadline);
    if (close < 0 || latest > INT64_MAX - close || !s.executionMapping)
        return core::unexpected(
            core::Error{"host.rpa.profile_incomplete", "Tail terminal is not representable"});
    const auto terminal = latest + close;
    auto mappedTerminal = RealtimeClock::seekPosition(*s.executionMapping, terminal);
    if (!mappedTerminal || terminal - close >= mappedTerminal->observationTick)
        return core::unexpected(
            core::Error{"host.rpa.profile_incomplete",
                        "Tail terminal cannot seal its exclusive capture frontier"});
    auto transition = s.realtimeClock->startTail(eof, now, segment, profile, lastTimer, close);
    if (!transition)
        return core::unexpected(transition.error());
    auto switched = s.executionScheduler->transitionClock(internalContext(next));
    if (!switched)
        return core::unexpected(switched.error());
    s.executionContext = next;
    if (s.confirmedHostContext)
        s.confirmedHostContext->clockSegment = segment;
    s.tailTransition = *transition;
    s.bumpGeneration();
    return *transition;
} catch (...) {
    return core::unexpected(core::Error{"host.rpa.capacity_exceeded", "Tail transition failed"});
}

auto PlaybackSession::deliverGameplayCapture(const GameplayCaptureDelivery& message,
                                             const ClockObservation& observation, std::int64_t now)
    -> core::Result<void> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{state_->operationActive};
    if (!state_->executionScheduler)
        return core::unexpected(gameplayError("Realtime session not started"));
    if (!state_->executionContext ||
        internalContext(message.context) != internalContext(*state_->executionContext))
        return core::unexpected(core::Error{"host.rpa.context_stale", "Capture context mismatch"});
    const auto confirmed = state_->executionScheduler->confirmedSequence();
    if (confirmed && message.deliverySequence <= *confirmed)
        return {};
    judgement::CaptureMessage value{internalContext(message.context), message.deliverySequence,
                                    judgement::CapturePrefix{judgement::Tick{0}}};
    if (const auto* b = std::get_if<GameplayClosedBatch>(&message.payload)) {
        judgement::ClosedCaptureBatch batch{judgement::Tick{b->originalTick.value},
                                            judgement::Tick{b->observedTick.value},
                                            b->batchId,
                                            b->fragmentIndex,
                                            b->fragmentCount,
                                            b->totalMembers,
                                            {},
                                            b->epochTerminated,
                                            b->pauseEpoch};
        const auto* content = detail::GameplayAccess::content(state_->gameplayContent);
        for (const auto& member : b->members) {
            auto input = canonicalOccurrence(member, content->configuration);
            if (!input)
                return core::unexpected(input.error());
            batch.members.push_back(std::move(*input));
        }
        value.payload = std::move(batch);
    } else if (const auto* prefix = std::get_if<GameplayCapturePrefix>(&message.payload))
        value.payload = judgement::CapturePrefix{judgement::Tick{prefix->exclusiveTick.value}};
    else {
        const auto end = std::get<GameplayEpochEnd>(message.payload);
        value.payload =
            judgement::EndRunEpoch{judgement::Tick{end.inclusiveTick.value}, end.pauseEpoch};
    }
    auto mapped = state_->tailTransition && observation == state_->tailTransition->eof
                      ? state_->realtimeClock->sampleTail(now)
                      : state_->realtimeClock->sample(observation, now);
    if (!mapped) {
        auto invalid = state_->executionScheduler->clockValid(false);
        if (!invalid)
            return invalid;
        auto synced = state_->syncExecutionState();
        if (!synced)
            return synced;
        return core::unexpected(mapped.error());
    }
    const auto proofTick = std::visit(
        [](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, judgement::ClosedCaptureBatch>)
                return payload.observedTick.value();
            else if constexpr (std::is_same_v<T, judgement::CapturePrefix>)
                return payload.exclusiveTick.value();
            else
                return payload.inclusiveTick.value();
        },
        value.payload);
    if (proofTick > mapped->observationTick) {
        auto revoked = state_->executionScheduler->clockValid(false);
        if (!revoked)
            return revoked;
        auto synced = state_->syncExecutionState();
        if (!synced)
            return synced;
        return core::unexpected(core::Error{"host.rpa.message_conflict",
                                            "Capture closure cannot cite a future observed Tick"});
    }
    auto previous = state_->gameplay->query();
    if (!previous)
        return core::unexpected(previous.error());
    const auto previousH = previous->kernelView().lastAdvanceHorizon;
    auto delivered = state_->executionScheduler->deliver(value);
    auto synced = state_->syncExecutionState();
    if (!synced)
        return synced;
    if (!delivered)
        return core::unexpected(detail::projectGameplayError(delivered.error()));
    auto query = state_->gameplay->query();
    if (query && query->kernelView().state == judgement::KernelSessionState::Faulted)
        state_->gameplayState = GameplayState::Faulted;
    else if (query && query->kernelView().lastAdvanceHorizon != previousH)
        state_->gameplayState = GameplayState::Running;
    state_->bumpGeneration();
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Capture delivery reservation failed"));
}
auto PlaybackSession::requestGameplayAdvance(GameplayAdvanceRequest r,
                                             const ClockObservation& observation, std::int64_t now)
    -> core::Result<GameplayAdvanceDecision> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return core::unexpected(guard.error());
    SessionOperation operation{state_->operationActive};
    if (!state_->executionScheduler || !state_->realtimeClock)
        return core::unexpected(gameplayError("Realtime session not started"));
    if (!state_->executionContext ||
        internalContext(r.context) != internalContext(*state_->executionContext))
        return core::unexpected(core::Error{"host.rpa.context_stale", "Advance context mismatch"});
    // Retransmission is a read of the original operation, including an expired clock sample.
    auto old = state_->executionScheduler->poll(r.requestId);
    if (!old && old.error().code() == "host.rpa.request_retired")
        return core::unexpected(old.error());
    if (!old) {
        auto mapped = state_->tailTransition && observation == state_->tailTransition->eof
                          ? state_->realtimeClock->sampleTail(now)
                          : state_->realtimeClock->sample(observation, now);
        if (!mapped) {
            auto invalid = state_->executionScheduler->clockValid(false);
            if (!invalid)
                return core::unexpected(invalid.error());
            auto synced = state_->syncExecutionState();
            if (!synced)
                return core::unexpected(synced.error());
            return core::unexpected(mapped.error());
        }
        if (r.horizon.value != mapped->horizon || r.context.clockSegment != mapped->segment)
            return core::unexpected(
                gameplayError("Advance H must be derived from the coherent clock observation"));
    }
    auto result =
        state_->executionScheduler->request({internalContext(r.context), r.requestId,
                                             r.commandOrdinal, judgement::Tick{r.horizon.value}});
    auto synced = state_->syncExecutionState();
    if (!synced)
        return core::unexpected(synced.error());
    if (!result)
        return core::unexpected(detail::projectGameplayError(result.error()));
    if (!old && result->disposition == judgement::AdvanceDisposition::executed) {
        state_->gameplayState = GameplayState::Running;
        state_->bumpGeneration();
    }
    if (result->result && result->result->state == judgement::KernelSessionState::Faulted)
        state_->gameplayState = GameplayState::Faulted;
    return publicDecision(*result);
} catch (...) {
    return core::unexpected(gameplayError("Advance decision publication failed"));
}
auto PlaybackSession::queryGameplayAdvance(std::uint64_t id) const
    -> core::Result<GameplayAdvanceDecision> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("query_gameplay_advance"));
    if (state_->operationActive)
        return core::unexpected(reentryError("query_gameplay_advance"));
    if (!state_->executionScheduler)
        return core::unexpected(gameplayError("Realtime session not started"));
    auto result = state_->executionScheduler->poll(id);
    if (!result)
        return core::unexpected(result.error());
    return publicDecision(*result);
} catch (...) {
    return core::unexpected(gameplayError("Advance query allocation failed"));
}
auto PlaybackSession::executionAdvanceRetention(
    GameplayExecutionContext& identityContext, std::optional<std::uint64_t>& lastAcceptedId,
    std::optional<std::uint64_t>& reclaimedThrough) const -> core::Result<void> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("advance_retention"));
    if (state_->operationActive)
        return core::unexpected(reentryError("advance_retention"));
    if (!state_->executionScheduler)
        return core::unexpected(gameplayError("Realtime session not started"));
    const auto progress = state_->executionScheduler->retentionProgress();
    identityContext = {progress.identityContext.sessionScope, progress.identityContext.clockSegment,
                       progress.identityContext.runEpoch};
    lastAcceptedId = progress.lastAcceptedId;
    reclaimedThrough = progress.reclaimedThrough;
    return {};
}
auto PlaybackSession::retireGameplayAdvance(std::uint64_t id) -> core::Result<void> {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    if (!state_->executionScheduler)
        return core::unexpected(gameplayError("Realtime session not started"));
    return state_->executionScheduler->retire(id);
}
auto PlaybackSession::acceptGameplayControl(GameplayExecutionContext context, std::uint64_t command)
    -> core::Result<void> {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    if (!state_->executionScheduler)
        return core::unexpected(gameplayError("Realtime session not started"));
    auto result = state_->executionScheduler->acceptControl(internalContext(context), command);
    auto synced = state_->syncExecutionState();
    return synced ? result : synced;
}
auto PlaybackSession::pauseRealtimeGameplay(std::int64_t now) -> core::Result<void> {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    if (!state_->executionScheduler)
        return core::unexpected(gameplayError("Realtime session not started"));
    auto result = state_->executionScheduler->pause();
    if (result && state_->tailTransition)
        result = state_->realtimeClock->adoptTailPause(now);
    auto synced = state_->syncExecutionState();
    if (!synced)
        return synced;
    if (result) {
        state_->gameplayState = GameplayState::Paused;
        state_->bumpGeneration();
    }
    return result;
}
auto PlaybackSession::resumeRealtimeGameplay(const GameplayReconciliation& r,
                                             GameplayExecutionContext context,
                                             const ClockObservation& observation, std::int64_t now,
                                             bool applied, std::optional<std::int64_t> tailResumeAt)
    -> core::Result<void> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{state_->operationActive};
    if (!state_->executionScheduler || !state_->realtimeClock)
        return core::unexpected(gameplayError("Realtime session not started"));
    judgement::RpaControlRecord value{judgement::RpaControlKind::resume,
                                      judgement::Tick{r.cutTick.value},
                                      r.controlOrdinal,
                                      r.pauseEpoch,
                                      r.finalTraceCut,
                                      {}};
    const auto* content = detail::GameplayAccess::content(state_->gameplayContent);
    bool failed = false;
    for (const auto& transition : r.trace) {
        auto input = canonicalOccurrence(transition.occurrence, content->configuration);
        if (!input) {
            failed = true;
            break;
        }
        value.trace.push_back({transition.sequence, std::move(input->key)});
    }
    auto known = state_->executionScheduler->resumeStatus(value, internalContext(context));
    auto synced = state_->syncExecutionState();
    if (!synced)
        return synced;
    if (!known)
        return core::unexpected(known.error());
    if (*known)
        return {};
    if (state_->gameplayState == GameplayState::ResumeAborted)
        return core::unexpected(
            core::Error{"host.rpa.resume_aborted", "Direct continuation has terminated"});
    // Reject identity and lifecycle mismatches before changing the clock segment.
    auto current = state_->gameplay->query();
    if (!current)
        return core::unexpected(current.error());
    const auto& logical = current->kernelView().rpa;
    if (!state_->executionContext || !logical || !logical->paused ||
        context.sessionScope != state_->executionContext->sessionScope ||
        context.runEpoch <= state_->executionContext->runEpoch ||
        context.clockSegment <= state_->executionContext->clockSegment ||
        logical->cutTick != value.cutTick || logical->pauseEpoch != value.pauseEpoch ||
        logical->nextControlOrdinal != value.controlOrdinal)
        return core::unexpected(core::Error{"host.rpa.context_stale",
                                            "Resume context or paused control identity mismatch"});
    if (!applied || (!failed && value.trace.size() < value.finalTraceCut))
        return state_->executionScheduler->resume(value, internalContext(context), applied, failed);
    auto clock = [&]() -> core::Result<ClockTicks> {
        if (!state_->tailTransition)
            return state_->realtimeClock->startSegment(context.clockSegment, observation, now);
        if (observation != state_->tailTransition->eof)
            return core::unexpected(
                core::Error{"host.rpa.clock_invalid", "Tail EOF identity changed"});
        if (tailResumeAt && *tailResumeAt > now)
            return core::unexpected(
                core::Error{"host.rpa.clock_invalid", "Tail resume action is in the future"});
        const auto minimum =
            current->kernelView().lastAdvanceHorizon
                ? std::optional<std::int64_t>{current->kernelView().lastAdvanceHorizon->value()}
                : std::nullopt;
        auto resumed = state_->realtimeClock->resumeTail(tailResumeAt.value_or(now),
                                                         context.clockSegment, minimum);
        if (!resumed)
            return core::unexpected(resumed.error());
        return state_->realtimeClock->sampleTail(now);
    }();
    failed = failed || !clock || (clock && clock->observationTick <= r.cutTick.value);
    auto result =
        state_->executionScheduler->resume(value, internalContext(context), applied, failed);
    if (result) {
        state_->executionContext = context;
        state_->gameplayState = GameplayState::Running;
        state_->bumpGeneration();
    } else if (result.error().code() != "host.rpa.control_pending") {
        auto q = state_->gameplay->query();
        state_->gameplayState = q && q->kernelView().state == judgement::KernelSessionState::Faulted
                                    ? GameplayState::Faulted
                                    : GameplayState::ResumeAborted;
    }
    return result;
} catch (...) {
    if (applied && state_->executionScheduler) {
        state_->executionScheduler->abortResume();
        state_->gameplayState = GameplayState::ResumeAborted;
    }
    return core::unexpected(gameplayError("Resume preparation allocation failed"));
}
auto PlaybackSession::gameplayExecutionLag(GameplayTick horizon) const
    -> core::Result<GameplayExecutionLag> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("gameplay_execution_lag"));
    if (state_->operationActive)
        return core::unexpected(reentryError("gameplay_execution_lag"));
    if (!state_->gameplay)
        return core::unexpected(gameplayError("Gameplay inactive"));
    auto query = state_->gameplay->query();
    if (!query)
        return core::unexpected(query.error());
    const auto& p = query->kernelView();
    const auto* content = detail::GameplayAccess::content(state_->gameplayContent);
    const auto frontier = judgement::offsetTicks(
        judgement::Tick{horizon.value},
        judgement::TickSpan{
            -content->configuration.latePolicy.windowCloseThreshold.measuredValue()->value()});
    if (!frontier)
        return core::unexpected(frontier.error());
    GameplayExecutionLag result{horizon,
                                {},
                                {},
                                p.fold && p.fold->workTick
                                    ? std::optional<GameplayTick>{{p.fold->workTick->value()}}
                                    : std::nullopt,
                                p.sealedFactCursor,
                                p.fold ? p.fold->factCursor : p.sealedFactCursor};
    if (p.lastAdvanceHorizon) {
        auto d = judgement::differenceTicks(judgement::Tick{horizon.value}, *p.lastAdvanceHorizon);
        if (!d)
            return core::unexpected(d.error());
        result.horizonLag = d->value();
    }
    if (p.processedFrontier) {
        auto d = judgement::differenceTicks(*frontier, *p.processedFrontier);
        if (!d)
            return core::unexpected(d.error());
        result.frontierLag = d->value();
    }
    return result;
} catch (...) {
    return core::unexpected(gameplayError("Lag query allocation failed"));
}

auto PlaybackSession::controlGameplay(GameplayControl control) -> core::Result<void> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("control_gameplay"));
    if (state_->operationActive)
        return core::unexpected(reentryError("control_gameplay"));
    SessionOperation operation{state_->operationActive};
    if (!state_->gameplay)
        return core::unexpected(gameplayError("Gameplay inactive"));
    auto selected = state_->gameplay->query();
    if (selected && selected->kernelView().rpa &&
        (control == GameplayControl::Pause || control == GameplayControl::Resume))
        return core::unexpected(gameplayError("Realtime Pause/Resume requires a cut and trace"));
    if (control == GameplayControl::Stop && state_->gameplayState == GameplayState::Faulted)
        return core::unexpected(gameplayError("Faulted Gameplay requires Reset or replacement"));
    if (control == GameplayControl::Reset || control == GameplayControl::Stop) {
        auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock);
        if (!mutation)
            return core::unexpected(std::move(mutation.error()));
        auto fresh = makeGameplaySession(state_->gameplayContent);
        if (!fresh)
            return core::unexpected(detail::projectGameplayError(std::move(fresh.error())));
        auto replacement = std::make_unique<judgement::JudgementSession>(std::move(*fresh));
        if (state_->lastFrame && state_->runtimeSession) {
            auto updated =
                state_->runtimeSession->updateGameplay(runtimeFrame(*state_->lastFrame), {}, false);
            if (!updated)
                return updated;
        }
        if (state_->executionScheduler)
            state_->pendingRetiredRequestFloor =
                state_->executionScheduler->retentionProgress().lastAcceptedId;
        state_->executionScheduler.reset();
        state_->realtimeClock.reset();
        state_->executionContext.reset();
        state_->realtimeWasStarted = false;
        state_->gameplay.swap(replacement);
        state_->gameplayState =
            control == GameplayControl::Stop ? GameplayState::Paused : GameplayState::Prepared;
        state_->gameplayReceipt.reset();
        state_->gameplayProjection = {};
        detail::CommittedPresentationState descriptor;
        descriptor.source = internal::presentation::Source::Restore;
        descriptor.frame = state_->lastFrame;
        descriptor.activationRevision = mutation->version();
        state_->publishPresentation(*mutation, descriptor);
    } else {
        if (state_->gameplayState == GameplayState::Faulted)
            return core::unexpected(gameplayError("Faulted Gameplay requires reset"));
        if (control == GameplayControl::Pause) {
            if (state_->gameplayState == GameplayState::Paused)
                return {};
            state_->gameplayState = GameplayState::Paused;
        } else if (control == GameplayControl::Resume) {
            if (state_->gameplayState != GameplayState::Paused)
                return {};
            state_->gameplayState = GameplayState::Running;
        } else
            return core::unexpected(gameplayError("Unknown Gameplay control"));
    }
    state_->bumpGeneration();
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Gameplay control allocation failed"));
}
#endif
} // namespace cuexis::playback
