#include "../../app/execution_host/capture_route.hpp"
#include "../../app/execution_host/session_worker.hpp"
#include "../../engine/core/internal/cuexis_internal/sha256.hpp"
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

namespace {
auto configuration() -> SessionWorkerConfiguration {
    return {32, 8, 64, 16, 16, 64, true, [] { return std::int64_t{0}; }};
}
auto await(SessionWorker& worker, std::uint64_t id) -> core::Result<SessionReply> {
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
void shutdown(SessionWorker& worker) {
    worker.requestShutdown();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!worker.exited()) {
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    REQUIRE(worker.joinExited());
    REQUIRE_FALSE(worker.post(SessionInspect{}, true));
}
auto controlProof(HostCoordinator& host, std::uint64_t operation)
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
void confirmProof(SessionWorker& worker, HostCoordinator& host, std::uint64_t operation) {
    send(worker, SessionConfirmControl{*controlProof(host, operation)});
}
auto acceptControl(HostCoordinator& host, const ControlRequest& request, std::uint64_t decision)
    -> bool {
    return bool(host.accept(request, decision));
}
auto finalizedContext(HostCoordinator& host, std::uint64_t operation) -> HostContext {
    return controlProof(host, operation)->finalizedContext.value();
}
struct Loaded final {
    PresentationCandidateInput input;
    HostCoordinator host;
};
auto load(SessionWorker& worker, PlaybackSource source,
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
void close(SessionWorker& worker, HostCoordinator& host, HostContext context,
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

} // namespace

TEST_CASE("B5 Session owner has separate thread and preserves control reply capacity",
          "[rpa][b5][worker]") {
    auto config = configuration();
    config.capacity = 4;
    config.controlReserve = 1;
    auto worker = SessionWorker::create(config);
    REQUIRE(worker);
    std::vector<std::uint64_t> ids;
    for (int i = 0; i < 3; ++i) {
        auto id = (*worker)->post(SessionInspect{});
        REQUIRE(id);
        ids.push_back(*id);
    }
    REQUIRE_FALSE((*worker)->post(SessionInspect{}));
    auto control = (*worker)->post(SessionInspect{}, true);
    REQUIRE(control);
    ids.push_back(*control);
    for (const auto id : ids) {
        auto result = await(**worker, id);
        REQUIRE(result);
        CHECK(std::get<SessionInspection>(*result).ownerThread != std::this_thread::get_id());
    }
    shutdown(**worker);
    config.testOnly = false;
    REQUIRE_FALSE(SessionWorker::create(config));
}
TEST_CASE("B5 worker safely rejects invalid delivery after local construction",
          "[rpa][b5][worker]") {
    auto config = configuration();
    config.delivery.consumers = 0;
    auto rejected = SessionWorker::create(config);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "host.rpa.context_stale");
}
TEST_CASE("B5 Prepared and software commit stay on worker with owning frame transfer",
          "[rpa][b5][worker]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    SessionSample sample;
    sample.context = loaded.input.context;
    sample.projectionScope = 1;
    sample.frameId = 1;
    sample.runtimeFrame = {0, 0, 0};
    sample.viewport = {640, 480};
    auto frame = std::get<FrameEnvelope>(send(**worker, sample));
    CHECK(frame.context == loaded.input.context);
    CHECK_FALSE(frame.resources.empty());
    auto stale = sample;
    ++stale.context.rendererGeneration;
    auto id = (*worker)->post(stale);
    REQUIRE(id);
    auto rejected = await(**worker, *id);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "host.rpa.context_stale");
    close(**worker, loaded.host, loaded.input.context, 2, 1);
    CHECK_FALSE(frame.resources.empty());
    shutdown(**worker);
}
namespace {
struct PausedScenario final {
    std::unique_ptr<SessionWorker> worker;
    Loaded loaded;
    std::unique_ptr<CaptureRouteOwner> capture;
    GameplayExecutionContext execution;
    ClockObservation clock;
    std::uint64_t generation{};
};
auto pausedScenario() -> std::unique_ptr<PausedScenario> {

    SessionTestContent fixture;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    rpa(fixture, mapping);
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation(), fixture.content());
    const GameplayExecutionContext execution{loaded.input.context.sessionScope, 1, 1};
    send(**worker,
         SessionRealtimeStart{{1000, 100, 64, "", true}, mapping, execution, "test.clock"});
    const ClockObservation clock{"test.clock",
                                 1,
                                 1,
                                 0,
                                 1000,
                                 0,
                                 ClockFramePosition{20, 1000000},
                                 ClockFramePosition{20, 1000000},
                                 {},
                                 ClockPlaybackState::Playing,
                                 ClockQuality::Exact,
                                 false,
                                 false,
                                 false};
    auto waiting = std::get<GameplayAdvanceDecision>(
        send(**worker, SessionAdvance{{execution, 1, 1, {20}}, clock}, false));
    CHECK(waiting.disposition == GameplayAdvanceDisposition::Waiting);
    CHECK(waiting.decisionOrdinal == 1);
    ControlRequest pause;
    pause.context = loaded.input.context;
    pause.context.controlSequence = 2;
    pause.operationId = 2;
    pause.commandOrdinal = 2;
    pause.kind = ControlKind::pause;
    pause.participants = {true, true, false, false};
    pause.target.context = pause.context;
    pause.target.commandOrdinal = 2;
    pause.target.tick = 20;
    pause.target.pauseEpoch = 1;
    pause.target.participantGenerations[1] =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(**worker, SessionAcceptControl{pause}));
    REQUIRE(acceptControl(loaded.host, pause, decision));
    CHECK(std::get<GameplayAdvanceDecision>(send(**worker, SessionQueryAdvance{1})).disposition ==
          GameplayAdvanceDisposition::CanceledNotExecuted);
    // Readiness binds the accepted generation before the permitted D2 cut drain mutates it.
    StepRequest step{{execution.sessionScope, 2, Participant::software, StepPhase::prepare, 0},
                     pause.target};
    REQUIRE(
        loaded.host.observe(std::get<StepReceipt>(send(**worker, SessionSoftwarePrepare{step}))));
    auto capture = CaptureRouteOwner::create(execution, 24, 4);
    REQUIRE(capture);
    REQUIRE(capture->capture(input()));
    StepRequest cut{{execution.sessionScope, 2, Participant::capture, StepPhase::cut, 0},
                    pause.target};
    auto cutReceipt = capture->cut(cut);
    REQUIRE(cutReceipt);
    REQUIRE(loaded.host.observe(*cutReceipt));
    auto deliveries = capture->pending();
    REQUIRE(deliveries);
    for (const auto& delivery : *deliveries)
        send(**worker, SessionCaptureDelivery{delivery, clock});
    REQUIRE(capture->acknowledge(execution, deliveries->back().deliverySequence));
    step.key.phase = StepPhase::fence;
    REQUIRE(loaded.host.observe(
        std::get<StepReceipt>(sendProof<SessionSoftwareFence>(**worker, step, loaded.host, 2))));
    step.key.phase = StepPhase::softwareCommit;
    auto paused = std::get<StepReceipt>(sendProof<SessionPause>(**worker, step, loaded.host, 2));
    REQUIRE(paused.status == StepStatus::applied);
    REQUIRE(loaded.host.observe(paused));
    REQUIRE(loaded.host.markSoftwareApplied());
    REQUIRE(loaded.host.finish());
    auto confirmedPause = std::make_unique<SessionConfirmControl>();
    confirmedPause->proof = std::move(*controlProof(loaded.host, 2));
    send(**worker, std::move(*confirmedPause));
    const auto generation =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    CHECK(std::get<StepReceipt>(sendProof<SessionPause>(**worker, step, loaded.host, 2)).status ==
          StepStatus::applied);
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation == generation);
    return std::make_unique<PausedScenario>(PausedScenario{
        std::move(*worker), std::move(loaded),
        std::make_unique<CaptureRouteOwner>(std::move(*capture)), execution, clock, generation});
}
void closePausedScenario(PausedScenario& scene) {
    auto& worker = scene.worker;
    auto& loaded = scene.loaded;

    close(*worker, loaded.host, finalizedContext(loaded.host, 2), 3, 3);
}
void seekPausedScenario(PausedScenario& scene) {
    auto& worker = scene.worker;
    auto& loaded = scene.loaded;
    auto& capture = scene.capture;
    const auto execution = scene.execution;
    const auto& clock = scene.clock;
    const auto generation = scene.generation;

    auto archive = std::get<GameplayReplay>(send(*worker, SessionArchive{}));
    const GameplayTick horizon{4};
    auto archiveCut =
        std::get<GameplayReplayCut>(send(*worker, SessionArchiveCut{archive, horizon}));
    auto bytes = archive.toBytes();
    REQUIRE(bytes);
    ControlRequest seek;
    seek.context = finalizedContext(loaded.host, 2);
    seek.context.controlSequence = 3;
    seek.operationId = 3;
    seek.commandOrdinal = 3;
    seek.kind = ControlKind::seek;
    seek.participants = {true, true, false, false};
    seek.target.context = seek.context;
    seek.target.commandOrdinal = 3;
    seek.target.tick = 20;
    seek.target.pauseEpoch = 1;
    seek.target.nextContext = seek.context;
    seek.target.nextContext->clockSegment = 2;
    seek.target.nextContext->runEpoch = 2;
    seek.target.participantGenerations[0] = execution.runEpoch;
    seek.target.participantGenerations[1] = generation;
    seek.target.archiveIdentity = PreparedSemanticIdentity{core::detail::sha256(*bytes)};
    seek.target.seekHorizon = horizon.value;
    seek.target.seekCompleteRecords = archiveCut.completeRecords;
    if (archiveCut.partialHorizon)
        seek.target.seekPartialHorizon = archiveCut.partialHorizon->value;
    auto decision = std::get<std::uint64_t>(send(*worker, SessionAcceptControl{seek}));
    REQUIRE(loaded.host.accept(seek, decision));
    StepRequest route{{execution.sessionScope, 3, Participant::capture, StepPhase::resumeRoute, 0},
                      seek.target};
    const GameplayExecutionContext next{execution.sessionScope, 2, 2};
    auto capturePrepare = route;
    capturePrepare.key.phase = StepPhase::prepare;
    auto capturePrepared = capture->prepare(capturePrepare);
    REQUIRE(capturePrepared);
    REQUIRE(capturePrepared->status == StepStatus::applied);
    REQUIRE(loaded.host.observe(*capturePrepared));
    auto routed = capture->resumeRoute(route, next);
    REQUIRE(routed);
    REQUIRE(loaded.host.observe(*routed));
    auto captureFence = route;
    captureFence.key.phase = StepPhase::fence;
    auto captureFenced = capture->fence(captureFence);
    REQUIRE(captureFenced);
    REQUIRE(captureFenced->status == StepStatus::applied);
    REQUIRE(loaded.host.observe(*captureFenced));
    StepRequest step{{execution.sessionScope, 3, Participant::software, StepPhase::prepare, 0},
                     seek.target};
    REQUIRE(
        loaded.host.observe(std::get<StepReceipt>(send(*worker, SessionSoftwarePrepare{step}))));
    step.key.phase = StepPhase::fence;
    REQUIRE(loaded.host.observe(
        std::get<StepReceipt>(send(*worker, SessionSoftwareFence{step, *loaded.host.query(3)}))));
    step.key.phase = StepPhase::softwareCommit;
    auto proof = *loaded.host.query(3);
    auto applied = std::get<StepReceipt>(send(
        *worker, SessionSeek{step, proof, archive, horizon, archiveCut, {}, {4}, {0.004, 0, 1}}));
    INFO((applied.cause ? std::string{applied.cause->message()} : std::string{}));
    REQUIRE(applied.status == StepStatus::applied);
    REQUIRE(loaded.host.observe(applied));
    REQUIRE(loaded.host.markSoftwareApplied());
    REQUIRE(loaded.host.finish());
    send(*worker, SessionConfirmControl{*loaded.host.query(3)});
    REQUIRE(capture->enableOrdinary(*loaded.host.query(3)));
    const auto appliedGeneration =
        std::get<SessionInspection>(send(*worker, SessionInspect{})).generation;
    CHECK(std::get<StepReceipt>(
              send(*worker,
                   SessionSeek{step, proof, archive, horizon, archiveCut, {}, {4}, {0.004, 0, 1}}))
              .status == StepStatus::applied);
    CHECK(std::get<SessionInspection>(send(*worker, SessionInspect{})).generation ==
          appliedGeneration);
    auto nextClock = clock;
    nextClock.segment = 2;
    nextClock.source = ClockFramePosition{4, 1000000};
    nextClock.presented = nextClock.source;
    auto waiting = std::get<GameplayAdvanceDecision>(
        send(*worker, SessionAdvance{{next, 2, 4, {4}}, nextClock}, false));
    CHECK(waiting.disposition == GameplayAdvanceDisposition::Waiting);
    // Seek restores the archive's decision cursor, including the recorded Pause drain.
    CHECK(waiting.decisionOrdinal == 6);
    close(*worker, loaded.host, finalizedContext(loaded.host, 3), 4, 5);
    CHECK(archive.valid());
    CHECK(archive.toBytes());
}
void resumePausedScenario(PausedScenario& scene) {
    auto& worker = scene.worker;
    auto& loaded = scene.loaded;
    auto& capture = scene.capture;
    const auto execution = scene.execution;
    const auto& clock = scene.clock;
    const auto generation = scene.generation;

    const auto ordinal =
        std::get<SessionInspection>(send(*worker, SessionInspect{})).reconciliationOrdinal;
    REQUIRE(ordinal);
    auto late = input();
    late.observationTick.value = 20;
    late.action = GameplayInputAction::Release;
    REQUIRE(capture->capture(late));
    ControlRequest resume;
    resume.context = finalizedContext(loaded.host, 2);
    resume.context.controlSequence = 3;
    resume.operationId = 3;
    resume.commandOrdinal = 3;
    resume.kind = ControlKind::resume;
    resume.participants = {true, true, false, false};
    resume.target.context = resume.context;
    resume.target.commandOrdinal = 3;
    resume.target.tick = 20;
    resume.target.pauseEpoch = 1;
    resume.target.reconciliationOrdinal = *ordinal;
    resume.target.nextContext = resume.context;
    resume.target.nextContext->clockSegment = 2;
    resume.target.nextContext->runEpoch = 2;
    resume.target.participantGenerations[0] = execution.runEpoch;
    resume.target.participantGenerations[1] = generation;
    auto accepted = std::get<std::uint64_t>(send(*worker, SessionAcceptControl{resume}));
    REQUIRE(loaded.host.accept(resume, accepted));
    StepRequest route{{execution.sessionScope, 3, Participant::capture, StepPhase::resumeRoute, 0},
                      resume.target};
    const GameplayExecutionContext next{execution.sessionScope, 2, 2};
    auto capturePrepare = route;
    capturePrepare.key.phase = StepPhase::prepare;
    auto capturePrepared = capture->prepare(capturePrepare);
    REQUIRE(capturePrepared);
    REQUIRE(capturePrepared->status == StepStatus::applied);
    REQUIRE(loaded.host.observe(*capturePrepared));
    auto routed = capture->resumeRoute(route, next);
    REQUIRE(routed);
    REQUIRE(loaded.host.observe(*routed));
    auto captureFence = route;
    captureFence.key.phase = StepPhase::fence;
    auto captureFenced = capture->fence(captureFence);
    REQUIRE(captureFenced);
    REQUIRE(captureFenced->status == StepStatus::applied);
    REQUIRE(loaded.host.observe(*captureFenced));
    auto reconciliation = capture->trace(*ordinal);
    REQUIRE(reconciliation);
    StepRequest step{{execution.sessionScope, 3, Participant::software, StepPhase::prepare, 0},
                     resume.target};
    REQUIRE(
        loaded.host.observe(std::get<StepReceipt>(send(*worker, SessionSoftwarePrepare{step}))));
    step.key.phase = StepPhase::fence;
    REQUIRE(loaded.host.observe(
        std::get<StepReceipt>(send(*worker, SessionSoftwareFence{step, *loaded.host.query(3)}))));
    step.key.phase = StepPhase::softwareCommit;
    REQUIRE(routed->effects.cut);
    REQUIRE(routed->effects.payloadIdentity);
    step.resumeTrace = ResumeTraceBinding{*routed->effects.cut, *routed->effects.payloadIdentity};
    auto resumedClock = clock;
    resumedClock.segment = 2;
    resumedClock.source = ClockFramePosition{22, 1000000};
    resumedClock.presented = resumedClock.source;
    auto proof = *loaded.host.query(3);
    auto resumed = std::get<StepReceipt>(
        send(*worker, SessionResume{step, proof, *reconciliation, next, resumedClock, *routed}));
    INFO((resumed.cause ? std::string{resumed.cause->message()} : std::string{}));
    REQUIRE(resumed.status == StepStatus::applied);
    REQUIRE(loaded.host.observe(resumed));
    REQUIRE(loaded.host.markSoftwareApplied());
    REQUIRE(loaded.host.finish());
    send(*worker, SessionConfirmControl{*loaded.host.query(3)});
    REQUIRE(capture->enableOrdinary(*loaded.host.query(3)));
    const auto afterResume =
        std::get<SessionInspection>(send(*worker, SessionInspect{})).generation;
    CHECK(std::get<StepReceipt>(send(*worker, SessionResume{step, proof, *reconciliation, next,
                                                            resumedClock, *routed}))
              .status == StepStatus::applied);
    CHECK(std::get<SessionInspection>(send(*worker, SessionInspect{})).generation == afterResume);
    resumedClock.source = ClockFramePosition{1020, 1000000};
    resumedClock.presented = resumedClock.source;
    ++resumedClock.publicationSequence;
    auto pending = std::get<GameplayAdvanceDecision>(
        send(*worker, SessionAdvance{{next, 2, 4, {1020}}, resumedClock}, false));
    CHECK(pending.disposition == GameplayAdvanceDisposition::Waiting);
    close(*worker, loaded.host, finalizedContext(loaded.host, 3), 4, 5);
}
} // namespace

TEST_CASE("B5 Load ordinals continue into realtime while Waiting leaves owner input loop free",
          "[rpa][b5][worker]") {
    // Each owner scenario has a separate frame: large typed control payloads do not
    // accumulate across Catch sections in an unoptimized Windows debug stack.
    auto scene = pausedScenario();
    SECTION("Paused owner can close directly") {
        closePausedScenario(*scene);
    }
    SECTION("Actual owning Replay Seek restarts execution ordinals in a new epoch") {
        seekPausedScenario(*scene);
    }
    SECTION("Actual R2 consumes owning final trace once on worker") {
        resumePausedScenario(*scene);
    }
    shutdown(*scene->worker);
}

TEST_CASE("B5 queued execution uses owner clock instead of stale submission time",
          "[rpa][b5][worker][clock]") {
    SessionTestContent fixture;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    rpa(fixture, mapping);
    auto config = configuration();
    config.monotonicNow = [] { return std::int64_t{1001}; };
    auto worker = SessionWorker::create(config);
    REQUIRE(worker);
    auto loaded = load(**worker, presentation(), fixture.content());
    const GameplayExecutionContext execution{loaded.input.context.sessionScope, 1, 1};
    send(**worker,
         SessionRealtimeStart{{1000, 100, 64, "", true}, mapping, execution, "test.clock"});
    const ClockObservation clock{"test.clock",
                                 1,
                                 1,
                                 0,
                                 1000,
                                 0,
                                 ClockFramePosition{20, 1000000},
                                 ClockFramePosition{20, 1000000},
                                 {},
                                 ClockPlaybackState::Playing,
                                 ClockQuality::Exact,
                                 false,
                                 false,
                                 false};
    auto posted = (*worker)->post(SessionAdvance{{execution, 1, 1, {20}}, clock}, false);
    REQUIRE(posted);
    auto rejected = await(**worker, *posted);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "host.rpa.clock_invalid");
    auto queried = (*worker)->post(SessionQueryAdvance{1}, true);
    REQUIRE(queried);
    CHECK_FALSE(await(**worker, *queried));
    // Rejected before admission: command 1 remains available to the directed Close.
    close(**worker, loaded.host, loaded.input.context, 2, 1);
    shutdown(**worker);
}

TEST_CASE("B5 suspended Close directs its own cleanup without a normal fence",
          "[rpa][b5][worker][close]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    ControlRequest close;
    close.context = loaded.input.context;
    ++close.context.controlSequence;
    close.kind = ControlKind::close;
    close.operationId = 2;
    close.commandOrdinal = 1;
    close.participants = {false, true, false, false};
    close.target.context = close.context;
    close.target.commandOrdinal = 1;
    close.target.participantGenerations[1] =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(**worker, SessionAcceptControl{close}));
    REQUIRE(loaded.host.accept(close, decision));
    // Delivery failure suspends the Close while the protocol deliberately stays closing.
    REQUIRE(loaded.host.suspend(2));
    CHECK(loaded.host.query(2)->status == ControlStatus::suspended);
    StepRequest retire{{close.context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
                       close.target};
    auto retired =
        std::get<StepReceipt>(send(**worker, SessionSoftwareRetire{retire, *loaded.host.query(2)}));
    CHECK(retired.status == StepStatus::applied);
    CHECK(retired.effects.referencesReleased);
    REQUIRE(loaded.host.observe(retired));
    retire.key.phase = StepPhase::close;
    auto closed =
        std::get<StepReceipt>(send(**worker, SessionSoftwareClose{retire, *loaded.host.query(2)}));
    REQUIRE(loaded.host.observe(closed));
    REQUIRE(loaded.host.finish());
    CHECK(loaded.host.query(2)->status == ControlStatus::complete);
    send(**worker, SessionConfirmControl{*loaded.host.query(2)});
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).state ==
          SessionState::Empty);
    shutdown(**worker);
}

namespace {
auto acceptedClose(SessionWorker& worker, Loaded& loaded) -> std::unique_ptr<ControlRequest> {
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
auto stepApplied(const core::Result<SessionReply>& reply) -> bool {
    const auto* step = reply ? std::get_if<StepReceipt>(&*reply) : nullptr;
    return step && step->status == StepStatus::applied;
}
auto queryOwnerStep(SessionWorker& worker, StepKey key) -> core::Result<SessionReply> {
    const auto posted = worker.post(SessionQueryStep{key}, true);
    REQUIRE(posted);
    return await(worker, *posted);
}
void failedCloseRetirement(bool oldFence) {
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
void failedCloseStep(bool oldFence) {
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
} // namespace

TEST_CASE("R1 C7 failed Close without old fence rejects software retirement",
          "[rpa][r1][c7][worker]") {
    failedCloseRetirement(false);
}
TEST_CASE("R1 C7a failed Close with old fence rejects software retirement",
          "[rpa][r1][c7a][worker]") {
    failedCloseRetirement(true);
}
TEST_CASE("R1 C7 failed Close step without old fence rejects after valid retirement",
          "[rpa][r1][c7][worker]") {
    failedCloseStep(false);
}
TEST_CASE("R1 C7a failed Close step with old fence rejects after valid retirement",
          "[rpa][r1][c7a][worker]") {
    failedCloseStep(true);
}

TEST_CASE("R1 C2 rejects each corrupted Close retirement binding without unloading",
          "[rpa][r1][c2][worker]") {
    const auto field = GENERATE("participant", "scope", "operation", "target", "decision",
                                "sequence", "clockSegment", "runEpoch", "resourceGeneration",
                                "rendererGeneration", "contentIdentity", "generation");
    CAPTURE(field);
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    auto request = acceptedClose(**worker, loaded);
    auto proof = controlProof(loaded.host, 2);
    StepRequest step{
        {request->context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
        request->target};
    const std::string_view name{field};
    if (name == "participant")
        step.key.participant = Participant::renderer;
    else if (name == "scope")
        ++step.key.sessionScope;
    else if (name == "operation")
        ++step.key.operationId;
    else if (name == "target")
        step.target.candidateId = 123;
    else if (name == "decision")
        ++proof->decisionOrdinal;
    else if (name == "sequence")
        ++proof->request.context.controlSequence;
    else if (name == "clockSegment")
        ++proof->request.context.clockSegment;
    else if (name == "runEpoch")
        ++proof->request.context.runEpoch;
    else if (name == "resourceGeneration")
        ++proof->request.context.resourceGeneration;
    else if (name == "rendererGeneration")
        ++proof->request.context.rendererGeneration;
    else if (name == "contentIdentity")
        ++proof->request.context.contentIdentity.sha256[0];
    else if (name == "generation")
        ++step.target.participantGenerations[1].value();
    const auto before = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    auto result = attemptProof<SessionSoftwareRetire>(**worker, step, *proof);
    const auto after = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    CHECK_FALSE(stepApplied(result));
    CHECK(after.generation == before.generation);
    CHECK(after.state == before.state);
    if (result) {
        const auto* receipt = std::get_if<StepReceipt>(&*result);
        CHECK_FALSE((receipt && receipt->effects.referencesReleased));
    }
    shutdown(**worker);
}

TEST_CASE("R1 C8 suspended or failed status without accepted binding cannot unload",
          "[rpa][r1][c8][worker]") {
    const auto status = GENERATE(ControlStatus::suspended, ControlStatus::failed);
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    const auto before = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    auto proof = std::make_unique<ControlReceipt>();
    proof->status = status;
    proof->request.kind = ControlKind::pause;
    proof->request.operationId = 1;
    proof->request.context.sessionScope = before.sessionScope;
    proof->request.target.context = proof->request.context;
    StepRequest step{{before.sessionScope, 1, Participant::software, StepPhase::retire, 0},
                     proof->request.target};
    auto result = attemptProof<SessionSoftwareRetire>(**worker, step, *proof);
    CHECK_FALSE(stepApplied(result));
    auto after = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    CHECK(after.generation == before.generation);
    CHECK(after.state == before.state);
    shutdown(**worker);
}

TEST_CASE("R1 C9 retains capture and audio prerequisites on normal and cleanup paths",
          "[rpa][r1][c9][worker]") {
    const auto mode = GENERATE(0, 1, 2);
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    const auto before = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    auto request = std::make_unique<ControlRequest>();
    request->context = loaded.input.context;
    ++request->context.controlSequence;
    request->kind = ControlKind::close;
    request->operationId = 2;
    request->commandOrdinal = 1;
    request->participants = {mode != 2, true, mode == 2, false};
    request->target.context = request->context;
    request->target.commandOrdinal = 1;
    request->target.tick = 0;
    request->target.pauseEpoch = 1;
    request->target.participantGenerations[1] = before.generation;
    const auto decision = std::get<std::uint64_t>(send(**worker, SessionAcceptControl{*request}));
    REQUIRE(loaded.host.accept(*request, decision));
    if (mode != 0)
        REQUIRE(loaded.host.suspend(2));
    StepRequest step{
        {request->context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
        request->target};
    auto proof = controlProof(loaded.host, 2);
    auto result = attemptProof<SessionSoftwareRetire>(**worker, step, *proof);
    REQUIRE_FALSE(result);
    CHECK(result.error().code() ==
          (mode == 0 ? "host.rpa.control_pending" : "host.rpa.device_pending"));
    const auto after = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    CHECK(after.generation == before.generation);
    CHECK(after.state == before.state);
    CHECK_FALSE(stepApplied(queryOwnerStep(**worker, step.key)));
    shutdown(**worker);
}

TEST_CASE("R1 C10 retire replay uses the original ledger without a second generation change",
          "[rpa][r1][c10][worker]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    auto request = acceptedClose(**worker, loaded);
    auto proof = controlProof(loaded.host, 2);
    const auto before = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    StepRequest step{
        {request->context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
        request->target};
    auto first = attemptProof<SessionSoftwareRetire>(**worker, step, *proof);
    REQUIRE(stepApplied(first));
    const auto generation = std::get<StepReceipt>(*first).effects.generation;
    CHECK(generation == before.generation + 1);
    auto repeated = attemptProof<SessionSoftwareRetire>(**worker, step, *proof);
    REQUIRE(stepApplied(repeated));
    CHECK(std::get<StepReceipt>(*repeated).effects == std::get<StepReceipt>(*first).effects);
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation == generation);
    auto newRequest = step;
    ++newRequest.key.operationId;
    CHECK_FALSE(stepApplied(attemptProof<SessionSoftwareRetire>(**worker, newRequest, *proof)));
    newRequest = step;
    ++newRequest.target.participantGenerations[1].value();
    CHECK_FALSE(stepApplied(attemptProof<SessionSoftwareRetire>(**worker, newRequest, *proof)));
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation == generation);
    REQUIRE(loaded.host.observe(std::get<StepReceipt>(*first)));
    step.key.phase = StepPhase::close;
    auto closed = attemptProof<SessionSoftwareClose>(**worker, step, *controlProof(loaded.host, 2));
    REQUIRE(stepApplied(closed));
    REQUIRE(loaded.host.observe(std::get<StepReceipt>(*closed)));
    REQUIRE(loaded.host.finish());
    send(**worker, SessionConfirmControl{*controlProof(loaded.host, 2)});
    shutdown(**worker);
}

TEST_CASE("R1 C4 same-sequence Close confirmation requires the complete accepted proof",
          "[rpa][r1][c4][worker]") {
    const auto field =
        GENERATE("decision", "clockSegment", "runEpoch", "resource", "renderer", "content");
    CAPTURE(field);
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    auto request = acceptedClose(**worker, loaded);
    StepRequest step{
        {request->context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
        request->target};
    auto retired =
        attemptProof<SessionSoftwareRetire>(**worker, step, *controlProof(loaded.host, 2));
    REQUIRE(stepApplied(retired));
    REQUIRE(loaded.host.observe(std::get<StepReceipt>(*retired)));
    step.key.phase = StepPhase::close;
    auto closed = attemptProof<SessionSoftwareClose>(**worker, step, *controlProof(loaded.host, 2));
    REQUIRE(stepApplied(closed));
    REQUIRE(loaded.host.observe(std::get<StepReceipt>(*closed)));
    REQUIRE(loaded.host.finish());
    auto proof = controlProof(loaded.host, 2);
    auto wrong = std::make_unique<ControlReceipt>(*proof);
    const std::string_view name{field};
    if (name == "decision")
        ++wrong->decisionOrdinal;
    else if (name == "clockSegment")
        ++wrong->request.context.clockSegment;
    else if (name == "runEpoch")
        ++wrong->request.context.runEpoch;
    else if (name == "resource")
        ++wrong->request.context.resourceGeneration;
    else if (name == "renderer")
        ++wrong->request.context.rendererGeneration;
    else
        ++wrong->request.context.contentIdentity.sha256[0];
    const auto posted = (*worker)->post(SessionConfirmControl{*wrong}, true);
    REQUIRE(posted);
    auto result = await(**worker, *posted);
    CHECK_FALSE(result);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    const auto before =
        std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(before.currentFence.valid);
    CHECK(before.currentFence.controlSequence == request->context.controlSequence);
#endif
    send(**worker, SessionConfirmControl{*proof});
    send(**worker, SessionConfirmControl{*proof});
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation ==
          std::get<StepReceipt>(*retired).effects.generation);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    const auto after =
        std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK_FALSE(after.currentFence.valid);
#endif
    shutdown(**worker);
}

TEST_CASE("R1 C4 a fresh Close cannot reuse an unconfirmed different control fence sequence",
          "[rpa][r1][c4][admission]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation(), {}, false);
    const auto before = std::get<SessionInspection>(send(**worker, SessionInspect{}));
    ControlRequest request;
    request.context = loaded.input.context; // Deliberately not a greater sequence.
    request.operationId = 2;
    request.commandOrdinal = 1;
    request.kind = ControlKind::close;
    request.participants = {false, true, false, false};
    request.target.context = request.context;
    request.target.commandOrdinal = 1;
    request.target.participantGenerations[1] = before.generation;
    const auto posted = (*worker)->post(SessionAcceptControl{request}, true);
    REQUIRE(posted);
    const auto result = await(**worker, *posted);
    CHECK_FALSE(result);
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation ==
          before.generation);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    const auto seen = std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(seen.currentFence.valid);
    CHECK(seen.currentFence.controlSequence == loaded.input.context.controlSequence);
#endif
    shutdown(**worker);
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R1 Close observation does not treat a valued canceled receipt as applied",
          "[rpa][r1][observation][worker]") {
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    auto request = acceptedClose(**worker, loaded);
    StepRequest step{
        {request->context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
        request->target};
    auto retired =
        attemptProof<SessionSoftwareRetire>(**worker, step, *controlProof(loaded.host, 2));
    REQUIRE(stepApplied(retired));
    REQUIRE(loaded.host.observe(std::get<StepReceipt>(*retired)));
    step.key.phase = StepPhase::close;
    const auto canceled = std::get<StepReceipt>(send(**worker, SessionCancelStep{step}));
    REQUIRE(canceled.status == StepStatus::canceledNotApplied);
    auto result = attemptProof<SessionSoftwareClose>(**worker, step, *controlProof(loaded.host, 2));
    REQUIRE(result);
    CHECK_FALSE(stepApplied(result));
    const auto seen = std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(seen.closeStep.closeAttempted);
    CHECK_FALSE(seen.closeStep.closeApplied);
    CHECK(seen.closeStep.closeState == obs::CloseStepState::rejected);
    shutdown(**worker);
}
#endif

TEST_CASE("B5 Close cleanup retires a suspended non-Close control before the join",
          "[rpa][r1][c1][b5][worker][close]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    // An accepted Pause that failed before entering its software fence: owner cleanup must not
    // require a normal control fence for a directed retire.
    ControlRequest pause;
    pause.context = loaded.input.context;
    ++pause.context.controlSequence;
    pause.kind = ControlKind::pause;
    pause.operationId = 2;
    pause.commandOrdinal = 1;
    pause.participants = {false, true, false, false};
    pause.target.context = pause.context;
    pause.target.commandOrdinal = 1;
    pause.target.tick = 20;
    pause.target.pauseEpoch = 1;
    pause.target.participantGenerations[1] =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(**worker, SessionAcceptControl{pause}));
    REQUIRE(loaded.host.accept(pause, decision));
    REQUIRE(loaded.host.suspend(2));
    StepRequest retire{{pause.context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
                       pause.target};
    auto retired =
        std::get<StepReceipt>(send(**worker, SessionSoftwareRetire{retire, *loaded.host.query(2)}));
    CHECK(retired.status == StepStatus::applied);
    CHECK(retired.effects.referencesReleased);
    REQUIRE(loaded.host.observe(retired));
    REQUIRE(loaded.host.completeCleanup());
    CHECK(loaded.host.query(2)->status == ControlStatus::failed);
    close(**worker, loaded.host, finalizedContext(loaded.host, 2), 3, 3);
    shutdown(**worker);
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R1 Close step rejection is separate from missing or retirement evidence",
          "[rpa][r1][observation][worker]") {
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    auto initial = std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(initial.closeStep.closeState == obs::CloseStepState::notExecuted);
    ControlRequest request;
    request.context = loaded.input.context;
    ++request.context.controlSequence;
    request.operationId = 2;
    request.commandOrdinal = 1;
    request.kind = ControlKind::close;
    request.participants = {false, true, false, false};
    request.target.context = request.context;
    request.target.commandOrdinal = 1;
    request.target.participantGenerations[1] =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(**worker, SessionAcceptControl{request}));
    REQUIRE(loaded.host.accept(request, decision));
    StepRequest step{{request.context.sessionScope, 2, Participant::software, StepPhase::close, 0},
                     request.target};
    auto posted = (*worker)->post(SessionSoftwareClose{step, *loaded.host.query(2)}, true);
    REQUIRE(posted);
    auto rejected = await(**worker, *posted);
    REQUIRE_FALSE(rejected);
    auto seen = std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(seen.closeStep.closeState == obs::CloseStepState::rejected);
    CHECK(seen.closeStep.closeAttempted);
    CHECK_FALSE(seen.closeStep.closeApplied);
    CHECK(std::string{seen.closeStep.firstErrorCode} == rejected.error().code());
    CHECK(seen.closeStep.identity.operationId == 2);
    CHECK(seen.closeStep.identity.decisionOrdinal == decision);
    CHECK_FALSE(seen.cleanup.identity.valid);
    CHECK_FALSE(seen.closeRetire.identity.valid);
    step.key.phase = StepPhase::retire;
    auto retired =
        std::get<StepReceipt>(send(**worker, SessionSoftwareRetire{step, *loaded.host.query(2)}));
    REQUIRE(loaded.host.observe(retired));
    auto afterRetire =
        std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(afterRetire.closeStep.closeState == obs::CloseStepState::rejected);
    CHECK(std::string{afterRetire.closeStep.firstErrorCode} == rejected.error().code());
    CHECK(afterRetire.closeRetire.retireApplied);
    CHECK_FALSE(afterRetire.closeRetire.closeAttempted);
    step.key.phase = StepPhase::close;
    auto closed =
        std::get<StepReceipt>(send(**worker, SessionSoftwareClose{step, *loaded.host.query(2)}));
    REQUIRE(loaded.host.observe(closed));
    auto done = std::get<obs::OwnerCloseObservation>(send(**worker, SessionRpaObservation{}));
    CHECK(done.closeStep.closeState == obs::CloseStepState::applied);
    CHECK(done.closeStep.closeApplied);
    CHECK(std::string{done.closeStep.firstErrorCode} == rejected.error().code());
    CHECK(done.closeRetire.retireApplied);
    CHECK_FALSE(done.cleanup.identity.valid);
    REQUIRE(loaded.host.finish());
    send(**worker, SessionConfirmControl{*loaded.host.query(2)});
    shutdown(**worker);
}
#endif

TEST_CASE("B5 Close retires the fence of an unconfirmed candidate",
          "[rpa][r1][c3][b5][worker][close]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    // Interruption class 3: the load committed and finished but its confirmation never arrived,
    // so the commit fence still names the candidate while Close retires every owner (candidate 0).
    auto loaded = load(**worker, presentation(), {}, false);
    close(**worker, loaded.host, loaded.input.context, 2, 1);
    shutdown(**worker);
}

TEST_CASE("B5 stale control receipt cannot release the Close fence",
          "[rpa][r1][c5][b5][worker][close]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    ControlRequest close;
    close.context = loaded.input.context;
    ++close.context.controlSequence;
    close.kind = ControlKind::close;
    close.operationId = 2;
    close.commandOrdinal = 1;
    close.participants = {false, true, false, false};
    close.target.context = close.context;
    close.target.commandOrdinal = 1;
    close.target.participantGenerations[1] =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(**worker, SessionAcceptControl{close}));
    REQUIRE(loaded.host.accept(close, decision));
    StepRequest retire{{close.context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
                       close.target};
    const auto heldGeneration =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    auto older = controlProof(loaded.host, 1);
    StepRequest olderRetire{
        {close.context.sessionScope, 1, Participant::software, StepPhase::retire, 0},
        older->request.target};
    CHECK_FALSE(stepApplied(attemptProof<SessionSoftwareRetire>(**worker, olderRetire, *older)));
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation ==
          heldGeneration);
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).state ==
          SessionState::Ready);
    auto retired =
        std::get<StepReceipt>(send(**worker, SessionSoftwareRetire{retire, *loaded.host.query(2)}));
    REQUIRE(loaded.host.observe(retired));
    // The load receipt owns an older sequence: it must cross neither the Close's fence nor its
    // generation, even though the load itself is fully applied.
    auto lateProof = controlProof(loaded.host, 1);
    StepRequest late{{close.context.sessionScope, 1, Participant::software, StepPhase::retire, 0},
                     lateProof->request.target};
    CHECK_FALSE(stepApplied(attemptProof<SessionSoftwareRetire>(**worker, late, *lateProof)));
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).generation ==
          retired.effects.generation);
    const auto repeated =
        attemptProof<SessionSoftwareRetire>(**worker, retire, *controlProof(loaded.host, 2));
    REQUIRE(stepApplied(repeated));
    CHECK(std::get<StepReceipt>(*repeated).effects == retired.effects);
    auto stale = (*worker)->post(SessionConfirmControl{*loaded.host.query(1)}, true);
    REQUIRE(stale);
    auto rejected = await(**worker, *stale);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "host.rpa.device_pending");
    retire.key.phase = StepPhase::close;
    auto closed =
        std::get<StepReceipt>(send(**worker, SessionSoftwareClose{retire, *loaded.host.query(2)}));
    REQUIRE(loaded.host.observe(closed));
    REQUIRE(loaded.host.finish());
    send(**worker, SessionConfirmControl{*loaded.host.query(2)});
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).state ==
          SessionState::Empty);
    shutdown(**worker);
}
