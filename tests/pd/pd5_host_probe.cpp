#include "../../app/player/src/execution_host.hpp"
#include "../rpa/execution_fixture.hpp"
#include "../support/environment.hpp"
#include "measurement_clocks.hpp"
#include "measurement_environment.hpp"
#include <cuexis_internal/presentation_metrics.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <cuexis/playback/frame_digest.hpp>
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>

#if defined(CUEXIS_PD5_DEVICE)
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
            cpuSubmit_.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
            return result;
        }
        auto result = backend_.submit(s, d);
        if (record)
            cpuSubmit_.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
        return result;
    }
    auto present() -> core::Result<void> override {
        const auto start = Clock::now();
        auto result = backend_.present();
        if (record)
            cpuPresent_.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
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
TEST_CASE("PD5 allocation observer tracks transient and cross-thread ownership", "[pd][pd5][memory]") {
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
              << "\",\"smoke\":"
              << (smoke ? "true" : "false") << ",\"gpu_timer\":" << (timing ? "true" : "false")
              << ",\"frames\":" << frames << ",\"warmup_frames\":" << warmup
              << ",\"cycles\":" << cycles << ",\"rate\":" << rate
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
#endif
