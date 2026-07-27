#include <cuexis/chart/canonical_chart_loader.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace {

bool hasDiagnostic(const cuexis::core::Diagnostics& diagnostics, std::string_view code,
                   std::string_view path = {}) {
    for (const auto& diagnostic : diagnostics.items()) {
        if (diagnostic.code() == code && (path.empty() || diagnostic.fieldPath() == path)) {
            return true;
        }
    }
    return false;
}

constexpr std::string_view minimalChart = R"json(
{
  "format": "cuexis.chart",
  "version": 1,
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {},
  "timing": {
    "offsetMs": 0.0,
    "defaultBpm": 120.0,
    "bpmChanges": [],
    "stops": []
  },
  "templates": [],
  "behaviors": [],
  "objects": [
    {
      "id": "019b0000-0000-7abc-8def-000000000010",
      "parent": null,
      "components": { "cuexis.element": { "version": 1 } },
      "extensions": {}
    }
  ],
  "requiredExtensions": [],
  "extensions": {}
}
)json";

std::string canonicalUuid(std::size_t index) {
    auto digits = std::to_string(index);
    return "019b0000-0000-7abc-8def-" + std::string(12 - digits.size(), '0') + digits;
}

} // namespace

TEST_CASE("Canonical loader produces a typed chart document", "[chart][canonical]") {
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(minimalChart);
    REQUIRE(loaded.hasValue());
    REQUIRE(loaded.document.has_value());
    CHECK(loaded.document->chartId.value == "019b0000-0000-7abc-8def-000000000001");
    CHECK(loaded.document->timing.defaultBpm == Catch::Approx(120.0));
    REQUIRE(loaded.document->objects.size() == 1);
    CHECK(loaded.document->objects[0].components.element);
}

TEST_CASE("Canonical loader reports deterministic paths for missing type and unknown fields",
          "[chart][canonical][diagnostics]") {
    constexpr std::string_view invalid = R"json(
{
  "format": "cuexis.chart",
  "version": "one",
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {},
  "timing": {"offsetMs": 0, "defaultBpm": 120, "bpmChanges": [], "stops": []},
  "templates": [],
  "behaviors": [],
  "objects": [],
  "requiredExtensions": [],
  "extensions": {},
  "futureCoreField": true
}
)json";
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(invalid);
    REQUIRE_FALSE(loaded.hasValue());
    CHECK(hasDiagnostic(loaded.diagnostics, "json.type.mismatch", "$/version"));
    CHECK(hasDiagnostic(loaded.diagnostics, "json.field.unknown", "$/futureCoreField"));
}

TEST_CASE("Canonical loader rejects unsupported versions timing events and required extensions",
          "[chart][canonical][unsupported]") {
    constexpr std::string_view invalid = R"json(
{
  "format": "cuexis.chart",
  "version": 3,
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {},
  "timing": {
    "offsetMs": 0,
    "defaultBpm": 120,
    "bpmChanges": [{"beat":{"numerator":4,"denominator":1},"bpm":180}],
    "stops": [{"beat":{"numerator":8,"denominator":1},"durationMs":250}]
  },
  "templates": [],
  "behaviors": [],
  "objects": [],
  "requiredExtensions": [{"id":"org.example.required","version":1}],
  "extensions": {}
}
)json";
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(invalid);
    REQUIRE_FALSE(loaded.hasValue());
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.version.unsupported", "$/version"));
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.timing.bpm_changes_unsupported",
                        "$/timing/bpmChanges"));
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.timing.stops_unsupported", "$/timing/stops"));
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.extension.required_unsupported",
                        "$/requiredExtensions/0"));
}

TEST_CASE("Canonical loader validates required extension entries before rejecting support",
          "[chart][canonical][extension][diagnostics]") {
    constexpr std::string_view invalid = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],"objects":[],
  "requiredExtensions":[
    {"id":"bad/id","version":1,"future":true},
    {"id":"org.example.zero","version":0},
    7
  ],
  "extensions":{}
}
)json";
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(invalid);
    REQUIRE_FALSE(loaded.hasValue());
    CHECK(hasDiagnostic(loaded.diagnostics, "json.field.unknown", "$/requiredExtensions/0/future"));
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.extension.id_invalid",
                        "$/requiredExtensions/0/id"));
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.extension.version_invalid",
                        "$/requiredExtensions/1/version"));
    CHECK(hasDiagnostic(loaded.diagnostics, "json.type.mismatch", "$/requiredExtensions/2"));
    CHECK_FALSE(hasDiagnostic(loaded.diagnostics, "chart.extension.required_unsupported"));
}

TEST_CASE("Canonical templates expand single inheritance and fixed-schema patches",
          "[chart][canonical][template]") {
    constexpr std::string_view chart = R"json(
{
  "format": "cuexis.chart",
  "version": 1,
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {},
  "timing": {"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates": [
    {
      "id":"019b0000-0000-7abc-8def-000000000020",
      "extends":null,
      "prototype":{"components":{
        "cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
        "cuexis.element":{"version":1}
      }},
      "extensions":{}
    },
    {
      "id":"019b0000-0000-7abc-8def-000000000021",
      "extends":{"domain":"template","id":"019b0000-0000-7abc-8def-000000000020"},
      "patch":[{"op":"replace","path":"/components/cuexis.transform/position","value":[2,3,4]}],
      "extensions":{}
    }
  ],
  "behaviors": [],
  "objects": [
    {
      "id":"019b0000-0000-7abc-8def-000000000010",
      "parent":null,
      "template":{"domain":"template","id":"019b0000-0000-7abc-8def-000000000021"},
      "overrides":[],
      "extensions":{}
    }
  ],
  "requiredExtensions":[],
  "extensions":{}
}
)json";
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(chart);
    REQUIRE(loaded.hasValue());
    REQUIRE(loaded.document->templates.size() == 2);
    REQUIRE(loaded.document->objects.size() == 1);
    REQUIRE(loaded.document->objects[0].components.transform.has_value());
    CHECK(loaded.document->objects[0].components.transform->position.x == Catch::Approx(2.0F));
    CHECK(loaded.document->objects[0].components.transform->position.y == Catch::Approx(3.0F));
    CHECK(loaded.document->objects[0].components.transform->position.z == Catch::Approx(4.0F));
}

TEST_CASE("Canonical loader preserves unknown optional extensions with warnings",
          "[chart][canonical][extension]") {
    constexpr std::string_view chart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000001",
  "metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],"objects":[],"requiredExtensions":[],
  "extensions":{"org.example.optional":{"version":1,"data":{"x":1}}}
}
)json";
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(chart);
    REQUIRE(loaded.hasValue());
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.extension.optional_unknown",
                        "$/extensions/org.example.optional"));
    CHECK(loaded.document->extensions.canonicalText.find("org.example.optional") !=
          std::string::npos);
}

TEST_CASE("Canonical loader detects template inheritance cycles", "[chart][canonical][template]") {
    constexpr std::string_view chart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[
    {"id":"019b0000-0000-7abc-8def-000000000020","extends":{"domain":"template","id":"019b0000-0000-7abc-8def-000000000021"},"patch":[],"extensions":{}},
    {"id":"019b0000-0000-7abc-8def-000000000021","extends":{"domain":"template","id":"019b0000-0000-7abc-8def-000000000020"},"patch":[],"extensions":{}}
  ],
  "behaviors":[],"objects":[],"requiredExtensions":[],"extensions":{}
}

)json";
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(chart);
    REQUIRE_FALSE(loaded.hasValue());
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.template.inheritance_cycle"));
}

TEST_CASE("Canonical loader exposes typed Chart v2 main music", "[chart][canonical][audio]") {
    constexpr std::string_view audioBlock = R"json(
  "audio":{"version":1,"mainMusic":{"domain":"asset","id":"audio.main"}},)json";
    auto chart = std::string{minimalChart};
    const auto version = chart.find("\"version\": 1");
    REQUIRE(version != std::string::npos);
    chart.replace(version, std::string_view{"\"version\": 1"}.size(), "\"version\": 2");
    const auto metadata = chart.find("\"metadata\": {},");
    REQUIRE(metadata != std::string::npos);
    chart.insert(metadata + std::string_view{"\"metadata\": {},"}.size(), audioBlock);

    const auto loaded = cuexis::chart::CanonicalChartLoader::load(chart);
    REQUIRE(loaded.hasValue());
    CHECK(loaded.document->version == 2);
    REQUIRE(loaded.document->audio.has_value());
    CHECK(loaded.document->audio->mainMusic.value == "audio.main");

    auto v1Chart = std::string{minimalChart};
    const auto v1Metadata = v1Chart.find("\"metadata\": {},");
    REQUIRE(v1Metadata != std::string::npos);
    v1Chart.insert(v1Metadata + std::string_view{"\"metadata\": {},"}.size(), audioBlock);
    const auto v1Audio = cuexis::chart::CanonicalChartLoader::load(v1Chart);
    REQUIRE_FALSE(v1Audio.hasValue());
    CHECK(hasDiagnostic(v1Audio.diagnostics, "json.field.unknown", "$/audio"));
}

TEST_CASE("Canonical loader rejects invalid object camera fields without default fallback",
          "[chart][canonical][camera][diagnostics]") {
    auto invalid = std::string{minimalChart};
    const auto component = invalid.find("{ \"cuexis.element\": { \"version\": 1 } }");
    REQUIRE(component != std::string::npos);
    invalid.replace(component,
                    std::string_view{"{ \"cuexis.element\": { \"version\": 1 } }"}.size(),
                    R"json({
        "cuexis.camera":{"version":1,"type":"orthographic","fovY":0,"near":2,"far":1}
      })json");

    const auto loaded = cuexis::chart::CanonicalChartLoader::load(invalid);
    REQUIRE_FALSE(loaded.hasValue());
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.camera.unsupported_type",
                        "$/objects/0/components/cuexis.camera/type"));
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.camera.invalid_fov",
                        "$/objects/0/components/cuexis.camera/fovY"));
    CHECK(hasDiagnostic(loaded.diagnostics, "chart.camera.near_exceeds_far",
                        "$/objects/0/components/cuexis.camera"));
}

TEST_CASE("Canonical loader expands the maximum template chain without recursive stack growth",
          "[chart][canonical][template][limits]") {
    constexpr std::size_t templateCount = 10000;
    std::string chart = R"json({
"format":"cuexis.chart","version":1,
"chartId":"019b0000-0000-7abc-8def-ffffffffffff","metadata":{},
"timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
"templates":[)json";
    chart.reserve(3U * 1024U * 1024U);
    for (std::size_t index = 0; index < templateCount; ++index) {
        if (index != 0) {
            chart.push_back(',');
        }
        chart += "{\"id\":\"" + canonicalUuid(index) + "\",\"extends\":";
        if (index + 1 == templateCount) {
            chart += "null,\"prototype\":{\"components\":{\"cuexis.element\":{\"version\":1}}}";
        } else {
            chart += "{\"domain\":\"template\",\"id\":\"" + canonicalUuid(index + 1) +
                     "\"},\"patch\":[]";
        }
        chart += ",\"extensions\":{}}";
    }
    chart += R"json(],"behaviors":[],"objects":[],"requiredExtensions":[],"extensions":{}})json";

    const auto loaded = cuexis::chart::CanonicalChartLoader::load(chart);
    REQUIRE(loaded.hasValue());
    REQUIRE(loaded.document->templates.size() == templateCount);
    CHECK(loaded.document->templates.front().prototype.element);
}
