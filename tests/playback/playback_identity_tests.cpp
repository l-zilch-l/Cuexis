#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view v1Chart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000201","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
            "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
  "templates":[],
  "behaviors":[],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000210","name":"camera","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,-10],
                          "rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.camera":{"version":1,"type":"perspective","fovY":60,
                       "near":0.1,"far":1000}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

[[nodiscard]] auto fixture(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           relative;
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"Could not read CFU-E3 fixture: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    const auto text = readText(path);
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] auto unusedProvider() -> std::shared_ptr<cuexis::content::IContentProvider> {
    auto provider = cuexis::content::HostContentProvider::create(
        [](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            return cuexis::core::unexpected(cuexis::core::Error{
                "test.content.unexpected", "The CFU-E3 provider must not be read"});
        });
    if (!provider) {
        throw std::runtime_error{"Could not create CFU-E3 content provider"};
    }
    return *provider;
}

[[nodiscard]] auto emptyMemoryProvider() -> std::shared_ptr<cuexis::content::IContentProvider> {
    auto provider = cuexis::content::MemoryContentProvider::create({});
    if (!provider) {
        throw std::runtime_error{"Could not create CFU-E3 memory provider"};
    }
    return *provider;
}

[[nodiscard]] auto typedChartSource(std::string_view relative, std::string sourceId,
                                    std::shared_ptr<cuexis::content::IContentProvider> provider)
    -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    return cuexis::playback::PlaybackSource::fromTypedProjectSource(
        {.sourceId = std::string{sourceId},
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                               .utf8Text = readText(fixture(relative))}},
         .assets = {}},
        std::move(provider));
}

[[nodiscard]] auto overrideOptions(double x, double scaleY, double fov)
    -> cuexis::playback::PlaybackPrepareOptions {
    return {
        .parameters = {
            .values = {
                {.id = "layout.x", .value = cuexis::playback::ChartParameterNumber{x}},
                {.id = "layout.scale-y", .value = cuexis::playback::ChartParameterNumber{scaleY}},
                {.id = "camera.fov", .value = cuexis::playback::ChartParameterNumber{fov}}}}};
}

[[nodiscard]] auto rationalOptions(std::int64_t numerator, std::int64_t denominator, double weight)
    -> cuexis::playback::PlaybackPrepareOptions {
    return {
        .parameters = {
            .values = {
                {.id = "motion.duration-scale",
                 .value = cuexis::playback::ChartParameterRational{numerator, denominator}},
                {.id = "motion.weight", .value = cuexis::playback::ChartParameterWeight{weight}}}}};
}

[[nodiscard]] auto loadIdentity(cuexis::playback::PlaybackSource&& source,
                                const cuexis::playback::PlaybackPrepareOptions& options = {})
    -> cuexis::playback::PreparedSemanticIdentity {
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(source), cuexis::playback::PlaybackMode::ChartClock, options);
    REQUIRE(prepared.has_value());
    const auto candidate = prepared->semanticIdentity();
    REQUIRE(candidate.has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    const auto active = session.semanticIdentity();
    REQUIRE(active.has_value());
    CHECK(*active == *candidate);
    return *active;
}

[[nodiscard]] auto framesEquivalent(const cuexis::playback::FrameSnapshot& left,
                                    const cuexis::playback::FrameSnapshot& right) -> bool {
    if (left.objects.size() != right.objects.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.objects.size(); ++index) {
        for (std::size_t component = 0; component < 16; ++component) {
            if (left.objects[index].worldMatrix[component] !=
                Catch::Approx(right.objects[index].worldMatrix[component])) {
                return false;
            }
        }
    }
    return left.camera.fovY == Catch::Approx(right.camera.fovY) &&
           left.camera.nearPlane == Catch::Approx(right.camera.nearPlane) &&
           left.camera.farPlane == Catch::Approx(right.camera.farPlane);
}

struct PreparedView final {
    cuexis::playback::PreparedSemanticIdentity identity{};
    cuexis::playback::FrameSnapshot frame{};
};

[[nodiscard]] auto prepareView(cuexis::playback::PlaybackSource&& source,
                               const cuexis::playback::PlaybackPrepareOptions& options = {})
    -> PreparedView {
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.load(std::move(source), cuexis::playback::PlaybackMode::ChartClock, options)
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto frame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(frame.has_value());
    auto identity = session.semanticIdentity();
    REQUIRE(identity.has_value());
    return {.identity = *identity, .frame = std::move(*frame)};
}

} // namespace

TEST_CASE("Empty and moved-from prepared playback have no semantic identity",
          "[playback][identity][cfu-e3]") {
    cuexis::playback::PlaybackSession session;
    const auto empty = session.semanticIdentity();
    REQUIRE_FALSE(empty.has_value());
    CHECK(empty.error().code() == "playback.session.empty");

    cuexis::playback::PreparedPlayback vacant;
    CHECK_FALSE(vacant.semanticIdentity().has_value());

    auto source = cuexis::playback::PlaybackSource::fromChartText(
        readText(fixture("valid/chart_v4_static_migration.json")));
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    REQUIRE(prepared->semanticIdentity().has_value());
    auto moved = std::move(*prepared);
    CHECK_FALSE(prepared->semanticIdentity().has_value());
    REQUIRE(moved.semanticIdentity().has_value());
    REQUIRE(session.commit(std::move(moved)).has_value());
    REQUIRE(session.unload().has_value());
    const auto afterUnload = session.semanticIdentity();
    REQUIRE_FALSE(afterUnload.has_value());
    CHECK(afterUnload.error().code() == "playback.session.empty");
}

TEST_CASE("Filesystem, memory, host and CXC sources share one static v4 semantic identity",
          "[playback][identity][cxc][cfu-e3][cfu-e4]") {
    const auto chart = readText(fixture("valid/chart_v4_static_migration.json"));
    auto textSource = cuexis::playback::PlaybackSource::fromChartText(chart);
    REQUIRE(textSource.has_value());
    auto hostSource =
        typedChartSource("valid/chart_v4_static_migration.json", "cfu-e3-host", unusedProvider());
    REQUIRE(hostSource.has_value());
    auto memorySource = typedChartSource("valid/chart_v4_static_migration.json", "cfu-e3-memory",
                                         emptyMemoryProvider());
    REQUIRE(memorySource.has_value());
    auto filesystemSource =
        cuexis::playback::PlaybackSource::fromFilesystemProject(fixture("static_project"));
    REQUIRE(filesystemSource.has_value());
    const auto package = fixture("golden/cxc_v1_v4_static.cxc");
    auto cxcFileSource = cuexis::playback::PlaybackSource::fromCxcFile(package);
    REQUIRE(cxcFileSource.has_value());
    auto cxcMemorySource = cuexis::playback::PlaybackSource::fromCxcMemory(readBytes(package));
    REQUIRE(cxcMemorySource.has_value());

    const auto text = prepareView(std::move(*textSource));
    const auto host = prepareView(std::move(*hostSource));
    const auto memory = prepareView(std::move(*memorySource));
    const auto filesystem = prepareView(std::move(*filesystemSource));
    const auto cxcFile = prepareView(std::move(*cxcFileSource));
    const auto cxcMemory = prepareView(std::move(*cxcMemorySource));

    CHECK(text.identity == host.identity);
    CHECK(text.identity == memory.identity);
    CHECK(text.identity == filesystem.identity);
    CHECK(text.identity == cxcFile.identity);
    CHECK(text.identity == cxcMemory.identity);
    CHECK(framesEquivalent(text.frame, host.frame));
    CHECK(framesEquivalent(text.frame, memory.frame));
    CHECK(framesEquivalent(text.frame, filesystem.frame));
    CHECK(framesEquivalent(text.frame, cxcFile.frame));
    CHECK(framesEquivalent(text.frame, cxcMemory.frame));
}

TEST_CASE("Parameterized v4 identity and FrameDigest follow frozen parameter values",
          "[playback][identity][digest][cfu-e3][cfu-e4]") {
    const auto parameterized = readText(fixture("valid/chart_v4_parameterized_transform.json"));
    auto defaultText = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(defaultText.has_value());
    auto defaultFilesystem =
        cuexis::playback::PlaybackSource::fromFilesystemProject(fixture("parameterized_project"));
    REQUIRE(defaultFilesystem.has_value());
    const auto defaultIdentity = loadIdentity(std::move(*defaultText));
    CHECK(loadIdentity(std::move(*defaultFilesystem)) == defaultIdentity);

    auto overrideText = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(overrideText.has_value());
    const auto overrideIdentity =
        loadIdentity(std::move(*overrideText), overrideOptions(-4.0, 2.0, 75.0));
    CHECK(overrideIdentity != defaultIdentity);

    cuexis::playback::PlaybackSession session;
    auto firstSource = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(firstSource.has_value());
    REQUIRE(session
                .load(std::move(*firstSource), cuexis::playback::PlaybackMode::ChartClock,
                      overrideOptions(3.0, 1.0, 60.0))
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto firstFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(firstFrame.has_value());
    const auto firstDigest =
        cuexis::playback::computeFrameDigest({.chartTimeMs = 0.0}, *firstFrame);
    const auto firstRepeat =
        cuexis::playback::computeFrameDigest({.chartTimeMs = 0.0}, *firstFrame);
    REQUIRE(firstDigest.has_value());
    REQUIRE(firstRepeat.has_value());
    CHECK(firstDigest->algorithmVersion == 3U);
    CHECK(firstDigest->value == firstRepeat->value);
    const auto beforeReload = session.semanticIdentity();
    REQUIRE(beforeReload.has_value());

    CHECK_FALSE(session
                    .reload(readText(fixture("valid/chart_v4_animation.json")),
                            {.chartTimeMs = 0.0}, cuexis::playback::ReloadPolicy::KeepChartTime)
                    .has_value());
    const auto afterFailure = session.semanticIdentity();
    REQUIRE(afterFailure.has_value());
    CHECK(*afterFailure == *beforeReload);
    auto failedFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(failedFrame.has_value());
    CHECK(framesEquivalent(*failedFrame, *firstFrame));

    auto replacement = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(replacement.has_value());
    REQUIRE(session
                .reload(std::move(*replacement), {.chartTimeMs = 0.0},
                        cuexis::playback::ReloadPolicy::KeepChartTime,
                        overrideOptions(7.0, 1.0, 60.0))
                .has_value());
    const auto afterSuccess = session.semanticIdentity();
    REQUIRE(afterSuccess.has_value());
    CHECK(*afterSuccess != *beforeReload);
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto secondFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(secondFrame.has_value());
    const auto secondDigest =
        cuexis::playback::computeFrameDigest({.chartTimeMs = 0.0}, *secondFrame);
    REQUIRE(secondDigest.has_value());
    CHECK(secondDigest->algorithmVersion == 3U);
    CHECK(secondDigest->value != firstDigest->value);
}

TEST_CASE("Reduced rational parameter inputs share one prepared semantic identity",
          "[playback][identity][parameters][cfu-e3]") {
    const auto chart = readText(fixture("valid/chart_v4_parameterized_rational.json"));
    auto firstSource = cuexis::playback::PlaybackSource::fromChartText(chart);
    REQUIRE(firstSource.has_value());
    auto secondSource = cuexis::playback::PlaybackSource::fromChartText(chart);
    REQUIRE(secondSource.has_value());
    auto thirdSource = cuexis::playback::PlaybackSource::fromChartText(chart);
    REQUIRE(thirdSource.has_value());

    const auto reduced = loadIdentity(std::move(*firstSource), rationalOptions(2, 2, 0.5));
    CHECK(loadIdentity(std::move(*secondSource), rationalOptions(1, 1, 0.5)) == reduced);
    CHECK(loadIdentity(std::move(*thirdSource), rationalOptions(2, 1, 0.5)) != reduced);
}

TEST_CASE("Successful v1, v2 and v3 prepare each expose a semantic identity",
          "[playback][identity][legacy][cfu-e3]") {
    auto v1Source = cuexis::playback::PlaybackSource::fromChartText(std::string{v1Chart});
    REQUIRE(v1Source.has_value());
    CHECK(loadIdentity(std::move(*v1Source)).sha256 !=
          cuexis::playback::PreparedSemanticIdentity{}.sha256);

    auto v2Source = cuexis::playback::PlaybackSource::fromFilesystemProject(
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage1d_project");
    REQUIRE(v2Source.has_value());
    cuexis::playback::PlaybackSession v2Session;
    auto v2Prepared =
        v2Session.prepareLoad(std::move(*v2Source), cuexis::playback::PlaybackMode::HostClock);
    REQUIRE(v2Prepared.has_value());
    REQUIRE(v2Prepared->semanticIdentity().has_value());
    REQUIRE(v2Session.commit(std::move(*v2Prepared)).has_value());
    REQUIRE(v2Session.semanticIdentity().has_value());

    auto v3Source = cuexis::playback::PlaybackSource::fromChartText(
        readText(fixture("valid/chart_v3_static_migration.json")));
    REQUIRE(v3Source.has_value());
    auto v4Source = cuexis::playback::PlaybackSource::fromChartText(
        readText(fixture("valid/chart_v4_static_migration.json")));
    REQUIRE(v4Source.has_value());
    CHECK(loadIdentity(std::move(*v3Source)) != loadIdentity(std::move(*v4Source)));
}

TEST_CASE("Animation CXC reload is rejected before World publish and keeps the active identity",
          "[playback][identity][capability][cxc][cfu-e4]") {
    auto initial =
        cuexis::playback::PlaybackSource::fromFilesystemProject(fixture("static_project"));
    REQUIRE(initial.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*initial), cuexis::playback::PlaybackMode::ChartClock).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto before = session.semanticIdentity();
    auto beforeFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(before.has_value());
    REQUIRE(beforeFrame.has_value());

    auto animation = cuexis::playback::PlaybackSource::fromCxcMemory(
        readBytes(fixture("golden/cxc_v1_v4_cxt.cxc")));
    REQUIRE(animation.has_value());
    const auto failed = session.reload(std::move(*animation), {.chartTimeMs = 0.0},
                                       cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.capability.preflight_failed");
    const auto after = session.semanticIdentity();
    REQUIRE(after.has_value());
    CHECK(*after == *before);
    auto afterFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(afterFrame.has_value());
    CHECK(framesEquivalent(*afterFrame, *beforeFrame));
}