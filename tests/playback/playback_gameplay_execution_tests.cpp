#include "playback_gameplay_fixture.hpp"

using namespace playback_gameplay_fixture;

TEST_CASE("Candidate realtime execution uses explicit profile and coherent clock",
          "[candidate][gameplay][rpa][b3]") {
    TestContent f;
    auto& a = f.request.assembly;
    a.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
    a.identityDeclarations.engine.executionProfileToken = a.executionProfile;
    a.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
    a.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    a.capabilityContext = {{"cuexis.gameplay.rpa.v1"}, {"cuexis.gameplay.rpa.v1"}};
    a.declaredCapabilities = {{{"cuexis.gameplay.rpa.v1", "1"}}};
    a.closureContributions.capabilities = {{"cuexis.gameplay.rpa.v1", "1"}};
    a.sources[0].document.requirements[0].required.capabilities = {{"cuexis.gameplay.rpa.v1", "1"}};
    f.config.engine[1] = "fact.semantic.rpa.v1";
    f.config.engine[4] = a.executionProfile;
    f.config.engine[5] = "late.window.batch.v1";
    f.config.enabledCapabilities = {"cuexis.gameplay.rpa.v1"};
    const playback::ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    auto calibration = playback::RealtimeClock::calibrationIdentity(mapping);
    REQUIRE(calibration);
    f.config.calibration = *calibration;
    const auto packed = f.packed();
    if (const auto exportFile = test_support::environmentVariable("CUEXIS_B3_EXPORT_FILE");
        !exportFile.empty()) {
        std::ofstream file{exportFile, std::ios::binary};
        REQUIRE(file);
        file.write(reinterpret_cast<const char*>(packed.data()),
                   static_cast<std::streamsize>(packed.size()));
        REQUIRE(file);
    }
    if (const auto exportDirectory =
            test_support::environmentVariable("CUEXIS_B5_REFERENCE_DIRECTORY");
        !exportDirectory.empty()) {
        const std::filesystem::path directory{exportDirectory};
        std::filesystem::create_directories(directory / "compiled");
        std::filesystem::copy(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                  "tests/fixtures/chart_format_update/static_project/assets",
                              directory / "assets",
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
        auto content = playback::GameplayContent::fromPacked(packed, f.config);
        REQUIRE(content);
        auto metadata = playback::detail::gameplayEntryMetadata(
            *content, f.config, "compiled/main.packed", "packed-chart", packed, {});
        REQUIRE(metadata);
        auto project = json::parse(
            R"({"format":"cuexis.project","version":1,"projectId":"019f0000-0000-7abc-8def-000000000c40","assetRoots":[{"id":"main","path":"assets"}],"entry":{"chart":{"root":"main","path":"charts/main.cuexis.chart.json"}},"extensions":{}})",
            {65536, 32, 8192});
        REQUIRE(project);
        project->find("extensions")
            ->object()
            ->emplace("cuexis.gameplay-entry.v1",
                      json::Value{json::Value::Object{
                          {"entries", json::Value{json::Value::Array{*metadata}}}}});
        auto text = json::serialize(*project);
        REQUIRE(text);
        std::ofstream projectFile{directory / "cuexis.project.json"};
        projectFile << *text;
        REQUIRE(projectFile);
        std::ofstream packedFile{directory / "compiled/main.packed", std::ios::binary};
        packedFile.write(reinterpret_cast<const char*>(packed.data()),
                         static_cast<std::streamsize>(packed.size()));
        REQUIRE(packedFile);
        auto configuration = playback::encodeGameplayConfiguration(f.config);
        REQUIRE(configuration);
        std::ofstream configurationFile{directory / "configuration.json"};
        configurationFile << *configuration;
        REQUIRE(configurationFile);
    }
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto source = playback::PlaybackSource::fromGameplayPacked(
        "b3", "compiled/main.packed", packed, f.config, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(source);
    playback::PlaybackSession s;
    REQUIRE(s.load(std::move(*source), playback::PlaybackMode::ChartClock));
    const playback::GameplayExecutionContext scope{1, 1, 1};
    auto mismatchedMapping = mapping;
    mismatchedMapping.inputCompensationUs = {1, 1};
    auto mismatch =
        s.beginRealtimeGameplay({1000, 100, 64, "", true}, mismatchedMapping, scope, "test.clock");
    REQUIRE_FALSE(mismatch);
    CHECK(mismatch.error().code() == "host.rpa.calibration_mismatch");
    REQUIRE(s.beginRealtimeGameplay({1000, 100, 64, "", true}, {{0, 1}, {0, 1}, {0, 1}, {1, 1}},
                                    scope, "test.clock"));
    const playback::ClockObservation observation{"test.clock",
                                                 1,
                                                 1,
                                                 0,
                                                 1000,
                                                 0,
                                                 playback::ClockFramePosition{20, 1000000},
                                                 playback::ClockFramePosition{20, 1000000},
                                                 {},
                                                 playback::ClockPlaybackState::Playing,
                                                 playback::ClockQuality::Exact,
                                                 false,
                                                 false,
                                                 false};
    auto r = s.requestGameplayAdvance({scope, 1, 0, {20}}, observation, 0);
    REQUIRE(r);
    CHECK(r->disposition == playback::GameplayAdvanceDisposition::Waiting);
    REQUIRE(s.deliverGameplayCapture(
        {scope, 0, playback::GameplayClosedBatch{{5}, {6}, 0, 0, 1, 1, {{input(), 0}}}},
        observation, 0));
    REQUIRE(s.deliverGameplayCapture({scope, 1, playback::GameplayCapturePrefix{{21}}}, observation,
                                     1));
    r = s.queryGameplayAdvance(1);
    REQUIRE(r);
    CHECK(r->disposition == playback::GameplayAdvanceDisposition::Executed);
    auto query = s.queryGameplay();
    REQUIRE(query);
    auto score = query->score();
    REQUIRE(score);
    CHECK(score->score == 2);
    auto archive = s.archiveGameplay();
    REQUIRE(archive);
    auto evaluation = s.evaluateGameplayReplay(*archive);
    REQUIRE(evaluation);
    REQUIRE(query->sameResult(evaluation->result));
    CHECK(*query->sameResult(evaluation->result));
    auto lag = s.gameplayExecutionLag({23});
    REQUIRE(lag);
    CHECK(lag->horizonLag == 3);
    CHECK(lag->frontierLag == 3);
    CHECK(lag->foldedFactCursor == 1);
    auto snapshot = s.snapshotGameplay();
    REQUIRE(snapshot);
    auto bytes = snapshot->toBytes();
    REQUIRE(bytes);
    CHECK(std::to_integer<unsigned>((*bytes)[8]) == 3);
    REQUIRE(s.retireGameplayAdvance(1));
    r = s.requestGameplayAdvance({scope, 1, 0, {20}}, observation, 2000);
    REQUIRE_FALSE(r);
    CHECK(r.error().code() == "host.rpa.request_retired");
    // Confirmed delivery retransmission returns its original acknowledgement without sampling
    // again.
    REQUIRE(s.deliverGameplayCapture({scope, 1, playback::GameplayCapturePrefix{{21}}}, observation,
                                     2000));
    CHECK(*s.gameplayState() == playback::GameplayState::Running);
    SECTION("Future closure revokes execution") {
        auto future = s.deliverGameplayCapture({scope, 2, playback::GameplayCapturePrefix{{25}}},
                                               observation, 1);
        REQUIRE_FALSE(future);
        CHECK(future.error().code() == "host.rpa.message_conflict");
        CHECK(*s.gameplayState() == playback::GameplayState::Suspended);
    }
    SECTION("Unacknowledged request identity conflict exposes Suspended") {
        auto accepted = s.requestGameplayAdvance({scope, 2, 1, {21}}, observation, 1);
        REQUIRE(accepted);
        REQUIRE(accepted->disposition == playback::GameplayAdvanceDisposition::Executed);
        auto conflict = s.requestGameplayAdvance({scope, 2, 1, {22}}, observation, 2000);
        REQUIRE_FALSE(conflict);
        CHECK(conflict.error().code() == "host.rpa.message_conflict");
        CHECK(*s.gameplayState() == playback::GameplayState::Suspended);
        auto actual = s.queryGameplay();
        REQUIRE(actual);
        auto actualState = actual->state();
        REQUIRE(actualState);
        CHECK(*actualState == playback::GameplayState::Suspended);
    }
    SECTION("Unconfirmed delivery conflict exposes Suspended") {
        REQUIRE(s.deliverGameplayCapture({scope, 3, playback::GameplayCapturePrefix{{21}}},
                                         observation, 1));
        auto conflict = s.deliverGameplayCapture({scope, 3, playback::GameplayCapturePrefix{{20}}},
                                                 observation, 1);
        REQUIRE_FALSE(conflict);
        CHECK(*s.gameplayState() == playback::GameplayState::Suspended);
    }
    SECTION("Rejected Resume leaves the new segment available") {
        REQUIRE(s.acceptGameplayControl(scope, 1));
        REQUIRE(s.deliverGameplayCapture({scope, 2, playback::GameplayEpochEnd{{21}, 1}},
                                         observation, 1));
        REQUIRE(s.pauseRealtimeGameplay());
        const playback::GameplayReconciliation resume{{21}, 0, 1, 0, {}};
        auto first = observation;
        first.segment = 2;
        first.presented = playback::ClockFramePosition{22, 1000000};
        first.source = first.presented;
        for (const auto invalid : {playback::GameplayExecutionContext{2, 2, 2},
                                   playback::GameplayExecutionContext{1, 2, 1}}) {
            auto rejected = s.resumeRealtimeGameplay(resume, invalid, first, 1, true);
            REQUIRE_FALSE(rejected);
            CHECK(rejected.error().code() == "host.rpa.context_stale");
            CHECK(*s.gameplayState() == playback::GameplayState::Paused);
        }
        SECTION("Successful Resume conflict synchronizes Suspended") {
            REQUIRE(s.resumeRealtimeGameplay(resume, {1, 2, 2}, first, 1, true));
            CHECK(*s.gameplayState() == playback::GameplayState::Running);
            auto conflict = resume;
            ++conflict.pauseEpoch;
            REQUIRE_FALSE(s.resumeRealtimeGameplay(conflict, {1, 2, 2}, first, 1, true));
            CHECK(*s.gameplayState() == playback::GameplayState::Suspended);
        }
        SECTION("Aborted Resume remains observable after clock failure") {
            first.underrun = true;
            REQUIRE_FALSE(s.resumeRealtimeGameplay(resume, {1, 2, 2}, first, 1, true));
            CHECK(*s.gameplayState() == playback::GameplayState::ResumeAborted);
            REQUIRE_FALSE(s.requestGameplayAdvance({scope, 2, 2, {22}}, first, 1));
            CHECK(*s.gameplayState() == playback::GameplayState::ResumeAborted);
        }
    }
    SECTION("Different bound calibration rejects Snapshot and Replay") {
        auto otherConfig = f.config;
        otherConfig.calibration = *playback::RealtimeClock::calibrationIdentity(mismatchedMapping);
        auto otherSource = playback::PlaybackSource::fromGameplayPacked(
            "b3-other", "compiled/main.packed", packed, otherConfig, {}, *provider,
            playback::GameplayPrepareIntent::GameplayOnly);
        REQUIRE(otherSource);
        playback::PlaybackSession other;
        REQUIRE(other.load(std::move(*otherSource), playback::PlaybackMode::ChartClock));
        REQUIRE_FALSE(other.restoreGameplay(*snapshot, {20}, {20, 0, 0}));
        // Evaluation is self-contained; importing history into this session must reject it.
        REQUIRE_FALSE(other.seekGameplay(*archive, {20}, {20}, {20, 0, 0}));
    }
    REQUIRE_FALSE(s.advanceGameplay({21}, {21}, {21, 0, 0}));
    REQUIRE(s.unload());
    CHECK(archive->valid());
    auto wire = archive->toBytes();
    REQUIRE(wire);
    CHECK(std::to_integer<unsigned>((*wire)[8]) == 3);
}

TEST_CASE("R3b RPA checkpoint survives Playback wrapping and transactional replacement",
          "[candidate][gameplay][rpa][r3b]") {
    TestContent f;
    auto& a = f.request.assembly;
    const std::string capability = "cuexis.gameplay.rpa.v1";
    a.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
    a.identityDeclarations.engine.executionProfileToken = a.executionProfile;
    a.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
    a.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    a.capabilityContext = {{capability}, {capability}};
    a.declaredCapabilities = {{{capability, "1"}}};
    a.closureContributions.capabilities = {{capability, "1"}};
    a.sources[0].document.requirements[0].required.capabilities = {{capability, "1"}};
    f.config.engine[1] = "fact.semantic.rpa.v1";
    f.config.engine[4] = a.executionProfile;
    f.config.engine[5] = "late.window.batch.v1";
    f.config.enabledCapabilities = {capability};
    const auto content = f.content();
    const auto* immutable = playback::detail::GameplayAccess::content(content);
    REQUIRE(immutable);
    auto reference = JudgementSession::create();
    REQUIRE(reference);
    REQUIRE(reference->configure(immutable->configuration));
    REQUIRE(reference->prepare(immutable->capsule.gameplay));
    REQUIRE(reference->advance(Tick{20}));
    REQUIRE(reference->advance(Tick{21}));
    REQUIRE(reference->advance(Tick{22}));
    auto sourceArchive = reference->archive();
    REQUIRE(sourceArchive);
    auto archive = playback::detail::GameplayAccess::replay(*sourceArchive);
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto source = playback::PlaybackSource::fromGameplayPacked(
        "r3b-wrapper", "compiled/main.packed", f.packed(), f.config, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(source);
    playback::PlaybackSession session;
    REQUIRE(session.load(std::move(*source), playback::PlaybackMode::ChartClock));
    auto checkpoint = session.checkpointGameplay(archive, {20});
    REQUIRE(checkpoint);
    REQUIRE(
        playback::detail::GameplayAccess::checkpoint(*checkpoint)->checkpoint.state->kernel.rpa);
    auto cut = archive.cutAt({22});
    REQUIRE(cut);
    REQUIRE(session.seekGameplay(archive, {22}, *cut, {}, {22}, {0, 0, 1}));
    auto origin = session.snapshotGameplay();
    REQUIRE(origin);
    auto oldResult = session.queryGameplay();
    REQUIRE(oldResult);
    REQUIRE(session.seekGameplay(archive, {22}, *cut, std::span{&*checkpoint, 1}, {22}, {0, 0, 2}));
    auto restored = session.snapshotGameplay();
    REQUIRE(restored);
    CHECK(sameSnapshot(playback::detail::GameplayAccess::snapshot(*origin)->snapshot.state(),
                       playback::detail::GameplayAccess::snapshot(*restored)->snapshot.state()));
    CHECK(*oldResult->sameResult(*session.queryGameplay()));
    REQUIRE(session.unload());
    CHECK(checkpoint->valid());
    REQUIRE(archive.toBytes());
    CHECK(sameSnapshot(playback::detail::GameplayAccess::snapshot(*origin)->snapshot.state(),
                       playback::detail::GameplayAccess::snapshot(*restored)->snapshot.state()));
}

TEST_CASE("B4 actual K1 Pause and R2 Resume preserve exactly one software effect",
          "[candidate][gameplay][rpa][b4]") {
    TestContent f;
    auto& a = f.request.assembly;
    a.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
    a.identityDeclarations.engine.executionProfileToken = a.executionProfile;
    a.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
    a.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    a.capabilityContext = {{"cuexis.gameplay.rpa.v1"}, {"cuexis.gameplay.rpa.v1"}};
    a.declaredCapabilities = {{{"cuexis.gameplay.rpa.v1", "1"}}};
    a.closureContributions.capabilities = {{"cuexis.gameplay.rpa.v1", "1"}};
    a.sources[0].document.requirements[0].required.capabilities = {{"cuexis.gameplay.rpa.v1", "1"}};
    f.config.engine[1] = "fact.semantic.rpa.v1";
    f.config.engine[4] = a.executionProfile;
    f.config.engine[5] = "late.window.batch.v1";
    f.config.enabledCapabilities = {"cuexis.gameplay.rpa.v1"};
    const playback::ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    auto calibration = playback::RealtimeClock::calibrationIdentity(mapping);
    REQUIRE(calibration);
    f.config.calibration = *calibration;

    using namespace playback;
    using namespace playback::detail;
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto packed = f.packed();
    auto source =
        PlaybackSource::fromGameplayPacked("b4", "compiled/main.packed", packed, f.config, {},
                                           *provider, GameplayPrepareIntent::GameplayOnly);
    REQUIRE(source);
    PlaybackSession session;
    REQUIRE(session.load(std::move(*source), PlaybackMode::ChartClock));
    const GameplayExecutionContext execution{1, 1, 1};
    REQUIRE(
        session.beginRealtimeGameplay({1000, 100, 64, "", true}, mapping, execution, "test.clock"));
    ClockObservation clock{"test.clock",
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
    REQUIRE(session.requestGameplayAdvance({execution, 1, 0, {20}}, clock, 0));
    HostContext hostContext{1, 1, 1, 0, 1, 1, {}};
    auto host = HostCoordinator::create(hostContext, 12, 1);
    auto capture = CaptureRouteOwner::create(execution, 24, 4);
    auto software = OwnerStepLedger::create(1, Participant::software, 24, 4);
    REQUIRE(host);
    REQUIRE(capture);
    REQUIRE(software);
    ControlRequest pause;
    pause.context = hostContext;
    pause.context.controlSequence = 1;
    pause.operationId = 1;
    pause.commandOrdinal = 1;
    pause.kind = ControlKind::pause;
    pause.target.context = pause.context;
    pause.target.commandOrdinal = 1;
    pause.target.tick = 20;
    pause.target.pauseEpoch = 1;
    pause.participants = {true, true, false, false};
    auto invalid = pause;
    invalid.context.sessionScope = 2;
    invalid.target.context = invalid.context;
    REQUIRE_FALSE(HostCoordinationAccess::acceptControl(session, *host, invalid));
    CHECK(session.queryGameplayAdvance(1)->disposition == GameplayAdvanceDisposition::Waiting);
    REQUIRE(HostCoordinationAccess::acceptControl(session, *host, pause));
    CHECK(session.queryGameplayAdvance(1)->disposition ==
          GameplayAdvanceDisposition::CanceledNotExecuted);
    auto accepted = host->query(1);
    REQUIRE(accepted);
    REQUIRE(HostCoordinationAccess::acceptControl(session, *host, pause));
    CHECK(host->query(1)->decisionOrdinal == accepted->decisionOrdinal);
    REQUIRE(capture->capture(input()));
    StepRequest cut{{1, 1, Participant::capture, StepPhase::cut, 0}, pause.target};
    auto cutReceipt = capture->cut(cut);
    REQUIRE(cutReceipt);
    REQUIRE(host->observe(*cutReceipt));
    auto deliveries = capture->pending();
    REQUIRE(deliveries);
    for (const auto& delivery : *deliveries)
        REQUIRE(session.deliverGameplayCapture(delivery, clock, 0));
    REQUIRE(capture->acknowledge(execution, deliveries->back().deliverySequence));
    StepRequest sw{{1, 1, Participant::software, StepPhase::softwareCommit, 0}, pause.target};
    auto wrongCut = sw;
    wrongCut.target.tick = 999;
    REQUIRE_FALSE(HostCoordinationAccess::applyPause(session, *software, wrongCut));
    CHECK_FALSE(software->query(sw.key));
    const auto stateBeforePause = session.gameplayState();
    REQUIRE(stateBeforePause);
    REQUIRE_FALSE(HostCoordinationAccess::applyPause(session, *software, sw));
    CHECK_FALSE(software->query(sw.key));
    CHECK(session.gameplayState() == *stateBeforePause);
    auto pauseFence = HostCoordinationAccess::enterControlFence(session, *host->query(1));
    REQUIRE(pauseFence);
    REQUIRE_FALSE(session.update({20, 0, 0}));
    auto paused = HostCoordinationAccess::applyPause(session, *software, sw);
    REQUIRE(paused);
    REQUIRE(paused->status == StepStatus::applied);
    CHECK(session.gameplayState() == GameplayState::Paused);
    auto generation = HostCoordinationAccess::generation(session);
    REQUIRE(generation);
    REQUIRE(HostCoordinationAccess::applyPause(session, *software, sw));
    CHECK(*HostCoordinationAccess::generation(session) == *generation);
    REQUIRE(host->observe(*paused));
    REQUIRE(host->markSoftwareApplied());
    REQUIRE(host->finish());
    REQUIRE(HostCoordinationAccess::leaveFence(session, *pauseFence, *host->query(1)));
    auto result = session.queryGameplay();
    REQUIRE(result);
    const auto ordinal = GameplayAccess::result(*result)->kernel.rpa->nextControlOrdinal;
    auto reconciliation = capture->trace(ordinal);
    REQUIRE(reconciliation);
    ControlRequest resume = pause;
    resume.context.controlSequence = 2;
    resume.operationId = 2;
    resume.commandOrdinal = 2;
    resume.kind = ControlKind::resume;
    resume.target.context = resume.context;
    resume.target.commandOrdinal = 2;
    resume.target.cut.reset();
    resume.target.reconciliationOrdinal = ordinal;
    resume.target.nextContext = resume.context;
    resume.target.nextContext->clockSegment = 2;
    resume.target.nextContext->runEpoch = 2;
    resume.target.payloadIdentity.reset();
    REQUIRE(HostCoordinationAccess::acceptControl(session, *host, resume));
    // A real paused transition arrives after acceptance but before final route cut.
    auto late = input();
    late.observationTick.value = 20;
    late.action = GameplayInputAction::Release;
    REQUIRE(capture->capture(late));
    StepRequest route{{1, 2, Participant::capture, StepPhase::resumeRoute, 0}, resume.target};
    auto routeReceipt = capture->resumeRoute(route, {1, 2, 2});
    REQUIRE(routeReceipt);
    REQUIRE(host->observe(*routeReceipt));
    reconciliation = capture->trace(ordinal);
    REQUIRE(reconciliation);
    CHECK(reconciliation->finalTraceCut == 1);
    sw = {{1, 2, Participant::software, StepPhase::softwareCommit, 0}, resume.target};
    REQUIRE(routeReceipt->effects.payloadIdentity);
    sw.resumeTrace =
        ResumeTraceBinding{*routeReceipt->effects.cut, *routeReceipt->effects.payloadIdentity};
    auto nextClock = clock;
    nextClock.segment = 2;
    nextClock.presented = ClockFramePosition{22, 1000000};
    nextClock.source = nextClock.presented;
    REQUIRE_FALSE(HostCoordinationAccess::applyResume(session, *software, sw, *reconciliation,
                                                      {1, 2, 2}, nextClock, 0, &*routeReceipt));
    CHECK_FALSE(software->query(sw.key));
    CHECK(session.gameplayState() == GameplayState::Paused);
    auto resumeFence = HostCoordinationAccess::enterControlFence(session, *host->query(2));
    REQUIRE(resumeFence);
    auto pending = HostCoordinationAccess::applyResume(session, *software, sw, *reconciliation,
                                                       {1, 2, 2}, nextClock, 0, nullptr);
    REQUIRE(pending);
    CHECK(pending->status == StepStatus::pending);
    SECTION("Post-cut failure terminates continuation and preserves unadmitted input") {
        auto release = input();
        release.observationTick.value = 22;
        release.action = GameplayInputAction::Release;
        REQUIRE(capture->capture(release));
        auto invalidClock = nextClock;
        invalidClock.underrun = true;
        auto aborted = HostCoordinationAccess::applyResume(
            session, *software, sw, *reconciliation, {1, 2, 2}, invalidClock, 0, &*routeReceipt);
        REQUIRE(aborted);
        REQUIRE(aborted->status == StepStatus::failed);
        REQUIRE(host->observe(*aborted));
        REQUIRE(host->abortResume());
        capture->abortResume();
        CHECK(session.gameplayState() == GameplayState::ResumeAborted);
        CHECK(capture->retained()->front().input.observationTick.value == 22);
        REQUIRE_FALSE(capture->enableOrdinary(*host->query(2)));
        REQUIRE(host->completeCleanup());
        auto another = resume;
        another.operationId = 3;
        another.commandOrdinal = 3;
        another.context = *resume.target.nextContext;
        another.context.controlSequence = 3;
        another.target.context = another.context;
        another.target.commandOrdinal = 3;
        another.target.nextContext->controlSequence = 3;
        another.target.nextContext->clockSegment = 3;
        another.target.nextContext->runEpoch = 3;
        auto rejected = HostCoordinationAccess::acceptControl(session, *host, another);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "host.rpa.resume_aborted");
    }
    SECTION("All owners confirmed") {
        auto resumed = HostCoordinationAccess::applyResume(session, *software, sw, *reconciliation,
                                                           {1, 2, 2}, nextClock, 0, &*routeReceipt);
        REQUIRE(resumed);
        REQUIRE(resumed->status == StepStatus::applied);
        generation = HostCoordinationAccess::generation(session);
        REQUIRE(HostCoordinationAccess::applyResume(session, *software, sw, *reconciliation,
                                                    {1, 2, 2}, nextClock, 2000, &*routeReceipt));
        CHECK(*HostCoordinationAccess::generation(session) == *generation);
        REQUIRE(host->observe(*resumed));
        REQUIRE(host->markSoftwareApplied());
        REQUIRE(host->finish());
        REQUIRE(HostCoordinationAccess::leaveFence(session, *resumeFence, *host->query(2)));
        REQUIRE(capture->enableOrdinary(*host->query(2)));
        CHECK(host->state() == HostState::running);
        CHECK(session.gameplayState() == GameplayState::Running);
        auto advanceClock = nextClock;
        advanceClock.publicationSequence += 1;
        advanceClock.presented = ClockFramePosition{1020, 1000000};
        advanceClock.source = advanceClock.presented;
        auto advance = session.requestGameplayAdvance({{1, 2, 2}, 2, 3, {1020}}, advanceClock, 0);
        if (!advance) {
            INFO(advance.error().code() << ": " << advance.error().message());
            REQUIRE(advance);
        }
        auto nextPause = pause;
        nextPause.context = *host->query(2)->finalizedContext;
        nextPause.context.controlSequence = 3;
        nextPause.operationId = 3;
        nextPause.commandOrdinal = 4;
        nextPause.target.context = nextPause.context;
        nextPause.target.commandOrdinal = 4;
        nextPause.target.tick = 1020;
        nextPause.target.pauseEpoch = 2;
        REQUIRE(HostCoordinationAccess::acceptControl(session, *host, nextPause));
        CHECK(session.queryGameplayAdvance(2)->disposition ==
              GameplayAdvanceDisposition::CanceledNotExecuted);
    }
}

TEST_CASE("Revision4 explicit entry reuses verified Header identity and rejects encoding mismatch",
          "[candidate][gameplay][entry][revision4]") {
    TestContent fixture;
    const auto bytes = fixture.packed(4);
    CHECK(playback::detail::packedGameplayRevision(bytes) == 4);
    CHECK_FALSE(playback::detail::packedGameplayRevision(std::span{bytes}.first(95)));
    const auto content = playback::GameplayContent::fromPacked(bytes, fixture.config);
    REQUIRE(content);
    auto metadata = playback::detail::gameplayEntryMetadata(*content, fixture.config, "main.packed",
                                                            "packed-chart", bytes, {});
    REQUIRE(metadata);
    CHECK(*metadata->find("encoding")->string() == "capsule.4");
    const auto* storage = playback::detail::GameplayAccess::content(*content);
    REQUIRE(storage);
    REQUIRE(storage->capsule.verifiedSemanticDigest);
    CHECK(*metadata->find("compiledSemanticIdentity")->string() ==
          core::detail::sha256Hex(*storage->capsule.verifiedSemanticDigest));
    CHECK(std::equal(storage->capsule.verifiedSemanticDigest->begin(),
                     storage->capsule.verifiedSemanticDigest->end(), bytes.begin() + 32,
                     [](auto a, auto b) { return a == std::to_integer<std::uint8_t>(b); }));
    const playback::GameplayConfigurationDecodeBudget budget{65536, 32, 16384, 4096, 1024, true};
    const playback::GameplayGraphDecodeBudget graphBudget{65536, 32, 16384, 4096, 1024, 512, 16384};
    const auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    const auto consume = [&](const json::Value& value) {
        const auto text = json::serialize(value);
        REQUIRE(text);
        return playback::PlaybackSource::fromGameplayEntry(
            "revision4", *text, bytes, fixture.config, budget, graphBudget, {}, *provider,
            playback::GameplayPrepareIntent::GameplayOnly);
    };
    REQUIRE(consume(*metadata));
    for (const auto* encoding : {"capsule.3", "capsule.5", "graph.1"}) {
        auto wrong = *metadata;
        *wrong.find("encoding") = json::Value{encoding};
        CHECK_FALSE(consume(wrong));
    }
    const auto legacy = fixture.content();
    CHECK_FALSE(playback::detail::gameplayEntryMetadata(legacy, fixture.config, "main.packed",
                                                        "packed-chart", bytes, {}));
}

TEST_CASE("Revision4 metadata faults precede a shared prepare failure",
          "[candidate][gameplay][entry][revision4][metadata-order]") {
    const auto encodingError =
        core::Error{"playback.gameplay.invalid", "Gameplay metadata encoding/Header mismatch"}
            .withContext("category", "identity_closure_incomplete")
            .withContext("severity", "error")
            .withContext("faulted", "false")
            .withContext("fieldPath", "metadata.encoding");
    TestContent fixture;
    const auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    const playback::GameplayConfigurationDecodeBudget budget{65536, 32, 16384, 4096, 1024, true};
    const playback::GameplayGraphDecodeBudget graphBudget{65536, 32, 16384, 4096, 1024, 512, 16384};
    auto disabled = fixture.config;
    disabled.enabledCapabilities.clear();
    for (const auto revision : {3U, 4U}) {
        DYNAMIC_SECTION("revision " << revision) {
            const auto bytes = fixture.packed(revision);
            const auto content = playback::GameplayContent::fromPacked(bytes, fixture.config);
            REQUIRE(content);
            const auto metadata = playback::detail::gameplayEntryMetadata(
                *content, fixture.config, "main.packed", "packed-chart", bytes, {});
            REQUIRE(metadata);
            const auto consume = [&](const json::Value& value, const auto& configuration) {
                const auto text = json::serialize(value);
                REQUIRE(text);
                return playback::PlaybackSource::fromGameplayEntry(
                    "metadata-order", *text, bytes, configuration, budget, graphBudget, {},
                    *provider, playback::GameplayPrepareIntent::GameplayOnly);
            };
            REQUIRE(consume(*metadata, fixture.config));
            const auto prepareFailure = consume(*metadata, disabled);
            REQUIRE_FALSE(prepareFailure);
            CHECK(prepareFailure.error().code() == "capability.disabled");
            auto wrong = *metadata;
            *wrong.find("compiledSemanticIdentity") = json::Value{std::string(64, '0')};
            const auto semanticFailure = consume(wrong, disabled);
            REQUIRE_FALSE(semanticFailure);
            if (revision == 4) {
                checkExactError(semanticFailure.error(),
                                core::Error{"playback.gameplay.invalid",
                                            "Gameplay metadata semantic identity mismatch"}
                                    .withContext("fieldPath", "metadata.compiledSemanticIdentity")
                                    .withContext("category", "identity_closure_incomplete")
                                    .withContext("severity", "error")
                                    .withContext("faulted", "false")
                                    .withContext("sourceCode", "packed.metadata.mismatch"));
                wrong = *metadata;
                *wrong.find("encoding") = json::Value{"capsule.3"};
                const auto encodingFailure = consume(wrong, disabled);
                REQUIRE_FALSE(encodingFailure);
                checkExactError(encodingFailure.error(), encodingError);
                *wrong.find("artifactIdentity") = json::Value{std::string(64, '0')};
                const auto artifactFailure = consume(wrong, disabled);
                REQUIRE_FALSE(artifactFailure);
                CHECK(artifactFailure.error().message() == "Gameplay artifact hash mismatch");
            } else {
                CHECK(semanticFailure.error().code() == prepareFailure.error().code());
                CHECK(semanticFailure.error().message() == prepareFailure.error().message());
                wrong = *metadata;
                *wrong.find("encoding") = json::Value{"capsule.4"};
                const auto mismatch = consume(wrong, disabled);
                REQUIRE_FALSE(mismatch);
                checkExactError(mismatch.error(), encodingError);
            }
        }
    }
}

TEST_CASE("Graph1 metadata faults retain their existing prepare rejection order",
          "[candidate][gameplay][entry][metadata-order]") {
    TestContent fixture;
    const auto prepared = prepareResolvedGameplay(fixture.request);
    REQUIRE(prepared);
    const auto graph =
        gameplay_packed::encodeGraph({fixture.chart,
                                      *prepared,
                                      fixture.owners,
                                      {"normalization.one", "coordinator.policy.greedy_v1"},
                                      {},
                                      {},
                                      3},
                                     {65536, 8192, 8192, true});
    REQUIRE(graph);
    const auto view = std::as_bytes(std::span{graph->data(), graph->size()});
    const std::vector<std::byte> bytes(view.begin(), view.end());
    const playback::GameplayConfigurationDecodeBudget budget{65536, 32, 16384, 4096, 1024, true};
    const playback::GameplayGraphDecodeBudget graphBudget{65536, 32, 16384, 4096, 1024, 512, 16384};
    const auto content = playback::GameplayContent::fromGraph(*graph, fixture.config, graphBudget);
    REQUIRE(content);
    auto metadata = playback::detail::gameplayEntryMetadata(*content, fixture.config, "main.graph",
                                                            "gameplay-graph", bytes, {});
    REQUIRE(metadata);
    const auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto disabled = fixture.config;
    disabled.enabledCapabilities.clear();
    const auto consume = [&](const json::Value& value, const auto& configuration) {
        const auto text = json::serialize(value);
        REQUIRE(text);
        return playback::PlaybackSource::fromGameplayEntry(
            "graph-order", *text, bytes, configuration, budget, graphBudget, {}, *provider,
            playback::GameplayPrepareIntent::GameplayOnly);
    };
    REQUIRE(consume(*metadata, fixture.config));
    const auto prepareFailure = consume(*metadata, disabled);
    REQUIRE_FALSE(prepareFailure);
    CHECK(prepareFailure.error().code() == "capability.disabled");
    *metadata->find("compiledSemanticIdentity") = json::Value{std::string(64, '0')};
    const auto both = consume(*metadata, disabled);
    REQUIRE_FALSE(both);
    CHECK(both.error().code() == prepareFailure.error().code());
    CHECK(both.error().message() == prepareFailure.error().message());
    const auto metadataOnly = consume(*metadata, fixture.config);
    REQUIRE_FALSE(metadataOnly);
    CHECK(metadataOnly.error().message() == "Gameplay metadata closure mismatch");
}

TEST_CASE("Replay source binding owns only a token and preserves content validation",
          "[candidate][gameplay][source-binding]") {
    using playback::detail::replayBindingMatches;
    using playback::detail::ReplayIdentityAccess;
    TestContent fixture;
    const auto content = fixture.content();
    playback::PlaybackSession session;
    fixture.load(session);
    REQUIRE(session.advanceGameplay({20}, {20}, {20, 0, 0}));
    auto captured = session.archiveGameplay();
    REQUIRE(captured);
    auto copy = *captured;
    auto separate = session.archiveGameplay();
    REQUIRE(separate);
    const auto identity = ReplayIdentityAccess::source(*captured);
    REQUIRE(identity);
    REQUIRE(identity->valid());
    CHECK(*ReplayIdentityAccess::source(copy) == *identity);
    CHECK(*ReplayIdentityAccess::source(*separate) != *identity);
    CHECK(replayBindingMatches(copy, {}, *identity).value());
    CHECK_FALSE(replayBindingMatches(*separate, {}, *identity).value());
    CHECK_FALSE(replayBindingMatches(copy, {}, {}).value());
    CHECK_FALSE(replayBindingMatches(copy, {}, playback::ReplaySourceIdentity{}).value());
    CHECK_FALSE(ReplayIdentityAccess::source(playback::GameplayReplay{}));
    auto bytes = captured->toBytes();
    REQUIRE(bytes);
    const playback::PreparedSemanticIdentity digest{core::detail::sha256(*bytes)};
    CHECK(replayBindingMatches(*separate, digest, {}).value());
    CHECK_FALSE(replayBindingMatches(copy, digest, *identity).value());
    const playback::GameplayCodecBudget budget{1024 * 1024, 100, 100000, true};
    auto imported = playback::GameplayReplay::fromBytes(*bytes, content, budget);
    REQUIRE(imported);
    CHECK_FALSE(replayBindingMatches(*imported, {}, *identity).value());
    CHECK(replayBindingMatches(*imported, digest, {}).value());
    auto corrupted = *bytes;
    corrupted.back() ^= std::byte{1};
    CHECK_FALSE(playback::GameplayReplay::fromBytes(corrupted, content, budget));
    const auto before = session.queryGameplay();
    REQUIRE(before);
    const auto cut = imported->cutAt({20});
    REQUIRE(cut);
    REQUIRE(session.seekGameplay(*imported, {20}, *cut, {}, {20}, {20, 0, 1}));
    CHECK(before->sameResult(*session.queryGameplay()).value());
    // A source token identifies even an invalid internal test wrapper; it cannot certify it.
    auto mutableData = std::make_shared<ReplayData>(
        playback::detail::GameplayAccess::replay(*imported)->archive.data());
    ++mutableData->eventCount;
    auto untrusted = playback::detail::GameplayAccess::replay(ReplayArchive{
        mutableData,
        std::make_shared<RecoveryInputs>(
            playback::detail::GameplayAccess::replay(*imported)->archive.dependencies())});
    auto untrustedSource = ReplayIdentityAccess::source(untrusted);
    REQUIRE(untrustedSource);
    CHECK(replayBindingMatches(untrusted, {}, *untrustedSource).value());
    CHECK_FALSE(session.seekGameplay(untrusted, {20}, *cut, {}, {20}, {20, 0, 1}));
    CHECK(before->sameResult(*session.queryGameplay()).value());
    const std::weak_ptr<const ReplayData> history =
        playback::detail::GameplayAccess::replay(*captured)->archive.anchor();
    REQUIRE_FALSE(history.expired());
    REQUIRE(session.unload());
    *captured = {};
    copy = {};
    *separate = {};
    CHECK(history.expired());
    CHECK(identity->valid());
}
