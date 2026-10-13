#pragma once
#include "../judgement/execution_test_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <filesystem>
#include <fstream>
namespace cuexis::test_support {
using namespace judgement;
using namespace judgement::testing;
using namespace playback;
inline auto presentation() -> playback::PlaybackSource {
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
struct SessionTestContent {
    Fixture base;
    ResolvedGameplayPrepareRequest request;
    chart::CanonicalSemanticChart chart;
    std::vector<gameplay_packed::RequirementOwner> owners;
    playback::GameplayConfiguration config;
    SessionTestContent() : request{base.request(), {}} {
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
        INFO((p ? "" : std::string{p.error().code()} + ": " + std::string{p.error().message()}));
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
inline auto input() -> playback::GameplayInput {
    return {{5},        1, playback::GameplayInputAction::Press, "lane.one", "domain.binding.one",
            "keyboard", {}};
}
inline void rpa(SessionTestContent& f, ClockMappingConfiguration mapping) {
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
    auto calibration = RealtimeClock::calibrationIdentity(mapping);
    REQUIRE(calibration);
    f.config.calibration = *calibration;
}
} // namespace cuexis::test_support
