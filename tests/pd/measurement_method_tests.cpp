#include "../rpa/execution_fixture.hpp"
#include "../support/environment.hpp"
#include "measurement_clocks.hpp"
#include "measurement_environment.hpp"
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <cuexis_internal/sha256.hpp>
#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
#include "../../app/execution_host/frame_observation.hpp"
#include "pd_allocation_probe.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
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
void playbackStep(PlaybackSession& session, bool realtime, std::size_t i) {
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
}
auto checkedStep(PlaybackSession& session, bool realtime, std::size_t i, FrameSnapshot& snapshot)
    -> std::uint64_t {
    playbackStep(session, realtime, i);
    const auto tick = static_cast<std::int64_t>(i + 1) * 10;
    const RuntimeFrame frame{realtime ? static_cast<double>(tick) / 1000.0
                                      : static_cast<double>(tick),
                             realtime ? 0.01 : 10, 0};
    take(session.extractFrame({1280, 720}, snapshot));
    verify(snapshot.objects.size() == 1 && snapshot.objects[0].visible == (tick >= 1000),
           "fixed trace visibility differs from oracle");
    return take(computeFrameDigest(frame, snapshot)).value;
}
auto trace(PlaybackSession& session, bool realtime, bool recording,
           pd_measure::Sample* clocks = nullptr) -> Evidence {
    Evidence result;
    FrameSnapshot snapshot;
    internal::pd::setRecording(false);
    for (std::size_t i = 0; i < warmupSteps; ++i)
        static_cast<void>(checkedStep(session, realtime, i, snapshot));
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
    const auto work = [&] {
        for (std::size_t i = warmupSteps; i < warmupSteps + measuredSteps; ++i)
            result.digest =
                (result.digest ^ checkedStep(session, realtime, i, snapshot)) * 1099511628211ULL;
    };
    if (clocks) {
        *clocks = pd_measure::measure(work);
        result.traceUs = clocks->wallUs;
    } else {
        const auto start = Clock::now();
        work();
        result.traceUs = elapsed(start);
    }
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
auto playbackOnly(PlaybackSession& session, bool realtime, pd_measure::Sample* clocks = nullptr)
    -> pd_measure::Sample {
    for (std::size_t i = 0; i < warmupSteps; ++i)
        playbackStep(session, realtime, i);
    const auto work = [&] {
        for (std::size_t i = warmupSteps; i < warmupSteps + measuredSteps; ++i)
            playbackStep(session, realtime, i);
    };
    const auto sample = pd_measure::measure(work);
    if (clocks)
        *clocks = sample;
    return sample;
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

#if defined(_WIN32) && !defined(CUEXIS_PD_ALLOCATION_OBSERVER)
namespace {
void sampleJson(const pd_measure::Sample& s) {
    std::cout << "{\"wall_us\":" << s.wallUs << ",\"cpu_us\":" << s.cpuUs
              << ",\"cycles\":" << s.cycles << ",\"qpc_start\":" << s.qpcStart
              << ",\"qpc_end\":" << s.qpcEnd << '}';
}
void anchorJson(std::string_view label) {
    const auto before = pd_measure::qpc();
    FILETIME utc{};
    GetSystemTimePreciseAsFileTime(&utc);
    const auto after = pd_measure::qpc();
    const auto value = (static_cast<std::uint64_t>(utc.dwHighDateTime) << 32) | utc.dwLowDateTime;
    std::cout << "PD_QPC_ANCHOR {\"label\":\"" << label << "\",\"pid\":" << GetCurrentProcessId()
              << ",\"tid\":" << GetCurrentThreadId()
              << ",\"qpc_frequency\":" << pd_measure::qpcFrequency() << ",\"qpc_before\":" << before
              << ",\"qpc_after\":" << after << ",\"utc_filetime_100ns\":" << value << "}\n";
}
void row(std::string_view kind, std::size_t group, bool active, std::size_t operations,
         const pd_measure::Sample& sample) {
    std::cout << "PD_V2 {\"kind\":\"" << kind << "\",\"group\":" << group
              << ",\"active\":" << (active ? "true" : "false") << ",\"operations\":" << operations
              << ",\"sample\":";
    sampleJson(sample);
    std::cout << "}\n";
}
} // namespace

TEST_CASE("PD interleaved scheduling measurement", "[.][pd][method-v2]") {
    const auto profile = test_support::environmentVariable("CUEXIS_PD_PROFILE");
    const auto phase = test_support::environmentVariable("CUEXIS_PD_V2_PHASE");
    REQUIRE((profile == "legacy" || profile == "rpa"));
    REQUIRE((phase == "pilot" || phase == "run"));
    const bool realtime = profile == "rpa", pilot = phase == "pilot";
    const auto seed = std::stoul(test_support::environmentVariable("CUEXIS_PD_V2_SEED"));
    const auto reads = std::stoull(test_support::environmentVariable("CUEXIS_PD_V2_READS"));
    REQUIRE(reads >= 4096);
    REQUIRE(reads <= 4194304);
    REQUIRE((reads & (reads - 1)) == 0);
    internal::pd::setRecording(false);
    std::cout << std::setprecision(17);
    anchorJson("begin");

    // Calibrate the observable accounting step; FILETIME units are not accuracy.
    std::vector<double> quanta;
    auto prior = pd_measure::counters();
    const auto calibrationStart = Clock::now();
    while (elapsed(calibrationStart) < 250000) {
        const auto next = pd_measure::counters();
        if (next.cpu100ns > prior.cpu100ns)
            quanta.push_back(static_cast<double>(next.cpu100ns - prior.cpu100ns) / 10.0);
        prior = next;
    }
    REQUIRE_FALSE(quanta.empty());
    const auto emptyStart = Clock::now();
    for (unsigned i = 0; i < 1000; ++i)
        static_cast<void>(pd_measure::measure([] {}));
    const auto envelope = elapsed(emptyStart) / 1000;
    const auto waiting =
        pd_measure::measure([] { std::this_thread::sleep_for(std::chrono::milliseconds{100}); });
    REQUIRE(waiting.wallUs >= 75000);
    REQUIRE(waiting.cpuUs < waiting.wallUs / 2);
    std::cout << "PD_V2_CLOCK {\"cpu_accounting_steps_us\":";
    numbers(quanta);
    std::cout << ",\"telemetry_envelope_us\":" << envelope << ",\"deliberate_wait\":";
    sampleJson(waiting);
    std::cout << "}\n";

    auto fixture = configuration(realtime);
    const auto packed = fixture.packed();
    const auto content = take(GameplayContent::fromPacked(packed, fixture.config));
    auto warm = prepare(content, realtime);
    const auto golden = trace(*warm, realtime, false);
    warm.reset();
    // Symmetric quartets cancel first-order temporal drift in paired estimates.
    // Each operation uses a fresh session and the same fixed trajectory.
    std::vector<bool> orientations =
        pilot ? std::vector<bool>{false, true} : std::vector<bool>{false, true, true, false};
    std::mt19937 random{static_cast<std::uint32_t>(seed)};
    std::shuffle(orientations.begin(), orientations.end(), random);
    std::unique_ptr<PlaybackSession> last;
    for (std::size_t group = 0; group < orientations.size(); ++group) {
        for (unsigned position = 0; position < 4; ++position) {
            const bool active = ((position == 1 || position == 2) != orientations[group]);
            auto session = prepare(content, realtime);
            pd_measure::Sample sample;
            const auto actual = trace(*session, realtime, active && !pilot, &sample);
            REQUIRE(actual.digest == golden.digest);
            REQUIRE(actual.replay == golden.replay);
            REQUIRE(actual.snapshot == golden.snapshot);
            row(pilot ? "trace_sham" : "trace", group, active, measuredSteps, sample);
            last = std::move(session);
        }
    }
    internal::pd::setRecording(false);
    FrameSnapshot snapshot;
    take(last->extractFrame({1280, 720}, snapshot));
    const RuntimeFrame frame{realtime ? 10.88 : 10880, realtime ? 0.01 : 10, 0};
    const auto readDigest = take(computeFrameDigest(frame, snapshot)).value;
    const auto blocks = pilot ? 3U : 8U;
    for (unsigned block = 0; block < blocks; ++block) {
        for (unsigned position = 0; position < 2; ++position) {
            const bool large = ((block + position + seed) % 2) != 0;
            const auto count = large ? reads : 4096;
            const auto sample = pd_measure::measure([&] {
                for (std::size_t i = 0; i < count; ++i)
                    take(last->extractFrame({1280, 720}, snapshot));
            });
            REQUIRE(take(computeFrameDigest(frame, snapshot)).value == readDigest);
            row(large ? "read_long" : "read_short", block, false, count, sample);
        }
    }
    REQUIRE(core::detail::sha256Hex(take(take(last->archiveGameplay()).toBytes())) ==
            golden.replay);
    REQUIRE(core::detail::sha256Hex(take(take(last->snapshotGameplay()).toBytes())) ==
            golden.snapshot);
    std::cout << "PD_V2_ID {\"profile\":\"" << profile << "\",\"phase\":\"" << phase
              << "\",\"seed\":" << seed << ",\"reads\":" << reads << ",\"digest\":" << golden.digest
              << ",\"replay_sha256\":\"" << golden.replay << "\",\"snapshot_sha256\":\""
              << golden.snapshot << "\",\"packed_sha256\":\"" << core::detail::sha256Hex(packed)
              << "\"}\n";
    anchorJson("end");
}
#endif

#if defined(_WIN32) && !defined(CUEXIS_PD_ALLOCATION_OBSERVER) &&                                  \
    !defined(CUEXIS_PD_METRICS_TEST_ONLY)
TEST_CASE("PD machine CPU warmup", "[.][pd][formal-warmup]") {
    const auto finish = pd_measure::qpc() + 30 * pd_measure::qpcFrequency();
    std::uint64_t checksum = 1;
    do {
        for (unsigned i = 0; i < 16384; ++i) {
            checksum = checksum * 6364136223846793005ULL + 1442695040888963407ULL;
            std::atomic_signal_fence(std::memory_order_seq_cst);
        }
    } while (pd_measure::qpc() < finish);
    std::cout << "PD_MACHINE_WARMUP {\"seconds\":30,\"checksum\":" << checksum << "}\n";
}

TEST_CASE("PD uninstrumented validated fixed workload", "[.][pd][formal-baseline]") {
    const auto profile = test_support::environmentVariable("CUEXIS_PD_PROFILE");
    const auto phase = test_support::environmentVariable("CUEXIS_PD_FORMAL_PHASE");
    REQUIRE((profile == "legacy" || profile == "rpa"));
    REQUIRE((phase == "pilot" || phase == "formal"));
    const bool realtime = profile == "rpa";
    const auto reads = std::stoull(test_support::environmentVariable("CUEXIS_PD_V2_READS"));
    REQUIRE(reads >= 4096);
    REQUIRE(reads <= 4194304);
    REQUIRE((reads & (reads - 1)) == 0);
    const auto blocks = phase == "pilot" ? 3U : 8U;
    std::cout << std::setprecision(17);
    pd_measure::environmentJson("begin");
    anchorJson("begin");
    auto fixture = configuration(realtime);
    const auto packed = fixture.packed();
    const auto content = take(GameplayContent::fromPacked(packed, fixture.config));
    auto warm = prepare(content, realtime);
    const auto golden = trace(*warm, realtime, false);
    warm.reset();
    std::unique_ptr<PlaybackSession> last;
    for (unsigned i = 0; i < blocks; ++i) {
        auto session = prepare(content, realtime);
        pd_measure::Sample sample;
        const auto actual = trace(*session, realtime, false, &sample);
        REQUIRE(actual.digest == golden.digest);
        REQUIRE(actual.replay == golden.replay);
        REQUIRE(actual.snapshot == golden.snapshot);
        row("validated_trace", i, false, measuredSteps, sample);
        last = std::move(session);
    }
    for (unsigned i = 0; i < blocks; ++i) {
        auto session = prepare(content, realtime);
        const auto sample = playbackOnly(*session, realtime);
        auto validation = prepare(content, realtime);
        const auto actual = trace(*validation, realtime, false);
        REQUIRE(actual.digest == golden.digest);
        REQUIRE(actual.replay == golden.replay);
        REQUIRE(actual.snapshot == golden.snapshot);
        row("playback_only", i, false, measuredSteps, sample);
    }
    FrameSnapshot snapshot;
    take(last->extractFrame({1280, 720}, snapshot));
    const RuntimeFrame frame{realtime ? 10.88 : 10880, realtime ? 0.01 : 10, 0};
    const auto readDigest = take(computeFrameDigest(frame, snapshot)).value;
    for (unsigned i = 0; i < blocks; ++i) {
        const auto sample = pd_measure::measure([&] {
            for (std::size_t j = 0; j < reads; ++j)
                take(last->extractFrame({1280, 720}, snapshot));
        });
        REQUIRE(take(computeFrameDigest(frame, snapshot)).value == readDigest);
        row("read_long", i, false, reads, sample);
    }
    std::cout << "PD_FORMAL_ID {\"profile\":\"" << profile
              << "\",\"metrics_compile_enabled\":false,\"allocator\":\"normal\",\"trace_includes_"
                 "validation\":true,\"digest\":"
              << golden.digest << ",\"replay_sha256\":\"" << golden.replay
              << "\",\"snapshot_sha256\":\"" << golden.snapshot << "\",\"packed_sha256\":\""
              << core::detail::sha256Hex(packed) << "\"}\n";
    anchorJson("end");
    pd_measure::environmentJson("end");
}
#endif

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

#if defined(CUEXIS_PD_ALLOCATION_OBSERVER)
TEST_CASE("PD4 journal charge covers the exact owning error tree allocations",
          "[pd][pd4][allocation]") {
    using namespace cuexis::playback::detail;
    HostContext c;
    c.sessionScope = c.clockSegment = c.runEpoch = c.controlSequence = c.resourceGeneration =
        c.rendererGeneration = 1;
    auto made = FrameObservationLedger::create(c, 1, 4, 4);
    REQUIRE(made);
    auto& ledger = *made;
    FrameEnvelope frame;
    frame.context = c;
    frame.projectionScope = 1;
    frame.frameId = 1;
    REQUIRE(ledger.publish(frame));
    REQUIRE(ledger.takeLatest());
    std::optional<core::Error> chain;
    for (int i = 0; i < 16; ++i) {
        core::Error error{"x", "y"};
        error.withContext("k", "v");
        if (chain)
            error.withCause(std::move(*chain));
        chain = std::move(error);
    }
    PresentReceipt receipt{c, 1, 1, PresentStatus::unknown, 1, std::move(chain)};
    const auto beforeBytes = ledger.journalBytes();
    const auto beforeLive = pd_allocation::live.load();
    auto observed = ledger.observe(receipt);
    const auto actual = pd_allocation::live.load() - beforeLive;
    const auto charged = ledger.journalBytes() - beforeBytes;
    REQUIRE(observed);
    REQUIRE(actual > 0);
    CHECK(charged >= actual);
    std::cout << "PD4_RECEIPT_STORAGE {\"requested_live_bytes\":" << actual
              << ",\"charged_bytes\":" << charged << "}\n";
    REQUIRE(ledger.observe({c, 1, 1, PresentStatus::succeeded, 2, {}}));
    CHECK(ledger.journalBytes() == beforeBytes);
}
#endif

#include "read_scaling_probe.hpp"
