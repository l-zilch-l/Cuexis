#pragma once

// Accepted-close proofs and the failed close retirement/step scenarios.

#include "rpa_b5_session_fixture.hpp"

namespace rpa_b5_close_fixture {

inline auto acceptedClose(SessionWorker& worker, Loaded& loaded)
    -> std::unique_ptr<ControlRequest> {
    auto request = std::make_unique<ControlRequest>();
    request->context = loaded.input.context;
    ++request->context.controlSequence;
    request->operationId = 2;
    request->commandOrdinal = 1;
    request->kind = ControlKind::close;
    request->participants = {false, true, false, false};
    request->target.context = request->context;
    request->target.commandOrdinal = 1;
    request->target.participantGenerations[1] =
        std::get<SessionInspection>(send(worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(worker, SessionAcceptControl{*request}));
    REQUIRE(loaded.host.accept(*request, decision));
    return request;
}
template <class Job>
auto attemptProof(SessionWorker& worker, const StepRequest& request, const ControlReceipt& proof)
    -> core::Result<SessionReply> {
    auto job = std::make_unique<Job>();
    job->request = request;
    job->proof = proof;
    auto message = std::make_unique<SessionCommand>(std::move(*job));
    auto posted = worker.post(std::move(*message), true);
    REQUIRE(posted);
    return await(worker, *posted);
}
inline auto stepApplied(const core::Result<SessionReply>& reply) -> bool {
    const auto* step = reply ? std::get_if<StepReceipt>(&*reply) : nullptr;
    return step && step->status == StepStatus::applied;
}
inline auto queryOwnerStep(SessionWorker& worker, StepKey key) -> core::Result<SessionReply> {
    const auto posted = worker.post(SessionQueryStep{key}, true);
    REQUIRE(posted);
    return await(worker, *posted);
}
inline void failedCloseRetirement(bool oldFence) {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation(), {}, !oldFence);
    const auto before = std::get<SessionInspection>(send(**worker, SessionInspect{}));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    const auto start =
        std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    REQUIRE(start.currentFence.valid == oldFence);
#endif
    auto request = acceptedClose(**worker, loaded);
    auto proof = controlProof(loaded.host, request->operationId);
    // Keep the real D2 identity/binding; inject only a failed status at the worker boundary.
    // This invalid proof must not authorize retirement, irrespective of an existing fence.
    proof->status = ControlStatus::failed;
    StepRequest step{
        {request->context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
        request->target};
    auto result = attemptProof<SessionSoftwareRetire>(**worker, step, *proof);
    const auto after = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    auto ledger = queryOwnerStep(**worker, step.key);
    std::cout << "R1.C7" << (oldFence ? "a" : "") << ".retire=" << stepApplied(result)
              << ",error=" << (result ? "none" : result.error().code())
              << ",generation=" << before.generation << "->" << after.generation
              << ",ledgerApplied=" << stepApplied(ledger) << '\n';
    CHECK_FALSE(result);
    CHECK_FALSE(stepApplied(result));
    if (result) {
        const auto* receipt = std::get_if<StepReceipt>(&*result);
        CHECK_FALSE((receipt && receipt->effects.referencesReleased));
    }
    CHECK_FALSE(stepApplied(ledger));
    CHECK(after.generation == before.generation);
    CHECK(after.state == before.state);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    const auto seen = std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK_FALSE(seen.closeRetire.retireApplied);
    CHECK_FALSE(seen.closeRetire.referencesReleased);
    CHECK(seen.closeRetire.identity.decisionOrdinal == proof->decisionOrdinal);
    CHECK(seen.currentFence.valid == start.currentFence.valid);
    CHECK(seen.currentFence.scope == start.currentFence.scope);
    CHECK(seen.currentFence.generation == start.currentFence.generation);
    CHECK(seen.currentFence.candidateId == start.currentFence.candidateId);
    CHECK(seen.currentFence.controlSequence == start.currentFence.controlSequence);
#endif
    // The real worker owner destroys its resources; no second prepare/accept/Close is issued.
    shutdown(**worker);
}
inline void failedCloseStep(bool oldFence) {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation(), {}, !oldFence);
    auto request = acceptedClose(**worker, loaded);
    StepRequest step{
        {request->context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
        request->target};
    auto preparing = controlProof(loaded.host, 2);
    auto retired = attemptProof<SessionSoftwareRetire>(**worker, step, *preparing);
    REQUIRE(stepApplied(retired));
    REQUIRE(loaded.host.observe(std::get<StepReceipt>(*retired)));
    const auto before = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    REQUIRE(before.state == SessionState::Empty);
    auto proof = controlProof(loaded.host, 2);
    proof->status = ControlStatus::failed;
    step.key.phase = StepPhase::close;
    auto result = attemptProof<SessionSoftwareClose>(**worker, step, *proof);
    auto ledger = queryOwnerStep(**worker, step.key);
    std::cout << "R1.C7" << (oldFence ? "a" : "") << ".closeStep=" << stepApplied(result)
              << ",error=" << (result ? "none" : result.error().code())
              << ",ledgerApplied=" << stepApplied(ledger) << '\n';
    CHECK_FALSE(result);
    CHECK_FALSE(stepApplied(result));
    CHECK_FALSE(stepApplied(ledger));
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation ==
          before.generation);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    const auto seen = std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(seen.closeStep.closeAttempted);
    CHECK_FALSE(seen.closeStep.closeApplied);
    CHECK(seen.closeStep.closeState == obs::CloseStepState::rejected);
    CHECK(seen.closeStep.firstErrorCode[0] != '\0');
    CHECK(seen.closeStep.identity.operationId == 2);
    CHECK(seen.closeRetire.retireApplied);
#endif
    shutdown(**worker);
}

} // namespace rpa_b5_close_fixture

using namespace rpa_b5_close_fixture;
