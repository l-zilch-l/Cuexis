#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/simple_chart_importer.hpp>
#include <cuexis/chart/uuid.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace {

bool hasDiagnostic(const cuexis::core::Diagnostics& diagnostics, std::string_view code,
                   std::string_view path = {}) {
    return std::any_of(diagnostics.items().begin(), diagnostics.items().end(),
                       [code, path](const cuexis::core::Diagnostic& diagnostic) {
                           return diagnostic.code() == code &&
                                  (path.empty() || diagnostic.fieldPath() == path);
                       });
}

bool hasDiagnosticContext(const cuexis::core::Diagnostics& diagnostics, std::string_view code,
                          std::string_view path, std::string_view key, std::string_view value) {
    return std::any_of(diagnostics.items().begin(), diagnostics.items().end(),
                       [=](const cuexis::core::Diagnostic& diagnostic) {
                           return diagnostic.code() == code && diagnostic.fieldPath() == path &&
                                  std::any_of(
                                      diagnostic.context().begin(), diagnostic.context().end(),
                                      [=](const cuexis::core::DiagnosticContext& context) {
                                          return context.key == key && context.value == value;
                                      });
                       });
}

std::size_t countDiagnostics(const cuexis::core::Diagnostics& diagnostics, std::string_view code) {
    return static_cast<std::size_t>(std::count_if(
        diagnostics.items().begin(), diagnostics.items().end(),
        [code](const cuexis::core::Diagnostic& diagnostic) { return diagnostic.code() == code; }));
}

constexpr std::string_view simpleChart = R"json(
{
  "format":"cuexis.chart.simple",
  "version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000001",
  "metadata":{"title":"Simple"},
  "timing":{"offsetMs":25,"bpm":120},
  "templates":{
    "note.standard":{
      "kind":"note",
      "transform":{"scale":[2,2,2]},
      "render":{"mesh":"asset:mesh.note","material":"asset:material.note"}
    }
  },
  "behaviors":{},
  "objects":{
    "lane.main":{"kind":"element","transform":{"rotationDeg":[90,0,0]}},
    "note.one":{
      "template":"template:note.standard",
      "parent":"object:lane.main",
      "beat":"1.25",
      "transform":{"position":[1,2,3]}
    }
  },
  "extensions":{}
}
)json";

} // namespace

TEST_CASE("Simple importer deterministically converts IDs beat Euler and template merge",
          "[chart][simple]") {
    const auto imported = cuexis::chart::SimpleChartImporter::import(simpleChart);
    REQUIRE(imported.hasValue());
    REQUIRE(imported.document.has_value());

    const auto laneId =
        cuexis::chart::uuidV5("019b0000-0000-7abc-8def-000000000001", "object:lane.main");
    const auto noteId =
        cuexis::chart::uuidV5("019b0000-0000-7abc-8def-000000000001", "object:note.one");
    REQUIRE(laneId.has_value());
    REQUIRE(noteId.has_value());

    const auto lane =
        std::find_if(imported.document->objects.begin(), imported.document->objects.end(),
                     [&](const auto& object) { return object.id.value == *laneId; });
    const auto note =
        std::find_if(imported.document->objects.begin(), imported.document->objects.end(),
                     [&](const auto& object) { return object.id.value == *noteId; });
    REQUIRE(lane != imported.document->objects.end());
    REQUIRE(note != imported.document->objects.end());
    REQUIRE(lane->components.transform.has_value());
    CHECK(lane->components.transform->rotation.x == Catch::Approx(0.70710677F));
    CHECK(lane->components.transform->rotation.y == Catch::Approx(0.0F));
    CHECK(lane->components.transform->rotation.z == Catch::Approx(0.0F));
    CHECK(lane->components.transform->rotation.w == Catch::Approx(0.70710677F));

    REQUIRE(note->parent.has_value());
    CHECK(note->parent->value == *laneId);
    REQUIRE(note->sourceTemplate.has_value());
    REQUIRE(note->components.note.has_value());
    REQUIRE(note->components.note->beat.has_value());
    CHECK(note->components.note->beat->numerator() == 5);
    CHECK(note->components.note->beat->denominator() == 4);
    REQUIRE(note->components.transform.has_value());
    CHECK(note->components.transform->position.x == Catch::Approx(1.0F));
    CHECK(note->components.transform->scale.x == Catch::Approx(2.0F));
    REQUIRE(note->components.renderable.has_value());
    CHECK(note->components.renderable->mesh.value == "mesh.note");
}

TEST_CASE("Simple importer output and runtime are independent of object key order",
          "[chart][simple][determinism]") {
    constexpr std::string_view first = R"json(
{"format":"cuexis.chart.simple","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"bpm":120},"templates":{},"behaviors":{},
 "objects":{"z":{"kind":"element"},"a":{"kind":"element"}},"extensions":{}}
)json";
    constexpr std::string_view second = R"json(
{"format":"cuexis.chart.simple","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"bpm":120},"templates":{},"behaviors":{},
 "objects":{"a":{"kind":"element"},"z":{"kind":"element"}},"extensions":{}}
)json";
    const auto firstDocument = cuexis::chart::SimpleChartImporter::import(first);
    const auto secondDocument = cuexis::chart::SimpleChartImporter::import(second);
    REQUIRE(firstDocument.hasValue());
    REQUIRE(secondDocument.hasValue());
    const auto firstRuntime = cuexis::chart::ChartCompiler::compile(*firstDocument.document);
    const auto secondRuntime = cuexis::chart::ChartCompiler::compile(*secondDocument.document);
    REQUIRE(firstRuntime.hasValue());
    REQUIRE(secondRuntime.hasValue());
    REQUIRE(firstRuntime.runtime->objects.size() == secondRuntime.runtime->objects.size());
    for (std::size_t index = 0; index < firstRuntime.runtime->objects.size(); ++index) {
        CHECK(firstRuntime.runtime->objects[index].id.value ==
              secondRuntime.runtime->objects[index].id.value);
    }
}

TEST_CASE("Simple importer preserves unknown fields and reports the original pointer",
          "[chart][simple][unknown]") {
    constexpr std::string_view chart = R"json(
{"format":"cuexis.chart.simple","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"bpm":120},"templates":{},"behaviors":{},
 "objects":{"lane":{"kind":"element","futureValue":42}},
 "extensions":{},"futureTop":true}
)json";
    const auto imported = cuexis::chart::SimpleChartImporter::import(chart);
    REQUIRE(imported.hasValue());
    CHECK(hasDiagnostic(imported.diagnostics, "chart.simple.field.unknown",
                        "$/objects/lane/futureValue"));
    CHECK(hasDiagnostic(imported.diagnostics, "chart.simple.field.unknown", "$/futureTop"));
    CHECK(imported.document->extensions.canonicalText.find("cuexis.simple.unknown") !=
          std::string::npos);
    REQUIRE(imported.document->objects.size() == 1);
    CHECK(imported.document->objects[0].extensions.canonicalText.find("futureValue") !=
          std::string::npos);
    const auto laneId =
        cuexis::chart::uuidV5("019b0000-0000-7abc-8def-000000000001", "object:lane");
    REQUIRE(laneId.has_value());
    CHECK(hasDiagnosticContext(imported.diagnostics, "chart.simple.field.unknown",
                               "$/objects/lane/futureValue", "canonical_id", *laneId));
}

TEST_CASE("Simple importer rejects unknown fields in Behavior subtrees",
          "[chart][simple][behavior][unknown]") {
    constexpr std::string_view chart = R"json(
{"format":"cuexis.chart.simple","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":5,"bpm":150,"futureTiming":{"swing":0.5}},
 "templates":{
   "element.standard":{
     "kind":"element",
     "render":{
       "mesh":"asset:mesh.test","material":"asset:material.test",
       "futureRender":{"layer":3}
     }
   }
 },
 "behaviors":{
   "move":{
     "futureBehavior":true,
     "tracks":[{
       "property":"transform.position.z","futureTrack":"ignored",
       "keys":[{"beat":"0","value":1,"futureKey":[1,2,3]}]
     }]
   }
 },
 "objects":{
   "lane":{
     "template":"template:element.standard","behavior":"behavior:move",
     "transform":{"position":[1,2,3],"futureTransform":{"axis":"x"}}
   }
 },
 "extensions":{}}
)json";
    const auto imported = cuexis::chart::SimpleChartImporter::import(chart);
    REQUIRE_FALSE(imported.hasValue());
    CHECK_FALSE(imported.document.has_value());

    constexpr std::array unknownPaths{
        std::string_view{"$/timing/futureTiming"},
        std::string_view{"$/templates/element.standard/render/futureRender"},
        std::string_view{"$/behaviors/move/futureBehavior"},
        std::string_view{"$/behaviors/move/tracks/0/futureTrack"},
        std::string_view{"$/behaviors/move/tracks/0/keys/0/futureKey"},
        std::string_view{"$/objects/lane/transform/futureTransform"}};
    CHECK(countDiagnostics(imported.diagnostics, "chart.simple.field.unknown") ==
          unknownPaths.size());
    for (const auto path : unknownPaths) {
        if (path.find("/behaviors/") != std::string_view::npos) {
            CHECK(hasDiagnostic(imported.diagnostics, "chart.simple.field.unknown", path));
        }
    }
    CHECK(countDiagnostics(imported.diagnostics, "json.field.unknown") == 0);
}

TEST_CASE("Simple importer rejects invalid beats reference domains and missing templates",
          "[chart][simple][diagnostics]") {
    constexpr std::string_view chart = R"json(
{"format":"cuexis.chart.simple","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"bpm":120},"templates":{},"behaviors":{},
 "objects":{
   "note.bad":{"kind":"note","beat":"1/0","parent":"template:nope"},
   "note.missing":{"template":"template:missing","beat":"1"}
 },"extensions":{}}
)json";
    const auto imported = cuexis::chart::SimpleChartImporter::import(chart);
    REQUIRE_FALSE(imported.hasValue());
    CHECK(hasDiagnostic(imported.diagnostics, "chart.beat.invalid_denominator"));
    CHECK(hasDiagnostic(imported.diagnostics, "chart.simple.reference.domain_invalid"));
    CHECK(hasDiagnostic(imported.diagnostics, "chart.simple.template.missing"));
}
