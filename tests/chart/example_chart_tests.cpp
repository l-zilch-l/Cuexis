#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/math.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open example chart: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::size_t hierarchyDepth(const cuexis::chart::ChartRuntime& runtime, std::size_t index) {
    std::size_t depth = 0;
    std::vector<bool> visited(runtime.objects.size(), false);
    while (runtime.objects[index].parentIndex) {
        REQUIRE(*runtime.objects[index].parentIndex < runtime.objects.size());
        REQUIRE_FALSE(visited[index]);
        visited[index] = true;
        index = *runtime.objects[index].parentIndex;
        ++depth;
    }
    return depth;
}

void checkExample(const std::filesystem::path& path) {
    const auto loaded = cuexis::chart::ChartLoader::load(readFile(path));
    REQUIRE(loaded.hasValue());
    const auto compiled = cuexis::chart::ChartCompiler::compile(*loaded.document);
    REQUIRE(compiled.hasValue());
    REQUIRE(compiled.runtime->objects.size() == 3);

    std::size_t maximumDepth = 0;
    bool foundTemplateNote = false;
    for (std::size_t index = 0; index < compiled.runtime->objects.size(); ++index) {
        maximumDepth = std::max(maximumDepth, hierarchyDepth(*compiled.runtime, index));
        const auto& components = compiled.runtime->objects[index].components;
        CHECK_FALSE(components.renderable.has_value());
        if (components.transform) {
            CHECK(std::abs(components.transform->position.x) <= 0.7F);
            CHECK(std::abs(components.transform->position.y) <= 0.7F);
            CHECK(std::abs(components.transform->position.z) <= 0.5F);
        }
        if (components.note) {
            REQUIRE(components.note->beat.has_value());
            foundTemplateNote = true;
        }
    }
    CHECK(maximumDepth == 2);
    CHECK(foundTemplateNote);
}

void checkCanonicalStage1AGolden(const cuexis::chart::ChartRuntime& runtime) {
    REQUIRE(runtime.objects.size() == 3);

    const auto& lane = runtime.objects[0];
    CHECK(lane.id.value == "019b0000-0000-7abc-8def-000000000010");
    CHECK_FALSE(lane.parentIndex.has_value());
    REQUIRE(lane.components.transform.has_value());
    CHECK(lane.components.transform->position == cuexis::core::Vec3{-0.3F, 0.0F, 0.0F});

    const auto& note = runtime.objects[1];
    CHECK(note.id.value == "019b0000-0000-7abc-8def-000000000011");
    REQUIRE(note.parentIndex.has_value());
    CHECK(*note.parentIndex == 0);
    REQUIRE(note.components.transform.has_value());
    CHECK(note.components.transform->position == cuexis::core::Vec3{0.3F, 0.0F, 0.0F});
    REQUIRE(note.components.note.has_value());
    REQUIRE(note.components.note->beat.has_value());
    CHECK(note.components.note->beat->toString() == "4/1");

    const auto& marker = runtime.objects[2];
    CHECK(marker.id.value == "019b0000-0000-7abc-8def-000000000012");
    REQUIRE(marker.parentIndex.has_value());
    CHECK(*marker.parentIndex == 1);
    REQUIRE(marker.components.transform.has_value());
    CHECK(marker.components.transform->position == cuexis::core::Vec3{0.0F, 0.3F, 0.0F});
    CHECK(marker.components.transform->scale == cuexis::core::Vec3{0.25F, 0.25F, 0.25F});
}

} // namespace

TEST_CASE("Canonical stage 1A example loads and compiles into a visible three-object hierarchy",
          "[chart][example]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                      "stage1a_example.cuexis.chart.json";
    checkExample(path);

    const auto loaded = cuexis::chart::ChartLoader::load(readFile(path));
    REQUIRE(loaded.hasValue());
    const auto compiled = cuexis::chart::ChartCompiler::compile(*loaded.document);
    REQUIRE(compiled.hasValue());
    checkCanonicalStage1AGolden(*compiled.runtime);
}

TEST_CASE("Stage 1B project fixture exposes three typed renderable asset references",
          "[chart][example][stage1b]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "stage1b_project" / "assets" / "charts" / "stage1b_example.cuexis.chart.json";
    const auto loaded = cuexis::chart::ChartLoader::load(readFile(path));
    REQUIRE(loaded.hasValue());

    const auto compiled = cuexis::chart::ChartCompiler::compile(*loaded.document);
    REQUIRE(compiled.hasValue());
    REQUIRE(compiled.runtime->objects.size() == 4);

    std::size_t renderableCount = 0;
    std::size_t cameraCount = 0;
    for (const auto& object : compiled.runtime->objects) {
        REQUIRE(object.components.transform.has_value());
        if (object.components.renderable.has_value()) {
            ++renderableCount;
            CHECK(object.components.renderable->mesh.value == "mesh.note");
            CHECK(object.components.renderable->material.value == "material.basic");
        }
        if (object.components.camera.has_value()) {
            ++cameraCount;
            CHECK(object.components.camera->type == "perspective");
            CHECK(object.components.camera->fovY == 45.0);
        }
    }
    CHECK(renderableCount == 3);
    CHECK(cameraCount == 1);
}

TEST_CASE("Canonical Stage 2 example loads and compiles through the v3 route",
          "[chart][example][stage2]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                      "stage2_example.cuexis.chart.json";
    const auto loaded = cuexis::chart::ChartLoader::load(readFile(path));
    REQUIRE(loaded.hasValue());
    REQUIRE(loaded.document->version == 3);
    REQUIRE(loaded.document->timing.tempoEvents.size() == 2);
    REQUIRE(loaded.document->timing.stops.size() == 1);
    REQUIRE(loaded.document->behaviors.size() == 1);
    REQUIRE(loaded.document->behaviors[0].events.size() == 2);

    const auto compiled = cuexis::chart::ChartCompiler::compile(*loaded.document);
    REQUIRE(compiled.hasValue());
    CHECK(compiled.runtime->version == 3);
    REQUIRE(compiled.runtime->behaviors.size() == 1);
    CHECK(compiled.runtime->behaviors[0].eventTracks.size() == 2);
}
