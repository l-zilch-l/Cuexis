#pragma once

//  Canonical gameplay graph - phases, facts, measures and resource or grace declarations.

#include "gameplay_graph_pattern.hpp"

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
struct GradeInterval final {
    std::int64_t minimum, maximum;
    std::string grade;
    friend auto operator==(const GradeInterval&, const GradeInterval&) -> bool = default;
    friend auto operator<=>(const GradeInterval&, const GradeInterval&) = default;
};

struct MeasureComponentDeclaration final {
    PhaseKind phase;
    //  The caller-declared category token, when the content declared one. Empty is "not assigned",
    //  which leaves the category to be derived from the phase; a non-empty value must be exactly
    //  the derived category, because Spec 3.20 rule 3 forbids a caller-assigned category.
    std::string categoryToken;
    //  The grade tokens the content declared for this component, in declaration order. An empty
    //  table is the `absent` state of the grade, not a table of defaults.
    std::vector<std::string> declaredGradeTokens;
    std::optional<std::vector<GradeInterval>> gradeTable;

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

} // namespace cuexis::judgement
