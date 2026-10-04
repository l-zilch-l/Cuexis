//  S7A-3 offline assembler tests: the entry and version gates, the P1-02 merge, the Spec 3.8.5
//  exclusion list, the closure completeness rule, atomic failure and the content-profile count.
//
//  The cases are organised by the rulings they pin down:
//
//    1. one assembling path: a packed-chart entry and a gameplay-graph entry carrying the same
//    typed
//       content produce the same graph and the same identities, and the ABI domain 8 playback rule
//       is the only thing that reads the entry kind;
//    2. the version gates: `gameplay.version = 2` and chart revision 5 are the only accepted
//    entries,
//       and an older revision is refused as an unmigrated one instead of being read as V2;
//    3. P1-02: stable identity is (source document identity, declaration ordinal), a duplicate name
//       is refused rather than merged, a cross-document reference must be explicit, and the whole
//       result is invariant under a permutation of every input array;
//    4. Spec 3.8.5 / Spec 7.2: each excluded content form reaches the stable rejection that owns
//    it,
//       with its capability id and its replacement path;
//    5. Spec 3.10: the Stage 7A resource subset, including the zero-grace rule;
//    6. Spec 6.2: a need the content did not declare is a stable failure and is never filled in;
//    7. atomic failure: a refused assembly publishes nothing and leaves an active value exactly as
//       it was;
//    8. the second half: every unfinished entry point refuses instead of fabricating a product.

#include "gameplay_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace judgement = cuexis::judgement;
namespace testing = cuexis::judgement::testing;

using judgement::AssembledGameplay;
using judgement::AssemblyRequest;
using judgement::ContentProfileCounts;
using judgement::ContentProfileLimits;
using judgement::ContentProfileVerdict;
using judgement::EntryKind;
using judgement::GameplayPublication;
using judgement::GameplaySource;
using judgement::SourceForm;
using judgement::UnsupportedContentDeclaration;
using judgement::UnsupportedContentKind;

using testing::contextValue;
using testing::Fixture;

//  Asserts the three parts of a stable rejection this suite reads: the code, the category and the
//  field path. The section is checked separately where it matters.
void expectRejection(const cuexis::core::Error& error, std::string_view code,
                     std::string_view category, std::string_view path) {
    CHECK(error.code() == code);
    CHECK(contextValue(error, "category") == category);
    CHECK(contextValue(error, "severity") == "error");
    CHECK(contextValue(error, "faulted") == "false");
    CHECK(contextValue(error, "field.path") == path);
}

void expectSection(const cuexis::core::Error& error, std::string_view section) {
    CHECK(contextValue(error, "field.section") == section);
}

//  Adds a second requirement to the fixture document, together with the declaration that names it.
void addSecondRequirement(AssemblyRequest& request) {
    auto& document = request.sources.front().document;
    judgement::RequirementRecord second = testing::makeTestRequirement();
    second.stableId.declarationOrdinal = 5U;
    second.identity.requirementLocalId = "requirement.two";
    second.identity.chartEntryId = "chart.entry.two";
    document.requirements.push_back(second);
    document.declarations.push_back(
        testing::testDeclaration(judgement::DeclarationKind::requirement, "requirement.two", 5U));
}

TEST_CASE("S7A-3 assembler accepts the canonical typed source", "[judgement][s7a-3][assembler]") {
    const Fixture fixture;
    const auto assembled = judgement::assembleGameplay(fixture.request());
    REQUIRE(assembled.has_value());
    const AssembledGameplay& value = assembled.value();

    CHECK(value.graph.gameplayVersion == 2U);
    CHECK(value.graph.graphRevision == 7U);
    CHECK(value.graph.rulesetRef == "ruleset.one");
    CHECK(value.graph.timebase == &fixture.timebase);
    CHECK(value.graph.latePolicy == &fixture.latePolicy);
    REQUIRE(value.graph.requirements.size() == 1U);
    CHECK(value.graph.requirements.front().identity.requirementLocalId == "requirement.one");
    REQUIRE(value.graph.mergedNamespace.declarations.size() == 4U);
    CHECK(value.graph.mergedNamespace.declarations.front().name == "domain.main");

    //  The merged namespace is ordered by stable identity, so the declaration order of the document
    //  cannot reach the graph.
    for (std::size_t index = 1; index < value.graph.mergedNamespace.declarations.size(); ++index) {
        CHECK_FALSE(value.graph.mergedNamespace.declarations[index].stableId <
                    value.graph.mergedNamespace.declarations[index - 1].stableId);
    }

    //  The published closure contains the ruleset contribution as well, while the declared set only
    //  carries the content-side minimum.
    REQUIRE(value.graph.derivedCapabilities.capabilities.size() == 2U);
    CHECK(value.graph.derivedCapabilities.capabilities[0].capabilityId == "test.capability.one");
    CHECK(value.graph.derivedCapabilities.capabilities[1].capabilityId ==
          "test.capability.ruleset");
    REQUIRE(value.graph.declaredCapabilities.capabilities.size() == 1U);
    CHECK(value.graph.derivedFeatures.features.size() == 2U);
    CHECK(value.graph.resourceClosure.resources.size() == 1U);
    REQUIRE(value.graph.sourceClosure.sourceDocumentIds.size() == 1U);
    CHECK(value.graph.sourceClosure.sourceDocumentIds.front() == "doc.alpha");
    CHECK(value.graph.sourceClosure.compilerProfileToken == "compiler.profile.one");

    //  The carrier form and its provenance are recorded for diagnosis only.
    CHECK(value.graph.diagnosticMap.form == SourceForm::memory);
    CHECK(value.graph.diagnosticMap.carrierProvenance == "provenance.memory.one");

    //  No content-profile threshold has been accepted yet, and that is reported rather than hidden
    //  behind a zero limit.
    CHECK(value.contentProfileVerdict == ContentProfileVerdict::notEnforcedPendingBounds);
    CHECK(value.contentProfile.requirements == 1U);
    CHECK(value.contentProfile.resources == 1U);
    CHECK(value.contentProfile.exclusiveRelations == 1U);
    CHECK(value.contentProfile.relationMembers == 1U);
    CHECK(value.contentProfile.mergedDeclarations == 4U);
    CHECK(value.contentProfile.derivedCapabilities == 2U);
    CHECK(value.contentProfile.derivedFeatures == 2U);
    CHECK(value.contentProfile.emissions == 1U);
    CHECK(value.contentProfile.patternNodes == 1U);
    CHECK(value.contentProfile.measureComponents == 1U);
    CHECK(value.contentProfile.phaseDeclarations == 2U);
}

TEST_CASE("S7A-3 there is one assembling path", "[judgement][s7a-3][entry]") {
    const Fixture fixture;
    AssemblyRequest graphEntry = fixture.request();
    graphEntry.entryKind = EntryKind::gameplayGraph;
    AssemblyRequest packedEntry = graphEntry;
    packedEntry.entryKind = EntryKind::packedChart;

    const auto fromGraph = judgement::assembleGameplay(graphEntry);
    const auto fromPacked = judgement::assembleGameplay(packedEntry);
    REQUIRE(fromGraph.has_value());
    REQUIRE(fromPacked.has_value());
    CHECK(fromGraph.value().chart.canonicalBytes() == fromPacked.value().chart.canonicalBytes());
    CHECK(fromGraph.value().content.canonicalBytes() ==
          fromPacked.value().content.canonicalBytes());
    CHECK(fromGraph.value().prepared.canonicalBytes() ==
          fromPacked.value().prepared.canonicalBytes());
    CHECK(judgement::equivalent(fromGraph.value().graph, fromPacked.value().graph));

    //  ABI domain 8: an authoring source is not a playback entry.
    AssemblyRequest authorSource = fixture.request();
    authorSource.entryKind = EntryKind::authorSource;
    authorSource.playback = false;
    const auto fromAuthor = judgement::assembleGameplay(authorSource);
    REQUIRE(fromAuthor.has_value());
    CHECK(fromAuthor.value().chart.canonicalBytes() == fromGraph.value().chart.canonicalBytes());
    CHECK(fromAuthor.value().content.canonicalBytes() ==
          fromGraph.value().content.canonicalBytes());
    CHECK(fromAuthor.value().prepared.canonicalBytes() ==
          fromGraph.value().prepared.canonicalBytes());
    CHECK(judgement::semanticDiff(fromAuthor.value().graph, fromGraph.value().graph).empty());

    authorSource.playback = true;
    const auto rejected = judgement::assembleGameplay(authorSource);
    REQUIRE_FALSE(rejected.has_value());
    expectRejection(rejected.error(), "judgement.s7a3.entry.playback_entry_mismatch",
                    "invalid_relation", "playback");
    expectSection(rejected.error(), "judgement.gameplay.entry");

    AssemblyRequest noSources = fixture.request();
    noSources.sources.clear();
    const auto noSourcesChecked = judgement::assembleGameplay(noSources);
    REQUIRE_FALSE(noSourcesChecked.has_value());
    expectRejection(noSourcesChecked.error(), "judgement.s7a3.entry.source_set_empty",
                    "invalid_relation", "sources");

    AssemblyRequest noTimebase = fixture.request();
    noTimebase.timebase = nullptr;
    const auto noTimebaseChecked = judgement::assembleGameplay(noTimebase);
    REQUIRE_FALSE(noTimebaseChecked.has_value());
    expectRejection(noTimebaseChecked.error(), "judgement.s7a3.entry.timebase_missing",
                    "invalid_relation", "timebaseRef");
}

TEST_CASE("S7A-3 version gates reject an unmigrated older revision",
          "[judgement][s7a-3][version]") {
    const Fixture fixture;

    AssemblyRequest olderChart = fixture.request();
    olderChart.sources.front().document.chartVersion = 4U;
    const auto olderChartChecked = judgement::assembleGameplay(olderChart);
    REQUIRE_FALSE(olderChartChecked.has_value());
    expectRejection(olderChartChecked.error(), "judgement.s7a3.entry.chart_version_unsupported",
                    "ambiguous_migration", "chartVersion");
    expectSection(olderChartChecked.error(), "judgement.gameplay.entry");

    AssemblyRequest olderGameplay = fixture.request();
    olderGameplay.sources.front().document.gameplayVersion = 1U;
    const auto olderGameplayChecked = judgement::assembleGameplay(olderGameplay);
    REQUIRE_FALSE(olderGameplayChecked.has_value());
    //  The Spec 7.2 R-17 candidate code for the gameplay semantic version gate.
    expectRejection(olderGameplayChecked.error(), "format.gameplay_version_unsupported",
                    "ambiguous_migration", "gameplay.version");

    AssemblyRequest duplicateDocument = fixture.request();
    duplicateDocument.sources.push_back(duplicateDocument.sources.front());
    const auto duplicateChecked = judgement::assembleGameplay(duplicateDocument);
    REQUIRE_FALSE(duplicateChecked.has_value());
    expectRejection(duplicateChecked.error(), "judgement.s7a3.entry.source_document_duplicate",
                    "invalid_relation", "sourceDocumentId");

    //  A pending late-policy parameter is refused by the S7A-2 gate that this batch reuses rather
    //  than re-implementing, and it keeps its own category.
    const Fixture pendingFixture;
    AssemblyRequest pendingPolicy = pendingFixture.request();
    judgement::LatePolicyParameters pending = pendingFixture.latePolicy;
    pending.policy = std::nullopt;
    const auto assembled = judgement::assembleGameplay(pendingPolicy);
    CHECK(assembled.has_value());
    AssemblyRequest rejectedPolicy = pendingFixture.request();
    judgement::LatePolicyParameters incomplete = pendingFixture.latePolicy;
    incomplete.finalizationWatermark =
        judgement::MeasuredParameter<judgement::TickSpan>::pendingMeasurement();
    rejectedPolicy.latePolicy = &incomplete;
    const auto rejected = judgement::assembleGameplay(rejectedPolicy);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(contextValue(rejected.error(), "category") == "late_policy_incomplete");
}

TEST_CASE("S7A-3 P1-02 merge rejects instead of merging", "[judgement][s7a-3][merge]") {
    const Fixture fixture;

    AssemblyRequest duplicateName = fixture.request();
    duplicateName.sources.front().document.declarations[1].localName = "domain.main";
    const auto duplicateNameChecked = judgement::assembleGameplay(duplicateName);
    REQUIRE_FALSE(duplicateNameChecked.has_value());
    expectRejection(duplicateNameChecked.error(), "judgement.s7a3.merge.declaration_name_duplicate",
                    "invalid_relation", "localName");
    expectSection(duplicateNameChecked.error(), "judgement.gameplay.merge");

    AssemblyRequest duplicateOrdinal = fixture.request();
    duplicateOrdinal.sources.front().document.declarations.push_back(testing::testDeclaration(
        judgement::DeclarationKind::patternDefinition, "pattern.other", 1U));
    const auto duplicateOrdinalChecked = judgement::assembleGameplay(duplicateOrdinal);
    REQUIRE_FALSE(duplicateOrdinalChecked.has_value());
    expectRejection(duplicateOrdinalChecked.error(), "judgement.s7a3.merge.stable_id_duplicate",
                    "invalid_relation", "declarationOrdinal");

    AssemblyRequest unassignedOrdinal = fixture.request();
    unassignedOrdinal.sources.front().document.declarations[3].stableId.declarationOrdinal = 0U;
    const auto unassignedChecked = judgement::assembleGameplay(unassignedOrdinal);
    REQUIRE_FALSE(unassignedChecked.has_value());
    expectRejection(unassignedChecked.error(),
                    "judgement.s7a3.merge.declaration_ordinal_unassigned", "invalid_relation",
                    "declarationOrdinal");

    //  A cross-document reference must name its document (P1-02): the same reference resolved by
    //  name lookup would be an implicit cross-invocation dependency.
    AssemblyRequest implicitCross = fixture.request();
    implicitCross.sources.front().document.declarations[1].references.push_back(
        judgement::DeclarationRef{.scope = judgement::ReferenceScope::explicitCrossDocument,
                                  .sourceDocumentId = {},
                                  .declarationOrdinal = 1U});
    const auto implicitCrossChecked = judgement::assembleGameplay(implicitCross);
    REQUIRE_FALSE(implicitCrossChecked.has_value());
    expectRejection(implicitCrossChecked.error(),
                    "judgement.s7a3.merge.cross_document_reference_implicit", "invalid_relation",
                    "references");

    //  The document component of a stable identity is the document the declaration lives in: a
    //  declaration may not claim another document's ordinal range.
    AssemblyRequest foreignDocument = fixture.request();
    foreignDocument.sources.front().document.declarations[1].stableId.sourceDocumentId = "doc.beta";
    const auto foreignChecked = judgement::assembleGameplay(foreignDocument);
    REQUIRE_FALSE(foreignChecked.has_value());
    expectRejection(foreignChecked.error(), "judgement.s7a3.declaration.structurally_incomplete",
                    "invalid_relation", "sourceDocumentId");
    expectSection(foreignChecked.error(), "judgement.gameplay.merge");

    AssemblyRequest dangling = fixture.request();
    dangling.sources.front().document.declarations[1].references.push_back(
        judgement::DeclarationRef{.scope = judgement::ReferenceScope::sameDocument,
                                  .sourceDocumentId = {},
                                  .declarationOrdinal = 99U});
    const auto danglingChecked = judgement::assembleGameplay(dangling);
    REQUIRE_FALSE(danglingChecked.has_value());
    expectRejection(danglingChecked.error(), "judgement.s7a3.merge.reference_dangling",
                    "invalid_relation", "references");

    //  An explicit cross-document reference resolves, and it resolves whatever order the documents
    //  and their arrays arrived in.
    AssemblyRequest crossDocument = fixture.request();
    crossDocument.sources.front().document.declarations[1].references.push_back(
        judgement::DeclarationRef{.scope = judgement::ReferenceScope::explicitCrossDocument,
                                  .sourceDocumentId = "doc.beta",
                                  .declarationOrdinal = 1U});
    GameplaySource beta{};
    beta.form = SourceForm::file;
    beta.provenanceToken = "provenance.file.beta";
    beta.document.sourceDocumentId = "doc.beta";
    beta.document.chartVersion = 5U;
    beta.document.gameplayVersion = 2U;
    beta.document.declarations = {testing::testDeclaration(
        judgement::DeclarationKind::patternDefinition, "pattern.shared", 1U, "doc.beta")};
    crossDocument.sources.push_back(beta);
    const auto crossChecked = judgement::assembleGameplay(crossDocument);
    CAPTURE(crossChecked.has_value() ? std::string{} : crossChecked.error().message());
    REQUIRE(crossChecked.has_value());
    REQUIRE(crossChecked.value().graph.mergedNamespace.declarations.size() == 5U);
    CHECK(crossChecked.value().graph.sourceClosure.sourceDocumentIds.size() == 2U);
    CHECK(crossChecked.value().graph.sourceClosure.sourceDocumentIds[0] == "doc.alpha");
    CHECK(crossChecked.value().graph.sourceClosure.sourceDocumentIds[1] == "doc.beta");

    AssemblyRequest forwardFirst = crossDocument;
    std::reverse(forwardFirst.sources.begin(), forwardFirst.sources.end());
    std::reverse(forwardFirst.sources[1].document.declarations.begin(),
                 forwardFirst.sources[1].document.declarations.end());
    const auto forwardChecked = judgement::assembleGameplay(forwardFirst);
    REQUIRE(forwardChecked.has_value());
    CHECK(forwardChecked.value().chart.canonicalBytes() ==
          crossChecked.value().chart.canonicalBytes());
    CHECK(forwardChecked.value().content.canonicalBytes() ==
          crossChecked.value().content.canonicalBytes());
    CHECK(forwardChecked.value().graph.mergedNamespace ==
          crossChecked.value().graph.mergedNamespace);
}

TEST_CASE("S7A-3 P1-02 merge is invariant under a permutation", "[judgement][s7a-3][reorder]") {
    const Fixture fixture;
    AssemblyRequest canonical = fixture.request();
    addSecondRequirement(canonical);

    const auto canonicalChecked = judgement::assembleGameplay(canonical);
    REQUIRE(canonicalChecked.has_value());

    AssemblyRequest shuffled = canonical;
    auto& document = shuffled.sources.front().document;
    std::reverse(document.declarations.begin(), document.declarations.end());
    std::reverse(document.requirements.begin(), document.requirements.end());
    std::reverse(document.resources.begin(), document.resources.end());
    std::reverse(document.relations.begin(), document.relations.end());
    std::reverse(document.solverProfiles.begin(), document.solverProfiles.end());
    std::reverse(document.factBindings.begin(), document.factBindings.end());
    std::reverse(document.judgementDomains.begin(), document.judgementDomains.end());
    shuffled.declaredCapabilities.capabilities.push_back(
        judgement::CapabilityRef{.capabilityId = "test.capability.one", .revision = {}});

    const auto shuffledChecked = judgement::assembleGameplay(shuffled);
    REQUIRE(shuffledChecked.has_value());

    //  Field-identical graphs: the same canonical bytes for all three identities, an empty semantic
    //  diff, and the same stable declaration table.
    CHECK(shuffledChecked.value().chart.canonicalBytes() ==
          canonicalChecked.value().chart.canonicalBytes());
    CHECK(shuffledChecked.value().content.canonicalBytes() ==
          canonicalChecked.value().content.canonicalBytes());
    CHECK(shuffledChecked.value().prepared.canonicalBytes() ==
          canonicalChecked.value().prepared.canonicalBytes());
    CHECK(judgement::equivalent(shuffledChecked.value().graph, canonicalChecked.value().graph));
    CHECK(judgement::semanticDiff(shuffledChecked.value().graph, canonicalChecked.value().graph)
              .empty());
    CHECK(shuffledChecked.value().graph.mergedNamespace ==
          canonicalChecked.value().graph.mergedNamespace);
    CHECK(shuffledChecked.value().contentProfile == canonicalChecked.value().contentProfile);
    REQUIRE(shuffledChecked.value().graph.requirements.size() == 2U);
    CHECK(shuffledChecked.value().graph.requirements[0].identity.requirementLocalId ==
          "requirement.one");
    CHECK(shuffledChecked.value().graph.requirements[1].identity.requirementLocalId ==
          "requirement.two");
}

TEST_CASE("S7A-3 excluded content forms reach their stable rejection",
          "[judgement][s7a-3][excluded]") {
    const Fixture fixture;

    const auto rejectWith = [&](UnsupportedContentKind kind, bool onResource, std::string_view code,
                                std::string_view category, std::string_view capabilityId,
                                std::string_view remediation, std::string_view path) {
        AssemblyRequest request = fixture.request();
        const UnsupportedContentDeclaration declared{.kind = kind,
                                                     .declaredToken = "declared.token"};
        if (onResource) {
            request.sources.front().document.resources.front().unsupportedForms = {declared};
        } else {
            request.sources.front().document.requirements.front().unsupportedForms = {declared};
        }
        const auto checked = judgement::assembleGameplay(request);
        REQUIRE_FALSE(checked.has_value());
        expectRejection(checked.error(), code, category, path);
        CHECK(contextValue(checked.error(), "capabilityId") == capabilityId);
        CHECK(contextValue(checked.error(), "remediation") == remediation);
        expectSection(checked.error(), "judgement.gameplay.graph");
    };

    rejectWith(UnsupportedContentKind::gameplayEffectsNotLowered, false, "migration.ambiguous",
               "ambiguous_migration", "", "", "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::boundedRelationInstance, false,
               "judgement.s7a3.content.unsupported_form", "non_terminating_source", "", "",
               "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::runtimeRepeatCounter, false,
               "capability.permanently_unsupported", "capability_disabled", "", "",
               "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::sameContact, false, "pattern.relation_unsupported",
               "capability_disabled", "pattern.relation.v1", "7B+",
               "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::continuousTrajectory, false, "input.continuous_unsupported",
               "capability_disabled", "input.trajectory.v1", "S7B-1",
               "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::contactFollowingSlider, false,
               "input.continuous_unsupported", "capability_disabled", "input.trajectory.v1",
               "S7B-1", "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::minimumReportRate, false, "input.continuous_unsupported",
               "capability_disabled", "input.trajectory.v1", "S7B-1",
               "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::handoffHold, false, "resource.handoff_unsupported",
               "capability_disabled", "resource.handoff.v1", "7B+",
               "requirements[0].unsupportedForm.declared.token");
    rejectWith(UnsupportedContentKind::resourceOwnerSet, true, "resource.capacity_unsupported",
               "capability_disabled", "resource.capacity.v1", "7B+",
               "resources[0].unsupportedForm.declared.token");

    //  A pattern node can carry an excluded form as well, and the rejection names the node.
    AssemblyRequest nodeForm = fixture.request();
    nodeForm.sources.front().document.requirements.front().pattern.root.unsupportedForm =
        UnsupportedContentDeclaration{.kind = UnsupportedContentKind::boundedRelationInstance,
                                      .declaredToken = "declared.node.token"};
    const auto nodeChecked = judgement::assembleGameplay(nodeForm);
    REQUIRE_FALSE(nodeChecked.has_value());
    expectRejection(nodeChecked.error(), "judgement.s7a3.content.unsupported_form",
                    "non_terminating_source", "pattern.root.unsupportedForm");

    //  Content whose semantics require Release / tail while no tail phase is declared is refused
    //  rather than given an inferred tail (Spec 3.8.3).
    AssemblyRequest implicitTail = fixture.request();
    implicitTail.sources.front().document.requirements.front().requiresReleaseTailSemantics = true;
    const auto implicitTailChecked = judgement::assembleGameplay(implicitTail);
    REQUIRE_FALSE(implicitTailChecked.has_value());
    expectRejection(implicitTailChecked.error(), "judgement.s7a3.requirement.release_tail_implicit",
                    "invalid_relation", "requirements[0].phases");

    //  The declared tail phase is what makes the same content acceptable.
    AssemblyRequest declaredTail = implicitTail;
    declaredTail.sources.front().document.requirements.front().phases.push_back(
        judgement::PhaseDeclaration{.kind = judgement::PhaseKind::tail, .declarationOrdinal = 3U});
    declaredTail.sources.front().document.requirements.front().measure.components.push_back(
        judgement::MeasureComponentDeclaration{.phase = judgement::PhaseKind::tail,
                                               .categoryToken = "hold_tail"});
    CHECK(judgement::assembleGameplay(declaredTail).has_value());
}

TEST_CASE("S7A-3 Stage 7A resource subset", "[judgement][s7a-3][resource]") {
    const Fixture fixture;

    AssemblyRequest capacity = fixture.request();
    capacity.sources.front().document.resources.front().declaredCapacity = 2U;
    const auto capacityChecked = judgement::assembleGameplay(capacity);
    REQUIRE_FALSE(capacityChecked.has_value());
    expectRejection(capacityChecked.error(), "resource.capacity_unsupported", "capability_disabled",
                    "resources[0].declaredCapacity");
    CHECK(contextValue(capacityChecked.error(), "capabilityId") == "resource.capacity.v1");
    CHECK(contextValue(capacityChecked.error(), "remediation") == "7B+");

    AssemblyRequest grace = fixture.request();
    grace.sources.front().document.resources.front().declaredGapGrace = judgement::TickSpan{5};
    const auto graceChecked = judgement::assembleGameplay(grace);
    REQUIRE_FALSE(graceChecked.has_value());
    expectRejection(graceChecked.error(), "resource.handoff_unsupported", "capability_disabled",
                    "resources[0].declaredGapGrace");
    CHECK(contextValue(graceChecked.error(), "capabilityId") == "resource.handoff.v1");

    //  A zero grace is the only legal Stage 7A value for the resource state machine, and the
    //  requirement-side preparedGrace is a different declaration this batch does not resolve.
    AssemblyRequest preparedGrace = fixture.request();
    preparedGrace.sources.front().document.requirements.front().preparedGrace =
        judgement::PreparedGrace{judgement::TickSpan{9}};
    CHECK(judgement::assembleGameplay(preparedGrace).has_value());

    AssemblyRequest binding = fixture.request();
    binding.sources.front().document.relations.front().kind = judgement::RelationKind::binding;
    const auto bindingChecked = judgement::assembleGameplay(binding);
    REQUIRE_FALSE(bindingChecked.has_value());
    expectRejection(bindingChecked.error(), "coordination.relation_unsupported",
                    "capability_disabled", "relations[0]");
    CHECK(contextValue(bindingChecked.error(), "capabilityId") == "coordination.relation.v1");

    AssemblyRequest relationCapacity = fixture.request();
    relationCapacity.sources.front().document.relations.front().declaredCapacity = 2U;
    const auto relationCapacityChecked = judgement::assembleGameplay(relationCapacity);
    REQUIRE_FALSE(relationCapacityChecked.has_value());
    expectRejection(relationCapacityChecked.error(), "resource.capacity_unsupported",
                    "capability_disabled", "relations[0].declaredCapacity");

    AssemblyRequest danglingResource = fixture.request();
    danglingResource.sources.front()
        .document.requirements.front()
        .resourceClaims.front()
        .resourceRef.resourceId = "resource.missing";
    const auto danglingResourceChecked = judgement::assembleGameplay(danglingResource);
    REQUIRE_FALSE(danglingResourceChecked.has_value());
    expectRejection(danglingResourceChecked.error(), "judgement.s7a3.resource.reference_dangling",
                    "invalid_relation", "requirements[0].resourceClaims");
}

TEST_CASE("S7A-3 closure completeness is never filled in", "[judgement][s7a-3][closure]") {
    const Fixture fixture;

    AssemblyRequest missingCapability = fixture.request();
    missingCapability.declaredCapabilities.capabilities.clear();
    const auto missingCapabilityChecked = judgement::assembleGameplay(missingCapability);
    REQUIRE_FALSE(missingCapabilityChecked.has_value());
    expectRejection(missingCapabilityChecked.error(), "judgement.s7a3.closure.declared_incomplete",
                    "identity_closure_incomplete", "declaredCapabilities.capabilityId");
    expectSection(missingCapabilityChecked.error(), "judgement.gameplay.closure");

    AssemblyRequest missingFeature = fixture.request();
    missingFeature.declaredFeatures.features.clear();
    const auto missingFeatureChecked = judgement::assembleGameplay(missingFeature);
    REQUIRE_FALSE(missingFeatureChecked.has_value());
    expectRejection(missingFeatureChecked.error(), "judgement.s7a3.closure.declared_incomplete",
                    "identity_closure_incomplete", "declaredFeatures.featureId");

    //  The ruleset and presentation contributions are owned by those subsystems, so they do not
    //  have to appear in the source-side declared set; the published closure still contains them.
    CHECK(judgement::assembleGameplay(fixture.request()).has_value());

    AssemblyRequest unknownCapability = fixture.request();
    unknownCapability.capabilityContext.recognisedCapabilityIds = {"test.capability.one"};
    unknownCapability.capabilityContext.enabledCapabilityIds = {"test.capability.one"};
    const auto unknownChecked = judgement::assembleGameplay(unknownCapability);
    REQUIRE_FALSE(unknownChecked.has_value());
    expectRejection(unknownChecked.error(), "capability.unknown", "unknown_capability",
                    "derivedCapabilities.capabilityId");
    CHECK(contextValue(unknownChecked.error(), "capabilityId") == "test.capability.ruleset");

    AssemblyRequest disabledCapability = fixture.request();
    disabledCapability.capabilityContext.enabledCapabilityIds = {"test.capability.one"};
    const auto disabledChecked = judgement::assembleGameplay(disabledCapability);
    REQUIRE_FALSE(disabledChecked.has_value());
    expectRejection(disabledChecked.error(), "judgement.s7a3.closure.capability_disabled",
                    "capability_disabled", "derivedCapabilities.capabilityId");

    //  A declared capability the derived closure does not need has an unresolved disposition
    //  (Spec 6.6), so this batch neither rejects it nor adds it to the derived closure.
    AssemblyRequest extraDeclared = fixture.request();
    extraDeclared.declaredCapabilities.capabilities.push_back(
        judgement::CapabilityRef{.capabilityId = "test.capability.extra", .revision = {}});
    extraDeclared.declaredFeatures.features.push_back(
        judgement::FeatureRef{.featureId = "test.feature.extra"});
    const auto extraChecked = judgement::assembleGameplay(extraDeclared);
    REQUIRE(extraChecked.has_value());
    CHECK(extraChecked.value().graph.declaredCapabilities.capabilities.size() == 2U);
    CHECK(extraChecked.value().graph.derivedCapabilities.capabilities.size() == 2U);
    CHECK(std::none_of(extraChecked.value().graph.derivedCapabilities.capabilities.begin(),
                       extraChecked.value().graph.derivedCapabilities.capabilities.end(),
                       [](const judgement::CapabilityRef& ref) {
                           return ref.capabilityId == "test.capability.extra";
                       }));
}

//  The ruleset / presentation contributions have one disposition in each of the three places they
//  could be read, and the three dispositions are different on purpose: the Spec 6.2 completeness
//  comparison ignores them (their subsystem owns them, so the content does not have to declare
//  them), the semantic diff compares them (they are graph content), and the content-artifact
//  identity covers them (they are provenance of the content that was assembled).
TEST_CASE("S7A-3 closure contributions are diffed and identified but not completeness-compared",
          "[judgement][s7a-3][closure]") {
    const Fixture fixture;

    const auto baseline = judgement::assembleGameplay(fixture.request());
    REQUIRE(baseline.has_value());
    //  The fixture declares a ruleset contribution the source-side declared set does not contain,
    //  and the assembly accepts it and publishes it.
    CHECK(baseline.value().graph.closureContributions.capabilities.size() == 1U);

    AssemblyRequest contributed = fixture.request();
    contributed.closureContributions.features.push_back(
        judgement::FeatureRef{.featureId = "test.feature.ruleset.two"});
    contributed.closureContributions.capabilities.push_back(
        judgement::CapabilityRef{.capabilityId = "test.capability.ruleset.two", .revision = {}});
    //  A contributed capability still has to be recognised and enabled by the compile, because the
    //  contribution joins the derived closure; what the completeness comparison ignores is the
    //  source-side *declaration* of it.
    contributed.capabilityContext.recognisedCapabilityIds.push_back("test.capability.ruleset.two");
    contributed.capabilityContext.enabledCapabilityIds.push_back("test.capability.ruleset.two");
    const auto checked = judgement::assembleGameplay(contributed);
    //  The completeness comparison never reads the contributions, so declaring more of them is not
    //  a completeness fault and is not a "surplus declaration" fault either.
    REQUIRE(checked.has_value());
    CHECK(checked.value().graph.closureContributions.capabilities.size() == 2U);
    CHECK(checked.value().graph.closureContributions.features.size() == 2U);

    //  The contributions are content in every identity that reads the closure: they join the
    //  derived closure, which the chart projection carries, and the provenance component of the
    //  content-artifact identity writes them directly. Both identities therefore change, and
    //  neither is a physical-order artefact.
    CHECK(checked.value().chart.canonicalBytes().toHex() !=
          baseline.value().chart.canonicalBytes().toHex());
    CHECK(checked.value().content.canonicalBytes().toHex() !=
          baseline.value().content.canonicalBytes().toHex());

    //  And the difference is a semantic difference, not a physical one.
    CHECK_FALSE(judgement::equivalent(baseline.value().graph, checked.value().graph));
}

//  A `sequence` chain of `depth` nodes whose deepest leaf is the atom `atomRef`. It is built
//  iteratively, and the intermediate nodes name a different atom, so changing the deepest leaf is a
//  change at exactly one place of the declaration tree.
[[nodiscard]] auto deepPatternChain(std::size_t depth, std::string atomRef)
    -> judgement::PatternNodeDeclaration {
    judgement::PatternNodeDeclaration chain{judgement::PatternPrimitive::atom, {},
                                            std::move(atomRef), {}, {}};
    for (std::size_t level = 1U; level < depth; ++level) {
        judgement::PatternNodeDeclaration step{judgement::PatternPrimitive::atom, {},
                                               "atom.step", {}, {}};
        chain =
            judgement::PatternNodeDeclaration{judgement::PatternPrimitive::sequence,
                                              {std::move(chain), std::move(step)}, {}, {}, {}};
    }
    return chain;
}

//  Risk: the identity projection of a Pattern declaration, and the semantic-diff rendering that
//  reads the same declaration tree, used to be recursive while the compile traversal was iterative.
//  A deeply nested legal declaration could therefore be projected with a host stack the compile
//  does not need. Both walks are iterative now, and this case locks that: an assembly of a chain
//  far deeper than a recursive projection would survive succeeds, and the deepest leaf is part of
//  the identity, so changing only that leaf changes the identity.
TEST_CASE("S7A-3 a deeply nested pattern is projected through identity",
          "[judgement][s7a-3][pattern][identity]") {
    constexpr std::size_t kDepth = 1024U;
    const Fixture fixture;

    AssemblyRequest deep = fixture.request();
    deep.sources.front().document.requirements.front().pattern.root =
        deepPatternChain(kDepth, "atom.deep");
    deep.sources.front().document.requirements.front().maxArmElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(kDepth);
    deep.sources.front().document.requirements.front().maxDeadlineElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(kDepth);
    const auto assembled = judgement::assembleGameplay(deep);
    REQUIRE(assembled.has_value());

    AssemblyRequest changed = fixture.request();
    changed.sources.front().document.requirements.front().pattern.root =
        deepPatternChain(kDepth, "atom.changed");
    changed.sources.front().document.requirements.front().maxArmElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(kDepth);
    changed.sources.front().document.requirements.front().maxDeadlineElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(kDepth);
    const auto changedAssembled = judgement::assembleGameplay(changed);
    REQUIRE(changedAssembled.has_value());

    //  The projection covers the whole tree, so a leaf at the deepest level is part of both
    //  identities.
    CHECK(changedAssembled.value().chart.canonicalBytes().toHex() !=
          assembled.value().chart.canonicalBytes().toHex());
    CHECK(changedAssembled.value().content.canonicalBytes().toHex() !=
          assembled.value().content.canonicalBytes().toHex());
    CHECK_FALSE(judgement::equivalent(assembled.value().graph, changedAssembled.value().graph));
}

TEST_CASE("S7A-3 failure is atomic", "[judgement][s7a-3][atomic]") {
    const Fixture fixture;
    GameplayPublication publication;
    CHECK_FALSE(publication.hasActive());
    CHECK(publication.active() == nullptr);

    AssemblyRequest rejected = fixture.request();
    rejected.sources.front().document.gameplayVersion = 1U;
    const auto failed = judgement::assembleInto(publication, rejected);
    REQUIRE_FALSE(failed.has_value());
    CHECK_FALSE(publication.hasActive());

    const auto accepted = judgement::assembleInto(publication, fixture.request());
    REQUIRE(accepted.has_value());
    REQUIRE(publication.hasActive());
    const std::string chartHex = publication.active()->chart.canonicalBytes().toHex();
    const std::string contentHex = publication.active()->content.canonicalBytes().toHex();
    CHECK(publication.active()->graph.graphRevision == 7U);

    //  A refused assembly leaves the active value exactly as it was: no half-published graph, no
    //  replaced identity.
    const auto refused = judgement::assembleInto(publication, rejected);
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(publication.hasActive());
    CHECK(publication.active()->chart.canonicalBytes().toHex() == chartHex);
    CHECK(publication.active()->content.canonicalBytes().toHex() == contentHex);
    CHECK(publication.active()->graph.graphRevision == 7U);

    //  A successful assembly replaces the value, and the replacement is the whole value.
    AssemblyRequest second = fixture.request();
    second.graphRevision = 8U;
    CHECK(judgement::assembleInto(publication, second).has_value());
    CHECK(publication.active()->graph.graphRevision == 8U);
    CHECK(publication.active()->chart.canonicalBytes().toHex() != chartHex);
}

TEST_CASE("S7A-3 containment capacity is a conservative atomic gate",
          "[judgement][s7a-3][containment][atomic]") {
    const Fixture fixture;
    GameplayPublication publication;
    REQUIRE(judgement::assembleInto(publication, fixture.request()).has_value());
    const std::string baselineChart = publication.active()->chart.canonicalBytes().toHex();

    AssemblyRequest missing = fixture.request();
    missing.sources.front().document.requirements.front().maxArmElements.reset();
    const auto missingChecked = judgement::assembleInto(publication, missing);
    REQUIRE_FALSE(missingChecked.has_value());
    CHECK(publication.active()->chart.canonicalBytes().toHex() == baselineChart);

    AssemblyRequest pending = fixture.request();
    pending.sources.front().document.requirements.front().maxDeadlineElements =
        judgement::MeasuredParameter<std::uint64_t>::pendingMeasurement();
    const auto pendingChecked = judgement::assembleInto(publication, pending);
    REQUIRE_FALSE(pendingChecked.has_value());
    CHECK(publication.active()->chart.canonicalBytes().toHex() == baselineChart);

    AssemblyRequest exceeded = fixture.request();
    exceeded.sources.front().document.requirements.front().maxArmElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(0U);
    const auto exceededChecked = judgement::assembleInto(publication, exceeded);
    REQUIRE_FALSE(exceededChecked.has_value());
    expectRejection(exceededChecked.error(), "judgement.s7a3.pattern.arm_bound_exceeded",
                    "budget_exceeded", "pattern.maxArmElements");
    CHECK(publication.active()->chart.canonicalBytes().toHex() == baselineChart);

    AssemblyRequest accepted = fixture.request();
    accepted.sources.front().document.requirements.front().maxArmElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(2U);
    accepted.sources.front().document.requirements.front().maxDeadlineElements =
        judgement::MeasuredParameter<std::uint64_t>::measured(2U);
    REQUIRE(judgement::assembleInto(publication, accepted).has_value());
    CHECK(publication.active()->chart.canonicalBytes().toHex() != baselineChart);
}

TEST_CASE("S7A-3 content profile counts and pending bounds", "[judgement][s7a-3][budget]") {
    const Fixture fixture;
    const auto assembled = judgement::assembleGameplay(fixture.request());
    REQUIRE(assembled.has_value());
    const ContentProfileCounts counts = assembled.value().contentProfile;

    const auto pending = judgement::checkContentProfile(counts, ContentProfileLimits{});
    REQUIRE(pending.has_value());
    CHECK(pending.value() == ContentProfileVerdict::notEnforcedPendingBounds);

    ContentProfileLimits measured{};
    measured.maxRequirements = judgement::MeasuredParameter<std::uint64_t>::measured(4U);
    measured.maxDiagnosticMapEntries = judgement::MeasuredParameter<std::uint64_t>::measured(100U);
    const auto partiallyMeasured = judgement::checkContentProfile(counts, measured);
    REQUIRE(partiallyMeasured.has_value());
    //  Some bounds are still pending, so the verdict keeps saying that rather than claiming the
    //  content is fully bounded.
    CHECK(partiallyMeasured.value() == ContentProfileVerdict::notEnforcedPendingBounds);

    ContentProfileLimits exceeded{};
    //  Zero is a literal upper bound and never "no limit" (BUDGET 3.2): one requirement is already
    //  above it.
    exceeded.maxRequirements = judgement::MeasuredParameter<std::uint64_t>::measured(0U);
    const auto exceededChecked = judgement::checkContentProfile(counts, exceeded);
    REQUIRE_FALSE(exceededChecked.has_value());
    expectRejection(exceededChecked.error(), "judgement.s7a3.budget.content_profile_exceeded",
                    "budget_exceeded", "contentProfile.maxRequirements");
    expectSection(exceededChecked.error(), "judgement.gameplay.budget");

    ContentProfileLimits tight{};
    tight.maxRequirements = judgement::MeasuredParameter<std::uint64_t>::measured(1U);
    tight.maxResources = judgement::MeasuredParameter<std::uint64_t>::measured(1U);
    tight.maxExclusiveRelations = judgement::MeasuredParameter<std::uint64_t>::measured(1U);
    tight.maxRelationMembers = judgement::MeasuredParameter<std::uint64_t>::measured(1U);
    tight.maxMergedDeclarations = judgement::MeasuredParameter<std::uint64_t>::measured(4U);
    tight.maxPatternNodes = judgement::MeasuredParameter<std::uint64_t>::measured(1U);
    tight.maxMeasureComponents = judgement::MeasuredParameter<std::uint64_t>::measured(1U);
    tight.maxFactBindings = judgement::MeasuredParameter<std::uint64_t>::measured(1U);
    tight.maxDerivedCapabilities = judgement::MeasuredParameter<std::uint64_t>::measured(2U);
    tight.maxDiagnosticMapEntries = judgement::MeasuredParameter<std::uint64_t>::measured(5U);
    const auto within = judgement::checkContentProfile(counts, tight);
    REQUIRE(within.has_value());
    CHECK(within.value() == ContentProfileVerdict::withinDeclaredBounds);

    //  An exceeded bound also stops the assembly, with the same diagnostic.
    AssemblyRequest overBudget = fixture.request();
    overBudget.contentProfileLimits = exceeded;
    const auto overBudgetChecked = judgement::assembleGameplay(overBudget);
    REQUIRE_FALSE(overBudgetChecked.has_value());
    expectRejection(overBudgetChecked.error(), "judgement.s7a3.budget.content_profile_exceeded",
                    "budget_exceeded", "contentProfile.maxRequirements");
}

TEST_CASE("S7A-3 reference partition follows the Spec 3.8.6 attribution",
          "[judgement][s7a-3][attribution]") {
    const Fixture fixture;
    const auto assembled = judgement::assembleGameplay(fixture.request());
    REQUIRE(assembled.has_value());
    const judgement::ReferenceClosurePartition partition =
        judgement::partitionReferences(assembled.value().graph);

    const auto contains = [](const std::vector<std::string>& entries, std::string_view wanted) {
        return std::find(entries.begin(), entries.end(), wanted) != entries.end();
    };
    CHECK(contains(partition.judgementRef0, "timebase:engine.tick.us.v1"));
    CHECK(contains(partition.judgementRef0, "requirement:requirement.one"));
    CHECK(contains(partition.judgementRef0, "solver-profile:solver.exclusive"));
    CHECK(contains(partition.judgementRef0, "coordination-policy:close.policy.one"));
    CHECK(contains(partition.judgementRef0, "emission:emission.node.one"));
    CHECK(contains(partition.judgementRef0, "fact-binding-existence:binding.one"));
    CHECK(contains(partition.judgementRef0, "resource-claim:resource.slot"));
    CHECK(contains(partition.judgementRef0, "claim-key:claim.key.one"));
    CHECK(contains(partition.judgementRef0, "judgement-domain:domain.main"));
    //  The canonical graph carries the judgement-side existence of a fact binding and never its
    //  detail, so this partition has no presentation entry to report.
    CHECK(partition.presentationManifest.empty());
    CHECK_FALSE(partition.diagnosticMap.empty());
    CHECK(contains(partition.diagnosticMap, "doc.alpha#4@requirement.one"));
}

TEST_CASE("S7A-3 prepared grace and resource plan resolve atomically",
          "[judgement][s7a-3][second-half]") {
    const judgement::RequirementRecord requirement = testing::makeTestRequirement();

    //  The compiled Pattern and the compiled Measure are implemented in this batch; their coverage
    //  lives in gameplay_pattern_compile_tests.cpp and gameplay_measure_compile_tests.cpp. Both are
    //  reachable and neither of them refuses this content.
    CHECK(judgement::compilePattern(requirement.pattern).has_value());
    CHECK(judgement::compileMeasure(requirement.measure).has_value());

    const judgement::GraceResolutionInputs graceInputs{.unitInTicks = testing::makeDuration(1, 1),
                                                       .minimumCanonical = 0,
                                                       .maximumCanonical = 8,
                                                       .candidate = judgement::TickSpan{3}};
    const auto grace = judgement::resolvePreparedGrace(requirement.grace, graceInputs);
    REQUIRE(grace.has_value());
    CHECK(grace->span() == judgement::TickSpan{3});

    const judgement::ResourceClaimResolutionInputs claimInputs{
        .declaredCapacity = 1U,
        .intents = {judgement::ResourceClaimIntent::claim},
        .candidates = {{.intent = judgement::ResourceClaimIntent::claim,
                        .policyToken = "policy.one",
                        .claimKeyToken = "key.one"}},
        .preparedGrace = judgement::PreparedGrace{judgement::TickSpan{0}}};
    const auto claims = judgement::resolveResourceClaims(claimInputs);
    REQUIRE(claims.has_value());
    CHECK(claims->candidateCount() == 1U);
    CHECK(claims->occupyingCandidateCount() == 1U);
    CHECK_FALSE(claims->hasObserveOnlyCandidates());
}

TEST_CASE("S7A-3 grace source selection is explicit and half-even",
          "[judgement][s7a-3][grace]") {
    const auto half = testing::makeDuration(1, 2);
    const auto one = testing::makeDuration(1, 1);
    judgement::GraceDeclaration inherited{
        .policy = judgement::GraceResolutionPolicy::inheritedDeclaration,
        .inheritedFromDeclarationId = "grace.source",
        .allowChartGrace = false};
    judgement::GraceResolutionInputs inputs{
        .unitInTicks = one,
        .minimumCanonical = 0,
        .maximumCanonical = 8,
        .chartDuration = std::nullopt,
        .inheritedDuration = half,
        .defaultDuration = std::nullopt,
        .candidate = std::nullopt};
    const auto resolved = judgement::resolvePreparedGrace(inherited, inputs);
    REQUIRE(resolved.has_value());
    CHECK(resolved->span() == judgement::TickSpan{0});

    inputs.inheritedDuration = testing::makeDuration(3, 2);
    const auto rounded = judgement::resolvePreparedGrace(inherited, inputs);
    REQUIRE(rounded.has_value());
    CHECK(rounded->span() == judgement::TickSpan{2});

    inputs.inheritedDuration = std::nullopt;
    const auto missing = judgement::resolvePreparedGrace(inherited, inputs);
    REQUIRE_FALSE(missing.has_value());
    expectRejection(missing.error(), "judgement.s7a3.grace.value_missing", "invalid_relation",
                    "preparedGrace");
}

TEST_CASE("S7A-3 resource plan rejects duplicate occupying claim keys",
          "[judgement][s7a-3][resource]") {
    const judgement::ResourceClaimResolutionInputs inputs{
        .declaredCapacity = 1U,
        .intents = {},
        .candidates = {
            {.intent = judgement::ResourceClaimIntent::claim,
             .policyToken = "policy.one",
             .claimKeyToken = "same"},
            {.intent = judgement::ResourceClaimIntent::consume,
             .policyToken = "policy.two",
             .claimKeyToken = "same"},
        },
        .preparedGrace = judgement::PreparedGrace{judgement::TickSpan{0}}};
    const auto result = judgement::resolveResourceClaims(inputs);
    REQUIRE_FALSE(result.has_value());
    expectRejection(result.error(), "judgement.s7a3.resource.claim_conflict", "invalid_relation",
                    "resourceClaims[1]");
}

TEST_CASE("S7A-3 resource plan rejects incomplete occupying declarations",
          "[judgement][s7a-3][resource][negative]") {
    using Candidate = judgement::ResourceClaimResolutionInputs::Candidate;
    const auto run = [](std::vector<Candidate> candidates,
                        std::vector<judgement::ResourceClaimIntent> intents = {}) {
        return judgement::resolveResourceClaims(
            judgement::ResourceClaimResolutionInputs{
                .declaredCapacity = 1U,
                .intents = std::move(intents),
                .candidates = std::move(candidates),
                .preparedGrace = judgement::PreparedGrace{judgement::TickSpan{0}}});
    };

    SECTION("capacity above the Stage 7A subset") {
        const auto result = judgement::resolveResourceClaims(
            judgement::ResourceClaimResolutionInputs{
                .declaredCapacity = 2U,
                .intents = {},
                .candidates = {},
                .preparedGrace = judgement::PreparedGrace{judgement::TickSpan{0}}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "resource.capacity_unsupported", "capability_disabled",
                        "declaredCapacity");
    }
    SECTION("occupying candidate without policy") {
        const auto result = run({Candidate{.intent = judgement::ResourceClaimIntent::claim,
                                            .policyToken = {},
                                            .claimKeyToken = "key"}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.s7a3.declaration.structurally_incomplete",
                        "invalid_relation", "resourceClaims[0]");
    }
    SECTION("occupying candidate without stable key") {
        const auto result = run({Candidate{.intent = judgement::ResourceClaimIntent::consume,
                                            .policyToken = "policy",
                                            .claimKeyToken = {}}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.s7a3.declaration.structurally_incomplete",
                        "invalid_relation", "resourceClaims[0]");
    }
    SECTION("observe cannot carry an occupying key") {
        const auto result = run({Candidate{.intent = judgement::ResourceClaimIntent::observe,
                                            .policyToken = "policy",
                                            .claimKeyToken = "key"}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.s7a3.resource.claim_conflict",
                        "invalid_relation", "resourceClaims");
    }
    SECTION("observe cannot carry a competition pair") {
        const auto result = run({Candidate{
            .intent = judgement::ResourceClaimIntent::observe,
            .policyToken = {},
            .claimKeyToken = {},
            .graceOverrideMode = judgement::GraceOverrideMode::none,
            .competition = judgement::ClaimPolicyDeclaration::CompetitionKey{0, 0}}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.s7a3.resource.claim_conflict",
                        "invalid_relation", "resourceClaims");
    }
    SECTION("legacy intent mismatch is rejected") {
        const auto result = run(
            {Candidate{.intent = judgement::ResourceClaimIntent::claim,
                       .policyToken = "policy",
                       .claimKeyToken = "key"}},
            {judgement::ResourceClaimIntent::consume});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.s7a3.declaration.structurally_incomplete",
                        "invalid_relation", "resourceClaims[0]");
    }
    SECTION("unsupported grace override is rejected") {
        const auto result = run({Candidate{.intent = judgement::ResourceClaimIntent::claim,
                                            .policyToken = "policy",
                                            .claimKeyToken = "key",
                                            .graceOverrideMode =
                                                judgement::GraceOverrideMode::sticky}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "resource.handoff_unsupported",
                        "capability_disabled", "resourceClaims[0].graceOverride");
    }
}

} // namespace
