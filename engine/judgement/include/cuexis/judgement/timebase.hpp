#pragma once

//  Judgement typed kernel - S7A-2 time domain, TimebaseProfile and exact beat mapping.
//
//  Authority for everything below is the Gameplay V2 Spec section 3.7 (round 2 ruling, 2026-10-03)
//  together with the Gameplay V2 ABI domain 1 (timebase). Frozen by this batch:
//
//    * ChartTick, judgementTick, observationTick and commitTick are four distinct signed 64-bit
//      integer domains (Spec 3.7.1 item 1). They are distinct types on purpose: the Spec forbids
//      collapsing the music Beat, the session physical time and the render frame time into one
//      unnamed Tick, and a distinct type makes the mixing unrepresentable rather than merely
//      forbidden by a comment.
//    * The unit is declared by the TimebaseProfile, never by a type name, an enumerator name or a
//      conversion function name (Spec 3.7.1 item 2). This header therefore contains no "seconds to
//      ticks" helper, no implicit zero value and no default unit: the profile token is a required,
//      explicitly spelled value and the tick scale is an explicit rational parameter.
//    * Tick arithmetic, displacement and difference reject overflow stably instead of saturating,
//      truncating or reaching undefined behaviour (Spec 3.7.1 item 4).
//    * RationalBeat -> judgementTick is an exact rational mapping performed at prepare time with
//      round-half-to-even and negative symmetry (Spec 3.7.2 items 1 and 2). No floating point and
//      no host math library take part. Tempo, stop and negative beats share one rule with no
//      exception path (Spec 3.7.2 item 3): a tempo change only changes the beat -> tick ratio, and
//      a stop freezes the mapping over [startBeat, endBeat) and adds its declared duration once, at
//      endBeat. A stop duration is never spread over the interval as an internal rate, the interval
//      may therefore map many beats onto one tick, and the rounding step still happens once.
//    * Same-tick collisions are ordered by the canonical key (tick, originKind, canonicalOrdinal);
//      ingress arrival order, container order and thread completion order are never an input to the
//      comparison (Spec 3.7.2 item 4).
//    * commitTick is the tick at which a transaction commit happens and a commit window is the
//      observation interval between two adjacent committable ticks; facts may only be committed at
//      a commitTick (Spec 0.1 and 3.7.5).
//    * Late policy parameters are typed parameters supplied by the TimebaseProfile or the ruleset.
//      Their numeric values are not frozen by this batch, so this header models "not measured" as a
//      first-class typed state instead of a constant, an implicit zero or a research slice limit
//      (Spec 3.7.4 items 1 and 2). A profile whose late policy is not measured rejects at prepare
//      with the late_policy_incomplete category (Spec 3.7.4 item 3).
//
//  Every operation that can leave the representable range, or that is handed a declaration it
//  cannot honour, reports that as a core::Result failure with a stable diagnostic. No exception
//  crosses this boundary, and no function that builds a diagnostic is noexcept: building the
//  diagnostic allocates its owning text.
//
//  Deliberately absent from this batch: serialization bytes and any wire format (still S1-05), the
//  final naming of every field, the S7C-1 CalibrationProfile extension fields, and all budget
//  numbers. The mapping's code shape is owned here because Spec 3.7.2 and ABI S7A-2 place it in the
//  S7A-2 implementation batch.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/diagnostic.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Tick domains (Spec 0.1, Spec 3.7.1, ABI domain 1)
//  ---------------------------------------------------------------------------------------------
//
//  One distinct type per time domain. Each carries exactly one signed 64-bit integer, which is the
//  width the round 2 ruling froze; the unit is deliberately not part of the representation and can
//  only be read from a TimebaseProfile. The wrapper makes "which domain is this" part of the type
//  instead of part of a comment, so a ChartTick cannot silently be used where a judgementTick is
//  required.
//
//  A default-constructed wrapper is the declared zero value of its own domain (tick 0), not an
//  "unspecified" placeholder and not a unit default: there is no conversion from it to any other
//  domain and no arithmetic on it that a TimebaseProfile did not authorise.

//  Signed 64-bit logical time value. Generic carrier for arithmetic; not a domain of its own.
class Tick final {
  public:
    constexpr Tick() noexcept = default;
    explicit constexpr Tick(std::int64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::int64_t {
        return value_;
    }

    friend constexpr auto operator==(Tick, Tick) noexcept -> bool = default;
    friend constexpr auto operator<=>(Tick, Tick) noexcept = default;

  private:
    std::int64_t value_{0};
};

//  Chart anchors, requirement windows and coordination windows (Spec 0.1).
class ChartTick final {
  public:
    constexpr ChartTick() noexcept = default;
    explicit constexpr ChartTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(ChartTick, ChartTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(ChartTick, ChartTick) noexcept = default;

  private:
    Tick value_{};
};

//  Prepare-time mapped, run-time read-only judgement time domain (Spec 0.1).
class JudgementTick final {
  public:
    constexpr JudgementTick() noexcept = default;
    explicit constexpr JudgementTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(JudgementTick, JudgementTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(JudgementTick, JudgementTick) noexcept = default;

  private:
    Tick value_{};
};

//  The canonical judgement time of one observation. Its only canonical source is the calibrated
//  session clock captured when input enters the judgement pipeline; device time, host arrival time,
//  audio time and render frame time stay diagnostic context (Spec 3.7.3).
class ObservationTick final {
  public:
    constexpr ObservationTick() noexcept = default;
    explicit constexpr ObservationTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(ObservationTick, ObservationTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(ObservationTick, ObservationTick) noexcept = default;

  private:
    Tick value_{};
};

//  The tick at which a transaction commits. Facts commit here and nowhere else (Spec 3.7.5).
class CommitTick final {
  public:
    constexpr CommitTick() noexcept = default;
    explicit constexpr CommitTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(CommitTick, CommitTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(CommitTick, CommitTick) noexcept = default;

  private:
    Tick value_{};
};

//  Half-open [start, end) extent over the time domain.
struct TimeInterval final {
    Tick start{};
    Tick end{};

    friend constexpr auto operator==(TimeInterval, TimeInterval) noexcept -> bool = default;
};

//  Tick displacement and signed tick difference. Both are signed 64-bit, and both the displacement
//  and the difference reject on overflow instead of saturating or truncating (Spec 3.7.1 item 4).
class TickSpan final {
  public:
    constexpr TickSpan() noexcept = default;
    explicit constexpr TickSpan(std::int64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::int64_t {
        return value_;
    }

    friend constexpr auto operator==(TickSpan, TickSpan) noexcept -> bool = default;

  private:
    std::int64_t value_{0};
};

class TickDelta final {
  public:
    constexpr TickDelta() noexcept = default;
    explicit constexpr TickDelta(std::int64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::int64_t {
        return value_;
    }

    friend constexpr auto operator==(TickDelta, TickDelta) noexcept -> bool = default;

  private:
    std::int64_t value_{0};
};

//  Tick arithmetic entry points. Every one of them returns Result because the width the round 2
//  ruling froze is a bounded width: an operation that leaves the representable range is a stable
//  rejection, not a wrapped or clamped value.
//
//  None of them is noexcept. A rejection carries an owning diagnostic (a code, a category, a
//  summary and context tokens), and building that text allocates; a noexcept signature would turn
//  an allocation failure into std::terminate at the module boundary.

//  origin + span, rejecting when the sum leaves the signed 64-bit range.
[[nodiscard]] auto offsetTicks(Tick origin, TickSpan span) -> core::Result<Tick>;
//  left - right, rejecting when the difference leaves the signed 64-bit range. Both operands may
//  hold any signed 64-bit value: the subtraction is checked, so INT64_MIN - 0 is the representable
//  result INT64_MIN and only a true out-of-range difference is rejected. The magnitude of a
//  difference has no representation of its own, so a caller that needs |delta| has to narrow it
//  itself and handle the INT64_MIN case; no helper here hides that.
[[nodiscard]] auto differenceTicks(Tick left, Tick right) -> core::Result<TickDelta>;
//  The half-open interval between two ticks, rejecting when end precedes start.
[[nodiscard]] auto makeInterval(Tick start, Tick end) -> core::Result<TimeInterval>;

//  ---------------------------------------------------------------------------------------------
//  Exact rational values (Spec 3.7.2 item 1)
//  ---------------------------------------------------------------------------------------------
//
//  An author beat and a tempo are exact rational values. These classes store one signed rational in
//  lowest terms and perform only integer arithmetic: no float, no double and no host math library
//  take part, which is exactly what Spec 3.7.2 item 1 requires of the mapping and what the S7A-2
//  forbidden-consumption list repeats ("floating point or a host math library taking part in Tick
//  decisions" is forbidden).
//
//  The denominator is always strictly positive and the pair is always coprime, so the sign lives
//  entirely in the numerator and comparison, rounding and negation are symmetric by construction.
//  Coprimality also makes value equality a memberwise comparison instead of a cross product that
//  could itself overflow.
//
//  Construction is checked, so the two-argument constructor is private and the only way to build a
//  rational is the named factory. There is deliberately no default constructor: a default-
//  constructed rational would be the value 0/1, and the S7A-2 ruling forbids 0/1 from becoming an
//  implicit declaration ("no implicit zero value and no default unit"). A caller that really wants
//  the zero beat has to spell `RationalBeat::create(0, 1)` and handle the Result, so the zero is a
//  declaration at the call site instead of a value the type hands out on its own.
//
//  A zero denominator, an INT64_MIN denominator and an INT64_MIN numerator are all rejected: the
//  last one because |INT64_MIN| is not representable in the numerator type.

class RationalBeat final {
  public:
    //  The only way to build a checked rational beat. Rejects a zero denominator, an INT64_MIN
    //  operand and a numerator whose magnitude cannot be reduced into the signed 64-bit range, with
    //  the budget_exceeded category instead of producing an unrepresentable value.
    [[nodiscard]] static auto create(std::int64_t numerator, std::int64_t denominator)
        -> core::Result<RationalBeat>;

    [[nodiscard]] constexpr auto numerator() const noexcept -> std::int64_t {
        return numerator_;
    }
    [[nodiscard]] constexpr auto denominator() const noexcept -> std::int64_t {
        return denominator_;
    }

    [[nodiscard]] constexpr auto isZero() const noexcept -> bool {
        return numerator_ == 0;
    }

    friend constexpr auto operator==(RationalBeat, RationalBeat) noexcept -> bool = default;

  private:
    constexpr RationalBeat(std::int64_t numerator, std::int64_t denominator) noexcept
        : numerator_(numerator), denominator_(denominator) {}

    std::int64_t numerator_;
    std::int64_t denominator_;
};

//  A tempo is an exact rational duration per beat, expressed in the tick-scale numerator unit of
//  the profile that consumes it. It is a rational, not a fixed-point value, because the mapping has
//  to stay exact until the final rounding step.
//
//  Like RationalBeat it has no default constructor: a zero duration is never a usable tempo, and
//  the ruling keeps "this profile never declared a tempo" in the declared state of the profile
//  instead of in the value of the duration. `RationalDuration::create(0, 1)` is available where a
//  zero really is being declared, and validatePrepare rejects it as a non-positive tempo.
class RationalDuration final {
  public:
    //  Same checked contract as RationalBeat::create.
    [[nodiscard]] static auto create(std::int64_t numerator, std::int64_t denominator)
        -> core::Result<RationalDuration>;

    [[nodiscard]] constexpr auto numerator() const noexcept -> std::int64_t {
        return numerator_;
    }
    [[nodiscard]] constexpr auto denominator() const noexcept -> std::int64_t {
        return denominator_;
    }

    friend constexpr auto operator==(RationalDuration, RationalDuration) noexcept -> bool = default;

  private:
    constexpr RationalDuration(std::int64_t numerator, std::int64_t denominator) noexcept
        : numerator_(numerator), denominator_(denominator) {}

    std::int64_t numerator_;
    std::int64_t denominator_;
};

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

//  ---------------------------------------------------------------------------------------------
//  RationalBeat -> judgementTick (Spec 3.7.2)
//  ---------------------------------------------------------------------------------------------

//  Maps an author beat to a judgement tick with exact rational arithmetic, rounding to the nearest
//  integer with ties going to the even side. The rounding rule is symmetric for negative values;
//  the mapping as a whole is not necessarily odd-symmetric about the origin once stops exist (see
//  below). Beats before the profile origin produce negative ticks. Tempo, stop and negative beats
//  all travel through this one function, so there is no exception path to keep in sync.
//
//  The mapped value is the exact signed accumulated tick time from `originBeat` to `beat`. Outside
//  a stop interval the rate is the tempo in effect at that beat; inside a stop interval the rate is
//  zero and the stop's whole declared duration is added once, at the stop's endBeat. The value is
//  therefore an exact rational, and the round-half-to-even step is applied once, at the end. In
//  other words F(originBeat) = 0 and F(endBeat) = F(startBeat) + duration in the exact domain, and
//  tick(beat) = roundHalfToEven(F(beat)); this is deliberately not the integer recurrence
//  tick(endBeat) = tick(startBeat) + duration, which would round twice.
//  Negative beats accumulate in the opposite direction over (beat, origin], so every stop whose
//  endBeat falls inside that interval contributes before the sign is applied. A mirrored stop
//  configuration can therefore lose odd symmetry, because the half-open jump direction cannot be
//  mirrored. Monotonicity and the directed endpoint relation hold on both sides.
//
//  A stop makes the mapping non-injective: several beats inside [startBeat, endBeat) carry the same
//  tick. This is a property of the frozen model, not an accident, and callers that need one beat
//  per tick have to resolve the tie outside this function.
//
//  The profile is re-validated here, so a call on a profile that validatePrepare would reject (or
//  on one that was never validated) is a stable rejection rather than undefined behaviour. Every
//  value that does not fit in the intermediate signed 64-bit rational representation is a stable
//  rejection too: this mapping never approximates and never saturates.
[[nodiscard]] auto mapBeatToTick(const TimebaseProfile& profile, RationalBeat beat)
    -> core::Result<JudgementTick>;

//  ---------------------------------------------------------------------------------------------
//  Same-tick tie rule (Spec 3.7.2 item 4)
//  ---------------------------------------------------------------------------------------------

//  The canonical origin kind of a candidate, a signal or a fact. The order of this enumeration is
//  the canonical originKind priority: it is a declaration of the specification and not an
//  implementation enumeration borrowed from a module registration order.
enum class OriginKind : std::uint8_t {
    observation = 0,
    timer = 1,
    coordination = 2,
    correction = 3,
};

//  The canonical ordinal of one item within its tick. It is derived from the canonical window or
//  observation identity, never from the ingress arrival order, the container order or the thread
//  completion order (Spec 0.1 canonical ordinal, Spec 3.7.2 item 4).
class CanonicalOrdinal final {
  public:
    constexpr CanonicalOrdinal() noexcept = default;
    explicit constexpr CanonicalOrdinal(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t {
        return value_;
    }

    friend constexpr auto operator==(CanonicalOrdinal, CanonicalOrdinal) noexcept -> bool = default;
    friend constexpr auto operator<=>(CanonicalOrdinal, CanonicalOrdinal) noexcept = default;

  private:
    std::uint64_t value_{0};
};

//  The canonical sort key of Spec 3.7.2 item 4. Deliberately has no ingress field: a key that
//  could carry an arrival ordinal would let a caller reintroduce the ordering the ruling forbids.
//
//  All three components are constructor arguments and no member has a default value, so a key
//  cannot be built without naming its origin kind. A defaulted member would make `OriginKind{}`,
//  that is `observation`, the kind of every key that forgot to say, which is exactly the implicit
//  default enumeration value the S7A-2 ruling forbids; omitting an argument is now a compile error
//  instead of a silent canonical choice. The members stay public for reading, but construction goes
//  through the constructor.
struct TickCollisionKey final {
    Tick tick;
    OriginKind originKind;
    CanonicalOrdinal canonicalOrdinal;

    constexpr TickCollisionKey(Tick tick, OriginKind originKind,
                               CanonicalOrdinal canonicalOrdinal) noexcept
        : tick(tick), originKind(originKind), canonicalOrdinal(canonicalOrdinal) {}

    friend auto operator==(const TickCollisionKey&, const TickCollisionKey&) noexcept
        -> bool = default;
};

//  Canonical same-tick ordering: tick, then originKind priority, then canonicalOrdinal. Being a
//  strict weak order over the canonical key only, it is invariant under any permutation of the
//  input sequence, which is the property the "no ingress ordering" rule is protecting. It is a
//  pure comparison and allocates nothing, so it is noexcept.
[[nodiscard]] auto canonicalOrder(const TickCollisionKey& left,
                                  const TickCollisionKey& right) noexcept -> bool;

//  ---------------------------------------------------------------------------------------------
//  commitTick and the commit window (Spec 0.1, Spec 3.7.5)
//  ---------------------------------------------------------------------------------------------

//  A commit window is the observation interval between two adjacent committable ticks. Its
//  boundaries are themselves committable ticks and its commitTick is the tick at which the facts
//  observed in the window commit.
//
//  Well-formedness (checked by commitAt): the commitTick lies within the closed interval
//  [openTick, closeTick], which is what makes the window's boundaries a valid observation interval
//  and its commit tick reachable. Facts are committed at the window's commitTick and nowhere else
//  inside the interval.
struct CommitWindow final {
    Tick openTick{};
    CommitTick commitTick{};
    Tick closeTick{};

    friend auto operator==(const CommitWindow&, const CommitWindow&) noexcept -> bool = default;
};

//  Rejects a fact commit attempted anywhere other than the window's commitTick, and rejects a
//  window whose own boundaries contradict its commitTick. The stable rejection is returned instead
//  of silently moving the commit to the next committable tick, because moving it would fabricate a
//  commit that the caller did not declare.
[[nodiscard]] auto commitAt(Tick tick, const CommitWindow& window) -> core::Result<void>;

} // namespace cuexis::judgement
