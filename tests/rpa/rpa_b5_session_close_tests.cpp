#include "rpa_b5_session_close_fixture.hpp"
#include "rpa_b5_session_fixture.hpp"

// Close retirement, binding and fence rejection cases.

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
