// Gameplay assembler diagnostics: the rejections and the stable enumeration tokens.

#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

namespace assembler_detail {

[[nodiscard]] auto rejection(const DiagnosticTokens& tokens) -> core::Error {
    const Diagnostic diagnostic{
        .code = tokens.code,
        .category = tokens.category,
        .severity = codes::kErrorSeverity,
        //  None of these rejections mutates session state, so none of them faults the session.
        .faulted = false,
        .summary = tokens.summary,
        .context =
            DiagnosticContext{
                .fieldPath = DiagnosticFieldPath{.section = tokens.section, .path = tokens.path},
                .requirement =
                    DiagnosticRequirementRef{.kind = codes::kAbsent, .identity = codes::kAbsent},
                .identity = DiagnosticIdentityComponent{.component = tokens.identityComponent,
                                                        .token = tokens.identityToken},
                .budget = nullptr,
                .capabilityId = tokens.capabilityId,
                .remediation = tokens.remediation,
                .rawTime = codes::kAbsent,
            },
    };
    return toError(diagnostic);
}

//  A structurally incomplete declaration (Spec 9.3, first class).
[[nodiscard]] auto declarationInvalidError(std::string_view code, std::string_view section,
                                           std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kInvalidRelationCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path});
}

//  A declared value that left the domain its declaration requires (Spec 9.3, second class).
[[nodiscard]] auto valueError(std::string_view code, std::string_view section,
                              std::string_view path, std::string_view summary) -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kBudgetExceededCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path});
}

//  A version gate: an older revision that was not migrated offline (Spec 2.1, Spec 7.2 R-17).
[[nodiscard]] auto versionError(std::string_view code, std::string_view path,
                                std::string_view summary) -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kAmbiguousMigrationCategory,
                                      .summary = summary,
                                      .section = codes::kEntrySection,
                                      .path = path});
}

//  A stable capability rejection with its capability id and replacement path.
[[nodiscard]] auto capabilityRejection(std::string_view code, std::string_view category,
                                       std::string_view section, std::string_view path,
                                       std::string_view summary, std::string_view capabilityId,
                                       std::string_view remediation) -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = category,
                                      .summary = summary,
                                      .section = section,
                                      .path = path,
                                      .capabilityId = capabilityId,
                                      .remediation = remediation});
}

//  A declared set smaller than the derived closure (Spec 6.2 rule 1).
[[nodiscard]] auto closureIncompleteError(std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = codes::kDeclaredClosureIncompleteCode,
                                      .category = codes::kIdentityClosureIncompleteCategory,
                                      .summary = summary,
                                      .section = codes::kClosureSection,
                                      .path = path});
}

//  ---------------------------------------------------------------------------------------------
//  Stable tokens of the enumerations
//  ---------------------------------------------------------------------------------------------
//
//  The identity bytes spell every enumeration by a token rather than by its ordinal value, so no
//  reader can mistake an enumerator's declared position for a contract. The token tables below are
//  the only spelling of these enumerations outside the type declarations.

[[maybe_unused, nodiscard]] auto entryKindToken(EntryKind kind) -> std::string_view {
    switch (kind) {
    case EntryKind::packedChart:
        return "packed-chart";
    case EntryKind::gameplayGraph:
        return "gameplay-graph";
    case EntryKind::authorSource:
        return "author-source";
    }
    return "?";
}

[[maybe_unused, nodiscard]] auto sourceFormToken(SourceForm form) -> std::string_view {
    switch (form) {
    case SourceForm::file:
        return "file";
    case SourceForm::memory:
        return "memory";
    }
    return "?";
}

[[nodiscard]] auto declarationKindToken(DeclarationKind kind) -> std::string_view {
    switch (kind) {
    case DeclarationKind::requirement:
        return "requirement";
    case DeclarationKind::patternDefinition:
        return "pattern";
    case DeclarationKind::measureDefinition:
        return "measure";
    case DeclarationKind::resourceRecord:
        return "resource";
    case DeclarationKind::judgementDomain:
        return "judgement-domain";
    case DeclarationKind::solverProfile:
        return "solver-profile";
    }
    return "?";
}

[[maybe_unused, nodiscard]] auto declarationRefScopeToken(ReferenceScope scope)
    -> std::string_view {
    switch (scope) {
    case ReferenceScope::sameDocument:
        return "same-document";
    case ReferenceScope::explicitCrossDocument:
        return "explicit-cross-document";
    }
    return "?";
}

[[nodiscard]] auto patternPrimitiveToken(PatternPrimitive primitive) -> std::string_view {
    switch (primitive) {
    case PatternPrimitive::atom:
        return "atom";
    case PatternPrimitive::sequence:
        return "sequence";
    case PatternPrimitive::choice:
        return "choice";
    case PatternPrimitive::boundedRepeat:
        return "bounded-repeat";
    case PatternPrimitive::skip:
        return "skip";
    case PatternPrimitive::instant:
        return "instant";
    case PatternPrimitive::complement:
        return "complement";
    }
    return "?";
}

[[nodiscard]] auto matchPolicyToken(MatchPolicy policy) -> std::string_view {
    switch (policy) {
    case MatchPolicy::leftmostFirst:
        return "leftmost-first";
    }
    return "?";
}

[[nodiscard]] auto phaseKindToken(PhaseKind kind) -> std::string_view {
    switch (kind) {
    case PhaseKind::tap:
        return "tap";
    case PhaseKind::head:
        return "head";
    case PhaseKind::body:
        return "body";
    case PhaseKind::tail:
        return "tail";
    }
    return "?";
}

[[nodiscard]] auto claimIntentToken(ResourceClaimIntent intent) -> std::string_view {
    switch (intent) {
    case ResourceClaimIntent::observe:
        return "observe";
    case ResourceClaimIntent::consume:
        return "consume";
    case ResourceClaimIntent::claim:
        return "claim";
    }
    return "?";
}

[[nodiscard]] auto graceOverrideToken(GraceOverrideMode mode) -> std::string_view {
    switch (mode) {
    case GraceOverrideMode::none:
        return "none";
    case GraceOverrideMode::sticky:
        return "sticky";
    case GraceOverrideMode::observe:
        return "observe";
    }
    return "?";
}

[[nodiscard]] auto gracePolicyToken(GraceResolutionPolicy policy) -> std::string_view {
    switch (policy) {
    case GraceResolutionPolicy::explicitDeclaration:
        return "explicit";
    case GraceResolutionPolicy::inheritedDeclaration:
        return "inherited";
    case GraceResolutionPolicy::defaultDeclaration:
        return "default";
    }
    return "?";
}

[[nodiscard]] auto frameResolutionToken(FrameResolution resolution) -> std::string_view {
    switch (resolution) {
    case FrameResolution::staticDeclaration:
        return "static";
    case FrameResolution::dynamicRuntimeFrame:
        return "dynamic";
    }
    return "?";
}

[[nodiscard]] auto relationKindToken(RelationKind kind) -> std::string_view {
    switch (kind) {
    case RelationKind::exclusive:
        return "exclusive";
    case RelationKind::binding:
        return "binding";
    case RelationKind::temporal:
        return "temporal";
    case RelationKind::quota:
        return "quota";
    }
    return "?";
}

[[nodiscard]] auto unsupportedKindToken(UnsupportedContentKind kind) -> std::string_view {
    switch (kind) {
    case UnsupportedContentKind::gameplayEffectsNotLowered:
        return "gameplay-effects-not-lowered";
    case UnsupportedContentKind::boundedRelationInstance:
        return "bounded-relation-instance";
    case UnsupportedContentKind::runtimeRepeatCounter:
        return "runtime-repeat-counter";
    case UnsupportedContentKind::dynamicRequirementGeneration:
        return "dynamic-requirement-generation";
    case UnsupportedContentKind::sameContact:
        return "same-contact";
    case UnsupportedContentKind::crossRequirementRelation:
        return "cross-requirement-relation";
    case UnsupportedContentKind::continuousTrajectory:
        return "continuous-trajectory";
    case UnsupportedContentKind::contactFollowingSlider:
        return "contact-following-slider";
    case UnsupportedContentKind::minimumReportRate:
        return "minimum-report-rate";
    case UnsupportedContentKind::reconstruction:
        return "reconstruction";
    case UnsupportedContentKind::handoffHold:
        return "handoff-hold";
    case UnsupportedContentKind::resourceMigration:
        return "resource-migration";
    case UnsupportedContentKind::resourceOwnerSet:
        return "resource-owner-set";
    case UnsupportedContentKind::resourceParallelSlot:
        return "resource-parallel-slot";
    }
    return "?";
}

[[nodiscard]] auto lateEventPolicyToken(LateEventPolicy policy) -> std::string_view {
    switch (policy) {
    case LateEventPolicy::rejectLate:
        return "reject-late";
    case LateEventPolicy::queueNextTick:
        return "queue-next-tick";
    }
    return "?";
}

[[nodiscard]] auto mapUnsupported(UnsupportedContentKind kind) -> UnsupportedMapping {
    switch (kind) {
    case UnsupportedContentKind::gameplayEffectsNotLowered:
        return UnsupportedMapping{.code = codes::kMigrationAmbiguousCode,
                                  .category = codes::kAmbiguousMigrationCategory,
                                  .capabilityId = codes::kAbsent,
                                  .remediation = codes::kAbsent};
    case UnsupportedContentKind::boundedRelationInstance:
        return UnsupportedMapping{.code = codes::kUnsupportedContentCode,
                                  .category = codes::kNonTerminatingSourceCategory,
                                  .capabilityId = codes::kAbsent,
                                  .remediation = codes::kAbsent};
    case UnsupportedContentKind::runtimeRepeatCounter:
    case UnsupportedContentKind::dynamicRequirementGeneration:
        return UnsupportedMapping{.code = codes::kCapabilityPermanentlyUnsupportedCode,
                                  .category = codes::kCapabilityDisabledCategory,
                                  .capabilityId = codes::kAbsent,
                                  .remediation = codes::kAbsent};
    case UnsupportedContentKind::sameContact:
    case UnsupportedContentKind::crossRequirementRelation:
        return UnsupportedMapping{.code = codes::kPatternRelationUnsupportedCode,
                                  .category = codes::kCapabilityDisabledCategory,
                                  .capabilityId = codes::kPatternRelationCapabilityId,
                                  .remediation = codes::kLaterBatchRemediation};
    case UnsupportedContentKind::continuousTrajectory:
    case UnsupportedContentKind::contactFollowingSlider:
    case UnsupportedContentKind::minimumReportRate:
    case UnsupportedContentKind::reconstruction:
        //  Spec 9.3 item 4: the two continuity families are separated by the field path, and both
        //  are the frozen R-05 rejection with its frozen capability id and remediation.
        return UnsupportedMapping{.code = codes::kInputContinuousUnsupportedCode,
                                  .category = codes::kCapabilityDisabledCategory,
                                  .capabilityId = codes::kContinuousCapabilityId,
                                  .remediation = codes::kContinuousRemediation};
    case UnsupportedContentKind::handoffHold:
    case UnsupportedContentKind::resourceMigration:
        return UnsupportedMapping{.code = codes::kResourceHandoffUnsupportedCode,
                                  .category = codes::kCapabilityDisabledCategory,
                                  .capabilityId = codes::kResourceHandoffCapabilityId,
                                  .remediation = codes::kLaterBatchRemediation};
    case UnsupportedContentKind::resourceOwnerSet:
    case UnsupportedContentKind::resourceParallelSlot:
        return UnsupportedMapping{.code = codes::kResourceCapacityUnsupportedCode,
                                  .category = codes::kCapabilityDisabledCategory,
                                  .capabilityId = codes::kResourceCapacityCapabilityId,
                                  .remediation = codes::kLaterBatchRemediation};
    }
    return UnsupportedMapping{.code = codes::kUnsupportedContentCode,
                              .category = codes::kCapabilityDisabledCategory,
                              .capabilityId = codes::kAbsent,
                              .remediation = codes::kAbsent};
}

} // namespace assembler_detail

} // namespace cuexis::judgement
