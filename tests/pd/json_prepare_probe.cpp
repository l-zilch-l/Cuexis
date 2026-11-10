// Legacy prepare cost probe: one timed synchronous prepareLoad per process on a chart generated
// in memory, then the digest of the first extracted frame.
//
// Evaluation instrument for the parse-once comparison. Not registered with CTest: the test case is
// hidden and is driven by a runner that sets affinity, priority and throttling before writing "go".

#include "../support/environment.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace cuexis;
using namespace playback;
using Clock = std::chrono::steady_clock;

template <class T> auto prepareTake(core::Result<T> result) -> T {
    if (!result)
        throw std::runtime_error(std::string{result.error().code()} + ": " +
                                 std::string{result.error().message()});
    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}

auto prepareCount(const char* key, unsigned fallback, unsigned limit) -> unsigned {
    const auto text = test_support::environmentVariable(key);
    if (text.empty())
        return fallback;
    std::size_t used{};
    const auto value = std::stoul(text, &used);
    if (used != text.size() || value == 0 || value > limit)
        throw std::runtime_error("Invalid finite prepare count");
    return static_cast<unsigned>(value);
}

[[nodiscard]] auto prepareSource(unsigned count) -> PlaybackSource {
    const auto read = [](const char* name) {
        std::ifstream file{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                               "assets/projects/stage3_project/assets" / name,
                           std::ios::binary};
        if (!file)
            throw std::runtime_error("Missing prepare fixture resource");
        const std::string text{std::istreambuf_iterator<char>{file}, {}};
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    };
    auto provider = prepareTake(content::MemoryContentProvider::create(
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
        chart << "},\"extensions\":{}}";
    }
    chart << "],\"requiredExtensions\":[],\"extensions\":{}}";
    const auto fixtureDump = test_support::environmentVariable("CUEXIS_PREPARE_FIXTURE_DUMP");
    if (!fixtureDump.empty()) {
        std::ofstream dump{std::filesystem::path{fixtureDump}, std::ios::binary | std::ios::trunc};
        dump << chart.str();
    }
    return prepareTake(
        PlaybackSource::fromTypedProject({.sourceId = "prepare-grid",
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

auto prepareUs(Clock::time_point begin, Clock::time_point end) -> double {
    return std::chrono::duration<double, std::micro>(end - begin).count();
}
} // namespace

TEST_CASE("CPU legacy preparation fixed diagnostic", "[.][prepare-probe]") {
    std::cout << "PREPARE_READY" << std::endl;
    std::string go;
    std::getline(std::cin, go);
    REQUIRE(go == "go");
    const auto count = prepareCount("CUEXIS_PREPARE_ENTITIES", 40000, 40000);
    auto source = prepareSource(count);
    PlaybackSession session;
    const auto begin = Clock::now();
    auto prepared = prepareTake(session.prepareLoad(std::move(source), PlaybackMode::ChartClock));
    const auto end = Clock::now();
    const auto identity = prepared.semanticIdentity();
    REQUIRE(identity);
    prepareTake(session.commit(std::move(prepared)));
    const RuntimeFrame frame{0, 0, 1};
    prepareTake(session.update(frame));
    auto snapshot = prepareTake(session.extractFrame({1920, 1080}));
    REQUIRE(snapshot.objects.size() == count);
    const auto digest = prepareTake(computeFrameDigest(frame, snapshot));
    std::cout << "PREPARE_RESULT {\"entities\":" << count
              << ",\"prepare_us\":" << std::setprecision(17) << prepareUs(begin, end)
              << ",\"identity\":\"";
    for (auto byte : identity->sha256)
        std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    std::cout << std::dec << "\",\"frame_digest\":" << digest.value << "}\n";
}
