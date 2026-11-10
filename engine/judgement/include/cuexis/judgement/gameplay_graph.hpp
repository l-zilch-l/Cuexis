#pragma once

//  Judgement typed kernel - S7A-3 (first half) Canonical Gameplay Graph model.
//
//  Authority: plan S7A-3 items 1-6 (docs/stage_plans/active/stage-07/plan.md lines 294-327),
//  Gameplay V2 Spec sections 3.2-3.6, 3.8, 3.8.6, 5.2, 5.5, 6.1-6.5, 7.2 and 9.3, Gameplay V2 ABI
//  domains 3, 4, 7 and 8, and the admission rulings this batch was authorised with: P1-01 (typed
//  judgement domain), P1-02 (relation merge), P1-14 (REF0 / manifest / diagnostic attribution),
//  P1-15 (equivalence is a field-by-field diff), P2-05 (action / requiredAction / domain are
//  independent), P2-07 (source closure / diagnostic map / content-artifact identity naming) and
//  S7A6-R10 (the three-way resource split).
//
//  Frozen by this batch (first half of S7A-3):
//
//    * the composition of the Canonical Gameplay Graph (Spec 3.2) as typed declarations. This is
//    the
//      only runtime semantic source of the gameplay domain, and the graph is a value: the assembler
//      owns what it publishes;
//    * Requirement, Pattern and Measure *declarations*, deliberately not compiled predicates (plan
//      S7A-3 item 1). No automaton state, no transition, no determinisation result and no compiled
//      predicate is representable here, because compiling them is the second half of this batch;
//    * the three-way resource split `resourceRef` / `claimPolicy` / resource record as three
//      separate types and three separate fields, so a claim can never be read as a definition and a
//      policy can never be read as an identity (S7A6-R10);
//    * `action`, `requiredAction` and `domain` as three independent declarations with no derivation
//      between them and no conversion between their types (P2-05);
//    * the static typed judgement domain record: the coordinate system and the per-axis range are
//      declared *inside* the domain, a presentation transform is not merely absent but unreachable
//      (this module admits cuexis/core and cuexis/judgement headers only), and a dynamically
//      resolved frame is a stable rejection with a 7B+ remediation (P1-01);
//    * the reference attribution table of Spec 3.8.6 as a fixed function from a reference kind to
//      exactly one of REF0 / manifest / diagnostic map, plus the rejection of a reference that asks
//      to be registered in a second class (P1-14);
//    * the P2-07 naming split: `SourceClosure`, `DiagnosticMap` and the content-artifact identity
//      carriers are three separately named, separately owned things. No ABI type row is added;
//    * the field-by-field semantic diff and the equivalence predicate built on it (P1-15). The
//      diagnostic map and every physical order are excluded from the comparison by construction;
//    * the checked geometry surface of the judgement domain: squaring, multiplication, addition and
//      narrowing either carry an exact range proof or are a stable rejection (ABI "units and
//      ranges"). No limit value is baked in anywhere.
//
//  Deliberately not frozen here, and therefore not representable:
//
//    * every numeric limit, threshold, default and research-slice measurement. The budget profiles,
//      their accepted upper bounds and the pattern state/expansion budgets belong to S7A-9;
//    * the compiled Pattern predicate and the compiled Measure grading. They are implemented in the
//      second half of S7A-3 and live in <cuexis/judgement/gameplay_assembler.hpp>, so no automaton
//      state, transition or compiled byte of them is representable in the declaration model;
//    * the resource claim / ownership / `preparedGrace` resolution (second half of S7A-3). This
//      header fixes the `PreparedGrace` value type and the `GraceDeclaration` input point; the
//      value in a graph produced by this batch is the value the source declared, and this batch
//      makes no claim that any particular value is legal in Stage 7A. The relation between Spec
//      3.10 ("grace = 0 is the only legal 7A value" for the resource state machine) and the
//      prepare-time `preparedGrace` of plan S7A-3 item 4 is a second-half question and is
//      deliberately not decided here;
//    * the phase priority ordering (CM-F06), the capability registry field set (CM-X01), the solver
//      budget numbers, the REF0 physical fields and every serialization encoding. The canonical
//      identity bytes of Spec 5.5 are one deterministic algorithm in this batch and are explicitly
//      not presented as a frozen wire or artifact contract.
//
//  Every validation function reports a `core::Result` failure with a stable diagnostic. No function
//  saturates, truncates, clamps or falls back to a default, and no exception crosses this boundary.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <cuexis/judgement/timebase.hpp>

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gameplay_graph_record.hpp"

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  The Canonical Gameplay Graph (Spec 3.2)
//  ---------------------------------------------------------------------------------------------

//  The single runtime semantic source of the gameplay domain (Spec 3.1). A legal physical entry is
//  normalized into exactly this value, so the Chart v5 JSON section, an emission and a typed
//  `gameplay-graph` entry can only differ in the diagnostics they carry.
//
//  The graph is a value, except for the two typed declarations it references: the TimebaseProfile
//  and the late-policy parameters of S7A-2 are borrowed, exactly as Spec 3.2's `timebaseRef` says.
//  The caller keeps those declarations alive for as long as it uses the graph; everything else in
//  the graph is owned.
//
//  Tables whose declaration order carries no semantics are stored in canonical order, so the value
//  is a function of its content and not of the order a source happened to use. `diagnosticMap` is
//  the one member that is deliberately outside every identity and outside the semantic diff.
struct CanonicalGameplayGraph final {
    //  `gameplay.version`. Stage 7A admits exactly 2; the assembler is the gate that enforces it.
    std::uint32_t gameplayVersion;
    //  The semantic graph revision (Spec 5.2 `graphRevision`).
    std::uint64_t graphRevision;
    std::string executionProfile;
    std::string normalizationProfileToken;
    std::string coordinatorPolicyToken;
    //  The typed timebase binding, borrowed (Spec 3.2 `timebaseRef`).
    const TimebaseProfile* timebase;
    //  The late-policy parameters the timebase binding was validated against (Spec 5.2 timebaseRef
    //  row: the prepared value, including the typed late policy, enters the judgement identity).
    const LatePolicyParameters* latePolicy;
    //  The declared ruleset binding and Interface projection token (Spec 5.2 `rulesetRef`).
    std::string rulesetRef;
    std::vector<RequirementRecord> requirements;
    std::vector<ResourceRecord> resources;
    std::vector<RelationDeclaration> relations;
    std::vector<SolverProfileDeclaration> solverProfiles;
    std::vector<FactBindingRef> factBindings;
    std::vector<JudgementDomainRecord> judgementDomains;
    //  The source-side minimum declaration (Spec 6.1 first arrow).
    DeclaredCapabilitySet declaredCapabilities;
    FeatureClosure declaredFeatures;
    //  The ruleset and presentation capability needs (Spec 6.1 middle arrows).
    ClosureContributions closureContributions;
    //  Spec 3.2 `capabilities[]`: the sorted derived closure the assembler produced.
    DerivedCapabilityClosure derivedCapabilities;
    FeatureClosure derivedFeatures;
    ResourceClosure resourceClosure;
    MergedNamespace mergedNamespace;
    SourceClosure sourceClosure;
    DiagnosticMap diagnosticMap;
};

//  ---------------------------------------------------------------------------------------------
//  Canonical table order
//  ---------------------------------------------------------------------------------------------
//
//  The strict weak orders that normalize the graph tables. They are derived from stable identities
//  only, never from an array position, an ingress ordinal or a completion order, which is what
//  makes the whole graph invariant under a permutation of the input.
//
//  They are deliberately not the S7A-4 fact sort key: the unique canonical fact order
//  `(commitTick, originKindPriority, canonicalOrdinal)` of Spec 3.9 is owned by S7A-4 and is not
//  represented in this header.

[[nodiscard]] auto canonicalCompare(const RequirementRecord& left,
                                    const RequirementRecord& right) noexcept
    -> std::strong_ordering;
[[nodiscard]] auto canonicalCompare(const ResourceRecord& left,
                                    const ResourceRecord& right) noexcept -> std::strong_ordering;
[[nodiscard]] auto canonicalCompare(const RelationDeclaration& left,
                                    const RelationDeclaration& right) noexcept
    -> std::strong_ordering;
[[nodiscard]] auto canonicalCompare(const SolverProfileDeclaration& left,
                                    const SolverProfileDeclaration& right) noexcept
    -> std::strong_ordering;
[[nodiscard]] auto canonicalCompare(const JudgementDomainRecord& left,
                                    const JudgementDomainRecord& right) noexcept
    -> std::strong_ordering;
[[nodiscard]] auto canonicalCompare(const MergedDeclaration& left,
                                    const MergedDeclaration& right) noexcept
    -> std::strong_ordering;

//  ---------------------------------------------------------------------------------------------
//  Closure derivation (Spec 6.1)
//  ---------------------------------------------------------------------------------------------

//  Derives the complete capability closure from the canonical graph and the ruleset / presentation
//  contribution. The result is sorted and deduplicated, exactly as the Packed header stores it.
//
//  Derivation never adds a token the content did not state: a capability no declaration named
//  cannot appear here, because there is no registry in this batch that could name it.
[[nodiscard]] auto deriveCapabilityClosure(const CanonicalGameplayGraph& graph)
    -> DerivedCapabilityClosure;

[[nodiscard]] auto deriveFeatureClosure(const CanonicalGameplayGraph& graph) -> FeatureClosure;

//  Every resource the graph references, sorted. A reference that does not resolve to a resource
//  record is not silently dropped: it is reported by the assembly validation, and the closure only
//  carries references that resolve.
[[nodiscard]] auto deriveResourceClosure(const CanonicalGameplayGraph& graph) -> ResourceClosure;

//  ---------------------------------------------------------------------------------------------
//  Field-by-field semantic diff (Spec 3.5, P1-15)
//  ---------------------------------------------------------------------------------------------

//  One field-level difference between two prepared graphs. The path is the canonical field path of
//  the differing field, and the two texts are the rendered declared values.
//
//  This vector *is* the golden evidence of Spec 3.5 / P1-15: two graphs are equivalent exactly when
//  it is empty, and when it is not empty it names every field that differs instead of reporting an
//  opaque hash inequality.
struct GraphFieldDifference final {
    std::string path;
    std::string left;
    std::string right;
};

//  Compares two prepared graphs field by field, in canonical table order.
//
//  Excluded by construction, never by a caller's discipline:
//
//    * the diagnostic map as a whole, including the carrier form and the carrier provenance;
//    * the source closure, because Spec 2.3 keeps toolchain and carrier identity out of the
//    semantic
//      judgement;
//    * every physical order: the tables are compared in canonical order, so permuting a source
//    array
//      is not a difference.
//
//  Included: the whole canonical graph content and both derived closures, because Spec 3.5 defines
//  equivalence as "the canonical graph and the derived capability closure are field-by-field
//  equal".
[[nodiscard]] auto semanticDiff(const CanonicalGameplayGraph& left,
                                const CanonicalGameplayGraph& right)
    -> std::vector<GraphFieldDifference>;

//  True exactly when `semanticDiff` is empty.
[[nodiscard]] auto equivalent(const CanonicalGameplayGraph& left,
                              const CanonicalGameplayGraph& right) -> bool;

} // namespace cuexis::judgement
