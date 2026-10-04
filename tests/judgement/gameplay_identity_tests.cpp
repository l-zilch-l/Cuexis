//  S7A-3 canonical identity tests: the chart / content / prepared split, and the bytes each one is
//  made of.
//
//  The cases are organised by the rulings they pin down:
//
//    1. the carrier form and its provenance are diagnostic context: a typed file source and a typed
//       memory source carrying the same document assemble into the same graph and the same three
//       identities (Spec 3.2, plan S7A-3 "typed file / memory source consistency");
//    2. P1-15 and Spec 5.2: the same final grace value and policy share the *judgement* identity
//    even
//       when they were inherited from different declarations, while the content identity separates
//       them; and a provenance-only change (an extra namespace declaration, a grace source, a
//       pattern reference) never reaches the chart component;
//    3. the engine, ruleset and session declarations reach the prepared identity and never the
//    chart
//       component, and the engine declaration has no member that could carry the snapshot state
//       schema revision (round 5);
//    4. the content projection is the chart projection plus provenance, so equal content bytes
//    imply
//       equal chart bytes;
//    5. the bytes are canonical: repeatable, opaque (not default constructible, not constructible
//       from the outside) and rendered as lowercase hex;
//    6. an identity collision is a stable rejection, not a silent last-writer-wins.

#include "gameplay_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

namespace judgement = cuexis::judgement;
namespace testing = cuexis::judgement::testing;

using judgement::AssembledGameplay;
using judgement::AssemblyRequest;
using judgement::CanonicalIdentityBytes;
using judgement::ChartIdentity;
using judgement::ContentIdentity;
using judgement::EngineIdentityDeclaration;
using judgement::EntryKind;
using judgement::PreparedIdentity;
using judgement::SourceForm;

using testing::contextValue;
using testing::Fixture;

//  Identity carriers have private constructors and are produced by one algorithm each.
static_assert(!std::is_default_constructible_v<CanonicalIdentityBytes>);
static_assert(!std::is_constructible_v<CanonicalIdentityBytes, std::vector<std::byte>>);
static_assert(!std::is_default_constructible_v<ChartIdentity>);
static_assert(!std::is_constructible_v<ChartIdentity, CanonicalIdentityBytes>);
static_assert(!std::is_default_constructible_v<ContentIdentity>);
static_assert(!std::is_constructible_v<ContentIdentity, CanonicalIdentityBytes>);
static_assert(!std::is_default_constructible_v<PreparedIdentity>);
static_assert(!std::is_constructible_v<PreparedIdentity, CanonicalIdentityBytes>);
static_assert(std::is_final_v<CanonicalIdentityBytes>);
static_assert(std::is_final_v<ChartIdentity>);
static_assert(std::is_final_v<ContentIdentity>);
static_assert(std::is_final_v<PreparedIdentity>);

[[nodiscard]] auto assembleOrFail(const AssemblyRequest& request) -> AssembledGameplay {
    const auto assembled = judgement::assembleGameplay(request);
    REQUIRE(assembled.has_value());
    return assembled.value();
}

[[nodiscard]] auto sameBytes(const CanonicalIdentityBytes& left,
                             const CanonicalIdentityBytes& right) -> bool {
    return left.bytes() == right.bytes();
}

void checkLowercaseHex(const CanonicalIdentityBytes& bytes) {
    const std::string hex = bytes.toHex();
    CHECK(hex.size() == bytes.bytes().size() * 2U);
    const bool isLowercaseHex = std::all_of(hex.begin(), hex.end(), [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
    CHECK(isLowercaseHex);
}

TEST_CASE("S7A-3 the carrier form is diagnosis not identity", "[judgement][s7a-3][identity]") {
    const Fixture fixture;
    AssemblyRequest memorySource = fixture.request();
    AssemblyRequest fileSource = memorySource;
    fileSource.sources.front().form = SourceForm::file;
    fileSource.sources.front().provenanceToken = "some/path/chart.cuexis";

    const AssembledGameplay fromMemory = assembleOrFail(memorySource);
    const AssembledGameplay fromFile = assembleOrFail(fileSource);

    CHECK(sameBytes(fromMemory.chart.canonicalBytes(), fromFile.chart.canonicalBytes()));
    CHECK(sameBytes(fromMemory.content.canonicalBytes(), fromFile.content.canonicalBytes()));
    CHECK(sameBytes(fromMemory.prepared.canonicalBytes(), fromFile.prepared.canonicalBytes()));
    CHECK(judgement::sharesJudgementIdentity(fromMemory.prepared, fromFile.prepared));
    CHECK(judgement::equivalent(fromMemory.graph, fromFile.graph));

    //  The carrier is recorded, in the diagnostic map only.
    CHECK(fromMemory.graph.diagnosticMap.form == SourceForm::memory);
    CHECK(fromFile.graph.diagnosticMap.form == SourceForm::file);
    CHECK(fromFile.graph.diagnosticMap.carrierProvenance == "some/path/chart.cuexis");
    CHECK(fromMemory.graph.diagnosticMap.carrierProvenance !=
          fromFile.graph.diagnosticMap.carrierProvenance);
    CHECK(fromMemory.graph.diagnosticMap.entries.size() ==
          fromFile.graph.diagnosticMap.entries.size());
    REQUIRE_FALSE(fromFile.graph.diagnosticMap.entries.empty());
    CHECK(fromFile.graph.diagnosticMap.entries.front().sourceDocumentId == "doc.alpha");
}

TEST_CASE("S7A-3 the same final grace shares the judgement identity",
          "[judgement][s7a-3][identity][p1-15]") {
    const Fixture fixture;
    AssemblyRequest inherited = fixture.request();
    auto& baselineGrace = inherited.sources.front().document.requirements.front().grace;
    baselineGrace.policy = judgement::GraceResolutionPolicy::inheritedDeclaration;
    baselineGrace.inheritedFromDeclarationId = "doc.alpha#4";
    const AssembledGameplay baseline = assembleOrFail(inherited);

    //  A different source for the same policy and the same final value: shared judgement identity,
    //  distinct content identity (the source of a resolved value is provenance).
    AssemblyRequest otherSource = inherited;
    otherSource.sources.front().document.requirements.front().grace.inheritedFromDeclarationId =
        "doc.beta#9";
    const AssembledGameplay fromOtherSource = assembleOrFail(otherSource);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromOtherSource.chart.canonicalBytes()));
    CHECK(sameBytes(baseline.prepared.canonicalBytes(), fromOtherSource.prepared.canonicalBytes()));
    CHECK(judgement::sharesJudgementIdentity(baseline.prepared, fromOtherSource.prepared));
    CHECK_FALSE(
        sameBytes(baseline.content.canonicalBytes(), fromOtherSource.content.canonicalBytes()));

    //  The chart grace permission is provenance as well.
    AssemblyRequest allowedChartGrace = inherited;
    allowedChartGrace.sources.front().document.requirements.front().grace.allowChartGrace = true;
    const AssembledGameplay fromChartGrace = assembleOrFail(allowedChartGrace);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromChartGrace.chart.canonicalBytes()));
    CHECK_FALSE(
        sameBytes(baseline.content.canonicalBytes(), fromChartGrace.content.canonicalBytes()));

    //  An extra declaration in the merged namespace is source provenance too: the namespace is the
    //  authoring name table, and the judgement projection reads the resolved records instead.
    AssemblyRequest extraDeclaration = inherited;
    extraDeclaration.sources.front().document.declarations.push_back(testing::testDeclaration(
        judgement::DeclarationKind::patternDefinition, "pattern.extra", 20U));
    const AssembledGameplay fromExtraDeclaration = assembleOrFail(extraDeclaration);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromExtraDeclaration.chart.canonicalBytes()));
    CHECK(sameBytes(baseline.prepared.canonicalBytes(),
                    fromExtraDeclaration.prepared.canonicalBytes()));
    CHECK_FALSE(sameBytes(baseline.content.canonicalBytes(),
                          fromExtraDeclaration.content.canonicalBytes()));

    //  The final value itself is judgement content: the same policy with a different final value is
    //  a different judgement identity.
    AssemblyRequest otherValue = inherited;
    otherValue.sources.front().document.requirements.front().preparedGrace =
        judgement::PreparedGrace{judgement::TickSpan{5}};
    const AssembledGameplay fromOtherValue = assembleOrFail(otherValue);
    CHECK_FALSE(sameBytes(baseline.chart.canonicalBytes(), fromOtherValue.chart.canonicalBytes()));
    CHECK_FALSE(judgement::sharesJudgementIdentity(baseline.prepared, fromOtherValue.prepared));
    CHECK_FALSE(
        sameBytes(baseline.content.canonicalBytes(), fromOtherValue.content.canonicalBytes()));

    //  A different graph revision is a different judgement projection.
    AssemblyRequest otherRevision = inherited;
    otherRevision.graphRevision = 8U;
    const AssembledGameplay fromOtherRevision = assembleOrFail(otherRevision);
    CHECK_FALSE(
        sameBytes(baseline.chart.canonicalBytes(), fromOtherRevision.chart.canonicalBytes()));
}

TEST_CASE("S7A-3 grace source policy is provenance when the effective policy is unchanged",
          "[judgement][s7a-3][identity][g2-s2]") {
    const Fixture fixture;

    AssemblyRequest explicitSource = fixture.request();
    explicitSource.sources.front().document.requirements.front().grace =
        judgement::GraceDeclaration{.policy = judgement::GraceResolutionPolicy::explicitDeclaration,
                                    .inheritedFromDeclarationId = {},
                                    .allowChartGrace = false};
    const AssembledGameplay fromExplicit = assembleOrFail(explicitSource);

    AssemblyRequest inheritedSource = explicitSource;
    inheritedSource.sources.front().document.requirements.front().grace =
        judgement::GraceDeclaration{.policy =
                                        judgement::GraceResolutionPolicy::inheritedDeclaration,
                                    .inheritedFromDeclarationId = "doc.alpha#4",
                                    .allowChartGrace = false};
    const AssembledGameplay fromInherited = assembleOrFail(inheritedSource);

    AssemblyRequest defaultSource = explicitSource;
    defaultSource.sources.front().document.requirements.front().grace =
        judgement::GraceDeclaration{.policy = judgement::GraceResolutionPolicy::defaultDeclaration,
                                    .inheritedFromDeclarationId = {},
                                    .allowChartGrace = false};
    const AssembledGameplay fromDefault = assembleOrFail(defaultSource);

    CHECK(sameBytes(fromExplicit.chart.canonicalBytes(), fromInherited.chart.canonicalBytes()));
    CHECK(sameBytes(fromExplicit.chart.canonicalBytes(), fromDefault.chart.canonicalBytes()));
    CHECK(
        sameBytes(fromExplicit.prepared.canonicalBytes(), fromInherited.prepared.canonicalBytes()));
    CHECK(sameBytes(fromExplicit.prepared.canonicalBytes(), fromDefault.prepared.canonicalBytes()));
    CHECK_FALSE(
        sameBytes(fromExplicit.content.canonicalBytes(), fromInherited.content.canonicalBytes()));
    CHECK_FALSE(
        sameBytes(fromExplicit.content.canonicalBytes(), fromDefault.content.canonicalBytes()));
}

TEST_CASE("S7A-3 the declared identity components reach only the prepared identity",
          "[judgement][s7a-3][identity]") {
    const Fixture fixture;
    const AssembledGameplay baseline = assembleOrFail(fixture.request());

    //  The engine declaration is exactly four semantic tokens; there is no member that could carry
    //  the snapshot state schema revision, because round 5 keeps it out of the engine identity.
    const EngineIdentityDeclaration& engine = fixture.request().identityDeclarations.engine;
    const auto& [judgementRevision, factRevision, fixedPointTable, phaseOrder] = engine;
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(judgementRevision)>, std::string>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(factRevision)>, std::string>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(fixedPointTable)>, std::string>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(phaseOrder)>, std::string>);
    CHECK(judgementRevision == "engine.judgement.revision.one");
    CHECK(factRevision == "engine.fact.revision.one");

    AssemblyRequest factRevisionChanged = fixture.request();
    factRevisionChanged.identityDeclarations.engine.factSemanticRevision =
        "engine.fact.revision.two";
    const AssembledGameplay fromFactRevision = assembleOrFail(factRevisionChanged);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromFactRevision.chart.canonicalBytes()));
    CHECK(sameBytes(baseline.content.canonicalBytes(), fromFactRevision.content.canonicalBytes()));
    CHECK_FALSE(
        sameBytes(baseline.prepared.canonicalBytes(), fromFactRevision.prepared.canonicalBytes()));
    CHECK_FALSE(judgement::sharesJudgementIdentity(baseline.prepared, fromFactRevision.prepared));

    AssemblyRequest phaseOrderChanged = fixture.request();
    phaseOrderChanged.identityDeclarations.engine.coordinationPhaseOrderToken =
        "coordination.phase.order.two";
    const AssembledGameplay fromPhaseOrder = assembleOrFail(phaseOrderChanged);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromPhaseOrder.chart.canonicalBytes()));
    CHECK_FALSE(
        sameBytes(baseline.prepared.canonicalBytes(), fromPhaseOrder.prepared.canonicalBytes()));

    AssemblyRequest rulesetChanged = fixture.request();
    rulesetChanged.identityDeclarations.ruleset.buildHash = "ruleset.build.hash.two";
    const AssembledGameplay fromRuleset = assembleOrFail(rulesetChanged);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromRuleset.chart.canonicalBytes()));
    CHECK_FALSE(
        sameBytes(baseline.prepared.canonicalBytes(), fromRuleset.prepared.canonicalBytes()));

    AssemblyRequest sessionChanged = fixture.request();
    sessionChanged.identityDeclarations.session.normalizationProfileToken = "normalization.two";
    const AssembledGameplay fromSession = assembleOrFail(sessionChanged);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromSession.chart.canonicalBytes()));
    CHECK(sameBytes(baseline.content.canonicalBytes(), fromSession.content.canonicalBytes()));
    CHECK_FALSE(
        sameBytes(baseline.prepared.canonicalBytes(), fromSession.prepared.canonicalBytes()));

    //  The module order of the ruleset identity is an ordered semantic list: reordering it is a
    //  different prepared identity even though the set of modules is the same.
    AssemblyRequest orderedRuleset = fixture.request();
    orderedRuleset.identityDeclarations.ruleset.moduleOrder = {"module.one", "module.two"};
    AssemblyRequest reorderedRuleset = orderedRuleset;
    reorderedRuleset.identityDeclarations.ruleset.moduleOrder = {"module.two", "module.one"};
    const AssembledGameplay fromOrdered = assembleOrFail(orderedRuleset);
    const AssembledGameplay fromReordered = assembleOrFail(reorderedRuleset);
    CHECK_FALSE(
        sameBytes(fromOrdered.prepared.canonicalBytes(), fromReordered.prepared.canonicalBytes()));

    //  The entry kind is not an identity component either: the packed entry and the graph entry of
    //  the same content share all three identities.
    AssemblyRequest packedEntry = fixture.request();
    packedEntry.entryKind = EntryKind::packedChart;
    const AssembledGameplay fromPackedEntry = assembleOrFail(packedEntry);
    CHECK(sameBytes(baseline.chart.canonicalBytes(), fromPackedEntry.chart.canonicalBytes()));
    CHECK(sameBytes(baseline.content.canonicalBytes(), fromPackedEntry.content.canonicalBytes()));
    CHECK(sameBytes(baseline.prepared.canonicalBytes(), fromPackedEntry.prepared.canonicalBytes()));
}

TEST_CASE("S7A-3 the content projection is the chart projection plus provenance",
          "[judgement][s7a-3][identity]") {
    const Fixture fixture;
    const AssembledGameplay assembled = assembleOrFail(fixture.request());

    const CanonicalIdentityBytes& chart = assembled.chart.canonicalBytes();
    const CanonicalIdentityBytes& content = assembled.content.canonicalBytes();
    const CanonicalIdentityBytes& prepared = assembled.prepared.canonicalBytes();
    CHECK_FALSE(chart.empty());
    CHECK(chart.bytes().size() < content.bytes().size());
    CHECK(chart.bytes().size() < prepared.bytes().size());
    checkLowercaseHex(chart);
    checkLowercaseHex(content);
    checkLowercaseHex(prepared);

    //  Equal content bytes imply equal chart bytes, because the content projection starts with the
    //  chart projection. The suite also checks the consequence the other way: every content-equal
    //  pair above is chart-equal.
    const AssembledGameplay repeated = assembleOrFail(fixture.request());
    CHECK(sameBytes(chart, repeated.chart.canonicalBytes()));
    CHECK(sameBytes(content, repeated.content.canonicalBytes()));
    CHECK(sameBytes(prepared, repeated.prepared.canonicalBytes()));
    CHECK(assembled.chart.canonicalBytes().toHex() == repeated.chart.canonicalBytes().toHex());

    //  The three headers are distinct: an identity is not the concatenation of the others, and the
    //  generators' signature tokens differ.
    CHECK_FALSE(sameBytes(chart, content));
    CHECK_FALSE(sameBytes(chart, prepared));
    CHECK_FALSE(sameBytes(content, prepared));
}

TEST_CASE("S7A-3 measured requirement capacities participate in chart identity",
          "[judgement][s7a-3][identity][capacity]") {
    const Fixture fixture;
    const AssembledGameplay baseline = assembleOrFail(fixture.request());

    AssemblyRequest changedArm = fixture.request();
    changedArm.sources.front().document.requirements.front().maxArmElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(2U);
    const AssembledGameplay arm = assembleOrFail(changedArm);
    CHECK_FALSE(sameBytes(baseline.chart.canonicalBytes(), arm.chart.canonicalBytes()));
    CHECK_FALSE(sameBytes(baseline.content.canonicalBytes(), arm.content.canonicalBytes()));

    AssemblyRequest changedDeadline = fixture.request();
    changedDeadline.sources.front().document.requirements.front().maxDeadlineElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(2U);
    const AssembledGameplay deadline = assembleOrFail(changedDeadline);
    CHECK_FALSE(sameBytes(baseline.chart.canonicalBytes(), deadline.chart.canonicalBytes()));

    AssemblyRequest pending = fixture.request();
    pending.sources.front().document.requirements.front().maxArmElements =
        judgement::MeasuredParameter<std::uint64_t>::pendingMeasurement();
    CHECK_FALSE(judgement::assembleGameplay(pending).has_value());

    AssemblyRequest absent = fixture.request();
    absent.sources.front().document.requirements.front().maxArmElements.reset();
    CHECK_FALSE(judgement::assembleGameplay(absent).has_value());
}

TEST_CASE("S7A-3 identity collisions are rejected", "[judgement][s7a-3][identity]") {
    const Fixture fixture;
    AssemblyRequest colliding = fixture.request();
    auto& document = colliding.sources.front().document;
    //  A second requirement record carrying the same canonical six-tuple: the six-tuple is what
    //  addresses a requirement in the judgement, so the second record cannot be addressed at all.
    //  Its stable declaration id is the same one, because a second declaration of the same name is
    //  already the P1-02 duplicate-name rejection.
    judgement::RequirementRecord duplicate = testing::makeTestRequirement();
    document.requirements.push_back(duplicate);

    const auto checked = judgement::assembleGameplay(colliding);
    REQUIRE_FALSE(checked.has_value());
    CHECK(checked.error().code() == "judgement.s7a3.identity.collision");
    CHECK(contextValue(checked.error(), "category") == "invalid_relation");
    CHECK(contextValue(checked.error(), "severity") == "error");
    CHECK(contextValue(checked.error(), "field.section") == "judgement.gameplay.graph");
}

} // namespace
