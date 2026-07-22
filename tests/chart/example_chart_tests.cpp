#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>

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

} // namespace

TEST_CASE("Canonical stage 1A example loads and compiles into a visible three-object hierarchy",
          "[chart][example]") {
    checkExample(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                 "stage1a_example.cuexis.chart.json");
}

TEST_CASE("Simple stage 1A example imports and compiles into the same demo-safe shape",
          "[chart][example]") {
    checkExample(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                 "stage1a_example.cuexis.chart.simple.json");
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
