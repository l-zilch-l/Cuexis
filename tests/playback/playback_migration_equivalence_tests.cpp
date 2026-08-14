#include <cuexis/chart/chart_migrator.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::array sampleTimesMs{-500.0, 0.0,    125.0,  250.0,  375.0, 500.0,
                                   750.0,  1000.0, 1100.0, 1250.0, 2000.0};

constexpr std::string_view v2Chart = R"json(
{
  "format":"cuexis.chart","version":2,
  "chartId":"019b0000-0000-7abc-8def-000000000501","metadata":{"title":"CFU-D3 v2 hop"},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "camera":{"type":"perspective","fovY":55,"near":0.1,"far":1000,
            "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,10]}},
  "templates":[],
  "behaviors":[{
    "id":"move","type":"behavior.transform.keyframe","version":1,
    "tracks":[
      {"property":"transform.position.x","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":-1.5},
        {"beat":{"numerator":2,"denominator":1},"value":1.5,"easing":"linear"}]},
      {"property":"camera.fovY","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":55},
        {"beat":{"numerator":2,"denominator":1},"value":75,"easing":"out_cubic"}]}
    ]
  }],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000510","name":"target","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.camera":{"version":1,"type":"perspective","fovY":55,"near":0.1,"far":1000},
      "cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"Could not read CFU-D3 fixture: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto sourcePath(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / relative;
}

[[nodiscard]] auto migrateToV3(std::string_view json) -> std::string {
    const auto migrated = cuexis::chart::ChartMigrator::migrateToV3(json);
    REQUIRE(migrated.hasValue());
    return migrated.artifact->chartJson;
}

[[nodiscard]] auto migrateToV4(std::string_view json) -> std::string {
    const auto migrated = cuexis::chart::ChartMigrator::migrateToV4(json);
    REQUIRE(migrated.hasValue());
    CHECK(migrated.artifact->report.targetVersion == 4U);
    CHECK(migrated.artifact->report.generatedClips == 0U);
    CHECK(migrated.artifact->report.generatedBindings == 0U);
    CHECK(migrated.artifact->report.generatedParameters == 0U);
    return migrated.artifact->chartJson;
}

[[nodiscard]] auto loadChartClock(std::string json)
    -> std::unique_ptr<cuexis::playback::PlaybackSession> {
    auto source = cuexis::playback::PlaybackSource::fromChartText(std::move(json));
    REQUIRE(source.has_value());
    auto session = std::make_unique<cuexis::playback::PlaybackSession>();
    REQUIRE(
        session->load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    return session;
}

struct SampledFrame final {
    cuexis::playback::RuntimeFrame frame{};
    cuexis::playback::FrameSnapshot snapshot{};
    cuexis::playback::FrameDigest digest{};
};

[[nodiscard]] auto sample(cuexis::playback::PlaybackSession& session, double chartTimeMs,
                          std::uint64_t discontinuity) -> SampledFrame {
    const cuexis::playback::RuntimeFrame frame{.chartTimeMs = chartTimeMs,
                                               .timeDiscontinuityId = discontinuity};
    REQUIRE(session.update(frame).has_value());
    auto snapshot = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(snapshot.has_value());
    auto digest = cuexis::playback::computeFrameDigest(frame, *snapshot);
    REQUIRE(digest.has_value());
    CHECK(digest->algorithmVersion == 3U);
    return {.frame = frame, .snapshot = std::move(*snapshot), .digest = *digest};
}

void checkSnapshotsBitEqual(const cuexis::playback::FrameSnapshot& first,
                            const cuexis::playback::FrameSnapshot& second) {
    REQUIRE(first.objects.size() == second.objects.size());
    for (std::size_t objectIndex = 0; objectIndex < first.objects.size(); ++objectIndex) {
        const auto& firstObject = first.objects[objectIndex];
        const auto& secondObject = second.objects[objectIndex];
        CHECK(firstObject.id == secondObject.id);
        CHECK(firstObject.hasTransform == secondObject.hasTransform);
        CHECK(firstObject.visible == secondObject.visible);
        CHECK(firstObject.materialAssetId == secondObject.materialAssetId);
        CHECK(firstObject.mesh == secondObject.mesh);
        CHECK(firstObject.material == secondObject.material);
        CHECK(firstObject.materialOpacity == secondObject.materialOpacity);
        for (std::size_t tintIndex = 0; tintIndex < 3; ++tintIndex) {
            CHECK(firstObject.materialTint[tintIndex] == secondObject.materialTint[tintIndex]);
        }
        for (std::size_t matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
            CHECK(firstObject.worldMatrix[matrixIndex] == secondObject.worldMatrix[matrixIndex]);
        }
    }

    CHECK(first.camera.active == second.camera.active);
    CHECK(first.camera.fovY == second.camera.fovY);
    CHECK(first.camera.nearPlane == second.camera.nearPlane);
    CHECK(first.camera.farPlane == second.camera.farPlane);
    CHECK(first.camera.pitch == second.camera.pitch);
    CHECK(first.camera.yaw == second.camera.yaw);
    CHECK(first.camera.roll == second.camera.roll);
    for (std::size_t matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
        CHECK(first.camera.viewMatrix[matrixIndex] == second.camera.viewMatrix[matrixIndex]);
        CHECK(first.camera.projectionMatrix[matrixIndex] ==
              second.camera.projectionMatrix[matrixIndex]);
    }
    CHECK(first.clearRed == second.clearRed);
    CHECK(first.clearGreen == second.clearGreen);
    CHECK(first.clearBlue == second.clearBlue);
    CHECK(first.clearAlpha == second.clearAlpha);
    CHECK(first.viewportWidth == second.viewportWidth);
    CHECK(first.viewportHeight == second.viewportHeight);
}

void checkSnapshotsNear(const cuexis::playback::FrameSnapshot& first,
                        const cuexis::playback::FrameSnapshot& second) {
    constexpr double tolerance = 1e-6;
    REQUIRE(first.objects.size() == second.objects.size());
    for (std::size_t objectIndex = 0; objectIndex < first.objects.size(); ++objectIndex) {
        CHECK(first.objects[objectIndex].id == second.objects[objectIndex].id);
        CHECK(first.objects[objectIndex].hasTransform == second.objects[objectIndex].hasTransform);
        CHECK(first.objects[objectIndex].visible == second.objects[objectIndex].visible);
        for (std::size_t matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
            CHECK(first.objects[objectIndex].worldMatrix[matrixIndex] ==
                  Catch::Approx(second.objects[objectIndex].worldMatrix[matrixIndex])
                      .margin(tolerance));
        }
    }
    CHECK(first.camera.active == second.camera.active);
    CHECK(first.camera.fovY == Catch::Approx(second.camera.fovY).margin(tolerance));
    CHECK(first.camera.nearPlane == Catch::Approx(second.camera.nearPlane).margin(tolerance));
    CHECK(first.camera.farPlane == Catch::Approx(second.camera.farPlane).margin(tolerance));
    CHECK(first.camera.pitch == Catch::Approx(second.camera.pitch).margin(tolerance));
    CHECK(first.camera.yaw == Catch::Approx(second.camera.yaw).margin(tolerance));
    CHECK(first.camera.roll == Catch::Approx(second.camera.roll).margin(tolerance));
    for (std::size_t matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
        CHECK(first.camera.viewMatrix[matrixIndex] ==
              Catch::Approx(second.camera.viewMatrix[matrixIndex]).margin(tolerance));
        CHECK(first.camera.projectionMatrix[matrixIndex] ==
              Catch::Approx(second.camera.projectionMatrix[matrixIndex]).margin(tolerance));
    }
}

void expectFormatVersions(cuexis::playback::PlaybackSession& source,
                          cuexis::playback::PlaybackSession& lifted, std::uint32_t sourceVersion) {
    const auto sourceInfo = source.contentInfo();
    const auto liftedInfo = lifted.contentInfo();
    REQUIRE(sourceInfo.has_value());
    REQUIRE(liftedInfo.has_value());
    CHECK(sourceInfo->chartFormatVersion == sourceVersion);
    CHECK(liftedInfo->chartFormatVersion == 4U);
}

void compareBitEqualTrace(cuexis::playback::PlaybackSession& source,
                          cuexis::playback::PlaybackSession& lifted,
                          std::span<const double> times) {
    std::uint64_t discontinuity = 1;
    for (const double chartTimeMs : times) {
        INFO("chartTimeMs=" << chartTimeMs);
        const auto left = sample(source, chartTimeMs, discontinuity);
        const auto right = sample(lifted, chartTimeMs, discontinuity);
        ++discontinuity;
        checkSnapshotsBitEqual(left.snapshot, right.snapshot);
        CHECK(left.digest.value == right.digest.value);
    }

    const auto seekOut = sample(source, 400.0, discontinuity);
    const auto seekOutLifted = sample(lifted, 400.0, discontinuity);
    ++discontinuity;
    checkSnapshotsBitEqual(seekOut.snapshot, seekOutLifted.snapshot);
    CHECK(seekOut.digest.value == seekOutLifted.digest.value);

    const auto seekBack = sample(source, 100.0, discontinuity);
    const auto seekBackLifted = sample(lifted, 100.0, discontinuity);
    checkSnapshotsBitEqual(seekBack.snapshot, seekBackLifted.snapshot);
    CHECK(seekBack.digest.value == seekBackLifted.digest.value);
}

void compareNearTrace(cuexis::playback::PlaybackSession& source,
                      cuexis::playback::PlaybackSession& lifted, std::span<const double> times) {
    std::uint64_t discontinuity = 1;
    for (const double chartTimeMs : times) {
        INFO("chartTimeMs=" << chartTimeMs);
        const auto left = sample(source, chartTimeMs, discontinuity);
        const auto right = sample(lifted, chartTimeMs, discontinuity);
        ++discontinuity;
        checkSnapshotsNear(left.snapshot, right.snapshot);
    }

    const auto seekOut = sample(source, 400.0, discontinuity);
    const auto seekOutLifted = sample(lifted, 400.0, discontinuity);
    ++discontinuity;
    checkSnapshotsNear(seekOut.snapshot, seekOutLifted.snapshot);

    const auto seekBack = sample(source, 100.0, discontinuity);
    const auto seekBackLifted = sample(lifted, 100.0, discontinuity);
    checkSnapshotsNear(seekBack.snapshot, seekBackLifted.snapshot);
}

} // namespace

TEST_CASE("Static v3 lift matches FrameSnapshot and FrameDigest v3",
          "[playback][migration][equivalence][cfu-d3]") {
    const auto sourceJson = readText(
        sourcePath("tests/fixtures/chart_format_update/valid/chart_v3_static_migration.json"));
    auto source = loadChartClock(sourceJson);
    auto lifted = loadChartClock(migrateToV4(sourceJson));
    expectFormatVersions(*source, *lifted, 3);
    compareBitEqualTrace(*source, *lifted, sampleTimesMs);
}

TEST_CASE("v3 Behavior and Stop lift matches seek and stop samples",
          "[playback][migration][equivalence][seek][stop][cfu-d3]") {
    const auto sourceJson = readText(sourcePath("assets/charts/stage2_example.cuexis.chart.json"));
    auto source = loadChartClock(sourceJson);
    auto lifted = loadChartClock(migrateToV4(sourceJson));
    expectFormatVersions(*source, *lifted, 3);
    compareBitEqualTrace(*source, *lifted, sampleTimesMs);
}

TEST_CASE("v1 Quaternion hop to v4 stays within the migration error budget",
          "[playback][migration][equivalence][v1][cfu-d3]") {
    const auto sourceJson =
        readText(sourcePath("tests/fixtures/stage2_migration_v1.cuexis.chart.json"));
    auto source = loadChartClock(sourceJson);
    auto lifted = loadChartClock(migrateToV4(sourceJson));
    expectFormatVersions(*source, *lifted, 1);
    compareNearTrace(*source, *lifted, sampleTimesMs);
}

TEST_CASE("v1 v3 hop and v4 lift produce identical Playback frames",
          "[playback][migration][equivalence][v1][cfu-d3]") {
    const auto sourceJson =
        readText(sourcePath("tests/fixtures/stage2_migration_v1.cuexis.chart.json"));
    auto hopped = loadChartClock(migrateToV3(sourceJson));
    auto lifted = loadChartClock(migrateToV4(sourceJson));
    expectFormatVersions(*hopped, *lifted, 3);
    compareBitEqualTrace(*hopped, *lifted, sampleTimesMs);
}

TEST_CASE("v2 hop lift matches FrameSnapshot and FrameDigest v3",
          "[playback][migration][equivalence][v2][cfu-d3]") {
    auto source = loadChartClock(std::string{v2Chart});
    auto hopped = loadChartClock(migrateToV3(v2Chart));
    auto lifted = loadChartClock(migrateToV4(v2Chart));
    expectFormatVersions(*source, *lifted, 2);
    expectFormatVersions(*hopped, *lifted, 3);
    compareNearTrace(*source, *lifted, sampleTimesMs);
    compareBitEqualTrace(*hopped, *lifted, sampleTimesMs);
}