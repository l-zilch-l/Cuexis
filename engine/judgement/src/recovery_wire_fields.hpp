#pragma once

// Field lists of the recovery DTOs. Each overload names the members that enter the
// canonical byte order, and the binary codec below only dispatches on the mapped type.

#include "execution_kernel.hpp"

#include <type_traits>

namespace cuexis::judgement::detail::wire {

template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, EmissionPathStep>
void fields(A& a, V& v) {
    a(v.nodeId, v.repeatIndex);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, RequirementIdentity>
void fields(A& a, V& v) {
    a(v.chartEntryId, v.invocationId, v.moduleId, v.exportId, v.emissionPath, v.requirementLocalId);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CanonicalObservationKey>
void fields(A& a, V& v) {
    a(v.observationTick, v.domainToken, v.sourceClass, v.channelToken, v.action, v.amount);
    if (a.revision == 3)
        a(v.sourceLocalPosition);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CanonicalInput>
void fields(A& a, V& v) {
    a(v.key, v.sequence);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, InputReceiptPending>
void fields(A& a, V& v) {
    a(v.observationKey, v.dispatchTick, v.wasForwarded, v.admissionFrontier, v.admissionHorizon);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, AdmittedObservation>
void fields(A& a, V& v) {
    a(v.observationKey, v.dispatchTick, v.wasForwarded, v.admissionFrontier, v.admissionHorizon);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ObservationOrigin>
void fields(A& a, V& v) {
    a(v.dispatchTick, v.observationKey);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, TimerOrigin>
void fields(A& a, V& v) {
    a(v.tick, v.requirement);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CoordinationOrigin>
void fields(A& a, V& v) {
    a(v.dispatchTick, v.observationKey);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CandidateKey>
void fields(A& a, V& v) {
    a(v.requirement, v.phase, v.origin, v.resourceId, v.intent);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ContactHandle>
void fields(A& a, V& v) {
    a(v.domainToken, v.sourceClass, v.channelToken, v.startPressKey);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, SlotIdentity>
void fields(A& a, V& v) {
    a(v.resourceId, v.slotToken, v.ordinal);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, LeaseIdentity>
void fields(A& a, V& v) {
    a(v.slot, v.requirement, v.candidate);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, HeadContactOwnership>
void fields(A& a, V& v) {
    a(v.requirement, v.contact, v.acquiredTick, v.lease);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, Free>
void fields(A& a, V& v) {
    static_cast<void>(v);
    a();
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, Terminal>
void fields(A& a, V& v) {
    static_cast<void>(v);
    a();
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, Held>
void fields(A& a, V& v) {
    a(v.requirement, v.lease, v.contact);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ResourceProjection>
void fields(A& a, V& v) {
    a(v.resourceId, v.state);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, PhaseProjection>
void fields(A& a, V& v) {
    a(v.requirement, v.phase, v.state);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, FactId>
void fields(A& a, V& v) {
    a(v.commitId, v.localOrdinal);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, LogicalCanonicalOrdinal>
void fields(A& a, V& v) {
    a(v.originScope, v.originOrdinal, v.localOrdinal, v.phaseRank, v.factKind);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, PhaseOutcomeFact>
void fields(A& a, V& v) {
    a(v.requirement, v.phase, v.outcome, v.category, v.error, v.evidence, v.originId, v.commitId,
      v.factId, v.commitTick, v.canonicalOrdinal, v.grade);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ReceiptFact>
void fields(A& a, V& v) {
    a(v.kind, v.observationKey, v.originId, v.commitId, v.factId, v.commitTick, v.canonicalOrdinal);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ObservationRecord>
void fields(A& a, V& v) {
    a(v.requirement, v.originKey, v.commitTick, v.observationKey, v.progressed, v.accepted);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, InputReceipt>
void fields(A& a, V& v) {
    a(v.observationKey, v.consideredCount, v.consumedBy, v.dispatchTick);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, HookValue>
void fields(A& a, V& v) {
    a(v.target, v.visibleTick, v.value);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, StatisticsCount>
void fields(A& a, V& v) {
    a(v.phase, v.outcome, v.grade, v.count);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, FoldProjection>
void fields(A& a, V& v) {
    a(v.score, v.combo, v.maxCombo, v.hits, v.misses, v.factCursor, v.workTick, v.produced,
      v.hookConsumerCursor, v.bonus, v.monoidValue, v.ledgerDerivedCount, v.counts, v.strayCount,
      v.consumeEmptyCount);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, KernelProjection>
void fields(A& a, V& v) {
    a(v.phases, v.resources, v.contacts, v.ownership, v.observers, v.receipts, v.facts,
      v.processedFrontier, v.lastAdvanceHorizon, v.state, v.faultDiagnostic, v.failedTick,
      v.requestedHorizon, v.kernelWorkTick, v.sealedFactCursor, v.fold,
      v.effectiveFactSemanticRevision, v.faultStage);
    if (a.revision == 3)
        a(v.rpa);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, MatcherSnapshot>
void fields(A& a, V& v) {
    a(v.states, v.epsilonAttempted);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, IngressSnapshot>
void fields(A& a, V& v) {
    a(v.accepted, v.lastObservedTick, v.nextId, v.exhausted);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, SnapshotDTO>
void fields(A& a, V& v) {
    a(v.kernel, v.matchers, v.coverageHistory, v.activeRequirements, v.timerCursor, v.nextCommitId,
      v.commitIdsExhausted, v.ingress, v.pending);
    if (a.revision == 3)
        a(v.rpaHistory);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ReplayRecord>
void fields(A& a, V& v) {
    a(v.batch, v.admission, v.horizon, v.result);
    if (a.revision == 3)
        a(v.control, v.rejection, v.controlDrain);
}

template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, PauseTransition>
void fields(A& a, V& v) {
    a(v.sequence, v.observation);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, RpaControlRecord>
void fields(A& a, V& v) {
    a(v.kind, v.cutTick, v.controlOrdinal, v.pauseEpoch, v.finalTraceCut, v.trace);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, DeferredInterruption>
void fields(A& a, V& v) {
    a(v.startPress, v.controlOrdinal, v.release);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ReconciliationOrigin>
void fields(A& a, V& v) {
    a(v.tick, v.controlOrdinal, v.deferredOccurrence, v.traceCut, v.release);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, RpaLogicalState>
void fields(A& a, V& v) {
    a(v.paused, v.cutTick, v.nextControlOrdinal, v.pauseEpoch, v.accepted, v.quarantine, v.deferred,
      v.controls, v.foldAtTickStart, v.foldBaseTick, v.foldedControlCount);
}

} // namespace cuexis::judgement::detail::wire
