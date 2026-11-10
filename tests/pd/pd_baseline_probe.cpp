#include "../rpa/execution_fixture.hpp"
#include <cstdint>
#include <cuexis/playback/frame_digest.hpp>
#include <iostream>

using namespace cuexis;

TEST_CASE("PD deterministic presentation baseline", "[.][pd][baseline]") {
    // Explicitly selected probe: fixed input/time/viewport, no device or wall clock input.
    cuexis::test_support::SessionTestContent f;
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
                          playback::GameplayPresentationTick{1000}}};
    playback::PlaybackSession session;
    f.load(session);
    const auto event = cuexis::test_support::input();
    REQUIRE(session.submitGameplay(std::span{&event, 1}));
    std::uint64_t digest = 14695981039346656037ULL;
    playback::FrameSnapshot snapshot;
    for (std::size_t i = 0; i < 1088; ++i) {
        const auto time = static_cast<std::int64_t>(i + 1) * 10;
        const playback::RuntimeFrame frame{static_cast<double>(time), 10, 0};
        REQUIRE(session.advanceGameplay({time}, {time}, frame));
        REQUIRE(session.extractFrame({1280, 720}, snapshot));
        REQUIRE(snapshot.objects.size() == 1);
        CHECK(snapshot.objects[0].visible == (time >= 1000));
        const auto result = playback::computeFrameDigest(frame, snapshot);
        REQUIRE(result);
        if (i >= 64)
            digest = (digest ^ result->value) * 1099511628211ULL;
    }
    const auto query = session.queryGameplay();
    REQUIRE(query);
    REQUIRE(query->score());
    CHECK(query->score()->hits == 1);
    CHECK(query->score()->score == 2);
    auto archive = session.archiveGameplay();
    REQUIRE(archive);
    auto replay = session.evaluateGameplayReplay(*archive);
    REQUIRE(replay);
    CHECK(replay->evidenceValid);
    REQUIRE(query->sameResult(replay->result));
    CHECK(*query->sameResult(replay->result));
    std::cout << "PD_BASELINE digest=" << digest << " samples=1024 warmup=64\n";
}
