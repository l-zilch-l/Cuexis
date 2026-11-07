#pragma once

// Worker configuration, reply waiting, control proofs and load/close helpers.

#include "../../app/execution_host/capture_route.hpp"
#include "../../app/execution_host/session_worker.hpp"
#include "../../engine/core/internal/cuexis_internal/sha256.hpp"
#include "../../engine/playback/src/replay_identity.hpp"
#include "execution_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <chrono>
#include <cstring>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace cuexis;
using namespace cuexis::test_support;
using namespace judgement;
using namespace judgement::testing;
using namespace playback;
using namespace playback::detail;

namespace rpa_b5_session_fixture {

inline auto configuration() -> SessionWorkerConfiguration {
    return {32, 8, 64, 16, 16, 64, true, [] { return std::int64_t{0}; }};
}
inline auto await(SessionWorker& worker, std::uint64_t id) -> core::Result<SessionReply> {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    for (;;) {
        auto result = worker.poll(id);
        REQUIRE(result);
        if (*result) {
            auto reply = std::move(**result);
            REQUIRE(worker.acknowledge(id));
            return reply;
        }
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}
template <typename Job>
auto send(SessionWorker& worker, Job&& command, bool control = true) -> SessionReply {
    // Construct the large command variant in this one short-lived call frame.
    auto message = std::make_unique<SessionCommand>(std::forward<Job>(command));
    auto posted = worker.post(std::move(*message), control);
    REQUIRE(posted);
    auto received = await(worker, *posted);
    INFO((received ? ""
                   : std::string{received.error().code()} + ": " +
                         std::string{received.error().message()}));
    REQUIRE(received);
    return std::move(*received);
}
inline void shutdown(SessionWorker& worker) {
    worker.requestShutdown();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!worker.exited()) {
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    REQUIRE(worker.joinExited());
    REQUIRE_FALSE(worker.post(SessionInspect{}, true));
}
inline auto controlProof(HostCoordinator& host, std::uint64_t operation)
    -> std::unique_ptr<ControlReceipt> {
    auto receipt = host.query(operation);
    REQUIRE(receipt);
    return std::make_unique<ControlReceipt>(std::move(*receipt));
}
template <class Job>
auto sendProof(SessionWorker& worker, const StepRequest& request, HostCoordinator& host,
               std::uint64_t operation) -> SessionReply {
    auto job = std::make_unique<Job>();
    job->request = request;
    job->proof = std::move(*controlProof(host, operation));
    return send(worker, std::move(*job));
}
template <class Job>
auto postProof(SessionWorker& worker, const StepRequest& request, const ControlReceipt& proof)
    -> core::Result<std::uint64_t> {
    auto job = std::make_unique<Job>();
    job->request = request;
    job->proof = proof;
    return worker.post(SessionCommand{std::move(*job)}, true);
}
inline void confirmProof(SessionWorker& worker, HostCoordinator& host, std::uint64_t operation) {
    send(worker, SessionConfirmControl{*controlProof(host, operation)});
}
inline auto acceptControl(HostCoordinator& host, const ControlRequest& request,
                          std::uint64_t decision) -> bool {
    return bool(host.accept(request, decision));
}
inline auto finalizedContext(HostCoordinator& host, std::uint64_t operation) -> HostContext {
    return controlProof(host, operation)->finalizedContext.value();
}
struct Loaded final {
    PresentationCandidateInput input;
    HostCoordinator host;
};
inline auto load(SessionWorker& worker, PlaybackSource source,
                 std::optional<GameplayContent> gameplay = {}, bool confirm = true) -> Loaded {
    presentation_renderer::TestPresentationRenderer renderer;
    SessionPrepareSource prepare;
    prepare.source = std::move(source);
    prepare.capabilities = renderer.capabilities();
    prepare.rendererInstance = renderer.identity().instance;
    prepare.rendererGeneration = renderer.identity().generation;
    prepare.controlSequence = 1;
    prepare.gameplay = std::move(gameplay);
    auto input = std::get<PresentationCandidateInput>(send(worker, std::move(prepare)));
    auto initial = input.context;
    initial.controlSequence = 0;
    initial.resourceGeneration = 0;
    initial.contentIdentity = {};
    auto host = HostCoordinator::create(initial, 16);
    REQUIRE(host);
    ControlRequest load;
    load.context = initial;
    load.context.controlSequence = 1;
    load.operationId = 1;
    load.kind = ControlKind::load;
    load.participants = {false, true, false, false};
    load.target.context = load.context;
    load.target.nextContext = input.context;
    load.target.candidateId = input.candidateId;
    load.target.participantGenerations[1] = input.mutationGeneration;
    auto decision = std::get<std::uint64_t>(send(worker, SessionAcceptControl{load}));
    CHECK(std::get<std::uint64_t>(send(worker, SessionAcceptControl{load})) == decision);
    REQUIRE(host->accept(load, decision));
    StepRequest request{{initial.sessionScope, 1, Participant::software, StepPhase::prepare, 0},
                        load.target};
    auto prepared = std::get<StepReceipt>(send(worker, SessionSoftwarePrepare{request}));
    REQUIRE(host->observe(prepared));
    request.key.phase = StepPhase::fence;
    auto fenced = std::get<StepReceipt>(sendProof<SessionSoftwareFence>(worker, request, *host, 1));
    REQUIRE(host->observe(fenced));
    request.key.phase = StepPhase::softwareCommit;
    auto incomplete = controlProof(*host, 1);
    incomplete
        ->steps[static_cast<unsigned>(Participant::software)]
               [static_cast<unsigned>(StepPhase::prepare)]
        .reset();
    auto rejectedId = postProof<SessionSoftwareCommit>(worker, request, *incomplete);
    REQUIRE(rejectedId);
    REQUIRE_FALSE(await(worker, *rejectedId));
    // Rejection before first software Apply retains owner-local Prepared for a valid proof.
    auto committed =
        std::get<StepReceipt>(sendProof<SessionSoftwareCommit>(worker, request, *host, 1));
    REQUIRE(committed.status == StepStatus::applied);
    REQUIRE(host->observe(committed));
    REQUIRE(host->markSoftwareApplied());
    REQUIRE(host->finish());
    if (confirm)
        confirmProof(worker, *host, 1);
    const auto generation = std::get<SessionInspection>(send(worker, SessionInspect{})).generation;
    auto repeated =
        std::get<StepReceipt>(sendProof<SessionSoftwareCommit>(worker, request, *host, 1));
    CHECK(repeated.status == StepStatus::applied);
    CHECK(std::get<SessionInspection>(send(worker, SessionInspect{})).generation == generation);
    return {std::move(input), std::move(*host)};
}
inline void close(SessionWorker& worker, HostCoordinator& host, HostContext context,
                  std::uint64_t operation, std::uint64_t command) {
    ControlRequest close;
    close.context = context;
    ++close.context.controlSequence;
    close.kind = ControlKind::close;
    close.operationId = operation;
    close.commandOrdinal = command;
    close.participants = {false, true, false, false};
    close.target.context = close.context;
    close.target.commandOrdinal = command;
    const auto beforeRetire = std::get<SessionInspection>(send(worker, SessionInspect{}));
    close.target.participantGenerations[1] = beforeRetire.generation;
    const auto decision = std::get<std::uint64_t>(send(worker, SessionAcceptControl{close}));
    REQUIRE(host.accept(close, decision));
    StepRequest retire{
        {context.sessionScope, operation, Participant::software, StepPhase::retire, 0},
        close.target};
    auto retired =
        std::get<StepReceipt>(send(worker, SessionSoftwareRetire{retire, *host.query(operation)}));
    REQUIRE(retired.effects.referencesReleased);
    // A retire that actually unloads bumps the generation; an already empty owner keeps it.
    // Either way the receipt, not the fence, carries the terminal generation (design 6.2).
    if (beforeRetire.state == SessionState::Empty)
        CHECK(retired.effects.generation == beforeRetire.generation);
    else
        CHECK(retired.effects.generation > beforeRetire.generation);
    REQUIRE(host.observe(retired));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    const auto retirementObservation =
        std::get<obs::OwnerCloseObservation>(send(worker, SessionRpaObservation{}));
    REQUIRE(retirementObservation.currentFence.valid);
    if (retirementObservation.currentFence.controlSequence < close.context.controlSequence) {
        CHECK(retirementObservation.closeRetire.existingFence);
        CHECK(retirementObservation.currentFence.controlSequence == context.controlSequence);
        CHECK(retirementObservation.currentFence.generation ==
              retirementObservation.closeRetire.existingIdentity.generation);
    } else {
        CHECK(retirementObservation.currentFence.controlSequence == close.context.controlSequence);
    }
#endif
    retire.key.phase = StepPhase::close;
    auto closed =
        std::get<StepReceipt>(send(worker, SessionSoftwareClose{retire, *host.query(operation)}));
    REQUIRE(host.observe(closed));
    REQUIRE(host.finish());
    send(worker, SessionConfirmControl{*host.query(operation)});
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK_FALSE(std::get<obs::OwnerCloseObservation>(send(worker, SessionRpaObservation{}))
                    .currentFence.valid);
#endif
    const auto afterClose = std::get<SessionInspection>(send(worker, SessionInspect{}));
    CHECK(afterClose.state == SessionState::Empty);
    CHECK(afterClose.generation == retired.effects.generation);
}

} // namespace rpa_b5_session_fixture

using namespace rpa_b5_session_fixture;
