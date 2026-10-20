#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <future>
#include <limits>

#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
#include "presentation_state.hpp"
#endif
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include "../rpa/execution_fixture.hpp"
#endif

using namespace cuexis;
namespace {
constexpr auto objectId = "019b0000-0000-7abc-8def-000000000210";
constexpr std::string_view chartJson = R"({"format":"cuexis.chart","version":1,
"chartId":"019b0000-0000-7abc-8def-000000000201","metadata":{},
"timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
"camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
"pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
"templates":[],"behaviors":[],"objects":[{
"id":"019b0000-0000-7abc-8def-000000000210","parent":null,
"components":{"cuexis.transform":{"version":1,"position":[2,3,4],
"rotation":[0,0,0,1],"scale":[1,1,1]},
"cuexis.camera":{"version":1,"type":"perspective","fovY":60,"near":0.1,"far":1000}},"extensions":{}}],
"requiredExtensions":[],"extensions":{}})";
} // namespace

TEST_CASE("PD3b synchronous owning and reusable snapshots share initial and lifetime semantics",
          "[pd][presentation-state]") {
    playback::PlaybackSession session;
    REQUIRE(session.loadChart(chartJson));
    auto initial = session.extractFrame({640, 480});
    REQUIRE(initial);
    REQUIRE(initial->objects.size() == 1);
    CHECK(initial->objects[0].id == objectId);
    CHECK(initial->objects[0].worldMatrix[12] == 2);
    CHECK(initial->objects[0].visible);
    CHECK(initial->objects[0].materialAssetId.empty());
    CHECK(initial->camera.active);
    const std::array writes{
        playback::HostOverrideWrite{objectId, playback::HostPropertyId::TransformPositionX, 7.0}};
    REQUIRE(session.acquireHostOverride(
        "host", 0, playback::hostPropertyBit(playback::HostPropertyId::TransformPositionX),
        {.kind = playback::HostOverrideLifetimeKind::RemainingFrames, .remainingFrames = 1},
        writes));
    const playback::RuntimeFrame frame{10, 10, 0};
    REQUIRE(session.update(frame));
    for (int i = 0; i < 3; ++i) {
        playback::FrameSnapshot reusable;
        REQUIRE(session.extractFrame({640, 480}, reusable));
        auto owning = session.extractFrame({640, 480});
        REQUIRE(owning);
        CHECK(owning->objects[0].worldMatrix[12] == 7);
        CHECK(playback::computeFrameDigest(frame, *owning)->value ==
              playback::computeFrameDigest(frame, reusable)->value);
        CHECK_FALSE(session.extractFrame({0, 480}));
        REQUIRE(session.chartInfo());
        REQUIRE(session.extractFrame({800, 600}));
    }
    REQUIRE(session.update({10, 0, 0}));
    CHECK(session.extractFrame({640, 480})->objects[0].worldMatrix[12] == 2);
    CHECK(initial->objects[0].worldMatrix[12] == 2); // old owning values survive newer state
}

TEST_CASE("PD3b camera materialization failure preserves the successfully updated World",
          "[pd][presentation-state]") {
    playback::PlaybackSession session;
    REQUIRE(session.loadChart(chartJson));
    auto original = session.extractFrame({640, 480});
    REQUIRE(original);
    const std::array writes{playback::HostOverrideWrite{
        objectId, playback::HostPropertyId::TransformScale, core::Vec3{0, 1, 1}}};
    REQUIRE(session.acquireHostOverride(
        "singular", 0, playback::hostPropertyBit(playback::HostPropertyId::TransformScale), {},
        writes));
    REQUIRE(session.update({10, 10, 0}));
    for (int i = 0; i < 2; ++i) {
        auto extracted = session.extractFrame({640, 480});
        REQUIRE_FALSE(extracted);
        CHECK(extracted.error().code() == "playback.snapshot.camera_not_invertible");
    }
    CHECK(original->camera.active);
}

#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
TEST_CASE("PD3b committed descriptor has exact current version and genuine source absence",
          "[pd][presentation-state]") {
    using Access = playback::detail::PresentationAccess;
    using Source = internal::presentation::Source;
    playback::PlaybackSession session;
    CHECK_FALSE(Access::state(session));
    REQUIRE(session.loadChart(chartJson));
    const auto prepared = *Access::state(session);
    CHECK(prepared.resultVersion == 1);
    CHECK(prepared.contentRevision == 1);
    CHECK(prepared.layoutRevision == 1);
    CHECK(prepared.worldRevision == 1);
    CHECK(prepared.updateOrdinal == 0);
    CHECK(prepared.source == Source::Prepared);
    CHECK_FALSE(prepared.frame);
    CHECK_FALSE(prepared.presentationTick);
    CHECK_FALSE(prepared.factCursor);
    CHECK_FALSE(prepared.controlCursor);
    CHECK_FALSE(prepared.workTick);
    CHECK_FALSE(prepared.hasFold);
    playback::FrameSnapshot snapshot;
    REQUIRE(Access::materialize(session, prepared.resultVersion, {640, 480}, snapshot));
    CHECK(Access::state(session)->resultVersion == prepared.resultVersion);
    auto wrongOwner =
        std::async(std::launch::async, [&] { return Access::state(session).has_value(); });
    CHECK_FALSE(wrongOwner.get());
    const playback::RuntimeFrame frame{20, 20, 9};
    REQUIRE(session.update(frame));
    const auto updated = *Access::state(session);
    CHECK(updated.resultVersion == prepared.resultVersion + 1);
    CHECK(updated.worldRevision == prepared.worldRevision + 1);
    CHECK(updated.updateOrdinal == 1);
    CHECK(updated.source == Source::RuntimeUpdate);
    REQUIRE(updated.frame);
    CHECK(updated.frame->chartTimeMs == 20);
    CHECK(updated.frame->timeDiscontinuityId == 9);
    auto old = Access::materialize(session, prepared.resultVersion, {640, 480}, snapshot);
    REQUIRE_FALSE(old);
    CHECK(old.error().code() == "playback.presentation.version_unavailable");
    REQUIRE_FALSE(session.update({std::numeric_limits<double>::max(), 0, 9}));
    CHECK(Access::state(session)->resultVersion == updated.resultVersion);
    const std::array writes{
        playback::HostOverrideWrite{objectId, playback::HostPropertyId::TransformPositionX, 7.0}};
    auto token = session.acquireHostOverride(
        "host", 0, playback::hostPropertyBit(playback::HostPropertyId::TransformPositionX), {},
        writes);
    REQUIRE(token);
    auto mutated = *Access::state(session);
    CHECK(mutated.resultVersion == updated.resultVersion + 1);
    CHECK(mutated.updateOrdinal == 1);
    CHECK(mutated.source == Source::Mutation);
    CHECK_FALSE(mutated.frame);
    REQUIRE(session.releaseHostOverride(*token));
    CHECK(Access::state(session)->resultVersion == mutated.resultVersion + 1);
    REQUIRE(session.reload(chartJson, {250, 0, 10}, playback::ReloadPolicy::KeepChartTime));
    const auto restored = *Access::state(session);
    CHECK(restored.source == Source::Restore);
    CHECK(restored.sessionScope == prepared.sessionScope);
    CHECK(restored.runtime.owner != prepared.runtime.owner);
    CHECK(restored.layoutRevision == 2);
    CHECK(restored.contentRevision == 2);
    REQUIRE(restored.frame);
    CHECK(restored.frame->chartTimeMs == 250);
    REQUIRE(session.unload());
    CHECK_FALSE(Access::state(session));
    REQUIRE(session.loadChart(chartJson));
    CHECK(Access::state(session)->resultVersion > restored.resultVersion);
    CHECK(Access::state(session)->layoutRevision == 3);
}

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
TEST_CASE("PD3b realtime descriptor records the actual Fold and excludes later control metadata",
          "[pd][presentation-state]") {
    using namespace playback;
    using Access = playback::detail::PresentationAccess;
    test_support::SessionTestContent content;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    const GameplayExecutionContext context{1, 1, 1};
    test_support::rpa(content, mapping);
    PlaybackSession session;
    content.load(session);
    auto initial = *Access::state(session);
    CHECK_FALSE(initial.hasFold);
    CHECK_FALSE(initial.factCursor);
    REQUIRE(session.beginRealtimeGameplay({1000000, 100, 4096, "pd3b-source", true}, mapping,
                                          context, "pd3b.synthetic"));
    const ClockObservation clock{"pd3b.synthetic",
                                 1,
                                 1,
                                 11,
                                 1000011,
                                 0,
                                 ClockFramePosition{10, 1000000},
                                 ClockFramePosition{10, 1000000},
                                 {},
                                 ClockPlaybackState::Playing,
                                 ClockQuality::Exact,
                                 false,
                                 false,
                                 false};
    REQUIRE(session.deliverGameplayCapture(
        {context, 0, GameplayClosedBatch{{5}, {6}, 0, 0, 1, 1, {{test_support::input(), 0}}}},
        clock, 11));
    REQUIRE(session.deliverGameplayCapture({context, 1, GameplayCapturePrefix{{10}}}, clock, 11));
    auto decision = session.requestGameplayAdvance({context, 1, 0, {10}}, clock, 11);
    REQUIRE(decision);
    CHECK(decision->disposition == GameplayAdvanceDisposition::Executed);
    CHECK(Access::state(session)->resultVersion == initial.resultVersion);
    REQUIRE(session.sampleGameplayPresentation({10}, {0.01, 0.01, 0}));
    auto published = *Access::state(session);
    CHECK(published.hasFold);
    REQUIRE(published.workTick);
    REQUIRE(published.factCursor);
    REQUIRE(published.controlCursor);
    auto lag = session.gameplayExecutionLag({10});
    REQUIRE(lag);
    CHECK(published.factCursor == lag->foldedFactCursor);
    REQUIRE(lag->foldedWorkTick);
    CHECK(published.workTick == lag->foldedWorkTick->value);
    REQUIRE(session.retireGameplayAdvance(1));
    REQUIRE(session.queryGameplayPresentation());
    REQUIRE(session.extractFrame({800, 600}));
    CHECK(Access::state(session)->resultVersion == published.resultVersion);
    CHECK(Access::state(session)->controlCursor == published.controlCursor);
    CHECK(Access::state(session)->workTick == published.workTick);
}

TEST_CASE("PD3b GameplayOnly has no fabricated presentation state", "[pd][presentation-state]") {
    test_support::SessionTestContent content;
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto source = playback::PlaybackSource::fromGameplayPacked(
        "headless", "compiled/main.packed", content.packed(), content.config, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(source);
    playback::PlaybackSession session{{.version = 1, .ids = {}}};
    REQUIRE(session.load(std::move(*source), playback::PlaybackMode::ChartClock));
    CHECK_FALSE(playback::detail::PresentationAccess::state(session));
    REQUIRE(session.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK_FALSE(playback::detail::PresentationAccess::state(session));
    CHECK_FALSE(session.extractFrame({640, 480}));
}

TEST_CASE("PD3b projection descriptor keeps actual source across reads controls and retry",
          "[pd][presentation-state]") {
    using Access = playback::detail::PresentationAccess;
    test_support::SessionTestContent content;
    playback::PlaybackSession session;
    content.load(session);
    REQUIRE(session.update({0, 0, 0}));
    const auto before = *Access::state(session);
    const auto event = test_support::input();
    REQUIRE(session.submitGameplay(std::span{&event, 1}));
    REQUIRE_FALSE(session.advanceGameplay({20}, {8}, {std::numeric_limits<double>::max(), 0, 0}));
    CHECK(Access::state(session)->resultVersion == before.resultVersion);
    REQUIRE(session.sampleGameplayPresentation({8}, {8, 8, 0}));
    auto published = *Access::state(session);
    CHECK(published.resultVersion == before.resultVersion + 1);
    CHECK(published.source == internal::presentation::Source::RuntimeUpdate);
    CHECK(published.presentationTick == 8);
    REQUIRE(published.factCursor);
    CHECK(*published.factCursor == 1);
    REQUIRE(published.frame);
    CHECK(published.frame->chartTimeMs == 8);
    CHECK(published.activationRevision == published.resultVersion);
    REQUIRE(session.queryGameplayPresentation());
    REQUIRE(session.gameplayAdvanceReceipt());
    REQUIRE(session.extractFrame({640, 480}));
    const auto activationRevision = published.activationRevision;
    REQUIRE(session.update({8, 0, 0}));
    auto ordinary = *Access::state(session);
    CHECK(ordinary.activationRevision == activationRevision);
    CHECK(ordinary.updateOrdinal == published.updateOrdinal + 1);
    CHECK_FALSE(ordinary.factCursor);
    published = ordinary;
    REQUIRE(session.controlGameplay(playback::GameplayControl::Pause));
    CHECK(Access::state(session)->resultVersion == published.resultVersion);
    CHECK(Access::state(session)->presentationTick == published.presentationTick);
    REQUIRE(session.controlGameplay(playback::GameplayControl::Resume));
    CHECK(Access::state(session)->resultVersion == published.resultVersion);
    REQUIRE(session.controlGameplay(playback::GameplayControl::Reset));
    auto reset = *Access::state(session);
    CHECK(reset.resultVersion == published.resultVersion + 1);
    CHECK(reset.source == internal::presentation::Source::Restore);
    CHECK(reset.updateOrdinal == published.updateOrdinal);
    CHECK_FALSE(reset.factCursor);
    CHECK_FALSE(reset.presentationTick);
}
#endif
#endif

TEST_CASE("PD3c public committed source query preserves absence and revisions",
          "[pd][source-query]") {
    playback::PlaybackSession session;
    auto empty = session.presentationSource();
    REQUIRE_FALSE(empty);
    CHECK(empty.error().code() == "playback.session.empty");
    REQUIRE(session.loadChart(chartJson));
    auto prepared = session.presentationSource();
    REQUIRE(prepared);
    CHECK(prepared->source == playback::PresentationSource::Prepared);
    CHECK_FALSE(prepared->frame);
    CHECK_FALSE(prepared->presentationTick);
    CHECK_FALSE(prepared->factCursor);
    CHECK_FALSE(prepared->controlCursor);
    CHECK_FALSE(prepared->activationRevision);
    CHECK_FALSE(prepared->hasFold);
    REQUIRE(session.extractFrame({640, 480}));
    CHECK(session.presentationSource()->resultVersion == prepared->resultVersion);
    REQUIRE(session.update({20, 5, 3}));
    auto updated = session.presentationSource();
    REQUIRE(updated);
    CHECK(updated->source == playback::PresentationSource::RuntimeUpdate);
    CHECK(updated->resultVersion > prepared->resultVersion);
    REQUIRE(updated->frame);
    CHECK(updated->frame->chartTimeMs == 20);
    CHECK_FALSE(updated->hasFold);
    CHECK_FALSE(updated->factCursor);
    auto wrongThread =
        std::async(std::launch::async, [&] { return session.presentationSource(); }).get();
    REQUIRE_FALSE(wrongThread);
    CHECK(session.presentationSource()->resultVersion == updated->resultVersion);
    const std::array writes{
        playback::HostOverrideWrite{objectId, playback::HostPropertyId::TransformPositionX, 7.0}};
    auto token = session.acquireHostOverride(
        "host", 0, playback::hostPropertyBit(playback::HostPropertyId::TransformPositionX), {},
        writes);
    REQUIRE(token);
    auto mutated = session.presentationSource();
    REQUIRE(mutated);
    CHECK(mutated->source == playback::PresentationSource::Mutation);
    CHECK(mutated->resultVersion == updated->resultVersion + 1);
    CHECK(mutated->updateOrdinal == updated->updateOrdinal);
    CHECK_FALSE(mutated->frame);
    CHECK_FALSE(mutated->hasFold);
    CHECK_FALSE(mutated->factCursor);
    REQUIRE(session.releaseHostOverride(*token));
    REQUIRE(session.reload(chartJson, {250, 0, 10}, playback::ReloadPolicy::KeepChartTime));
    auto restored = session.presentationSource();
    REQUIRE(restored);
    CHECK(restored->source == playback::PresentationSource::Restore);
    CHECK(restored->sessionScope == prepared->sessionScope);
    CHECK(restored->contentRevision == prepared->contentRevision + 1);
    CHECK(restored->layoutRevision == prepared->layoutRevision + 1);
    REQUIRE(restored->frame);
    CHECK(restored->frame->chartTimeMs == 250);
    CHECK_FALSE(restored->hasFold);
    CHECK_FALSE(restored->factCursor);
    REQUIRE(session.unload());
    CHECK_FALSE(session.presentationSource());
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    test_support::SessionTestContent content;
    content.load(session);
    const auto event = test_support::input();
    REQUIRE(session.submitGameplay(std::span{&event, 1}));
    REQUIRE(session.advanceGameplay({20}, {8}, {8, 8, 0}));
    auto projected = session.presentationSource();
    REQUIRE(projected);
    REQUIRE(projected->hasFold);
    REQUIRE(projected->activationRevision);
    REQUIRE(session.update({8, 0, 0}));
    auto ordinary = session.presentationSource();
    REQUIRE(ordinary);
    CHECK_FALSE(ordinary->hasFold);
    CHECK_FALSE(ordinary->factCursor);
    CHECK(ordinary->activationRevision == projected->activationRevision);
    const auto version = ordinary->resultVersion;
    const auto ordinal = ordinary->updateOrdinal;
    REQUIRE(session.controlGameplay(playback::GameplayControl::Pause));
    CHECK(session.presentationSource()->resultVersion == version);
    REQUIRE(session.controlGameplay(playback::GameplayControl::Resume));
    CHECK(session.presentationSource()->resultVersion == version);
    CHECK(session.presentationSource()->updateOrdinal == ordinal);
    CHECK(session.presentationSource()->activationRevision == projected->activationRevision);
#endif
}
