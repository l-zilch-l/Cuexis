#include "../../engine/render_opengl/internal/cuexis_internal/gpu_timer.hpp"
#include "../support/environment.hpp"
#include <SDL3/SDL.h>
#include <catch2/catch_test_macros.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>
#include <cuexis_internal/pixel_probe.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace cuexis;
using namespace playback;
using Clock = std::chrono::steady_clock;
template <class T> auto throughputTake(core::Result<T> result) -> T {
    if (!result)
        throw std::runtime_error(std::string{result.error().code()} + ": " +
                                 std::string{result.error().message()});
    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}
auto throughputCount(const char* key, unsigned fallback, unsigned limit) -> unsigned {
    const auto text = test_support::environmentVariable(key);
    if (text.empty())
        return fallback;
    std::size_t used{};
    const auto value = std::stoul(text, &used);
    if (used != text.size() || value == 0 || value > limit)
        throw std::runtime_error("Invalid finite GPU throughput count");
    return static_cast<unsigned>(value);
}
auto throughputSource(unsigned count, bool animated) -> PlaybackSource {
    const auto read = [](const char* name) {
        std::ifstream file{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                               "assets/projects/stage3_project/assets" / name,
                           std::ios::binary};
        if (!file)
            throw std::runtime_error("Missing GPU throughput resource");
        const std::string text{std::istreambuf_iterator<char>{file}, {}};
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    };
    auto provider = throughputTake(content::MemoryContentProvider::create(
        {{"main", "triangle.mesh.bin", read("meshes/triangle.mesh.bin")},
         {"main", "opaque.material.bin", read("materials/opaque.material.bin")}}));
    std::ostringstream chart;
    chart
        << R"({"format":"cuexis.chart","version":1,"chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
"timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
"camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,"pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,10]}},
"templates":[],"behaviors":[{"id":"move","type":"behavior.transform.keyframe","version":1,"tracks":[
{"property":"transform.position.z","keys":[{"beat":{"numerator":0,"denominator":1},"value":0},
{"beat":{"numerator":2400,"denominator":1},"value":1,"easing":"linear"}]}]}],"objects":[)";
    const auto columns = static_cast<unsigned>(std::ceil(std::sqrt(count)));
    const auto rows = (count + columns - 1) / columns;
    const auto center = std::min(count - 1, (rows / 2) * columns + columns / 2);
    for (unsigned i = 0; i < count; ++i) {
        if (i)
            chart << ',';
        // Keep a real mesh over the center pixel even for a small or incomplete grid.
        const auto cell = i == 0 ? center : i == center ? 0 : i;
        const auto x =
            (static_cast<int>(cell % columns) - static_cast<int>(center % columns)) * 0.09;
        const auto y =
            (static_cast<int>(cell / columns) - static_cast<int>(center / columns)) * 0.05;
        chart << "{\"id\":\"019b0000-0000-7abc-8def-" << std::hex << std::setw(12)
              << std::setfill('0') << i + 2 << std::dec << "\",\"parent\":null,\"components\":{"
              << "\"cuexis.transform\":{\"version\":1,\"position\":[" << x << ',' << y
              << R"(,0],"rotation":[0,0,0,1],"scale":[0.06,0.06,0.06]},
"cuexis.renderable":{"version":1,"mesh":{"domain":"asset","id":"mesh"},"material":{"domain":"asset","id":"material"}})";
        if (animated)
            chart
                << R"(,"cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}})";
        chart << "},\"extensions\":{}}";
    }
    chart << "],\"requiredExtensions\":[],\"extensions\":{}}";
    // The host applications cannot otherwise be driven with the same stress chart, because the
    // fixture only ever existed in this process. Writing the exact text the loader receives lets a
    // product-level run reuse it byte for byte.
    const auto fixtureDump = test_support::environmentVariable("CUEXIS_GPU_FIXTURE_DUMP");
    if (!fixtureDump.empty()) {
        std::ofstream dump{std::filesystem::path{fixtureDump}, std::ios::binary | std::ios::trunc};
        dump << chart.str();
    }
    return throughputTake(
        PlaybackSource::fromTypedProject({.sourceId = "gpu-throughput-grid",
                                          .chartJson = chart.str(),
                                          .assets = {{.id = "mesh",
                                                      .type = PlaybackAssetType::Mesh,
                                                      .rootId = "main",
                                                      .logicalSource = "triangle.mesh.bin"},
                                                     {.id = "material",
                                                      .type = PlaybackAssetType::Material,
                                                      .rootId = "main",
                                                      .logicalSource = "opaque.material.bin"}}},
                                         std::move(provider)));
}
auto throughputUs(Clock::time_point begin, Clock::time_point end) -> double {
    return std::chrono::duration<double, std::micro>(end - begin).count();
}
// Startup observability (probe instrument v2). The registered measurement contract runs the
// whole synchronous startup before the window exists, which is what made the first seconds
// unobservable: nothing was printed while a stage ran and nothing recorded when the window
// appeared. The heartbeat only exists while a startup stage is active, prints nothing once the
// window is live, and is joined before warmup, so the measured loop keeps its previous shape.
struct StartupTrace final {
    std::mutex mutex;
    std::atomic<const char*> stage{nullptr};
    std::atomic<std::int64_t> stageBeginUs{0};
    std::atomic<bool> stop{false};
    std::thread heartbeat;
    Clock::time_point origin = Clock::now();
};

auto startupTrace() -> StartupTrace& {
    static StartupTrace trace;
    return trace;
}

auto startupUs(Clock::time_point point) -> double {
    return throughputUs(startupTrace().origin, point);
}

void startupLine(const std::string& line) {
    auto& trace = startupTrace();
    const std::lock_guard<std::mutex> lock{trace.mutex};
    std::cout << line << std::endl;
}

void startupHeartbeat() {
    auto& trace = startupTrace();
    while (!trace.stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds{250});
        if (trace.stop.load(std::memory_order_relaxed))
            break;
        const auto* name = trace.stage.load(std::memory_order_relaxed);
        if (name == nullptr)
            continue;
        const auto nowUs = startupUs(Clock::now());
        const auto beginUs =
            static_cast<double>(trace.stageBeginUs.load(std::memory_order_relaxed));
        std::ostringstream line;
        line << std::setprecision(17) << "GPU_STARTUP_HEARTBEAT " << name << ' ' << nowUs - beginUs
             << ' ' << nowUs;
        startupLine(line.str());
    }
}

void startupBegin() {
    auto& trace = startupTrace();
    trace.origin = Clock::now();
    trace.stop.store(false, std::memory_order_relaxed);
    trace.stage.store(nullptr, std::memory_order_relaxed);
    trace.heartbeat = std::thread{startupHeartbeat};
}

void startupFinish() {
    auto& trace = startupTrace();
    trace.stop.store(true, std::memory_order_relaxed);
    if (trace.heartbeat.joinable())
        trace.heartbeat.join();
}

template <class Function> auto throughputStartupStage(const char* name, Function&& function) {
    auto& trace = startupTrace();
    const auto begin = Clock::now();
    trace.stageBeginUs.store(static_cast<std::int64_t>(startupUs(begin)),
                             std::memory_order_relaxed);
    trace.stage.store(name, std::memory_order_relaxed);
    {
        const std::lock_guard<std::mutex> lock{trace.mutex};
        std::cout << "GPU_STARTUP_BEGIN " << name << std::endl;
    }
    auto result = function();
    const auto end = Clock::now();
    trace.stage.store(nullptr, std::memory_order_relaxed);
    {
        const std::lock_guard<std::mutex> lock{trace.mutex};
        std::cout << "GPU_STARTUP_END " << name << " " << std::setprecision(17)
                  << throughputUs(begin, end) << std::endl;
        std::cout << std::setprecision(17) << "GPU_STARTUP_AT " << name << ' ' << startupUs(end)
                  << std::endl;
    }
    return result;
}

void throughputPumpEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
            (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
            throw std::runtime_error("GPU throughput cancelled by window close or Escape");
        }
    }
}

auto throughputInView(const FrameSnapshot& snapshot) -> std::size_t {
    const auto transform = [](const float* matrix, const std::array<double, 4>& point) {
        std::array<double, 4> result{};
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t col = 0; col < 4; ++col)
                result[row] += matrix[col * 4 + row] * point[col];
        return result;
    };
    std::size_t count{};
    for (const auto& object : snapshot.objects) {
        const auto view =
            transform(snapshot.camera.viewMatrix,
                      {object.worldMatrix[12], object.worldMatrix[13], object.worldMatrix[14], 1.});
        const auto clip = transform(snapshot.camera.projectionMatrix, view);
        if (object.visible && object.hasTransform && clip[3] > 0 && std::abs(clip[0]) < clip[3] &&
            std::abs(clip[1]) < clip[3] && std::abs(clip[2]) < clip[3])
            ++count;
    }
    return count;
}
} // namespace

TEST_CASE("GPU visible entity throughput probe", "[.][device][gpu][throughput]") {
    // Controller configures scheduling before releasing this barrier. No GL work precedes it.
    if (test_support::environmentVariable("CUEXIS_GPU_BARRIER") == "1") {
        std::cout << "GPU_THROUGHPUT_READY" << std::endl;
        std::string go;
        std::getline(std::cin, go);
        REQUIRE(go == "go");
    }
    const auto count = throughputCount("CUEXIS_GPU_ENTITIES", 40000, 40000);
    const auto frames = throughputCount("CUEXIS_GPU_FRAMES", 600, 2000000);
    const auto warmup = throughputCount("CUEXIS_GPU_WARMUP", 120, 10000);
    const auto seconds = throughputCount("CUEXIS_GPU_SECONDS", 0, 120);
    const auto warmupMs = throughputCount("CUEXIS_GPU_WARMUP_MS", 0, 10000);
    const auto viewportWidth = throughputCount("CUEXIS_GPU_WIDTH", 1280, 3840);
    const auto viewportHeight = throughputCount("CUEXIS_GPU_HEIGHT", 720, 2160);
    const FrameViewport viewport{viewportWidth, viewportHeight};
    const auto mode = test_support::environmentVariable("CUEXIS_GPU_MODE");
    REQUIRE((mode == "render-only" || mode == "static" || mode == "animated"));
    const bool timing = test_support::environmentVariable("CUEXIS_GPU_TIMER") != "0";
    // Complete synchronous CPU preparation before creating a window. Keep the real startup
    // costs visible in the log; session and prepared state stay on their creating owner thread.
    startupBegin();
    startupLine("GPU_STARTUP_ORIGIN 0");
    PlaybackSession session;
    auto source = throughputStartupStage(
        "fixture_source", [&] { return throughputSource(count, mode == "animated"); });
    auto prepared = throughputStartupStage("playback_prepare", [&] {
        return throughputTake(session.prepareLoad(std::move(source), PlaybackMode::ChartClock));
    });
    auto runtime = throughputStartupStage(
        "sdl_runtime", [&] { return throughputTake(platform_sdl::SdlRuntime::create()); });
    render_opengl::OpenGlConfig config;
    config.vsync = false;
    auto configuration = throughputStartupStage("gl_context", [&] {
        return throughputTake(render_opengl::configureOpenGlContext(runtime, config));
    });
    platform_sdl::WindowConfig wc;
    wc.width = static_cast<int>(viewportWidth);
    wc.height = static_cast<int>(viewportHeight);
    wc.highDpi = false;
    wc.title = "Cuexis visible entities GPU throughput";
    auto window = throughputStartupStage("window_create", [&] {
        return throughputTake(platform_sdl::SdlWindow::create(runtime, wc));
    });
    throughputPumpEvents();
    auto* nativeWindow = static_cast<SDL_Window*>(window.lease().nativeHandle());
    {
        const auto flags = SDL_GetWindowFlags(nativeWindow);
        int windowWidth{}, windowHeight{};
        REQUIRE(SDL_GetWindowSize(nativeWindow, &windowWidth, &windowHeight));
        std::ostringstream line;
        line << std::setprecision(17) << "GPU_STARTUP_WINDOW {\"at_us\":" << startupUs(Clock::now())
             << ",\"window_id\":" << SDL_GetWindowID(nativeWindow)
             << ",\"hidden\":" << ((flags & SDL_WINDOW_HIDDEN) ? "true" : "false")
             << ",\"opengl\":" << ((flags & SDL_WINDOW_OPENGL) ? "true" : "false")
             << ",\"width\":" << windowWidth << ",\"height\":" << windowHeight << '}';
        startupLine(line.str());
    }
    auto backend = throughputStartupStage("backend_create", [&] {
        return throughputTake(
            render_opengl::OpenGlBackend::create(window, std::move(configuration)));
    });
    throughputStartupStage("pixel_readback", [&] {
        throughputTake(
            render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(backend, true));
        return true;
    });
    int interval{};
    REQUIRE(SDL_GL_GetSwapInterval(&interval));
    REQUIRE(interval == 0);
    int width{}, height{};
    REQUIRE(SDL_GetWindowSizeInPixels(nativeWindow, &width, &height));
    REQUIRE(width == static_cast<int>(viewportWidth));
    REQUIRE(height == static_cast<int>(viewportHeight));
    auto presentation = throughputStartupStage(
        "renderer_prepare", [&] { return throughputTake(backend.prepare(prepared, {})); });
    throughputTake(backend.accepts(presentation));
    throughputStartupStage("session_commit", [&] {
        throughputTake(session.commit(std::move(prepared)));
        return true;
    });
    throughputStartupStage("backend_activate", [&] {
        backend.activate(std::move(presentation));
        return true;
    });
    throughputPumpEvents();
    throughputStartupStage("first_update", [&] {
        throughputTake(session.update({0, 0, 1}));
        return true;
    });
    auto snapshot = throughputStartupStage(
        "first_extract", [&] { return throughputTake(session.extractFrame(viewport)); });
    REQUIRE(snapshot.objects.size() == count);
    REQUIRE(throughputInView(snapshot) == count);
    const bool readback = test_support::environmentVariable("CUEXIS_GPU_PIXEL_READBACK") != "0";
    // Pixel proof is explicit and outside steady-frame timing in both modes.
    throughputPumpEvents();
    throughputStartupStage("first_submit",
                           [&] { return throughputTake(backend.submit(snapshot)); });
    throughputStartupStage("first_present", [&] {
        throughputTake(backend.present());
        return true;
    });
    REQUIRE(backend.lastPixelProbe().rgba[0] > 200);
    REQUIRE(backend.lastPixelProbe().rgba[1] > 100);
    REQUIRE(backend.lastPixelProbe().rgba[2] < 160);
    {
        // Everything after this point is measured: the window exists, shows a proven frame and
        // the heartbeat is stopped so no reporting thread can add wakeups to the measurement.
        std::ostringstream line;
        line << std::setprecision(17) << "GPU_STARTUP_SHOWN " << startupUs(Clock::now());
        startupLine(line.str());
        std::ostringstream total;
        total << std::setprecision(17)
              << "GPU_STARTUP_TOTAL {\"first_present_us\":" << startupUs(Clock::now()) << '}';
        startupLine(total.str());
    }
    startupFinish();
    throughputTake(
        render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(backend, readback));
    render_opengl::test_only::GpuTimer timer;
    if (timing)
        throughputTake(timer.initialize());
    struct Row {
        double update{}, extract{}, submit{}, present{}, total{}, cycle{}, end{};
    };
    std::vector<Row> samples;
    std::vector<std::uint64_t> gpuNs;
    const auto reserve = seconds ? std::max(frames, seconds * 5000) : frames;
    samples.reserve(reserve);
    gpuNs.reserve(timing ? reserve : 0);
    const auto collect = [&](bool drain) {
        const auto deadline = Clock::now() + std::chrono::seconds{5};
        while (timer.pending()) {
            const auto result = throughputTake(timer.poll());
            if (result) {
                if (result->ordinal != gpuNs.size())
                    throw std::runtime_error("GPU query ordinal mismatch");
                gpuNs.push_back(result->nanoseconds);
            } else if (!drain) {
                break;
            } else {
                if (Clock::now() >= deadline)
                    throw std::runtime_error("GPU query drain timed out");
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        }
    };
    Clock::time_point measuredStart, measuredEnd;
    const auto warmupStart = Clock::now();
    auto lastPresented = warmupStart;
    bool measuring = false;
    unsigned warmupCount = 0;
    for (unsigned i = 0; i < 2100000; ++i) {
        if (!measuring && warmupCount >= warmup &&
            throughputUs(warmupStart, lastPresented) >= warmupMs * 1000.) {
            measuring = true;
            measuredStart = lastPresented;
        }
        throughputPumpEvents();
        if (timing)
            collect(false);
        Row row;
        const auto begin = Clock::now();
        auto updated = begin;
        auto extracted = begin;
        if (mode != "render-only") {
            throughputTake(
                session.update({static_cast<double>(i + 1) * 1000. / 240., 1000. / 240., 1}));
            updated = Clock::now();
            throughputTake(session.extractFrame(viewport, snapshot));
            extracted = Clock::now();
        }
        if (timing && measuring)
            throughputTake(timer.begin(samples.size()));
        {
            const auto draw = throughputTake(backend.submit(snapshot, nullptr));
            if (!draw.cameraActive || draw.opaque.size() != count || !draw.transparent.empty() ||
                !backend.lastPixelProbe().presentationDrawn)
                throw std::runtime_error("Not all expected visible entities were drawn");
        }
        if (timing && measuring)
            throughputTake(timer.end());
        const auto submitted = Clock::now();
        throughputTake(backend.present());
        const auto presented = Clock::now();
        row = {throughputUs(begin, updated),
               throughputUs(updated, extracted),
               throughputUs(extracted, submitted),
               throughputUs(submitted, presented),
               throughputUs(begin, presented),
               throughputUs(lastPresented, presented),
               measuring ? throughputUs(measuredStart, presented) : 0.};
        lastPresented = presented;
        if (measuring)
            samples.push_back(row);
        else
            ++warmupCount;
        if (measuring && (seconds ? row.end >= seconds * 1000000. : samples.size() >= frames))
            break;
    }
    measuredEnd = lastPresented;
    if (timing)
        collect(true);
    REQUIRE_FALSE(samples.empty());
    REQUIRE(samples.size() <= 2000000);
    if (seconds)
        REQUIRE(throughputUs(measuredStart, measuredEnd) >= seconds * 1000000.);
    else
        REQUIRE(samples.size() == frames);
    REQUIRE(gpuNs.size() == (timing ? samples.size() : 0));
    const auto inView = throughputInView(snapshot);
    REQUIRE(inView == count);
    throughputTake(render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(backend, true));
    throughputTake(backend.submit(snapshot));
    throughputTake(backend.present());
    // The opaque fixture is orange. A background-only frame cannot pass this proof.
    REQUIRE(backend.lastPixelProbe().rgba[0] > 200);
    REQUIRE(backend.lastPixelProbe().rgba[1] > 100);
    REQUIRE(backend.lastPixelProbe().rgba[2] < 160);
    const auto moved = static_cast<std::size_t>(
        std::count_if(snapshot.objects.begin(), snapshot.objects.end(), [](const auto& object) {
            return object.hasTransform && object.worldMatrix[14] > 0;
        }));
    REQUIRE(moved == (mode == "animated" ? count : 0));
    std::cout << std::setprecision(17) << "GPU_THROUGHPUT_ID {\"mode\":\"" << mode
              << "\",\"entities\":" << count << ",\"draws_per_frame\":" << count
              << ",\"in_view_centers\":" << inView << ",\"fixture_revision\":3"
              << ",\"moved_entities\":" << moved << ",\"frames\":" << samples.size()
              << ",\"duration_seconds\":" << seconds << ",\"warmup_frames\":" << warmupCount
              << ",\"pixel_readback\":" << (readback ? "true" : "false")
              << ",\"outside_timing_pixel_proof_frames\":2"
              << ",\"warmup_ms\":" << warmupMs << ",\"timer\":" << (timing ? "true" : "false")
              << ",\"vsync\":" << interval << ",\"width\":" << width << ",\"height\":" << height
              << ",\"vendor\":" << std::quoted(backend.info().vendor)
              << ",\"renderer\":" << std::quoted(backend.info().renderer)
              << ",\"gl_version\":" << std::quoted(backend.info().version)
              << ",\"measured_wall_us\":" << throughputUs(measuredStart, measuredEnd)
              << ",\"pixel_rgba\":[" << unsigned(backend.lastPixelProbe().rgba[0]) << ','
              << unsigned(backend.lastPixelProbe().rgba[1]) << ','
              << unsigned(backend.lastPixelProbe().rgba[2]) << ','
              << unsigned(backend.lastPixelProbe().rgba[3]) << ']'
              << ",\"audio\":false,\"formal\":false}\n";
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        std::cout << "GPU_THROUGHPUT_SAMPLE {\"ordinal\":" << i << ",\"update_us\":" << s.update
                  << ",\"extract_us\":" << s.extract << ",\"submit_us\":" << s.submit
                  << ",\"present_us\":" << s.present << ",\"total_us\":" << s.total
                  << ",\"cycle_us\":" << s.cycle << ",\"end_us\":" << s.end;
        if (timing)
            std::cout << ",\"gpu_interval_us\":" << static_cast<double>(gpuNs[i]) / 1000.;
        std::cout << "}\n";
    }
    throughputTake(timer.close());
    throughputTake(backend.close());
}
