#include "playback_gameplay_fixture.hpp"

using namespace playback_gameplay_fixture;

// Candidate admission, fold, snapshot, binding and codec cases.

TEST_CASE("Candidate Gameplay zero-input Miss and pure playback compatibility",
          "[candidate][gameplay][golden]") {
    TestContent f;
    playback::PlaybackSession live, pure;
    f.load(live);
    REQUIRE(pure.load(presentation(), playback::PlaybackMode::ChartClock));
    REQUIRE(live.advanceGameplay({20}, {20}, {20.0, 0.0, 0}));
    REQUIRE(pure.update({20.0, 0.0, 0}));
    auto q = live.queryGameplay();
    REQUIRE(q);
    auto score = q->score();
    REQUIRE(score);
    CHECK(score->score == -1);
    CHECK(score->hits == 0);
    CHECK(score->misses == 1);
    CHECK(*q->factCount() == 1);
    CHECK(*q->publishableFactCount() == 1);
    CHECK_FALSE(pure.queryGameplay());
    CHECK(*pure.gameplayState() == playback::GameplayState::Inactive);
    auto l = live.extractFrame({640, 480}), p = pure.extractFrame({640, 480});
    REQUIRE(l);
    REQUIRE(p);
    REQUIRE(l->objects.size() == 1);
    CHECK(playback::computeFrameDigest({20, 0, 0}, *l)->value ==
          playback::computeFrameDigest({20, 0, 0}, *p)->value);
    auto a = live.archiveGameplay();
    REQUIRE(a);
    auto e = live.evaluateGameplayReplay(*a);
    REQUIRE(e);
    CHECK(e->evidenceValid);
    CHECK(*q->sameResult(e->result));
    REQUIRE(live.unload());
    CHECK(*q->factCount() == 1);
}
TEST_CASE("Candidate Gameplay admission stales Prepared and frame rejection is atomic",
          "[candidate][gameplay][transaction]") {
    TestContent f;
    playback::PlaybackSession s;
    f.load(s);
    auto pending =
        s.prepareGameplayReload(presentation(), f.content(), playback::ReloadPolicy::KeepChartTime);
    REQUIRE(pending);
    const auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    CHECK_FALSE(s.commit(std::move(*pending)));
    auto old = s.queryGameplay();
    REQUIRE(old);
    CHECK_FALSE(s.advanceGameplay({20}, {20}, {std::numeric_limits<double>::quiet_NaN(), 0, 0}));
    auto now = s.queryGameplay();
    REQUIRE(now);
    CHECK(*old->sameResult(*now));
    CHECK_FALSE(s.gameplayAdvanceReceipt());
    REQUIRE(s.advanceGameplay({20}, {20}, {20.0, 0, 0}));
    auto q = s.queryGameplay();
    REQUIRE(q);
    auto score = q->score();
    REQUIRE(score);
    CHECK(score->score == 2);
    CHECK(score->combo == 1);
    CHECK(score->misses == 0);
    CHECK_FALSE(s.advanceGameplay({21}, {21}, {19.0, 0, 0}));
    CHECK(*q->sameResult(*s.queryGameplay()));
    CHECK(s.gameplayAdvanceReceipt()->requestedHorizon.value == 20);
}
TEST_CASE("Candidate Gameplay Fold failure keeps sealed Fact and full ReplayEvaluation",
          "[candidate][gameplay][fault]") {
    TestContent f;
    f.config.maximumScore = 1;
    playback::PlaybackSession s;
    f.load(s);
    const auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    CHECK_FALSE(s.advanceGameplay({20}, {20}, {20, 0, 0}));
    auto q = s.queryGameplay();
    REQUIRE(q);
    CHECK(*q->state() == playback::GameplayState::Faulted);
    CHECK(*q->factCount() == 1);
    CHECK(*q->publishableFactCount() == 0);
    CHECK(q->score()->score == 0);
    auto r = s.gameplayAdvanceReceipt();
    REQUIRE(r);
    CHECK(r->faultStage == playback::GameplayFaultStage::Fold);
    CHECK_FALSE(r->publicationFailureStage);
    CHECK_FALSE(r->runtimeUpdated);
    CHECK_FALSE(s.update({20, 0, 0}));
    CHECK_FALSE(s.snapshotGameplay());
    auto a = s.archiveGameplay();
    REQUIRE(a);
    auto e = s.evaluateGameplayReplay(*a);
    REQUIRE(e);
    CHECK(e->evidenceValid);
    CHECK(*q->sameResult(e->result));
    REQUIRE(s.controlGameplay(playback::GameplayControl::Reset));
    CHECK(*s.queryGameplay()->factCount() == 0);
}
TEST_CASE("Candidate Gameplay Snapshot exact Seek and pause share actual kernel",
          "[candidate][gameplay][recovery]") {
    TestContent f;
    playback::PlaybackSession s;
    f.load(s);
    const auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    auto snap = s.snapshotGameplay();
    REQUIRE(snap);
    REQUIRE(s.controlGameplay(playback::GameplayControl::Pause));
    CHECK_FALSE(s.submitGameplay(std::span{&event, 1}));
    CHECK_FALSE(s.update({1, 1, 0}));
    REQUIRE(s.controlGameplay(playback::GameplayControl::Resume));
    REQUIRE(s.advanceGameplay({20}, {20}, {20, 0, 0}));
    auto result = s.queryGameplay();
    REQUIRE(result);
    auto archive = s.archiveGameplay();
    REQUIRE(archive);
    REQUIRE(s.restoreGameplay(*snap, {20}, {20, 0, 0}));
    REQUIRE(s.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK(*result->sameResult(*s.queryGameplay()));
    REQUIRE(s.seekGameplay(*archive, {5}, {5}, {5, 0, 1}));
    CHECK(*s.queryGameplay()->factCount() == 0);
    REQUIRE(s.seekGameplay(*archive, {20}, {20}, {20, 0, 1}));
    CHECK(*result->sameResult(*s.queryGameplay()));
}

TEST_CASE("Candidate FactBinding intervals rebuild actual resolver state",
          "[candidate][gameplay][presentation]") {
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
                          {0},
                          playback::GameplayPresentationTick{10}},
                         {"binding.one",
                          ref,
                          playback::GameplayPhase::Tap,
                          playback::GameplayOutcome::Hit,
                          playback::GameplayTimingClass::Any,
                          "019b0000-0000-7abc-8def-000000000002",
                          false,
                          {20},
                          playback::GameplayPresentationTick{30}}};
    playback::PlaybackSession s;
    f.load(s);
    auto pending = s.snapshotGameplay();
    REQUIRE(pending);
    const auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    const auto visible = [&] {
        auto frame = s.extractFrame({640, 480});
        REQUIRE(frame);
        REQUIRE(frame->objects.size() == 1);
        return frame->objects[0].visible;
    };
    REQUIRE(s.advanceGameplay({8}, {8}, {8, 0, 0}));
    CHECK_FALSE(visible());
    auto replay = s.archiveGameplay();
    REQUIRE(replay);
    REQUIRE(s.advanceGameplay({20}, {10}, {10, 2, 0}));
    CHECK(visible());
    REQUIRE(s.advanceGameplay({21}, {20}, {20, 10, 0}));
    CHECK_FALSE(visible());
    REQUIRE(s.advanceGameplay({32}, {30}, {30, 10, 0}));
    CHECK(visible());
    auto before = s.queryGameplay();
    REQUIRE(before);
    CHECK_FALSE(s.advanceGameplay({33}, {5}, {31, 1, 0}));
    CHECK(*before->sameResult(*s.queryGameplay()));
    REQUIRE(s.seekGameplay(*replay, {8}, {8}, {8, 0, 1}));
    CHECK_FALSE(visible());
    REQUIRE(s.controlGameplay(playback::GameplayControl::Reset));
    CHECK(visible());
    REQUIRE(s.restoreGameplay(*pending, {8}, {8, 0, 1}));
    CHECK(visible());
}

TEST_CASE("Candidate facade matches independent source scan and finite Fold golden",
          "[candidate][gameplay][oracle]") {
    TestContent f;
    const auto c = f.content();
    const auto* immutable = playback::detail::GameplayAccess::content(c);
    for (bool hit : {false, true}) {
        playback::PlaybackSession s;
        auto p = s.prepareGameplayLoad(presentation(), playback::PlaybackMode::ChartClock, c);
        REQUIRE(p);
        REQUIRE(s.commit(std::move(*p)));
        std::vector<AdmittedObservation> observations;
        if (hit) {
            auto event = input();
            REQUIRE(s.submitGameplay(std::span{&event, 1}));
            observations.push_back({{ObservationTick{Tick{5}},
                                     "domain.binding.one",
                                     "keyboard",
                                     "lane.one",
                                     InputAction::press,
                                     {}},
                                    Tick{5},
                                    false,
                                    {},
                                    {}});
        }
        // Source scan does not call production advance, timers, Fold, replay or recovery.
        auto oracle = scanReference(immutable->capsule.gameplay, immutable->configuration,
                                    observations, Tick{20});
        REQUIRE(s.advanceGameplay({8}, {8}, {8, 0, 0}));
        REQUIRE(s.advanceGameplay({20}, {20}, {20, 12, 0}));
        auto result = s.queryGameplay();
        REQUIRE(result);
        const auto& actual = playback::detail::GameplayAccess::result(*result)->kernel;
        CHECK(actual.facts == oracle.facts);
        CHECK(actual.phases == oracle.phases);
        CHECK(actual.resources == oracle.resources);
        CHECK(actual.contacts == oracle.contacts);
        CHECK(actual.ownership == oracle.ownership);
        CHECK(actual.observers == oracle.observers);
        CHECK(actual.receipts == oracle.receipts);
        CHECK(actual.processedFrontier == oracle.processedFrontier);
        REQUIRE(actual.fold);
        const auto& fold = *actual.fold;
        CHECK(fold.score == (hit ? 2 : -1));
        CHECK(fold.combo == (hit ? 1 : 0));
        CHECK(fold.maxCombo == (hit ? 1 : 0));
        CHECK(fold.hits == (hit ? 1 : 0));
        CHECK(fold.misses == (hit ? 0 : 1));
        CHECK(fold.factCursor == 1);
        CHECK(fold.produced.empty());
        CHECK(fold.hookConsumerCursor == 0);
        CHECK(fold.bonus == 0);
        CHECK(fold.monoidValue == 0);
        CHECK(fold.ledgerDerivedCount == 1);
        CHECK(fold.strayCount == 0);
        CHECK(fold.consumeEmptyCount == 0);
        const std::vector<StatisticsCount> expected{
            {PhaseKind::tap, Outcome::hit, {}, hit ? 1U : 0U},
            {PhaseKind::tap, Outcome::miss, {}, hit ? 0U : 1U}};
        CHECK(fold.counts == expected);
        auto archive = s.archiveGameplay();
        REQUIRE(archive);
        auto replay = s.evaluateGameplayReplay(*archive);
        REQUIRE(replay);
        CHECK(replay->evidenceValid);
        CHECK(*result->sameResult(replay->result));
    }
}

TEST_CASE("Candidate typed Packed entry enters actual combined prepare without fallback JSON",
          "[candidate][gameplay][entry]") {
    TestContent f;
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto source = playback::PlaybackSource::fromGameplayPacked(
        "typed-gameplay", "compiled/main.packed", f.packed(), f.config, {}, *provider);
    INFO((source ? "" : std::string{source.error().code()}));
    REQUIRE(source);
    playback::PlaybackSession s;
    auto p = s.prepareLoad(std::move(*source), playback::PlaybackMode::ChartClock);
    REQUIRE(p);
    CHECK_FALSE(s.queryGameplay());
    REQUIRE(s.commit(std::move(*p)));
    REQUIRE(s.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK(s.queryGameplay()->score()->misses == 1);
    CHECK(s.contentInfo()->chartFormatVersion == 5);
    auto broken = f.packed();
    broken.pop_back();
    CHECK_FALSE(playback::PlaybackSource::fromGameplayPacked(
        "typed-gameplay", "compiled/main.packed", std::move(broken), f.config, {}, *provider));
    CHECK(s.queryGameplay()->score()->misses == 1);
    CHECK_FALSE(playback::PlaybackSource::fromGameplayPacked("typed-gameplay", "../escape.packed",
                                                             f.packed(), f.config, {}, *provider));
}

TEST_CASE("Candidate public W2 codecs and certified checkpoint retain owning full results",
          "[candidate][gameplay][codec]") {
    TestContent f;
    const auto content = f.content();
    playback::PlaybackSession s;
    f.load(s);
    auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    REQUIRE(s.advanceGameplay({20}, {20}, {20, 0, 0}));
    auto q = s.queryGameplay();
    REQUIRE(q);
    auto snapshot = s.snapshotGameplay();
    REQUIRE(snapshot);
    auto replay = s.archiveGameplay();
    REQUIRE(replay);
    auto snapshotBytes = snapshot->toBytes();
    REQUIRE(snapshotBytes);
    auto replayBytes = replay->toBytes();
    REQUIRE(replayBytes);
    const playback::GameplayCodecBudget budget{1024 * 1024, 100, 100000, true};
    auto decoded = playback::GameplaySnapshot::fromBytes(*snapshotBytes, content, budget);
    REQUIRE(decoded);
    auto archive = playback::GameplayReplay::fromBytes(*replayBytes, content, budget);
    REQUIRE(archive);
    CHECK(*q->sameResult(s.evaluateGameplayReplay(*archive)->result));
    auto checkpoint = s.checkpointGameplay(*archive, {20});
    REQUIRE(checkpoint);
    REQUIRE(checkpoint->valid());
    const auto* internal = playback::detail::GameplayAccess::replay(*archive);
    const auto cut = judgement::replayCutAt(internal->archive, Tick{20});
    REQUIRE(s.seekGameplay(*archive, {20}, {cut.completeRecords, {}}, std::span{&*checkpoint, 1},
                           {20}, {20, 0, 1}));
    CHECK(*q->sameResult(*s.queryGameplay()));
    REQUIRE(s.restoreGameplay(*decoded, {20}, {20, 0, 1}));
    CHECK(*q->sameResult(*s.queryGameplay()));
    auto altered = *snapshotBytes;
    altered[0] ^= std::byte{1};
    CHECK_FALSE(playback::GameplaySnapshot::fromBytes(altered, content, budget));
    CHECK_FALSE(playback::GameplayReplay::fromBytes(*replayBytes, content, {1, 1, 1, true}));
    CHECK_FALSE(playback::GameplayReplay::fromBytes(*replayBytes, content,
                                                    {1024 * 1024, 100, 100000, false}));
    REQUIRE(s.unload());
    auto moved = std::move(*archive);
    CHECK_FALSE(archive->valid());
    CHECK(moved.valid());
    CHECK(moved.toBytes());
    CHECK_FALSE(playback::GameplaySnapshot{}.toBytes());
    CHECK_FALSE(playback::GameplayResult{}.score());
    CHECK(*q->factCount() == 1);
}

TEST_CASE("Candidate reconstruction preserves Host frame lifetime and failed Runtime frame",
          "[candidate][gameplay][transaction][resolver]") {
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
                          {0},
                          {}}};
    playback::PlaybackSession s;
    f.load(s);
    REQUIRE(s.update({0, 0, 0}));
    const playback::HostOverrideWrite write{"019b0000-0000-7abc-8def-000000000002",
                                            playback::HostPropertyId::RenderVisible, true};
    auto token = s.acquireHostOverride(
        "host", INT64_MIN, playback::hostPropertyBit(playback::HostPropertyId::RenderVisible),
        {.kind = playback::HostOverrideLifetimeKind::RemainingFrames, .remainingFrames = 2},
        std::span{&write, 1});
    REQUIRE(token);
    auto event = input();
    REQUIRE(s.submitGameplay(std::span{&event, 1}));
    REQUIRE(s.advanceGameplay({8}, {8}, {8, 0, 0}));
    auto snap = s.snapshotGameplay();
    REQUIRE(snap);
    auto archive = s.archiveGameplay();
    REQUIRE(archive);
    const auto visible = [&] {
        auto frame = s.extractFrame({640, 480});
        REQUIRE(frame);
        return frame->objects[0].visible;
    };
    CHECK(visible());
    REQUIRE(s.restoreGameplay(*snap, {8}, {8, 0, 0}));
    CHECK(visible());
    REQUIRE(s.seekGameplay(*archive, {8}, {8}, {8, 0, 1}));
    CHECK(visible());
    REQUIRE(s.advanceGameplay({9}, {9}, {9, 1, 1}));
    CHECK(visible());
    REQUIRE(s.advanceGameplay({10}, {10}, {10, 1, 1}));
    CHECK_FALSE(visible());
    auto before = s.extractFrame({640, 480});
    REQUIRE(before);
    CHECK_FALSE(s.advanceGameplay({20}, {20}, {std::numeric_limits<double>::max(), 0, 1}));
    auto after = s.extractFrame({640, 480});
    REQUIRE(after);
    CHECK(playback::computeFrameDigest({10, 1, 1}, *before)->value ==
          playback::computeFrameDigest({10, 1, 1}, *after)->value);
    auto receipt = s.gameplayAdvanceReceipt();
    REQUIRE(receipt);
    CHECK(receipt->requestedHorizon.value == 20);
    CHECK(receipt->admitted);
    CHECK_FALSE(receipt->runtimeUpdated);
    REQUIRE(receipt->error);
    CHECK_FALSE(receipt->faultStage);
    CHECK(receipt->publicationFailureStage == playback::GameplayPublicationFailureStage::Runtime);
    CHECK(*s.queryGameplay()->state() == playback::GameplayState::Running);
}
