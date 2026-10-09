//  S7A-3 canonical gameplay graph tests: the model, the P1-01 judgement domain, the checked
//  geometry, the closure derivation and the field-by-field semantic diff.
//
//  The cases are organised by the rulings they pin down:
//
//    1. the reference attribution of Spec 3.8.6 as a total function with no second registration
//       (P1-14);
//    2. P2-05: action, required action and domain are three independent declarations. The test
//    proves
//       it structurally, with type-level assertions, rather than by inspection;
//    3. P1-01: the static typed judgement domain. The domain declares its coordinate system and the
//       range of every axis inside itself, an incomplete or reversed axis table is a stable failure
//       in the Spec 9.3 class it belongs to, a dynamic frame is a 7B+ capability rejection, and the
//       record has no presentation member at all (a structured binding of the whole record fails to
//       compile if one is added);
//    4. the checked judgement geometry: exact results, exact rejection at the signed 64-bit
//    boundary,
//       and no clamping anywhere;
//    5. the closure derivation repeats only declared needs and never merges the declared set into
//    the
//       derived closure (ABI domain 8);
//    6. P1-15: equivalence is the field-by-field diff. The diff names every differing field, is
//    empty
//       for a permuted graph, and is empty for two graphs that differ only in their diagnostic map
//       or their source closure.

#include "gameplay_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/diagnostic.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>
#include <cuexis/judgement/input_boundary.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

TEST_CASE("S7A-3 Pattern value lifecycle is independent of declaration depth",
          "[judgement][s7a-3][deep-value]") {
    using namespace cuexis::judgement;
    PatternNodeDeclaration root{PatternPrimitive::sequence, {}, {}, {}, {}};
    auto* node = &root;
    for (std::size_t i = 0; i < 20000; ++i) {
        node->operands.emplace_back();
        node = &node->operands.back();
        node->primitive = PatternPrimitive::sequence;
    }
    node->primitive = PatternPrimitive::skip;
    auto copy = root;
    CHECK(copy == root);
    auto* leaf = &copy;
    while (!leaf->operands.empty()) {
        leaf = &leaf->operands[0];
    }
    leaf->primitive = PatternPrimitive::instant;
    CHECK_FALSE(copy == root);
    PatternNodeDeclaration assigned{};
    assigned = copy;
    CHECK(assigned == copy);
    assigned = std::move(root);
    CHECK_FALSE(assigned == copy);
    auto selfCopy = [](auto& target, const auto& source) { target = source; };
    selfCopy(assigned, assigned);
    CHECK_FALSE(assigned == copy);
    auto selfMove = [](auto& target, auto&& source) { target = std::move(source); };
    selfMove(assigned, std::move(assigned));
    CHECK_FALSE(assigned == copy);
    std::vector<PatternNodeDeclaration> forest;
    forest.push_back(assigned);
    forest.push_back(copy);
    CHECK(forest[0] == assigned);
    CHECK(forest[1] == copy);
    forest.clear();
}

namespace {

namespace judgement = cuexis::judgement;
namespace testing = cuexis::judgement::testing;

using judgement::CanonicalGameplayGraph;
using judgement::CapabilityRef;
using judgement::ClosureContributions;
using judgement::DeclaredCapabilitySet;
using judgement::DerivedCapabilityClosure;
using judgement::DomainBindingRef;
using judgement::FeatureClosure;
using judgement::FeatureRef;
using judgement::FrameResolution;
using judgement::GraceDeclaration;
using judgement::GraceResolutionPolicy;
using judgement::JudgementAxisRange;
using judgement::JudgementDomainRecord;
using judgement::PhaseDeclaration;
using judgement::PhaseKind;
using judgement::PreparedGrace;
using judgement::ReferenceClosure;
using judgement::ReferenceKind;
using judgement::RequiredActionRef;
using judgement::RequirementRecord;
using judgement::ResourceRef;
using judgement::TickSpan;

using testing::contextValue;
using testing::makeTestLatePolicy;
using testing::makeTestRequirement;
using testing::makeTestTimebase;

[[nodiscard]] auto makeGraph(const judgement::TimebaseProfile& timebase,
                             const judgement::LatePolicyParameters& latePolicy)
    -> CanonicalGameplayGraph {
    CanonicalGameplayGraph graph{};
    graph.gameplayVersion = 2U;
    graph.graphRevision = 7U;
    graph.timebase = &timebase;
    graph.latePolicy = &latePolicy;
    graph.rulesetRef = "ruleset.one";
    return graph;
}

//  Every reference kind belongs to exactly one closure class (Spec 3.8.6 rule 1).
TEST_CASE("S7A-3 reference attribution is a total function", "[judgement][s7a-3][attribution]") {
    CHECK(judgement::closureOf(ReferenceKind::requirement) == ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::pattern) == ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::measure) == ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::resource) == ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::claimKey) == ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::emission) == ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::timebaseProjection) ==
          ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::latePolicyProjection) ==
          ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::solverProfileProjection) ==
          ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::coordinationPolicyProjection) ==
          ReferenceClosure::judgementRef0);
    CHECK(judgement::closureOf(ReferenceKind::factBindingExistence) ==
          ReferenceClosure::judgementRef0);
    //  The presentation contribution never enters a judgement closure (Spec 3.8.6 rule 3).
    CHECK(judgement::closureOf(ReferenceKind::factBindingDetail) ==
          ReferenceClosure::presentationManifest);
    CHECK(judgement::closureOf(ReferenceKind::presentationResource) ==
          ReferenceClosure::presentationManifest);
    CHECK(judgement::closureOf(ReferenceKind::presentationOverride) ==
          ReferenceClosure::presentationManifest);
    CHECK(judgement::closureOf(ReferenceKind::sourceMapLocation) ==
          ReferenceClosure::diagnosticMap);

    //  A reference registers in the class its kind owns, and nowhere else (Spec 3.8.6 rule 6).
    const auto owned =
        judgement::declareReference(ReferenceKind::resource, ReferenceClosure::judgementRef0);
    REQUIRE(owned.has_value());
    CHECK(owned.value() == ReferenceClosure::judgementRef0);
    const auto crossing =
        judgement::declareReference(ReferenceKind::resource, ReferenceClosure::diagnosticMap);
    REQUIRE_FALSE(crossing.has_value());
    CHECK(crossing.error().code() == "judgement.s7a3.declaration.structurally_incomplete");
    CHECK(contextValue(crossing.error(), "category") == "invalid_relation");
    CHECK(contextValue(crossing.error(), "severity") == "error");
    CHECK(contextValue(crossing.error(), "faulted") == "false");
    CHECK(contextValue(crossing.error(), "field.section") == "judgement.gameplay.graph");
}

//  P2-05: the three declarations are independent, and that independence is a property of the types.
TEST_CASE("S7A-3 action required action and domain are independent", "[judgement][s7a-3][p2-05]") {
    static_assert(!std::is_convertible_v<judgement::InputAction, RequiredActionRef>);
    static_assert(!std::is_convertible_v<RequiredActionRef, DomainBindingRef>);
    static_assert(!std::is_convertible_v<DomainBindingRef, RequiredActionRef>);
    static_assert(!std::is_constructible_v<RequiredActionRef, std::string>);
    static_assert(!std::is_constructible_v<DomainBindingRef, std::string>);

    const auto action = RequiredActionRef::fromToken("action.primary");
    REQUIRE(action.has_value());
    CHECK(action.value().token() == "action.primary");

    //  An empty token is the unassigned state of the field and is refused at the boundary, so no
    //  consumer can read an unnamed declaration as a declared one.
    const auto emptyAction = RequiredActionRef::fromToken("");
    REQUIRE_FALSE(emptyAction.has_value());
    CHECK(emptyAction.error().code() == "judgement.s7a3.declaration.structurally_incomplete");
    CHECK(contextValue(emptyAction.error(), "category") == "invalid_relation");

    const auto emptyDomain = DomainBindingRef::fromToken("");
    REQUIRE_FALSE(emptyDomain.has_value());
    CHECK(contextValue(emptyDomain.error(), "field.path") == "domainBinding");

    //  A requirement may declare a required action that no input-side action corresponds to:
    //  nothing derives one from the other, so the declaration is well formed on its own.
    const RequirementRecord requirement = makeTestRequirement();
    CHECK(requirement.requiredActions.front().token() == "action.primary");
    CHECK(requirement.domainBinding.token() == "domain.binding.one");
    CHECK(requirement.judgementDomainId == "domain.main");
}

//  P1-01: the domain declares its own coordinate system, its own axis ranges and its own frame.
TEST_CASE("S7A-3 judgement domain validation", "[judgement][s7a-3][p1-01]") {
    const JudgementDomainRecord valid = testing::makeTestJudgementDomain();
    CHECK(judgement::validateJudgementDomain(valid).has_value());

    //  The record is exactly six declarations, and none of them is a presentation type: a
    //  structured binding of the whole record stops compiling the moment a member is added.
    const auto& [domainId, coordinateSystemToken, axes, frameResolution, provider, required] =
        valid;
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(domainId)>, std::string>);
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(coordinateSystemToken)>, std::string>);
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(axes)>, std::vector<JudgementAxisRange>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(frameResolution)>, FrameResolution>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(provider)>, std::string>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(required)>, judgement::RequiredRefs>);
    CHECK(domainId == "domain.main");
    CHECK(coordinateSystemToken == "coordinate.plane");
    CHECK(axes.size() == 1U);
    CHECK(frameResolution == FrameResolution::staticDeclaration);
    CHECK(provider.empty());

    //  A domain table without a declared range is structurally incomplete (Spec 9.3 first class).
    JudgementDomainRecord noAxes = valid;
    noAxes.axes.clear();
    const auto noAxesChecked = judgement::validateJudgementDomain(noAxes);
    REQUIRE_FALSE(noAxesChecked.has_value());
    CHECK(noAxesChecked.error().code() == "judgement.s7a3.domain.range_undeclared");
    CHECK(contextValue(noAxesChecked.error(), "category") == "invalid_relation");
    CHECK(contextValue(noAxesChecked.error(), "field.section") == "judgement.gameplay.domain");
    CHECK(contextValue(noAxesChecked.error(), "field.path") == "axes");

    JudgementDomainRecord noCoordinateSystem = valid;
    noCoordinateSystem.coordinateSystemToken.clear();
    const auto noCoordinateSystemChecked = judgement::validateJudgementDomain(noCoordinateSystem);
    REQUIRE_FALSE(noCoordinateSystemChecked.has_value());
    CHECK(contextValue(noCoordinateSystemChecked.error(), "field.path") == "coordinateSystemToken");

    //  A reversed range is a declared value that left the domain its declaration requires
    //  (Spec 9.3 second class), which is a different category from the incomplete table above.
    JudgementDomainRecord reversed = valid;
    reversed.axes = {
        JudgementAxisRange{.axisToken = "axis.horizontal", .minimum = 10, .maximum = 9}};
    const auto reversedChecked = judgement::validateJudgementDomain(reversed);
    REQUIRE_FALSE(reversedChecked.has_value());
    CHECK(reversedChecked.error().code() == "judgement.s7a3.domain.range_reversed");
    CHECK(contextValue(reversedChecked.error(), "category") == "budget_exceeded");

    JudgementDomainRecord duplicateAxis = valid;
    duplicateAxis.axes = {
        JudgementAxisRange{.axisToken = "axis.horizontal", .minimum = 0, .maximum = 5},
        JudgementAxisRange{.axisToken = "axis.horizontal", .minimum = 0, .maximum = 6}};
    const auto duplicateChecked = judgement::validateJudgementDomain(duplicateAxis);
    REQUIRE_FALSE(duplicateChecked.has_value());
    CHECK(duplicateChecked.error().code() == "judgement.s7a3.declaration.duplicate_entry");

    //  A dynamically resolved frame is registered as a later-batch candidate (P1-01) and is refused
    //  with the capability it belongs to and the batch it lands in.
    JudgementDomainRecord dynamicFrame = valid;
    dynamicFrame.frameResolution = FrameResolution::dynamicRuntimeFrame;
    dynamicFrame.dynamicFrameProviderToken = "provider.runtime.one";
    const auto dynamicChecked = judgement::validateJudgementDomain(dynamicFrame);
    REQUIRE_FALSE(dynamicChecked.has_value());
    CHECK(dynamicChecked.error().code() == "judgement.s7a3.domain.dynamic_frame_unsupported");
    CHECK(contextValue(dynamicChecked.error(), "category") == "capability_disabled");
    CHECK(contextValue(dynamicChecked.error(), "capabilityId") == "geometry.dynamic_frame.v1");
    CHECK(contextValue(dynamicChecked.error(), "remediation") == "7B+");
    CHECK(contextValue(dynamicChecked.error(), "field.path") == "frameResolution");
}

//  Geometry is exact or refused, and the refusal is the second Spec 9.3 class.
TEST_CASE("S7A-3 judgement geometry is exact or refused", "[judgement][s7a-3][geometry]") {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

    const auto sum = judgement::geometryAdd(40, 2);
    REQUIRE(sum.has_value());
    CHECK(sum.value() == 42);
    const auto negativeSum = judgement::geometryAdd(-40, 2);
    REQUIRE(negativeSum.has_value());
    CHECK(negativeSum.value() == -38);

    const auto product = judgement::geometryMultiply(6, 7);
    REQUIRE(product.has_value());
    CHECK(product.value() == 42);
    const auto negativeProduct = judgement::geometryMultiply(-6, 7);
    REQUIRE(negativeProduct.has_value());
    CHECK(negativeProduct.value() == -42);
    const auto minimumProduct = judgement::geometryMultiply(kMin, 1);
    REQUIRE(minimumProduct.has_value());
    CHECK(minimumProduct.value() == kMin);

    const auto square = judgement::geometrySquare(7);
    REQUIRE(square.has_value());
    CHECK(square.value() == 49);
    const auto negativeSquare = judgement::geometrySquare(-7);
    REQUIRE(negativeSquare.has_value());
    CHECK(negativeSquare.value() == 49);

    const auto overflowSum = judgement::geometryAdd(kMax, 1);
    REQUIRE_FALSE(overflowSum.has_value());
    CHECK(overflowSum.error().code() == "judgement.s7a3.domain.geometry_overflow");
    CHECK(contextValue(overflowSum.error(), "category") == "budget_exceeded");
    const auto underflowSum = judgement::geometryAdd(kMin, -1);
    REQUIRE_FALSE(underflowSum.has_value());
    CHECK(underflowSum.error().code() == "judgement.s7a3.domain.geometry_overflow");
    const auto overflowProduct = judgement::geometryMultiply(kMax, 2);
    REQUIRE_FALSE(overflowProduct.has_value());
    CHECK(overflowProduct.error().code() == "judgement.s7a3.domain.geometry_overflow");
    const auto overflowSquare = judgement::geometrySquare(kMax);
    REQUIRE_FALSE(overflowSquare.has_value());
    CHECK(overflowSquare.error().code() == "judgement.s7a3.domain.geometry_overflow");

    const JudgementAxisRange wide{.axisToken = "axis.wide", .minimum = kMin, .maximum = kMax};
    const auto extent = judgement::axisExtent(wide);
    REQUIRE_FALSE(extent.has_value());
    CHECK(extent.error().code() == "judgement.s7a3.domain.geometry_overflow");

    const JudgementAxisRange narrow{.axisToken = "axis.narrow", .minimum = -10, .maximum = 10};
    const auto narrowExtent = judgement::axisExtent(narrow);
    REQUIRE(narrowExtent.has_value());
    CHECK(narrowExtent.value() == 20);

    const auto inside = judgement::narrowToAxis(narrow, -10);
    REQUIRE(inside.has_value());
    CHECK(inside.value() == -10);
    //  Outside the declared range the value is refused rather than clamped: a clamped judgement
    //  input would silently change a Fact.
    const auto outside = judgement::narrowToAxis(narrow, 11);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().code() == "judgement.s7a3.domain.narrowing_rejected");
    CHECK(contextValue(outside.error(), "category") == "budget_exceeded");
    const auto unsatisfiable = judgement::narrowToAxis(wide, 0);
    REQUIRE_FALSE(unsatisfiable.has_value());
    CHECK(unsatisfiable.error().code() == "judgement.s7a3.domain.geometry_overflow");
}

//  The closure repeats declared needs only, and the declared set is never merged into it.
TEST_CASE("S7A-3 closure derivation repeats only declared needs", "[judgement][s7a-3][closure]") {
    const auto timebase = makeTestTimebase();
    const auto latePolicy = makeTestLatePolicy();
    CanonicalGameplayGraph graph = makeGraph(timebase, latePolicy);

    RequirementRecord first = makeTestRequirement();
    first.required.capabilities = {CapabilityRef{.capabilityId = "needed.b", .revision = {}},
                                   CapabilityRef{.capabilityId = "needed.a", .revision = {}}};
    first.required.features = {FeatureRef{.featureId = "feature.b"}};
    RequirementRecord second = makeTestRequirement();
    second.stableId.declarationOrdinal = 5U;
    second.identity.requirementLocalId = "requirement.two";
    second.required.capabilities = {CapabilityRef{.capabilityId = "needed.a", .revision = {}}};
    second.required.features = {FeatureRef{.featureId = "feature.a"}};
    graph.requirements = {first, second};
    graph.resourceClosure = judgement::deriveResourceClosure(graph);
    graph.declaredCapabilities = DeclaredCapabilitySet{
        .capabilities = {CapabilityRef{.capabilityId = "declared.only", .revision = {}}}};
    graph.closureContributions = ClosureContributions{
        .features = {FeatureRef{.featureId = "feature.ruleset"}},
        .capabilities = {CapabilityRef{.capabilityId = "ruleset.capability", .revision = {}}}};

    const DerivedCapabilityClosure derived = judgement::deriveCapabilityClosure(graph);
    REQUIRE(derived.capabilities.size() == 3U);
    CHECK(derived.capabilities[0].capabilityId == "needed.a");
    CHECK(derived.capabilities[1].capabilityId == "needed.b");
    CHECK(derived.capabilities[2].capabilityId == "ruleset.capability");
    //  The declared set is a separate value: nothing merges it into the derived closure.
    CHECK(std::find_if(derived.capabilities.begin(), derived.capabilities.end(),
                       [](const CapabilityRef& ref) {
                           return ref.capabilityId == "declared.only";
                       }) == derived.capabilities.end());

    const FeatureClosure features = judgement::deriveFeatureClosure(graph);
    REQUIRE(features.features.size() == 3U);
    CHECK(features.features[0].featureId == "feature.a");
    CHECK(features.features[1].featureId == "feature.b");
    CHECK(features.features[2].featureId == "feature.ruleset");

    //  The resource closure is the referenced resource set, deduplicated: the claim and the
    //  relation name the same resource.
    REQUIRE(graph.resourceClosure.resources.size() == 1U);
    CHECK(graph.resourceClosure.resources[0].resourceId == "resource.slot");
}

//  P1-15: equivalence is the field-by-field diff, not a hash and not the diagnostic map.
TEST_CASE("S7A-3 semantic diff is field by field", "[judgement][s7a-3][p1-15]") {
    const auto timebase = makeTestTimebase();
    const auto latePolicy = makeTestLatePolicy();
    CanonicalGameplayGraph left = makeGraph(timebase, latePolicy);
    left.requirements = {makeTestRequirement()};
    CanonicalGameplayGraph right = left;

    CHECK(judgement::semanticDiff(left, right).empty());
    CHECK(judgement::equivalent(left, right));

    //  A diagnostic map and a source closure difference is not a semantic difference (Spec 2.3).
    left.diagnosticMap = judgement::DiagnosticMap{
        .form = judgement::SourceForm::file,
        .carrierProvenance = "some/path/chart.cuexis",
        .entries = {judgement::DiagnosticMap::Entry{.declarationId = "doc.alpha#4",
                                                    .sourceDocumentId = "doc.alpha",
                                                    .fieldPath = "requirement.one"}}};
    left.sourceClosure = judgement::SourceClosure{.sourceDocumentIds = {"doc.alpha"},
                                                  .compilerProfileToken = "compiler.profile.one"};
    right.diagnosticMap = judgement::DiagnosticMap{
        .form = judgement::SourceForm::memory, .carrierProvenance = "memory:0", .entries = {}};
    right.sourceClosure = judgement::SourceClosure{.sourceDocumentIds = {"doc.beta"},
                                                   .compilerProfileToken = "compiler.profile.two"};
    CHECK(judgement::semanticDiff(left, right).empty());

    //  A physical order difference is not a semantic difference either.
    RequirementRecord second = makeTestRequirement();
    second.stableId.declarationOrdinal = 5U;
    second.identity.requirementLocalId = "requirement.two";
    right.requirements = {second, makeTestRequirement()};
    CanonicalGameplayGraph reordered = right;
    reordered.requirements = {makeTestRequirement(), second};
    CHECK(judgement::semanticDiff(right, reordered).empty());

    //  One differing field is named, with both values, and nothing else is reported.
    CanonicalGameplayGraph changed = reordered;
    changed.requirements[0].preparedGrace = PreparedGrace{TickSpan{9}};
    const auto differences = judgement::semanticDiff(reordered, changed);
    REQUIRE(differences.size() == 1U);
    CHECK(differences[0].path.find("requirements[") == 0U);
    CHECK(differences[0].path.find("preparedGrace") != std::string::npos);
    CHECK(differences[0].left == "3");
    CHECK(differences[0].right == "9");
    CHECK_FALSE(judgement::equivalent(reordered, changed));

    //  A presence difference is reported as a presence difference rather than as a missing row.
    CanonicalGameplayGraph removed = reordered;
    removed.requirements = {second};
    const auto presenceDifferences = judgement::semanticDiff(reordered, removed);
    REQUIRE(presenceDifferences.size() == 2U);
    CHECK(presenceDifferences[0].path.find(".size") != std::string::npos);
    CHECK(presenceDifferences[0].left == "2");
    CHECK(presenceDifferences[0].right == "1");
    CHECK(presenceDifferences[1].right == "<absent>");

    //  A scalar difference on the graph itself is named by its own path.
    CanonicalGameplayGraph otherRevision = reordered;
    otherRevision.graphRevision = 8U;
    const auto revisionDifferences = judgement::semanticDiff(reordered, otherRevision);
    REQUIRE(revisionDifferences.size() == 1U);
    CHECK(revisionDifferences[0].path == "graphRevision");
    CHECK(revisionDifferences[0].left == "7");
    CHECK(revisionDifferences[0].right == "8");

    //  The grace declaration is part of the declaration content, and the content identity is where
    //  its explicit / inherited source is recorded; the diff reports it because it is a graph
    //  field.
    CanonicalGameplayGraph inheritedGrace = reordered;
    inheritedGrace.requirements[0].grace =
        GraceDeclaration{.policy = GraceResolutionPolicy::inheritedDeclaration,
                         .inheritedFromDeclarationId = "doc.alpha#4",
                         .allowChartGrace = false};
    const auto graceDifferences = judgement::semanticDiff(reordered, inheritedGrace);
    REQUIRE(graceDifferences.size() == 1U);
    CHECK(graceDifferences[0].path.find("grace") != std::string::npos);

    //  The closure contributions of the ruleset and presentation subsystems are excluded from the
    //  Spec 6.2 *completeness comparison* of the assembler, because those subsystems own them. They
    //  are still graph content: the field-by-field diff reports them, and a contribution that
    //  changed is a semantic difference.
    CanonicalGameplayGraph contributed = reordered;
    contributed.closureContributions = ClosureContributions{
        .features = {FeatureRef{.featureId = "feature.ruleset"}},
        .capabilities = {CapabilityRef{.capabilityId = "ruleset.capability", .revision = {}}}};
    CHECK_FALSE(judgement::equivalent(reordered, contributed));
    const auto contributionDifferences = judgement::semanticDiff(reordered, contributed);
    REQUIRE_FALSE(contributionDifferences.empty());
    CHECK(contributionDifferences[0].path.rfind("closureContributions", 0U) == 0U);

    CanonicalGameplayGraph otherFeature = contributed;
    otherFeature.closureContributions.features = {FeatureRef{.featureId = "feature.ruleset.two"}};
    const auto featureDifferences = judgement::semanticDiff(contributed, otherFeature);
    REQUIRE_FALSE(featureDifferences.empty());
    CHECK(featureDifferences[0].path.rfind("closureContributionFeatures", 0U) == 0U);
    CHECK_FALSE(judgement::equivalent(contributed, otherFeature));
}

TEST_CASE("S7A-3 requirement capacities use one four-state ordering",
          "[judgement][s7a-3][capacity][ordering]") {
    using Capacity = std::optional<judgement::MeasuredParameter<std::uint64_t>>;
    const Capacity absent;
    const Capacity pending = judgement::MeasuredParameter<std::uint64_t>::pendingMeasurement();
    const Capacity measuredZero = judgement::MeasuredParameter<std::uint64_t>::measured(0U);
    const Capacity measuredSeven = judgement::MeasuredParameter<std::uint64_t>::measured(7U);

    CHECK(judgement::compareRequirementCapacity(absent, absent) == std::strong_ordering::equal);
    CHECK(judgement::compareRequirementCapacity(pending, pending) == std::strong_ordering::equal);
    CHECK(judgement::compareRequirementCapacity(measuredZero, measuredZero) ==
          std::strong_ordering::equal);
    CHECK(judgement::compareRequirementCapacity(measuredSeven, measuredSeven) ==
          std::strong_ordering::equal);
    CHECK(judgement::compareRequirementCapacity(absent, pending) == std::strong_ordering::less);
    CHECK(judgement::compareRequirementCapacity(pending, measuredZero) ==
          std::strong_ordering::less);
    CHECK(judgement::compareRequirementCapacity(measuredZero, measuredSeven) ==
          std::strong_ordering::less);
    CHECK(judgement::compareRequirementCapacity(measuredSeven, measuredZero) ==
          std::strong_ordering::greater);

    const Capacity values[] = {absent, pending, measuredZero, measuredSeven};
    for (const Capacity& left : values) {
        for (const Capacity& right : values) {
            const auto forward = judgement::compareRequirementCapacity(left, right);
            const auto reverse = judgement::compareRequirementCapacity(right, left);
            CHECK((forward == std::strong_ordering::equal) ==
                  (reverse == std::strong_ordering::equal));
            if (forward == std::strong_ordering::less) {
                CHECK(reverse == std::strong_ordering::greater);
            }
            if (forward == std::strong_ordering::greater) {
                CHECK(reverse == std::strong_ordering::less);
            }
        }
    }
    CHECK(judgement::compareRequirementCapacity(absent, measuredSeven) ==
          std::strong_ordering::less);
    CHECK(judgement::compareRequirementCapacity(measuredSeven, measuredZero) ==
          std::strong_ordering::greater);
    CHECK(judgement::compareRequirementCapacity(absent, measuredZero) ==
          std::strong_ordering::less);
    for (const Capacity& first : values) {
        for (const Capacity& second : values) {
            for (const Capacity& third : values) {
                const auto firstSecond = judgement::compareRequirementCapacity(first, second);
                const auto secondThird = judgement::compareRequirementCapacity(second, third);
                const auto firstThird = judgement::compareRequirementCapacity(first, third);
                if (firstSecond != std::strong_ordering::greater &&
                    secondThird != std::strong_ordering::greater) {
                    CHECK(firstThird != std::strong_ordering::greater);
                }
            }
        }
    }
}

} // namespace
