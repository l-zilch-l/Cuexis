#pragma once

//  Judgement typed kernel - S7A-2 diagnostic code tokens.
//
//  The machine-readable code table, the category set and the severity set belong to round 6
//  (CM-D04 / P2-04 / P2-06) and are still unfrozen; their creation is a separate accepted work
//  item, so no code is registered yet. The tokens below are therefore the S7A-2 native spelling of
//  the codes the batch has to return, and they live in a src-only header that is never installed.
//
//  Each token is either
//
//    * a category or a candidate code that the Gameplay V2 Spec S7A-2 mapping table already names
//      (`budget_exceeded`, `late_policy_incomplete`, `input.continuous_unsupported` -- the latter
//      is the R-05 candidate code of Spec 7.2 and the value ABI's code-family list records for the
//      input family), or
//    * an S7A-2 local token, marked `s7a2`, for a rejection the batch must distinguish but the Spec
//      has not named yet. A local token is not a registered code and must not be treated as one.
//
//  No token here may be written into a public header: ABI forbids publishing a code that the code
//  table does not register, and this batch registers none.

#include <string_view>

namespace cuexis::judgement::codes {

//  ---------------------------------------------------------------------------------------------
//  Categories named by the Gameplay V2 Spec
//  ---------------------------------------------------------------------------------------------

//  Spec 9.2 category list; the S7A-2 mapping table of Spec 3.7.8 assigns tick overflow and amount
//  range / narrowing / quantization overflow to this category.
inline constexpr std::string_view kBudgetExceededCategory{"budget_exceeded"};
//  Spec 9.2 category list; Spec 3.7.4 item 3 and item 4 assign a missing or unmeasured late-policy
//  parameter and a duplicate queue entry to this category.
inline constexpr std::string_view kLatePolicyIncompleteCategory{"late_policy_incomplete"};
//  Spec 9.2 category list. The nine Spec categories contain no "malformed declaration" category, so
//  a declaration whose canonical structure or ordering is not valid, and a call that violates a
//  declared ordering contract, are reported as invalid_relation: of the nine it is the one that
//  addresses "the declared relationship is not the one the contract permits". Adding a tenth
//  category here would preempt CM-D04, and reusing budget_exceeded would misreport a structural
//  fault as a range fault.
inline constexpr std::string_view kInvalidRelationCategory{"invalid_relation"};

//  ---------------------------------------------------------------------------------------------
//  Codes that map onto an existing Spec 7.2 rejection entry (no new R number)
//  ---------------------------------------------------------------------------------------------

//  R-05 candidate code (Spec 7.2) for continuous trajectories, sliders, area coverage, minimum
//  report rate and reconstruction. ABI's input and geometry code family records the same code.
inline constexpr std::string_view kInputContinuousUnsupportedCode{"input.continuous_unsupported"};
//  The capability id that goes with the R-05 rejection. Spec 7.2 rule 1 requires a capability
//  rejection to name a capability id and a replacement path.
inline constexpr std::string_view kContinuousCapabilityId{"input.trajectory.v1"};
//  The replacement path of the R-05 rejection: the continuous input capability is admitted by
//  S7B-1, not by Stage 7A (plan S7A-2 item 5).
inline constexpr std::string_view kContinuousRemediation{"S7B-1"};

//  ---------------------------------------------------------------------------------------------
//  S7A-2 local refinement tokens
//  ---------------------------------------------------------------------------------------------
//
//  These distinguish *which* value failed inside a category the Spec already owns. They are not new
//  Spec entries and they do not change the Spec 7.2 count of 19.

//  Tick overflow: a tick value, displacement or difference left the signed 64-bit range. Spec
//  3.7.8 maps this onto the budget_exceeded category plus the Spec 9.3 "out of range is atomic
//  failure" path. The same token covers an intermediate rational that does not fit the signed
//  64-bit exact-rational representation the mapper works in: the mapping never approximates, so a
//  value it cannot hold exactly is a range failure of the same kind.
inline constexpr std::string_view kTickOverflowCode{"judgement.s7a2.timebase.tick_overflow"};

//  A profile-declared numeric value left the domain its own declaration requires (a non-positive
//  tick scale, initial tempo, tempo duration or stop duration). The value is representable but not
//  usable, which is the "declared value not satisfiable" case Spec 3.7.8 maps onto budget_exceeded.
inline constexpr std::string_view kProfileValueOutOfRangeCode{
    "judgement.s7a2.timebase.profile_value_out_of_range"};
//  A rational pair is not a representable exact rational: a zero denominator, an INT64_MIN
//  denominator, or an INT64_MIN numerator whose magnitude has no signed 64-bit representation.
inline constexpr std::string_view kRationalInvalidCode{"judgement.s7a2.timebase.rational_invalid"};
//  A half-open interval was declared with an end that precedes its start.
inline constexpr std::string_view kIntervalReversedCode{
    "judgement.s7a2.timebase.interval_reversed"};
//  A commit window's own boundaries contradict its commit tick, so no tick can satisfy it.
inline constexpr std::string_view kCommitWindowInvalidCode{
    "judgement.s7a2.timebase.commit_window_invalid"};
//  A fact commit was attempted at a tick other than the window's commit tick.
inline constexpr std::string_view kCommitNotAtCommitTickCode{
    "judgement.s7a2.timebase.commit_not_at_commit_tick"};
//  An amount value left the declared AmountSpec range. Spec 3.7.6 item 3 / 3.7.8.
inline constexpr std::string_view kAmountOutOfRangeCode{"judgement.s7a2.input.amount_out_of_range"};
//  An amount could not be represented in the declared canonical integer quantization, that is the
//  narrowing case of Spec 3.7.6 item 3.
inline constexpr std::string_view kAmountNarrowedCode{"judgement.s7a2.input.amount_narrowed"};
//  A late-policy parameter is missing or left at pending measurement. Spec 3.7.4 item 3.
inline constexpr std::string_view kLatePolicyPendingCode{"judgement.s7a2.late.parameter_pending"};
//  No late event policy was declared for the session. Spec 3.7.4 items 1-3 and the Spec 9.3
//  prepare atomic-failure condition "the same-tick policy is not declared": the selection is a
//  declaration, not a measurement, so it gets its own token instead of borrowing the
//  pending-measurement one.
inline constexpr std::string_view kLatePolicyUndeclaredCode{
    "judgement.s7a2.late.policy_undeclared"};
//  A duplicate queue entry, detected from the canonical observation identity and never from an
//  ingress ordinal. Spec 3.7.4 item 4.
inline constexpr std::string_view kDuplicateQueueCode{"judgement.s7a2.late.duplicate_queue_entry"};
//  A queue hop beyond the declared maximum. The number is supplied by the profile, so this is a
//  declared-range failure and not a frozen limit.
inline constexpr std::string_view kQueueHopExceededCode{"judgement.s7a2.late.queue_hop_exceeded"};
//  The canonical calibrated session clock moved backwards. Spec 3.7.3 makes that clock the only
//  canonical source of observationTick, so a reversal is a rejection and not a reordering. Its
//  category is invalid_relation: a clock regression is a time-order relation error, which is the
//  Spec 9.3 case invalid_relation covers, while budget_exceeded is reserved for a numeric value
//  that leaves a declared range or a budget.
inline constexpr std::string_view kTimeReversalCode{"judgement.s7a2.timebase.time_reversal"};
//  A calibrated clock value that cannot be represented in the signed 64-bit tick domain reuses
//  kTickOverflowCode; a negative calibrated clock is legal and carries no rejection token at all
//  (S7A-2 input review, 2026-10-03).
//  The ingress sequence was already consumed. Spec 3.7.4 item 4 keeps sequence identity separate
//  from canonical observation identity, so this is reported on its own token.
inline constexpr std::string_view kIngressSequenceDuplicateCode{
    "judgement.s7a2.input.ingress_sequence_duplicate"};
//  Normalizing before a session clock was captured is an order-of-use violation; it repairs to the
//  existing invalid_relation atomic-failure path instead of a dedicated 7A token (S7A-2 input
//  review, 2026-10-03).
//  A profile declaration was structurally invalid: a non-positive scale or duration, an unordered
//  or duplicated tempo / stop section, or a missing explicit unit declaration.
inline constexpr std::string_view kProfileInvalidCode{"judgement.s7a2.timebase.profile_invalid"};
//  An InputMapping declaration is structurally invalid: a missing profile identity, version or
//  source class, a domain with no token, or the same domain token declared twice. Spec 3.7.8 and
//  Spec 9.3 put a structurally incomplete or self-contradicting declaration in invalid_relation,
//  and the duplicate-domain rule is the same "reject rather than resolve by position" rule the
//  timebase profile applies to a duplicated tempo or stop section.
inline constexpr std::string_view kInputMappingInvalidCode{
    "judgement.s7a2.input.mapping_declaration_invalid"};
//  An InputMapping profile was offered to a session that already holds one. Spec 5.2 makes the
//  mapping a session component and a runtime modification a stable rejection (CM-T09).
inline constexpr std::string_view kRuntimeMappingChangeCode{
    "judgement.s7a2.input.runtime_mapping_change"};
//  A second, different canonical observation arrived at a tick this session already admitted. This
//  is the explicit same-tick behavior of CM-T10, and it is decided by the canonical observation
//  identity, never by the arrival order.
inline constexpr std::string_view kSameTickCollisionCode{
    "judgement.s7a2.input.same_tick_collision"};

//  ---------------------------------------------------------------------------------------------
//  Severity, context field paths and the absence marker
//  ---------------------------------------------------------------------------------------------

inline constexpr std::string_view kErrorSeverity{"error"};

//  Category token of a capability rejection. Spec 9.2 lists the capability categories as
//  unknown_capability / capability_disabled and nothing else, so a capability that Stage 7A does
//  not open is reported as `capability_disabled` (the id is recognised, this compile does not
//  enable it) and only a genuinely unrecognised id is `unknown_capability`. There is deliberately
//  no token spelling a bare "capability" category: such a token existed here, it was not one of the
//  nine Spec 9.2 categories, and every path that emits a capability rejection now shares the two
//  legal spellings below.
inline constexpr std::string_view kCapabilityDisabledCategory{"capability_disabled"};

//  Absence convention of <cuexis/judgement/diagnostic.hpp>: an empty token means this context
//  component does not apply to this diagnostic. It is a documented absence marker, not a default
//  value for an unfrozen role.
inline constexpr std::string_view kAbsent{};

inline constexpr std::string_view kTimebaseSection{"judgement.timebase"};
inline constexpr std::string_view kInputSection{"judgement.input"};
inline constexpr std::string_view kProfileIdPath{"profileId"};
inline constexpr std::string_view kTickScalePath{"tickScale"};
inline constexpr std::string_view kInitialTempoPath{"initialTempo"};
inline constexpr std::string_view kOriginBeatPath{"originBeat"};
inline constexpr std::string_view kTempoSectionsPath{"tempoSections"};
inline constexpr std::string_view kStopSectionsPath{"stopSections"};
inline constexpr std::string_view kUnitPath{"unitToken"};
inline constexpr std::string_view kTickPath{"tick"};
inline constexpr std::string_view kBeatPath{"beat"};
inline constexpr std::string_view kRationalPath{"rational"};
inline constexpr std::string_view kCommitWindowPath{"commitWindow"};
inline constexpr std::string_view kIntervalPath{"interval"};
inline constexpr std::string_view kLatePolicyPath{"latePolicy"};
inline constexpr std::string_view kFinalizationWatermarkPath{"latePolicy.finalizationWatermark"};
inline constexpr std::string_view kMaxQueueHopPath{"latePolicy.maxQueueHop"};
inline constexpr std::string_view kWindowClosePath{"latePolicy.windowCloseThreshold"};
inline constexpr std::string_view kWindowOpenPath{"latePolicy.windowOpenThreshold"};
inline constexpr std::string_view kCanonicalQueueKeyPath{"lateQueue.canonicalKey"};
inline constexpr std::string_view kCalibratedClockPath{"observationTick.calibratedClock"};
inline constexpr std::string_view kIngressSequencePath{"ingressSequence"};
inline constexpr std::string_view kContinuityPath{"continuityCapability"};
inline constexpr std::string_view kAmountPath{"amountSpec"};
inline constexpr std::string_view kMappingProfileIdPath{"inputMapping.profileId"};
inline constexpr std::string_view kMappingProfileVersionPath{"inputMapping.profileVersion"};
inline constexpr std::string_view kMappingSourceClassPath{"inputMapping.sourceClass"};
inline constexpr std::string_view kMappingDomainsPath{"inputMapping.domains"};
inline constexpr std::string_view kDomainTokenPath{"domainToken"};
inline constexpr std::string_view kAmountRangePath{"amountSpec.range"};
inline constexpr std::string_view kAmountScalePath{"amountSpec.scale"};
inline constexpr std::string_view kDiscontinuityPath{"discontinuity"};
inline constexpr std::string_view kSameTickCollisionPath{"observationTick.sameTickCollision"};
inline constexpr std::string_view kRawTimePath{"rawTimestamps"};

//  ---------------------------------------------------------------------------------------------
//  S7A-3 offline assembler tokens
//  ---------------------------------------------------------------------------------------------
//
//  Same rule as the S7A-2 block above: the machine-readable code table is still unfrozen (round 6,
//  CM-D04 / P2-04 / P2-06), so every token below is either
//
//    * a candidate reject code that the Gameplay V2 Spec section 7.2 table already names
//      (`format.gameplay_version_unsupported`, `resource.capacity_unsupported`,
//      `coordination.relation_unsupported`, `resource.handoff_unsupported`,
//      `input.continuous_unsupported`, `pattern.relation_unsupported`,
//      `geometry.inference_rejected`, `capability.permanently_unsupported`, `capability.unknown`,
//      `migration.ambiguous`), or
//    * an S7A-3 local refinement token, marked `s7a3`, for a rejection this batch has to
//    distinguish
//      but the Spec has not named yet.
//
//  A local token is not a registered code and must not be treated as one. No token here is written
//  into a public header.

//  Spec 9.2 category list; the identity and closure incompleteness of a declared set that is
//  smaller than the derived closure. Spec 6.2 rule 1 makes that a stable failure of the assembly.
inline constexpr std::string_view kIdentityClosureIncompleteCategory{"identity_closure_incomplete"};
//  Spec 9.2 category list; an unknown capability id (Spec 7.2 R-16). The recognised-but-disabled
//  spelling is `kCapabilityDisabledCategory` above, because the S7A-2 input boundary and the S7A-3
//  assembler share it and a second definition would be two spellings of one category.
inline constexpr std::string_view kUnknownCapabilityCategory{"unknown_capability"};
//  Spec 9.2 category list; a migration that cannot be decided (Spec 7.2 R-18 / R-19), which is
//  where an older gameplay revision lands when it reaches this entry un-migrated.
inline constexpr std::string_view kAmbiguousMigrationCategory{"ambiguous_migration"};
//  Spec 9.2 category list; an unexpanded or unexpanded-able loop representation (Spec 3.8.4).
inline constexpr std::string_view kNonTerminatingSourceCategory{"non_terminating_source"};

//  --- Spec 7.2 candidate codes (no new R number) -------------------------------------------------

//  R-17: an older `gameplay.version` that was not explicitly migrated offline.
inline constexpr std::string_view kGameplayVersionUnsupportedCode{
    "format.gameplay_version_unsupported"};
//  R-01: `capacity > 1`, owner sets and parallel slots.
inline constexpr std::string_view kResourceCapacityUnsupportedCode{"resource.capacity_unsupported"};
//  R-02 / R-03: a binding, temporal or quota relation.
inline constexpr std::string_view kCoordinationRelationUnsupportedCode{
    "coordination.relation_unsupported"};
//  R-04: handoff, the gap / handoff_pending states and a non-zero resource-state grace.
inline constexpr std::string_view kResourceHandoffUnsupportedCode{"resource.handoff_unsupported"};
//  R-11: `sameContact` and a cross-requirement relation hidden inside a pattern.
inline constexpr std::string_view kPatternRelationUnsupportedCode{"pattern.relation_unsupported"};
//  R-12: inferring a judgement domain from a render position, a material, a camera or an animation.
inline constexpr std::string_view kGeometryInferenceRejectedCode{"geometry.inference_rejected"};
//  R-14: run-time script, per-frame callback, dynamic requirement generation.
inline constexpr std::string_view kCapabilityPermanentlyUnsupportedCode{
    "capability.permanently_unsupported"};
//  R-16: an unknown capability id.
inline constexpr std::string_view kCapabilityUnknownCode{"capability.unknown"};
//  A capability the compiling toolchain recognises but did not enable for this compile.
inline constexpr std::string_view kCapabilityDisabledCode{
    "judgement.s7a3.closure.capability_disabled"};
//  R-18 / R-19: non-lowered `effects` and an undecidable migration.
inline constexpr std::string_view kMigrationAmbiguousCode{"migration.ambiguous"};

//  --- S7A-3 local tokens ------------------------------------------------------------------------

//  The outer chart version gate: the only accepted value is 5 (Spec 2.1).
inline constexpr std::string_view kChartVersionUnsupportedCode{
    "judgement.s7a3.entry.chart_version_unsupported"};
//  ABI domain 8: an `author-source` entry must be explicitly non-playback, and every playback flag
//  has to be carried with an explicit entry kind.
inline constexpr std::string_view kPlaybackEntryMismatchCode{
    "judgement.s7a3.entry.playback_entry_mismatch"};
//  An assembly with no typed source at all.
inline constexpr std::string_view kSourceSetEmptyCode{"judgement.s7a3.entry.source_set_empty"};
//  Two source documents sharing one stable identity: their stable declaration ids would collide.
inline constexpr std::string_view kDuplicateSourceDocumentCode{
    "judgement.s7a3.entry.source_document_duplicate"};
//  The graph's typed timebase binding is missing.
inline constexpr std::string_view kTimebaseMissingCode{"judgement.s7a3.entry.timebase_missing"};
//  Two declarations in the merged namespace share one name. P1-02 rejects instead of merging.
inline constexpr std::string_view kDeclarationNameDuplicateCode{
    "judgement.s7a3.merge.declaration_name_duplicate"};
//  Two declarations share one stable id (source document identity + declaration ordinal).
inline constexpr std::string_view kStableIdDuplicateCode{
    "judgement.s7a3.merge.stable_id_duplicate"};
//  A declaration ordinal that was never assigned. Ordinals are declarations of the source document
//  and start at 1, so 0 is the unassigned state of the field rather than an index.
inline constexpr std::string_view kDeclarationOrdinalZeroCode{
    "judgement.s7a3.merge.declaration_ordinal_unassigned"};
//  A cross-document reference that names no document. P1-02 requires it to be explicit.
inline constexpr std::string_view kCrossDocumentReferenceImplicitCode{
    "judgement.s7a3.merge.cross_document_reference_implicit"};
//  A reference that names no declaration of the merged namespace.
inline constexpr std::string_view kReferenceDanglingCode{"judgement.s7a3.merge.reference_dangling"};
//  A declaration that is missing a component its own contract requires.
inline constexpr std::string_view kDeclarationIncompleteCode{
    "judgement.s7a3.declaration.structurally_incomplete"};
//  A declaration table that lists the same key twice.
inline constexpr std::string_view kDeclarationDuplicateCode{
    "judgement.s7a3.declaration.duplicate_entry"};
//  A judgement domain that declares no axis range at all.
inline constexpr std::string_view kDomainRangeUndeclaredCode{
    "judgement.s7a3.domain.range_undeclared"};
//  A judgement axis whose declared maximum precedes its minimum.
inline constexpr std::string_view kDomainRangeReversedCode{"judgement.s7a3.domain.range_reversed"};
//  A geometry computation whose exact result leaves the signed 64-bit domain.
inline constexpr std::string_view kDomainGeometryOverflowCode{
    "judgement.s7a3.domain.geometry_overflow"};
//  A canonical value that cannot be represented inside its declared axis range.
inline constexpr std::string_view kDomainNarrowingRejectedCode{
    "judgement.s7a3.domain.narrowing_rejected"};
//  A judgement domain asking for a dynamically resolved frame (P1-01, 7B+ candidate).
inline constexpr std::string_view kDynamicFrameUnsupportedCode{
    "judgement.s7a3.domain.dynamic_frame_unsupported"};
//  A content form Stage 7A rejects. The kind travels in the field path so that one token can cover
//  the whole Spec 3.8.5 exclusion list, the way Spec 9.3 item 4 distinguishes the two continuity
//  cases by `field.path` alone.
inline constexpr std::string_view kUnsupportedContentCode{
    "judgement.s7a3.content.unsupported_form"};
//  Content whose semantics require Release / tail while the requirement declares no tail phase
//  (Spec 3.8.3 rule 2; the batch never infers a tail from an implicit legacy profile).
inline constexpr std::string_view kImplicitReleaseTailCode{
    "judgement.s7a3.requirement.release_tail_implicit"};
//  A requirement with no declared solver profile (Spec 9.3 atomic-failure condition).
inline constexpr std::string_view kSolverProfileMissingCode{
    "judgement.s7a3.requirement.solver_profile_missing"};
//  A resource reference that does not resolve to a resource record.
inline constexpr std::string_view kResourceReferenceDanglingCode{
    "judgement.s7a3.resource.reference_dangling"};
//  Two prepared declarations carrying the same canonical identity.
inline constexpr std::string_view kIdentityCollisionCode{"judgement.s7a3.identity.collision"};
//  The declared capability or feature set is smaller than the derived closure. Spec 6.2 rule 1: the
//  writer never fills the gap in.
inline constexpr std::string_view kDeclaredClosureIncompleteCode{
    "judgement.s7a3.closure.declared_incomplete"};
//  A content-profile count above a measured upper bound (BUDGET 3.2).
inline constexpr std::string_view kContentProfileExceededCode{
    "judgement.s7a3.budget.content_profile_exceeded"};
//  A second-half entry point reached before the part that owns it is implemented. The token is a
//  placeholder, not a frozen design: it must disappear from this table together with both of its
//  call sites once the third part of S7A-3 implements `resolvePreparedGrace` and
//  `resolveResourceClaims` for real. Its section, path and remediation tokens go with it.
inline constexpr std::string_view kSecondHalfPendingCode{
    "judgement.s7a3.second_half.not_implemented"};

//  --- S7A-3 second half (first part): the compiled Pattern and the compiled Measure
//  -----------------
//
//  The compiled products of plan S7A-3 items 2 and 3 report through the same nine Spec 9.2
//  categories as the rest of the batch. A containment or derivation fault is `invalid_relation`,
//  and a genuinely unexpanded loop stays `non_terminating_source` and is never used for anything
//  else.
//
//  The budget criterion in one place, because it is the same for every dimension and it is NOT
//  "did the count overflow":
//
//    * a count that was MEASURED above an accepted bound is `budget_exceeded`;
//    * a count that is a PROVEN LOWER BOUND of its dimension and is above an accepted bound is
//      `budget_exceeded` as well. Exact checked arithmetic over non-negative counts, and the
//      monotone accumulations of the compile, are such proofs: an absent value means the true value
//      is at least 2^64, which is above every representable bound, and a declared finite longest
//      acceptable trace length `L` proves the Spec 8.2 state count is at least `L + 1`;
//    * a dimension with NO accepted bound compares nothing and refuses nothing;
//    * a count that is NOT a proven lower bound (a count multiplied away by zero copies) and a
//      construction that does not complete are MEASUREMENT GAPS of the module and never rejections
//      by themselves. An internal construction bound is a limit of this module's measurement and
//      can never act as a content threshold.
//
//  History of this criterion (both corrections are in force at once): the previous round's
//  `judgement.s7a3.pattern.expansion_not_representable` and
//  `judgement.s7a3.pattern.state_count_not_measured` codes were deleted because the S7A-3
//  implementation review rejected mapping every absent or overflowing count onto a refusal (thread
//  `01a0fe91-416c-7f31-a48a-d1890a0e68d1`, verdict `reject`); the same thread's round-3 review
//  (verdict `reject`, confidence 0.93) then rejected the opposite extreme, which had stopped
//  comparing a PROVEN lower bound against an accepted bound at all. No code was added or deleted
//  for the round-3 correction: it reuses `budget_exceeded` and the dimension's existing path.

//  A compiled-pattern count, or a proven lower bound of one, above a budget dimension that a
//  measurement accepted. One code and one category cover the measured overrun and the proven one.
inline constexpr std::string_view kPatternBudgetExceededCode{
    "judgement.s7a3.pattern.budget_exceeded"};
//  An atom the pattern matches that the requirement does not declare among its arms.
inline constexpr std::string_view kPatternAtomOutsideDeclaredArmsCode{
    "judgement.s7a3.pattern.atom_outside_declared_arms"};
//  A pattern that cannot be contained in the declared arm / deadline element capacity.
inline constexpr std::string_view kPatternArmBoundExceededCode{
    "judgement.s7a3.pattern.arm_bound_exceeded"};
//  A measure component whose declared category is not the category its phase derives (Spec 3.20
//  rule 3: the category is derived and is never assigned by a caller).
inline constexpr std::string_view kMeasureCategoryNotDerivedCode{
    "judgement.s7a3.measure.category_not_derived_from_phase"};
//  --- S7A-3 second half (third part): preparedGrace and resource claim resolution ---------------
//
//  Plan S7A-3 item 4 resolves the final `preparedGrace` of one requirement and the claim /
//  ownership of the single `capacity = 1` exclusive resource in the prepare phase. The declarations
//  that decide the value are reported through the same three-way split the rest of the batch uses
//  (Spec 9.3): a structurally incomplete or self-contradicting declaration is `invalid_relation`, a
//  declared value that cannot be honoured inside its own declared domain (a non-positive
//  quantisation step, a reversed range, an unquantisable or out-of-range value) is
//  `budget_exceeded`, and a form that Stage 7A does not open is the capability rejection Spec 7.2
//  already names
//  (`resource.handoff_unsupported` for the sticky / observing override, the `continuityGrace`
//  request and the resource-state grace; `resource.capacity_unsupported` for a capacity other than
//  1). No tenth category is introduced and no R entry is added.

//  An inherited grace declaration that names no declaration to inherit from.
inline constexpr std::string_view kGraceInheritanceUndeclaredCode{
    "judgement.s7a3.grace.inheritance_undeclared"};
//  A grace declaration that is not inherited while still naming a declaration to inherit from.
inline constexpr std::string_view kGraceInheritanceUnexpectedCode{
    "judgement.s7a3.grace.inheritance_unexpected"};
//  The value a declared policy resolves from was never supplied: an explicit declaration without a
//  declared value, an inherited declaration whose source declared none, or a default declaration
//  while the default frozen before prepare holds no value. The resolution never substitutes zero.
inline constexpr std::string_view kGraceValueMissingCode{"judgement.s7a3.grace.value_missing"};
//  A chart-supplied grace value while the declaration does not allow the chart to supply one
//  (CM-R08: `allowChartGrace` decides exactly this case).
inline constexpr std::string_view kGraceChartSupplyNotAllowedCode{
    "judgement.s7a3.grace.chart_supply_not_allowed"};
//  A quantisation step that is not a positive number of ticks, so no value can be quantised against
//  it. Spec 9.3 second class: the declaration is complete but the value cannot be honoured.
inline constexpr std::string_view kGraceUnitNotPositiveCode{
    "judgement.s7a3.grace.unit_not_positive"};
//  A declared canonical range whose maximum precedes its minimum: no value can satisfy it.
inline constexpr std::string_view kGraceRangeReversedCode{"judgement.s7a3.grace.range_reversed"};
//  A grace value that is not an exact whole number of the declared quantisation units (Spec 9.3
//  "grace quantisation inconsistency"). The resolution never rounds, truncates or clamps it.
inline constexpr std::string_view kGraceQuantizationMismatchCode{
    "judgement.s7a3.grace.quantization_mismatch"};
//  A quantised grace value whose canonical unit count has no signed 64-bit representation.
inline constexpr std::string_view kGraceNotRepresentableCode{
    "judgement.s7a3.grace.not_representable"};
//  A quantised grace value outside the declared canonical range.
inline constexpr std::string_view kGraceOutOfRangeCode{"judgement.s7a3.grace.out_of_range"};
//  Two occupying claims of the same resource carry the same stable claim key, so the tie cannot be
//  decided deterministically from the prepared graph. Stage 7A refuses it at prepare instead of
//  resolving it by input order, thread order or a global search.
inline constexpr std::string_view kResourceClaimConflictCode{
    "judgement.s7a3.resource.claim_conflict"};
//  An illegal resource state transition: a declared entry state that contradicts its own lease
//  (held without an owner, free or terminal with one), or an occupying claim against a resource
//  whose slot became permanently terminal. Spec 9.3 lists the illegal resource state transition as
//  a prepare atomic failure condition and the batch maps it onto invalid_relation.
inline constexpr std::string_view kResourceStateTransitionInvalidCode{
    "judgement.s7a3.resource.state_transition_invalid"};

//  --- capability ids and remediation paths ------------------------------------------------------
//
//  Spec 7.2 rule 1 requires an R-01 to R-11 rejection to name a capability id and a replacement
//  path. The Spec names exactly two of them (`resource.handoff.v1` in the R-04 row and
//  `input.trajectory.v1` in the R-05 row); the rest are S7A-3 local tokens built on the same
//  `<subject>.<aspect>.v1` spelling, and their final spelling belongs to the same code-table ruling
//  that owns every other token here.

inline constexpr std::string_view kResourceCapacityCapabilityId{"resource.capacity.v1"};
inline constexpr std::string_view kCoordinationRelationCapabilityId{"coordination.relation.v1"};
inline constexpr std::string_view kResourceHandoffCapabilityId{"resource.handoff.v1"};
inline constexpr std::string_view kPatternRelationCapabilityId{"pattern.relation.v1"};
inline constexpr std::string_view kDynamicFrameCapabilityId{"geometry.dynamic_frame.v1"};
//  The replacement path of the 7B+ candidates of the Stage 7A plan. It is a batch label and never a
//  version number.
inline constexpr std::string_view kLaterBatchRemediation{"7B+"};
//  The replacement path of a second-half S7A-3 entry point. Also a batch label.
inline constexpr std::string_view kSecondHalfRemediation{"S7A-3-second-half"};

//  --- sections and field paths ------------------------------------------------------------------

inline constexpr std::string_view kEntrySection{"judgement.gameplay.entry"};
inline constexpr std::string_view kMergeSection{"judgement.gameplay.merge"};
inline constexpr std::string_view kGraphSection{"judgement.gameplay.graph"};
inline constexpr std::string_view kDomainSection{"judgement.gameplay.domain"};
inline constexpr std::string_view kClosureSection{"judgement.gameplay.closure"};
inline constexpr std::string_view kBudgetSection{"judgement.gameplay.budget"};
inline constexpr std::string_view kSecondHalfSection{"judgement.gameplay.secondHalf"};

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
inline constexpr std::string_view kSecondHalfPath{"secondHalf"};
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
