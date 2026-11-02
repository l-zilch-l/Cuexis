#pragma once

//  Judgement typed kernel - tempo and stop sections, late policy and TimebaseProfile.

#include "timebase_rational.hpp"
#include "timebase_tick.hpp"

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/diagnostic.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  TimebaseProfile (Spec 3.7.1, ABI domain 1 TimebaseProfileRef)
//  ---------------------------------------------------------------------------------------------

//  One tempo section of the chart. `startBeat` is the author beat at which the section becomes
//  effective; the section before the first declaration is the profile's initial tempo. A tempo
//  change only changes the beat -> tick ratio; it does not introduce a second rounding rule
//  (Spec 3.7.2 item 3).
//
//  Both fields are required members with no zero default: a tempo section is a declaration, and a
//  section that omits its start beat or its tempo would be an implicit 0/1 declaration.
struct TempoSection final {
    RationalBeat startBeat;
    //  Duration of one beat under this tempo, in the tick-scale numerator unit. Must be positive.
    RationalDuration durationPerBeat;

    friend auto operator==(const TempoSection&, const TempoSection&) noexcept -> bool = default;
};

//  A stop freezes beat progress over a non-degenerate beat interval. Over [startBeat, endBeat) the
//  mapping does not advance at all: every beat of the interval maps to the same tick,
//  tick(startBeat). The declared duration is added once, at endBeat, so the exact accumulated
//  tick time satisfies tick(endBeat) = tick(startBeat) + duration while the internal rate of the
//  interval is zero (Spec 3.7.2 item 3, round 2 ruling).
//
//  The stop therefore never contributes an internal rate: its duration is not spread over the
//  interval, and the beat -> tick mapping is deliberately non-injective inside it. Beats that
//  collapse onto one tick are ordered by the canonical same-tick key, not by their beat order.
//  Everything else travels through the same exact integral and the same round-half-to-even step, so
//  a stop is not a second rounding path.
//
//  A degenerate interval (startBeat >= endBeat) is rejected by validatePrepare rather than given an
//  invented instantaneous meaning, and two stop intervals may not overlap.
struct StopSection final {
    RationalBeat startBeat;
    RationalBeat endBeat;
    //  Frozen duration of the interval, in the tick-scale numerator unit. Must be positive.
    RationalDuration duration;

    friend auto operator==(const StopSection&, const StopSection&) noexcept -> bool = default;
};

//  How a window is finalized with respect to a late observation. The set of enumerators is the
//  ABI domain 1 set (reject_late / queue_next_tick); it declares no default value, because a
//  default policy would be exactly the implicit default the ruling forbids, and every declaration
//  site below therefore has to name the enumerator it means.
enum class LateEventPolicy : std::uint8_t {
    rejectLate,
    queueNextTick,
};

//  A typed late-policy parameter (Spec 3.7.4 items 1 and 2).
//
//  "Not measured" is a first-class state of the parameter, not a numeric default. The wrapper has
//  no implicit conversion to the value type, no zero default and no sentinel number, so a consumer
//  cannot accidentally read a placeholder as a measurement: it has to ask the tag first. This is
//  how the ruling's `pending_measurement` registration is represented without becoming a constant.
template <typename Value> class MeasuredParameter final {
  public:
    //  The only way to build an unmeasured parameter: the state is named at the call site.
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

    //  Returns nullopt while the parameter is pending measurement. There is deliberately no
    //  "value or default" accessor.
    [[nodiscard]] constexpr auto measuredValue() const noexcept -> const Value* {
        return value_.has_value() ? &value_.value() : nullptr;
    }

    friend constexpr auto operator==(MeasuredParameter left, MeasuredParameter right) noexcept
        -> bool = default;

  private:
    std::optional<Value> value_{};
};

//  The four typed late-policy parameters of Spec 3.7.4 item 1 plus the session-level policy
//  selection. Every field is a parameter, not a number: the numeric values are a later-batch
//  blocking item, so a profile that supplies a number where the registry expects
//  `pending_measurement` is a caller error, and a profile that leaves a parameter unmeasured is
//  rejected at prepare.
//
//  No field has a default value. In particular the policy selection is an optional with no
//  default enumerator: "which late policy does this session use" is a declaration the ruleset has
//  to make, and a session that never made it rejects at prepare with the late_policy_incomplete
//  category. A defaulted enumerator here would be exactly the implicit default the S7A-2 ruling
//  forbids.
struct LatePolicyParameters final {
    MeasuredParameter<TickSpan> finalizationWatermark =
        MeasuredParameter<TickSpan>::pendingMeasurement();
    MeasuredParameter<TickSpan> maxQueueHop = MeasuredParameter<TickSpan>::pendingMeasurement();
    MeasuredParameter<TickSpan> windowCloseThreshold =
        MeasuredParameter<TickSpan>::pendingMeasurement();
    MeasuredParameter<TickSpan> windowOpenThreshold =
        MeasuredParameter<TickSpan>::pendingMeasurement();
    //  The session-level late event policy. Absent until a ruleset declares it.
    std::optional<LateEventPolicy> policy{};

    friend auto operator==(const LatePolicyParameters&, const LatePolicyParameters&) noexcept
        -> bool = default;
};

//  The fixed timebase binding of one session (Spec 3.7.1, ABI domain 1).
//
//  `tickScale` is the exact rational size of one tick expressed in the declared unit's numerator
//  sub-unit. Stage 7A recommends `engine.tick.us.v1`, which declares the unit `us` (microseconds)
//  with `tickScale = 1/1`: one tick is exactly one microsecond. That is a unit declaration, not a
//  numeric limit: it does not cap any range, it is not a research slice value, and it is spelled
//  explicitly by the profile instead of being baked into a type name.
//
//  The session-level late event policy selection deliberately does not live here as well: one
//  session has one policy, and repeating the selection on the profile would create a second source
//  of truth for it. It lives on LatePolicyParameters, which the ruleset supplies.
//
//  `tickScale`, `originBeat` and `initialTempo` carry no zero default. A rational has no default
//  constructor at all, so a profile cannot be default-constructed into an all-zero declaration; the
//  only way to build one is to name every field, which is what makeMicrosecondTimebase does.
struct TimebaseProfile final {
    //  Stable profile identity, for example "engine.tick.us.v1". Required and explicit: there is no
    //  default profile identity, because a default unit is an implicit unit.
    std::string_view profileId;
    //  The declared unit token. It is a label of the declaration and never a conversion function
    //  name, so a caller cannot read "us" and convert seconds behind the profile's back.
    std::string_view unitToken;
    //  Exact size of one tick in the unit's numerator sub-unit. Must be positive.
    RationalDuration tickScale;
    //  Author beat that maps to judgementTick 0.
    RationalBeat originBeat;
    //  Initial (pre-first-declaration) tempo, in the tick-scale numerator unit per beat. An absent
    //  optional is the explicit "this profile never declared a tempo" state; a declared value must
    //  be positive. Both states are prepare failures, with distinguishable diagnostics: an absent
    //  tempo is a structurally incomplete declaration, a declared non-positive one is a value
    //  outside the domain its declaration requires.
    std::optional<RationalDuration> initialTempo{};
    //  Tempo sections ordered by startBeat. The mapper rejects an unordered or duplicate section
    //  rather than picking one, because picking one would be a container-order decision.
    std::vector<TempoSection> tempoSections;
    //  Stop sections ordered by startBeat, same rejection rule, and additionally non-degenerate and
    //  non-overlapping.
    std::vector<StopSection> stopSections;
};

//  The declared Stage 7A unit binding, built explicitly. It sets the profile identity
//  (`engine.tick.us.v1`), the unit token (`us`), the tick scale (1/1, one tick is one microsecond)
//  and the origin beat (0/1) at the construction point, so each of them is a named declaration
//  rather than a zero the type supplied.
//
//  It deliberately does not declare a tempo: `initialTempo` is set to the explicit absent state.
//  A tempo is chart content expressed in microseconds per beat, and every candidate number would be
//  an invented default that the S7A-2 ruling forbids, so validatePrepare rejects this declaration
//  until the chart supplies one. No default unit and no implicit conversion exist: a caller that
//  wants microseconds-per-tick has to say so through this declaration.
[[nodiscard]] auto makeMicrosecondTimebase() -> TimebaseProfile;

//  Validates the declared profile and the late-policy parameters before any mapping happens.
//
//  Everything the mapper depends on is checked here rather than discovered later:
//    * the unit and the profile identity are explicitly declared (no default unit);
//    * the profile explicitly declares an initial tempo, and the tick scale and every declared
//      duration are positive and exact. A missing initial tempo is a structurally incomplete
//      declaration and is rejected with the invalid_relation category; a declared non-positive
//      value is rejected with budget_exceeded, because it left the domain its declaration requires;
//    * the tempo sections are strictly ordered by start beat and the stop sections are strictly
//      ordered, non-degenerate and non-overlapping;
//    * every late-policy parameter is measured and the policy selection is declared. A pending
//      measurement or an undeclared policy is a stable rejection with the late_policy_incomplete
//      category, which is the prepare atomic-failure condition of Spec 9.3.
//
//  The magnitude relations between the late-policy parameters are deliberately not checked: their
//  numbers are a later-batch blocking item, so imposing an ordering between them here would freeze
//  semantics this batch does not own. Only presence is checked.
[[nodiscard]] auto validatePrepare(const TimebaseProfile& profile,
                                   const LatePolicyParameters& latePolicy) -> core::Result<void>;

} // namespace cuexis::judgement
