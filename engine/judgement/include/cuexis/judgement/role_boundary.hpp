#pragma once

//  Judgement typed kernel - S7A-1 judgement-side role boundary.
//
//  Same rule as <cuexis/judgement/input_boundary.hpp>: every declaration below is an incomplete
//  type. S7A-1 freezes existence, role and the 7A rejection path only, and promises no member, no
//  integer width, no enumeration value, no default value, no layout and no serialization byte.
//
//  Blocked representations behind these roles:
//
//    MeasureSpec field set                  unresolved item #3, fields not frozen
//    Fact category / grade / outcome sets   round 5, CM-S10 / P1-07
//    capability registry entry              unresolved, CM-X01
//    diagnostic code, category, severity    round 6, CM-D04 / P2-04 / P2-06
//
//  Role inventory. Candidate names from the Gameplay V2 ABI; final naming belongs to unresolved
//  item #3 and is not frozen here.
//
//    RequirementRecord       prepared record of one local program instance
//    PatternRef              reference to a compiled pattern
//    MeasureSpec             multi-component quantization spec and per-component grading
//    PreparedGrace           final grace value, read-only after prepare
//    GraceResolutionPolicy   explicit / inherited / default resolution policy
//    ResourceClaimIntent     requirement-side observe / consume / claim declaration
//    FactRecord              append-only ledger entry
//    JudgementResult         consumer-facing projection of one judgement
//    JudgementIdentity       engine / ruleset / chart / session four-component identity
//    DiagnosticCode          stable string code
//    DiagnosticCategory      category
//    DiagnosticSeverity      severity
//    JudgementError          recoverable error payload
//    CapabilityId            globally unique stable capability id
//    CapabilityRecord        capability registry entry
//    CapabilityState         four-state capability query result
//
//  Roles the ABI fixes as rejections rather than representations are deliberately absent from this
//  list, so that no later reader mistakes an absent type for an unfinished one:
//
//    * BindingRelation, TemporalRelation and QuotaRelation are 7A stable rejections; capacity > 1,
//      owner sets, parallel slots, gap and handoff_pending are rejection paths as well.
//    * GripPolicy is the Gameplay I name replaced by PreparedGrace + GraceResolutionPolicy +
//      ResourceClaimIntent; it is not reintroduced under any alias.
//    * correction, Replay injection and per-frame script hooks are closed in 7A; a value that
//      carries them is a stable rejection, not a type in this batch.
//
//  An unsupported capability must never be mapped onto an older kind. The roles above name no
//  kind, and the diagnostic carriers in <cuexis/judgement/diagnostic.hpp> carry an opaque
//  capability token, so no rejection can silently rewrite a capability into a legacy taxonomy.

namespace cuexis::judgement::roles {

//  Declared and never defined: no member, no width, no enumeration value, no default value, no
//  layout. Each declaration fixes that the role exists and is distinct; nothing else.
class RequirementRecord;
class PatternRef;
class MeasureSpec;
class PreparedGrace;
class GraceResolutionPolicy;
class ResourceClaimIntent;
class FactRecord;
class JudgementResult;
class JudgementIdentity;
class DiagnosticCode;
class DiagnosticCategory;
class DiagnosticSeverity;
class JudgementError;
class CapabilityId;
class CapabilityRecord;
class CapabilityState;

} // namespace cuexis::judgement::roles
