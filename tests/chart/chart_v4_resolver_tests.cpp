#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open resolver fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto fixture(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           relative;
}

[[nodiscard]] auto loadSource(std::string_view relative) -> cuexis::chart::ChartV4SourceDocument {
    const auto loaded = cuexis::chart::ChartV4Loader::load(readFile(fixture(relative)));
    if (!loaded.hasValue()) {
        throw std::runtime_error{"Chart v4 fixture did not load"};
    }
    return *loaded.document;
}

[[nodiscard]] auto hasDiagnostic(const cuexis::chart::ChartV4ResolveResult& result,
                                 std::string_view code) -> bool {
    return std::ranges::any_of(result.diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

} // namespace

TEST_CASE("Chart v4 resolver applies defaults and host parameter overrides",
          "[chart][v4][resolver][parameters][cfu-c2]") {
    const auto source = loadSource("valid/chart_v4_parameterized_transform.json");
    const auto defaults = cuexis::chart::ChartV4Resolver::resolve(source);
    REQUIRE(defaults.hasValue());
    CHECK(defaults.artifact->document.chart.camera.fovY == Catch::Approx(60.0));
    REQUIRE(defaults.artifact->document.chart.objects.size() == 1);
    REQUIRE(defaults.artifact->document.chart.objects.front().components.transform.has_value());
    const auto& defaultTransform =
        *defaults.artifact->document.chart.objects.front().components.transform;
    CHECK(defaultTransform.position.x == Catch::Approx(2.0F));
    CHECK(defaultTransform.scale.y == Catch::Approx(1.0F));

    const std::array inputs{
        cuexis::chart::ChartParameterInput{"camera.fov", cuexis::chart::ChartParameterType::Number,
                                           75.0},
        cuexis::chart::ChartParameterInput{"layout.scale-y",
                                           cuexis::chart::ChartParameterType::Number, 2.0},
        cuexis::chart::ChartParameterInput{"layout.x", cuexis::chart::ChartParameterType::Number,
                                           -0.0},
    };
    const auto overridden = cuexis::chart::ChartV4Resolver::resolve(source, inputs);
    REQUIRE(overridden.hasValue());
    CHECK(overridden.artifact->document.chart.camera.fovY == Catch::Approx(75.0));
    const auto& transform =
        *overridden.artifact->document.chart.objects.front().components.transform;
    CHECK(transform.position.x == Catch::Approx(0.0F));
    CHECK(transform.scale.y == Catch::Approx(2.0F));

    auto positiveZero = inputs;
    positiveZero.back().value = 0.0;
    const auto normalized = cuexis::chart::ChartV4Resolver::resolve(source, positiveZero);
    REQUIRE(normalized.hasValue());
    CHECK(normalized.artifact->parameterIdentity == overridden.artifact->parameterIdentity);
    CHECK(normalized.artifact->chartIdentity == defaults.artifact->chartIdentity);
}

TEST_CASE("Chart parameter resolver rejects unknown duplicate type range and missing input",
          "[chart][v4][resolver][parameters][failure][cfu-c2]") {
    const auto source = loadSource("valid/chart_v4_parameterized_transform.json");
    SECTION("unknown") {
        const std::array inputs{cuexis::chart::ChartParameterInput{
            "unknown", cuexis::chart::ChartParameterType::Number, 1.0}};
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source, inputs);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.parameter.unknown"));
    }
    SECTION("duplicate") {
        const std::array inputs{
            cuexis::chart::ChartParameterInput{"layout.x",
                                               cuexis::chart::ChartParameterType::Number, 1.0},
            cuexis::chart::ChartParameterInput{"layout.x",
                                               cuexis::chart::ChartParameterType::Number, 2.0},
        };
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source, inputs);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.parameter.duplicate"));
    }
    SECTION("type") {
        const auto rational = *cuexis::chart::RationalBeat::create(1, 1);
        const std::array inputs{cuexis::chart::ChartParameterInput{
            "layout.x", cuexis::chart::ChartParameterType::Rational, rational}};
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source, inputs);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.parameter.type_mismatch"));
    }
    SECTION("range") {
        const std::array inputs{cuexis::chart::ChartParameterInput{
            "camera.fov", cuexis::chart::ChartParameterType::Number, 180.0}};
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source, inputs);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.parameter.out_of_range"));
    }
    SECTION("missing") {
        constexpr std::string_view chart = R"({
          "format":"cuexis.chart","version":4,
          "chartId":"019f0000-0000-7abc-8def-0000000004f1","metadata":{},
          "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
          "parameters":[{"id":"required.value","type":"number","constraints":{}}],
          "templates":[],"behaviors":[],"animationTemplateImports":[],"animationClips":[],
          "objects":[],"requiredExtensions":[],"extensions":{}})";
        const auto loaded = cuexis::chart::ChartV4Loader::load(chart);
        REQUIRE(loaded.hasValue());
        const auto result = cuexis::chart::ChartV4Resolver::resolve(*loaded.document);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.parameter.missing"));
    }
}

TEST_CASE("CXT bindings lower to stable generated identities per concrete Object",
          "[chart][v4][resolver][cxt][lowering][cfu-c2]") {
    const auto source = loadSource("valid/chart_v4_cxt_template_binding.json");
    const std::array documents{cuexis::chart::ProjectDocument{
        "templates/move-y.cxt", readFile(fixture("valid/templates/move-y.cxt"))}};
    const auto result = cuexis::chart::ChartV4Resolver::resolve(source, {}, documents);
    REQUIRE(result.hasValue());
    REQUIRE(result.artifact->animationProgram.objects.size() == 2);
    REQUIRE(result.artifact->animationProgram.clips.size() == 2);
    REQUIRE(result.artifact->cxtIdentities.size() == 1);
    CHECK(result.artifact->cxtIdentities.front().importId == "motion.move-y");

    const auto& firstLayer = result.artifact->animationProgram.objects[0].layers.front();
    const auto& secondLayer = result.artifact->animationProgram.objects[1].layers.front();
    REQUIRE(std::holds_alternative<cuexis::chart::GeneratedAnimationIdentity>(firstLayer.identity));
    REQUIRE(
        std::holds_alternative<cuexis::chart::GeneratedAnimationIdentity>(secondLayer.identity));
    const auto& firstIdentity =
        std::get<cuexis::chart::GeneratedAnimationIdentity>(firstLayer.identity);
    const auto& secondIdentity =
        std::get<cuexis::chart::GeneratedAnimationIdentity>(secondLayer.identity);
    CHECK(firstIdentity.objectId != secondIdentity.objectId);
    CHECK(firstIdentity.templateId == "motion.move-y");
    CHECK(firstIdentity.recordKind == cuexis::chart::GeneratedRecordKind::Layer);
    CHECK(firstLayer.weight == Catch::Approx(1.0));
    REQUIRE(firstLayer.blendGroups.size() == 1);
    CHECK(firstLayer.blendGroups.front().weight == Catch::Approx(1.0));
    REQUIRE(firstLayer.blendGroups.front().instances.size() == 1);
    CHECK(firstLayer.blendGroups.front().instances.front().durationScale.toString() == "1/1");

    CHECK(result.artifact->capabilityRequirements ==
          std::vector<std::string>{"cuexis.animation.clip.v1", "cuexis.animation.layers.v1",
                                   "cuexis.chart.v4", "cuexis.source.cxt.v1"});

    const std::array parameters{
        cuexis::chart::ChartParameterInput{
            "motion.duration-scale", cuexis::chart::ChartParameterType::Rational,
            cuexis::chart::ChartParameterValue{*cuexis::chart::RationalBeat::create(1, 2)}},
        cuexis::chart::ChartParameterInput{"motion.weight",
                                           cuexis::chart::ChartParameterType::Weight, 0.25},
    };
    const auto overridden = cuexis::chart::ChartV4Resolver::resolve(source, parameters, documents);
    REQUIRE(overridden.hasValue());
    REQUIRE(overridden.artifact->animationProgram.objects.size() == 2);
    const auto& overriddenFirst = overridden.artifact->animationProgram.objects[0].layers.front();
    CHECK(overriddenFirst.weight == Catch::Approx(1.0));
    REQUIRE(overriddenFirst.blendGroups.size() == 1);
    CHECK(overriddenFirst.blendGroups.front().weight == Catch::Approx(0.25));
    REQUIRE(overriddenFirst.blendGroups.front().instances.size() == 1);
    const auto& overriddenInstance = overriddenFirst.blendGroups.front().instances.front();
    CHECK(overriddenInstance.startBeat.toString() == "8/1");
    CHECK(overriddenInstance.durationScale.toString() == "1/2");
    CHECK(overriddenInstance.weight == Catch::Approx(1.0));
}

TEST_CASE("Template Animator expands to the concrete Object program",
          "[chart][v4][resolver][template][cfu-c2]") {
    const auto source = loadSource("valid/chart_v4_template_animator.json");
    const auto result = cuexis::chart::ChartV4Resolver::resolve(source);
    REQUIRE(result.hasValue());
    REQUIRE(result.artifact->animationProgram.objects.size() == 1);
    REQUIRE(result.artifact->animationProgram.objects.front().layers.size() == 1);
    CHECK(std::get<std::string>(
              result.artifact->animationProgram.objects.front().layers.front().identity) ==
          "layer.template-pulse");
}

TEST_CASE("CXT resolver rejects missing and mismatched project documents",
          "[chart][v4][resolver][cxt][failure][cfu-c2]") {
    SECTION("missing") {
        const auto source = loadSource("invalid/chart_v4_cxt_missing_import.json");
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "cxt.import.missing"));
    }
    SECTION("ID mismatch") {
        const auto source = loadSource("invalid/chart_v4_cxt_id_mismatch.json");
        const std::array documents{cuexis::chart::ProjectDocument{
            "templates/move-y.cxt", readFile(fixture("invalid/templates/move-y.cxt"))}};
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source, {}, documents);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "cxt.template.id_mismatch"));
    }
}

TEST_CASE("Resolver validates effective animation properties after parameter resolution",
          "[chart][v4][resolver][animation][failure][cfu-c2]") {
    SECTION("additive material") {
        const auto source = loadSource("invalid/chart_v4_additive_material.json");
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.animation.additive_unsupported"));
    }

    SECTION("discrete partial weight") {
        const auto source = loadSource("invalid/chart_v4_discrete_partial_weight.json");
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.animation.discrete_weight_unsupported"));
    }
}
