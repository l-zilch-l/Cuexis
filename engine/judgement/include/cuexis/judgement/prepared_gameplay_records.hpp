#pragma once

//  Judgement runtime - the prepared gameplay records and the pure-tick input view.
//
//  This header is the runtime side of the author / compiled boundary. It carries plain runtime
//  values only: Core, the judgement runtime value headers and the standard library. No author
//  header (`gameplay_assembler*`, `gameplay_graph*`, `timebase.hpp`) and no compile-side context is
//  reachable from here, so an independent runtime consumer can build the view itself.
//
//  Namespace mapping: the running records live in `cuexis::judgement::runtime`. The legacy prepared
//  types (`cuexis::judgement::PreparedRequirement`, `PreparedResource`, `PreparedGameplay`,
//  `PreparedTimerKey`) keep their existing declarations and ABI; they are not renamed and their
//  size/layout baseline is untouched. The shared value declarations (`StableDeclarationId`,
//  `PhaseKind`, `Outcome`, `FactCategory`, `RequirementIdentity`, `PreparedIdentityDeclarations`)
//  are the same types in both worlds because the runtime headers own them and the author headers
//  re-include them. The small legacy declaration shapes the records use (`AtomBinding`,
//  `ClaimPolicyDeclaration`, `ResourceClaimDeclaration`, `CompiledMeasureComponent`,
//  `PatternExecutionProgram`, `LatePolicyParameters`, `PreparedTimerKey`) are mirrored here in the
//  runtime namespace; the author-side copies stay untouched for compatibility and a compiler /
//  Reader adapts them explicitly. Mirrored shapes carry the same member names, order and default
//  comparisons as their author-side counterparts so the adaptation is a field copy, not a
//  reinterpretation.

#include <cuexis/judgement/normalized_score.hpp>
#include <cuexis/judgement/phase_types.hpp>
#include <cuexis/judgement/prepared_identity.hpp>
#include <cuexis/judgement/timebase_tick.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement {
class PreparedRuleset;
}

namespace cuexis::judgement::runtime {

//  ---------------------------------------------------------------------------------------------
//  Mirrored declaration shapes (field-compatible with the author-side copies)
//  ---------------------------------------------------------------------------------------------

//  The input-side verb, mirrored from `<cuexis/judgement/input_boundary_mapping.hpp>` so this
//  header does not pull the author timebase chain. The enumerators and their order are identical.
enum class InputAction : std::uint8_t {
    press,
    release,
    update,
};

//  A literal late event policy. The set is the ABI domain 1 set; no default value is declared.
enum class LateEventPolicy : std::uint8_t {
    rejectLate,
    queueNextTick,
};

//  A typed late-policy parameter. "Not measured" is a state of its own, never a numeric default.
template <typename Value> class MeasuredParameter final {
  public:
    [[nodiscard]] static constexpr auto pendingMeasurement() noexcept -> MeasuredParameter {
        return MeasuredParameter{};
    }

    [[nodiscard]] static constexpr auto measured(Value value) noexcept -> MeasuredParameter {
        MeasuredParameter parameter;
        parameter.value_ = value;
        return parameter;
    }

    [[nodiscard]] constexpr auto isPendingMeasurement() const noexcept -> bool {
        return !value_.has_value();
    }
    [[nodiscard]] constexpr auto isMeasured() const noexcept -> bool {
        return value_.has_value();
    }
    [[nodiscard]] constexpr auto measuredValue() const noexcept -> const Value* {
        return value_.has_value() ? &value_.value() : nullptr;
    }

    friend constexpr auto operator==(MeasuredParameter left, MeasuredParameter right) noexcept
        -> bool = default;

  private:
    std::optional<Value> value_{};
};

//  The four typed late-policy parameters plus the session-level policy selection.
struct LatePolicyParameters final {
    MeasuredParameter<TickSpan> finalizationWatermark =
        MeasuredParameter<TickSpan>::pendingMeasurement();
    MeasuredParameter<TickSpan> maxQueueHop = MeasuredParameter<TickSpan>::pendingMeasurement();
    MeasuredParameter<TickSpan> windowCloseThreshold =
        MeasuredParameter<TickSpan>::pendingMeasurement();
    MeasuredParameter<TickSpan> windowOpenThreshold =
        MeasuredParameter<TickSpan>::pendingMeasurement();
    std::optional<LateEventPolicy> policy{};

    friend auto operator==(const LatePolicyParameters&, const LatePolicyParameters&) noexcept
        -> bool = default;
};

//  One timing timer key. The runtime view carries the same fields as the legacy key; the session
//  builder creates these from prepared timing, not from an author graph.
enum class PreparedTimerKind : std::uint8_t {
    activation,
    windowOpen,
    bodyEnd,
    phaseClose,
    hardDeadline
};

struct PreparedTimerKey final {
    RequirementIdentity requirement;
    PhaseKind phase;
    PreparedTimerKind kind;
    Tick tick;
    std::optional<Tick> windowStart;
    std::optional<Tick> windowEnd;
    friend auto operator==(const PreparedTimerKey&, const PreparedTimerKey&) -> bool = default;
};

//  One accepted amount range on an atom binding.
struct AmountMatchRange final {
    std::int64_t minimum;
    std::int64_t maximum;
    friend auto operator==(const AmountMatchRange&, const AmountMatchRange&) noexcept
        -> bool = default;
};

//  One atom binding as the runtime records carry it.
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

//  The resource claim intent of one declaration.
enum class ResourceClaimIntent : std::uint8_t {
    claim,
    observe,
};

//  The declared arbitration policy of one claim.
struct ClaimPolicyDeclaration final {
    std::string policyToken;
    std::string claimKeyToken;
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
};

//  How a declared grace override was requested.
enum class GraceOverrideMode : std::uint8_t { none, sticky, observe };

struct GraceOverrideDeclaration final {
    GraceOverrideMode mode;
    std::string overrideToken;

    friend auto operator==(const GraceOverrideDeclaration&,
                           const GraceOverrideDeclaration&) noexcept -> bool = default;
};

//  One resource claim as the runtime records carry it.
struct ResourceClaimDeclaration final {
    std::string resourceId;
    ClaimPolicyDeclaration claimPolicy;
    ResourceClaimIntent intent;
    GraceOverrideDeclaration graceOverride;

    friend auto operator==(const ResourceClaimDeclaration&,
                           const ResourceClaimDeclaration&) noexcept -> bool = default;
};

//  The presence state of a measure component's grade table.
enum class GradePresence : std::uint8_t { absent, declared };

//  One compiled measure component: the phase, its derived fact category, the legacy grade-token
//  presence and the v3 index into `PreparedNormalizedScore::gradeTables`. The two grade carriers
//  are mutually exclusive.
struct CompiledMeasureComponent final {
    PhaseKind phase;
    FactCategory category;
    GradePresence grade;
    std::vector<std::string> declaredGradeTokens;
    std::optional<std::uint32_t> gradeTableIndex;

    friend auto operator==(const CompiledMeasureComponent&,
                           const CompiledMeasureComponent&) noexcept -> bool = default;
};

//  The executable automaton of one pattern, when the source carried one.
struct PatternExecutionProgram final {
    std::vector<std::string> atomRefs;
    std::size_t start;
    std::vector<std::uint8_t> accepting;
    std::vector<std::optional<std::size_t>> transitions;
    std::vector<std::uint8_t> live;
};

//  ---------------------------------------------------------------------------------------------
//  Requirement timing and prepared records
//  ---------------------------------------------------------------------------------------------

//  One phase target. Formerly `RequirementRecord::PhaseTarget`; the runtime copy is
//  field-identical.
struct PhaseTarget final {
    PhaseKind phase;
    Tick chartTick;
    friend auto operator==(const PhaseTarget&, const PhaseTarget&) noexcept -> bool = default;
};

//  One prepared success window. `[start, end)` and the phase instance it belongs to.
struct SuccessWindow final {
    Tick start;
    Tick end;
    PhaseDeclaration phase;
    friend auto operator==(const SuccessWindow&, const SuccessWindow&) noexcept -> bool = default;
};

//  The timing of one prepared requirement. Formerly `RequirementRecord::Timing`; `deadline` is
//  carried by `PreparedRequirement` itself.
struct RequirementTiming final {
    Tick end;
    std::vector<SuccessWindow> successWindows;
    std::optional<TimeInterval> body;
    std::vector<PhaseTarget> phaseTargets;
    friend auto operator==(const RequirementTiming&, const RequirementTiming&) noexcept
        -> bool = default;
};

//  One prepared requirement: everything the running kernel needs, and nothing a compiler needs.
struct PreparedRequirement final {
    StableDeclarationId stableId;
    RequirementIdentity identity;
    std::vector<PhaseDeclaration> phases;
    RequirementTiming timing;
    Tick deadline;
    std::optional<std::uint64_t> expandedPatternNodes;
    std::vector<CompiledMeasureComponent> measure;
    std::vector<AtomBinding> atomBindings;
    std::vector<ResourceClaimDeclaration> resourceClaims;
    std::optional<ClaimPolicyDeclaration::CompetitionKey> independentCompetition;
    std::optional<PatternExecutionProgram> executionProgram;
};

//  One prepared resource and the requirement instances that claim it, in claim order.
struct PreparedResource final {
    std::string resourceId;
    std::string slotToken;
    std::vector<std::uint32_t> requirementIndices;
    bool terminalAfterTermination;
};

//  The verified domain of one prepared run. The new beat-us mode fixes the rational at 1/1 and the
//  origin at 0; legacy sources carry their existing rational values.
struct PreparedTimebase final {
    std::string identity;
    std::int64_t microsecondsNumerator;
    std::int64_t microsecondsDenominator;
    Tick origin;
};

//  The borrowed, owning-buffer-free input view one prepared gameplay offers. The pointed values are
//  owned by the prepared storage and stay alive as long as the handle.
struct CompiledGameplayView final {
    const PreparedTimebase& timebase;
    const LatePolicyParameters& latePolicy;
    const PreparedIdentityDeclarations& identities;
    std::string_view contentIdentity, preparedIdentity, executionProfile;
    const PreparedRuleset* ruleset;                 // Legacy only; null for v3.
    const PreparedNormalizedScore* normalizedScore; // Beat-us v3 only; null for legacy.
    std::span<const PreparedRequirement> requirements;
    std::span<const PreparedResource> resources;
    std::span<const PreparedTimerKey> timers;
    //  The exact chart-projection bytes of the source content (the bytes `writeChartProjection`
    //  appends after the identity header). A compile-side producer fills it; the runtime identity
    //  generator concatenates it without knowing any author type.
    std::span<const std::byte> chartProjection{};
};

//  The budgets `prepareCompiledGameplay` applies. A pending dimension is not a number and is not
//  enforced; the producer checks its own pre-compile limits.
struct PrepareGameplayLimits final {
    MeasuredParameter<std::uint64_t> maxRequirements, maxResources, maxMeasureComponents;
    MeasuredParameter<std::uint64_t> maxExpandedPatternNodes, maxAutomatonStates;
};

//  ---------------------------------------------------------------------------------------------
//  Shared local-domain helper (the release dispatch -> observation envelope)
//  ---------------------------------------------------------------------------------------------

//  One half-open observation interval in tick units.
struct ObservationDomain final {
    std::int64_t start, end;
    friend auto operator==(const ObservationDomain&, const ObservationDomain&) noexcept
        -> bool = default;
};

//  The conservative observation envelope of one real release dispatch interval `[dispatchStart,
//  dispatchEnd)`: `[max(INT64_MIN, dispatchStart - hop), dispatchEnd)`. A `queueNextTick` policy
//  passes `hop = maxQueueHop`; `rejectLate` passes zero. The subtraction is checked and clamps at
//  `INT64_MIN`. Empty dispatch intervals have no observation domain.
[[nodiscard]] auto releaseObservationDomain(std::int64_t dispatchStart, std::int64_t dispatchEnd,
                                            std::int64_t hop) -> std::optional<ObservationDomain>;

//  The fixed full-mark basis of one score against the prepared requirement records: the per-unit
//  maximum judge points sum and the combo reward sum. Units are the ones named by
//  `score.units`, in the declaration order of the requirements and their phases. A unit whose
//  members carry no local qualified key is rejected with reason `score_unit_unqualified`; a local
//  key without an exact rule is rejected with reason `score_rule_missing`.
[[nodiscard]] auto deriveScoreBasis(std::span<const PreparedRequirement> requirements,
                                    const LatePolicyParameters& policy,
                                    const PreparedNormalizedScore& score)
    -> core::Result<ScoreBasis>;

} // namespace cuexis::judgement::runtime
