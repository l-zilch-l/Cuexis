
// Common finite diagnostic harness, also compiled against the archived pre-PD source.
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

#if defined(_WIN32)
namespace {
auto scalePresentation(std::size_t objects) -> cuexis::playback::PlaybackSource {
    const auto read = [](std::string_view path) {
        std::ifstream in{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                             "assets/projects/stage3_project/assets" / path,
                         std::ios::binary};
        REQUIRE(in);
        std::string text{std::istreambuf_iterator<char>{in}, {}};
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    };
    auto provider = cuexis::content::MemoryContentProvider::create(
        {{"main", "triangle.mesh.bin", read("meshes/triangle.mesh.bin")},
         {"main", "opaque.material.bin", read("materials/opaque.material.bin")}});
    REQUIRE(provider);
    std::ostringstream chart;
    chart
        << R"({"format":"cuexis.chart","version":1,)"
        << R"("chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},)"
        << R"("timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},)"
        << R"("camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,)"
        << R"("pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},)"
        << R"("templates":[],"behaviors":[{"id":"move","type":"behavior.transform.keyframe","version":1,"tracks":[{"property":"transform.position.x","keys":[{"beat":{"numerator":0,"denominator":1},"value":0},{"beat":{"numerator":2400,"denominator":1},"value":10,"easing":"linear"}]}]}],"objects":[ )";
    for (std::size_t i = 0; i < objects; ++i) {
        if (i != 0)
            chart << ',';
        chart << R"({"id":"019b0000-0000-7abc-8def-)" << std::hex << std::setw(12)
              << std::setfill('0') << (i + 2) << std::dec << R"(","parent":null,"components":{)";
        if (i == 0)
            chart
                << R"("cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}},)";
        chart << R"("cuexis.transform":{"version":1,)"
              << R"("position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},)"
              << R"("cuexis.renderable":{"version":1,"mesh":{"domain":"asset","id":"mesh"},)"
              << R"("material":{"domain":"asset","id":"material"}}},"extensions":{}})";
    }
    chart << R"(],"requiredExtensions":[],"extensions":{}})";
    std::cout << "PD_FOLLOW_CONTENT {\"objects\":" << objects << ",\"sha256\":\""
              << cuexis::core::detail::sha256Hex(cuexis::core::detail::sha256(chart.str()))
              << "\"}\n";
    auto source = cuexis::playback::PlaybackSource::fromTypedProject(
        {.sourceId = "pd-scale",
         .chartJson = chart.str(),
         .assets = {{.id = "mesh",
                     .type = cuexis::playback::PlaybackAssetType::Mesh,
                     .rootId = "main",
                     .logicalSource = "triangle.mesh.bin"},
                    {.id = "material",
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "opaque.material.bin"}}},
        std::move(*provider));
    REQUIRE(source);
    return std::move(*source);
}
} // namespace

TEST_CASE("PD finite read and update scaling", "[.][pd][followup]") {
    std::cout << std::setprecision(17);
    pd_measure::environmentJson("begin");
    const auto text = cuexis::test_support::environmentVariable("CUEXIS_FOLLOW_COUNTS");
    REQUIRE_FALSE(text.empty());
    const bool split = !cuexis::test_support::environmentVariable("CUEXIS_FOLLOW_SPLIT").empty();
    std::istringstream counts{text};
    std::size_t objects{}, readCount{}, updateCount{};
    while (counts >> objects >> readCount >> updateCount) {
        REQUIRE(objects <= 40000);
        REQUIRE(readCount > 0);
        REQUIRE(updateCount > 0);
        for (const bool updated : {false, true}) {
            auto source = scalePresentation(objects);
            cuexis::playback::PlaybackSession session;
            take(session.commit(take(session.prepareLoad(
                std::move(source), cuexis::playback::PlaybackMode::ChartClock))));
            cuexis::playback::FrameSnapshot snapshot;
            std::size_t ordinal{};
            const auto update = [&] {
                ++ordinal;
                take(session.update({10 + static_cast<double>(ordinal) * 0.01, 0.01, 0}));
            };
            for (unsigned i = 0; i < 64; ++i) {
                update();
                take(session.extractFrame({1280, 720}, snapshot));
            }
            REQUIRE(snapshot.objects.size() == objects);
            REQUIRE(snapshot.camera.active);
            const auto count = updated ? updateCount : readCount;
            if (split) {
                double updateUs = 0.0;
                double extractUs = 0.0;
                for (std::size_t i = 0; i < count; ++i) {
                    if (updated) {
                        const auto start = Clock::now();
                        update();
                        updateUs += elapsed(start);
                    }
                    const auto start = Clock::now();
                    take(session.extractFrame({1280, 720}, snapshot));
                    extractUs += elapsed(start);
                }
                std::cout << "PD_FOLLOW_SPLIT {\"objects\":" << objects << ",\"mode\":\""
                          << (updated ? "updated" : "read") << "\",\"operations\":" << count
                          << ",\"update_us\":" << updateUs << ",\"extract_us\":" << extractUs
                          << "}\n";
            }
            auto prior = take(cuexis::playback::computeFrameDigest(
                                  {10 + static_cast<double>(ordinal) * 0.01, 0.01, 0}, snapshot))
                             .value;
            for (unsigned block = 0; block < 3; ++block) {
                const auto sample = pd_measure::measure([&] {
                    for (std::size_t i = 0; i < count; ++i) {
                        if (updated)
                            update();
                        take(session.extractFrame({1280, 720}, snapshot));
                    }
                });
                const auto digest =
                    take(cuexis::playback::computeFrameDigest(
                             {10 + static_cast<double>(ordinal) * 0.01, 0.01, 0}, snapshot))
                        .value;
                REQUIRE(snapshot.objects.size() == objects);
                if (updated)
                    REQUIRE(digest != prior);
                else
                    REQUIRE(digest == prior);
                prior = digest;
                std::cout << "PD_FOLLOW_ROW {\"objects\":" << objects << ",\"mode\":\""
                          << (updated ? "updated" : "read") << "\",\"block\":" << block
                          << ",\"operations\":" << count << ",\"elapsed_us\":" << sample.wallUs
                          << ",\"cpu_us\":" << sample.cpuUs << ",\"cycles\":" << sample.cycles
                          << ",\"digest\":" << digest << ",\"ordinal\":" << ordinal << "}\n";
            }
        }
    }
    pd_measure::environmentJson("end");
}
#endif
