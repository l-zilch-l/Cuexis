#include "../rpa/execution_fixture.hpp"
#include "../support/environment.hpp"
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <cuexis_internal/sha256.hpp>
#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
#include "pd_allocation_probe.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace cuexis;
using namespace playback;
using Clock = std::chrono::steady_clock;
constexpr std::size_t warmupSteps = 64, measuredSteps = 1024, trajectories = 8;
constexpr std::size_t readBlocks = 128, readsPerBlock = 4096;
constexpr ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
constexpr GameplayExecutionContext context{1, 1, 1};

template <class T> auto take(core::Result<T> result) -> T {
    if (!result)
        throw std::runtime_error(std::string{result.error().code()} + ": " +
                                 std::string{result.error().message()});
    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}
void verify(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
auto elapsed(Clock::time_point start) -> double {
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}
void numbers(const std::vector<double>& values) {
    std::cout << '[';
    for (std::size_t i = 0; i < values.size(); ++i)
        std::cout << (i ? "," : "") << values[i];
    std::cout << ']';
}
auto observation(std::size_t index) -> ClockObservation {
    const auto position = static_cast<std::int64_t>(index + 1) * 10;
    return {"pd.synthetic",
            1,
            index + 1,
            position + 1,
            position + 1000001,
            0,
            ClockFramePosition{position, 1000000},
            ClockFramePosition{position, 1000000},
            {},
            ClockPlaybackState::Playing,
            ClockQuality::Exact,
            false,
            false,
            false};
}
auto configuration(bool realtime) -> test_support::SessionTestContent {
    test_support::SessionTestContent fixture;
    if (realtime)
        test_support::rpa(fixture, mapping);
    const auto& e = fixture.request.assembly.sources[0].document.requirements[0].identity;
    GameplayRequirementRef ref{e.chartEntryId, e.invocationId,       e.moduleId,
                               e.exportId,     e.requirementLocalId, {}};
    for (const auto& step : e.emissionPath)
        ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
    fixture.config.bindings = {{"binding.one",
                                ref,
                                GameplayPhase::Tap,
                                GameplayOutcome::Hit,
                                GameplayTimingClass::Any,
                                "019b0000-0000-7abc-8def-000000000002",
                                false,
                                {0},
                                GameplayPresentationTick{1000}}};
    return fixture;
}
auto prepare(const GameplayContent& content, bool realtime) -> std::unique_ptr<PlaybackSession> {
#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
    const auto liveBefore = pd_allocation::live.load();
    pd_allocation::peak.store(liveBefore);
    pd_allocation::count.store(0);
    pd_allocation::bytes.store(0);
    pd_allocation::tracking = true;
#endif
    auto session = std::make_unique<PlaybackSession>();
    auto prepared = take(session->prepareGameplayLoad(test_support::presentation(),
                                                      PlaybackMode::ChartClock, content));
    take(session->commit(std::move(prepared)));
    if (realtime) {
        take(session->beginRealtimeGameplay({1000000, 100, 4096, "pd-fixed-rpa-v1", true}, mapping,
                                            context, "pd.synthetic"));
        take(session->deliverGameplayCapture(
            {context, 0, GameplayClosedBatch{{5}, {6}, 0, 0, 1, 1, {{test_support::input(), 0}}}},
            observation(0), 11));
    } else {
        const auto event = test_support::input();
        take(session->submitGameplay(std::span{&event, 1}));
    }
#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
    pd_allocation::tracking = false;
    const auto liveAfter = pd_allocation::live.load();
    const auto high = pd_allocation::peak.load();
    const auto count = pd_allocation::count.load();
    const auto bytes = pd_allocation::bytes.load();
    std::cout << "PD_MEMORY {\"phase\":\"prepare\",\"allocation_count\":" << count
              << ",\"requested_bytes\":" << bytes << ",\"live_before\":" << liveBefore
              << ",\"live_after\":" << liveAfter << ",\"peak_live\":" << high << "}\n";
#endif
    return session;
}
struct Evidence final {
    std::uint64_t digest{14695981039346656037ULL};
    std::string replay, snapshot;
    double traceUs{};
};
auto step(PlaybackSession& session, bool realtime, std::size_t i, FrameSnapshot& snapshot)
    -> std::uint64_t {
    const auto tick = static_cast<std::int64_t>(i + 1) * 10;
    const RuntimeFrame frame{realtime ? static_cast<double>(tick) / 1000.0
                                      : static_cast<double>(tick),
                             realtime ? 0.01 : 10, 0};
    if (realtime) {
        const auto clock = observation(i);
        take(session.deliverGameplayCapture({context, i + 1, GameplayCapturePrefix{{tick}}}, clock,
                                            tick + 1));
        const auto decision =
            take(session.requestGameplayAdvance({context, i + 1, i, {tick}}, clock, tick + 1));
        verify(decision.disposition == GameplayAdvanceDisposition::Executed,
               "RPA fixed trace did not execute");
        take(session.sampleGameplayPresentation({tick}, frame));
        take(session.retireGameplayAdvance(i + 1));
    } else {
        take(session.advanceGameplay({tick}, {tick}, frame));
    }
    take(session.extractFrame({1280, 720}, snapshot));
    verify(snapshot.objects.size() == 1 && snapshot.objects[0].visible == (tick >= 1000),
           "fixed trace visibility differs from oracle");
    return take(computeFrameDigest(frame, snapshot)).value;
}
auto trace(PlaybackSession& session, bool realtime, bool recording) -> Evidence {
    Evidence result;
    FrameSnapshot snapshot;
    internal::pd::setRecording(false);
    for (std::size_t i = 0; i < warmupSteps; ++i)
        static_cast<void>(step(session, realtime, i, snapshot));
    internal::pd::setRecording(recording);
    {
        // Observe the active binding window separately from the fixed timed trace.
        const auto feedback = take(session.gameplayFeedback(1));
        verify(feedback.size() == 1, "fixed trace feedback window differs from oracle");
    }
#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
    const auto liveBefore = pd_allocation::live.load();
    pd_allocation::peak.store(liveBefore);
    pd_allocation::count.store(0);
    pd_allocation::bytes.store(0);
    pd_allocation::tracking = true;
#endif
    const auto start = Clock::now();
    for (std::size_t i = warmupSteps; i < warmupSteps + measuredSteps; ++i)
        result.digest = (result.digest ^ step(session, realtime, i, snapshot)) * 1099511628211ULL;
    result.traceUs = elapsed(start);
#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
    pd_allocation::tracking = false;
    const auto liveAfter = pd_allocation::live.load();
    const auto high = pd_allocation::peak.load();
    const auto count = pd_allocation::count.load();
    const auto bytes = pd_allocation::bytes.load();
    std::cout << "PD_MEMORY {\"phase\":\"trace\",\"recording\":" << (recording ? "true" : "false")
              << ",\"steps\":1024,\"allocation_count\":" << count
              << ",\"requested_bytes\":" << bytes << ",\"live_before\":" << liveBefore
              << ",\"live_after\":" << liveAfter << ",\"peak_live\":" << high << "}\n";
#endif
    internal::pd::setRecording(false);
    const auto query = take(session.queryGameplay());
    const auto score = take(query.score());
    verify(score.hits == 1 && score.score == 2 && take(query.factCount()) == 1 &&
               take(query.publishableFactCount()) == 1,
           "fixed trace Gameplay result differs from oracle");
    auto archive = take(session.archiveGameplay());
    const auto replay = take(session.evaluateGameplayReplay(archive));
    verify(replay.evidenceValid && take(query.sameResult(replay.result)),
           "fixed trace complete Replay differs");
    result.replay = core::detail::sha256Hex(take(archive.toBytes()));
    result.snapshot = core::detail::sha256Hex(take(take(session.snapshotGameplay()).toBytes()));
    if (!realtime)
        verify(result.digest == 17504891284788938091ULL, "legacy trajectory digest changed");
    return result;
}
} // namespace

TEST_CASE("PD fixed trajectory measurement", "[.][pd][method]") {
    const auto profile = cuexis::test_support::environmentVariable("CUEXIS_PD_PROFILE");
    if (profile.empty())
        SKIP("Explicit PD measurement profile required");
    REQUIRE((profile == "legacy" || profile == "rpa"));
    const bool realtime = profile == "rpa";
    internal::pd::setRecording(false);
    auto fixture = configuration(realtime);
    const auto packed = fixture.packed();
    const auto content = take(GameplayContent::fromPacked(packed, fixture.config));
    auto warm = prepare(content, realtime);
    const auto golden = trace(*warm, realtime, false);
    warm.reset();
    std::vector<double> prepareUs, traceUs, readUs, emptyReadUs;
    std::unique_ptr<PlaybackSession> last;
    for (std::size_t i = 0; i < trajectories; ++i) {
        const auto start = Clock::now();
        auto session = prepare(content, realtime);
        prepareUs.push_back(elapsed(start));
        const auto actual = trace(*session, realtime, true);
        REQUIRE(actual.digest == golden.digest);
        REQUIRE(actual.replay == golden.replay);
        REQUIRE(actual.snapshot == golden.snapshot);
        traceUs.push_back(actual.traceUs);
        last = std::move(session);
    }
    // Same committed state for every read block. No new update or history extension.
    FrameSnapshot snapshot;
    take(last->extractFrame({1280, 720}, snapshot));
    const RuntimeFrame frame{realtime ? 10.88 : 10880, realtime ? 0.01 : 10, 0};
    const auto readDigest = take(computeFrameDigest(frame, snapshot)).value;
    for (std::size_t block = 0; block < readBlocks; ++block) {
        auto start = Clock::now();
        for (std::size_t i = 0; i < readsPerBlock; ++i)
            std::atomic_signal_fence(std::memory_order_seq_cst);
        emptyReadUs.push_back(elapsed(start));
        start = Clock::now();
        for (std::size_t i = 0; i < readsPerBlock; ++i)
            take(last->extractFrame({1280, 720}, snapshot));
        readUs.push_back(elapsed(start));
        REQUIRE(take(computeFrameDigest(frame, snapshot)).value == readDigest);
    }
    // Read-only block must not mutate the semantic snapshot or Replay.
    REQUIRE(core::detail::sha256Hex(take(take(last->archiveGameplay()).toBytes())) ==
            golden.replay);
    REQUIRE(core::detail::sha256Hex(take(take(last->snapshotGameplay()).toBytes())) ==
            golden.snapshot);
    const auto configurationText = take(encodeGameplayConfiguration(fixture.config));
    std::cout << std::setprecision(17) << "PD_METHOD {\"profile\":\"" << profile
              << "\",\"digest\":" << golden.digest << ",\"replay_sha256\":\"" << golden.replay
              << "\",\"snapshot_sha256\":\"" << golden.snapshot << "\",\"packed_sha256\":\""
              << core::detail::sha256Hex(packed) << "\",\"configuration_sha256\":\""
              << core::detail::sha256Hex(core::detail::sha256(configurationText))
              << "\",\"fact_count\":1,\"publishable_cursor\":1,\"hits\":1,\"score\":2,"
                 "\"feedback_scope\":1,\"binding_window\":[0,1000],\"result_version\":null,"
                 "\"consumer_delivery_age\":null,\"warmup_steps\":64,\"steps_per_trace\":1024,"
                 "\"read_operations_per_block\":"
                 "4096,\"trace_us\":";
    numbers(traceUs);
    std::cout << ",\"prepare_us\":";
    numbers(prepareUs);
    std::cout << ",\"read_block_us\":";
    numbers(readUs);
    std::cout << ",\"empty_read_block_us\":";
    numbers(emptyReadUs);
    std::cout << "}\n";
}

TEST_CASE("PD clock and observer calibration", "[.][pd][method]") {
    internal::pd::setRecording(false);
    std::vector<double> steps;
    auto previous = Clock::now();
    for (std::size_t i = 0; i < 100000; ++i) {
        const auto now = Clock::now();
        steps.push_back(std::chrono::duration<double, std::nano>(now - previous).count());
        previous = now;
    }
    std::sort(steps.begin(), steps.end());
    const auto positive = std::find_if(steps.begin(), steps.end(), [](double x) { return x > 0; });
    REQUIRE(positive != steps.end());
    internal::pd::setRecording(true);
    const auto started = Clock::now();
    for (std::size_t i = 0; i < 4096; ++i) {
        internal::pd::Timer parent{internal::pd::Stage::projection};
        internal::pd::Timer child{internal::pd::Stage::runtime};
        child.items(1);
    }
    const auto observerUs = elapsed(started);
    internal::pd::setRecording(false);
    std::cout << std::setprecision(17) << "PD_CLOCK {\"minimum_positive_ns\":" << *positive
              << ",\"adjacent_median_ns\":" << steps[50000]
              << ",\"adjacent_p99_ns\":" << steps[99000]
              << ",\"nested_empty_block_us\":" << observerUs
              << ",\"nested_empty_operations\":4096}\n";
}

#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
TEST_CASE("PD allocation observer checks requested bytes and alignment", "[pd][allocation]") {
    internal::pd::setRecording(false);
    const auto initial = pd_allocation::live.load();
    pd_allocation::peak.store(initial);
    pd_allocation::count.store(0);
    pd_allocation::bytes.store(0);
    pd_allocation::tracking = true;
    auto* ordinary = ::operator new(37);
    auto* aligned = ::operator new(73, std::align_val_t{256});
    const auto observedCount = pd_allocation::count.load();
    const auto observedBytes = pd_allocation::bytes.load();
    const auto observedLive = pd_allocation::live.load();
    const auto observedPeak = pd_allocation::peak.load();
    pd_allocation::tracking = false;
    const auto address = reinterpret_cast<std::uintptr_t>(aligned);
    ::operator delete(ordinary);
    ::operator delete(aligned, std::align_val_t{256});
    const auto finalLive = pd_allocation::live.load();
    REQUIRE(observedCount == 2);
    REQUIRE(observedBytes == 110);
    REQUIRE(observedLive == initial + 110);
    REQUIRE(observedPeak == initial + 110);
    REQUIRE(finalLive == initial);
    REQUIRE(address % 256 == 0);
}
#endif
