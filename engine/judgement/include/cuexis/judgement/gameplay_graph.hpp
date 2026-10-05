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

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Reference attribution (P1-14 / Spec 3.8.6, P2-07)
//  ---------------------------------------------------------------------------------------------

//  The three classes a gameplay reference can belong to (Spec 3.8.6 rule 1). A reference belongs to
//  exactly one of them; it never crosses a class and is never registered twice.
//
//  P2-07 unifies the overlapping names of the identity table into three separately named things.
//  The mapping used by this batch is explicit and one-directional:
//
//    REF0 / judgement closure        <->  the judgement projection that the chart component and the
//                                         `content-artifact identity` carriers consume;
//    manifest / presentation closure <->  the presentation contribution, which never enters a
//                                         judgement identity (Spec 3.8.6 rule 3);
//    diagnostic / source map         <->  `DiagnosticMap`, which enters no identity at all.
enum class ReferenceClosure : std::uint8_t {
    judgementRef0,
    presentationManifest,
    diagnosticMap,
};

//  The declared kind of one gameplay reference. The kind is what decides the closure, and the
//  decision is made by prepare and never by the host, the presentation layer or a session (Spec
//  3.8.6 rule 5). The enumeration lists exactly the rows of the Spec 3.8.6 attribution matrix.
enum class ReferenceKind : std::uint8_t {
    //  Judgement-necessary references: a missing or changed one changes a Fact, a Score, a Combo,
    //  the statistics, the Fact order, an identity or a rejection.
    requirement,
    pattern,
    measure,
    resource,
    claimKey,
    emission,
    requirementIdentityProjection,
    timebaseProjection,
    latePolicyProjection,
    normalizationProfileProjection,
    solverProfileProjection,
    coordinationPolicyProjection,
    //  FactBinding judgement-side existence only. The binding detail stays presentation-side, which
    //  is exactly the "existence and judgement-domain decision only" cell of the matrix.
    factBindingExistence,
    //  Pure presentation references: a missing one still allows headless judgement.
    factBindingDetail,
    presentationResource,
    presentationOverride,
    //  Diagnosis, editors and migration only.
    sourceMapLocation,
};

//  The frozen attribution of one reference kind (Spec 3.8.6 matrix). It is a total function: every
//  kind has exactly one class, so a caller cannot pick a class for a kind.
[[nodiscard]] constexpr auto closureOf(ReferenceKind kind) noexcept -> ReferenceClosure {
    switch (kind) {
    case ReferenceKind::requirement:
    case ReferenceKind::pattern:
    case ReferenceKind::measure:
    case ReferenceKind::resource:
    case ReferenceKind::claimKey:
    case ReferenceKind::emission:
    case ReferenceKind::requirementIdentityProjection:
    case ReferenceKind::timebaseProjection:
    case ReferenceKind::latePolicyProjection:
    case ReferenceKind::normalizationProfileProjection:
    case ReferenceKind::solverProfileProjection:
    case ReferenceKind::coordinationPolicyProjection:
    case ReferenceKind::factBindingExistence:
        return ReferenceClosure::judgementRef0;
    case ReferenceKind::factBindingDetail:
    case ReferenceKind::presentationResource:
    case ReferenceKind::presentationOverride:
        return ReferenceClosure::presentationManifest;
    case ReferenceKind::sourceMapLocation:
        return ReferenceClosure::diagnosticMap;
    }
    return ReferenceClosure::diagnosticMap;
}

//  A request to register a reference in a named closure. Spec 3.8.6 rule 6 rejects a reference
//  whose attribution cannot be decided uniquely or that would cross classes, and the way to make
//  that a property of the code is to make the request the caller-supplied part: a request that
//  names the class the kind already owns succeeds and returns that class, and any other request is
//  a stable rejection rather than a re-registration.
[[nodiscard]] auto declareReference(ReferenceKind kind, ReferenceClosure requested)
    -> core::Result<ReferenceClosure>;

//  ---------------------------------------------------------------------------------------------
//  Declaration namespace identity (P1-02)
//  ---------------------------------------------------------------------------------------------

//  The stable identity of one declaration (P1-02): the identity of the source document it was
//  declared in, plus its declaration ordinal inside that document.
//
//  Both halves travel with the declaration. The ordinal is never a position in the array the
//  assembler was handed, so permuting the input array cannot renumber anything, and the canonical
//  order below is the stable id order rather than the input order.
struct StableDeclarationId final {
    std::string sourceDocumentId;
    std::uint32_t declarationOrdinal;

    friend auto operator==(const StableDeclarationId&, const StableDeclarationId&) noexcept
        -> bool = default;
    friend auto operator<=>(const StableDeclarationId&, const StableDeclarationId&) noexcept
        -> std::strong_ordering = default;
};

//  What a named declaration declares. The merge treats every kind alike: a name is unique in the
//  merged namespace whatever it names.
enum class DeclarationKind : std::uint8_t {
    requirement,
    patternDefinition,
    measureDefinition,
    resourceRecord,
    judgementDomain,
    solverProfile,
};

//  A reference from one declaration to another.
//
//  `sameDocument` resolves inside the referring declaration's own source document. An
//  `explicitCrossDocument` reference names the other document as well; P1-02 requires a
//  cross-invocation reference to be explicit, so a cross-document reference that leaves the
//  document identity empty is a stable rejection and is never resolved by looking the name up
//  somewhere else.
enum class ReferenceScope : std::uint8_t {
    sameDocument,
    explicitCrossDocument,
};

struct DeclarationRef final {
    ReferenceScope scope;
    //  Required when `scope` is `explicitCrossDocument`; absent (empty) otherwise.
    std::string sourceDocumentId;
    std::uint32_t declarationOrdinal;

    friend auto operator==(const DeclarationRef&, const DeclarationRef&) noexcept -> bool = default;
    friend auto operator<=>(const DeclarationRef&, const DeclarationRef&) noexcept
        -> std::strong_ordering = default;
};

//  One declaration of the merged global namespace. It owns its name and its resolved references, so
//  the merged namespace is a value that outlives the request that produced it.
struct MergedDeclaration final {
    StableDeclarationId stableId;
    DeclarationKind kind;
    //  The global name. Two declarations with the same name are rejected, never merged (P1-02), so
    //  a name identifies at most one declaration here.
    std::string name;
    //  Resolved references, sorted by the referenced stable id. Every entry exists in the merged
    //  namespace: a dangling reference is an atomic prepare failure.
    std::vector<StableDeclarationId> references;
    //  The declared needs of this declaration (Spec 6.1 derivation input).
    std::vector<std::string> requiredFeatureIds;
    std::vector<std::string> requiredCapabilityIds;

    friend auto operator==(const MergedDeclaration&, const MergedDeclaration&) noexcept
        -> bool = default;
};

//  The merged global namespace (P1-02), sorted by stable id. The sort is what makes the merge
//  invariant under a permutation of the input: the result is a function of the stable ids alone.
struct MergedNamespace final {
    std::vector<MergedDeclaration> declarations;

    friend auto operator==(const MergedNamespace&, const MergedNamespace&) noexcept
        -> bool = default;
};

//  ---------------------------------------------------------------------------------------------
//  Capability and feature references
//  ---------------------------------------------------------------------------------------------

//  A capability reference declared by content (ABI domain 3 `CapabilityRef`). It is an opaque
//  stable id plus the revision the declaration named. The capability registry entry field set is
//  unfrozen (CM-X01, first consumer S7A-7), so this batch carries nothing but the two tokens and
//  claims no ordering, no range and no monotonicity rule beyond exact token equality.
//
//  An empty revision is the documented absence marker "this declaration named no revision". It is
//  carried as declared and is never defaulted to a value this batch would have to invent.
struct CapabilityRef final {
    std::string capabilityId;
    std::string revision;

    friend auto operator==(const CapabilityRef&, const CapabilityRef&) noexcept -> bool = default;
    friend auto operator<=>(const CapabilityRef&, const CapabilityRef&) noexcept
        -> std::strong_ordering = default;
};

//  A capability of the physical carrier (Packed META `requiredFeatures`). Unfrozen and opaque: this
//  batch invents no feature registry and no feature semantics.
struct FeatureRef final {
    std::string featureId;

    friend auto operator==(const FeatureRef&, const FeatureRef&) noexcept -> bool = default;
    friend auto operator<=>(const FeatureRef&, const FeatureRef&) noexcept
        -> std::strong_ordering = default;
};

//  What one declaration states it needs. The assembler is the only thing that derives a closure
//  from these declarations, and it never adds a token the content did not state (Spec 6.2 rule 1:
//  the compiler must not silently fill the gap).
struct RequiredRefs final {
    std::vector<FeatureRef> features;
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const RequiredRefs&, const RequiredRefs&) noexcept -> bool = default;
};

//  The declared closure tables of a prepared graph. They are separate values from the derived
//  closures below and there is deliberately no operation that merges them (ABI domain 8).
struct DeclaredCapabilitySet final {
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const DeclaredCapabilitySet&, const DeclaredCapabilitySet&) noexcept
        -> bool = default;
};

struct FeatureClosure final {
    std::vector<FeatureRef> features;

    friend auto operator==(const FeatureClosure&, const FeatureClosure&) noexcept -> bool = default;
};

//  The complete closure the offline assembler derived from the canonical graph, the ruleset binding
//  and the presentation closure contribution (Spec 6.1). Sorted and deduplicated, so the closure is
//  a set: the order a caller declared its needs in cannot reach it.
struct DerivedCapabilityClosure final {
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const DerivedCapabilityClosure&,
                           const DerivedCapabilityClosure&) noexcept -> bool = default;
};

struct ResourceRef final {
    std::string resourceId;

    friend auto operator==(const ResourceRef&, const ResourceRef&) noexcept -> bool = default;
    friend auto operator<=>(const ResourceRef&, const ResourceRef&) noexcept
        -> std::strong_ordering = default;
};

//  Every resource the graph references, sorted by resource id. A reference that does not resolve to
//  a `ResourceRecord` is a dangling reference and is rejected, so every closure entry resolves.
struct ResourceClosure final {
    std::vector<ResourceRef> resources;

    friend auto operator==(const ResourceClosure&, const ResourceClosure&) noexcept
        -> bool = default;
};

//  The capability needs of the ruleset binding and of the presentation closure, as declarations.
//
//  Not every source of the Spec 6.1 derivation chain is chart content: the ruleset binding and the
//  presentation closure contribute needs of their own. Only their *needs* travel here, never their
//  content (no material, no shader, no animation, no audio), so carrying them cannot put a
//  presentation field into the judgement closure.
struct ClosureContributions final {
    std::vector<FeatureRef> features;
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const ClosureContributions&, const ClosureContributions&) noexcept
        -> bool = default;
};

//  ---------------------------------------------------------------------------------------------
//  Requirement identity (Spec 4, ABI domain 7)
//  ---------------------------------------------------------------------------------------------

//  One step of an emission path: a stable node id and the repeat index that node was expanded at.
struct EmissionPathStep final {
    std::string nodeId;
    std::uint64_t repeatIndex;

    friend auto operator==(const EmissionPathStep&, const EmissionPathStep&) noexcept
        -> bool = default;
    friend auto operator<=>(const EmissionPathStep&, const EmissionPathStep&) noexcept
        -> std::strong_ordering = default;
};

//  The canonical stable addressing identity of one requirement: the frozen six-tuple of Spec 4.
//
//  The comparison below is the canonical identity order of the graph tables. It is not the S7A-4
//  fact sort key and not a same-tick tie rule: the unique canonical fact order of Spec 3.9 is owned
//  by S7A-4 and is deliberately not represented here.
struct RequirementIdentity final {
    std::string chartEntryId;
    std::string invocationId;
    std::string moduleId;
    std::string exportId;
    std::vector<EmissionPathStep> emissionPath;
    std::string requirementLocalId;

    friend auto operator==(const RequirementIdentity&, const RequirementIdentity&) noexcept
        -> bool = default;
    friend auto operator<=>(const RequirementIdentity&, const RequirementIdentity&) noexcept
        -> std::strong_ordering = default;
};

//  The requirement-side action declaration (P2-05). It is a distinct type from the input-side
//  `InputAction` of <cuexis/judgement/input_boundary.hpp>: the requirement declares what it
//  requires, the observation carries what arrived, and nothing converts one into the other or
//  derives one from the other.
class RequiredActionRef final {
  public:
    RequiredActionRef() noexcept = default;

    //  The only way to name a required action. Empty means "not declared" and is rejected here, at
    //  the boundary, instead of travelling as an unnamed declaration every consumer would read
    //  differently.
    [[nodiscard]] static auto fromToken(std::string token) -> core::Result<RequiredActionRef>;

    [[nodiscard]] auto token() const noexcept -> const std::string& {
        return token_;
    }

    friend auto operator==(const RequiredActionRef&, const RequiredActionRef&) noexcept
        -> bool = default;
    friend auto operator<=>(const RequiredActionRef&, const RequiredActionRef&) noexcept
        -> std::strong_ordering = default;

  private:
    explicit RequiredActionRef(std::string token) : token_(std::move(token)) {}

    std::string token_;
};

//  The scope binding of a requirement (P2-05). It names the domain the requirement is scoped to; it
//  is neither an action nor an amount, and it is not derived from either. The enumeration set
//  behind the token belongs to round 6 and is not frozen, so a token is all this batch carries.
class DomainBindingRef final {
  public:
    DomainBindingRef() noexcept = default;

    [[nodiscard]] static auto fromToken(std::string token) -> core::Result<DomainBindingRef>;

    [[nodiscard]] auto token() const noexcept -> const std::string& {
        return token_;
    }

    friend auto operator==(const DomainBindingRef&, const DomainBindingRef&) noexcept
        -> bool = default;
    friend auto operator<=>(const DomainBindingRef&, const DomainBindingRef&) noexcept
        -> std::strong_ordering = default;

  private:
    explicit DomainBindingRef(std::string token) : token_(std::move(token)) {}

    std::string token_;
};

//  Requirement-level Pattern capacity declarations. The optional wrapper distinguishes a field
//  that was not declared at all from a declared field whose measurement is still pending.
enum class RequirementCapacityState : std::uint8_t {
    absent,
    pending,
    measured,
};

struct RequirementCapacityKey final {
    RequirementCapacityState state;
    std::uint64_t value;

    friend auto operator==(const RequirementCapacityKey&, const RequirementCapacityKey&) noexcept
        -> bool = default;
};

[[nodiscard]] auto
requirementCapacityKey(const std::optional<MeasuredParameter<std::uint64_t>>& value) noexcept
    -> RequirementCapacityKey;

[[nodiscard]] auto
compareRequirementCapacity(const std::optional<MeasuredParameter<std::uint64_t>>& left,
                           const std::optional<MeasuredParameter<std::uint64_t>>& right) noexcept
    -> std::strong_ordering;

//  ---------------------------------------------------------------------------------------------
//  Pattern declarations (ABI domain 3)
//  ---------------------------------------------------------------------------------------------

//  The ABI's frozen primitive set. No enumerator carries a numeric value and the enumerator order
//  carries no semantics; the identity encoder spells each primitive by a stable token instead of by
//  its ordinal precisely so that no reader can mistake the order for a contract.
//
//  `boundedRepeat` is a prepare-time finite static structure only (Spec 3.8.4): a run-time repeat
//  counter, an accumulator and a dynamically generated requirement are not representable as a
//  primitive at all.
enum class PatternPrimitive : std::uint8_t {
    atom,
    sequence,
    choice,
    boundedRepeat,
    skip,
    instant,
    complement,
};

//  ABI domain 3 `MatchPolicy`: non-deterministic matching is fixed to `leftmost-first`. One
//  enumerator, because the contract admits one policy; a second enumerator would be a second
//  policy.
enum class MatchPolicy : std::uint8_t {
    leftmostFirst,
};

//  The declared repeat bounds of one `boundedRepeat`. These are declarations, not limits: the
//  prepare-time expansion and its budget check are the second half, and this batch never turns a
//  declared bound into a production limit.
struct RepeatBoundsDeclaration final {
    std::uint64_t minimum;
    std::uint64_t maximum;

    friend auto operator==(const RepeatBoundsDeclaration&, const RepeatBoundsDeclaration&) noexcept
        -> bool = default;
    friend auto operator<=>(const RepeatBoundsDeclaration&, const RepeatBoundsDeclaration&) noexcept
        -> std::strong_ordering = default;
};

//  The content forms Stage 7A must reject rather than implement, each with the stable rejection
//  entry of Spec 7.2 that owns it.
//
//  Every form is representable on purpose. A form that could not be spelled at all would turn a
//  stable rejection into a compile error, and the contract requires the rejection: a caller has to
//  be able to say "this content is a bounded relation instance" and be told no.
enum class UnsupportedContentKind : std::uint8_t {
    //  Non-empty Gameplay `effects` that was not explicitly lowered into presentation data that
    //  provably does not affect judgement or Replay (Q-05, R-18).
    gameplayEffectsNotLowered,
    //  An unbounded or otherwise unexpanded loop representation (Spec 3.8.4, CM-C10 / P1-03). The
    //  stable reject code for this form is closed with this batch and stays a src-only token.
    boundedRelationInstance,
    //  A run-time repeat counter, accumulator or dynamic requirement generation (R-14).
    runtimeRepeatCounter,
    dynamicRequirementGeneration,
    //  `sameContact` and a cross-requirement relation hidden inside a pattern (R-11).
    sameContact,
    crossRequirementRelation,
    //  Continuous trajectories, a contact-following slider, area coverage, a minimum report rate
    //  and
    //  reconstruction (R-05, admitted by S7B-1 only).
    continuousTrajectory,
    contactFollowingSlider,
    minimumReportRate,
    reconstruction,
    //  Handoff Hold and resource migration between requirements (R-04).
    handoffHold,
    resourceMigration,
    //  Owner sets and parallel slots of one resource (R-01).
    resourceOwnerSet,
    resourceParallelSlot,
};

//  One declared content form Stage 7A rejects. The declared token is carried so the diagnostic can
//  name what it refused instead of only naming the kind.
struct UnsupportedContentDeclaration final {
    UnsupportedContentKind kind;
    std::string declaredToken;

    friend auto operator==(const UnsupportedContentDeclaration&,
                           const UnsupportedContentDeclaration&) noexcept -> bool = default;
    friend auto operator<=>(const UnsupportedContentDeclaration&,
                            const UnsupportedContentDeclaration&) noexcept
        -> std::strong_ordering = default;
};

//  One Pattern declaration node: a primitive applied to zero or more operand nodes, with the
//  declared reference an atom matches and the declared bounds of a bounded repeat.
//
//  It is a declaration tree in the source's own shape and deliberately not a compiled automaton.
//  `operands` is ordered and the order is semantic: it is the arm order of a leftmost-first choice
//  and the element order of a sequence, which is why it is not normalized.
struct PatternNodeDeclaration final {
    PatternPrimitive primitive{};
    std::vector<PatternNodeDeclaration> operands;
    //  The declared reference an `atom` matches. Empty for every other primitive, and a primitive
    //  that requires it while it is empty is a structurally incomplete declaration.
    std::string atomRef;
    //  Present exactly for `boundedRepeat`.
    std::optional<RepeatBoundsDeclaration> repeatBounds;
    //  Present when this node is one of the forms Stage 7A rejects.
    std::optional<UnsupportedContentDeclaration> unsupportedForm;

    PatternNodeDeclaration() = default;
    PatternNodeDeclaration(PatternPrimitive primitive, std::vector<PatternNodeDeclaration> operands,
                           std::string atomRef = {},
                           std::optional<RepeatBoundsDeclaration> repeatBounds = {},
                           std::optional<UnsupportedContentDeclaration> unsupportedForm = {});
    PatternNodeDeclaration(const PatternNodeDeclaration& other);
    PatternNodeDeclaration(PatternNodeDeclaration&& other) noexcept = default;
    auto operator=(const PatternNodeDeclaration& other) -> PatternNodeDeclaration&;
    auto operator=(PatternNodeDeclaration&& other) noexcept -> PatternNodeDeclaration&;
    ~PatternNodeDeclaration() noexcept;

    friend auto operator==(const PatternNodeDeclaration&, const PatternNodeDeclaration&) -> bool;

  private:
    // Used only while destroying a tree; never part of its declaration or identity.
    PatternNodeDeclaration* destructionParent_ = nullptr;
};

//  One Pattern declaration. An empty `patternId` means the requirement declares its pattern inline
//  (the Chart v5 candidate path); a non-empty one names a declaration of the merged namespace.
struct PatternDeclaration final {
    std::string patternId;
    //  ABI domain 3 fixes non-deterministic matching to `leftmost-first`, so this field has exactly
    //  one legal declaration. It is carried instead of assumed, because "which policy does this
    //  pattern use" is a declaration the reader can check rather than a convention.
    MatchPolicy matchPolicy;
    PatternNodeDeclaration root;
    RequiredRefs required;

    friend auto operator==(const PatternDeclaration&, const PatternDeclaration&) -> bool;
};

//  ---------------------------------------------------------------------------------------------
//  Measure declarations (ABI domain 3)
//  ---------------------------------------------------------------------------------------------

//  The phase classification of one requirement, as far as Stage 7A fixes it: a Tap segment, Hold
//  head / body and the explicitly declared Release / tail. The phase *registry* and the phase
//  priority weights are CM-F06 and are not frozen, so no priority value is representable.
//
//  `tap` is the one-segment case: Spec 3.20 rule 3 ties `FactCategory` to the phase one to one, and
//  the frozen category set contains `tap`, so a requirement whose single segment is a Tap declares
//  that phase here instead of having a category with no phase.
enum class PhaseKind : std::uint8_t {
    tap,
    head,
    body,
    tail,
};

//  One declared phase of a requirement. Release / tail is only ever an explicitly declared optional
//  phase of the same requirement and never an automatically generated second requirement (Spec
//  3.8.3, Q-18).
struct PhaseDeclaration final {
    PhaseKind kind;
    //  Declaration ordinal of the phase inside its requirement. It is a declaration index, not a
    //  priority.
    std::uint32_t declarationOrdinal;

    friend auto operator==(const PhaseDeclaration&, const PhaseDeclaration&) noexcept
        -> bool = default;
    friend auto operator<=>(const PhaseDeclaration&, const PhaseDeclaration&) noexcept
        -> std::strong_ordering = default;
};

struct PhaseTarget final {
    PhaseKind phase;
    Tick chartTick;

    friend auto operator==(const PhaseTarget&, const PhaseTarget&) noexcept -> bool = default;
    friend auto operator<=>(const PhaseTarget&, const PhaseTarget&) noexcept
        -> std::strong_ordering = default;
};

struct AmountMatchRange final {
    std::int64_t minimum;
    std::int64_t maximum;

    friend auto operator==(const AmountMatchRange&, const AmountMatchRange&) noexcept
        -> bool = default;
    friend auto operator<=>(const AmountMatchRange&, const AmountMatchRange&) noexcept
        -> std::strong_ordering = default;
};

struct AtomBinding final {
    std::string atomRef;
    std::string domainToken;
    std::string sourceClass;
    std::string channelToken;
    InputAction action;
    std::optional<AmountMatchRange> amountRange;
    bool tailOnly;

    friend auto operator==(const AtomBinding&, const AtomBinding&) noexcept -> bool = default;
};

//  ABI domain 5 `FactCategory`: the fact classification layer, one to one with the phase (Spec 3.20
//  rule 3). The category is *derived* from the phase and is never assigned by a caller, so a
//  component that spells a different category than its phase implies is refused rather than
//  accepted with two names for one fact.
enum class FactCategory : std::uint8_t {
    tap,
    holdHead,
    holdBody,
    holdTail,
};

//  The frozen Stage 7A fact category set, in canonical order. A category outside this set is not
//  representable at all.
inline constexpr std::array<FactCategory, 4> kStage7AFactCategories{
    FactCategory::tap, FactCategory::holdHead, FactCategory::holdBody, FactCategory::holdTail};

//  The one-to-one derivation of Spec 3.20 rule 3: the phase decides the category.
[[nodiscard]] constexpr auto factCategoryOfPhase(PhaseKind phase) noexcept -> FactCategory {
    switch (phase) {
    case PhaseKind::tap:
        return FactCategory::tap;
    case PhaseKind::head:
        return FactCategory::holdHead;
    case PhaseKind::body:
        return FactCategory::holdBody;
    case PhaseKind::tail:
        return FactCategory::holdTail;
    }
    return FactCategory::tap;
}

//  ABI domain 5 `Outcome`: the Stage 7A result layer. The set is exactly these two values (Spec
//  3.20 rule 1 and rule 2); a Hold head / body / tail outcome is a *phase-local* outcome, it is not
//  a member here and it does not add a second result dimension.
enum class Outcome : std::uint8_t {
    hit,
    miss,
};

inline constexpr std::array<Outcome, 2> kStage7AOutcomes{Outcome::hit, Outcome::miss};

//  The stable spelling of one fact category, as declared by the content in a component's
//  `categoryToken`. Declared here and defined in the implementation, so the header itself carries
//  no string table.
[[nodiscard]] auto factCategoryToken(FactCategory category) noexcept -> std::string_view;

//  The optional grade table result of one component. The grade scale, the tolerance and the
//  aggregation rule are CM-S10 / P1-07 and are not frozen, so the only thing Stage 7A fixes is the
//  presence state: a component whose content declared no grade table stays `absent` and is never
//  given a default table (Spec 3.20 rule 4).
enum class GradePresence : std::uint8_t {
    absent,
    declared,
};

//  One declared measure component: the phase it quantifies, the category it reports under and the
//  grade tokens its content declared for it.
//
//  Stage 7A allows several phase/category components and applies grading per component. The grade
//  *scale*, the tolerance and the aggregation rule belong to CM-S10 / P1-07 and are first consumed
//  by S7A-5, so the declared tokens travel as opaque declarations: this batch neither interprets
//  them nor substitutes a default table for a missing one.
struct MeasureComponentDeclaration final {
    PhaseKind phase;
    //  The caller-declared category token, when the content declared one. Empty is "not assigned",
    //  which leaves the category to be derived from the phase; a non-empty value must be exactly
    //  the derived category, because Spec 3.20 rule 3 forbids a caller-assigned category.
    std::string categoryToken;
    //  The grade tokens the content declared for this component, in declaration order. An empty
    //  table is the `absent` state of the grade, not a table of defaults.
    std::vector<std::string> declaredGradeTokens;

    friend auto operator==(const MeasureComponentDeclaration&,
                           const MeasureComponentDeclaration&) noexcept -> bool = default;
    friend auto operator<=>(const MeasureComponentDeclaration&,
                            const MeasureComponentDeclaration&) noexcept
        -> std::strong_ordering = default;
};

struct MeasureSpecDeclaration final {
    //  One component per declared phase/category. The table is normalized as a set keyed by
    //  (phase, category) because the declaration order of the components carries no semantics.
    std::vector<MeasureComponentDeclaration> components;
    RequiredRefs required;

    friend auto operator==(const MeasureSpecDeclaration&, const MeasureSpecDeclaration&) noexcept
        -> bool = default;
};

//  ---------------------------------------------------------------------------------------------
//  Resources: the three-way split (S7A6-R10)
//  ---------------------------------------------------------------------------------------------

//  ABI domain 3 `ResourceClaimIntent`. `observe` produces an Observation only: it does not occupy
//  the resource, does not change its owner, does not enter the resource state table and needs no
//  slot (Spec 3.10 rule 6).
enum class ResourceClaimIntent : std::uint8_t {
    observe,
    consume,
    claim,
};

//  The claim policy side of the split. Its field set is P2-06 and is unfrozen, so only the declared
//  policy token and the stable claim key travel here; the claim/ownership resolution is the second
//  half of S7A-3.
struct ClaimPolicyDeclaration final {
    //  Stable policy token. Empty means "not declared" and is rejected.
    std::string policyToken;
    //  ABI domain 4 `ClaimKey`: the stable arbitration key after prepare. It is a declaration here;
    //  the key derivation belongs to S7A-4.
    std::string claimKeyToken;
    // K4: only this explicit pair orders occupying claims; absence is not priority zero.
    struct CompetitionKey final {
        std::int64_t priority;
        std::uint64_t tieRank;
        friend auto operator==(const CompetitionKey&, const CompetitionKey&) noexcept
            -> bool = default;
        friend auto operator<=>(const CompetitionKey&, const CompetitionKey&) noexcept = default;
    };
    std::optional<CompetitionKey> competition;

    friend auto operator==(const ClaimPolicyDeclaration&, const ClaimPolicyDeclaration&) noexcept
        -> bool = default;
    friend auto operator<=>(const ClaimPolicyDeclaration&, const ClaimPolicyDeclaration&) noexcept
        -> std::strong_ordering = default;
};

//  How a declared grace override was requested. `sticky` and `observe` are the two illegal override
//  forms plan S7A-3 item 4 makes stable diagnostics; they are representable so that the second half
//  can reject them by name.
enum class GraceOverrideMode : std::uint8_t {
    none,
    sticky,
    observe,
};

struct GraceOverrideDeclaration final {
    GraceOverrideMode mode;
    std::string overrideToken;

    friend auto operator==(const GraceOverrideDeclaration&,
                           const GraceOverrideDeclaration&) noexcept -> bool = default;
};

//  ABI domain 3 `GraceResolutionPolicy`: the declared explicit / inherited / default resolution
//  policy. No enumerator is a default *value*: the policy is a declaration the content has to make,
//  and a missing declaration is a rejection rather than the first enumerator.
enum class GraceResolutionPolicy : std::uint8_t {
    explicitDeclaration,
    inheritedDeclaration,
    defaultDeclaration,
};

//  ABI domain 3 `PreparedGrace`: the final grace value, read-only after prepare.
//
//  The value type is fixed by this batch; the resolution algorithm that computes it is not. Two
//  requirements whose prepared grace and grace policy are the same share a judgement identity, and
//  the explicit / inherited *source* of the value enters the content identity and the diagnostic
//  context only (Spec 5.2, plan S7A-3 item 5).
class PreparedGrace final {
  public:
    PreparedGrace() noexcept = default;
    explicit PreparedGrace(TickSpan span) noexcept : span_(span) {}

    [[nodiscard]] auto span() const noexcept -> TickSpan {
        return span_;
    }

    friend auto operator==(const PreparedGrace&, const PreparedGrace&) noexcept -> bool = default;

  private:
    TickSpan span_{};
};

//  The grace input point of one requirement: which policy applies, where an inherited value would
//  come from, and whether the chart may supply one.
//
//  `allowChartGrace` is a declaration flag, not a policy: whether a chart-supplied value is legal
//  is decided by the resolution algorithm, which is the second half. Nothing in this declaration is
//  validated by this batch beyond its structure.
struct GraceDeclaration final {
    GraceResolutionPolicy policy;
    //  The declaration an inherited value would come from, named by its stable declaration id.
    //  Empty for an explicit or default declaration.
    std::string inheritedFromDeclarationId;
    bool allowChartGrace;

    friend auto operator==(const GraceDeclaration&, const GraceDeclaration&) noexcept
        -> bool = default;
};

//  One requirement-side resource reference: the reference, the policy and the intent, as three
//  separate fields of three separate types (S7A6-R10). The resource record itself is a distinct
//  member of the graph and is never reachable through this declaration.
struct ResourceClaimDeclaration final {
    ResourceRef resourceRef;
    ClaimPolicyDeclaration claimPolicy;
    ResourceClaimIntent intent;
    GraceOverrideDeclaration graceOverride;

    friend auto operator==(const ResourceClaimDeclaration&,
                           const ResourceClaimDeclaration&) noexcept -> bool = default;
};

//  The global definition of one resource: the third piece of the split. It owns the declared
//  capacity, the stable slot token and the declared representations Stage 7A rejects.
//
//  `declaredGapGrace` is the Gameplay I resource-state grace, for which Spec 3.10 fixes `0` as the
//  only legal Stage 7A value. It is deliberately a different field from `PreparedGrace`: V2 splits
//  the old concept, and reading one as the other is exactly the confusion the ruling forbids.
struct ResourceRecord final {
    ResourceRef ref;
    //  The capacity the content declared. Stage 7A admits exactly 1; any other declared value
    //  reaches a stable rejection instead of being unspellable (Spec 3.10).
    std::uint64_t declaredCapacity;
    //  The stable slot identity of the prepared graph. A `capacity = 1` resource uses one slot and
    //  the slot identity enters the prepared judgement inputs; the engine assigns it, a host does
    //  not.
    std::string slotToken;
    //  ABI domain 4 `ResourceDecisionPolicyRef`. Declared token; its field set is not frozen.
    std::string decisionPolicyRef;
    //  Whether the resource becomes permanently terminal after termination (slot reuse then stops).
    bool terminalAfterTermination;
    //  The declared Gameplay I resource-state grace, in ticks. Spec 3.10: only 0 is legal in 7A.
    TickSpan declaredGapGrace;
    //  Declared representations the 7A subset rejects: owner sets and parallel slots (R-01),
    //  handoff and the gap / handoff_pending states (R-04).
    std::vector<UnsupportedContentDeclaration> unsupportedForms;
    RequiredRefs required;

    friend auto operator==(const ResourceRecord&, const ResourceRecord&) noexcept -> bool = default;
};

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
