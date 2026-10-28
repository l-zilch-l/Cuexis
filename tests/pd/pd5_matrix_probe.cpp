#include "../../app/execution_host/frame_delivery.hpp"
#include "../../app/player/src/execution_host.hpp"
#include "../rpa/execution_fixture.hpp"
#include "../support/environment.hpp"
#include "measurement_environment.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#if defined(_WIN32)
namespace {
using namespace cuexis;
using namespace playback;
using namespace playback::detail;
using Clock = std::chrono::steady_clock;
template <class T> auto matrixTake(core::Result<T> result) -> T {
    if (!result)
        throw std::runtime_error(std::string{result.error().code()} + ": " +
                                 std::string{result.error().message()});
    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}
auto matrixNow() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch())
        .count();
}
auto matrixCount(const char* key, unsigned fallback) -> unsigned {
    const auto text = test_support::environmentVariable(key);
    if (text.empty())
        return fallback;
    std::size_t end{};
    const auto count = std::stoul(text, &end);
    if (end != text.size() || count == 0 || count > 1000000)
        throw std::runtime_error("Invalid finite matrix count");
    return static_cast<unsigned>(count);
}
auto sparseSource(unsigned entities) -> PlaybackSource {
    const auto read = [](const char* name) {
        std::ifstream input{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                "assets/projects/stage3_project/assets" / name,
                            std::ios::binary};
        REQUIRE(input);
        std::string text{std::istreambuf_iterator<char>{input}, {}};
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    };
    auto provider = matrixTake(content::MemoryContentProvider::create(
        {{"main", "triangle.mesh.bin", read("meshes/triangle.mesh.bin")},
         {"main", "opaque.material.bin", read("materials/opaque.material.bin")}}));
    std::ostringstream chart;
    chart
        << R"({"format":"cuexis.chart","version":1,"chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
"timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
"camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,"pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
"templates":[],"behaviors":[{"id":"move","type":"behavior.transform.keyframe","version":1,"tracks":[
{"property":"transform.position.x","keys":[{"beat":{"numerator":0,"denominator":1},"value":0},
{"beat":{"numerator":2400,"denominator":1},"value":10,"easing":"linear"}]}]}],"objects":[)";
    for (unsigned i = 0; i < entities; ++i) {
        if (i)
            chart << ',';
        chart
            << "{\"id\":\"019b0000-0000-7abc-8def-" << std::hex << std::setw(12)
            << std::setfill('0') << i + 2 << std::dec << "\",\"parent\":null,\"components\":{"
            << R"("cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]})";
        if (i == 0)
            chart
                << R"(,"cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}},
"cuexis.renderable":{"version":1,"mesh":{"domain":"asset","id":"mesh"},"material":{"domain":"asset","id":"material"}})";
        chart << "},\"extensions\":{}}";
    }
    chart << "],\"requiredExtensions\":[],\"extensions\":{}}";
    return matrixTake(
        PlaybackSource::fromTypedProject({.sourceId = "pd5-sparse",
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
} // namespace

#if defined(CUEXIS_PD5_MATRIX_DEVICE)
TEST_CASE("PD5 renderer and capture workload matrix", "[.][pd][pd5][matrix][device]") {
    pd5Counts.clear();
#else
TEST_CASE("PD5 presentation workload matrix", "[.][pd][pd5][matrix]") {
#endif
    internal::pd::setRecording(false);
    const auto mode = test_support::environmentVariable("CUEXIS_PD5_MODE");
    REQUIRE((mode == "no-consumer" || mode == "low-delivery" || mode == "capture-1" ||
             mode == "capture-2" || mode == "capture-4" || mode == "sparse" || mode == "topology"));
    const auto frames = matrixCount("CUEXIS_PD5_FRAMES", 14400);
    const auto warmup = matrixCount("CUEXIS_PD5_WARMUP", 2400);
    const bool smoke = test_support::environmentVariable("CUEXIS_PD5_SMOKE") == "1";
    const auto ratio = mode == "low-delivery" ? 8U : 1U;
    const auto consumers = mode == "no-consumer" ? 0U
                           : mode == "capture-4" ? 4U
                           : mode == "capture-2" ? 2U
                                                 : 1U;
    const bool sparse = mode == "sparse" || mode == "topology";
    internal::pd::memory::checkpoint("before-load");
    auto session = std::make_unique<PlaybackSession>();
#if defined(CUEXIS_PD5_MATRIX_DEVICE)
    REQUIRE((mode == "capture-1" || mode == "capture-2" || mode == "capture-4"));
    const unsigned renderers = 1;
    auto sdl = matrixTake(platform_sdl::SdlRuntime::create());
    render_opengl::OpenGlConfig gl;
    gl.vsync = true;
    auto configuration = matrixTake(render_opengl::configureOpenGlContext(sdl, gl));
    platform_sdl::WindowConfig wc;
    wc.width = 1280;
    wc.height = 720;
    wc.title = "Cuexis PD5 renderer and captures";
    auto window = matrixTake(platform_sdl::SdlWindow::create(sdl, wc));
    if (smoke)
        REQUIRE(SDL_HideWindow(static_cast<SDL_Window*>(window.lease().nativeHandle())));
    auto backend =
        matrixTake(render_opengl::OpenGlBackend::create(window, std::move(configuration)));
    Pd5GpuRenderer renderer{std::move(backend),
                            test_support::environmentVariable("CUEXIS_PD5_GPU_TIMER") == "1"};
    auto initialPrepared =
        matrixTake(session->prepareLoad(test_support::presentation(), PlaybackMode::ChartClock));
    auto presentation = matrixTake(renderer.prepare(initialPrepared, {}));
    matrixTake(renderer.accepts(presentation));
    matrixTake(session->commit(std::move(initialPrepared)));
    renderer.activate(std::move(presentation));
#else
    const unsigned renderers = 0;
    matrixTake(session->load(sparse ? sparseSource(200) : test_support::presentation(),
                             PlaybackMode::ChartClock));
#endif
    const auto totalConsumers = consumers + renderers;
    const auto initial = matrixTake(session->presentationSource());
    auto delivery = matrixTake(FrameDelivery::create({8, 8, 8U * 1024U * 1024U, 32, 64}));
    HostContext context;
    context.sessionScope = context.controlSequence = context.resourceGeneration = 1;
    context.clockSegment = context.runEpoch = context.rendererGeneration = 1;
    std::vector<ConsumerRegistration> registrations;
    const auto registerConsumers = [&] {
        for (unsigned i = 0; i < totalConsumers; ++i) {
            ConsumerProfile consumer{
                ConsumerMode::exactCapture, false, 1, 1024U * 1024U, 1024U * 1024U, 0, 0, 1};
#if defined(CUEXIS_PD5_MATRIX_DEVICE)
            if (i == consumers) {
                consumer.mode = ConsumerMode::realtimeLatest;
                consumer.renderer = true;
                consumer.maxPayloads = 2;
                consumer.maxBytes = consumer.maxPayloads * consumer.frameByteBound;
                consumer.maxAttempts = 2;
                consumer.rendererInstance = renderer.identity().instance;
                consumer.rendererGeneration = renderer.identity().generation;
            }
#endif
            auto registration = matrixTake(delivery->registerConsumer(i + 1, context, consumer));
            matrixTake(delivery->confirmRegistration({registration, true}));
            registrations.push_back(registration);
        }
    };
    registerConsumers();
    internal::pd::memory::checkpoint("active");
    std::size_t peakCount{}, peakBytes{};
    const auto retireDelivery = [&] {
        const auto storage = delivery->storageMetrics();
        peakCount = std::max(peakCount, storage.peakCount);
        peakBytes = std::max(peakBytes, storage.peakBytes);
        REQUIRE(storage.liveCount == 0);
        REQUIRE(delivery->attemptMetrics().liveCount == 0);
        for (const auto& registration : registrations) {
            auto revoked = matrixTake(delivery->beginRevoke(registration.consumerId));
            matrixTake(delivery->confirmRevoke({revoked, false}));
        }
        registrations.clear();
        delivery->closeAdmission();
    };
    std::map<std::string, unsigned> counts;
    auto sample = [&](const char* metric, std::int64_t start, std::int64_t end) {
        std::cout << "PD5_SAMPLE {\"metric\":\"" << metric << "\",\"ordinal\":" << counts[metric]++
                  << ",\"elapsed_us\":" << end - start << ",\"complete\":true}\n";
    };
    std::cout << std::setprecision(17);
    pd_measure::environmentJson("begin");
    std::uint64_t digest = 14695981039346656037ULL, updates{}, materializations{}, segment = 0;
    std::uint64_t ordinal = initial.updateOrdinal;
    std::int64_t tick{};
    unsigned reloads{};
    const auto anchor = Clock::now();
    for (unsigned i = 0; i < warmup + frames; ++i) {
        if (!smoke)
            std::this_thread::sleep_until(
                anchor +
                std::chrono::nanoseconds{static_cast<std::int64_t>(i) * 1000000000LL / 240});
        const bool measured = i >= warmup;
        if (mode == "topology" && i && i % (smoke ? 4U : 256U) == 0) {
            auto replacement = sparseSource(200 + (++reloads % 2));
            retireDelivery();
            const auto start = matrixNow();
            auto prepared = matrixTake(session->prepareReload(
                std::move(replacement), {0, 0, ++segment}, ReloadPolicy::RestartAtZero));
            matrixTake(session->commit(std::move(prepared)));
            const auto end = matrixNow();
            tick = 0;
            ordinal = matrixTake(session->presentationSource()).updateOrdinal;
            ++context.resourceGeneration;
            ++context.controlSequence;
            context.clockSegment = segment;
            delivery = matrixTake(FrameDelivery::create({8, 8, 8U * 1024U * 1024U, 32, 64}));
            registerConsumers();
            if (measured)
                sample("topology_reload", start, end);
        }
        RuntimeFrame runtime{};
        for (unsigned u = 0; u < ratio; ++u) {
            tick += 7000;
            runtime = {static_cast<double>(tick) / 1000., 7., segment};
            const auto start = matrixNow();
            internal::pd::setRecording(measured);
            matrixTake(session->update(runtime));
            internal::pd::setRecording(false);
            const auto end = matrixNow();
            REQUIRE(matrixTake(session->presentationSource()).updateOrdinal == ++ordinal);
            if (measured) {
                sample("sdk_update", start, end);
                ++updates;
            }
        }
        const auto version = matrixTake(session->presentationSource()).resultVersion;
        std::optional<std::uint64_t> revisionDigest;
        std::vector<FrameEnvelope> held;
        held.reserve(totalConsumers);
        for (const auto& registration : registrations) {
            FrameRequest request;
            request.consumerId = registration.consumerId;
            request.requestId = i + 1;
            request.context = context;
            request.projectionScope = context.resourceGeneration;
            request.viewport = {1280, 720};
            request.exactVersion = version;
            request.deadlineMonotonicUs = matrixNow() + 5000000;
            const auto start = matrixNow();
            auto admission = matrixTake(delivery->admit(request, version, matrixNow(), false));
            REQUIRE(admission.materialize);
            auto payload = std::make_shared<FramePayload>();
            payload->storage = std::move(admission.reservation);
            const auto readStart = matrixNow();
            internal::pd::setRecording(measured);
            matrixTake(session->extractFrame(request.viewport, payload->snapshot));
            internal::pd::setRecording(false);
            const auto readEnd = matrixNow();
            REQUIRE(payloadBytes(*payload) <= payload->storage.bytes());
            FrameEnvelope envelope;
            envelope.context = context;
            envelope.projectionScope = context.resourceGeneration;
            envelope.frameId = admission.info.frameId;
            envelope.runtimeFrame = runtime;
            envelope.source = matrixTake(session->presentationSource());
            envelope.payload = std::move(payload);
            envelope.attemptLease = FrameAttemptLease::create(std::move(admission.attempt));
            envelope.dispatch = std::make_shared<DispatchToken>();
            REQUIRE(
                matrixTake(delivery->publish(request, std::move(envelope), matrixNow())).status ==
                DeliveryStatus::ready);
            auto taken =
                matrixTake(delivery->take(request.consumerId, request.requestId, matrixNow()));
            const auto end = matrixNow();
            REQUIRE(taken);
            REQUIRE(taken->source->resultVersion == version);
            const auto frameDigest =
                matrixTake(computeFrameDigest(runtime, taken->snapshotView())).value;
            if (revisionDigest)
                REQUIRE(frameDigest == *revisionDigest);
            revisionDigest = frameDigest;
            digest = (digest ^ frameDigest) * 1099511628211ULL;
            held.push_back(std::move(*taken));
            if (measured) {
                sample("materialize", readStart, readEnd);
                sample(registration.profile.renderer ? "renderer_delivery" : "capture_delivery",
                       start, end);
                ++materializations;
            }
        }
        REQUIRE(delivery->storageMetrics().liveCount == totalConsumers);
#if defined(CUEXIS_PD5_MATRIX_DEVICE)
        renderer.record = measured;
        matrixTake(renderer.submit(held.back().snapshotView(), nullptr));
        matrixTake(renderer.present());
        renderer.collect();
        SDL_PumpEvents();
#endif
        held.clear();
        REQUIRE(delivery->storageMetrics().liveCount == 0);
        REQUIRE(delivery->attemptMetrics().liveCount == 0);
    }
    retireDelivery();
    session.reset();
    REQUIRE(updates == static_cast<std::uint64_t>(frames) * ratio);
    REQUIRE(materializations == static_cast<std::uint64_t>(frames) * totalConsumers);
#if defined(CUEXIS_PD5_MATRIX_DEVICE)
    renderer.collect(true);
    matrixTake(renderer.close());
    for (const auto& [metric, count] : pd5Counts)
        counts[metric] += count;
#endif
    internal::pd::memory::checkpoint("after-close");
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    REQUIRE(internal::pd::metrics.path);
    const auto& observedSnapshot =
        internal::pd::metrics.stages[static_cast<unsigned>(internal::pd::Stage::snapshot)];
    REQUIRE(observedSnapshot.count == materializations);
    REQUIRE(observedSnapshot.omitted == 0);
#endif
    pd_measure::environmentJson("end");
    std::cout << "PD5_ID {\"schema\":\"pd5.probe.v1\",\"mode\":\"" << mode << "\",\"harness\":\""
#if defined(CUEXIS_PD5_MATRIX_DEVICE)
              << "renderer-matrix"
#else
              << "presentation-matrix"
#endif
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
              << "\","
              << "\"smoke\":" << (smoke ? "true" : "false") << ",\"frames\":" << frames
              << ",\"warmup_frames\":" << warmup << ",\"updates\":" << updates
              << ",\"materializations\":" << materializations
              << ",\"capture_consumers\":" << consumers << ",\"renderer_consumers\":" << renderers
              << ",\"source_step_ticks\":7000,\"source_tick_rate\":1000000"
              << ",\"base_entities\":" << (sparse ? 200 : 1) << ",\"max_entities\":"
              << (mode == "topology" ? 201
                  : sparse           ? 200
                                     : 1)
              << ",\"reloads\":" << reloads << ",\"delivery_generations\":" << reloads + 1
              << ",\"payload_peak_count\":" << peakCount << ",\"payload_peak_bytes\":" << peakBytes
              << ",\"payload_charge_per_frame\":1048576,\"payload_live_after_close\":0,\"attempt_"
                 "live_after_close\":0"
              << ",\"gameplay\":false,\"replay_equal\":null,\"audio\":false,\"gpu\":"
              << (renderers ? "true" : "false") << ",\"closed\":true,\"digest\":" << digest
              << ",\"counts\":{";
    bool comma = false;
    for (const auto& [name, count] : counts) {
        if (comma)
            std::cout << ',';
        comma = true;
        std::cout << '"' << name << "\":" << count;
    }
    std::cout << "}}\n";
}
#if !defined(CUEXIS_PD5_MATRIX_DEVICE)
#include "pd5_capacity_probe.hpp"
#endif
#endif
