#include <cuexis/chart/chart_runtime.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

cuexis::chart::ChartObject makeElement(std::string id, std::optional<std::string> parent = {}) {
    cuexis::chart::ObjectComponents components;
    components.element = true;
    return cuexis::chart::ChartObject{
        cuexis::chart::ChartObjectId{std::move(id)},
        std::nullopt,
        parent ? std::optional<cuexis::chart::ChartObjectId>{cuexis::chart::ChartObjectId{
                     std::move(*parent)}}
               : std::nullopt,
        std::nullopt,
        std::move(components),
        {},
    };
}

cuexis::chart::ChartDocument makeDocument(std::vector<cuexis::chart::ChartObject> objects) {
    return cuexis::chart::ChartDocument{
        cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"},
        {},
        cuexis::chart::ChartTiming{0.0, 120.0},
        {},
        {},
        {},
        std::move(objects),
        {},
    };
}

bool hasDiagnostic(const cuexis::core::Diagnostics& diagnostics, std::string_view code) {
    return std::any_of(
        diagnostics.items().begin(), diagnostics.items().end(),
        [code](const cuexis::core::Diagnostic& diagnostic) { return diagnostic.code() == code; });
}

std::string indexedId(std::string_view prefix, std::size_t index, std::size_t width) {
    auto digits = std::to_string(index);
    return std::string{prefix} + std::string(width - digits.size(), '0') + digits;
}

} // namespace

TEST_CASE("ChartRuntime object order and parent indices are independent of input order",
          "[chart][runtime][determinism]") {
    const std::string parentId = "019b0000-0000-7abc-8def-000000000020";
    const std::string childId = "019b0000-0000-7abc-8def-000000000010";
    auto first = makeDocument(
        {makeElement(parentId), makeElement(childId, std::optional<std::string>{parentId})});
    auto second = makeDocument(
        {makeElement(childId, std::optional<std::string>{parentId}), makeElement(parentId)});

    const auto firstRuntime = cuexis::chart::ChartCompiler::compile(first);
    const auto secondRuntime = cuexis::chart::ChartCompiler::compile(second);
    REQUIRE(firstRuntime.hasValue());
    REQUIRE(secondRuntime.hasValue());
    REQUIRE(firstRuntime.runtime->objects.size() == 2);
    REQUIRE(secondRuntime.runtime->objects.size() == 2);

    CHECK(firstRuntime.runtime->objects[0].id.value == childId);
    CHECK(firstRuntime.runtime->objects[1].id.value == parentId);
    REQUIRE(firstRuntime.runtime->objects[0].parentIndex.has_value());
    CHECK(*firstRuntime.runtime->objects[0].parentIndex == 1);
    CHECK_FALSE(firstRuntime.runtime->objects[1].parentIndex.has_value());
    CHECK(secondRuntime.runtime->objects[0].id.value == firstRuntime.runtime->objects[0].id.value);
    CHECK(secondRuntime.runtime->objects[0].parentIndex ==
          firstRuntime.runtime->objects[0].parentIndex);
}

TEST_CASE("ChartRuntime skips a missing-parent subtree with a warning",
          "[chart][runtime][hierarchy]") {
    const std::string missingId = "019b0000-0000-7abc-8def-000000000099";
    const std::string skippedRootId = "019b0000-0000-7abc-8def-000000000020";
    const std::string skippedChildId = "019b0000-0000-7abc-8def-000000000021";
    auto document =
        makeDocument({makeElement("019b0000-0000-7abc-8def-000000000010"),
                      makeElement(skippedRootId, std::optional<std::string>{missingId}),
                      makeElement(skippedChildId, std::optional<std::string>{skippedRootId})});

    const auto runtime = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE(runtime.hasValue());
    REQUIRE(runtime.runtime->objects.size() == 1);
    CHECK(runtime.runtime->objects[0].id.value == "019b0000-0000-7abc-8def-000000000010");
    CHECK(hasDiagnostic(runtime.diagnostics, "chart.hierarchy.parent_missing"));
}

TEST_CASE("ChartRuntime rejects hierarchy cycles without publishing partial data",
          "[chart][runtime][hierarchy]") {
    const std::string firstId = "019b0000-0000-7abc-8def-000000000010";
    const std::string secondId = "019b0000-0000-7abc-8def-000000000011";
    auto document = makeDocument({makeElement(firstId, std::optional<std::string>{secondId}),
                                  makeElement(secondId, std::optional<std::string>{firstId})});

    const auto runtime = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE_FALSE(runtime.hasValue());
    CHECK_FALSE(runtime.runtime.has_value());
    CHECK(hasDiagnostic(runtime.diagnostics, "chart.hierarchy.cycle"));
}

TEST_CASE("ChartRuntime validates deep hierarchies without recursive stack growth",
          "[chart][runtime][hierarchy][limits]") {
    constexpr std::size_t objectCount = 12000;
    std::vector<cuexis::chart::ChartObject> objects;
    objects.reserve(objectCount);
    for (std::size_t index = 0; index < objectCount; ++index) {
        const auto id = indexedId("object-", index, 5);
        const auto parent = index + 1 < objectCount
                                ? std::optional<std::string>{indexedId("object-", index + 1, 5)}
                                : std::nullopt;
        objects.push_back(makeElement(id, parent));
    }

    const auto runtime = cuexis::chart::ChartCompiler::compile(makeDocument(std::move(objects)));
    REQUIRE(runtime.hasValue());
    REQUIRE(runtime.runtime->objects.size() == objectCount);
    REQUIRE(runtime.runtime->objects.front().parentIndex.has_value());
    CHECK(*runtime.runtime->objects.front().parentIndex == 1);
    CHECK_FALSE(runtime.runtime->objects.back().parentIndex.has_value());
}

TEST_CASE("ChartRuntime validates behavior references and sorts behavior IDs",
          "[chart][runtime][behavior]") {
    auto document = makeDocument({makeElement("019b0000-0000-7abc-8def-000000000010")});
    document.behaviors.push_back(cuexis::chart::ChartBehavior{
        cuexis::chart::BehaviorId{"behavior.z"}, "behavior.transform.keyframe", 1, {"[]"}});
    document.behaviors.push_back(cuexis::chart::ChartBehavior{
        cuexis::chart::BehaviorId{"behavior.a"}, "behavior.transform.keyframe", 1, {"[]"}});
    document.objects[0].components.behavior =
        cuexis::chart::BehaviorReferenceData{cuexis::chart::BehaviorId{"behavior.a"}};

    const auto runtime = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE(runtime.hasValue());
    REQUIRE(runtime.runtime->behaviors.size() == 2);
    CHECK(runtime.runtime->behaviors[0].id.value == "behavior.a");
    CHECK(runtime.runtime->behaviors[1].id.value == "behavior.z");

    document.objects[0].components.behavior =
        cuexis::chart::BehaviorReferenceData{cuexis::chart::BehaviorId{"behavior.missing"}};
    const auto missing = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE_FALSE(missing.hasValue());
    CHECK(hasDiagnostic(missing.diagnostics, "chart.reference.behavior_missing"));
}

TEST_CASE("ChartRuntime preserves backend-neutral render AssetIds", "[chart][runtime][assets]") {
    auto document = makeDocument({makeElement("019b0000-0000-7abc-8def-000000000010")});
    document.objects[0].components.renderable =
        cuexis::chart::RenderableData{cuexis::chart::AssetId{"mesh.note.standard"},
                                      cuexis::chart::AssetId{"material.note.standard"}};

    const auto runtime = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE(runtime.hasValue());
    REQUIRE(runtime.runtime->objects[0].components.renderable.has_value());
    CHECK(runtime.runtime->objects[0].components.renderable->mesh.value == "mesh.note.standard");
    CHECK(runtime.runtime->objects[0].components.renderable->material.value ==
          "material.note.standard");
}

TEST_CASE("ChartRuntime validates programmatic default and object cameras",
          "[chart][runtime][camera]") {
    auto document = makeDocument({makeElement("019b0000-0000-7abc-8def-000000000010")});
    document.camera.nearPlane = 10.0;
    document.camera.farPlane = 1.0;
    document.objects[0].components.camera =
        cuexis::chart::CameraComponentData{"orthographic", 180.0, -1.0, 0.0};

    const auto runtime = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE_FALSE(runtime.hasValue());
    CHECK(hasDiagnostic(runtime.diagnostics, "chart.camera.unsupported_type"));
    CHECK(hasDiagnostic(runtime.diagnostics, "chart.camera.invalid_fov"));
    CHECK(hasDiagnostic(runtime.diagnostics, "chart.camera.invalid_near"));
    CHECK(hasDiagnostic(runtime.diagnostics, "chart.camera.invalid_far"));
    CHECK(hasDiagnostic(runtime.diagnostics, "chart.camera.near_exceeds_far"));
}

TEST_CASE("ChartRuntime preserves v2 main music and rejects audio on v1",
          "[chart][runtime][audio]") {
    auto v2 = makeDocument({makeElement("019b0000-0000-7abc-8def-000000000010")});
    v2.version = 2;
    v2.audio = cuexis::chart::ChartAudioData{1, cuexis::chart::AssetId{"audio.main"}};
    const auto compiled = cuexis::chart::ChartCompiler::compile(v2);
    REQUIRE(compiled.hasValue());
    CHECK(compiled.runtime->version == 2);
    REQUIRE(compiled.runtime->mainMusic.has_value());
    CHECK(compiled.runtime->mainMusic->value == "audio.main");

    v2.version = 1;
    const auto rejected = cuexis::chart::ChartCompiler::compile(v2);
    REQUIRE_FALSE(rejected.hasValue());
    CHECK(hasDiagnostic(rejected.diagnostics, "chart.audio.not_available_in_v1"));
}
