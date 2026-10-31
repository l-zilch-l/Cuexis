#include "../rpa/execution_fixture.hpp"
#include <algorithm>
#include <cuexis/playback/frame_digest.hpp>
#include <limits>

using namespace cuexis;
namespace {
void bindings(test_support::SessionTestContent& f) {
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
                                      {0},
                                      playback::GameplayPresentationTick{10},
                                      playback::GameplayAggregation::GroupCommit,
                                      {"binding.one", "binding.two"}};
    auto missing = one;
    missing.bindingId = "binding.two";
    missing.target = "absent";
    auto future = one;
    future.start = {20};
    future.end.reset();
    f.config.bindings = {future, one, missing, one};
}
auto digest(playback::PlaybackSession& s, const playback::RuntimeFrame& frame) -> std::uint64_t {
    auto snapshot = s.extractFrame({640, 480});
    REQUIRE(snapshot);
    auto value = playback::computeFrameDigest(frame, *snapshot);
    REQUIRE(value);
    return value->value;
}
} // namespace

TEST_CASE("PD projection Runtime failure retains candidate state for presentation retry",
          "[pd][projection][transaction][retry]") {
    test_support::SessionTestContent f;
    bindings(f);
    playback::PlaybackSession retry, control;
    f.load(retry);
    f.load(control);
    auto initial = retry.snapshotGameplay();
    REQUIRE(initial);
    REQUIRE(retry.update({0, 0, 0}));
    const auto before = digest(retry, {0, 0, 0});
    const auto event = test_support::input();
    for (auto* s : {&retry, &control})
        REQUIRE(s->submitGameplay(std::span{&event, 1}));
    // Existing Runtime representability rejection, after seal/Fold and projection preparation.
    REQUIRE_FALSE(retry.advanceGameplay({20}, {0}, {std::numeric_limits<double>::max(), 0, 0}));
    auto receipt = retry.gameplayAdvanceReceipt();
    REQUIRE(receipt);
    CHECK(receipt->admitted);
    CHECK_FALSE(receipt->runtimeUpdated);
    CHECK(receipt->publicationFailureStage == playback::GameplayPublicationFailureStage::Runtime);
    CHECK(receipt->publishableFacts == 1);
    auto failed = retry.queryGameplayPresentation();
    REQUIRE(failed);
    CHECK(failed->sources.empty());
    CHECK(failed->projectionScope == 0);
    CHECK(digest(retry, {0, 0, 0}) == before);
    CHECK(retry.lastOperationDiagnostics()->items().empty());
    // No second submit or kernel advance: use the same already successful Fact prefix.
    REQUIRE(retry.sampleGameplayPresentation({0}, {0, 0, 0}));
    REQUIRE(control.advanceGameplay({20}, {0}, {0, 0, 0}));
    REQUIRE(retry.queryGameplayPresentation()->sources.size() == 1);
    CHECK(digest(retry, {0, 0, 0}) == digest(control, {0, 0, 0}));
    auto diagnostics = retry.lastOperationDiagnostics();
    REQUIRE(diagnostics);
    CHECK(std::ranges::count_if(diagnostics->items(),
                                [](const auto& d) { return d.code() == "partial-group"; }) == 1);
    auto scope = retry.queryGameplayPresentation()->projectionScope;
    REQUIRE(scope != 0);
    // Empty publication interval still expires the first window and rejects the future alias.
    REQUIRE(retry.sampleGameplayPresentation({20}, {20, 20, 0}));
    CHECK(retry.queryGameplayPresentation()->sources.empty());
    CHECK(retry.extractFrame({640, 480})->objects[0].visible);
    auto archive = retry.archiveGameplay();
    REQUIRE(archive);
    auto evaluated = retry.evaluateGameplayReplay(*archive);
    REQUIRE(evaluated);
    CHECK(*retry.queryGameplay()->sameResult(evaluated->result));
    REQUIRE(retry.restoreGameplay(*initial, {0}, {0, 0, 1}));
    REQUIRE(retry.submitGameplay(std::span{&event, 1}));
    REQUIRE(retry.advanceGameplay({20}, {20}, {20, 20, 1}));
    CHECK(retry.queryGameplayPresentation()->projectionScope != scope);
    REQUIRE(retry.queryGameplayPresentation()->sources.size() == 1);
    CHECK_FALSE(retry.extractFrame({640, 480})->objects[0].visible);
    CHECK(*retry.queryGameplay()->sameResult(evaluated->result));
}

TEST_CASE("PD indexed aliases preserve the declared representative source lifetime",
          "[pd][projection][aliases]") {
    test_support::SessionTestContent f;
    bindings(f);
    auto first = f.config.bindings[1];
    first.aggregation = playback::GameplayAggregation::Any;
    first.groupMembers.clear();
    first.end = playback::GameplayPresentationTick{20};
    auto second = first;
    second.end = playback::GameplayPresentationTick{10};
    f.config.bindings = {first, second};
    for (int order = 0; order < 2; ++order) {
        playback::PlaybackSession s;
        f.load(s);
        const auto event = test_support::input();
        REQUIRE(s.submitGameplay(std::span{&event, 1}));
        REQUIRE(s.advanceGameplay({20}, {5}, {5, 0, 0}));
        auto sources = s.queryGameplayPresentation();
        REQUIRE(sources);
        REQUIRE(sources->sources.size() == 1);
        REQUIRE(sources->sources.front().end);
        CHECK(sources->sources.front().end->value == (order == 0 ? 20 : 10));
        REQUIRE(s.sampleGameplayPresentation({10}, {10, 5, 0}));
        sources = s.queryGameplayPresentation();
        REQUIRE(sources);
        REQUIRE(sources->sources.size() == 1);
        CHECK(sources->sources.front().end->value == 20);
        std::reverse(f.config.bindings.begin(), f.config.bindings.end());
    }
}

TEST_CASE("PD prepare keeps authoritative conflict and alias rejection in both declaration orders",
          "[pd][projection][prepare]") {
    test_support::SessionTestContent f;
    bindings(f);
    auto conflict = f.config.bindings[1];
    conflict.visible = true;
    f.config.bindings.push_back(conflict);
    for (int order = 0; order < 2; ++order) {
        auto content = playback::GameplayContent::fromPacked(f.packed(), f.config);
        REQUIRE_FALSE(content);
        CHECK(content.error().code() == "playback.gameplay.invalid");
        std::reverse(f.config.bindings.begin(), f.config.bindings.end());
    }
    f.config.bindings.pop_back();
    auto alias = f.config.bindings.back();
    alias.groupMembers.clear();
    alias.aggregation = playback::GameplayAggregation::Any;
    f.config.bindings.push_back(alias);
    for (int order = 0; order < 2; ++order) {
        CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
        std::reverse(f.config.bindings.begin(), f.config.bindings.end());
    }
}
