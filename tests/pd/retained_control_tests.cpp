#include "../../app/player/src/execution_host.hpp"
#include "../rpa/execution_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <iostream>

using namespace cuexis;
using namespace playback;
using namespace playback::detail;
namespace {
constexpr std::string_view animatedChart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000201","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
            "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
  "templates":[],
  "behaviors":[{
    "id":"move","type":"behavior.transform.keyframe","version":1,
    "tracks":[
      {"property":"transform.position.x","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":0},
        {"beat":{"numerator":1,"denominator":1},"value":10,"easing":"linear"}]},
      {"property":"camera.fovY","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":60},
        {"beat":{"numerator":1,"denominator":1},"value":90,"easing":"linear"}]}
    ]
  }],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000210","name":"camera","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,-10],
                          "rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.camera":{"version":1,"type":"perspective","fovY":60,
                       "near":0.1,"far":1000},
      "cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";
auto settings() -> player::ExecutionHostOptions {
    player::ExecutionHostOptions o;
    o.session = {64, 16, 128, 32, 32, 64, true, {}};
    o.audio.testOnly = true;
    o.hostCapacity = 16;
    o.captureCapacity = 64;
    o.captureReserve = 16;
    o.replyDeadline = std::chrono::seconds{5};
    o.viewport = {640, 480};
    o.realtime = SessionRealtimeStart{
        {1000000, 10, 128, "", true}, {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, {}, "pd3c"};
    return o;
}
auto now() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
auto observation(const HostContext& c, std::uint64_t sequence, std::int64_t tick)
    -> ClockObservation {
    ClockObservation o{};
    o.monotonicDomain = "pd3c";
    o.segment = c.clockSegment;
    o.publicationSequence = sequence;
    o.sampleMonotonicUs = now();
    o.validUntilMonotonicUs = o.sampleMonotonicUs + 1000000;
    o.presented = ClockFramePosition{tick, 1000000};
    o.playback = ClockPlaybackState::Playing;
    o.quality = ClockQuality::Exact;
    return o;
}
void sameSource(const PresentationSourceInfo& a, const PresentationSourceInfo& b) {
    CHECK(a.sessionScope == b.sessionScope);
    CHECK(a.resultVersion == b.resultVersion);
    CHECK(a.contentRevision == b.contentRevision);
    CHECK(a.layoutRevision == b.layoutRevision);
    CHECK(a.worldRevision == b.worldRevision);
    CHECK(a.updateOrdinal == b.updateOrdinal);
    CHECK(a.source == b.source);
    CHECK(a.presentationTick == b.presentationTick);
    CHECK(a.workTick == b.workTick);
    CHECK(a.factCursor == b.factCursor);
    CHECK(a.controlCursor == b.controlCursor);
    CHECK(a.activationRevision == b.activationRevision);
    CHECK(a.hasFold == b.hasFold);
    REQUIRE(a.frame.has_value() == b.frame.has_value());
    if (a.frame) {
        CHECK(a.frame->chartTimeMs == b.frame->chartTimeMs);
        CHECK(a.frame->simulationDeltaTimeMs == b.frame->simulationDeltaTimeMs);
        CHECK(a.frame->timeDiscontinuityId == b.frame->timeDiscontinuityId);
    }
}
} // namespace
TEST_CASE("PD3c Pause drain redraws older last success and Resume preserves source",
          "[pd][retained-control]") {
    presentation_renderer::TestPresentationRenderer renderer;
    auto o = settings();
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, o.realtime->mapping);
    auto made = player::ExecutionHost::create(renderer, o, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation(), fixture.content()));
    CHECK_FALSE(host.sample(0, {}, true)); // no successful presentation exists
    CHECK(host.state() == HostState::running);
    auto first = host.sample(0, {0, 0, host.context().clockSegment}, false);
    REQUIRE(first);
    REQUIRE(first->source);
    auto firstReceipt = host.present(*first);
    REQUIRE(firstReceipt);
    REQUIRE(firstReceipt->status == PresentStatus::succeeded);
    CHECK_FALSE(host.present(*first)); // one attempt executes once
    auto newer = host.sample(1, {0.001, 0.001, host.context().clockSegment}, false);
    REQUIRE(newer);
    REQUIRE(newer->source->resultVersion > first->source->resultVersion);
    std::uint64_t sequence{};
    std::int64_t tick{100};
    const auto clockStart = now();
    player::ExecutionHost::ClockSource clock =
        [&](const HostContext& c) -> core::Result<ClockObservation> {
        return observation(c, ++sequence, tick + now() - clockStart);
    };
    REQUIRE(host.pause(clock));
    CHECK_FALSE(host.present(*newer)); // outstanding pre-control attempt is revoked
    newer->payload.reset();            // this reader explicitly ends before the resize replacement
    auto frozen = host.sample(999999, {999, 999, 999}, true);
    REQUIRE(frozen);
    CHECK(frozen->projectionScope == first->projectionScope);
    CHECK(frozen->frameId > newer->frameId);
    CHECK(frozen->context == host.context());
    CHECK(frozen->context != first->context);
    REQUIRE(frozen->source);
    sameSource(*frozen->source, *first->source);
    CHECK(frozen->presentationTick == first->presentationTick);
    CHECK(frozen->membershipView() == first->membershipView());
    CHECK(frozen->snapshot.objects.empty());
    CHECK(frozen->payload);
    auto originalDigest = computeFrameDigest(*first->runtimeFrame, first->snapshotView());
    auto frozenDigest = computeFrameDigest(*frozen->runtimeFrame, frozen->snapshotView());
    REQUIRE(originalDigest);
    REQUIRE(frozenDigest);
    CHECK(originalDigest->value == frozenDigest->value);
    auto frozenReceipt = host.present(*frozen);
    REQUIRE(frozenReceipt);
    REQUIRE(frozenReceipt->status == PresentStatus::succeeded);
    auto beforeResize = host.sample(888888, {}, true);
    REQUIRE(beforeResize);
    CHECK(beforeResize->payload == frozen->payload);
    REQUIRE(host.resize({1200, 400}));
    CHECK_FALSE(host.present(*beforeResize));
    auto resized = host.sample(888888, {}, true);
    REQUIRE(resized);
    CHECK(resized->snapshotView().viewportWidth == 1200);
    CHECK(resized->snapshotView().viewportHeight == 400);
    CHECK(resized->snapshotView().objects.size() == first->snapshotView().objects.size());
    sameSource(*resized->source, *first->source);
    REQUIRE(host.present(*resized));
    tick = 200;
    REQUIRE(host.resume(clock));
    auto afterResume = host.sample(777777, {}, true);
    REQUIRE(afterResume);
    CHECK(afterResume->projectionScope == first->projectionScope);
    sameSource(*afterResume->source, *first->source);
    REQUIRE(host.present(*afterResume));
    const auto originalSnapshot = first->snapshotView();
    first->payload.reset();
    frozen->payload.reset();
    beforeResize->payload.reset();
    auto running = host.sample(201, {0.201, 0, host.context().clockSegment}, false);
    REQUIRE(running);
    CHECK(running->projectionScope == first->projectionScope);
    CHECK(running->source->resultVersion > newer->source->resultVersion);
    REQUIRE(host.present(*running));
    tick = 300;
    auto pausedAgain = host.pause(clock);
    INFO((pausedAgain ? ""
                      : std::string{pausedAgain.error().code()} + ": " +
                            std::string{pausedAgain.error().message()}));
    REQUIRE(pausedAgain);
    auto archive = host.archive();
    REQUIRE(archive);
    auto cut = host.replayCut(*archive, {0});
    REQUIRE(cut);
    tick = 0;
    resized->payload.reset();
    afterResume->payload.reset();
    running->payload.reset();
    REQUIRE(host.seek(*archive, *cut, {0}, clock));
    CHECK_FALSE(host.sample(0, {}, true)); // Seek cannot reuse the old success
    auto sought = host.sample(0, {0, 0, host.context().clockSegment}, false);
    REQUIRE(sought);
    CHECK(sought->projectionScope != first->projectionScope);
    REQUIRE(host.present(*sought));
    REQUIRE(host.close());
    CHECK_FALSE(host.present(*sought));
    CHECK_FALSE(originalSnapshot.objects.empty()); // detached owning values survive Close
}
TEST_CASE("PD3c pure EOF clock binding preserves the successful presentation",
          "[pd][retained-control]") {
    presentation_renderer::TestPresentationRenderer renderer;
    auto o = settings();
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, o.realtime->mapping);
    auto made = player::ExecutionHost::create(renderer, o, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation(), fixture.content()));
    auto frame = host.sample(0, {0, 0, host.context().clockSegment}, false);
    REQUIRE(frame);
    REQUIRE(host.present(*frame));
    auto eof = observation(host.context(), 1, 0);
    eof.source = eof.presented;
    eof.playback = ClockPlaybackState::EndOfStream;
    REQUIRE(host.beginTail(eof, {{1, 1}, 100000, 0, "pd3c EOF fixture", true}));
    auto rebound = host.sample(999, {}, true);
    REQUIRE(rebound);
    CHECK(rebound->projectionScope == frame->projectionScope);
    CHECK(rebound->context.clockSegment > frame->context.clockSegment);
    sameSource(*rebound->source, *frame->source);
    REQUIRE(host.present(*rebound));
    REQUIRE(host.close());
}

TEST_CASE("PD3c retained backing survives newer World and sender mutation",
          "[pd][retained-control]") {
    presentation_renderer::TestPresentationRenderer renderer;
    auto o = settings();
    o.realtime.reset();
    auto made = player::ExecutionHost::create(renderer, o, {});
    REQUIRE(made);
    auto& host = **made;
    auto source = PlaybackSource::fromChartText(std::string{animatedChart});
    REQUIRE(source);
    REQUIRE(host.load(std::move(*source)));
    auto first = host.sample(0, {0, 0, 1}, false);
    REQUIRE(first);
    const auto original = first->snapshotView();
    CHECK(first->snapshotView().camera.fovY == 60);
    // The supplied value cannot replace the immutable owner-issued attempt.
    first->snapshot.camera.fovY = 1;
    first->snapshot.objects.clear();
    REQUIRE(host.present(*first));
    auto newer = host.sample(500000, {500, 500, 1}, false);
    REQUIRE(newer);
    CHECK(newer->snapshotView().camera.fovY == 90);
    CHECK(newer->snapshotView().objects[0].worldMatrix[12] != original.objects[0].worldMatrix[12]);
    auto retained = host.sample(999999, {999, 999, 999}, true);
    REQUIRE(retained);
    CHECK(retained->snapshotView().camera.fovY == 60);
    CHECK(retained->snapshotView().objects[0].worldMatrix[12] ==
          original.objects[0].worldMatrix[12]);
    CHECK(retained->source->resultVersion == first->source->resultVersion);
    CHECK_FALSE(retained->source->hasFold);
    CHECK_FALSE(retained->foldedFactCursor);
    CHECK_FALSE(retained->presentationTick);
    REQUIRE(host.present(*retained));
    REQUIRE(host.close());
}
TEST_CASE("PD3c unknown device and replacement generation forbid retained submission",
          "[pd][retained-control]") {
    for (const bool replace : {false, true}) {
        presentation_renderer::TestPresentationRenderer renderer;
        auto o = settings();
        o.realtime.reset();
        auto made = player::ExecutionHost::create(renderer, o, {});
        REQUIRE(made);
        auto& host = **made;
        REQUIRE(host.load(test_support::presentation()));
        auto first = host.sample(0, {0, 0, 1}, false);
        REQUIRE(first);
        REQUIRE(host.present(*first));
        if (replace)
            REQUIRE(renderer.rebuild());
        else
            renderer.markDeviceLost();
        auto retained = host.sample(0, {}, true);
        if (replace) {
            REQUIRE_FALSE(retained);
            CHECK(retained.error().code() == "host.rpa.context_stale");
        } else {
            REQUIRE(retained);
            auto receipt = host.present(*retained);
            REQUIRE(receipt);
            CHECK(receipt->status != PresentStatus::succeeded);
        }
        CHECK(host.state() == HostState::suspended);
        CHECK_FALSE(host.sample(0, {}, true));
    }
}

TEST_CASE("PD4 host updates without credit and reports NoRetainedFrame after explicit revocation",
          "[pd][pd4][retained-control]") {
    presentation_renderer::TestPresentationRenderer renderer;
    auto o = settings();
    o.realtime.reset();
    auto made = player::ExecutionHost::create(renderer, o, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation()));
    auto first = host.sample(0, {0, 0, 1}, false);
    REQUIRE(first);
    REQUIRE(host.present(*first));
    auto held = host.sample(1, {.001, .001, 1}, false);
    REQUIRE(held);
    REQUIRE(host.present(*held));
    auto skipped = host.trySample(2, {.002, .001, 1}, false);
    REQUIRE(skipped);
    CHECK_FALSE(*skipped);
    CHECK(host.deliveryStatus() == DeliveryStatus::skippedNoCredit);
    CHECK(host.state() == HostState::running);
    CHECK(host.storageMetrics().liveCount == 2);
    const auto version = held->source->resultVersion;
    first->payload.reset();
    auto recovered = host.sample(3, {.003, .001, 1}, false);
    REQUIRE(recovered);
    CHECK(recovered->source->resultVersion >=
          version + 2); // skipped delivery still committed update
    REQUIRE(host.present(*recovered));
    held->payload.reset();
    REQUIRE(host.revokeRetained());
    auto missing = host.trySample(999, {}, true);
    REQUIRE(missing);
    CHECK_FALSE(*missing);
    CHECK(host.deliveryStatus() == DeliveryStatus::noRetainedFrame);
    CHECK(host.state() == HostState::running);
    CHECK(host.storageMetrics().liveCount == 1); // external lease is not forcibly freed
    recovered->payload.reset();
    CHECK(host.storageMetrics().liveCount == 0);
    auto next = host.sample(4, {.004, .001, 1}, false);
    REQUIRE(next);
    REQUIRE(host.present(*next));
    auto backing = next->payload;
    REQUIRE(host.close());
    CHECK(host.storageMetrics().retiredCount == 1);
    CHECK_FALSE(backing->snapshot.objects.empty());
    next->payload.reset();
    made->reset(); // external storage is independent of the joined/destroyed Worker
    CHECK_FALSE(backing->snapshot.objects.empty());
    backing.reset();
}
TEST_CASE("PD4 actual host long steady state reclaims frames with a tiny observation journal",
          "[pd][pd4][long-host]") {
    presentation_renderer::TestPresentationRenderer renderer;
    auto o = settings();
    o.session.frameCapacity = 4;
    o.session.feedbackCapacity = 4;
    o.session.observation.bytes = 16384;
    o.session.observation.tombstones = 2;
    o.session.observation.coverageIntervals = 2;
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, o.realtime->mapping);
    auto made = player::ExecutionHost::create(renderer, o, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation(), fixture.content()));
    std::uint64_t last{};
    const auto clockStart = now();
    for (std::uint64_t i = 1; i <= 5000; ++i) {
        const auto tick = static_cast<std::int64_t>(i * 7000) + now() -
                          clockStart; // at least 35 seconds of source
        auto clock = observation(host.context(), i, tick);
        auto advanced = host.advance(clock);
        INFO(i);
        INFO((advanced ? ""
                       : std::string{advanced.error().code()} + ":" +
                             std::string{advanced.error().message()}));
        REQUIRE(advanced);
        REQUIRE(advanced->disposition != GameplayAdvanceDisposition::Failed);
        auto frame = host.sample(
            tick, {static_cast<double>(tick) / 1000., .007, host.context().clockSegment}, false);
        REQUIRE(frame);
        CHECK(frame->frameId > last);
        last = frame->frameId;
        REQUIRE(host.present(*frame));
        CHECK(host.storageMetrics().liveCount <= 2);
    }
    auto result = host.queryGameplay();
    REQUIRE(result);
    auto archive = host.archive();
    REQUIRE(archive);
    auto replay = host.evaluateReplay(*archive);
    REQUIRE(replay);
    REQUIRE(replay->evidenceValid);
    auto same = result->sameResult(replay->result);
    REQUIRE(same);
    CHECK(*same);
    CHECK(host.storageMetrics().peakCount <= 2);
    auto history = host.observationInfo();
    REQUIRE(history);
    CHECK(history->retainedAttempts <= 4);
    CHECK(history->bytes <= o.session.observation.bytes);
    CHECK(history->peakBytes <= o.session.observation.bytes);
    CHECK(history->incomplete.size() <= o.session.observation.coverageIntervals);
    CHECK(history->attemptsEvicted);
    std::cout << "PD4_LONG_HOST {\"attempts\":5000,\"payload_peak\":"
              << host.storageMetrics().peakCount << ",\"journal_bytes\":" << history->bytes
              << ",\"journal_peak_bytes\":" << history->peakBytes << ",\"replay_equal\":true}\n";
    REQUIRE(host.close());
    CHECK(host.storageMetrics().liveCount == 0);
}

TEST_CASE("PD4 paused retained revocation degrades explicitly and legal Resume restores delivery",
          "[pd][pd4][retained-control]") {
    presentation_renderer::TestPresentationRenderer renderer;
    auto o = settings();
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, o.realtime->mapping);
    auto made = player::ExecutionHost::create(renderer, o, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation(), fixture.content()));
    auto first = host.sample(0, {0, 0, host.context().clockSegment}, false);
    REQUIRE(first);
    REQUIRE(host.present(*first));
    const auto start = now();
    std::uint64_t sequence = 0;
    player::ExecutionHost::ClockSource clock =
        [&](const HostContext& c) -> core::Result<ClockObservation> {
        return observation(c, ++sequence, 100 + now() - start);
    };
    REQUIRE(host.pause(clock));
    REQUIRE(host.revokeRetained());
    auto missing = host.trySample(INT64_MAX, {999, 999, 999}, true);
    REQUIRE(missing);
    CHECK_FALSE(*missing);
    CHECK(host.deliveryStatus() == DeliveryStatus::noRetainedFrame);
    CHECK(host.state() == HostState::paused);
    first->payload.reset();
    REQUIRE(host.resume(clock));
    auto fresh = host.sample(1'000'000, {1000, 0, host.context().clockSegment}, false);
    REQUIRE(fresh);
    REQUIRE(host.present(*fresh));
    REQUIRE(host.close());
}
