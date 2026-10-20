#include "../rpa/execution_fixture.hpp"
#include <cuexis/playback/frame_digest.hpp>

using namespace cuexis;
namespace {
constexpr auto childId = "019b0000-0000-7abc-8def-000000000002";
constexpr auto parentId = "019b0000-0000-7abc-8def-000000000003";
auto hierarchyPresentation() -> playback::PlaybackSource {
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
        {{"main", "mesh.bin", read("meshes/triangle.mesh.bin")},
         {"main", "material.bin", read("materials/opaque.material.bin")}});
    REQUIRE(provider);
    auto source = playback::PlaybackSource::fromTypedProject(
        {.sourceId = "pd3a.hierarchy",
         .chartJson = R"json({"format":"cuexis.chart","version":1,
"chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
"timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
"camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
"defaultTransform":{"position":[0,0,-10]}},"templates":[],
"behaviors":[{"id":"grow","type":"behavior.transform.keyframe","version":1,
"tracks":[{"property":"transform.scale","keys":[
{"beat":{"numerator":0,"denominator":1},"value":[1,1,1]},
{"beat":{"numerator":1,"denominator":1},"value":[1e30,1e30,1e30]}]}]}],
"objects":[
{"id":"019b0000-0000-7abc-8def-000000000003","parent":null,"components":{
"cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
"cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"grow"}}},"extensions":{}},
{"id":"019b0000-0000-7abc-8def-000000000002","parent":{"domain":"object","id":"019b0000-0000-7abc-8def-000000000003"},"components":{
"cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
"cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"grow"}},
"cuexis.renderable":{"version":1,"mesh":{"domain":"asset","id":"mesh"},"material":{"domain":"asset","id":"material"}}},"extensions":{}}],
"requiredExtensions":[],"extensions":{}})json",
         .assets = {{.id = "mesh",
                     .type = playback::PlaybackAssetType::Mesh,
                     .rootId = "main",
                     .logicalSource = "mesh.bin"},
                    {.id = "material",
                     .type = playback::PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "material.bin"}}},
        std::move(*provider));
    REQUIRE(source);
    return std::move(*source);
}
auto digest(playback::PlaybackSession& session, playback::RuntimeFrame frame) -> std::uint64_t {
    auto snapshot = session.extractFrame({640, 480});
    REQUIRE(snapshot);
    auto result = playback::computeFrameDigest(frame, *snapshot);
    REQUIRE(result);
    return result->value;
}
} // namespace

TEST_CASE("PD3a derived World failure retains sealed prefix and projection until same-prefix retry",
          "[pd][transaction][playback][retry]") {
    test_support::SessionTestContent content;
    const auto& identity = content.request.assembly.sources[0].document.requirements[0].identity;
    playback::GameplayRequirementRef ref{identity.chartEntryId,       identity.invocationId,
                                         identity.moduleId,           identity.exportId,
                                         identity.requirementLocalId, {}};
    for (const auto& step : identity.emissionPath)
        ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
    content.config.bindings = {{"binding.one",
                                ref,
                                playback::GameplayPhase::Tap,
                                playback::GameplayOutcome::Hit,
                                playback::GameplayTimingClass::Any,
                                childId,
                                false,
                                {0},
                                {},
                                playback::GameplayAggregation::Any,
                                {}}};
    playback::PlaybackSession retry, control;
    for (auto* session : {&retry, &control}) {
        auto prepared = session->prepareGameplayLoad(
            hierarchyPresentation(), playback::PlaybackMode::ChartClock, content.content());
        if (!prepared) {
            auto diagnostics = session->lastOperationDiagnostics();
            REQUIRE(diagnostics);
            for (const auto& item : diagnostics->items())
                UNSCOPED_INFO(item.code() << ": " << item.message() << " at " << item.fieldPath());
        }
        INFO((prepared ? "" : std::string{prepared.error().message()}));
        REQUIRE(prepared);
        REQUIRE(session->commit(std::move(*prepared)));
        REQUIRE(session->update({0, 0, 0}));
        const auto input = test_support::input();
        REQUIRE(session->submitGameplay(std::span{&input, 1}));
    }
    const auto before = digest(retry, {0, 0, 0});
    auto failed = retry.advanceGameplay({20}, {20}, {500, 500, 0});
    REQUIRE_FALSE(failed);
    auto receipt = retry.gameplayAdvanceReceipt();
    REQUIRE(receipt);
    CHECK(receipt->admitted);
    CHECK_FALSE(receipt->runtimeUpdated);
    CHECK(receipt->publicationFailureStage == playback::GameplayPublicationFailureStage::Runtime);
    CHECK(receipt->publishableFacts == 1);
    CHECK(retry.queryGameplayPresentation()->projectionScope == 0);
    CHECK(retry.queryGameplayPresentation()->sources.empty());
    CHECK(digest(retry, {0, 0, 0}) == before);
    const auto repair = [](playback::PlaybackSession& session) {
        const std::array writes{playback::HostOverrideWrite{
            parentId, playback::HostPropertyId::TransformScale, core::Vec3{1, 1, 1}}};
        REQUIRE(session.acquireHostOverride(
            "repair", 0, playback::hostPropertyBit(playback::HostPropertyId::TransformScale),
            {.kind = playback::HostOverrideLifetimeKind::RemainingFrames, .remainingFrames = 2},
            writes));
    };
    repair(retry);
    repair(control);
    // The admitted input and sealed Fact are reused without another submit or kernel advance.
    REQUIRE(retry.sampleGameplayPresentation({20}, {500, 500, 0}));
    REQUIRE(control.advanceGameplay({20}, {20}, {500, 500, 0}));
    CHECK(retry.queryGameplayPresentation()->sources.size() == 1);
    CHECK(digest(retry, {500, 500, 0}) == digest(control, {500, 500, 0}));
    CHECK(*retry.queryGameplay()->sameResult(*control.queryGameplay()));
    auto archive = retry.archiveGameplay();
    REQUIRE(archive);
    auto replay = retry.evaluateGameplayReplay(*archive);
    REQUIRE(replay);
    CHECK(*retry.queryGameplay()->sameResult(replay->result));
    REQUIRE(retry.sampleGameplayPresentation({21}, {500, 0, 0}));
    REQUIRE(control.sampleGameplayPresentation({21}, {500, 0, 0}));
    CHECK(digest(retry, {500, 0, 0}) == digest(control, {500, 0, 0}));
}
