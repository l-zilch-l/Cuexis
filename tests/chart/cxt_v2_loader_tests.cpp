#include <cuexis/chart/cxt_v2_loader.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace cuexis::chart;

namespace {
const char* stair = R"(
{"format":"cuexis.animation-template","version":2,"moduleId":"pattern.stair","moduleKind":"pattern","metadata":{},
"parameters":[{"id":"groups","type":"integer","default":4,"minimum":1,"maximum":10000}],"prototypes":[{"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":3}],"components":[],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"point","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"literal","value":{"domain":"judgement-domain","id":"candidate.lanes4"}},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[]}],"extensions":{}}],
"patterns":[{"id":"stair","nodes":[{"op":"repeat","nodeId":"groups","count":{"kind":"parameter","id":"groups"},"index":"g","body":[{"op":"emit","nodeId":"n0","prototype":"tap","bindings":[{"slot":"beat","source":{"kind":"affine","input":{"kind":"index","id":"g"},"scale":{"kind":"literal","value":{"numerator":1,"denominator":1}},"offset":{"kind":"literal","value":{"numerator":0,"denominator":1}}}},{"slot":"lane","source":{"kind":"literal","value":0}}],"parent":{"kind":"invocation-parent"}}]}],"extensions":{}}],"animations":[],"exports":[{"kind":"pattern","id":"stair"}],"requiredExtensions":[],"extensions":{}})";
const char* directPrototype = R"({
"format":"cuexis.animation-template","version":2,"moduleId":"prototype.tap","moduleKind":"prototype","metadata":{},
"parameters":[],"prototypes":[{"id":"tap","slots":[{"id":"beat","type":"beat","required":true},{"id":"lane","type":"integer","required":true,"minimum":0,"maximum":3}],"components":[],"requirements":[{"id":"hit","kind":"tap","interval":{"kind":"point","startBeat":{"kind":"slot","id":"beat"}},"judgementDomain":{"kind":"literal","value":{"domain":"judgement-domain","id":"candidate.lanes4"}},"requiredAction":{"kind":"literal","value":{"domain":"action","id":"press"}},"constraints":[{"kind":"lane","value":{"kind":"slot","id":"lane"}}],"effects":[]}],"extensions":{}}],
"patterns":[],"animations":[],"exports":[{"kind":"prototype","id":"tap"}],"requiredExtensions":[],"extensions":{}})";
} // namespace

TEST_CASE("CXT v2 pattern expands finite entities") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "b";
    invocation.moduleId = "pattern.stair";
    invocation.exportId = "stair";
    auto result = CxtV2Loader::expand(stair, invocation);
    REQUIRE(result.chart.has_value());
    CHECK(result.counts.entityCount == 4);
    CHECK(result.counts.requirementCount == 4);
}

TEST_CASE("CXT v1 is rejected by explicit v2 route") {
    CxtV2Invocation invocation;
    invocation.moduleId = "x";
    auto result =
        CxtV2Loader::expand(R"({"format":"cuexis.animation-template","version":1})", invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(result.diagnostics.hasErrors());
}

TEST_CASE("CXT v2 count budget is checked before expansion") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "b";
    invocation.moduleId = "pattern.stair";
    invocation.exportId = "stair";
    invocation.parameters.push_back(CxtV2ParameterBinding{"groups", std::int64_t{10000}});
    ChartLimits limits;
    limits.maxCxtV2ExpansionEntities = 3;
    auto result = CxtV2Loader::expand(stair, invocation, limits);
    CHECK_FALSE(result.chart.has_value());
    CHECK(result.diagnostics.hasErrors());
}

TEST_CASE("CXT v2 prototype module supports direct invocation") {
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
    const auto& identity = std::get<GeneratedEntityIdentity>(result.chart->entities[0].identity);
    REQUIRE(identity.path.size() == 1);
    CHECK(identity.path[0] == SemanticIdentityStep{"__direct__", 0U});
    CHECK(result.chart->entities[0].requirements[0].interval.startBeat ==
          RationalBeat::create(3, 2).value());
}

TEST_CASE("CXT v2 rejects prototype slot binding type and export mismatch") {
    CxtV2Invocation invocation;
    invocation.chartId.value = "chart";
    invocation.bindingId = "direct";
    invocation.moduleId = "prototype.tap";
    invocation.exportId = "wrong";
    invocation.slotBindings.push_back(CxtV2ParameterBinding{"beat", std::int64_t{1}});
    invocation.slotBindings.push_back(CxtV2ParameterBinding{"lane", std::int64_t{4}});
    const auto result = CxtV2Loader::expand(directPrototype, invocation);
    CHECK_FALSE(result.chart.has_value());
    CHECK(result.diagnostics.hasErrors());
}
