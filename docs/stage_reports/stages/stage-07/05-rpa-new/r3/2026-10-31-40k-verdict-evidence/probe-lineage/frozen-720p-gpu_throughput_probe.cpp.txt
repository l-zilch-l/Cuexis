#include "../../engine/render_opengl/internal/cuexis_internal/gpu_timer.hpp"
#include "../support/environment.hpp"
#include <SDL3/SDL.h>
#include <catch2/catch_test_macros.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
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
    for (unsigned i = 0; i < count; ++i) {
        if (i)
            chart << ',';
        const auto x = (static_cast<int>(i % 200) - 100) * 0.09;
        const auto y = (static_cast<int>(i / 200) - 100) * 0.05;
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
    const auto frames = throughputCount("CUEXIS_GPU_FRAMES", 600, 10000);
    const auto warmup = throughputCount("CUEXIS_GPU_WARMUP", 120, 10000);
    const auto mode = test_support::environmentVariable("CUEXIS_GPU_MODE");
    REQUIRE((mode == "render-only" || mode == "static" || mode == "animated"));
    const bool timing = test_support::environmentVariable("CUEXIS_GPU_TIMER") != "0";
    auto runtime = throughputTake(platform_sdl::SdlRuntime::create());
    render_opengl::OpenGlConfig config;
    config.vsync = false;
    auto configuration = throughputTake(render_opengl::configureOpenGlContext(runtime, config));
    platform_sdl::WindowConfig wc;
    wc.width = 1280;
    wc.height = 720;
    wc.highDpi = false;
    wc.title = "Cuexis 40000 visible entities GPU throughput";
    auto window = throughputTake(platform_sdl::SdlWindow::create(runtime, wc));
    auto* nativeWindow = static_cast<SDL_Window*>(window.lease().nativeHandle());
    auto backend =
        throughputTake(render_opengl::OpenGlBackend::create(window, std::move(configuration)));
    int interval{};
    REQUIRE(SDL_GL_GetSwapInterval(&interval));
    REQUIRE(interval == 0);
    int width{}, height{};
    REQUIRE(SDL_GetWindowSizeInPixels(nativeWindow, &width, &height));
    REQUIRE(width == 1280);
    REQUIRE(height == 720);
    PlaybackSession session;
    auto prepared = throughputTake(
        session.prepareLoad(throughputSource(count, mode == "animated"), PlaybackMode::ChartClock));
    auto presentation = throughputTake(backend.prepare(prepared, {}));
    throughputTake(backend.accepts(presentation));
    throughputTake(session.commit(std::move(prepared)));
    backend.activate(std::move(presentation));
    throughputTake(session.update({0, 0, 1}));
    auto snapshot = throughputTake(session.extractFrame({1280, 720}));
    REQUIRE(snapshot.objects.size() == count);
    REQUIRE(throughputInView(snapshot) == count);
    render_opengl::test_only::GpuTimer timer;
    if (timing)
        throughputTake(timer.initialize());
    struct Row {
        double update{}, extract{}, submit{}, present{}, total{};
    };
    std::vector<Row> samples;
    std::vector<std::uint64_t> gpuNs;
    samples.reserve(frames);
    gpuNs.reserve(frames);
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
    for (unsigned i = 0; i < warmup + frames; ++i) {
        SDL_PumpEvents();
        if (timing)
            collect(false);
        if (i == warmup)
            measuredStart = Clock::now();
        Row row;
        const auto begin = Clock::now();
        auto updated = begin;
        auto extracted = begin;
        if (mode != "render-only") {
            throughputTake(
                session.update({static_cast<double>(i + 1) * 1000. / 240., 1000. / 240., 1}));
            updated = Clock::now();
            throughputTake(session.extractFrame({1280, 720}, snapshot));
            extracted = Clock::now();
        }
        if (timing && i >= warmup)
            throughputTake(timer.begin(i - warmup));
        {
            const auto draw = throughputTake(backend.submit(snapshot, nullptr));
            if (!draw.cameraActive || draw.opaque.size() != count || !draw.transparent.empty() ||
                !backend.lastPixelProbe().presentationDrawn)
                throw std::runtime_error("Not all expected visible entities were drawn");
        }
        if (timing && i >= warmup)
            throughputTake(timer.end());
        const auto submitted = Clock::now();
        throughputTake(backend.present());
        const auto presented = Clock::now();
        row = {throughputUs(begin, updated), throughputUs(updated, extracted),
               throughputUs(extracted, submitted), throughputUs(submitted, presented),
               throughputUs(begin, presented)};
        if (i >= warmup)
            samples.push_back(row);
    }
    measuredEnd = Clock::now();
    if (timing)
        collect(true);
    REQUIRE(samples.size() == frames);
    REQUIRE(gpuNs.size() == (timing ? frames : 0));
    const auto inView = throughputInView(snapshot);
    REQUIRE(inView == count);
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
              << ",\"in_view_centers\":" << inView << ",\"fixture_revision\":2"
              << ",\"moved_entities\":" << moved << ",\"frames\":" << frames
              << ",\"warmup_frames\":" << warmup << ",\"timer\":" << (timing ? "true" : "false")
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
                  << ",\"present_us\":" << s.present << ",\"total_us\":" << s.total;
        if (timing)
            std::cout << ",\"gpu_interval_us\":" << static_cast<double>(gpuNs[i]) / 1000.;
        std::cout << "}\n";
    }
    throughputTake(timer.close());
    throughputTake(backend.close());
}
