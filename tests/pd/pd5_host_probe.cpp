#include "../../app/player/src/execution_host.hpp"
#include "../rpa/execution_fixture.hpp"
#include "../support/environment.hpp"
#include "measurement_clocks.hpp"
#include "measurement_environment.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <cuexis/judgement/judgement_session.hpp>
#include <cuexis/judgement/recovery.hpp>
#include <cuexis/judgement/recovery_codec.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>

#if defined(CUEXIS_PD5_DEVICE)
// The playback internals need the judgement internal tree; only the device probe target carries
// that include path, and only the device build compiles the seek class fixtures that use it.
#include "../../engine/judgement/src/execution_testing.hpp"
#include "../../engine/playback/src/gameplay_internal.hpp"
#include "../../engine/render_opengl/internal/cuexis_internal/gpu_timer.hpp"
#include <SDL3/SDL.h>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>
#endif

#if defined(_WIN32)
namespace {
using namespace cuexis;
using namespace playback;
using namespace playback::detail;
using namespace presentation_renderer;
using Clock = std::chrono::steady_clock;
template <class T> auto pd5Take(core::Result<T> value) -> T {
    if (!value)
        throw std::runtime_error(std::string{value.error().code()} + ": " +
                                 std::string{value.error().message()});
    if constexpr (!std::is_void_v<T>)
        return std::move(*value);
}
auto pd5Now() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch())
        .count();
}
auto pd5Count(const char* key, unsigned fallback, unsigned limit) -> unsigned {
    const auto text = test_support::environmentVariable(key);
    if (text.empty())
        return fallback;
    std::size_t used{};
    const auto value = std::stoul(text, &used);
    if (used != text.size() || value == 0 || value > limit)
        throw std::runtime_error("Invalid finite PD5 count");
    return static_cast<unsigned>(value);
}
std::map<std::string, unsigned> pd5Counts;
void pd5Sample(const char* metric, double elapsed, std::int64_t start = 0,
               std::int64_t confirmed = 0, std::int64_t end = 0) {
    std::cout << "PD5_SAMPLE {\"metric\":\"" << metric << "\",\"ordinal\":" << pd5Counts[metric]++
              << ",\"elapsed_us\":" << elapsed << ",\"complete\":true";
    if (start)
        std::cout << ",\"start_us\":" << start << ",\"confirmed_us\":" << confirmed
                  << ",\"first_present_us\":" << end;
    std::cout << "}\n";
}
#if defined(CUEXIS_PD5_DEVICE)
class Pd5GpuRenderer final : public IPresentationRenderer {
  public:
    explicit Pd5GpuRenderer(render_opengl::OpenGlBackend backend, bool timing)
        : backend_(std::move(backend)), timing_(timing) {
        cpuSubmit_.reserve(8);
        cpuPresent_.reserve(8);
        if (timing_)
            pd5Take(timer_.initialize());
    }
    bool record{};
    auto capabilities() const noexcept -> const PresentationCapabilities& override {
        return backend_.capabilities();
    }
    auto identity() const noexcept -> RendererIdentity override {
        return backend_.identity();
    }
    auto prepare(PreparedPlayback& p, const PresentationRequest& r)
        -> core::Result<PreparedPresentation> override {
        return backend_.prepare(p, r);
    }
    auto prepareOwned(const PresentationCandidateInput& p)
        -> core::Result<PreparedPresentation> override {
        return backend_.prepareOwned(p);
    }
    auto accepts(const PreparedPresentation& p) const -> core::Result<void> override {
        return backend_.accepts(p);
    }
    void activate(PreparedPresentation&& p) noexcept override {
        backend_.activate(std::move(p));
    }
    void discard(PreparedPresentation&& p) noexcept override {
        backend_.discard(std::move(p));
    }
    bool hasActivePresentation() const noexcept override {
        return backend_.hasActivePresentation();
    }
    auto submit(const FrameSnapshot& s, const render::RenderScene* d)
        -> core::Result<DrawSummary> override {
        const auto start = Clock::now();
        if (timing_ && record) {
            auto begun = timer_.begin(ordinal_++);
            if (!begun)
                return core::unexpected(begun.error());
            auto result = backend_.submit(s, d);
            auto ended = timer_.end();
            if (!ended)
                return core::unexpected(ended.error());
            cpuSubmit_.push_back(
                std::chrono::duration<double, std::micro>(Clock::now() - start).count());
            return result;
        }
        auto result = backend_.submit(s, d);
        if (record)
            cpuSubmit_.push_back(
                std::chrono::duration<double, std::micro>(Clock::now() - start).count());
        return result;
    }
    auto present() -> core::Result<void> override {
        const auto start = Clock::now();
        auto result = backend_.present();
        if (record)
            cpuPresent_.push_back(
                std::chrono::duration<double, std::micro>(Clock::now() - start).count());
        return result;
    }
    auto resize(std::uint32_t w, std::uint32_t h) -> core::Result<void> override {
        return backend_.resize(w, h);
    }
    auto rebuild() -> core::Result<void> override {
        auto stopped = timer_.close();
        if (!stopped)
            return stopped;
        auto rebuilt = backend_.rebuild();
        if (!rebuilt || !timing_)
            return rebuilt;
        return timer_.initialize();
    }
    auto close() -> core::Result<void> override {
        auto stopped = timer_.close();
        if (!stopped)
            return stopped;
        return backend_.close();
    }
    void collect(bool drain = false) {
        for (const auto elapsed : cpuSubmit_)
            pd5Sample("cpu_submit", elapsed);
        cpuSubmit_.clear();
        for (const auto elapsed : cpuPresent_)
            pd5Sample("present_call", elapsed);
        cpuPresent_.clear();
        const auto deadline = Clock::now() + std::chrono::seconds{5};
        while (timer_.pending()) {
            auto sample = pd5Take(timer_.poll());
            if (sample) {
                REQUIRE(sample->ordinal == gpuRead_++);
                pd5Sample("gpu_render", static_cast<double>(sample->nanoseconds) / 1000.);
            } else if (!drain) {
                break;
            } else {
                REQUIRE(Clock::now() < deadline);
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        }
    }

  private:
    void detachCandidate(std::uint64_t, std::uint64_t,
                         const PresentationCandidateToken&) noexcept override {
        std::terminate();
    }
    // Timer destruction precedes backend/context destruction.
    render_opengl::OpenGlBackend backend_;
    render_opengl::test_only::GpuTimer timer_;
    bool timing_{};
    std::vector<double> cpuSubmit_, cpuPresent_;
    std::uint64_t ordinal_{}, gpuRead_{};
};
#endif

#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
TEST_CASE("PD5 allocation observer tracks transient and cross-thread ownership",
          "[pd][pd5][memory]") {
    const auto before = cuexis::internal::pd::memory::snapshot();
    auto* block = ::operator new(12345, std::align_val_t{256});
    REQUIRE(reinterpret_cast<std::uintptr_t>(block) % 256 == 0);
    const auto during = cuexis::internal::pd::memory::snapshot();
    REQUIRE(during.liveCount == before.liveCount + 1);
    REQUIRE(during.requestedBytes == before.requestedBytes + 12345);
    REQUIRE(during.storageBytes > before.storageBytes + 12345);
    std::thread release{[&] { ::operator delete(block, std::align_val_t{256}); }};
    release.join();
    const auto after = cuexis::internal::pd::memory::snapshot();
    REQUIRE(after.requestedBytes == before.requestedBytes);
    REQUIRE(after.liveCount == before.liveCount);
    REQUIRE(after.peakRequestedBytes >= during.requestedBytes);
    REQUIRE_FALSE(after.exceeded);
    auto* zero = ::operator new[](0, std::nothrow);
    REQUIRE(zero);
    ::operator delete[](zero, std::nothrow);
}
#endif
} // namespace

TEST_CASE("PD5 scripted host timing probe", "[.][pd][pd5][timing-probe]") {
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    {
        // Attribution only: these are compile-time instrumented stage timings, whose overhead is
        // not subtracted and whose numbers the plan forbids accepting as the budget.
        namespace pd = cuexis::internal::pd;
        const auto& metrics = pd::threadMetrics();
        const auto report = [&](pd::Stage stage, const char* name) {
            const auto& samples = metrics.stages[static_cast<unsigned>(stage)];
            if (samples.count == 0) {
                std::cout << "PD5_STAGE {\"stage\":\"" << name << "\",\"count\":0}\n";
                return;
            }
            std::vector<double> values(samples.exclusiveUs.begin(),
                                       samples.exclusiveUs.begin() + samples.count);
            std::sort(values.begin(), values.end());
            const auto at = [&](double quantile) {
                const auto index = std::min(
                    values.size() - 1, static_cast<std::size_t>(quantile * (values.size() - 1)));
                return values[index];
            };
            std::cout << "PD5_STAGE {\"stage\":\"" << name << "\",\"count\":" << samples.count
                      << ",\"p50_us\":" << at(0.50) << ",\"p95_us\":" << at(0.95)
                      << ",\"p99_us\":" << at(0.99) << ",\"max_us\":" << values.back() << "}\n";
        };
        report(pd::Stage::judgement, "judgement");
        report(pd::Stage::projection, "projection");
        report(pd::Stage::runtime, "runtime");
        report(pd::Stage::snapshot, "snapshot");
        report(pd::Stage::queue, "queue");
    }
#endif
    internal::pd::setProcessRecording(false);
    pd5Counts.clear();
    std::cout << std::setprecision(17);
    const auto mode = test_support::environmentVariable("CUEXIS_PD5_MODE");
    REQUIRE((mode == "controls" || mode == "stream"));
    const auto frames = pd5Count("CUEXIS_PD5_FRAMES", 512, 1000000);
    const auto warmup = pd5Count("CUEXIS_PD5_WARMUP", 32, 100000);
    const auto cycles = pd5Count("CUEXIS_PD5_CYCLES", 10, 100);
    const auto rate = pd5Count("CUEXIS_PD5_RATE", 240, 4000);
    const bool smoke = test_support::environmentVariable("CUEXIS_PD5_SMOKE") == "1";
    const bool timing = test_support::environmentVariable("CUEXIS_PD5_GPU_TIMER") == "1";
#if defined(CUEXIS_PD5_DEVICE)
    auto runtime = pd5Take(platform_sdl::SdlRuntime::create());
    render_opengl::OpenGlConfig glConfig;
    glConfig.vsync = true;
    std::optional<render_opengl::OpenGlContextConfiguration> config{
        pd5Take(render_opengl::configureOpenGlContext(runtime, glConfig))};
    platform_sdl::WindowConfig windowConfig;
    windowConfig.title = "Cuexis PD5 measurement tool";
    auto window = pd5Take(platform_sdl::SdlWindow::create(runtime, windowConfig));
    if (smoke)
        REQUIRE(SDL_HideWindow(static_cast<SDL_Window*>(window.lease().nativeHandle())));
    bool firstBackend = true;
#else
    REQUIRE_FALSE(timing);
#endif
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    std::unique_ptr<IPresentationRenderer> renderer;
    std::unique_ptr<player::ExecutionHost> host;
    std::int64_t logicalTick{};
    std::uint64_t sequence{};
    std::optional<std::uint64_t> lastSampleSegment;
    std::int64_t lastSampleTick{};
    auto clock = [&](const HostContext& c) -> core::Result<ClockObservation> {
        ClockObservation o{};
        o.monotonicDomain = "pd5.steady.us";
        o.segment = c.clockSegment;
        o.publicationSequence = ++sequence;
        o.sampleMonotonicUs = pd5Now();
        o.validUntilMonotonicUs = o.sampleMonotonicUs + 1000000;
        logicalTick += 7000;
        o.presented = ClockFramePosition{logicalTick, 1000000};
        o.maximumPresented = o.presented;
        o.playback = ClockPlaybackState::Playing;
        o.quality = ClockQuality::Exact;
        return o;
    };
    auto load = [&] {
#if defined(CUEXIS_PD5_DEVICE)
        if (!firstBackend)
            config.emplace(pd5Take(render_opengl::configureOpenGlContext(runtime, glConfig)));
        firstBackend = false;
        auto backend = pd5Take(render_opengl::OpenGlBackend::create(window, std::move(*config)));
        renderer = std::make_unique<Pd5GpuRenderer>(std::move(backend), timing);
#else
        renderer = std::make_unique<TestPresentationRenderer>();
#endif
        player::ExecutionHostOptions options;
        options.session = {64, 16, 128, 32, 4, 4, true, {}};
        options.session.observation.bytes = 16384;
        options.session.observation.tombstones = 2;
        options.session.observation.coverageIntervals = 2;
        options.session.observation.retentionUs = 1000000;
        options.session.observation.receiptDeadlineUs = 5000000;
        options.session.observation.members = 4096;
        options.session.delivery = {8, 8, 64U * 1024U * 1024U, 32, 64};
        options.session.maxFrameMembers = 4096;
        options.session.receiptCapacity = 128;
        options.audio.testOnly = true;
        options.hostCapacity = 16;
        options.captureCapacity = 64;
        options.captureReserve = 16;
        options.replyDeadline = std::chrono::seconds{5};
        options.viewport = {1280, 720};
        options.realtime =
            SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "pd5.steady.us"};
        logicalTick = 0;
        sequence = 0;
        lastSampleSegment.reset();
        host = pd5Take(player::ExecutionHost::create(*renderer, options, {}));
        pd5Take(host->load(test_support::presentation(), fixture.content()));
    };
    std::uint64_t digest = 14695981039346656037ULL;
    std::uint64_t lastPresentedQpc{};
    auto draw = [&](bool frozen) {
        if (!frozen) {
            const auto decision = pd5Take(host->advance(pd5Take(clock(host->context()))));
            REQUIRE(decision.disposition == GameplayAdvanceDisposition::Executed);
            REQUIRE(decision.request.horizon.value == logicalTick);
        }
        const auto tick = host->pausedCut().value_or(logicalTick);
        const auto delta = !frozen && lastSampleSegment == host->context().clockSegment
                               ? static_cast<double>(tick - lastSampleTick) / 1000.
                               : 0.;
        auto frame = pd5Take(host->sample(
            tick, {static_cast<double>(tick) / 1000., delta, host->context().clockSegment},
            frozen));
        lastSampleSegment = host->context().clockSegment;
        lastSampleTick = tick;
        const auto receipt = pd5Take(host->present(frame));
        lastPresentedQpc = pd_measure::qpc();
        const auto presentedAt = pd5Now();
        REQUIRE(receipt.status == PresentStatus::succeeded);
        digest = (digest ^
                  pd5Take(computeFrameDigest(*frame.runtimeFrame, frame.snapshotView())).value) *
                 1099511628211ULL;
        return presentedAt;
    };
    pd_measure::environmentJson("begin");
    internal::pd::memory::checkpoint("before-load");
    const auto loadedAt = pd5Now();
    load();
    const auto loadConfirmed = pd5Now();
    const auto firstFrame = draw(false);
    internal::pd::memory::checkpoint("active");
    // Host-local load is diagnostic; process launch -> first frame is measured by the runner.
    pd5Sample("load_host", static_cast<double>(firstFrame - loadedAt), loadedAt, loadConfirmed,
              firstFrame);
    std::cout << "PD5_FIRST_FRAME {\"qpc\":" << lastPresentedQpc
              << ",\"frequency\":" << pd_measure::qpcFrequency() << "}\n";
    if (mode == "controls") {
        internal::pd::setProcessRecording(true);
        auto control = [&](const char* metric, auto&& action, bool frozen) {
            const auto start = pd5Now();
            action();
            const auto confirmed = pd5Now();
            const auto end = draw(frozen);
            pd5Sample(metric, static_cast<double>(end - start), start, confirmed, end);
        };
        for (unsigned i = 0; i < cycles; ++i) {
            control("pause", [&] { pd5Take(host->pause(clock)); }, true);
            control("resume", [&] { pd5Take(host->resume(clock)); }, false);
            control("pause", [&] { pd5Take(host->pause(clock)); }, true);
            control(
                "reset_paused",
                [&] {
                    pd5Take(host->close());
                    host.reset();
                    renderer.reset();
                    load();
                    draw(false);
                    pd5Take(host->pause(clock));
                },
                true);
            control("resume", [&] { pd5Take(host->resume(clock)); }, false);
            control(
                "reset_running",
                [&] {
                    pd5Take(host->close());
                    host.reset();
                    renderer.reset();
                    load();
                },
                false);
            control(
                "resize",
                [&] {
                    pd5Take(
                        host->resize(i % 2 ? FrameViewport{1280, 720} : FrameViewport{960, 540}));
                },
                false);
        }
    } else {
        auto anchor = Clock::now();
        for (unsigned i = 0; i < warmup + frames; ++i) {
            internal::pd::setProcessRecording(i >= warmup);
            if (!smoke)
                std::this_thread::sleep_until(
                    anchor +
                    std::chrono::nanoseconds{static_cast<std::int64_t>(i) * 1000000000LL / rate});
#if defined(CUEXIS_PD5_DEVICE)
            auto& timed = static_cast<Pd5GpuRenderer&>(*renderer);
            timed.record = i >= warmup;
            timed.collect();
            SDL_PumpEvents();
#endif
            const auto started = pd5Now();
            const auto decision = pd5Take(host->advance(pd5Take(clock(host->context()))));
            REQUIRE(decision.disposition == GameplayAdvanceDisposition::Executed);
            REQUIRE(decision.request.horizon.value == logicalTick);
            const auto delta = static_cast<double>(logicalTick - lastSampleTick) / 1000.;
            auto frame = pd5Take(host->sample(
                logicalTick,
                {static_cast<double>(logicalTick) / 1000., delta, host->context().clockSegment},
                false));
            lastSampleTick = logicalTick;
            const auto ready = pd5Now();
            const auto receipt = pd5Take(host->present(frame));
            const auto presented = pd5Now();
            REQUIRE(receipt.status == PresentStatus::succeeded);
            digest =
                (digest ^
                 pd5Take(computeFrameDigest(*frame.runtimeFrame, frame.snapshotView())).value) *
                1099511628211ULL;
            if (i >= warmup) {
                pd5Sample("request_to_ready", static_cast<double>(ready - started));
                pd5Sample("request_to_present_return", static_cast<double>(presented - started));
            }
            REQUIRE(host->storageMetrics().liveCount <= 2);
        }
#if defined(CUEXIS_PD5_DEVICE)
        static_cast<Pd5GpuRenderer&>(*renderer).collect(true);
#endif
    }
    internal::pd::setProcessRecording(false);
    const auto result = pd5Take(host->queryGameplay());
    const auto replay = pd5Take(host->evaluateReplay(pd5Take(host->archive())));
    REQUIRE(replay.evidenceValid);
    REQUIRE(pd5Take(result.sameResult(replay.result)));
    const auto storageBeforeClose = host->storageMetrics();
    const auto history = pd5Take(host->observationInfo());
    REQUIRE(history.retainedAttempts <= 4);
    REQUIRE(history.peakBytes <= 16384);
    const auto closeStart = pd5Now();
    internal::pd::memory::checkpoint("before-close");
    pd5Take(host->close());
    const auto storageAfterClose = host->storageMetrics();
    REQUIRE(storageAfterClose.liveCount == 0);
    REQUIRE(storageAfterClose.liveBytes == 0);
    host.reset();
    renderer.reset();
    internal::pd::memory::checkpoint("after-close");
    pd5Sample("close", static_cast<double>(pd5Now() - closeStart));
    pd_measure::environmentJson("end");
    std::cout << "PD5_ID {\"schema\":\"pd5.probe.v1\",\"mode\":\"" << mode
              << "\",\"metrics_compile_enabled\":"
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
              << "true"
#else
              << "false"
#endif
              << ",\"allocator\":\""
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
              << "lifetime-observer"
#else
              << "normal"
#endif
              << "\",\"smoke\":" << (smoke ? "true" : "false")
              << ",\"gpu_timer\":" << (timing ? "true" : "false") << ",\"frames\":" << frames
              << ",\"warmup_frames\":" << warmup << ",\"cycles\":" << cycles << ",\"rate\":" << rate
              << ",\"source_step_ticks\":7000,\"source_tick_rate\":1000000"
              << ",\"payload_peak_count\":" << storageBeforeClose.peakCount
              << ",\"payload_peak_bytes\":" << storageBeforeClose.peakBytes
              << ",\"payload_after_close\":" << storageAfterClose.liveCount
              << ",\"journal_peak_bytes\":" << history.peakBytes
              << ",\"audio\":false,\"closed\":true,\"replay_equal\":true,\"digest\":" << digest
              << ",\"counts\":{";
    bool comma = false;
    for (const auto& [metric, count] : pd5Counts) {
        if (comma)
            std::cout << ',';
        comma = true;
        std::cout << '"' << metric << "\":" << count;
    }
    std::cout << "}}\n";
}

#if defined(CUEXIS_PD5_DEVICE)
// Seek gate probe: the full host seek on the real OpenGL backend. The functional matrix measures
// the same operations on the CPU test renderer, which is why its readings cannot stand for the
// gate.
//
// CUEXIS_PD5_SEEK_CLASS selects one of the pre-declared Seek classes. The hot and cold classes
// are carried by the record-count axis (a fresh process is the cold class); this probe adds the
// three classes that need a different fixture:
//   natural  - a certificate-carrying checkpoint minted by the session itself, first use
//   external - an archive that arrives as bytes, so the request pays first certification
//   fallback - a hint that can never be legal for the target, so the request pays origin replay
TEST_CASE("PD5 scripted seek timing probe", "[.][pd][pd5][seek-gate]") {
    std::cout << std::setprecision(17);
    const auto recordCount = pd5Count("CUEXIS_PD5_SEEK_RECORDS", 129, 20000);
    const auto seekCount = pd5Count("CUEXIS_PD5_SEEK_COUNT", 16, 4096);
    const auto seekClass = [] {
        const auto text = test_support::environmentVariable("CUEXIS_PD5_SEEK_CLASS");
        return text.empty() ? std::string{"hot"} : text;
    }();
    if (seekClass != "hot" && seekClass != "natural" && seekClass != "external" &&
        seekClass != "fallback")
        throw std::runtime_error("Invalid CUEXIS_PD5_SEEK_CLASS");
    auto runtime = pd5Take(platform_sdl::SdlRuntime::create());
    render_opengl::OpenGlConfig glConfig;
    glConfig.vsync = false;
    std::optional<render_opengl::OpenGlContextConfiguration> config{
        pd5Take(render_opengl::configureOpenGlContext(runtime, glConfig))};
    platform_sdl::WindowConfig windowConfig;
    windowConfig.title = "Cuexis PD5 seek probe";
    auto window = pd5Take(platform_sdl::SdlWindow::create(runtime, windowConfig));
    REQUIRE(SDL_HideWindow(static_cast<SDL_Window*>(window.lease().nativeHandle())));
    Pd5GpuRenderer renderer{
        pd5Take(render_opengl::OpenGlBackend::create(window, std::move(*config))), false};
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    player::ExecutionHostOptions settings;
    settings.session = {64, 16, 128, 32, 4, 4, true, {}};
    settings.session.observation.bytes = 16384;
    settings.session.observation.tombstones = 2;
    settings.session.observation.coverageIntervals = 2;
    settings.session.observation.retentionUs = 1000000;
    settings.session.observation.receiptDeadlineUs = 5000000;
    settings.session.observation.members = 4096;
    settings.session.delivery = {8, 8, 64U * 1024U * 1024U, 32, 64};
    settings.session.maxFrameMembers = 4096;
    settings.session.receiptCapacity = 128;
    settings.audio.testOnly = true;
    settings.hostCapacity = 16;
    settings.captureCapacity = 64;
    settings.captureReserve = 16;
    settings.replyDeadline = std::chrono::seconds{5};
    settings.viewport = {1280, 720};
    // Repeated seeks in one session each take a control reservation, so the probe sizes the host
    // for the whole seek loop instead of the single seek the functional matrix performs.
    settings.hostCapacity = 256;
    settings.captureCapacity = 256;
    settings.captureReserve = 64;
    settings.seekIdentity = player::SeekIdentityPolicy::owningSource;
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "pd5.seek"};
    std::int64_t logicalTick{};
    std::uint64_t sequence{};
    auto clock = [&](const HostContext& context) -> core::Result<ClockObservation> {
        ClockObservation observation{};
        observation.monotonicDomain = "pd5.seek";
        observation.segment = context.clockSegment;
        observation.publicationSequence = ++sequence;
        observation.sampleMonotonicUs = pd5Now();
        observation.validUntilMonotonicUs = observation.sampleMonotonicUs + 1000000;
        logicalTick += 7000;
        observation.presented = ClockFramePosition{logicalTick, 1000000};
        observation.maximumPresented = observation.presented;
        observation.source = observation.presented;
        observation.playback = ClockPlaybackState::Playing;
        observation.quality = ClockQuality::Exact;
        return observation;
    };
    // Keep the owning content alive: the access view and every replay decode borrow from it.
    const auto content = fixture.content();
    auto host = pd5Take(player::ExecutionHost::create(renderer, settings, {}));
    pd5Take(host->load(test_support::presentation(), content));
    for (unsigned i = 0; i < recordCount; ++i) {
        pd5Take(host->advance(pd5Take(clock(host->context()))));
        auto frame = pd5Take(host->sample(
            logicalTick,
            {static_cast<double>(logicalTick) / 1000., 0, host->context().clockSegment}, false));
        pd5Take(host->present(frame));
    }
    auto archive = pd5Take(host->archive());
    const auto* basis = GameplayAccess::content(content);
    const auto* naturalSource = GameplayAccess::replay(archive);
    REQUIRE(basis);
    REQUIRE(naturalSource);
    // Fixture preparation happens before any request is timed: only consuming the certificate,
    // certifying an external source or replaying an unusable hint belongs inside the 20 ms gate.
    std::vector<GameplayCheckpoint> hints;
    std::optional<GameplayReplay> externalReplay;
    std::optional<GameplayReplay> foreignReplay;
    if (seekClass == "natural") {
        // A certified restore needs the checkpoint cut to end exactly on a record horizon, and the
        // fixture's first record sits at 7000. Every target at or after 8103 can consume it; the
        // first such request carries the first-use cost and the later ones reuse the carrier.
        auto minted = cuexis::judgement::JudgementSession::checkpoint(
            naturalSource->archive, cuexis::judgement::Tick{7000});
        REQUIRE(minted);
        hints.push_back(GameplayAccess::checkpoint(std::move(*minted)));
    }
    if (seekClass == "external") {
        const auto encoded = pd5Take(archive.toBytes());
        auto decoded =
            GameplayReplay::fromBytes(encoded, content, {encoded.size(), 10000, 10000000, true});
        REQUIRE(decoded);
        externalReplay = std::move(*decoded);
    }
    if (seekClass == "fallback") {
        // A checkpoint minted from an independent decoded archive can never bind this source, and
        // its horizon is the last target, so most requests also exceed it.
        const auto encoded = pd5Take(archive.toBytes());
        auto decoded =
            GameplayReplay::fromBytes(encoded, content, {encoded.size(), 10000, 10000000, true});
        REQUIRE(decoded);
        const auto* foreign = GameplayAccess::replay(*decoded);
        REQUIRE(foreign);
        const auto lastHorizon =
            cuexis::judgement::Tick{1103 + static_cast<std::int64_t>(seekCount - 1) * 7000};
        auto minted =
            cuexis::judgement::JudgementSession::checkpoint(foreign->archive, lastHorizon);
        REQUIRE(minted);
        hints.push_back(GameplayAccess::checkpoint(std::move(*minted)));
        foreignReplay = std::move(*decoded);
    }
    const auto& sought = externalReplay ? *externalReplay : archive;
    // Oracle: the unhinted origin path on the CPU, one per horizon. Neither host nor device work
    // is reused, so the comparison proves the measured request produced the same published state.
    auto oracle = cuexis::judgement::JudgementSession::create();
    REQUIRE(oracle);
    REQUIRE(oracle->configure(basis->configuration));
    REQUIRE(oracle->prepare(basis->capsule.gameplay));
    // Precondition of the class, measured once outside the timed region: a session-natural
    // certificate must actually be consumed, an unusable hint must actually be rejected. Without
    // this the class label would only describe the input, not the path the request took.
    const auto* preconditionSource = GameplayAccess::replay(sought);
    REQUIRE(preconditionSource);
    std::vector<cuexis::judgement::ReplayCheckpoint> judgementHints;
    for (const auto& hint : hints) {
        const auto* storage = GameplayAccess::checkpoint(hint);
        REQUIRE(storage);
        judgementHints.push_back(storage->checkpoint);
    }
    const auto preconditionHorizon = cuexis::judgement::Tick{seekClass == "natural" ? 8103 : 1103};
    const auto preconditionCut =
        cuexis::judgement::replayCutAt(preconditionSource->archive, preconditionHorizon);
    auto precondition = oracle->forkForSeek(preconditionSource->archive, preconditionHorizon,
                                            preconditionCut, judgementHints);
    REQUIRE(precondition);
    const auto certifiedRecords =
        cuexis::judgement::detail::KernelTestAccess::certifiedPrefixRecords(*precondition);
    if (seekClass == "natural")
        REQUIRE(certifiedRecords > 0);
    else
        REQUIRE(certifiedRecords == 0);
    std::size_t verified{};
    std::size_t mismatched{};
    pd_measure::environmentJson("seek-begin");
    std::cout << "PD5_SEEK_ID {\"schema\":\"pd5.seek.v1\",\"records\":" << recordCount
              << ",\"seeks\":" << seekCount << ",\"seek_class\":\"" << seekClass
              << "\",\"hints\":" << hints.size() << ",\"certified_records\":" << certifiedRecords
              << "}\n";
    for (unsigned index = 0; index < seekCount; ++index) {
        const auto horizon = 1103 + static_cast<std::int64_t>(index) * 7000;
        pd5Take(host->pause(clock));
        const auto started = pd5Now();
        auto cut = pd5Take(host->replayCut(sought, GameplayTick{horizon}));
        pd5Take(host->seek(sought, cut, GameplayTick{horizon}, clock, hints));
        const auto ready = pd5Now();
        auto frame = pd5Take(host->sample(
            horizon, {static_cast<double>(horizon) / 1000., 0, host->context().clockSegment},
            false));
        pd5Take(host->present(frame));
        const auto presented = pd5Now();
        const auto oracleCut = cuexis::judgement::replayCutAt(naturalSource->archive,
                                                              cuexis::judgement::Tick{horizon});
        auto expected = oracle->forkForSeek(naturalSource->archive,
                                            cuexis::judgement::Tick{horizon}, oracleCut);
        REQUIRE(expected);
        auto expectedSnapshot = expected->snapshot();
        REQUIRE(expectedSnapshot);
        auto expectedBytes = encodeSnapshot(*expectedSnapshot);
        REQUIRE(expectedBytes);
        auto actual = pd5Take(host->snapshotGameplay());
        auto actualBytes = pd5Take(actual.toBytes());
        const bool matches = actualBytes == *expectedBytes;
        ++verified;
        if (!matches)
            ++mismatched;
        std::cout << "PD5_SEEK {\"records\":" << recordCount << ",\"seek_class\":\"" << seekClass
                  << "\",\"index\":" << index << ",\"horizon\":" << horizon
                  << ",\"request_to_ready_us\":" << (ready - started)
                  << ",\"request_to_present_us\":" << (presented - started)
                  << ",\"state_matches_oracle\":" << (matches ? "true" : "false") << "}\n";
    }
    std::cout << "PD5_SEEK_VERIFY {\"records\":" << recordCount << ",\"seek_class\":\"" << seekClass
              << "\",\"verified\":" << verified << ",\"mismatched\":" << mismatched << "}\n";
    pd5Take(host->close());
}
#endif

#endif
