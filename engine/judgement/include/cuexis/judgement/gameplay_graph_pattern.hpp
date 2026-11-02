#pragma once

//  Canonical gameplay graph - requirement identity, capacity and pattern declarations.

#include "gameplay_graph_reference.hpp"

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

} // namespace cuexis::judgement
