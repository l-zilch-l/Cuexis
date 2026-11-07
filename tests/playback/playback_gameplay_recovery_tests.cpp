#include "playback_gameplay_fixture.hpp"

using namespace playback_gameplay_fixture;

// Candidate registry, provenance, capability, lifetime and graph cases.

TEST_CASE("Candidate public actual Ruleset registry and Hook use original finite Fold",
          "[candidate][gameplay][registry][hook]") {
    TestContent f;
    f.config.actualRulesetBuild = "forged-build";
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    f.config.actualRulesetBuild = "cuexis.finite-fold.1";
    f.config.modules.push_back({"hook", "1", "cuexis.finite-fold.1"});
    f.config.hooks = {{"fold.bonus.u64",
                       playback::GameplayHookConsumer::Fold,
                       playback::GameplayCombine::Maximum,
                       {"hook"},
                       3,
                       false}};
    playback::PlaybackSession s;
    f.load(s);
    const auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    REQUIRE(s.advanceGameplay({8}, {8}, {8, 0, 0}));
    auto q = s.queryGameplay();
    REQUIRE(q);
    const auto& k = playback::detail::GameplayAccess::result(*q)->kernel;
    REQUIRE(k.fold);
    CHECK(k.fold->produced.size() == 1);
    CHECK(k.fold->bonus == 0);
    REQUIRE(s.advanceGameplay({9}, {9}, {9, 1, 0}));
    auto next = s.queryGameplay();
    REQUIRE(next);
    const auto& folded = playback::detail::GameplayAccess::result(*next)->kernel;
    REQUIRE(folded.fold);
    CHECK(folded.fold->bonus == 3);
    auto archive = s.archiveGameplay();
    REQUIRE(archive);
    auto replay = s.evaluateGameplayReplay(*archive);
    REQUIRE(replay);
    CHECK(*next->sameResult(replay->result));
    f.config.hooks[0].consumer = playback::GameplayHookConsumer::Kernel;
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    f.config.hooks[0].consumer = playback::GameplayHookConsumer::Fold;
    f.config.hooks[0].stepRoute = true;
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
}

TEST_CASE("Explicit GameplayOnly uses identical kernel without a World",
          "[candidate][gameplay][headless]") {
    TestContent f;
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto source = playback::PlaybackSource::fromGameplayPacked(
        "headless", "compiled/main.packed", f.packed(), f.config, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(source);
    playback::PlaybackSession only{{.version = 1, .ids = {}}}, visual;
    f.load(visual);
    auto p = only.prepareLoad(std::move(*source), playback::PlaybackMode::ChartClock);
    REQUIRE(p);
    REQUIRE(only.commit(std::move(*p)));
    REQUIRE(only.advanceGameplay({20}, {20}, {20, 0, 0}));
    REQUIRE(visual.advanceGameplay({20}, {20}, {20, 0, 0}));
    auto query = only.queryGameplay();
    REQUIRE(query);
    CHECK(*query->sameResult(*visual.queryGameplay()));
    CHECK_FALSE(only.extractFrame({640, 480}));
    CHECK_FALSE(only.update({21, 1, 0}));
    CHECK_FALSE(only.gameplayAdvanceReceipt()->runtimeUpdated);
    auto archive = only.archiveGameplay();
    REQUIRE(archive);
    REQUIRE(only.seekGameplay(*archive, {20}, {20}, {20, 0, 1}));
    CHECK(*query->sameResult(*only.queryGameplay()));
    REQUIRE(only.controlGameplay(playback::GameplayControl::Reset));
    CHECK(*only.queryGameplay()->factCount() == 0);
}

TEST_CASE("Candidate Stop discards pending and contacts without invented Facts",
          "[candidate][gameplay][control]") {
    TestContent f;
    playback::PlaybackSession s;
    f.load(s);
    auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    auto old = s.archiveGameplay();
    REQUIRE(old);
    REQUIRE(s.controlGameplay(playback::GameplayControl::Stop));
    CHECK(*s.gameplayState() == playback::GameplayState::Paused);
    CHECK(*s.queryGameplay()->factCount() == 0);
    CHECK_FALSE(s.advanceGameplay({20}, {20}, {20, 0, 0}));
    REQUIRE(s.controlGameplay(playback::GameplayControl::Resume));
    REQUIRE(s.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK(s.queryGameplay()->score()->misses == 1);
    auto historical = s.evaluateGameplayReplay(*old);
    REQUIRE(historical);
    CHECK(historical->evidenceValid);
    CHECK(*historical->result.factCount() == 0);
}

TEST_CASE("Candidate input retains provenance without moving logical observation",
          "[candidate][gameplay][input]") {
    TestContent f;
    playback::PlaybackSession a, b;
    f.load(a);
    f.load(b);
    auto first = input(), second = input();
    second.rawTimestamps = {INT64_MIN, INT64_MAX, -7, 123};
    REQUIRE(a.submitGameplay(std::span{&first, 1}));
    REQUIRE(b.submitGameplay(std::span{&second, 1}));
    REQUIRE(a.advanceGameplay({20}, {20}, {20, 0, 0}));
    REQUIRE(b.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK(*a.queryGameplay()->sameResult(*b.queryGameplay()));
    TestContent missing;
    missing.config.enabledCapabilities = {"unregistered"};
    auto unknown = playback::GameplayContent::fromPacked(missing.packed(), missing.config);
    REQUIRE_FALSE(unknown);
    CHECK(unknown.error().code() == "capability.unknown");
    second.sequence = 2;
    second.observationTick = {21};
    second.crossedSamplingGap = true;
    auto rejected = b.submitGameplay(std::span{&second, 1});
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "input.continuous_unsupported");
    CHECK(*a.queryGameplay()->sameResult(*b.queryGameplay()));
}

TEST_CASE("Candidate capability machine description matches actual seven-field registry",
          "[candidate][gameplay][registry]") {
    auto registry = playback::gameplayCapabilities();
    REQUIRE(registry);
    REQUIRE(registry->size() == 2);
    std::ifstream file{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                       "schemas/cuexis.gameplay-capabilities.v1.json"};
    REQUIRE(file);
    const std::string text{std::istreambuf_iterator<char>{file}, {}};
    auto data = json::parse(text, {1024 * 1024, 64, 1024 * 1024});
    REQUIRE(data);
    REQUIRE(data->find("capabilities"));
    REQUIRE(data->find("capabilities")->array());
    const auto& row = data->find("capabilities")->array()->at(0);
    const auto& actual = registry->at(0);
    CHECK(*row.find("id")->string() == actual.id);
    CHECK(*row.find("revision")->string() == actual.revision);
    CHECK(*row.find("build")->string() == actual.build);
    const auto* record = row.find("record");
    REQUIRE(record);
    REQUIRE(record->object());
    CHECK(record->object()->size() == 7);
    CHECK(*record->find("semanticKind")->string() == actual.record.semanticKind);
    CHECK(*record->find("requiredFormat")->string() == actual.record.requiredFormat);
    CHECK(*record->find("staticBudget")->string() == actual.record.staticBudget);
    CHECK(*record->find("snapshotCost")->string() == actual.record.snapshotCost);
    CHECK(*record->find("replayImpact")->string() == actual.record.replayImpact);
    CHECK(*record->find("stableRejectCode")->string() == actual.record.stableRejectCode);
    CHECK(record->find("supportedDomains")->array()->size() ==
          actual.record.supportedDomains.size());
    CHECK(*record->find("supportedDomains")->array()->at(0).string() ==
          actual.record.supportedDomains[0]);
    TestContent f;
    f.config.enabledCapabilities.clear();
    auto disabled = playback::GameplayContent::fromPacked(f.packed(), f.config);
    REQUIRE_FALSE(disabled);
    CHECK(disabled.error().code() == "capability.disabled");
    f.config.enabledCapabilities = {"cuexis.gameplay.t4-k4.v1"};
    f.request.assembly.closureContributions.capabilities[0].revision = "unexpected";
    f.request.assembly.declaredCapabilities.capabilities[0].revision = "unexpected";
    f.request.assembly.sources[0].document.requirements[0].required.capabilities[0].revision =
        "unexpected";
    auto revision = playback::GameplayContent::fromPacked(f.packed(), f.config);
    REQUIRE_FALSE(revision);
    CHECK(revision.error().code() == "capability.revision_mismatch");
}

TEST_CASE(
    "Candidate optional target failure preserves complete kernel and reports registered severity",
    "[candidate][gameplay][presentation]") {
    TestContent f;
    const auto& e = f.request.assembly.sources[0].document.requirements[0].identity;
    playback::GameplayRequirementRef ref{e.chartEntryId, e.invocationId,       e.moduleId,
                                         e.exportId,     e.requirementLocalId, {}};
    for (const auto& step : e.emissionPath)
        ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
    TestContent plain;
    f.config.bindings = {{"binding.one",
                          ref,
                          playback::GameplayPhase::Tap,
                          playback::GameplayOutcome::Hit,
                          playback::GameplayTimingClass::Any,
                          "absent-presentation-target",
                          false,
                          {0},
                          {}}};
    playback::PlaybackSession missing, control;
    f.load(missing);
    plain.load(control);
    const auto event = input();
    REQUIRE(missing.submitGameplay(std::span{&event, 1}));
    REQUIRE(control.submitGameplay(std::span{&event, 1}));
    REQUIRE(missing.advanceGameplay({20}, {20}, {20, 0, 0}));
    REQUIRE(control.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK(*missing.queryGameplay()->sameResult(*control.queryGameplay()));
    auto diagnostics = missing.lastOperationDiagnostics();
    REQUIRE(diagnostics);
    REQUIRE(diagnostics->items().size() == 1);
    CHECK(diagnostics->items()[0].code() == "presentation-target-missing");
    CHECK(diagnostics->items()[0].severity() == core::DiagnosticSeverity::Error);
    CHECK(*missing.gameplayState() == playback::GameplayState::Running);
    auto frame = missing.extractFrame({640, 480});
    REQUIRE(frame);
    CHECK(frame->objects[0].visible);
}

TEST_CASE("Candidate public installed-consumer fixture",
          "[candidate][.candidate-gameplay-fixture]") {
    TestContent f;
    auto bytes = f.packed();
    const auto root = std::filesystem::path{CUEXIS_S7A78_FIXTURE_DIR};
    std::filesystem::create_directories(root);
    std::ofstream file{root / "main.packed", std::ios::binary | std::ios::trunc};
    REQUIRE(file);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    REQUIRE(file);
    file.close();
    REQUIRE(f.content().valid());
    const auto config = playback::encodeGameplayConfiguration(f.config);
    REQUIRE(config);
    std::ofstream configurationFile{root / "configuration.json",
                                    std::ios::binary | std::ios::trunc};
    configurationFile << *config;
    REQUIRE(configurationFile.good());
    chart::CanonicalSemanticChart foundation;
    foundation.chartId = {"019a0000-0000-7000-8000-000000000001"};
    auto foundationBytes = chart::packed::encode(foundation);
    REQUIRE(foundationBytes);
    std::ofstream foundationFile{root / "foundation.packed", std::ios::binary | std::ios::trunc};
    foundationFile.write(reinterpret_cast<const char*>(foundationBytes->data()),
                         static_cast<std::streamsize>(foundationBytes->size()));
    REQUIRE(foundationFile.good());
    f.chart.mainMusic = chart::AssetId{"audio.main"};
    f.chart.resourceClosure.resources = {
        {{"audio.main"}, chart::CanonicalResourceUseKind::MainMusic}};
    const auto musicPacked = f.packed();
    std::ofstream musicFile{root / "music.packed", std::ios::binary | std::ios::trunc};
    musicFile.write(reinterpret_cast<const char*>(musicPacked.data()),
                    static_cast<std::streamsize>(musicPacked.size()));
    REQUIRE(musicFile.good());
    foundation.mainMusic = chart::AssetId{"audio.main"};
    foundation.resourceClosure.resources = f.chart.resourceClosure.resources;
    foundationBytes = chart::packed::encode(foundation);
    REQUIRE(foundationBytes);
    std::ofstream musicFoundation{root / "music-foundation.packed",
                                  std::ios::binary | std::ios::trunc};
    musicFoundation.write(reinterpret_cast<const char*>(foundationBytes->data()),
                          static_cast<std::streamsize>(foundationBytes->size()));
    REQUIRE(musicFoundation.good());
}

TEST_CASE("Candidate typed lifetime is independent of presentation frame cadence",
          "[candidate][gameplay][presentation][golden]") {
    TestContent f;
    const auto& e = f.request.assembly.sources[0].document.requirements[0].identity;
    playback::GameplayRequirementRef ref{e.chartEntryId, e.invocationId,       e.moduleId,
                                         e.exportId,     e.requirementLocalId, {}};
    for (const auto& step : e.emissionPath)
        ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
    f.config.bindings = {{"binding.one",
                          ref,
                          playback::GameplayPhase::Tap,
                          playback::GameplayOutcome::Hit,
                          playback::GameplayTimingClass::Any,
                          "019b0000-0000-7abc-8def-000000000002",
                          false,
                          {10},
                          playback::GameplayPresentationTick{20}}};
    playback::PlaybackSession dense, sparse, skipped;
    for (auto* session : {&dense, &sparse, &skipped}) {
        f.load(*session);
        const auto event = input();
        REQUIRE(session->submitGameplay(std::span{&event, 1}));
        REQUIRE(session->advanceGameplay({20}, {8}, {8, 0, 0}));
    }
    const auto visible = [](playback::PlaybackSession& session) {
        auto frame = session.extractFrame({640, 480});
        REQUIRE(frame);
        REQUIRE(frame->objects.size() == 1);
        return frame->objects[0].visible;
    };
    CHECK(visible(dense));
    CHECK(visible(sparse));
    REQUIRE(dense.advanceGameplay({20}, {9}, {9, 1, 0}));
    for (auto* session : {&dense, &sparse}) {
        REQUIRE(session->advanceGameplay({20}, {10}, {10, 0, 0}));
        CHECK_FALSE(visible(*session));
    }
    for (std::int64_t t : {11, 15, 19}) {
        REQUIRE(dense.advanceGameplay({20}, {t}, {static_cast<double>(t), 0, 0}));
        CHECK_FALSE(visible(dense));
    }
    for (auto* session : {&dense, &sparse, &skipped}) {
        REQUIRE(session->advanceGameplay({20}, {20}, {20, 0, 0}));
        CHECK(visible(*session));
        CHECK_FALSE(session->gameplayAdvanceReceipt()->publicationFailureStage);
    }
    // Sparse and skipped render calls do not re-admit input or change the actual kernel/Fold.
    const auto result = dense.queryGameplay();
    REQUIRE(result);
    for (auto* session : {&sparse, &skipped}) {
        const auto other = session->queryGameplay();
        REQUIRE(other);
        CHECK(*result->sameResult(*other));
    }
    CHECK(result->score()->hits == 1);
    CHECK(result->score()->score == 2);
}

TEST_CASE("Candidate timing and Miss projection follow manual phase-local golden",
          "[candidate][gameplay][presentation][golden]") {
    struct Golden {
        std::optional<std::int64_t> observation;
        playback::GameplayTimingClass timing;
        std::optional<std::int64_t> error;
    };
    const std::array<Golden, 4> goldens{{
        {4, playback::GameplayTimingClass::Early, -1},
        {5, playback::GameplayTimingClass::Exact, 0},
        {6, playback::GameplayTimingClass::Late, 1},
        {{}, playback::GameplayTimingClass::Any, {}},
    }};
    for (const auto& golden : goldens) {
        for (bool matches : {false, true}) {
            TestContent f;
            const auto& e = f.request.assembly.sources[0].document.requirements[0].identity;
            playback::GameplayRequirementRef ref{e.chartEntryId, e.invocationId,       e.moduleId,
                                                 e.exportId,     e.requirementLocalId, {}};
            for (const auto& step : e.emissionPath)
                ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
            const auto outcome = golden.observation ? playback::GameplayOutcome::Hit
                                                    : playback::GameplayOutcome::Miss;
            auto timing = golden.timing;
            if (!matches)
                timing = timing == playback::GameplayTimingClass::Early
                             ? playback::GameplayTimingClass::Late
                             : playback::GameplayTimingClass::Early;
            f.config.bindings = {{"binding.one",
                                  ref,
                                  playback::GameplayPhase::Tap,
                                  outcome,
                                  timing,
                                  "019b0000-0000-7abc-8def-000000000002",
                                  false,
                                  {0},
                                  {}}};
            playback::PlaybackSession session;
            f.load(session);
            if (golden.observation) {
                auto event = input();
                event.observationTick.value = *golden.observation;
                REQUIRE(session.submitGameplay(std::span{&event, 1}));
            }
            REQUIRE(session.advanceGameplay({20}, {0}, {0, 0, 0}));
            const auto result = session.queryGameplay();
            REQUIRE(result);
            REQUIRE(*result->factCount() == 1);
            const auto& fact = std::get<PhaseOutcomeFact>(
                playback::detail::GameplayAccess::result(*result)->kernel.facts[0]);
            REQUIRE(fact.error.has_value() == golden.error.has_value());
            if (golden.error)
                CHECK(fact.error->value() == *golden.error);
            CHECK(result->score()->score == (golden.observation ? 2 : -1));
            auto frame = session.extractFrame({640, 480});
            REQUIRE(frame);
            REQUIRE(frame->objects.size() == 1);
            CHECK(frame->objects[0].visible == !matches);
            auto replay = session.archiveGameplay();
            REQUIRE(replay);
            auto evaluated = session.evaluateGameplayReplay(*replay);
            REQUIRE(evaluated);
            CHECK(evaluated->evidenceValid);
            CHECK(*result->sameResult(evaluated->result));
        }
    }
}

TEST_CASE("Candidate typed Graph source uses the same actual prepare kernel Fold and recovery",
          "[candidate][gameplay][entry][graph]") {
    TestContent f;
    auto prepared = prepareResolvedGameplay(f.request);
    REQUIRE(prepared);
    auto graph =
        gameplay_packed::encodeGraph({f.chart,
                                      *prepared,
                                      f.owners,
                                      {"normalization.one", "coordinator.policy.greedy_v1"},
                                      {},
                                      {},
                                      3},
                                     {131072, 8192, 8192, true});
    REQUIRE(graph);
    std::vector<std::byte> bytes(graph->size());
    std::memcpy(bytes.data(), graph->data(), graph->size());
    const playback::GameplayGraphDecodeBudget budget{131072, 64, 8192, 16384, 8192, 8192, 65536};
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    for (const auto intent : {playback::GameplayPrepareIntent::Presentation,
                              playback::GameplayPrepareIntent::GameplayOnly}) {
        auto graphSource = playback::PlaybackSource::fromGameplayGraph(
            "typed-graph", "compiled/main.graph", bytes, f.config, budget, {}, *provider, intent);
        REQUIRE(graphSource);
        auto packedSource = playback::PlaybackSource::fromGameplayPacked(
            "typed-packed", "compiled/main.packed", f.packed(), f.config, {}, *provider, intent);
        REQUIRE(packedSource);
        playback::PlaybackSession graphSession, packedSession;
        auto graphCandidate =
            graphSession.prepareLoad(std::move(*graphSource), playback::PlaybackMode::ChartClock);
        REQUIRE(graphCandidate);
        CHECK_FALSE(graphSession.queryGameplay());
        REQUIRE(graphSession.commit(std::move(*graphCandidate)));
        REQUIRE(packedSession.load(std::move(*packedSource), playback::PlaybackMode::ChartClock));
        const auto event = input();
        REQUIRE(graphSession.submitGameplay(std::span{&event, 1}));
        REQUIRE(packedSession.submitGameplay(std::span{&event, 1}));
        for (const std::int64_t h : {4, 5, 20}) {
            REQUIRE(graphSession.advanceGameplay({h}, {h}, {static_cast<double>(h), 0, 0}));
            REQUIRE(packedSession.advanceGameplay({h}, {h}, {static_cast<double>(h), 0, 0}));
            CHECK(*graphSession.queryGameplay()->sameResult(*packedSession.queryGameplay()));
        }
        const auto result = graphSession.queryGameplay();
        REQUIRE(result);
        CHECK(result->score()->score == 2);
        CHECK(result->score()->hits == 1);
        auto archive = graphSession.archiveGameplay();
        REQUIRE(archive);
        const auto replay = graphSession.evaluateGameplayReplay(*archive);
        REQUIRE(replay);
        CHECK(replay->evidenceValid);
        CHECK(*replay->result.sameResult(*result));
        const auto snapshot = graphSession.snapshotGameplay();
        REQUIRE(snapshot);
        REQUIRE(graphSession.controlGameplay(playback::GameplayControl::Pause));
        REQUIRE(graphSession.restoreGameplay(*snapshot, {20}, {20, 0, 0}));
        CHECK(*graphSession.queryGameplay()->sameResult(*result));
        auto checkpoint = graphSession.checkpointGameplay(*archive, {20});
        REQUIRE(checkpoint);
        const auto cut = archive->cutAt({20});
        REQUIRE(cut);
        REQUIRE(graphSession.seekGameplay(*archive, {20}, *cut, std::span{&*checkpoint, 1}, {20},
                                          {20, 0, 0}));
        CHECK(*graphSession.queryGameplay()->sameResult(*result));
        REQUIRE(graphSession.controlGameplay(playback::GameplayControl::Reset));
        REQUIRE(graphSession.advanceGameplay({20}, {20}, {20, 0, 0}));
        CHECK(graphSession.queryGameplay()->score()->misses == 1);
    }
    auto rejected = playback::GameplayContent::fromGraph(R"({"GPR0":[invalid)", f.config, budget);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.structure.invalid");
    bool category = false, severity = false;
    for (const auto& context : rejected.error().context()) {
        if (context.key == "category" && context.value == "invalid_relation")
            category = true;
        if (context.key == "severity" && context.value == "error")
            severity = true;
    }
    CHECK(category);
    CHECK(severity);
}
