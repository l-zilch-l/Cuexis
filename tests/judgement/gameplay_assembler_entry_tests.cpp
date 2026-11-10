//  S7A-3 assembler cases. See gameplay_assembler_test_fixture.hpp for the shared
//  aliases and rejection helpers.

#include "gameplay_assembler_test_fixture.hpp"

// Entry, version, merge, exclusion and closure cases.

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
    expectRejection(rejected.error(), "judgement.entry.playback_entry_mismatch", "invalid_relation",
                    "playback");
    expectSection(rejected.error(), "judgement.gameplay.entry");

    AssemblyRequest noSources = fixture.request();
    noSources.sources.clear();
    const auto noSourcesChecked = judgement::assembleGameplay(noSources);
    REQUIRE_FALSE(noSourcesChecked.has_value());
    expectRejection(noSourcesChecked.error(), "judgement.entry.source_set_empty",
                    "invalid_relation", "sources");

    AssemblyRequest noTimebase = fixture.request();
    noTimebase.timebase = nullptr;
    const auto noTimebaseChecked = judgement::assembleGameplay(noTimebase);
    REQUIRE_FALSE(noTimebaseChecked.has_value());
    expectRejection(noTimebaseChecked.error(), "judgement.entry.timebase_missing",
                    "invalid_relation", "timebaseRef");
}

TEST_CASE("S7A-3 version gates reject an unmigrated older revision",
          "[judgement][s7a-3][version]") {
    const Fixture fixture;

    AssemblyRequest olderChart = fixture.request();
    olderChart.sources.front().document.chartVersion = 4U;
    const auto olderChartChecked = judgement::assembleGameplay(olderChart);
    REQUIRE_FALSE(olderChartChecked.has_value());
    expectRejection(olderChartChecked.error(), "judgement.entry.chart_version_unsupported",
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
    expectRejection(duplicateChecked.error(), "judgement.entry.source_document_duplicate",
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
    expectRejection(duplicateNameChecked.error(), "judgement.merge.declaration_name_duplicate",
                    "invalid_relation", "localName");
    expectSection(duplicateNameChecked.error(), "judgement.gameplay.merge");

    AssemblyRequest duplicateOrdinal = fixture.request();
    duplicateOrdinal.sources.front().document.declarations.push_back(testing::testDeclaration(
        judgement::DeclarationKind::patternDefinition, "pattern.other", 1U));
    const auto duplicateOrdinalChecked = judgement::assembleGameplay(duplicateOrdinal);
    REQUIRE_FALSE(duplicateOrdinalChecked.has_value());
    expectRejection(duplicateOrdinalChecked.error(), "judgement.merge.stable_id_duplicate",
                    "invalid_relation", "declarationOrdinal");

    AssemblyRequest unassignedOrdinal = fixture.request();
    unassignedOrdinal.sources.front().document.declarations[3].stableId.declarationOrdinal = 0U;
    const auto unassignedChecked = judgement::assembleGameplay(unassignedOrdinal);
    REQUIRE_FALSE(unassignedChecked.has_value());
    expectRejection(unassignedChecked.error(), "judgement.merge.declaration_ordinal_unassigned",
                    "invalid_relation", "declarationOrdinal");

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
                    "judgement.merge.cross_document_reference_implicit", "invalid_relation",
                    "references");

    //  The document component of a stable identity is the document the declaration lives in: a
    //  declaration may not claim another document's ordinal range.
    AssemblyRequest foreignDocument = fixture.request();
    foreignDocument.sources.front().document.declarations[1].stableId.sourceDocumentId = "doc.beta";
    const auto foreignChecked = judgement::assembleGameplay(foreignDocument);
    REQUIRE_FALSE(foreignChecked.has_value());
    expectRejection(foreignChecked.error(), "judgement.declaration.structurally_incomplete",
                    "invalid_relation", "sourceDocumentId");
    expectSection(foreignChecked.error(), "judgement.gameplay.merge");

    AssemblyRequest dangling = fixture.request();
    dangling.sources.front().document.declarations[1].references.push_back(
        judgement::DeclarationRef{.scope = judgement::ReferenceScope::sameDocument,
                                  .sourceDocumentId = {},
                                  .declarationOrdinal = 99U});
    const auto danglingChecked = judgement::assembleGameplay(dangling);
    REQUIRE_FALSE(danglingChecked.has_value());
    expectRejection(danglingChecked.error(), "judgement.merge.reference_dangling",
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

TEST_CASE("Declaration lookup preserves sparse cross-document metadata and boundary rejections",
          "[judgement][merge][declaration-lookup]") {
    const Fixture fixture;
    auto request = fixture.request();
    for (const auto documentId : {"doc.beta", "doc.gamma"}) {
        GameplaySource source{};
        source.form = SourceForm::memory;
        source.document.sourceDocumentId = documentId;
        source.document.chartVersion = 5;
        source.document.gameplayVersion = 2;
        request.sources.push_back(std::move(source));
    }
    constexpr std::uint32_t count = 257;
    for (std::size_t sourceIndex = 0; sourceIndex < request.sources.size(); ++sourceIndex) {
        auto& source = request.sources[sourceIndex];
        const auto& target = request.sources[(sourceIndex + 1) % request.sources.size()];
        for (std::uint32_t i = 0; i < count; ++i) {
            auto declaration = testing::testDeclaration(
                judgement::DeclarationKind::patternDefinition,
                source.document.sourceDocumentId + ".pattern." + std::to_string(i), 100 + 2 * i,
                source.document.sourceDocumentId);
            declaration.references = {
                {judgement::ReferenceScope::explicitCrossDocument, target.document.sourceDocumentId,
                 100 + 2 * ((i + 1) % count)},
                {judgement::ReferenceScope::explicitCrossDocument, target.document.sourceDocumentId,
                 100 + 2 * ((i + 1) % count)}};
            if (i % 2 == 0) {
                declaration.required.features = {{std::string{testing::kFeatureId}}};
            } else {
                declaration.required.capabilities = {{std::string{testing::kCapabilityId}, {}}};
            }
            source.document.declarations.push_back(std::move(declaration));
        }
    }
    const auto canonical = judgement::assembleGameplay(request);
    REQUIRE(canonical);
    for (const auto& source : request.sources) {
        for (const auto& declaration : source.document.declarations) {
            const auto& rows = canonical->graph.mergedNamespace.declarations;
            const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
                return row.stableId == declaration.stableId;
            });
            REQUIRE(found != rows.end());
            CHECK(found->name == declaration.localName);
            if (declaration.stableId.declarationOrdinal >= 100) {
                const auto& ref = declaration.references.front();
                CHECK(found->references == std::vector<judgement::StableDeclarationId>{
                                               {ref.sourceDocumentId, ref.declarationOrdinal}});
                CHECK(found->requiredFeatureIds ==
                      (declaration.required.features.empty()
                           ? std::vector<std::string>{}
                           : std::vector<std::string>{std::string{testing::kFeatureId}}));
                CHECK(found->requiredCapabilityIds ==
                      (declaration.required.capabilities.empty()
                           ? std::vector<std::string>{}
                           : std::vector<std::string>{std::string{testing::kCapabilityId}}));
            }
        }
    }
    auto reordered = request;
    std::reverse(reordered.sources.begin(), reordered.sources.end());
    for (auto& source : reordered.sources) {
        std::reverse(source.document.declarations.begin(), source.document.declarations.end());
    }
    const auto shuffled = judgement::assembleGameplay(reordered);
    REQUIRE(shuffled);
    CHECK(shuffled->graph.mergedNamespace == canonical->graph.mergedNamespace);
    CHECK(shuffled->prepared.canonicalBytes() == canonical->prepared.canonicalBytes());

    SECTION("reference before the first declaration") {
        request.sources[0].document.declarations.back().references[0].declarationOrdinal = 0;
    }
    SECTION("reference in an ordinal gap") {
        request.sources[0].document.declarations.back().references[0].declarationOrdinal = 101;
    }
    SECTION("reference after the last declaration") {
        request.sources[0].document.declarations.back().references[0].declarationOrdinal = 999;
    }
    SECTION("reference names a document before the namespace") {
        request.sources[0].document.declarations.back().references[0].sourceDocumentId = "aaa";
    }
    SECTION("reference names a document after the namespace") {
        request.sources[0].document.declarations.back().references[0].sourceDocumentId = "zzz";
    }
    const auto invalid = judgement::assembleGameplay(request);
    REQUIRE_FALSE(invalid);
    expectRejection(invalid.error(), "judgement.merge.reference_dangling", "invalid_relation",
                    "references");
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
               "judgement.content.unsupported_form", "non_terminating_source", "", "",
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
    expectRejection(nodeChecked.error(), "judgement.content.unsupported_form",
                    "non_terminating_source", "pattern.root.unsupportedForm");

    //  Content whose semantics require Release / tail while no tail phase is declared is refused
    //  rather than given an inferred tail (Spec 3.8.3).
    AssemblyRequest implicitTail = fixture.request();
    implicitTail.sources.front().document.requirements.front().requiresReleaseTailSemantics = true;
    const auto implicitTailChecked = judgement::assembleGameplay(implicitTail);
    REQUIRE_FALSE(implicitTailChecked.has_value());
    expectRejection(implicitTailChecked.error(), "judgement.requirement.release_tail_implicit",
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
    expectRejection(danglingResourceChecked.error(), "judgement.resource.reference_dangling",
                    "invalid_relation", "requirements[0].resourceClaims");
}
