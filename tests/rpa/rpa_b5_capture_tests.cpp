#include "../../engine/playback/src/capture_route.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/realtime_clock.hpp>

TEST_CASE("B5 tail replica adopts actual source actions rather than reply arrival times",
          "[b5][clock]") {
    using namespace cuexis::playback;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    auto main = RealtimeClock::create({1000, 0, 64, "", true}, mapping, "mirror", 1);
    auto worker = RealtimeClock::create({1000, 0, 64, "", true}, mapping, "mirror", 1);
    REQUIRE(main);
    REQUIRE(worker);
    ClockObservation eof{};
    eof.monotonicDomain = "mirror";
    eof.segment = 1;
    eof.publicationSequence = 1;
    eof.validUntilMonotonicUs = 1000;
    eof.source = ClockFramePosition{20, 1000000};
    eof.presented = eof.source;
    eof.playback = ClockPlaybackState::EndOfStream;
    eof.quality = ClockQuality::Exact;
    const TailProfile profile{{1, 1}, 1000, 0, "explicit mirror", true};
    REQUIRE(main->startTail(eof, 0, 2, profile, 100, 1000));
    REQUIRE(worker->startTail(eof, 0, 2, profile, 100, 1000));
    REQUIRE(worker->sampleTail(80)->horizon == 100);
    REQUIRE(main->pauseTail(20));
    REQUIRE(worker->adoptTailPause(20));
    REQUIRE(main->pausedTailTicks()->horizon == 40);
    REQUIRE(worker->pausedTailTicks()->horizon == 40);
    CHECK_FALSE(worker->permitsAdvance());
    REQUIRE(main->resumeTail(100, 3, 2200));
    REQUIRE(worker->resumeTail(100, 3, 2200));
    CHECK(main->sampleTail(110)->observationTick == 50);
    CHECK(worker->sampleTail(130)->observationTick == 70);
    CHECK(main->sampleTail(3000)->horizon == 2200);
    CHECK(worker->sampleTail(3000)->horizon == 2200);
}
#include <cuexis/playback/realtime_clock.hpp>

using namespace cuexis;
using namespace playback;
using namespace playback::detail;
namespace {
auto request(StepPhase phase) -> StepRequest {
    StepRequest r;
    r.key = {1, 1, Participant::capture, phase, 0};
    r.target.context = {1, 1, 1, 1, 1, 1, {}};
    r.target.expectedGeneration = 1;
    r.target.tick = 10;
    r.target.pauseEpoch = 1;
    return r;
}
auto closeProof(const StepRequest& r, const StepReceipt& cut) -> ControlReceipt {
    ControlReceipt proof;
    proof.request.context = r.target.context;
    proof.request.operationId = r.key.operationId;
    proof.request.kind = ControlKind::close;
    proof.request.target = r.target;
    proof.status = ControlStatus::preparing;
    proof.steps[0][static_cast<std::size_t>(StepPhase::cut)] = cut;
    return proof;
}
auto clockObservation() -> ClockObservation {
    ClockObservation o{};
    o.monotonicDomain = "b5.test";
    o.segment = 1;
    o.publicationSequence = 1;
    o.sampleMonotonicUs = 0;
    o.validUntilMonotonicUs = 1000;
    o.presented = ClockFramePosition{1, 1000};
    o.playback = ClockPlaybackState::Playing;
    o.quality = ClockQuality::Exact;
    return o;
}
} // namespace
TEST_CASE("B5 capture Close retains owning evidence and requires actual cut and acknowledgements",
          "[rpa][b5][capture]") {
    auto owner = CaptureRouteOwner::create({1, 1, 1}, 16, 5);
    REQUIRE(owner);
    GameplayInput input{{5}, 1, GameplayInputAction::Press, "lane", "domain", "keyboard", {}};
    REQUIRE(owner->capture(input));
    auto cut = owner->cut(request(StepPhase::cut));
    REQUIRE(cut);
    auto r = request(StepPhase::close);
    auto proof = closeProof(r, *cut);
    auto evidence = owner->exportCloseEvidence();
    REQUIRE(evidence);
    REQUIRE(evidence->pending().size() == 2);
    REQUIRE_FALSE(owner->close(r, proof, *evidence));
    SECTION("Normal delivery acknowledgement preserves its separate meaning") {
        REQUIRE(owner->acknowledge({1, 1, 1}, 1));
        REQUIRE_FALSE(owner->close(r, proof, *evidence));
        evidence = owner->exportCloseEvidence();
        REQUIRE(evidence);
    }
    SECTION("Explicit software retirement retains undelivered evidence without ACK") {
        auto software = OwnerStepLedger::create(1, Participant::software, 8, 2);
        REQUIRE(software);
        auto retired = r;
        retired.key.participant = Participant::software;
        retired.key.phase = StepPhase::retire;
        REQUIRE(*software->start(retired));
        std::vector<int> owned{1, 2};
        std::vector<int>{}.swap(owned);
        REQUIRE(owned.empty());
        REQUIRE(
            software->complete({retired, StepStatus::applied, {0, {}, {}, {}, false, true}, {}}));
        proof.steps[1][static_cast<std::size_t>(StepPhase::retire)] = *software->query(retired.key);
    }
    auto closed = owner->close(r, proof, *evidence);
    REQUIRE(closed);
    CHECK(closed->status == StepStatus::applied);
    CHECK(closed->effects.referencesReleased);
    REQUIRE(owner->close(r, proof, *evidence));
    CHECK_FALSE(owner->capture(input));
    auto anotherCut = request(StepPhase::cut);
    anotherCut.key.operationId = 2;
    CHECK_FALSE(owner->cut(anotherCut));
    CHECK(owner->pending()->empty());
}
TEST_CASE("B5 fault Close can release undelivered records only after an owning export",
          "[rpa][b5][capture]") {
    auto owner = CaptureRouteOwner::create({1, 1, 1}, 16, 5);
    REQUIRE(owner);
    GameplayInput input{{5}, 1, GameplayInputAction::Press, "lane", "domain", "keyboard", {}};
    REQUIRE(owner->capture(input));
    auto cut = owner->cut(request(StepPhase::cut));
    REQUIRE(cut);
    REQUIRE_FALSE(owner->capture(input, false));
    auto evidence = owner->exportCloseEvidence();
    REQUIRE(evidence);
    REQUIRE(evidence->pending().size() == 2);
    auto r = request(StepPhase::close);
    auto proof = closeProof(r, *cut);
    CaptureCloseEvidence missing;
    REQUIRE_FALSE(owner->close(r, proof, missing));
    REQUIRE_FALSE(owner->close(r, proof, *evidence));
    // This fixture owns only D1. A participating software owner must separately prove retirement.
    proof.request.participants[static_cast<std::size_t>(Participant::software)] = false;
    auto closed = owner->close(r, proof, *evidence);
    REQUIRE(closed);
    CHECK(closed->effects.referencesReleased);
    CHECK(evidence->pending().size() == 2);
    CHECK(std::get<GameplayClosedBatch>(evidence->pending()[0].payload).members.size() == 1);
    CHECK(owner->pending()->empty());
}
TEST_CASE("B5 capture readiness confirms the actual cut without inventing one",
          "[rpa][b5][capture]") {
    auto owner = CaptureRouteOwner::create({1, 1, 1}, 16, 5);
    REQUIRE(owner);
    REQUIRE(owner->prepare(request(StepPhase::prepare))->status == StepStatus::applied);
    SECTION("Fence before cut is terminal rejected") {
        CHECK(owner->fence(request(StepPhase::fence))->status == StepStatus::rejectedNotApplied);
    }
    SECTION("Fence after cut is applied and duplicate does not create deliveries") {
        REQUIRE(owner->cut(request(StepPhase::cut)));
        CHECK(owner->fence(request(StepPhase::fence))->status == StepStatus::applied);
        REQUIRE(owner->fence(request(StepPhase::fence)));
        CHECK(owner->pending()->size() == 1);
    }
}
TEST_CASE("B5 paused clock observations use frozen rational position without granting advance",
          "[rpa][b5][clock]") {
    auto clock = RealtimeClock::create({1000, 10, 16, "", true}, {{0, 1}, {1, 2}, {0, 1}, {1, 1}},
                                       "b5.test", 1);
    REQUIRE(clock);
    auto observation = clockObservation();
    auto playing = clock->sample(observation, 100);
    REQUIRE(playing);
    REQUIRE(clock->permitsAdvance());
    observation.publicationSequence = 2;
    observation.playback = ClockPlaybackState::Paused;
    auto paused = clock->observePaused(observation, 500);
    REQUIRE(paused);
    CHECK(paused->observationTick == 1000);
    CHECK(paused->horizon == 1000);
    CHECK_FALSE(clock->permitsAdvance());
    CHECK(clock->observePaused(observation, 600)->horizon == 1000);
    SECTION("Paused observation cannot lower normal horizon") {
        observation.publicationSequence = 3;
        observation.playback = ClockPlaybackState::Playing;
        observation.sampleMonotonicUs = 600;
        REQUIRE_FALSE(clock->sample(observation, 600));
        CHECK_FALSE(clock->permitsAdvance());
    }
    SECTION("Same publication changed payload is rejected") {
        observation.presented->frame = 2;
        REQUIRE_FALSE(clock->observePaused(observation, 700));
    }
}
TEST_CASE("B5 explicit host suspension preserves real owner terminal status", "[rpa][b5][host]") {
    HostContext c{1, 1, 1, 0, 1, 1, {}};
    auto host = HostCoordinator::create(c, 4);
    REQUIRE(host);
    auto r = request(StepPhase::activate);
    ControlRequest control{r.target.context, 1, 0, ControlKind::load, r.target};
    REQUIRE(host->accept(control, 0));
    StepReceipt actual{r, StepStatus::unknown, {}, {}};
    REQUIRE(host->observe(actual));
    REQUIRE(host->suspend(1));
    CHECK(host->state() == HostState::suspended);
    auto proof = host->query(1);
    REQUIRE(proof);
    CHECK(proof->steps[0][static_cast<std::size_t>(StepPhase::activate)]->status ==
          StepStatus::unknown);
    CHECK_FALSE(host->completeCleanup());
    CHECK_FALSE(host->suspend(2));
}
TEST_CASE("B5 inverse seek mapping preserves rational offsets and rejects unavailable source",
          "[rpa][b5][clock]") {
    ClockMappingConfiguration mapping{{7, 2}, {3, 2}, {-1, 2}, {3, 2}};
    auto position = RealtimeClock::seekPosition(mapping, 1);
    REQUIRE(position);
    CHECK(position->sourceUs == RealtimeRational{9, 2});
    CHECK(position->observationTick == 0); // -0.5 ties to the even integer zero.
    CHECK(position->horizon == 1);
    CHECK(position->presentationTick == 0); // +0.5 ties to the even integer zero.
    position = RealtimeClock::seekPosition(mapping, 0);
    REQUIRE(position);
    CHECK(position->sourceUs == RealtimeRational{7, 2});
    CHECK(position->observationTick == -2); // -1.5 ties to even -2.
    CHECK(position->presentationTick == 0);
    mapping.fixedRate = {1, 1};
    CHECK(RealtimeClock::seekPosition(mapping, 0)->sourceUs == RealtimeRational{7, 2});
    REQUIRE_FALSE(RealtimeClock::seekPosition(mapping, -4));
    mapping.timingOffsetUs = {1, 1};
    REQUIRE_FALSE(RealtimeClock::seekPosition(mapping, INT64_MAX));
    mapping.timingOffsetUs = {0, 1};
    mapping.displayLeadUs = {1, 1};
    REQUIRE_FALSE(RealtimeClock::seekPosition(mapping, INT64_MAX));
}
