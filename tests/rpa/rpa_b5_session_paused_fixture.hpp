#pragma once

// The paused scenario that drives seek, resume and close through the owner.

#include "rpa_b5_session_fixture.hpp"

namespace rpa_b5_paused_fixture {

struct PausedScenario final {
    std::unique_ptr<SessionWorker> worker;
    Loaded loaded;
    std::unique_ptr<CaptureRouteOwner> capture;
    GameplayExecutionContext execution;
    ClockObservation clock;
    std::uint64_t generation{};
};
inline auto pausedScenario() -> std::unique_ptr<PausedScenario> {

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
inline void closePausedScenario(PausedScenario& scene) {
    auto& worker = scene.worker;
    auto& loaded = scene.loaded;

    close(*worker, loaded.host, finalizedContext(loaded.host, 2), 3, 3);
}
inline void seekPausedScenario(PausedScenario& scene, bool sourceBinding) {
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
    auto alternate = std::get<GameplayReplay>(send(*worker, SessionArchive{}));
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
    if (sourceBinding) {
        auto source = ReplayIdentityAccess::source(archive);
        REQUIRE(source);
        seek.target.replaySourceIdentity = *source;
    } else {
        seek.target.archiveIdentity = PreparedSemanticIdentity{core::detail::sha256(*bytes)};
    }
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
    const auto rejected = [&](const GameplayReplay& candidate, GameplayReplayCut candidateCut,
                              const StepRequest& attempted) {
        const auto generationBefore =
            std::get<SessionInspection>(send(*worker, SessionInspect{})).generation;
        auto command = std::make_unique<SessionCommand>(SessionSeek{
            attempted, proof, candidate, horizon, candidateCut, {}, {4}, {0.004, 0, 1}});
        const auto posted = worker->post(std::move(*command), true);
        REQUIRE(posted);
        CHECK_FALSE(await(*worker, *posted));
        CHECK(std::get<SessionInspection>(send(*worker, SessionInspect{})).generation ==
              generationBefore);
    };
    if (sourceBinding)
        rejected(alternate, archiveCut, step);
    auto wrongCut = archiveCut;
    ++wrongCut.completeRecords;
    rejected(archive, wrongCut, step);
    auto missing = step;
    missing.target.archiveIdentity.reset();
    missing.target.replaySourceIdentity.reset();
    rejected(archive, archiveCut, missing);
    auto both = step;
    both.target.archiveIdentity = PreparedSemanticIdentity{core::detail::sha256(*bytes)};
    both.target.replaySourceIdentity = *ReplayIdentityAccess::source(archive);
    rejected(archive, archiveCut, both);
    auto invalidToken = missing;
    invalidToken.target.replaySourceIdentity = ReplaySourceIdentity{};
    rejected(archive, archiveCut, invalidToken);
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
    if (sourceBinding)
        rejected(alternate, archiveCut, step);
    rejected(archive, wrongCut, step);
    rejected(archive, archiveCut, both);
    rejected(archive, archiveCut, missing);
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
inline void resumePausedScenario(PausedScenario& scene) {
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

} // namespace rpa_b5_paused_fixture

using namespace rpa_b5_paused_fixture;
