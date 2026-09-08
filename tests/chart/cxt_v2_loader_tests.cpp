#include <cuexis/chart/cxt_v2_loader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

using namespace cuexis::chart;

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto fixture(const std::string& name) -> std::string {
    return readFile(std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                    "chart_format_foundation" / "valid" / name);
}

[[nodiscard]] auto hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code)
    -> bool {
    return std::any_of(
        diagnostics.items().begin(), diagnostics.items().end(),
        [code](const cuexis::core::Diagnostic& diagnostic) { return diagnostic.code() == code; });
}

[[nodiscard]] auto stairInvocation() -> CxtV2Invocation {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart-foundation";
    invocation.bindingId = "intro-stair";
    invocation.moduleId = "pattern.stair";
    invocation.exportId = "stair";
    invocation.startBeat = RationalBeat::create(16, 1).value();
    return invocation;
}

constexpr auto directPrototype = R"({
"format":"cuexis.animation-template","version":2,"moduleId":"prototype.tap","moduleKind":"prototype","metadata":{},
"parameters":[],"prototypes":[{"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":3}],"components":[],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"point","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"literal","value":{"domain":"judgement-domain","id":"candidate.lanes4"}},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[],"extensions":{}}],"extensions":{}}],
"patterns":[],"animations":[],"exports":[{"kind":"prototype","id":"tap"}],"requiredExtensions":[],"extensions":{}})";

[[nodiscard]] auto wrapPattern(std::string_view prototypeJson, std::string_view nodesJson)
    -> std::string {
    std::string text =
        R"({"format":"cuexis.animation-template","version":2,"moduleId":"pattern.case","moduleKind":"pattern","metadata":{},"parameters":[],"prototypes":[)";
    text += prototypeJson;
    text += R"(],"patterns":[{"id":"main","nodes":[)";
    text += nodesJson;
    text +=
        R"(],"extensions":{}}],"animations":[],"exports":[{"kind":"pattern","id":"main"}],"requiredExtensions":[],"extensions":{}})";
    return text;
}

constexpr auto tapPrototype =
    R"({"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":3}],"components":[],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"point","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"literal","value":{"domain":"judgement-domain","id":"candidate.lanes4"}},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[],"extensions":{}}],"extensions":{}})";

} // namespace

TEST_CASE("CXT v2 four-lane stair expands 16 entities and 16 requirements",
          "[chart][foundation][f2]") {
    auto invocation = stairInvocation();
    const auto result = CxtV2Loader::expand(fixture("pattern_stair.cxt"), invocation);
    REQUIRE(result.chart.has_value());
    CHECK(result.counts.entityCount == 16);
    CHECK(result.counts.requirementCount == 16);
    REQUIRE(result.chart->entities.size() == 16);

    const auto& n2 = result.chart->entities[2];
    const auto& identity = std::get<GeneratedEntityIdentity>(n2.identity);
    CHECK(identity.bindingId == "intro-stair");
    CHECK(identity.moduleId == "pattern.stair");
    CHECK(identity.exportId == "stair");
    REQUIRE(identity.path.size() == 2);
    CHECK(identity.path[0] == SemanticIdentityStep{"groups", 1U});
    CHECK(identity.path[1] == SemanticIdentityStep{"n2", 0U});
    CHECK_FALSE(n2.parent.has_value());
    REQUIRE(n2.components.size() == 1);
    CHECK(std::get<CanonicalTransform>(n2.components.front()).position.x == 2.0F);
    REQUIRE(n2.requirements.size() == 1);
    CHECK(n2.requirements.front().interval.startBeat == RationalBeat::create(33, 2).value());
    CHECK(std::get<LaneConstraint>(n2.requirements.front().constraints.front()).lane == 2);
}

TEST_CASE("CXT v2 expansion identity is independent of source array order",
          "[chart][foundation][f2]") {
    auto invocation = stairInvocation();
    const auto left = CxtV2Loader::expand(fixture("pattern_stair.cxt"), invocation);
    const auto right = CxtV2Loader::expand(fixture("pattern_stair_reordered.cxt"), invocation);
    REQUIRE(left.chart.has_value());
    REQUIRE(right.chart.has_value());
    REQUIRE(left.chart->entities.size() == right.chart->entities.size());
    for (std::size_t index = 0; index < left.chart->entities.size(); ++index) {
        CHECK(left.chart->entities[index] == right.chart->entities[index]);
    }
}

TEST_CASE("CXT v2 groups=10000 preflights 40000 entities before allocation",
          "[chart][foundation][f2]") {
    auto invocation = stairInvocation();
    invocation.parameters.push_back(CxtV2ParameterBinding{"groups", std::int64_t{10000}});
    ChartLimits limits;
    limits.maxCxtV2ExpansionEntities = 39999;
    const auto result = CxtV2Loader::expand(fixture("pattern_stair.cxt"), invocation, limits);
    CHECK_FALSE(result.chart.has_value());
    CHECK(result.counts.entityCount == 40000);
    CHECK(result.counts.requirementCount == 40000);
    CHECK(hasCode(result.diagnostics, "cxt.v2.budget.expansion"));
}

TEST_CASE("CXT v1 is rejected by explicit v2 route", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.moduleId = "x";
    auto result =
        CxtV2Loader::expand(R"({"format":"cuexis.animation-template","version":1})", invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.version_unsupported"));
}

TEST_CASE("CXT v2 prototype module supports direct invocation without a Transform",
          "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "direct";
    invocation.moduleId = "prototype.tap";
    invocation.exportId = "tap";
    invocation.slotBindings.push_back(
        CxtV2ParameterBinding{"beat", RationalBeat::create(3, 2).value()});
    invocation.slotBindings.push_back(CxtV2ParameterBinding{"lane", std::int64_t{2}});
    const auto result = CxtV2Loader::expand(directPrototype, invocation);
    REQUIRE(result.chart.has_value());
    REQUIRE(result.chart->entities.size() == 1);
    CHECK(result.chart->entities[0].components.empty());
    const auto& identity = std::get<GeneratedEntityIdentity>(result.chart->entities[0].identity);
    REQUIRE(identity.path.size() == 1);
    CHECK(identity.path[0] == SemanticIdentityStep{"__direct__", 0U});
    CHECK(result.chart->entities[0].requirements[0].interval.startBeat ==
          RationalBeat::create(3, 2).value());
}

TEST_CASE("CXT v2 rejects an export mismatch", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "direct";
    invocation.moduleId = "prototype.tap";
    invocation.exportId = "wrong";
    invocation.slotBindings.push_back(
        CxtV2ParameterBinding{"beat", RationalBeat::create(1, 1).value()});
    invocation.slotBindings.push_back(CxtV2ParameterBinding{"lane", std::int64_t{1}});
    const auto result = CxtV2Loader::expand(directPrototype, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.export_mismatch"));
}

TEST_CASE("CXT v2 rejects a prototype slot binding type mismatch", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "direct";
    invocation.moduleId = "prototype.tap";
    invocation.exportId = "tap";
    invocation.slotBindings.push_back(CxtV2ParameterBinding{"beat", std::int64_t{1}});
    invocation.slotBindings.push_back(CxtV2ParameterBinding{"lane", std::int64_t{1}});
    const auto result = CxtV2Loader::expand(directPrototype, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.slot_type_invalid"));
}

TEST_CASE("CXT v2 rejects a missing required slot", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "case";
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto text = wrapPattern(
        tapPrototype,
        R"({"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"lane","source":{"kind":"literal","value":1}}],"parent":{"kind":"root"}})");
    const auto result = CxtV2Loader::expand(text, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.slot_missing"));
}

TEST_CASE("CXT v2 rejects lane 4 on candidate.lanes4", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "case";
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto prototype =
        R"({"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":10}],"components":[],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"point","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"literal","value":{"domain":"judgement-domain","id":"candidate.lanes4"}},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[],"extensions":{}}],"extensions":{}})";
    const auto text = wrapPattern(
        prototype,
        R"({"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":0,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":4}}],"parent":{"kind":"root"}})");
    const auto result = CxtV2Loader::expand(text, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.lane_out_of_range"));
}

TEST_CASE("CXT v2 rejects a dynamic judgementDomain reference", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto prototype =
        R"({"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":3}],"components":[],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"point","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"parameter","id":"domain"},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[],"extensions":{}}],"extensions":{}})";
    const auto text = wrapPattern(
        prototype,
        R"({"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":0,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":0}}],"parent":{"kind":"root"}})");
    const auto result = CxtV2Loader::expand(text, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.requirement_unsupported"));
}

TEST_CASE("CXT v2 rejects a parent cycle", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "case";
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto text = wrapPattern(
        tapPrototype,
        R"({"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":0,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":0}}],"parent":{"kind":"emission","nodeId":"n1"}},{"op":"emit","nodeId":"n1","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":1,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":1}}],"parent":{"kind":"emission","nodeId":"n0"}})");
    const auto result = CxtV2Loader::expand(text, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.parent_cycle"));
}

TEST_CASE("CXT v2 rejects unknown core fields and non-empty extensions",
          "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.moduleId = "pattern.stair";
    invocation.exportId = "stair";
    auto unknown = fixture("pattern_stair.cxt");
    const auto closing = unknown.find_last_of('}');
    REQUIRE(closing != std::string::npos);
    unknown.insert(closing, R"(,"futureCoreField":true)");
    const auto unknownResult = CxtV2Loader::expand(unknown, invocation);
    CHECK_FALSE(unknownResult.chart.has_value());
    CHECK(hasCode(unknownResult.diagnostics, "cxt.v2.field_unknown"));

    auto extensions = fixture("pattern_stair.cxt");
    const auto marker = std::string{R"("extensions": {})"};
    const auto offset = extensions.rfind(marker);
    REQUIRE(offset != std::string::npos);
    extensions.replace(offset, marker.size(), R"("extensions":{"eval":1})");
    const auto extensionResult = CxtV2Loader::expand(extensions, invocation);
    CHECK_FALSE(extensionResult.chart.has_value());
    CHECK(hasCode(extensionResult.diagnostics, "cxt.v2.extension_unsupported"));
}

TEST_CASE("CXT v2 rejects number, vector, and boolean parameter types", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto expandType = [&](std::string_view typeJson) {
        std::string text =
            R"({"format":"cuexis.animation-template","version":2,"moduleId":"pattern.case","moduleKind":"pattern","metadata":{},"parameters":[)";
        text += typeJson;
        text +=
            R"(],"prototypes":[],"patterns":[{"id":"main","nodes":[{"op":"repeat","nodeId":"r","count":{"kind":"literal","value":0},"index":"i","body":[{"op":"emit","nodeId":"n0","prototype":"tap","bindings":[],"parent":{"kind":"root"}}]}],"extensions":{}}],"animations":[],"exports":[{"kind":"pattern","id":"main"}],"requiredExtensions":[],"extensions":{}})";
        return CxtV2Loader::expand(text, invocation);
    };
    for (const auto* typeJson :
         {R"({"id":"n","type":"number","default":1.0,"minimum":0.0,"maximum":2.0})",
          R"({"id":"n","type":"boolean","default":true})",
          R"({"id":"n","type":"vector","default":[0,0,0]})"}) {
        const auto result = expandType(typeJson);
        CHECK_FALSE(result.chart.has_value());
        CHECK(hasCode(result.diagnostics, "cxt.v2.parameter_type_unsupported"));
    }
}

TEST_CASE("CXT v2 rejects malformed JSON without publishing a chart", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto result = CxtV2Loader::expand("{", invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK_FALSE(result.diagnostics.items().empty());
}

TEST_CASE("CXT v2 rejects a dropped requirement interval instead of publishing a partial chart",
          "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto prototype =
        R"({"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":3}],"components":[],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"range","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"literal","value":{"domain":"judgement-domain","id":"candidate.lanes4"}},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[],"extensions":{}}],"extensions":{}})";
    const auto text = wrapPattern(
        prototype,
        R"({"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":0,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":0}}],"parent":{"kind":"root"}})");
    const auto result = CxtV2Loader::expand(text, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.requirement_unsupported"));
}

TEST_CASE("CXT v2 affine integer multiply uses checked arithmetic", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "case";
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    invocation.parameters.push_back(
        CxtV2ParameterBinding{"n", std::numeric_limits<std::int64_t>::max()});
    const auto overflowPrototype =
        R"({"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":3},{"id":"x","type":"integer","required":true}],"components":[{"type":"cuexis.transform","version":1,"fields":[{"path":"position[0]","source":{"kind":"slot","id":"x"}}],"extensions":{}}],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"point","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"literal","value":{"domain":"judgement-domain","id":"candidate.lanes4"}},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[],"extensions":{}}],"extensions":{}})";
    auto text = std::string{
        R"({"format":"cuexis.animation-template","version":2,"moduleId":"pattern.case","moduleKind":"pattern","metadata":{},"parameters":[{"id":"n","type":"integer","default":0,"minimum":0,"maximum":)"};
    text += std::to_string(std::numeric_limits<std::int64_t>::max());
    text += R"(}],"prototypes":[)";
    text += overflowPrototype;
    text +=
        R"(],"patterns":[{"id":"main","nodes":[{"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":0,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":0}},{"slot":"x","source":{"kind":"affine","input":{"kind":"parameter","id":"n"},"scale":{"kind":"literal","value":2},"offset":{"kind":"literal","value":0}}}],"parent":{"kind":"root"}}],"extensions":{}}],"animations":[],"exports":[{"kind":"pattern","id":"main"}],"requiredExtensions":[],"extensions":{}})";
    const auto overflow = CxtV2Loader::expand(text, invocation);
    CHECK_FALSE(overflow.chart.has_value());
    CHECK(hasCode(overflow.diagnostics, "cxt.v2.arithmetic_overflow"));

    invocation.parameters.clear();
    invocation.parameters.push_back(CxtV2ParameterBinding{"n", std::int64_t{-2}});
    auto negative = std::string{
        R"({"format":"cuexis.animation-template","version":2,"moduleId":"pattern.case","moduleKind":"pattern","metadata":{},"parameters":[{"id":"n","type":"integer","default":0,"minimum":-10,"maximum":10}],"prototypes":[)"};
    negative += overflowPrototype;
    negative +=
        R"(],"patterns":[{"id":"main","nodes":[{"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":0,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":0}},{"slot":"x","source":{"kind":"affine","input":{"kind":"parameter","id":"n"},"scale":{"kind":"literal","value":3},"offset":{"kind":"literal","value":0}}}],"parent":{"kind":"root"}}],"extensions":{}}],"animations":[],"exports":[{"kind":"pattern","id":"main"}],"requiredExtensions":[],"extensions":{}})";
    const auto scaled = CxtV2Loader::expand(negative, invocation);
    REQUIRE(scaled.chart.has_value());
    REQUIRE(scaled.chart->entities.size() == 1);
    CHECK(std::get<CanonicalTransform>(scaled.chart->entities[0].components.front()).position.x ==
          -6.0F);
}

TEST_CASE("CXT v2 rejects affine Repeat count", "[chart][foundation][f2]") {
    CxtV2Invocation invocation;
    invocation.moduleId = "pattern.case";
    invocation.exportId = "main";
    const auto text = wrapPattern(
        tapPrototype,
        R"({"op":"repeat","nodeId":"r","count":{"kind":"affine","input":{"kind":"literal","value":1},"scale":{"kind":"literal","value":1},"offset":{"kind":"literal","value":0}},"index":"i","body":[{"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"literal","value":{"numerator":0,"denominator":1}}},{"slot":"lane","source":{"kind":"literal","value":0}}],"parent":{"kind":"root"}}]})");
    const auto result = CxtV2Loader::expand(text, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(hasCode(result.diagnostics, "cxt.v2.repeat_count_invalid"));
}
