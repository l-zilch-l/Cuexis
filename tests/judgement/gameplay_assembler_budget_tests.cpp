//  S7A-3 assembler cases. See gameplay_assembler_test_fixture.hpp for the shared
//  aliases and rejection helpers.

#include "gameplay_assembler_test_fixture.hpp"

// Closure precedence, projection, atomicity, budget, grace and resource plan cases.

TEST_CASE("S7A-3 closure completeness is never filled in", "[judgement][s7a-3][closure]") {
    const Fixture fixture;

    AssemblyRequest missingCapability = fixture.request();
    missingCapability.declaredCapabilities.capabilities.clear();
    const auto missingCapabilityChecked = judgement::assembleGameplay(missingCapability);
    REQUIRE_FALSE(missingCapabilityChecked.has_value());
    expectRejection(missingCapabilityChecked.error(), "judgement.closure.declared_incomplete",
                    "identity_closure_incomplete", "declaredCapabilities.capabilityId");
    expectSection(missingCapabilityChecked.error(), "judgement.gameplay.closure");

    AssemblyRequest missingFeature = fixture.request();
    missingFeature.declaredFeatures.features.clear();
    const auto missingFeatureChecked = judgement::assembleGameplay(missingFeature);
    REQUIRE_FALSE(missingFeatureChecked.has_value());
    expectRejection(missingFeatureChecked.error(), "judgement.closure.declared_incomplete",
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
    expectRejection(disabledChecked.error(), "judgement.closure.capability_disabled",
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

TEST_CASE("S7A-3 overlapping closure contributions do not satisfy content declarations",
          "[judgement][s7a-3][closure]") {
    const Fixture fixture;
    AssemblyRequest request = fixture.request();
    auto& document = request.sources.front().document;
    const auto contentNeeds = document.requirements.front().required;
    document.requirements.front().required.capabilities.push_back(
        contentNeeds.capabilities.front());
    document.requirements.front().required.features.push_back(contentNeeds.features.front());
    document.requirements.front().pattern.required = contentNeeds;
    document.declarations.front().required = contentNeeds;
    const auto rulesetNeeds = request.closureContributions;
    request.closureContributions.capabilities.push_back(contentNeeds.capabilities.front());
    request.closureContributions.capabilities.push_back(contentNeeds.capabilities.front());
    request.closureContributions.capabilities.push_back(rulesetNeeds.capabilities.front());
    request.closureContributions.features.push_back(contentNeeds.features.front());
    request.closureContributions.features.push_back(contentNeeds.features.front());
    request.closureContributions.features.push_back(rulesetNeeds.features.front());

    std::string missingPath;
    SECTION("overlapping repeated needs still report missing content capabilities first") {
        request.declaredCapabilities.capabilities.clear();
        request.declaredFeatures.features.clear();
        missingPath = "declaredCapabilities.capabilityId";
    }
    SECTION("overlapping repeated features still need a content declaration") {
        request.declaredFeatures.features.clear();
        missingPath = "declaredFeatures.featureId";
    }
    SECTION("a declaration of another capability revision does not cover the content need") {
        request.declaredCapabilities.capabilities.front().revision = "revision.other";
        missingPath = "declaredCapabilities.capabilityId";
    }
    SECTION("one declaration covers repeats while contribution-only needs stay out") {}

    const AssemblyRequest original = request;
    const auto assembled = judgement::assembleGameplay(request);
    //  Neither successful derivation nor a completeness rejection may sort, erase or fill the
    //  caller's repeated source needs, contributions or declarations in place.
    CHECK(request.declaredCapabilities == original.declaredCapabilities);
    CHECK(request.declaredFeatures == original.declaredFeatures);
    CHECK(request.closureContributions == original.closureContributions);
    CHECK(document.requirements.front().required ==
          original.sources.front().document.requirements.front().required);
    CHECK(document.requirements.front().pattern.required ==
          original.sources.front().document.requirements.front().pattern.required);
    CHECK(document.declarations == original.sources.front().document.declarations);
    if (!missingPath.empty()) {
        REQUIRE_FALSE(assembled.has_value());
        expectRejection(assembled.error(), "judgement.closure.declared_incomplete",
                        "identity_closure_incomplete", missingPath);
        expectSection(assembled.error(), "judgement.gameplay.closure");
    } else {
        REQUIRE(assembled.has_value());
        const auto& graph = assembled.value().graph;
        REQUIRE(graph.derivedCapabilities.capabilities.size() == 2U);
        CHECK(graph.derivedCapabilities.capabilities.front() == contentNeeds.capabilities.front());
        CHECK(graph.derivedCapabilities.capabilities.back() == rulesetNeeds.capabilities.front());
        REQUIRE(graph.derivedFeatures.features.size() == 2U);
        CHECK(graph.derivedFeatures.features.front() == contentNeeds.features.front());
        CHECK(graph.derivedFeatures.features.back() == rulesetNeeds.features.front());
        CHECK(graph.declaredCapabilities == original.declaredCapabilities);
        CHECK(graph.declaredFeatures == original.declaredFeatures);
    }
}

TEST_CASE("S7A-3 closure unknown checks precede disabled and completeness checks",
          "[judgement][s7a-3][closure]") {
    const Fixture fixture;
    AssemblyRequest request = fixture.request();
    //  Both unknown ids sort after the fixture's disabled content capability. Their input order
    //  is reversed so the first diagnostic must follow the canonical closure, not input order.
    request.closureContributions.capabilities.push_back(
        judgement::CapabilityRef{.capabilityId = "test.capability.unknown.z", .revision = {}});
    request.closureContributions.capabilities.push_back(
        judgement::CapabilityRef{.capabilityId = "test.capability.unknown.a", .revision = {}});
    request.capabilityContext.enabledCapabilityIds.clear();
    request.declaredCapabilities.capabilities.clear();
    request.declaredFeatures.features.clear();

    std::string expectedCode = "capability.unknown";
    std::string expectedCategory = "unknown_capability";
    std::string expectedCapability = "test.capability.unknown.a";
    SECTION("the first unknown wins over earlier disabled capabilities and missing declarations") {}
    SECTION("after all ids are recognised the first disabled wins over missing declarations") {
        request.capabilityContext.recognisedCapabilityIds.push_back("test.capability.unknown.z");
        request.capabilityContext.recognisedCapabilityIds.push_back("test.capability.unknown.a");
        expectedCode = "judgement.closure.capability_disabled";
        expectedCategory = "capability_disabled";
        expectedCapability = "test.capability.one";
    }

    const AssemblyRequest original = request;
    const auto assembled = judgement::assembleGameplay(request);
    CHECK(request.declaredCapabilities == original.declaredCapabilities);
    CHECK(request.declaredFeatures == original.declaredFeatures);
    CHECK(request.closureContributions == original.closureContributions);
    CHECK(request.capabilityContext.recognisedCapabilityIds ==
          original.capabilityContext.recognisedCapabilityIds);
    CHECK(request.capabilityContext.enabledCapabilityIds ==
          original.capabilityContext.enabledCapabilityIds);
    CHECK(request.sources.front().document.requirements.front().required ==
          original.sources.front().document.requirements.front().required);
    REQUIRE_FALSE(assembled.has_value());
    expectRejection(assembled.error(), expectedCode, expectedCategory,
                    "derivedCapabilities.capabilityId");
    expectSection(assembled.error(), "judgement.gameplay.closure");
    CHECK(contextValue(assembled.error(), "capabilityId") == expectedCapability);
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
    judgement::PatternNodeDeclaration chain{
        judgement::PatternPrimitive::atom, {}, std::move(atomRef), {}, {}};
    for (std::size_t level = 1U; level < depth; ++level) {
        judgement::PatternNodeDeclaration step{
            judgement::PatternPrimitive::atom, {}, "atom.step", {}, {}};
        chain = judgement::PatternNodeDeclaration{
            judgement::PatternPrimitive::sequence, {std::move(chain), std::move(step)}, {}, {}, {}};
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
    expectRejection(exceededChecked.error(), "judgement.pattern.arm_bound_exceeded",
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
    expectRejection(exceededChecked.error(), "judgement.budget.content_profile_exceeded",
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
    expectRejection(overBudgetChecked.error(), "judgement.budget.content_profile_exceeded",
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

TEST_CASE("S7A-3 grace source selection is explicit and half-even", "[judgement][s7a-3][grace]") {
    const auto half = testing::makeDuration(1, 2);
    const auto one = testing::makeDuration(1, 1);
    judgement::GraceDeclaration inherited{
        .policy = judgement::GraceResolutionPolicy::inheritedDeclaration,
        .inheritedFromDeclarationId = "grace.source",
        .allowChartGrace = false};
    judgement::GraceResolutionInputs inputs{.unitInTicks = one,
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
    expectRejection(missing.error(), "judgement.grace.value_missing", "invalid_relation",
                    "preparedGrace");
}

TEST_CASE("S7A-3 resource plan rejects duplicate occupying claim keys",
          "[judgement][s7a-3][resource]") {
    const judgement::ResourceClaimResolutionInputs inputs{
        .declaredCapacity = 1U,
        .intents = {},
        .candidates =
            {
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
    expectRejection(result.error(), "judgement.resource.claim_conflict", "invalid_relation",
                    "resourceClaims[1]");
}

TEST_CASE("S7A-3 resource plan rejects incomplete occupying declarations",
          "[judgement][s7a-3][resource][negative]") {
    using Candidate = judgement::ResourceClaimResolutionInputs::Candidate;
    const auto run = [](std::vector<Candidate> candidates,
                        std::vector<judgement::ResourceClaimIntent> intents = {}) {
        return judgement::resolveResourceClaims(judgement::ResourceClaimResolutionInputs{
            .declaredCapacity = 1U,
            .intents = std::move(intents),
            .candidates = std::move(candidates),
            .preparedGrace = judgement::PreparedGrace{judgement::TickSpan{0}}});
    };

    SECTION("capacity above the Stage 7A subset") {
        const auto result =
            judgement::resolveResourceClaims(judgement::ResourceClaimResolutionInputs{
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
        expectRejection(result.error(), "judgement.declaration.structurally_incomplete",
                        "invalid_relation", "resourceClaims[0]");
    }
    SECTION("occupying candidate without stable key") {
        const auto result = run({Candidate{.intent = judgement::ResourceClaimIntent::consume,
                                           .policyToken = "policy",
                                           .claimKeyToken = {}}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.declaration.structurally_incomplete",
                        "invalid_relation", "resourceClaims[0]");
    }
    SECTION("observe cannot carry an occupying key") {
        const auto result = run({Candidate{.intent = judgement::ResourceClaimIntent::observe,
                                           .policyToken = "policy",
                                           .claimKeyToken = "key"}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.resource.claim_conflict", "invalid_relation",
                        "resourceClaims");
    }
    SECTION("observe cannot carry a competition pair") {
        const auto result = run(
            {Candidate{.intent = judgement::ResourceClaimIntent::observe,
                       .policyToken = {},
                       .claimKeyToken = {},
                       .graceOverrideMode = judgement::GraceOverrideMode::none,
                       .competition = judgement::ClaimPolicyDeclaration::CompetitionKey{0, 0}}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.resource.claim_conflict", "invalid_relation",
                        "resourceClaims");
    }
    SECTION("legacy intent mismatch is rejected") {
        const auto result = run({Candidate{.intent = judgement::ResourceClaimIntent::claim,
                                           .policyToken = "policy",
                                           .claimKeyToken = "key"}},
                                {judgement::ResourceClaimIntent::consume});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "judgement.declaration.structurally_incomplete",
                        "invalid_relation", "resourceClaims[0]");
    }
    SECTION("unsupported grace override is rejected") {
        const auto result =
            run({Candidate{.intent = judgement::ResourceClaimIntent::claim,
                           .policyToken = "policy",
                           .claimKeyToken = "key",
                           .graceOverrideMode = judgement::GraceOverrideMode::sticky}});
        REQUIRE_FALSE(result);
        expectRejection(result.error(), "resource.handoff_unsupported", "capability_disabled",
                        "resourceClaims[0].graceOverride");
    }
}

TEST_CASE("Named pattern lookup preserves kind and merge errors after namespace movement",
          "[judgement][assembler][namespace]") {
    Fixture fixture;
    auto request = fixture.request();
    auto& document = request.sources.front().document;
    document.requirements.front().pattern.patternId = "zz.pattern";
    document.declarations.push_back(testing::testDeclaration(
        judgement::DeclarationKind::patternDefinition, "zz.pattern", 1000));
    document.declarations.push_back(testing::testDeclaration(
        judgement::DeclarationKind::patternDefinition, "aa.pattern", 1001));
    std::reverse(document.declarations.begin(), document.declarations.end());

    SECTION("name order differs from stable ID and input order") {
        auto assembled = judgement::assembleGameplay(request);
        REQUIRE(assembled);
        CHECK(document.declarations.front().stableId.declarationOrdinal == 1001);
        CHECK(assembled->chart.canonicalBytes().bytes() ==
              judgement::makeChartIdentity(assembled->graph).canonicalBytes().bytes());
        CHECK(assembled->content.canonicalBytes().bytes() ==
              judgement::makeContentIdentity(assembled->graph).canonicalBytes().bytes());
        CHECK(assembled->prepared.canonicalBytes().bytes() ==
              judgement::makePreparedIdentity(assembled->graph, request.identityDeclarations)
                  .canonicalBytes()
                  .bytes());
    }
    SECTION("the same name in another declaration kind is not a pattern") {
        auto named = std::find_if(document.declarations.begin(), document.declarations.end(),
                                  [](const auto& d) { return d.localName == "zz.pattern"; });
        REQUIRE(named != document.declarations.end());
        named->kind = judgement::DeclarationKind::resourceRecord;
        auto rejected = judgement::assembleGameplay(request);
        REQUIRE_FALSE(rejected);
        expectRejection(rejected.error(), "judgement.merge.reference_dangling", "invalid_relation",
                        "requirements[0].pattern");
    }
    SECTION("a missing name beyond the final entry stays dangling") {
        document.requirements.front().pattern.patternId = "zzzz.missing";
        auto rejected = judgement::assembleGameplay(request);
        REQUIRE_FALSE(rejected);
        expectRejection(rejected.error(), "judgement.merge.reference_dangling", "invalid_relation",
                        "requirements[0].pattern");
    }
    SECTION("duplicate merge names precede a dangling pattern") {
        document.requirements.front().pattern.patternId = "missing";
        document.declarations.push_back(testing::testDeclaration(
            judgement::DeclarationKind::patternDefinition, "aa.pattern", 1002));
        auto rejected = judgement::assembleGameplay(request);
        REQUIRE_FALSE(rejected);
        expectRejection(rejected.error(), "judgement.merge.declaration_name_duplicate",
                        "invalid_relation", "localName");
    }
}
