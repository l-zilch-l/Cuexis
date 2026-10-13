#include "../../engine/core/internal/cuexis_internal/sha256.hpp"
#include "../../engine/playback/src/capture_route.hpp"
#include "../../engine/playback/src/session_worker.hpp"
#include "execution_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <filesystem>
#include <fstream>
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
          std::optional<GameplayContent> gameplay = {}) -> Loaded {
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
    auto fenced =
        std::get<StepReceipt>(send(worker, SessionSoftwareFence{request, *host->query(1)}));
    REQUIRE(host->observe(fenced));
    request.key.phase = StepPhase::softwareCommit;
    auto incomplete = *host->query(1);
    incomplete
        .steps[static_cast<unsigned>(Participant::software)]
              [static_cast<unsigned>(StepPhase::prepare)]
        .reset();
    auto rejectedId = worker.post(SessionSoftwareCommit{request, incomplete}, true);
    REQUIRE(rejectedId);
    REQUIRE_FALSE(await(worker, *rejectedId));
    // Rejection before first software Apply retains owner-local Prepared for a valid proof.
    auto committed =
        std::get<StepReceipt>(send(worker, SessionSoftwareCommit{request, *host->query(1)}));
    REQUIRE(committed.status == StepStatus::applied);
    REQUIRE(host->observe(committed));
    REQUIRE(host->markSoftwareApplied());
    REQUIRE(host->finish());
    send(worker, SessionConfirmControl{*host->query(1)});
    const auto generation = std::get<SessionInspection>(send(worker, SessionInspect{})).generation;
    auto repeated =
        std::get<StepReceipt>(send(worker, SessionSoftwareCommit{request, *host->query(1)}));
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
    close.target.participantGenerations[1] =
        std::get<SessionInspection>(send(worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(worker, SessionAcceptControl{close}));
    REQUIRE(host.accept(close, decision));
    StepRequest retire{
        {context.sessionScope, operation, Participant::software, StepPhase::retire, 0},
        close.target};
    auto retired =
        std::get<StepReceipt>(send(worker, SessionSoftwareRetire{retire, *host.query(operation)}));
    REQUIRE(retired.effects.referencesReleased);
    REQUIRE(host.observe(retired));
    retire.key.phase = StepPhase::close;
    auto closed =
        std::get<StepReceipt>(send(worker, SessionSoftwareClose{retire, *host.query(operation)}));
    REQUIRE(host.observe(closed));
    REQUIRE(host.finish());
    send(worker, SessionConfirmControl{*host.query(operation)});
    CHECK(std::get<SessionInspection>(send(worker, SessionInspect{})).state == SessionState::Empty);
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
