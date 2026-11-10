#pragma once

// Section names, context field paths and identity component tokens reported by the judgement
// diagnostics. They are a token set of their own, so they live outside the code table.

#include <string_view>

namespace cuexis::judgement::codes {

//  --- sections and field paths ------------------------------------------------------------------

inline constexpr std::string_view kEntrySection{"judgement.gameplay.entry"};
inline constexpr std::string_view kMergeSection{"judgement.gameplay.merge"};
inline constexpr std::string_view kGraphSection{"judgement.gameplay.graph"};
inline constexpr std::string_view kDomainSection{"judgement.gameplay.domain"};
inline constexpr std::string_view kClosureSection{"judgement.gameplay.closure"};
inline constexpr std::string_view kBudgetSection{"judgement.gameplay.budget"};

inline constexpr std::string_view kEntryKindPath{"entryKind"};
inline constexpr std::string_view kPlaybackPath{"playback"};
inline constexpr std::string_view kChartVersionPath{"chartVersion"};
inline constexpr std::string_view kGameplayVersionPath{"gameplay.version"};
inline constexpr std::string_view kSourceDocumentsPath{"sources"};
inline constexpr std::string_view kSourceDocumentIdPath{"sourceDocumentId"};
inline constexpr std::string_view kCarrierProvenancePath{"carrierProvenance"};
inline constexpr std::string_view kTimebaseRefPath{"timebaseRef"};
inline constexpr std::string_view kLatePolicyReferencePath{"latePolicyRef"};
inline constexpr std::string_view kRulesetRefPath{"rulesetRef"};
inline constexpr std::string_view kGraphRevisionPath{"graphRevision"};
inline constexpr std::string_view kDeclarationsPath{"declarations"};
inline constexpr std::string_view kDeclarationOrdinalPath{"declarationOrdinal"};
inline constexpr std::string_view kLocalNamePath{"localName"};
inline constexpr std::string_view kReferencesPath{"references"};
inline constexpr std::string_view kRequirementsPath{"requirements"};
inline constexpr std::string_view kResourcesPath{"resources"};
inline constexpr std::string_view kRelationsPath{"relations"};
inline constexpr std::string_view kSolverProfilesPath{"solverProfiles"};
inline constexpr std::string_view kFactBindingsPath{"factBindings"};
inline constexpr std::string_view kJudgementDomainsPath{"judgementDomains"};
inline constexpr std::string_view kPatternPath{"pattern"};
inline constexpr std::string_view kMeasurePath{"measure"};
inline constexpr std::string_view kComponentsPath{"components"};
inline constexpr std::string_view kPhasesPath{"phases"};
inline constexpr std::string_view kRequiredActionsPath{"requiredActions"};
inline constexpr std::string_view kDomainBindingPath{"domainBinding"};
inline constexpr std::string_view kJudgementDomainRefPath{"judgementDomainId"};
inline constexpr std::string_view kResourceClaimPath{"resourceClaims"};
inline constexpr std::string_view kResourceRefPath{"resourceRef"};
inline constexpr std::string_view kClaimPolicyPath{"claimPolicy"};
inline constexpr std::string_view kClaimKeyPath{"claimKeyToken"};
inline constexpr std::string_view kPolicyTokenPath{"policyToken"};
inline constexpr std::string_view kIntentPath{"intent"};
inline constexpr std::string_view kGracePath{"grace"};
inline constexpr std::string_view kPreparedGracePath{"preparedGrace"};
inline constexpr std::string_view kGraceOverridePath{"graceOverride"};
inline constexpr std::string_view kCapacityPath{"declaredCapacity"};
inline constexpr std::string_view kSlotPath{"slotToken"};
inline constexpr std::string_view kGapGracePath{"declaredGapGrace"};
inline constexpr std::string_view kDecisionPolicyPath{"decisionPolicyRef"};
inline constexpr std::string_view kAxisTokenPath{"axisToken"};
inline constexpr std::string_view kAxesPath{"axes"};
inline constexpr std::string_view kCoordinateSystemPath{"coordinateSystemToken"};
inline constexpr std::string_view kFrameResolutionPath{"frameResolution"};
inline constexpr std::string_view kDomainIdPath{"domainId"};
inline constexpr std::string_view kPatternRootPath{"pattern.root"};
inline constexpr std::string_view kAtomRefPath{"atomRef"};
inline constexpr std::string_view kRepeatBoundsPath{"repeatBounds"};
inline constexpr std::string_view kUnsupportedFormPath{"unsupportedForm"};
inline constexpr std::string_view kSolverProfileRefPath{"solverProfileRef"};
inline constexpr std::string_view kLocalClosePolicyPath{"localClosePolicyToken"};
inline constexpr std::string_view kBindingIdPath{"bindingId"};
inline constexpr std::string_view kDeclaredCapabilitiesPath{"declaredCapabilities"};
inline constexpr std::string_view kDeclaredFeaturesPath{"declaredFeatures"};
inline constexpr std::string_view kDerivedCapabilitiesPath{"derivedCapabilities"};
inline constexpr std::string_view kDerivedFeaturesPath{"derivedFeatures"};
inline constexpr std::string_view kCapabilityIdPath{"capabilityId"};
inline constexpr std::string_view kCapabilityRevisionPath{"capability.revision"};
inline constexpr std::string_view kFeatureIdPath{"featureId"};
inline constexpr std::string_view kMergedNamespacePath{"mergedNamespace"};
inline constexpr std::string_view kClosureContributionsPath{"closureContributions"};
inline constexpr std::string_view kClosureContributionFeaturesPath{"closureContributionFeatures"};
inline constexpr std::string_view kResourceClosurePath{"resourceClosure"};
inline constexpr std::string_view kSourceClosurePath{"sourceClosure"};
inline constexpr std::string_view kDiagnosticMapPath{"diagnosticMap"};
inline constexpr std::string_view kContentProfilePath{"contentProfile"};
inline constexpr std::string_view kPatternDeclaredActionRefsPath{"declaredActionRefs"};
inline constexpr std::string_view kPatternMaxArmElementsPath{"maxArmElements"};
inline constexpr std::string_view kPatternMaxDeadlineElementsPath{"maxDeadlineElements"};
inline constexpr std::string_view kPatternMaxDeclarationDepthPath{"maxDeclarationDepth"};
inline constexpr std::string_view kPatternMaxExpansionCountPath{"maxExpansionCount"};
inline constexpr std::string_view kPatternMaxStateCountPath{"maxStateCount"};
inline constexpr std::string_view kPatternMaxEvaluationStepsPath{"maxEvaluationSteps"};
inline constexpr std::string_view kPatternMaxCompiledBytesPath{"maxCompiledBytes"};
inline constexpr std::string_view kCategoryTokenPath{"categoryToken"};
inline constexpr std::string_view kDeclaredGradeTokensPath{"declaredGradeTokens"};
inline constexpr std::string_view kIdentityComponentPath{"identity.component"};
inline constexpr std::string_view kEngineComponentToken{"engine"};
inline constexpr std::string_view kRulesetComponentToken{"ruleset"};
inline constexpr std::string_view kChartComponentToken{"chart"};
inline constexpr std::string_view kSessionComponentToken{"session"};

} // namespace cuexis::judgement::codes
