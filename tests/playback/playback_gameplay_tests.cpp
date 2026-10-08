#include "../judgement/execution_reference.hpp"
#include "../judgement/execution_test_fixture.hpp"
#include "gameplay_entry_internal.hpp"
#include "gameplay_internal.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
using namespace cuexis;
using namespace judgement;
using namespace judgement::testing;
namespace {
auto presentation() -> playback::PlaybackSource {
    const auto read = [](std::string_view path) {
        std::ifstream in{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                             "assets/projects/stage3_project/assets" / path,
                         std::ios::binary};
        REQUIRE(in);
        std::string text{std::istreambuf_iterator<char>{in}, {}};
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    };
    auto provider = content::MemoryContentProvider::create(
        {{"main", "triangle.mesh.bin", read("meshes/triangle.mesh.bin")},
         {"main", "opaque.material.bin", read("materials/opaque.material.bin")}});
    REQUIRE(provider);
    auto s = playback::PlaybackSource::fromTypedProject(
        {.sourceId = "gameplay-presentation",
         .chartJson = R"({"format":"cuexis.chart","version":1,
    "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
    "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
    "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
    "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
    "templates":[],"behaviors":[],"objects":[{"id":"019b0000-0000-7abc-8def-000000000002","parent":null,
    "components":{"cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
    "cuexis.renderable":{"version":1,"mesh":{"domain":"asset","id":"mesh"},"material":{"domain":"asset","id":"material"}}},"extensions":{}}],
    "requiredExtensions":[],"extensions":{}})",
         .assets = {{.id = "mesh",
                     .type = playback::PlaybackAssetType::Mesh,
                     .rootId = "main",
                     .logicalSource = "triangle.mesh.bin"},
                    {.id = "material",
                     .type = playback::PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "opaque.material.bin"}}},
        std::move(*provider));
    REQUIRE(s);
    return std::move(*s);
}
struct TestContent {
    Fixture base;
    ResolvedGameplayPrepareRequest request;
    chart::CanonicalSemanticChart chart;
    std::vector<gameplay_packed::RequirementOwner> owners;
    playback::GameplayConfiguration config;
    TestContent() : request{base.request(), {}} {
        base.latePolicy = executionLate();
        executionFields(request.assembly);
        auto& a = request.assembly;
        const std::string cap = "cuexis.gameplay.t4-k4.v1";
        a.capabilityContext = {{cap}, {cap}};
        a.declaredCapabilities = {{{cap, "1"}}};
        a.closureContributions.capabilities = {{cap, "1"}};
        auto& r = a.sources[0].document.requirements[0];
        r.required.capabilities = {{cap, "1"}};
        r.phases = {{PhaseKind::tap, 1}};
        r.requiresReleaseTailSemantics = false;
        r.measure.components = {{PhaseKind::tap, "tap", {}, {}}};
        r.timing = RequirementRecord::Timing{
            Tick{8}, {{Tick{0}, Tick{8}, {PhaseKind::tap, 1}}}, {}, {{PhaseKind::tap, Tick{5}}}};
        r.preparedGrace = PreparedGrace{TickSpan{0}};
        r.atomBindings.resize(1);
        a.sources[0].document.relations[0].policy.claimKeyToken.clear();
        chart.chartId.value = "019b0000-0000-7abc-8def-000000000001";
        chart.features = {{std::string{kFeatureId}, 1}, {std::string{kRulesetFeatureId}, 1}};
        chart::CanonicalEntity entity;
        entity.identity = chart::ExplicitEntityIdentity{
            chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000002"}};
        entity.components = {chart::CanonicalTransform{}};
        chart.entities.push_back(entity);
        owners.push_back({r.identity, entity.identity});
        const auto& e = a.identityDeclarations.engine;
        const auto& s = a.identityDeclarations.session;
        config.engine = {e.judgementSemanticRevision, e.factSemanticRevision,
                         e.fixedPointTableId,         e.coordinationPhaseOrderToken,
                         *e.executionProfileToken,    *e.lateAlgorithmToken};
        config.rulesetInterfaceProjection = a.identityDeclarations.ruleset.interfaceProjectionToken;
        config.rulesetBuild = a.identityDeclarations.ruleset.buildHash;
        config.rulesetModuleOrder = a.identityDeclarations.ruleset.moduleOrder;
        config.session = {s.loadoutToken, s.defaultGraceSourceToken, s.normalizationProfileToken,
                          s.judgementConfigToken};
        config.enabledCapabilities = {cap};
        config.mappingId = "mapping";
        config.mappingVersion = "v1";
        config.sourceClass = "keyboard";
        config.calibration = "calibration.fixture";
        config.domains = {{"domain.binding.one", {1, 1}, -100, 100, true}};
        config.actualRulesetInterface = "cuexis.ruleset.finite";
        config.actualRulesetRevision = "1";
        config.actualRulesetBuild = "cuexis.finite-fold.1";
        config.modules = {{"score", "1", "cuexis.finite-fold.1"},
                          {"combo", "1", "cuexis.finite-fold.1"},
                          {"statistics", "1", "cuexis.finite-fold.1"}};
        config.programPolicy = playback::GameplayProgramPolicy::Locked;
        config.outcomeScope = playback::GameplayOutcomeScope::Declared;
        config.arithmetic = playback::GameplayArithmetic::Checked;
        config.externalPackage = false;
        config.life = false;
        config.loadout = "loadout.fixture";
        config.initialScore = 0;
        config.minimumScore = INT64_MIN;
        config.maximumScore = INT64_MAX;
        config.initialCombo = 0;
        config.scoreRules = {
            {playback::GameplayPhase::Tap, playback::GameplayOutcome::Hit, {}, 2, true},
            {playback::GameplayPhase::Tap, playback::GameplayOutcome::Miss, {}, -1, false}};
        config.testOnly = true;
    }
    auto packed() -> std::vector<std::byte> {
        auto p = prepareResolvedGameplay(request);
        REQUIRE(p);
        auto bytes = gameplay_packed::encode(
            {chart, *p, owners, {"normalization.one", "coordinator.policy.greedy_v1"}, {}, {}, 3});
        INFO((bytes ? "" : std::string{bytes.error().message()}));
        REQUIRE(bytes);
        return std::move(*bytes);
    }
    auto content() -> playback::GameplayContent {
        auto bytes = packed();
        auto c = playback::GameplayContent::fromPacked(bytes, config);
        INFO((c ? "" : std::string{c.error().code()} + ": " + std::string{c.error().message()}));
        REQUIRE(c);
        return *c;
    }
    void load(playback::PlaybackSession& s) {
        auto p =
            s.prepareGameplayLoad(presentation(), playback::PlaybackMode::ChartClock, content());
        if (!p) {
            auto d = s.lastOperationDiagnostics();
            REQUIRE(d);
            for (const auto& item : d->items())
                INFO(item.message());
        }
        INFO((p ? "" : std::string{p.error().message()}));
        REQUIRE(p);
        REQUIRE(s.commit(std::move(*p)));
    }
};
auto input() -> playback::GameplayInput {
    return {{5},        1, playback::GameplayInputAction::Press, "lane.one", "domain.binding.one",
            "keyboard", {}};
}
} // namespace
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
    REQUIRE(registry->size() == 1);
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
TEST_CASE("Gameplay configuration codec preserves exact integers and rejects implicit shape",
          "[playback][gameplay][candidate]") {
    TestContent fixture;
    auto& c = fixture.config;
    c.initialScore = INT64_MIN;
    c.initialCombo = UINT64_MAX;
    c.domains[0].minimum = -9007199254740993LL;
    c.domains[0].maximum = 9007199254740993LL;
    c.hooks = {{"hook",
                playback::GameplayHookConsumer::Shared,
                playback::GameplayCombine::BitOr,
                {"score"},
                UINT64_MAX,
                true}};
    playback::GameplayFactBinding binding{};
    binding.bindingId = "binding";
    binding.source = {"entry",  "invocation",  "module",
                      "export", "requirement", {{"repeat", UINT64_MAX}}};
    binding.phase = playback::GameplayPhase::Tail;
    binding.outcome = playback::GameplayOutcome::Miss;
    binding.timing = playback::GameplayTimingClass::Late;
    binding.target = "target";
    binding.visible = false;
    binding.start = {INT64_MIN};
    binding.end = playback::GameplayPresentationTick{INT64_MAX};
    c.bindings = {binding};
    const playback::GameplayConfigurationDecodeBudget budget{65536, 32, 16384, 4096, 1024, true};
    auto encoded = playback::encodeGameplayConfiguration(c);
    REQUIRE(encoded);
    auto decoded = playback::decodeGameplayConfiguration(*encoded, budget);
    REQUIRE(decoded);
    CHECK(decoded->initialScore == INT64_MIN);
    CHECK(decoded->initialCombo == UINT64_MAX);
    CHECK(decoded->domains[0].minimum == -9007199254740993LL);
    CHECK(decoded->domains[0].maximum == 9007199254740993LL);
    CHECK(decoded->hooks[0].value == UINT64_MAX);
    CHECK(decoded->bindings[0].source.emissionPath[0].repeatIndex == UINT64_MAX);
    CHECK(decoded->bindings[0].start.value == INT64_MIN);
    REQUIRE(decoded->bindings[0].end);
    CHECK(decoded->bindings[0].end->value == INT64_MAX);
    auto reencoded = playback::encodeGameplayConfiguration(*decoded);
    REQUIRE(reencoded);
    CHECK(*reencoded == *encoded);
    auto value = json::parse(*encoded, {65536, 32, 16384});
    REQUIRE(value);
    const auto reject = [&](json::Value input) {
        auto text = json::serialize(input);
        REQUIRE(text);
        auto result = playback::decodeGameplayConfiguration(*text, budget);
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "playback.gameplay.invalid");
    };
    auto missing = *value;
    missing.find("configuration")->object()->erase("calibration");
    reject(std::move(missing));
    auto unknown = *value;
    unknown.find("configuration")->object()->emplace("hiddenDefault", json::Value{true});
    reject(std::move(unknown));
    auto floating = *value;
    *floating.find("configuration")->find("initialScore") = json::Value{1.0};
    reject(std::move(floating));
    auto overflowing = *value;
    *overflowing.find("configuration")->find("initialScore") = json::Value{UINT64_MAX};
    reject(std::move(overflowing));
    auto unknownEnum = *value;
    *unknownEnum.find("configuration")->find("arithmetic") = json::Value{"unchecked"};
    reject(std::move(unknownEnum));
    auto absentTestOnly = *value;
    *absentTestOnly.find("configuration")->find("testOnly") = json::Value{false};
    reject(std::move(absentTestOnly));
    auto shortIdentity = *value;
    shortIdentity.find("configuration")->find("engine")->array()->pop_back();
    reject(std::move(shortIdentity));
    auto duplicate = *encoded;
    duplicate.insert(1, "\"version\":1,");
    CHECK_FALSE(playback::decodeGameplayConfiguration(duplicate, budget));
    for (unsigned limit = 0; limit < 6; ++limit) {
        auto tiny = budget;
        if (limit == 0)
            tiny.maxBytes = 1;
        if (limit == 1)
            tiny.maxDepth = 1;
        if (limit == 2)
            tiny.maxStringBytes = 1;
        if (limit == 3)
            tiny.maxValues = 1;
        if (limit == 4)
            tiny.maxContainerElements = 1;
        if (limit == 5)
            tiny.testOnly = false;
        auto rejected = playback::decodeGameplayConfiguration(*encoded, tiny);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "capability.budget_insufficient");
        bool categoryFound = false;
        for (const auto& context : rejected.error().context())
            if (context.key == "category") {
                CHECK(context.value == "budget_exceeded");
                categoryFound = true;
            }
        CHECK(categoryFound);
    }
    c.programPolicy = static_cast<playback::GameplayProgramPolicy>(255);
    CHECK_FALSE(playback::encodeGameplayConfiguration(c));
    TestContent runtimeFixture;
    auto runtimeText = playback::encodeGameplayConfiguration(runtimeFixture.config);
    REQUIRE(runtimeText);
    auto runtimeConfig = playback::decodeGameplayConfiguration(*runtimeText, budget);
    REQUIRE(runtimeConfig);
    CHECK(playback::GameplayContent::fromPacked(runtimeFixture.packed(), *runtimeConfig));
}
TEST_CASE("Gameplay Entry metadata rejects incomplete closures before source publication",
          "[playback][gameplay][candidate]") {
    TestContent fixture;
    auto bytes = fixture.packed();
    auto content = fixture.content();
    const playback::GameplayConfigurationDecodeBudget budget{65536, 32, 16384, 4096, 1024, true};
    const playback::GameplayGraphDecodeBudget graphBudget{65536, 32, 16384, 4096, 1024, 512, 16384};
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto metadata = playback::detail::gameplayEntryMetadata(content, fixture.config, "main.packed",
                                                            "packed-chart", bytes, {});
    REQUIRE(metadata);
    const auto consume = [&](const json::Value& v, std::vector<std::byte> payload) {
        auto text = json::serialize(v);
        REQUIRE(text);
        return playback::PlaybackSource::fromGameplayEntry(
            "entry-fixture", *text, std::move(payload), fixture.config, budget, graphBudget, {},
            *provider, playback::GameplayPrepareIntent::GameplayOnly);
    };
    auto source = consume(*metadata, bytes);
    REQUIRE(source);
    playback::PlaybackSession session;
    auto prepared = session.prepareLoad(std::move(*source), playback::PlaybackMode::ChartClock);
    REQUIRE(prepared);
    REQUIRE(session.commit(std::move(*prepared)));
    auto receipt = session.advanceGameplay({20}, {20}, playback::RuntimeFrame{});
    REQUIRE(receipt);
    auto result = session.queryGameplay();
    REQUIRE(result);
    auto score = result->score();
    REQUIRE(score);
    CHECK(score->misses == 1);
    CHECK(score->score == -1);
    for (const auto* field :
         {"entryKind", "compiledSemanticIdentity", "artifactIdentity", "rulesetBinding",
          "capabilityClosure", "resourcePresentationClosure", "sourceOf"}) {
        auto missing = *metadata;
        missing.object()->erase(field);
        CHECK_FALSE(consume(missing, bytes));
    }
    auto tampered = bytes;
    tampered.back() ^= std::byte{1};
    auto badHash = consume(*metadata, std::move(tampered));
    REQUIRE_FALSE(badHash);
    CHECK(badHash.error().message() == "Gameplay artifact hash mismatch");
    const auto rejectField = [&](const char* field, json::Value replacement) {
        auto changed = *metadata;
        *changed.find(field) = std::move(replacement);
        CHECK_FALSE(consume(changed, bytes));
    };
    rejectField("compiledSemanticIdentity", json::Value{std::string(64, '0')});
    rejectField("expandedEntityCount", json::Value{std::uint64_t{2}});
    rejectField("expandedRequirementCount", json::Value{1.0});
    rejectField("compilerProfile", json::Value{"fake-profile"});
    rejectField("entryKind", json::Value{"author-source"});
    rejectField("encoding", json::Value{"capsule.2"});
    rejectField("version", json::Value{std::uint64_t{2}});
    rejectField("path", json::Value{"../outside"});
    auto fakeCapability = *metadata;
    fakeCapability.find("capabilityClosure")->find("derived")->array()->clear();
    CHECK_FALSE(consume(fakeCapability, bytes));
    auto fakeResource = *metadata;
    fakeResource.find("resourcePresentationClosure")
        ->find("gameplayResources")
        ->array()
        ->emplace_back("fake");
    CHECK_FALSE(consume(fakeResource, bytes));
    auto packagedSource = *metadata;
    *packagedSource.find("sourceOf")->find("kind") = json::Value{"packaged"};
    CHECK_FALSE(consume(packagedSource, bytes));
    auto unknown = *metadata;
    unknown.object()->emplace("hiddenProof", json::Value{true});
    CHECK_FALSE(consume(unknown, bytes));
}
TEST_CASE(
    "Gameplay Entry CXC file memory and captured filesystem generation share complete results",
    "[candidate][gameplay][entry][cxc]") {
    TestContent fixture;
    auto content = fixture.content();
    auto packed = fixture.packed();
    auto prepared = prepareResolvedGameplay(fixture.request);
    REQUIRE(prepared);
    auto graph =
        gameplay_packed::encodeGraph({fixture.chart,
                                      *prepared,
                                      fixture.owners,
                                      {"normalization.one", "coordinator.policy.greedy_v1"},
                                      {},
                                      {},
                                      3},
                                     {131072, 8192, 8192, true});
    REQUIRE(graph);
    const playback::GameplayConfigurationDecodeBudget metadataBudget{131072, 64,   8192,
                                                                     16384,  8192, true};
    const playback::GameplayGraphDecodeBudget graphBudget{131072, 64,   8192, 16384,
                                                          8192,   8192, 65536};
    const auto bytesOf = [](std::string_view text) {
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    };
    const auto read = [&](std::string_view relative) {
        std::ifstream stream{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                 "tests/fixtures/chart_format_update/static_project" / relative,
                             std::ios::binary};
        REQUIRE(stream);
        return std::string{std::istreambuf_iterator<char>{stream}, {}};
    };
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    for (const auto kind : {"packed-chart", "gameplay-graph"}) {
        auto bytes = std::string_view{kind} == "packed-chart" ? packed : bytesOf(*graph);
        const auto entryPath = std::string_view{kind} == "packed-chart"
                                   ? "compiled/main.packed"
                                   : "compiled/main.graph.json";
        auto metadata = playback::detail::gameplayEntryMetadata(content, fixture.config, entryPath,
                                                                kind, bytes, {});
        REQUIRE(metadata);
        auto project = json::parse(read("cuexis.project.json"), {65536, 32, 8192});
        REQUIRE(project);
        project->find("extensions")
            ->object()
            ->emplace("cuexis.gameplay-entry.v1",
                      json::Value{json::Value::Object{
                          {"entries", json::Value{json::Value::Array{*metadata}}}}});
        auto projectText = json::serialize(*project);
        REQUIRE(projectText);
        cxc::CxcWriteRequest request;
        request.entries = {
            {"cuexis.project.json", bytesOf(*projectText)},
            {"assets/cuexis.asset-index.json", bytesOf(read("assets/cuexis.asset-index.json"))},
            {"assets/charts/main.cuexis.chart.json",
             bytesOf(read("assets/charts/main.cuexis.chart.json"))},
            {entryPath, bytes}};
        auto package = cxc::CxcWriter::write(request);
        std::string diagnostics;
        for (const auto& diagnostic : package.diagnostics.items())
            diagnostics +=
                std::string{diagnostic.code()} + ": " + std::string{diagnostic.message()} + "\n";
        INFO(diagnostics);
        REQUIRE(package.hasValue());
        struct Directory {
            std::filesystem::path path;
            Directory() {
                const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
                for (unsigned i = 0; i < 4096; ++i) {
                    path = std::filesystem::temp_directory_path() /
                           ("cuexis-gameplay-entry-" + std::to_string(nonce) + "-" +
                            std::to_string(i));
                    if (std::filesystem::create_directory(path))
                        return;
                }
                throw std::runtime_error{"Cannot claim fixture directory"};
            }
            ~Directory() {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        } directory;
        const auto write = [](const std::filesystem::path& path,
                              std::span<const std::byte> payload) {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream{path, std::ios::binary};
            REQUIRE(stream);
            stream.write(reinterpret_cast<const char*>(payload.data()),
                         static_cast<std::streamsize>(payload.size()));
            REQUIRE(stream.good());
        };
        write(directory.path / "package.cxc", *package.bytes);
        for (const auto& entry : request.entries)
            write(directory.path / "generation" / entry.path, entry.bytes);
        std::optional<playback::GameplayResult> expected;
        for (unsigned carrier = 0; carrier < 3; ++carrier) {
            auto source =
                carrier == 0 ? playback::PlaybackSource::fromCxcMemoryGameplayEntry(
                                   *package.bytes, entryPath, fixture.config, metadataBudget,
                                   graphBudget, playback::GameplayPrepareIntent::GameplayOnly)
                : carrier == 1
                    ? playback::PlaybackSource::fromCxcFileGameplayEntry(
                          directory.path / "package.cxc", entryPath, fixture.config, metadataBudget,
                          graphBudget, playback::GameplayPrepareIntent::GameplayOnly)
                    : playback::PlaybackSource::fromFilesystemGameplayEntry(
                          directory.path / "generation", entryPath, fixture.config, metadataBudget,
                          graphBudget, playback::GameplayPrepareIntent::GameplayOnly);
            INFO((source ? "" : std::string{source.error().message()}));
            REQUIRE(source);
            playback::PlaybackSession session;
            auto candidate =
                session.prepareLoad(std::move(*source), playback::PlaybackMode::ChartClock);
            REQUIRE(candidate);
            REQUIRE(session.commit(std::move(*candidate)));
            auto event = input();
            REQUIRE(session.submitGameplay(std::span{&event, 1}));
            REQUIRE(session.advanceGameplay({20}, {20}, {20, 0, 0}));
            auto result = session.queryGameplay();
            REQUIRE(result);
            CHECK(result->score()->score == 2);
            CHECK(result->score()->hits == 1);
            if (expected)
                CHECK(*expected->sameResult(*result));
            else
                expected = *result;
            auto archive = session.archiveGameplay();
            REQUIRE(archive);
            auto replay = session.evaluateGameplayReplay(*archive);
            REQUIRE(replay);
            CHECK(replay->evidenceValid);
            CHECK(*result->sameResult(replay->result));
        }
        auto tinyBudget = metadataBudget;
        tinyBudget.maxBytes = 1;
        auto budgetRejected = playback::PlaybackSource::fromCxcMemoryGameplayEntry(
            *package.bytes, entryPath, fixture.config, tinyBudget, graphBudget,
            playback::GameplayPrepareIntent::GameplayOnly);
        REQUIRE_FALSE(budgetRejected);
        CHECK(budgetRejected.error().code() == "capability.budget_insufficient");
        bool sourceFound = false;
        for (const auto& context : budgetRejected.error().context())
            if (context.key == "sourceCode") {
                CHECK(context.value == "json.parse.size_limit");
                sourceFound = true;
            }
        CHECK(sourceFound);
        CHECK_FALSE(playback::PlaybackSource::fromCxcMemoryGameplayEntry(
            *package.bytes, "wrong.packed", fixture.config, metadataBudget, graphBudget,
            playback::GameplayPrepareIntent::GameplayOnly));
        auto corrupt = *package.bytes;
        corrupt[corrupt.size() / 2] ^= std::byte{1};
        CHECK_FALSE(playback::PlaybackSource::fromCxcMemoryGameplayEntry(
            std::move(corrupt), entryPath, fixture.config, metadataBudget, graphBudget,
            playback::GameplayPrepareIntent::GameplayOnly));
    }
}

TEST_CASE("Candidate FactBinding groups preserve partial commits without changing judgement",
          "[candidate][gameplay][presentation][group]") {
    TestContent f;
    auto& doc = f.request.assembly.sources[0].document;
    doc.factBindings.push_back({"binding.two"});
    doc.requirements[0].factBindingRefs.push_back("binding.two");
    const auto& e = doc.requirements[0].identity;
    playback::GameplayRequirementRef ref{e.chartEntryId, e.invocationId,       e.moduleId,
                                         e.exportId,     e.requirementLocalId, {}};
    for (const auto& step : e.emissionPath)
        ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
    playback::GameplayFactBinding one{"binding.one",
                                      ref,
                                      playback::GameplayPhase::Tap,
                                      playback::GameplayOutcome::Hit,
                                      playback::GameplayTimingClass::Any,
                                      "019b0000-0000-7abc-8def-000000000002",
                                      false,
                                      {10},
                                      playback::GameplayPresentationTick{30}};
    one.aggregation = playback::GameplayAggregation::GroupCommit;
    one.groupMembers = {"binding.two", "binding.one"};
    auto two = one;
    two.bindingId = "binding.two";
    two.target = "absent-presentation-target";
    f.config.bindings = {one, two};
    TestContent plain = f;
    plain.config.bindings.clear();
    playback::PlaybackSession grouped, control;
    f.load(grouped);
    plain.load(control);
    const auto event = input();
    for (auto* session : {&grouped, &control}) {
        REQUIRE(session->submitGameplay(std::span{&event, 1}));
        REQUIRE(session->advanceGameplay({20}, {10}, {10, 0, 0}));
    }
    CHECK(*grouped.queryGameplay()->sameResult(*control.queryGameplay()));
    CHECK_FALSE(grouped.extractFrame({640, 480})->objects[0].visible);
    auto sourceMap = grouped.queryGameplayPresentation();
    REQUIRE(sourceMap);
    REQUIRE(sourceMap->sources.size() == 1);
    CHECK_FALSE(sourceMap->sources[0].visible);
    CHECK(sourceMap->projectionScope != 0);
    const auto originalScope = sourceMap->projectionScope;
    auto diagnostics = grouped.lastOperationDiagnostics();
    REQUIRE(diagnostics);
    CHECK(std::ranges::any_of(diagnostics->items(),
                              [](const auto& d) { return d.code() == "partial-group"; }));
    CHECK(std::ranges::any_of(diagnostics->items(), [](const auto& d) {
        return d.code() == "presentation-target-missing";
    }));
    REQUIRE(grouped.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK_FALSE(grouped.extractFrame({640, 480})->objects[0].visible);
    CHECK(*grouped.gameplayState() == playback::GameplayState::Running);
    REQUIRE(grouped.advanceGameplay({20}, {30}, {30, 0, 0}));
    CHECK(grouped.extractFrame({640, 480})->objects[0].visible);
    CHECK(grouped.queryGameplayPresentation()->sources.empty());
    auto archive = grouped.archiveGameplay();
    REQUIRE(archive);
    REQUIRE(grouped.seekGameplay(*archive, {20}, *archive->cutAt({20}), {}, {10}, {10, 0, 1}));
    CHECK_FALSE(grouped.extractFrame({640, 480})->objects[0].visible);
    CHECK(grouped.queryGameplayPresentation()->projectionScope != originalScope);
    CHECK(sourceMap->sources.size() == 1); // Owning old map remains valid.
    auto laterOne = one;
    laterOne.start = {20};
    laterOne.end = playback::GameplayPresentationTick{30};
    auto earlyMissing = two;
    earlyMissing.start = {0};
    earlyMissing.end = playback::GameplayPresentationTick{10};
    f.config.bindings = {laterOne, earlyMissing};
    playback::PlaybackSession later;
    f.load(later);
    REQUIRE(later.submitGameplay(std::span{&event, 1}));
    REQUIRE(later.advanceGameplay({20}, {0}, {0, 0, 0}));
    CHECK(later.extractFrame({640, 480})->objects[0].visible);
    REQUIRE(later.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK_FALSE(later.extractFrame({640, 480})->objects[0].visible);
    f.config.bindings = {one, two};
    f.config.bindings[0].groupMembers.push_back("missing.binding");
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    f.config.bindings = {one, two};
    for (auto& b : f.config.bindings)
        b.aggregation = playback::GameplayAggregation::All;
    f.config.bindings[1].outcome = playback::GameplayOutcome::Miss;
    auto alias = f.config.bindings[1];
    alias.aggregation = playback::GameplayAggregation::Any;
    alias.groupMembers.clear();
    alias.outcome = playback::GameplayOutcome::Hit;
    f.config.bindings.push_back(alias);
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    std::reverse(f.config.bindings.begin(), f.config.bindings.end());
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    std::reverse(f.config.bindings.begin(), f.config.bindings.end());
    f.config.bindings.pop_back();

    playback::PlaybackSession all;
    f.load(all);
    REQUIRE(all.submitGameplay(std::span{&event, 1}));
    REQUIRE(all.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK(all.extractFrame({640, 480})->objects[0].visible);
    CHECK(*all.queryGameplay()->sameResult(*control.queryGameplay()));
}

TEST_CASE("Candidate capability four-state query is readonly and requires matching actual commit",
          "[candidate][gameplay][registry][query]") {
    TestContent f;
    playback::PlaybackSession session;
    const auto cap = "cuexis.gameplay.t4-k4.v1";
    auto unknown = session.queryGameplayCapability("unknown", "1", &f.config);
    REQUIRE(unknown);
    CHECK(unknown->state == playback::GameplayCapabilityState::Unknown);
    CHECK(unknown->diagnostic->code() == "capability.unknown");
    auto revision = session.queryGameplayCapability(cap, "2", &f.config);
    REQUIRE(revision);
    CHECK(revision->diagnostic->code() == "capability.revision_mismatch");
    CHECK(session.queryGameplayCapability(cap, "1")->state ==
          playback::GameplayCapabilityState::Disabled);
    auto pending = session.queryGameplayCapability(cap, "1", &f.config);
    REQUIRE(pending);
    CHECK(pending->state == playback::GameplayCapabilityState::Insufficient);
    CHECK(pending->diagnostic->code() == "capability.budget_insufficient");
    auto prepared = session.prepareGameplayLoad(presentation(), playback::PlaybackMode::ChartClock,
                                                f.content());
    REQUIRE(prepared);
    CHECK(session.queryGameplayCapability(cap, "1", &f.config)->state ==
          playback::GameplayCapabilityState::Insufficient);
    REQUIRE(session.commit(std::move(*prepared)));
    auto ready = session.queryGameplayCapability(cap, "1", &f.config);
    REQUIRE(ready);
    CHECK(ready->state == playback::GameplayCapabilityState::Available);
    CHECK_FALSE(ready->diagnostic);
    auto changed = f.config;
    changed.calibration = "another.calibration";
    CHECK(session.queryGameplayCapability(cap, "1", &changed)->state ==
          playback::GameplayCapabilityState::Insufficient);
    changed.enabledCapabilities.clear();
    CHECK(session.queryGameplayCapability(cap, "1", &changed)->state ==
          playback::GameplayCapabilityState::Disabled);
    auto reload = session.prepareGameplayReload(presentation(), f.content(),
                                                playback::ReloadPolicy::KeepChartTime);
    REQUIRE(reload);
    CHECK(session.queryGameplayCapability(cap, "1")->state ==
          playback::GameplayCapabilityState::Available);
    REQUIRE(session.commit(std::move(*reload))); // Readonly query did not invalidate generation.
    REQUIRE(session.controlGameplay(playback::GameplayControl::Pause));
    CHECK(session.queryGameplayCapability(cap, "1")->state ==
          playback::GameplayCapabilityState::Available);
    CHECK(*session.gameplayState() == playback::GameplayState::Paused);
}

TEST_CASE(
    "Candidate complete Hold Graph and Packed sources match independent scan and full recovery",
    "[candidate][gameplay][hold][oracle][entry]") {
    TestContent f;
    executionFields(f.request.assembly);
    auto& r = f.request.assembly.sources[0].document.requirements[0];
    r.measure.components = {{PhaseKind::head, "hold_head", {}, {}},
                            {PhaseKind::body, "hold_body", {}, {}},
                            {PhaseKind::tail, "hold_tail", {}, {}}};
    f.config.scoreRules.clear();
    for (auto [phase, weight] : std::vector<std::pair<playback::GameplayPhase, std::int64_t>>{
             {playback::GameplayPhase::Head, 2},
             {playback::GameplayPhase::Body, 3},
             {playback::GameplayPhase::Tail, 5}}) {
        f.config.scoreRules.push_back({phase, playback::GameplayOutcome::Hit, {}, weight, true});
        f.config.scoreRules.push_back({phase, playback::GameplayOutcome::Miss, {}, -weight, false});
    }
    const auto content = f.content();
    const auto* immutable = playback::detail::GameplayAccess::content(content);
    auto graph = gameplay_packed::encodeGraph(
        {immutable->capsule.chart, immutable->capsule.gameplay, immutable->capsule.owners,
         immutable->capsule.profiles, immutable->capsule.patterns, immutable->capsule.measures, 3},
        {131072, 8192, 8192, true});
    REQUIRE(graph);
    auto provider = cuexis::content::MemoryContentProvider::create({});
    REQUIRE(provider);
    playback::PlaybackSession packedSession, graphSession;
    auto packedSource = playback::PlaybackSource::fromGameplayPacked(
        "hold", "compiled/main.packed", f.packed(), f.config, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(packedSource);
    std::vector<std::byte> graphBytes(graph->size());
    std::memcpy(graphBytes.data(), graph->data(), graph->size());
    auto graphSource = playback::PlaybackSource::fromGameplayGraph(
        "hold", "compiled/main.graph", std::move(graphBytes), f.config,
        {131072, 64, 8192, 16384, 8192, 8192, 65536}, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(graphSource);
    for (auto pair :
         {std::pair{&packedSession, &*packedSource}, std::pair{&graphSession, &*graphSource}}) {
        auto prepared =
            pair.first->prepareLoad(std::move(*pair.second), playback::PlaybackMode::ChartClock);
        REQUIRE(prepared);
        REQUIRE(pair.first->commit(std::move(*prepared)));
    }
    auto press = input();
    press.observationTick = {900};
    auto release = press;
    release.observationTick = {1000};
    release.sequence = 2;
    release.action = playback::GameplayInputAction::Release;
    const std::array events{press, release};
    std::vector<AdmittedObservation> observations;
    for (const auto& event : events)
        observations.push_back(
            {{ObservationTick{Tick{event.observationTick.value}},
              "domain.binding.one",
              "keyboard",
              "lane.one",
              event.action == playback::GameplayInputAction::Press ? InputAction::press
                                                                   : InputAction::release,
              {}},
             Tick{event.observationTick.value},
             false,
             {},
             {}});
    auto oracle = scanReference(immutable->capsule.gameplay, immutable->configuration, observations,
                                Tick{1100});
    for (auto* session : {&packedSession, &graphSession}) {
        REQUIRE(session->submitGameplay(events));
        auto pending = session->snapshotGameplay();
        REQUIRE(pending);
        REQUIRE(session->advanceGameplay({900}, {7}, {0, 0, 0}));
        CHECK(*session->queryGameplay()->factCount() == 0);
        REQUIRE(session->advanceGameplay({1100}, {14}, {0, 0, 0}));
        auto result = session->queryGameplay();
        REQUIRE(result);
        const auto score = *result->score();
        CHECK(score.score == 10);
        CHECK(score.combo == 3);
        CHECK(score.hits == 3);
        CHECK(score.misses == 0);
        const auto& actual = playback::detail::GameplayAccess::result(*result)->kernel;
        CHECK(actual.facts == oracle.facts);
        CHECK(actual.phases == oracle.phases);
        CHECK(actual.resources == oracle.resources);
        CHECK(actual.contacts == oracle.contacts);
        CHECK(actual.ownership == oracle.ownership);
        CHECK(actual.observers == oracle.observers);
        CHECK(actual.receipts == oracle.receipts);
        auto archive = session->archiveGameplay();
        REQUIRE(archive);
        auto replay = session->evaluateGameplayReplay(*archive);
        REQUIRE(replay);
        CHECK(replay->evidenceValid);
        CHECK(*result->sameResult(replay->result));
        auto checkpoint = session->checkpointGameplay(*archive, {1100});
        REQUIRE(checkpoint);
        REQUIRE(session->seekGameplay(*archive, {1100}, *archive->cutAt({1100}),
                                      std::span{&*checkpoint, 1}, {14}, {0, 0, 1}));
        CHECK(*result->sameResult(*session->queryGameplay()));
        REQUIRE(session->restoreGameplay(*pending, {0}, {0, 0, 2}));
        REQUIRE(session->advanceGameplay({1100}, {14}, {0, 0, 2}));
        CHECK(*result->sameResult(*session->queryGameplay()));
    }
    CHECK(*packedSession.queryGameplay()->sameResult(*graphSession.queryGameplay()));
}

TEST_CASE("Candidate unknown capability first diagnostic is independent of declaration order",
          "[candidate][gameplay][diagnostic]") {
    TestContent f;
    const auto packed = f.packed();
    for (const auto& capabilities : std::vector<std::vector<std::string>>{
             {"unknown.z", "unknown.a"}, {"unknown.a", "unknown.z"}}) {
        f.config.enabledCapabilities = capabilities;
        const auto content = playback::GameplayContent::fromPacked(packed, f.config);
        REQUIRE_FALSE(content);
        CHECK(content.error().code() == "capability.unknown");
        const auto& context = content.error().context();
        const auto found = std::find_if(context.begin(), context.end(),
                                        [](const auto& row) { return row.key == "capabilityId"; });
        REQUIRE(found != context.end());
        CHECK(found->value == "unknown.a");
    }
}
