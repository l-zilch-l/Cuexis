#include "../rpa/execution_fixture.hpp"

using namespace cuexis;
TEST_CASE("PD publication sampling cannot activate a faulted Fold prefix",
          "[pd][publication][fault]") {
    test_support::SessionTestContent f;
    f.config.maximumScore = 1;
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
    playback::PlaybackSession session;
    f.load(session);
    const auto event = test_support::input();
    REQUIRE(session.submitGameplay(std::span{&event, 1}));
    REQUIRE_FALSE(session.advanceGameplay({20}, {20}, {20, 0, 0}));
    auto receipt = session.gameplayAdvanceReceipt();
    REQUIRE(receipt);
    CHECK(receipt->sealedFacts > receipt->publishableFacts);
    auto before = session.queryGameplayPresentation();
    REQUIRE(before);
    auto sampled = session.sampleGameplayPresentation({20}, {20, 0, 0});
    CHECK_FALSE(sampled);
    auto after = session.queryGameplayPresentation();
    REQUIRE(after);
    CHECK(after->sources.empty());
    CHECK(after->projectionScope == before->projectionScope);
    auto result = session.queryGameplay();
    REQUIRE(result);
    auto archive = session.archiveGameplay();
    REQUIRE(archive);
    auto replay = session.evaluateGameplayReplay(*archive);
    REQUIRE(replay);
    REQUIRE(result->sameResult(replay->result));
    CHECK(*result->sameResult(replay->result));
}
