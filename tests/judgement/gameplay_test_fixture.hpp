#pragma once

//  S7A-3 shared test fixture: one valid canonical gameplay assembly request and the small helpers
//  the graph, assembler and identity suites read it with.
//
//  Every token, identity and number below is a test-local declaration. None of them is a frozen
//  registry entry, a production limit or a recommended value: the batch under test freezes the
//  declaration model and the rejection behaviour, and the tokens here exist so that a declaration
//  can be spelled at all. The late-policy magnitudes are deliberately meaningless presence probes,
//  for the same reason the S7A-2 suite uses meaningless ones.
//
//  The fixture keeps the two borrowed typed declarations (the timebase profile and the late-policy
//  parameters) as members, because the canonical graph borrows them (Spec 3.2 `timebaseRef`). A
//  test that copies the request must keep the Fixture object alive, which is what the Fixture type
//  is for.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::judgement::testing {

//  The declarations the fixture names. The "test."-prefixed capability and feature ids are the
//  fixture's own tokens; the capability registry is unfrozen (CM-X01) and this batch invents no
//  registry entry.
inline constexpr std::string_view kDocumentId{"doc.alpha"};
inline constexpr std::string_view kDomainId{"domain.main"};
inline constexpr std::string_view kResourceId{"resource.slot"};
inline constexpr std::string_view kSolverId{"solver.exclusive"};
inline constexpr std::string_view kRequirementLocalId{"requirement.one"};
inline constexpr std::string_view kCapabilityId{"test.capability.one"};
inline constexpr std::string_view kRulesetCapabilityId{"test.capability.ruleset"};
inline constexpr std::string_view kFeatureId{"test.feature.one"};
inline constexpr std::string_view kRulesetFeatureId{"test.feature.ruleset"};
inline constexpr std::string_view kPolicyToken{"policy.exclusive"};
inline constexpr std::string_view kClaimKeyToken{"claim.key.one"};

//  The fixture's declaration ordinals. They are declarations of the source document, so they travel
//  with the declarations and a permutation of the input array cannot renumber them.
inline constexpr std::uint32_t kDomainOrdinal{1};
inline constexpr std::uint32_t kResourceOrdinal{2};
inline constexpr std::uint32_t kSolverOrdinal{3};
inline constexpr std::uint32_t kRequirementOrdinal{4};

[[nodiscard]] inline auto contextValue(const core::Error& error, std::string_view key)
    -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

[[nodiscard]] inline auto makeBeat(std::int64_t numerator, std::int64_t denominator)
    -> RationalBeat {
    const auto created = RationalBeat::create(numerator, denominator);
    REQUIRE(created.has_value());
    return created.value();
}

[[nodiscard]] inline auto makeDuration(std::int64_t numerator, std::int64_t denominator)
    -> RationalDuration {
    const auto created = RationalDuration::create(numerator, denominator);
    REQUIRE(created.has_value());
    return created.value();
}

//  A validated timebase profile: the declared microsecond unit with an explicitly declared tempo,
//  so that the S7A-2 prepare gate is satisfied rather than worked around.
[[nodiscard]] inline auto makeTestTimebase() -> TimebaseProfile {
    return TimebaseProfile{
        .profileId = "engine.tick.us.v1",
        .unitToken = "us",
        .tickScale = makeDuration(1, 1),
        .originBeat = makeBeat(0, 1),
        .initialTempo = makeDuration(500000, 1),
        .tempoSections = {},
        .stopSections = {},
    };
}

//  Every late-policy parameter is measured, because a pending one is itself a prepare rejection.
[[nodiscard]] inline auto makeTestLatePolicy() -> LatePolicyParameters {
    return LatePolicyParameters{
        .finalizationWatermark = MeasuredParameter<TickSpan>::measured(TickSpan{4}),
        .maxQueueHop = MeasuredParameter<TickSpan>::measured(TickSpan{2}),
        .windowCloseThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{8}),
        .windowOpenThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{1}),
        .policy = LateEventPolicy::rejectLate,
    };
}

[[nodiscard]] inline auto testDeclaration(DeclarationKind kind, std::string name,
                                          std::uint32_t ordinal,
                                          std::string_view documentId = kDocumentId)
    -> LocalDeclaration {
    return LocalDeclaration{.stableId =
                                StableDeclarationId{.sourceDocumentId = std::string{documentId},
                                                    .declarationOrdinal = ordinal},
                            .kind = kind,
                            .localName = std::move(name),
                            .references = {},
                            .required = RequiredRefs{}};
}

//  The canonical Requirement identity of the fixture (Spec 4 six-tuple).
[[nodiscard]] inline auto testRequirementIdentity() -> RequirementIdentity {
    return RequirementIdentity{
        .chartEntryId = "chart.entry.one",
        .invocationId = "invocation.one",
        .moduleId = "module.one",
        .exportId = "export.one",
        .emissionPath = {EmissionPathStep{.nodeId = "emission.node.one", .repeatIndex = 0U}},
        .requirementLocalId = std::string{kRequirementLocalId},
    };
}

[[nodiscard]] inline auto testRequirementStableId() -> StableDeclarationId {
    return StableDeclarationId{.sourceDocumentId = std::string{kDocumentId},
                               .declarationOrdinal = kRequirementOrdinal};
}

[[nodiscard]] inline auto makeTestJudgementDomain() -> JudgementDomainRecord {
    return JudgementDomainRecord{
        .domainId = std::string{kDomainId},
        .coordinateSystemToken = "coordinate.plane",
        .axes = {JudgementAxisRange{.axisToken = "axis.horizontal", .minimum = 0, .maximum = 1000}},
        .frameResolution = FrameResolution::staticDeclaration,
        .dynamicFrameProviderToken = {},
        .required = RequiredRefs{},
    };
}

[[nodiscard]] inline auto makeTestResource() -> ResourceRecord {
    return ResourceRecord{
        .ref = ResourceRef{.resourceId = std::string{kResourceId}},
        .declaredCapacity = 1U,
        .slotToken = "slot.one",
        .decisionPolicyRef = "decision.policy.one",
        .terminalAfterTermination = false,
        .declaredGapGrace = TickSpan{0},
        .unsupportedForms = {},
        .required = RequiredRefs{},
    };
}

[[nodiscard]] inline auto makeTestSolverProfile() -> SolverProfileDeclaration {
    return SolverProfileDeclaration{
        .solverId = std::string{kSolverId},
        .revision = "revision.one",
        .algorithmToken = "coordinator.policy.greedy_v1",
        .objective = {"first-eligible"},
        .tieBreak = {"priority.asc", "tieRank.asc"},
        .rejectIfNonUnique = true,
        .required = RequiredRefs{},
    };
}

[[nodiscard]] inline auto makeTestRequirement() -> RequirementRecord {
    auto action = RequiredActionRef::fromToken("action.primary");
    REQUIRE(action.has_value());
    auto binding = DomainBindingRef::fromToken("domain.binding.one");
    REQUIRE(binding.has_value());
    return RequirementRecord{
        .stableId = testRequirementStableId(),
        .identity = testRequirementIdentity(),
        .requiredActions = {action.value()},
        .domainBinding = binding.value(),
        .judgementDomainId = std::string{kDomainId},
        .phases = {PhaseDeclaration{.kind = PhaseKind::head, .declarationOrdinal = 1U},
                   PhaseDeclaration{.kind = PhaseKind::body, .declarationOrdinal = 2U}},
        .requiresReleaseTailSemantics = false,
        .pattern =
            PatternDeclaration{.patternId = {},
                               .matchPolicy = MatchPolicy::leftmostFirst,
                               .root = PatternNodeDeclaration{PatternPrimitive::atom, {},
                                                              "atom.one", {}, {}},
                               .required = RequiredRefs{}},
        .maxArmElements = MeasuredParameter<std::uint64_t>::measured(1U),
        .maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(1U),
        .measure =
            //  The declared category token is the one the phase derives (Spec 3.20 rule 3), because
            //  the category is derived from the phase and a caller-assigned category is refused.
        MeasureSpecDeclaration{
            .components = {MeasureComponentDeclaration{
                .phase = PhaseKind::head, .categoryToken = "hold_head", .declaredGradeTokens = {}}},
            .required = RequiredRefs{}},
        .resourceClaims = {ResourceClaimDeclaration{
            .resourceRef = ResourceRef{.resourceId = std::string{kResourceId}},
            .claimPolicy = ClaimPolicyDeclaration{.policyToken = std::string{kPolicyToken},
                                                  .claimKeyToken = std::string{kClaimKeyToken}},
            .intent = ResourceClaimIntent::claim,
            .graceOverride =
                GraceOverrideDeclaration{.mode = GraceOverrideMode::none, .overrideToken = {}}}},
        .grace = GraceDeclaration{.policy = GraceResolutionPolicy::explicitDeclaration,
                                  .inheritedFromDeclarationId = {},
                                  .allowChartGrace = false},
        .preparedGrace = PreparedGrace{TickSpan{3}},
        .factBindingRefs = {"binding.one"},
        .solverProfileRef = std::string{kSolverId},
        .localClosePolicyToken = "close.policy.one",
        .required = RequiredRefs{.features = {FeatureRef{.featureId = std::string{kFeatureId}}},
                                 .capabilities = {CapabilityRef{
                                     .capabilityId = std::string{kCapabilityId}, .revision = {}}}},
        .unsupportedForms = {},
    };
}

[[nodiscard]] inline auto makeTestRelation() -> RelationDeclaration {
    return RelationDeclaration{
        .kind = RelationKind::exclusive,
        .resourceRef = ResourceRef{.resourceId = std::string{kResourceId}},
        .members = {testRequirementStableId()},
        .policy = ClaimPolicyDeclaration{.policyToken = std::string{kPolicyToken},
                                         .claimKeyToken = std::string{kClaimKeyToken}},
        .declaredCapacity = 1U,
        .required = RequiredRefs{},
    };
}

//  One typed source document holding the fixture's whole content. Everything is declared in the
//  merged namespace, because the namespace is the one authority for names (P1-02).
[[nodiscard]] inline auto makeTestSourceDocument() -> GameplaySourceDocument {
    return GameplaySourceDocument{
        .sourceDocumentId = std::string{kDocumentId},
        .chartVersion = 5U,
        .gameplayVersion = 2U,
        .declarations = {testDeclaration(DeclarationKind::judgementDomain, std::string{kDomainId},
                                         kDomainOrdinal),
                         testDeclaration(DeclarationKind::resourceRecord, std::string{kResourceId},
                                         kResourceOrdinal),
                         testDeclaration(DeclarationKind::solverProfile, std::string{kSolverId},
                                         kSolverOrdinal),
                         testDeclaration(DeclarationKind::requirement,
                                         std::string{kRequirementLocalId}, kRequirementOrdinal)},
        .requirements = {makeTestRequirement()},
        .resources = {makeTestResource()},
        .relations = {makeTestRelation()},
        .solverProfiles = {makeTestSolverProfile()},
        .factBindings = {FactBindingRef{.bindingId = "binding.one"}},
        .judgementDomains = {makeTestJudgementDomain()},
    };
}

[[nodiscard]] inline auto makeTestEngineIdentity() -> EngineIdentityDeclaration {
    return EngineIdentityDeclaration{
        .judgementSemanticRevision = "engine.judgement.revision.one",
        .factSemanticRevision = "engine.fact.revision.one",
        .fixedPointTableId = "fixed.point.table.one",
        .coordinationPhaseOrderToken = "coordination.phase.order.one",
    };
}

[[nodiscard]] inline auto makeTestIdentityDeclarations() -> PreparedIdentityDeclarations {
    return PreparedIdentityDeclarations{
        .engine = makeTestEngineIdentity(),
        .ruleset =
            RulesetIdentityDeclaration{.interfaceProjectionToken = "interface.projection.one",
                                       .moduleOrder = {"module.one"},
                                       .buildHash = "ruleset.build.hash.one"},
        .session = SessionIdentityDeclaration{.loadoutToken = "loadout.one",
                                              .defaultGraceSourceToken = "grace.source.one",
                                              .normalizationProfileToken = "normalization.one",
                                              .judgementConfigToken = "judgement.config.one"},
    };
}

//  The canonical valid request. `timebase` and `latePolicy` are borrowed by the graph, so the
//  caller keeps them alive; `Fixture` below is the ordinary way to do that.
[[nodiscard]] inline auto makeValidRequest(const TimebaseProfile& timebase,
                                           const LatePolicyParameters& latePolicy)
    -> AssemblyRequest {
    return AssemblyRequest{
        .entryKind = EntryKind::gameplayGraph,
        .playback = true,
        .sources = {GameplaySource{.form = SourceForm::memory,
                                   .provenanceToken = "provenance.memory.one",
                                   .document = makeTestSourceDocument()}},
        .graphRevision = 7U,
        .rulesetRef = "ruleset.one",
        .declaredCapabilities =
            DeclaredCapabilitySet{.capabilities = {CapabilityRef{
                                      .capabilityId = std::string{kCapabilityId}, .revision = {}}}},
        .declaredFeatures =
            FeatureClosure{.features = {FeatureRef{.featureId = std::string{kFeatureId}}}},
        .closureContributions =
            ClosureContributions{
                .features = {FeatureRef{.featureId = std::string{kRulesetFeatureId}}},
                .capabilities = {CapabilityRef{.capabilityId = std::string{kRulesetCapabilityId},
                                               .revision = {}}}},
        .compilerProfileToken = "compiler.profile.one",
        .timebase = &timebase,
        .latePolicy = &latePolicy,
        .capabilityContext =
            CompileCapabilityContext{.recognisedCapabilityIds = {std::string{kCapabilityId},
                                                                 std::string{kRulesetCapabilityId}},
                                     .enabledCapabilityIds = {std::string{kCapabilityId},
                                                              std::string{kRulesetCapabilityId}}},
        .contentProfileLimits = ContentProfileLimits{},
        .identityDeclarations = makeTestIdentityDeclarations(),
    };
}

//  Keeps the two borrowed declarations at a stable address for the lifetime of one test case.
struct Fixture final {
    TimebaseProfile timebase{makeTestTimebase()};
    LatePolicyParameters latePolicy{makeTestLatePolicy()};

    [[nodiscard]] auto request() const -> AssemblyRequest {
        return makeValidRequest(timebase, latePolicy);
    }
};

} // namespace cuexis::judgement::testing
