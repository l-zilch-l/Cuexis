// PlaybackSession gameplay commands: the gameplay session, its projection and the publication.

#include "playback_session_internal.hpp"

// Seek entry points and the public/private gameplay translators they share with the realtime
// unit. Their declarations live in playback_session_internal.hpp.

namespace cuexis::playback {

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
auto PlaybackSession::seekGameplay(const GameplayReplay& a, GameplayTick h,
                                   GameplayPresentationTick t, const RuntimeFrame& frame)
    -> core::Result<void> {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    const auto* archive = detail::GameplayAccess::replay(a);
    if (!archive)
        return core::unexpected(gameplayError("Empty Replay"));
    const auto cut = judgement::replayCutAt(archive->archive, judgement::Tick{h.value});
    return seekGameplay(a, h, {cut.completeRecords, publicTick(cut.partialHorizon)}, {}, t, frame);
}
auto PlaybackSession::seekGameplay(const GameplayReplay& a, GameplayTick h,
                                   const GameplayReplayCut& cut,
                                   std::span<const GameplayCheckpoint> checkpoints,
                                   GameplayPresentationTick t, const RuntimeFrame& frame)
    -> core::Result<void> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{state_->operationActive};
    const auto* archive = detail::GameplayAccess::replay(a);
    if (!archive || !state_->gameplay)
        return core::unexpected(gameplayError("Replay or active Gameplay absent"));
    // Reuse the active run's validated identity and construct one independent Seek candidate.
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    auto s2 = internal::execution_diagnostics::rpa::SeekScope{
        internal::execution_diagnostics::rpa::SeekPhase::s2CandidatePrep};
#endif
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    {
        using Phase = internal::execution_diagnostics::rpa::SeekPhase;
        using Work = internal::execution_diagnostics::rpa::SeekWork;
        const auto* content = detail::GameplayAccess::content(state_->gameplayContent);
        if (content != nullptr) {
            const auto& prepared = content->capsule.gameplay;
            internal::execution_diagnostics::rpa::seekSet(
                Phase::s2CandidatePrep, &Work::entities,
                static_cast<std::uint64_t>(content->capsule.chart.entities.size()));
            internal::execution_diagnostics::rpa::seekSet(
                Phase::s2CandidatePrep, &Work::requirements,
                static_cast<std::uint64_t>(prepared.requirements().size()));
            internal::execution_diagnostics::rpa::seekSet(
                Phase::s2CandidatePrep, &Work::timerRequirements,
                static_cast<std::uint64_t>(prepared.timers().size()));
            internal::execution_diagnostics::rpa::seekSet(
                Phase::s2CandidatePrep, &Work::timerElements,
                static_cast<std::uint64_t>(prepared.timers().size()));
            std::uint64_t phaseOffsets = 0;
            std::uint64_t matcherElements = 0;
            for (const auto& requirement : prepared.requirements()) {
                phaseOffsets += requirement.measure.componentCount();
                matcherElements += requirement.pattern.stateCount().value_or(0);
            }
            internal::execution_diagnostics::rpa::seekSet(Phase::s2CandidatePrep,
                                                          &Work::phaseOffsets, phaseOffsets);
            internal::execution_diagnostics::rpa::seekSet(Phase::s2CandidatePrep,
                                                          &Work::matcherElements, matcherElements);
        }
    }
#endif
    std::vector<judgement::ReplayCheckpoint> certificates;
    certificates.reserve(checkpoints.size());
    for (const auto& c : checkpoints) {
        const auto* stored = detail::GameplayAccess::checkpoint(c);
        if (!stored)
            return core::unexpected(gameplayError("Empty Checkpoint"));
        certificates.push_back(stored->checkpoint);
    }
    judgement::ReplayCut internalCut{cut.completeRecords,
                                     cut.partialHorizon
                                         ? std::optional{judgement::Tick{cut.partialHorizon->value}}
                                         : std::nullopt};
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    // Certificate assembly is part of candidate preparation: the vectors above are sized and
    // filled here, so the S2 scope must not close before this point.
    internal::execution_diagnostics::rpa::seekSet(
        internal::execution_diagnostics::rpa::SeekPhase::s2CandidatePrep,
        &internal::execution_diagnostics::rpa::SeekWork::requirementSets,
        static_cast<std::uint64_t>(certificates.size()));
    s2.finish();
    internal::execution_diagnostics::rpa::SeekScope s3{
        internal::execution_diagnostics::rpa::SeekPhase::s3Validation};
    internal::execution_diagnostics::rpa::seekSet(
        internal::execution_diagnostics::rpa::SeekPhase::s3Validation,
        &internal::execution_diagnostics::rpa::SeekWork::actualRecords,
        static_cast<std::uint64_t>(cut.completeRecords));
#endif
    auto candidate = state_->gameplay->forkForSeek(archive->archive, judgement::Tick{h.value},
                                                   internalCut, certificates);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    using CandidatePhase = internal::execution_diagnostics::rpa::SeekPhase;
    using CandidateWork = internal::execution_diagnostics::rpa::SeekWork;
    internal::execution_diagnostics::rpa::seekAdd(CandidatePhase::s2CandidatePrep,
                                                  &CandidateWork::candidateCalls);
    internal::execution_diagnostics::rpa::seekSet(
        CandidatePhase::s2CandidatePrep, &CandidateWork::validCandidates, candidate ? 1 : 0);
    internal::execution_diagnostics::rpa::seekSet(
        CandidatePhase::s2CandidatePrep, &CandidateWork::discardedCandidates, candidate ? 0 : 1);
    internal::execution_diagnostics::rpa::seekSet(
        CandidatePhase::s2CandidatePrep, &CandidateWork::preparedHandles, candidate ? 1 : 0);
    internal::execution_diagnostics::rpa::seekSet(CandidatePhase::s2CandidatePrep,
                                                  &CandidateWork::configCopies, candidate ? 1 : 0);
#endif
    if (!candidate)
        return core::unexpected(detail::projectGameplayError(std::move(candidate.error())));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    s3.finish();
    internal::execution_diagnostics::rpa::SeekScope s5{
        internal::execution_diagnostics::rpa::SeekPhase::s5PublishSwap};
#endif
    auto replacement = std::make_unique<judgement::JudgementSession>(std::move(*candidate));
    auto q = replacement->query();
    if (!q)
        return core::unexpected(detail::projectGameplayError(std::move(q.error())));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    using PublishPhase = internal::execution_diagnostics::rpa::SeekPhase;
    using PublishWork = internal::execution_diagnostics::rpa::SeekWork;
    const auto& projection = q->kernelView();
    internal::execution_diagnostics::rpa::seekSet(
        PublishPhase::s5PublishSwap, &PublishWork::objects,
        static_cast<std::uint64_t>(projection.phases.size() + projection.resources.size()));
    internal::execution_diagnostics::rpa::seekSet(
        PublishPhase::s5PublishSwap, &PublishWork::publishedFacts,
        static_cast<std::uint64_t>(projection.facts.size()));
    internal::execution_diagnostics::rpa::seekSet(
        PublishPhase::s5PublishSwap, &PublishWork::copyAllocations, 1); // owning session
    internal::execution_diagnostics::rpa::seekSet(
        PublishPhase::s5PublishSwap, &PublishWork::oldOwnersRetained, state_->gameplay ? 1 : 0);
    internal::execution_diagnostics::rpa::seekSet(PublishPhase::s5PublishSwap,
                                                  &PublishWork::releases, state_->gameplay ? 1 : 0);
#endif
    if (auto published =
            detail::GameplayPublicationAccess::publish(*this, *replacement, t, frame, false, true);
        !published)
        return published;
    if (state_->executionScheduler)
        state_->pendingRetiredRequestFloor =
            state_->executionScheduler->retentionProgress().lastAcceptedId;
    state_->executionScheduler.reset();
    state_->realtimeClock.reset();
    state_->executionContext.reset();
    state_->realtimeWasStarted = false;
    state_->gameplay.swap(replacement);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    s5.finish();
#endif
    state_->gameplayState = q->kernelView().rpa && q->kernelView().rpa->paused
                                ? GameplayState::Paused
                                : GameplayState::Running;
    state_->gameplayReceipt.reset();
    state_->bumpGeneration();
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Gameplay Seek allocation failed"));
}

auto internalContext(GameplayExecutionContext c) -> judgement::RpaContext {
    return {c.sessionScope, c.clockSegment, c.runEpoch};
}
auto canonicalOccurrence(const GameplayOccurrence& occurrence,
                         const judgement::SessionConfiguration& config)
    -> core::Result<judgement::CanonicalInput> try {
    const auto& input = occurrence.input;
    const auto mapping = config.inputMapping.view();
    if (input.crossedSamplingGap || input.reconnected || input.droppedSamples)
        return core::unexpected(core::Error{"host.rpa.capture_gap", "Capture continuity lost"});
    if (input.sourceClass != mapping.sourceClass.token() ||
        input.action > GameplayInputAction::Update)
        return core::unexpected(gameplayError("Occurrence source or action invalid"));
    std::optional<std::int64_t> amount;
    if (input.amount) {
        auto domain = std::find_if(mapping.domains.begin(), mapping.domains.end(),
                                   [&](const auto& d) { return d.domainToken == input.domain; });
        if (domain == mapping.domains.end())
            return core::unexpected(gameplayError("Occurrence domain not declared"));
        auto quantity =
            judgement::RationalBeat::create(input.amount->numerator, input.amount->denominator);
        if (!quantity)
            return core::unexpected(detail::projectGameplayError(quantity.error()));
        auto quantized = judgement::quantizeAmount(domain->amount, *quantity);
        if (!quantized)
            return core::unexpected(detail::projectGameplayError(quantized.error()));
        amount = quantized->canonicalInteger;
    }
    return judgement::CanonicalInput{
        {judgement::ObservationTick{judgement::Tick{input.observationTick.value}}, input.domain,
         input.sourceClass, input.channel, static_cast<judgement::InputAction>(input.action),
         amount, occurrence.sourceLocalPosition},
        judgement::IngressSequence{input.sequence}};
} catch (...) {
    return core::unexpected(gameplayError("Occurrence normalization allocation failed"));
}
auto publicDecision(const judgement::AdvanceDecision& decision) -> GameplayAdvanceDecision {
    const auto& r = decision.request;
    return {
        {{r.context.sessionScope, r.context.clockSegment, r.context.runEpoch},
         r.requestId,
         r.commandOrdinal,
         {r.horizon.value()}},
        static_cast<GameplayAdvanceDisposition>(decision.disposition),
        decision.decisionOrdinal,
        decision.verifiedExclusive
            ? std::optional<GameplayTick>{{decision.verifiedExclusive->value()}}
            : std::nullopt,
        decision.result
            ? detail::GameplayAccess::result(
                  *decision.result, decision.result->state == judgement::KernelSessionState::Faulted
                                        ? GameplayState::Faulted
                                        : GameplayState::Running)
            : GameplayResult{},
        decision.error ? std::optional<core::Error>{detail::projectGameplayError(*decision.error)}
                       : std::nullopt};
}
#endif

} // namespace cuexis::playback
