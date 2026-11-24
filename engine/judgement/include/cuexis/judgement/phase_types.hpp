#pragma once

//  Judgement runtime - phase and declaration identity value types.
//
//  These are the phase-layer values the running kernel and the prepared records use: the declared
//  phase kinds and phases, the two-value outcome, the fact category derived from a phase and the
//  stable declaration identity. They are pure runtime values: an independent runtime consumer and
//  the running records read them without the graph, the assembler or any compile-side header.

#include <array>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement {

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

//  The stable identity of one declaration (P1-02).
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

} // namespace cuexis::judgement
