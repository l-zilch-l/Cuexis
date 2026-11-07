#pragma once

//  Canonical gameplay graph - judgement domains, requirements, relations and closures.

#include "gameplay_graph_phase.hpp"

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

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Judgement domain (P1-01)
//  ---------------------------------------------------------------------------------------------

//  The declared range of one judgement-domain axis. Both bounds are declared by the content; this
//  batch freezes no range number and no axis name.
struct JudgementAxisRange final {
    //  The axis is identified by a declared token rather than by an enumerator: the coordinate
    //  system is declared inside the judgement domain (P1-01), so the domain names its own axes.
    std::string axisToken;
    std::int64_t minimum;
    std::int64_t maximum;

    friend auto operator==(const JudgementAxisRange&, const JudgementAxisRange&) noexcept
        -> bool = default;
    friend auto operator<=>(const JudgementAxisRange&, const JudgementAxisRange&) noexcept
        -> std::strong_ordering = default;
};

//  How a judgement domain resolves its frame of reference (P1-01).
enum class FrameResolution : std::uint8_t {
    //  The only form Stage 7A admits: a frame declared statically inside the judgement domain
    //  record.
    //  It is never inherited from and never depends on a presentation transform.
    staticDeclaration,
    //  A frame that would be resolved at run time, from the presentation or from a runtime entity.
    //  P1-01 registers dynamic frames as a 7B+ candidate and requires a stable rejection, so the
    //  request is representable and refused.
    dynamicRuntimeFrame,
};

//  The static typed judgement domain record (P1-01).
//
//  Isolation is structural rather than promised. This record has no presentation member, no
//  transform, no viewport and no runtime entity handle, and the module admits cuexis/core and
//  cuexis/judgement headers only, so no presentation transform is reachable from this translation
//  unit in the first place: a geometry that depended on a presentation transform would make
//  judgement depend on the viewport, which destroys reproducibility. The judgement-geometry tests
//  assert the member types of this record so that adding a presentation member fails the suite.
//
//  The coordinate system and the per-axis ranges are declared here, inside the domain. Geometry
//  belongs to the Gameplay closure (P1-01): it is judgement content, not presentation content.
struct JudgementDomainRecord final {
    std::string domainId;
    //  The token of the coordinate system declared inside this domain, with the range of each axis
    //  declared alongside it.
    std::string coordinateSystemToken;
    //  One declaration per axis, normalized as a set keyed by the axis token. An empty table is a
    //  structurally incomplete declaration and is rejected: "the domain declares its range" is the
    //  frozen requirement, and an empty table would be an implicit unbounded range.
    std::vector<JudgementAxisRange> axes;
    FrameResolution frameResolution;
    //  Required when `frameResolution` is `dynamicRuntimeFrame`: the declared provider token
    //  travels so that the stable rejection can name what it refused. Absent otherwise.
    std::string dynamicFrameProviderToken;
    RequiredRefs required;

    friend auto operator==(const JudgementDomainRecord&, const JudgementDomainRecord&) noexcept
        -> bool = default;
};

//  Validates one judgement domain record as a declaration.
//
//  Checks, in the order the diagnostics are reported:
//
//    * the domain identity and the coordinate-system token are declared, and the axis table is not
//      empty: a missing component is a structurally incomplete declaration -> invalid_relation
//      (Spec 9.3, first class);
//    * every axis token is declared once and the range is not reversed: a declared range whose
//      maximum precedes its minimum is a declaration that cannot be honoured -> budget_exceeded
//      (Spec 9.3, second class). The two classes are kept apart exactly as the Spec requires;
//    * the frame is statically declared. A dynamic frame is a stable capability rejection that
//    names
//      the registered 7B+ candidate path.
[[nodiscard]] auto validateJudgementDomain(const JudgementDomainRecord& domain)
    -> core::Result<void>;

//  ---------------------------------------------------------------------------------------------
//  Checked judgement geometry (ABI "units and ranges")
//  ---------------------------------------------------------------------------------------------
//
//  Every judgement inequality must carry a non-overflowing proof, and "using integers" is not by
//  itself a determinism proof. The four operations below are the whole geometry surface of this
//  batch. Each one is exact: it either returns the exact result or reports a stable rejection with
//  the budget_exceeded category, which is the Spec 9.3 "a declared value left the domain its
//  declaration requires" case. Nothing saturates, truncates or clamps, and no limit, threshold or
//  research-slice measurement is baked in: the only bound is the signed 64-bit domain itself.

[[nodiscard]] auto geometryAdd(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t>;

[[nodiscard]] auto geometryMultiply(std::int64_t left, std::int64_t right)
    -> core::Result<std::int64_t>;

[[nodiscard]] auto geometrySquare(std::int64_t value) -> core::Result<std::int64_t>;

//  The exact extent `maximum - minimum` of one declared axis, or a stable rejection when the axis
//  is not a usable declaration or the extent does not fit.
[[nodiscard]] auto axisExtent(const JudgementAxisRange& axis) -> core::Result<std::int64_t>;

//  Narrows a canonical value into one declared axis range. A value outside the declared range is
//  rejected rather than clamped, and an axis that no value can satisfy is rejected as a declaration
//  that cannot be honoured.
[[nodiscard]] auto narrowToAxis(const JudgementAxisRange& axis, std::int64_t value)
    -> core::Result<std::int64_t>;

//  ---------------------------------------------------------------------------------------------
//  Requirement, relation, solver profile and fact binding declarations
//  ---------------------------------------------------------------------------------------------

//  One prepared requirement declaration.
//
//  `requiredActions`, `domainBinding` and the input-side action of an observation are three
//  independent declarations (P2-05). Nothing here derives one from another, and no function in this
//  module converts between them: a requirement declaring an action that no input ever reports is a
//  well-formed declaration, and a requirement whose domain binding mentions no action is equally
//  well-formed.
struct RequirementRecord final {
    //  The merge identity of this declaration (P1-02).
    StableDeclarationId stableId;
    //  The canonical six-tuple addressing identity (Spec 4). Two declarations carrying the same
    //  six-tuple are an identity collision and are rejected.
    RequirementIdentity identity;
    std::vector<RequiredActionRef> requiredActions;
    DomainBindingRef domainBinding;
    //  The judgement domain this requirement is scoped to, by domain id. A reference that does not
    //  resolve is a dangling reference and is rejected.
    std::string judgementDomainId;
    //  The declared phases. The tail phase is optional and is only ever explicit (Spec 3.8.3).
    std::vector<PhaseDeclaration> phases;
    //  Set by content whose semantics require Release / tail. With no declared tail phase this is a
    //  stable rejection: the batch never infers a tail from an implicit legacy profile.
    bool requiresReleaseTailSemantics;
    PatternDeclaration pattern;
    //  Requirement-level arm/deadline capacities. Absence and pending are intentionally distinct:
    //  neither is a publishable compiled identity, and the semantic diff must retain the state.
    std::optional<MeasuredParameter<std::uint64_t>> maxArmElements;
    std::optional<MeasuredParameter<std::uint64_t>> maxDeadlineElements;
    MeasureSpecDeclaration measure;
    std::vector<ResourceClaimDeclaration> resourceClaims;
    GraceDeclaration grace;
    //  The final grace value. In this batch it is the value the source declaration supplied (the
    //  input point of the second half); the resolution algorithm that computes it from the
    //  declaration is not implemented here, and this batch makes no claim about which values are
    //  legal in Stage 7A.
    PreparedGrace preparedGrace;
    //  FactBinding judgement-side existence references. A reference that does not resolve is
    //  rejected; the binding detail is presentation-side and is not carried.
    std::vector<std::string> factBindingRefs;
    //  The solver profile this requirement's coordination uses. Empty means "not declared", and a
    //  missing solver profile is a prepare atomic-failure condition (Spec 9.3).
    std::string solverProfileRef;
    //  ABI domain 3 `LocalClosePolicy`, as a declared token.
    std::string localClosePolicyToken;
    RequiredRefs required;
    //  Declared forms Stage 7A rejects (Spec 3.8.5 items 4, 7, 8, 9, 10, 11).
    std::vector<UnsupportedContentDeclaration> unsupportedForms;
    // Explicit arm membership, independent of required input actions.
    std::vector<std::string> patternArmRefs;
    std::vector<AtomBinding> atomBindings;
    std::optional<ClaimPolicyDeclaration::CompetitionKey> independentCompetition;
    struct SuccessWindow final {
        Tick start;
        Tick end;
        PhaseDeclaration phase;
        friend auto operator==(const SuccessWindow&, const SuccessWindow&) noexcept
            -> bool = default;
    };
    struct Timing final {
        Tick end;
        std::vector<SuccessWindow> successWindows;
        std::optional<TimeInterval> body;
        std::vector<PhaseTarget> phaseTargets;
        friend auto operator==(const Timing&, const Timing&) noexcept -> bool = default;
    };
    std::optional<Timing> timing;

    friend auto operator==(const RequirementRecord&, const RequirementRecord&) -> bool;
};

//  ABI domain 4 relation kinds. Stage 7A admits exactly `exclusive` with `capacity = 1`; `binding`,
//  `temporal` and `quota` are stable rejections (R-02 / R-03) and are representable here so that
//  the rejection can name the kind it refused.
enum class RelationKind : std::uint8_t {
    exclusive,
    binding,
    temporal,
    quota,
};

//  One declared relation. The 7A subset has exactly one relation kind; the member list of that
//  relation is the only membership carrier of the prepared graph, so there is no second group table
//  that could disagree with it.
struct RelationDeclaration final {
    RelationKind kind;
    ResourceRef resourceRef;
    //  The member requirements, by stable declaration id. Normalized to a sorted set: membership
    //  does not depend on the order a source happened to list it in.
    std::vector<StableDeclarationId> members;
    ClaimPolicyDeclaration policy;
    //  The declared capacity of the relation. Stage 7A admits exactly 1 (R-01 otherwise).
    std::uint64_t declaredCapacity;
    RequiredRefs required;

    friend auto operator==(const RelationDeclaration&, const RelationDeclaration&) noexcept
        -> bool = default;
};

//  One declared solver profile. The ABI fixes the field *semantics* (solverId / revision /
//  algorithm / objective / tieBreak / rejectIfNonUnique) and explicitly does not fix a default
//  list, the `K` / fuel / `max*` numbers or the serialization, which are S7A-9. No budget number is
//  representable here, so nothing can be consumed as one.
//
//  `objective` and `tieBreak` are ordered semantic lists, which is why they are not normalized.
struct SolverProfileDeclaration final {
    std::string solverId;
    std::string revision;
    std::string algorithmToken;
    std::vector<std::string> objective;
    std::vector<std::string> tieBreak;
    bool rejectIfNonUnique;
    RequiredRefs required;

    friend auto operator==(const SolverProfileDeclaration&,
                           const SolverProfileDeclaration&) noexcept -> bool = default;
};

//  A FactBinding judgement-side existence reference. Only existence travels: the binding detail is
//  presentation closure content and is not part of this graph.
struct FactBindingRef final {
    std::string bindingId;

    friend auto operator==(const FactBindingRef&, const FactBindingRef&) noexcept -> bool = default;
    friend auto operator<=>(const FactBindingRef&, const FactBindingRef&) noexcept
        -> std::strong_ordering = default;
};

//  ---------------------------------------------------------------------------------------------
//  Provenance: source closure and diagnostic map (P2-07)
//  ---------------------------------------------------------------------------------------------

//  The carried form of one typed gameplay source. It is provenance: recorded for diagnosis and
//  never a semantic input. Two carriers holding the same typed content assemble into the same graph
//  and the same identities, which is the "typed file / memory source consistency" the plan
//  requires, and making the form a diagnostic-only field is how that becomes a property of the
//  code.
enum class SourceForm : std::uint8_t {
    file,
    memory,
};

//  P2-07 item 1: the source closure. It carries the authoring provenance of one prepared artifact:
//  the stable identities of the source documents that took part and the compiler profile token.
//
//  Every member is authoring content or toolchain identity, and it enters the content projection.
//  It never enters the chart component of the judgement identity: a compiler profile change is a
//  different toolchain, not a different judgement.
struct SourceClosure final {
    //  Stable identities of every source document that took part, sorted by identity.
    std::vector<std::string> sourceDocumentIds;
    //  The compiler / packer profile token, declared by the toolchain. Unfrozen.
    std::string compilerProfileToken;

    friend auto operator==(const SourceClosure&, const SourceClosure&) noexcept -> bool = default;
};

//  P2-07 item 2: the diagnostic map, the separately named and separately owned form of the
//  `sourceMap` Spec 3.2 lists and Spec 5.2 attributes to diagnosis only.
//
//  Nothing in it enters any identity, and the semantic diff skips it as a whole rather than
//  comparing member by member, so a diagnostic map can never make two equivalent graphs look
//  different. The carrier form and the carrier provenance live here as well: they are what a human
//  needs to locate a fault, and they are not content.
struct DiagnosticMap final {
    SourceForm form;
    std::string carrierProvenance;
    struct Entry final {
        //  The stable declaration id the entry locates.
        std::string declarationId;
        std::string sourceDocumentId;
        //  The author-side field path of the declaration.
        std::string fieldPath;
    };
    std::vector<Entry> entries;
};

} // namespace cuexis::judgement
