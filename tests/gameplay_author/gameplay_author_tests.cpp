#include "../judgement/execution_test_fixture.hpp"
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/tools/gameplay_author.hpp>
#include <fstream>
#include <sstream>

namespace author = cuexis::tools::gameplay_author;
namespace capsule = cuexis::gameplay_packed;
using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
using cuexis::json::gameplay::SourceKind;
namespace {
auto path(std::string_view name) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests/fixtures/gameplay_author" / name;
}
auto source(std::string_view name) -> std::string {
    std::ifstream input{path(name), std::ios::binary};
    REQUIRE(input);
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}
auto context() -> author::AuthorCompileContext {
    Fixture fixture;
    auto request = fixture.request();
    executionFields(request);
    author::AuthorCompileContext c;
    c.chart.entryId = "chart.entry.one";
    c.chart.foundation.chartId = {"019a0000-0000-7000-8000-000000000001"};
    c.invocation = cuexis::chart::CxtV2Invocation{};
    c.invocation->chartId = c.chart.foundation.chartId;
    c.invocation->bindingId = "invocation.one";
    c.invocation->moduleId = "module.one";
    c.invocation->exportId = "export.one";
    c.invocation->parameters.push_back({"outerCount", std::int64_t{2}});
    c.compilerProfileToken = request.compilerProfileToken;
    c.capabilities = request.capabilityContext;
    c.identityDeclarations = request.identityDeclarations;
    return c;
}
auto encode(const author::AuthorPreparedArtifact& a) -> std::vector<std::byte> {
    auto result = capsule::encode(
        {a.chart, a.gameplay, a.owners, a.profiles, a.additionalPatterns, a.additionalMeasures, 3});
    INFO((result ? "" : std::string{result.error().message()}));
    REQUIRE(result);
    return *result;
}
void replace(std::string& s, std::string_view a, std::string_view b) {
    auto at = s.find(a);
    REQUIRE(at != std::string::npos);
    s.replace(at, a.size(), b);
}
} // namespace
TEST_CASE("Real inline and CXT 2x3 Binding expansion share canonical assembly",
          "[author][equivalence][affine]") {
    auto c = context();
    auto left = author::compile(source("inline_2x3.json"), SourceKind::chartInline, c);
    auto right = author::compile(source("pattern_2x3.cxt"), SourceKind::cxtModule, c);
    INFO((left ? "" : std::string{left.error().message()}));
    REQUIRE(left);
    INFO((right ? "" : std::string{right.error().message()}));
    REQUIRE(right);
    CHECK(
        semanticDiff(left->gameplay.assembled().graph, right->gameplay.assembled().graph).empty());
    REQUIRE(right->chart.entities.size() == 6);
    CHECK(left->chart.entities == right->chart.entities);
    REQUIRE(right->gameplay.assembled().graph.requirements.size() == 6);
    for (std::size_t i = 0; i < 6; ++i) {
        const auto& r = right->gameplay.assembled().graph.requirements[i];
        REQUIRE(r.independentCompetition);
        CHECK(r.independentCompetition->priority == -1);
        CHECK(r.independentCompetition->tieRank == 10 + 2 * i);
        CHECK(r.identity.emissionPath[0].repeatIndex == i / 3 + 1);
        CHECK(r.identity.emissionPath[1].repeatIndex == i % 3 + 1);
        CHECK(r.identity.emissionPath[2].repeatIndex == 0);
    }
    auto bytes = encode(*left);
    CHECK(bytes == encode(*right));
    auto file = author::read(path("pattern_2x3.cxt"), SourceKind::cxtModule, c);
    REQUIRE(file);
    CHECK(bytes == encode(*file));
    capsule::DecodeContext decode{c.capabilities,
                                  c.contentLimits,
                                  c.patternBudget,
                                  c.identityDeclarations,
                                  "coordinator.policy.greedy_v1",
                                  3};
    auto packed = capsule::decode(bytes, decode);
    INFO((packed ? "" : std::string{packed.error().message()}));
    REQUIRE(packed);
    CHECK(semanticDiff(packed->gameplay.assembled().graph, right->gameplay.assembled().graph)
              .empty());
    CHECK(packed->patterns.size() >= 2);
    CHECK(packed->measures.size() >= 2);
    CHECK_FALSE(cuexis::chart::PackedChartReader::decode(bytes));
    auto original = cuexis::chart::CxtV2Loader::expand(source("pattern_2x3.cxt"), *c.invocation,
                                                       c.foundationLimits);
    CHECK_FALSE(original.chart);
}
TEST_CASE("Real prototype synthetic identity matches inline", "[author][prototype]") {
    auto c = context();
    c.invocation->parameters.clear();
    auto a = author::compile(source("prototype.cxt"), SourceKind::cxtModule, c);
    INFO((a ? "" : std::string{a.error().message()}));
    REQUIRE(a);
    auto b = author::compile(source("inline_prototype.json"), SourceKind::chartInline, c);
    REQUIRE(b);
    CHECK(encode(*a) == encode(*b));
    CHECK(a->gameplay.assembled().graph.requirements[0].identity.emissionPath ==
          std::vector<EmissionPathStep>{{"__direct__", 0}});
}
TEST_CASE("Author source rejections preserve active owning artifact", "[author][atomic][hostile]") {
    auto c = context();
    auto good = author::compile(source("inline_2x3.json"), SourceKind::chartInline, c);
    REQUIRE(good);
    std::optional<author::AuthorPreparedArtifact> active{*good};
    const auto bytes = encode(*active);
    auto bad = source("inline_2x3.json");
    SECTION("unknown extension") {
        replace(bad, "cuexis.gameplay.v2", "unknown.extension");
    }
    SECTION("duplicate key") {
        replace(bad, "\"version\": 5", "\"version\": 5, \"version\": 5");
    }
    SECTION("wrong root version") {
        replace(bad, "\"version\": 5", "\"version\": 4");
    }
    SECTION("unknown core key") {
        replace(bad, "\"format\":", "\"unknown\": true, \"format\":");
    }
    SECTION("noncanonical integer") {
        replace(bad, "\"tieRank\": \"10\"", "\"tieRank\": \"010\"");
    }
    SECTION("rank collision") {
        replace(bad, "\"tieRank\": \"12\"", "\"tieRank\": \"10\"");
    }
    SECTION("foreign ENT identity") {
        replace(bad, "\"bindingId\": \"invocation.one\"", "\"bindingId\": \"different\"");
    }
    SECTION("legacy row") {
        replace(bad, "\"requirements\": []", "\"requirements\": [true]");
    }
    SECTION("wrong normalization") {
        replace(bad, "normalization.one", "normalization.other");
    }
    CHECK_FALSE(author::compileInto(active, bad, SourceKind::chartInline, c));
    REQUIRE(active);
    CHECK(encode(*active) == bytes);
}
TEST_CASE("Affine metadata is proven from actual frozen Repeat tree", "[author][affine][hostile]") {
    auto c = context();
    auto bytes = source("pattern_2x3.cxt");
    SECTION("wrong radix") {
        auto at = bytes.find("radices");
        REQUIRE(at != std::string::npos);
        at = bytes.find("\"3\"", at);
        REQUIRE(at != std::string::npos);
        bytes.replace(at, 3, "\"4\"");
    }
    SECTION("overflow") {
        replace(bytes, "\"base\": \"10\"", "\"base\": \"18446744073709551615\"");
    }
    SECTION("wrong node") {
        auto at = bytes.find("nodeOrder");
        REQUIRE(at != std::string::npos);
        at = bytes.find("outer", at);
        REQUIRE(at != std::string::npos);
        bytes.replace(at, 5, "wrong");
    }
    SECTION("binding changed") {
        c.invocation->parameters[0].value = std::int64_t{3};
    }
    CHECK_FALSE(author::compile(bytes, SourceKind::cxtModule, c));
}

TEST_CASE("Real author source permutations preserve bytes and unused definitions",
          "[author][determinism]") {
    auto c = context();
    auto a = author::compile(source("inline_2x3.json"), SourceKind::chartInline, c);
    REQUIRE(a);
    const auto bytes = encode(*a);
    for (const auto& name : {"inline_2x3_permuted.json", "pattern_2x3_permuted.cxt"}) {
        auto b = author::compile(source(name),
                                 std::string_view{name}.ends_with(".cxt") ? SourceKind::cxtModule
                                                                          : SourceKind::chartInline,
                                 c);
        INFO((b ? "" : std::string{b.error().message()}));
        REQUIRE(b);
        CHECK(semanticDiff(a->gameplay.assembled().graph, b->gameplay.assembled().graph).empty());
        CHECK(bytes == encode(*b));
        CHECK(b->additionalPatterns.size() == 1);
        CHECK(b->additionalMeasures.size() == 1);
    }
    const auto baseline = cuexis::chart::ChartLoader::load(source("inline_2x3.json"));
    CHECK_FALSE(baseline.document);
    CHECK(baseline.diagnostics.hasErrors());
}

TEST_CASE("Real emission allows multiple requirements on one entity and none on another",
          "[author][ownership]") {
    auto c = context();
    auto a = author::compile(source("inline_multi.json"), SourceKind::chartInline, c);
    REQUIRE(a);
    auto b = author::compile(source("pattern_multi.cxt"), SourceKind::cxtModule, c);
    INFO((b ? "" : std::string{b.error().message()}));
    REQUIRE(b);
    REQUIRE(b->chart.entities.size() == 6);
    REQUIRE(b->owners.size() == 6);
    CHECK(encode(*a) == encode(*b));
    CHECK(std::count_if(b->owners.begin(), b->owners.end(), [&](const auto& o) {
              return o.entity == b->chart.entities.front().identity;
          }) == 2);
    CHECK(std::count_if(b->owners.begin(), b->owners.end(), [&](const auto& o) {
              return o.entity == b->chart.entities.back().identity;
          }) == 0);
}

TEST_CASE("Zero Repeat validates references and keeps zero-instance author output owning",
          "[author][zero][atomic]") {
    auto c = context();
    c.invocation->parameters[0].value = std::int64_t{0};
    auto a = author::compile(source("inline_zero.json"), SourceKind::chartInline, c);
    REQUIRE(a);
    auto b = author::compile(source("pattern_zero.cxt"), SourceKind::cxtModule, c);
    INFO((b ? "" : std::string{b.error().message()}));
    REQUIRE(b);
    CHECK(b->chart.entities.empty());
    CHECK(b->gameplay.assembled().graph.requirements.empty());
    CHECK(b->additionalPatterns.size() == 1);
    CHECK(encode(*a) == encode(*b));
    auto bad = source("pattern_zero.cxt");
    replace(bad, "\"prototype\": \"tap\"", "\"prototype\": \"unknown\"");
    std::optional<author::AuthorPreparedArtifact> active{*b};
    const auto bytes = encode(*active);
    CHECK_FALSE(author::compileInto(active, bad, SourceKind::cxtModule, c));
    CHECK(encode(*active) == bytes);
    c.foundationLimits.maxInputBytes = 10;
    CHECK_FALSE(author::compileInto(active, source("pattern_zero.cxt"), SourceKind::cxtModule, c));
    CHECK(encode(*active) == bytes);
}
