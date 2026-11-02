#pragma once

//  Judgement typed kernel - input-layer boundary partition.
//
//  The Stage 7A ruling S1-04 keeps the input layer inside this module as a separate header
//  partition instead of splitting a cuexis_input target. This partition declares the input-side
//  roles; the judgement-side roles are declared in <cuexis/judgement/role_boundary.hpp> and the
//  time roles in <cuexis/judgement/timebase.hpp>.
//
//  The file has two halves with different freezing states.
//
//  1. The role inventory (namespace roles). Every declaration there is an incomplete type on
//     purpose. S7A-1 freezes existence, role and the 7A rejection path only for those names, and
//     promises no member, no integer width, no enumeration value, no default value, no layout and
//     no serialization byte for them:
//
//       InputDomain enumeration set            round 6, P2-05
//       Tick storage width, conversion rule
//       and same-tick tie rule                 round 2, CM-T03 / CM-T04 (frozen in timebase.hpp)
//       TickSpan / TickDelta                   frozen in timebase.hpp
//       DomainAmount range                     Spec 3.7.6, this batch
//
//     The incomplete names stay incomplete after S7A-2: the concrete types this batch freezes are
//     declared below in the cuexis::judgement namespace, and the two spellings are deliberately
//     kept apart so no later reader can mistake a role placeholder for a live representation. The
//     compile-time half of that rule is asserted in tests/judgement/judgement_boundary_tests.cpp.
//
//  2. The frozen input normalization boundary (this batch, S7A-2). Authority is the Gameplay V2
//     Spec section 3.7 (round 2 ruling, 2026-10-03) plus ABI domain 2 and the contract matrix
//     entries CM-T05 / CM-T09 / CM-T10 / CM-T13. Frozen here:
//
//       * every InputDomain declaration must carry a typed AmountSpec that declares the canonical
//         integer quantization, the scale, the representable range and the boundary policy
//         (Spec 3.7.6 item 1). The spec is a required member, not an optional one, so a domain that
//         has no declared quantization is unrepresentable rather than merely discouraged.
//       * quantization uses exact integer arithmetic and round-half-to-even with negative symmetry
//         (Spec 3.7.6 item 2, the same rule as the Tick side in Spec 3.7.2 item 2). A value outside
//         the declared range, a value whose canonical integer is not representable, and an
//         unrepresentable exact quotient are stable rejections with no saturation and no truncation
//         (Spec 3.7.6 item 3).
//       * the InputMapping profile belongs to the session and its source identity, version and
//         quantization enter the session judgement identity (CM-T09, Spec 5.2). The quantization is
//         the declared AmountSpec of every domain declaration, so the whole domain declaration set
//         is a session identity component. The mapping does not change the Canonical Gameplay
//         Graph, and a runtime mapping mutation is a stable rejection.
//       * observationTick is captured once, at the entry of the judgement pipeline, from the
//         calibrated session clock. Device time, host arrival time, audio time and render frame
//         time travel beside it as diagnostic context only and are not an input to the tick
//         (Spec 3.7.3). The entry API takes the calibrated session clock and the raw timestamps as
//         two separate arguments; there is no conversion from the raw timestamp to a tick.
//       * duplicate queue admission is rejected on the canonical observation identity, never on the
//         ingress ordinal or the arrival order (Spec 3.7.4 item 4).
//       * continuous input capability - continuous trajectories, a minimum report rate,
//         reconstruction and discontinuity representation - is a stable rejection in 7A, pointing
//         at S7B-1 (Spec 3.7.7, plan S7A-2 item 5, ABI R-05).
//
//  Deliberately NOT frozen here, and therefore not representable:
//
//    * the InputDomain enumeration set (round 6, P2-05). A domain is declared by an explicit token
//      and its AmountSpec; no enumerator is invented for it.
//    * any business quantity, range bound, default boundary policy or budget number (Spec 3.7.6
//      item 4, Spec 3.7.7). Every number below is supplied by the declaration site.
//    * the byte composition of the canonical observation identity and the derivation of
//      canonicalOrdinal. This batch freezes the criterion (canonical fields only, never the ingress
//      ordinal); the byte contract belongs to the batch that first consumes it (S7A-4).
//    * the normalization form of contact; ContactRef stays an incomplete role until its own batch.
//    * serialization of any kind (S1-05).
//
//  Every failure path below returns core::Result and no function that builds a diagnostic is
//  noexcept: building the owning diagnostic text allocates.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/timebase.hpp>

#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "input_boundary_amount.hpp"
#include "input_boundary_mapping.hpp"

namespace cuexis::judgement::roles {

//  Declared and never defined: no member, no width, no enumeration value, no default value, no
//  layout. Each declaration fixes that the role exists and is distinct; nothing else.
class Tick;
class TimeInterval;
class TickSpan;
class TickDelta;
class NormalizedObservation;
class InputDomain;
class InputAction;
class ChannelRef;
class DomainAmount;
class SourceClass;
class EventSequence;

} // namespace cuexis::judgement::roles

namespace cuexis::judgement {

class SessionIngressState;
struct InputMappingProfile;
struct ClockedIngress;
namespace detail {
struct OwnedIngressSubject final {
    std::string domain;
    std::string channel;
    std::string source;
};
struct IngressJournal;
class ExecutionKernel;
auto prepareIngressBatch(const SessionIngressState&, const InputMappingProfile&,
                         std::span<const ClockedIngress>, bool,
                         std::span<const std::optional<std::int64_t>>)
    -> core::Result<IngressJournal>;
auto reserveIngressBatch(SessionIngressState&, const IngressJournal&) -> core::Result<void>;
void commitIngressBatch(SessionIngressState&, IngressJournal&&) noexcept;
} // namespace detail

//  ---------------------------------------------------------------------------------------------
//  NormalizedObservation (Spec 3.7.3, CM-T05, CM-T10)
//  ---------------------------------------------------------------------------------------------

//  A normalized amount attached to an observation: the quantized canonical integer, the domain
//  token it belongs to, and the specification that produced it. The specification is borrowed from
//  the session's mapping profile, the same non-owning prepared-view convention the ABI fixes.
struct NormalizedAmount final {
    QuantizedAmount value;
    std::string_view domainToken;
};

//  The canonical normalization of one discrete input edge.
//
//  This is the field-role boundary of CM-T05 and it is deliberately not the final field table:
//
//    observationId        derived by the session; a session counter, not a canonical fact
//    observationTick      captured once at the entry from the calibrated session clock (Spec 3.7.3)
//    ingressSequence      supplied by the host adapter before entry (CM-T06); not an identity input
//    domain               declared by the mapping profile
//    action               the 7A discrete verb; an edge, not a continuous sample
//    channel              a declared logical channel, never a device scan code
//    amount               the quantized domain amount, present only when the observation declares
//    one sourceClass          the device class, without serial numbers
//
//  The `contact` field of CM-T05 is absent on purpose: the optional long-lived contact handle is
//  frozen for a later batch (Q-11 / CM-C09, first consumed by S7A-4), so contact stays an
//  incomplete role and no field is invented for it here. The `discontinuity` field of CM-T05 is
//  also absent: crossing a sampling hole is a stable rejection at the entry (Spec 3.7.7), so no
//  observation can carry it.
//
//  No raw timestamp is a member. Device time, host arrival time, audio time and render frame time
//  are diagnostic context, and they are not merely documented as such: the only entry API takes
//  them as a separate argument whose type has no route into this structure.
struct NormalizedObservation final {
    ObservationId observationId;
    ObservationTick observationTick;
    IngressSequence ingressSequence;
    std::string_view domainToken;
    InputAction action;
    ChannelRef channel;
    SourceClass sourceClass;
    //  Absent when the incoming event declared no amount. It is an optional member and not a zero
    //  amount, because a zero integer would be a fabricated measurement.
    std::optional<NormalizedAmount> amount;
};

//  The canonical identity of one observation, as far as this batch freezes it.
//
//  It carries the canonical fields that identify the observation and no ordinal of any kind: there
//  is no ingressSequence member and no arrival index member, so the two orderings the rulings
//  forbid from deciding duplication are not merely ignored by the comparison, they are
//  unrepresentable in it.
//
//  The byte composition and the derivation of canonicalOrdinal from this identity belong to the
//  batch that first consumes them (S7A-4), so this structure fixes the criterion and not the bytes.
struct CanonicalIngressSubject final {
    ObservationTick observationTick;
    std::string_view domainToken;
    InputAction action;
    ChannelRef channel;
    SourceClass sourceClass;
    bool hasAmount;
    std::int64_t amountCanonicalInteger;

    //  Spelled out rather than defaulted: a defaulted comparison would have to order the bool
    //  member, and the criterion here is equality of the canonical fields.
    friend constexpr auto operator==(const CanonicalIngressSubject& left,
                                     const CanonicalIngressSubject& right) noexcept -> bool {
        return left.observationTick == right.observationTick &&
               left.domainToken == right.domainToken && left.action == right.action &&
               left.channel == right.channel && left.sourceClass == right.sourceClass &&
               left.hasAmount == right.hasAmount &&
               left.amountCanonicalInteger == right.amountCanonicalInteger;
    }
};

//  One normalized entry: the canonical subject the entry is identified by and the observation
//  itself. The subject is derived by deriveCanonicalIngressSubject, never supplied by a caller.
struct NormalizedObservationEntry final {
    CanonicalIngressSubject subject;
    NormalizedObservation observation;
};

//  ---------------------------------------------------------------------------------------------
//  The entry boundary: calibrated session clock in, raw timestamps beside it (Spec 3.7.3)
//  ---------------------------------------------------------------------------------------------

//  The raw timestamps of one arriving input event. Every field is diagnostic context: none of them
//  is an input to observationTick, and the type deliberately has no conversion, no accessor and no
//  arithmetic that could produce one. `rawTicks` is a raw device or host count whose unit belongs
//  to the device, which is exactly why it must not be mistaken for a tick.
struct RawIngressTimestamps final {
    std::int64_t rawTicks;
    std::int64_t hostArrivalTicks;
    std::int64_t audioFrameTicks;
    std::int64_t renderFrameTicks;
};

//  The discontinuity state of the arriving event. Stage 7A can express only "no crossing": a
//  discontinuity, a reconnect or a dropped sample is a stable rejection at the entry (Spec 3.7.7),
//  because its representation belongs to S7B-1. The field exists so the incoming declaration has a
//  typed place to state what it observed; it is not a permission.
struct DiscontinuityDeclaration final {
    bool crossedSamplingGap;
    bool reconnected;
    bool droppedSamples;
};

//  The continuity capability an event declares. Stage 7A opens the discrete press / release /
//  update path only; a continuous trajectory, a minimum report rate or a reconstruction request is
//  a stable rejection that points at S7B-1 (Spec 3.7.7, plan S7A-2 item 5).
enum class ContinuityKind : std::uint8_t {
    discrete,
    trajectory,
    minimumReportRate,
    reconstruction,
};

//  Everything one arriving event declares besides its canonical fields. This is the judgement
//  pipeline entry: the calibrated session clock is passed to normalizeObservation as a separate
//  ObservationTick argument and is never derived from anything in this structure.
struct IngressDeclaration final {
    //  The monotonic submission ordinal the host adapter supplies before the event enters the
    //  session (CM-T06). It is recorded as session state and it never decides duplication or
    //  ordering.
    IngressSequence ingressSequence;
    //  The input-side verb of the edge.
    InputAction action;
    //  The declared logical channel. The caller's token must outlive the returned entry, the same
    //  non-owning convention the ABI fixes for prepared views.
    ChannelRef channel;
    //  The domain this event belongs to. An empty token means "not stated", which resolves to the
    //  single declared domain when the mapping declares exactly one; with several declared domains
    //  an unstated domain is refused rather than guessed by table order.
    std::string_view domainToken{};
    //  The raw, uncalibrated timestamps. Diagnostic context only.
    RawIngressTimestamps rawTimestamps;
    //  The discontinuity state the source reports. A crossing is rejected, not represented.
    DiscontinuityDeclaration discontinuity;
    //  The continuity capability the source declares. Anything but discrete is rejected.
    ContinuityKind continuity;
    //  The exact quantity the event carries, when the event carries one. Absent means the event
    //  declares no amount; it never means zero. The quantity is exact because the quantization
    //  divides it by the declared scale exactly (Spec 3.7.6 item 2).
    std::optional<RationalBeat> quantity;
};

//  Per-session ingress state. Owner thread: the thread that owns the session. All the members are
//  session state and none of them enters any identity (Spec 5.2 keeps in-session counters out of
//  every identity).
//
//  Reset behaviour: reset() returns every member to its created value, which is what makes
//  "a refused entry changes nothing" observable rather than promised.
//
//  The observed sequence ordinals are the session's record of which ingress sequences were already
//  admitted. The rule that uses it is unconditional: an ingress sequence is admitted at most once
//  per session. Keeping the whole set, instead of only the highest ordinal, is deliberate - a
//  highest-only rule would reject a *fresh* ordinal that happens to be lower than a previously seen
//  one, which is a different decision from the one the contract makes and one this batch does not
//  own. The cost of the set is therefore the price of not inventing a rule.
struct ClockedIngress final {
    ObservationTick observationTick;
    IngressDeclaration declaration;
};

class SessionIngressState final {
  public:
    SessionIngressState() = default;

    SessionIngressState(const SessionIngressState&) = delete;
    auto operator=(const SessionIngressState&) -> SessionIngressState& = delete;
    SessionIngressState(SessionIngressState&&) = delete;
    auto operator=(SessionIngressState&&) -> SessionIngressState& = delete;
    ~SessionIngressState() = default;

    //  The calibrated session clock captured at the most recent entry, or nullopt before the first
    //  entry. There is no zero default: "no clock was captured yet" and "the clock read zero" are
    //  different states, and only the first is representable before the first entry.
    [[nodiscard]] auto lastObservedTick() const noexcept -> const ObservationTick*;

    //  Returns every member to its created-phase value. A reset session accepts again exactly what
    //  a freshly created session accepts.
    void reset() noexcept;

  private:
    friend class detail::ExecutionKernel;
    friend auto detail::prepareIngressBatch(const SessionIngressState&, const InputMappingProfile&,
                                            std::span<const ClockedIngress>, bool,
                                            std::span<const std::optional<std::int64_t>>)
        -> core::Result<detail::IngressJournal>;
    friend auto detail::reserveIngressBatch(SessionIngressState&, const detail::IngressJournal&)
        -> core::Result<void>;
    friend void detail::commitIngressBatch(SessionIngressState&, detail::IngressJournal&&) noexcept;
    friend auto normalizeObservation(SessionIngressState&, const InputMappingProfile&,
                                     ObservationTick, const IngressDeclaration&)
        -> core::Result<NormalizedObservationEntry>;

    std::optional<ObservationTick> lastObservedTick_{};
    std::uint64_t nextObservationId_{0};
    bool observationIdsExhausted_{false};
    std::vector<std::unique_ptr<const detail::OwnedIngressSubject>> ownedSubjects_{};
    std::vector<IngressSequence> admittedSequences_{};
    std::vector<CanonicalIngressSubject> admittedSubjects_{};
};

//  The single normalization entry of live and Replay traffic (plan S7A-2 item 6).
//
//  The three arguments are three different things and are deliberately not combined:
//
//    sessionClock   the calibrated session clock, captured by the session owner at the moment the
//                   input enters the judgement pipeline. It is the only canonical source of
//                   observationTick (Spec 3.7.3 item 1);
//    declaration    everything the arriving event says about itself, including the raw timestamps
//                   and the continuity capability. The raw timestamps reach the diagnostic channel
//                   and nothing else (Spec 3.7.3 item 2);
//    mapping        the session's mapping profile, which resolves the domain and its AmountSpec.
//
//  Rejections, each with a stable code and category:
//
//    * an uncalibrated or undeclared clock: the calibrated session clock is a separate argument, so
//      "the clock was never captured" is not expressible as a value here and cannot be silently
//      read as zero. A caller that has no captured clock has no ObservationTick to pass;
//    * a discontinuity, a reconnect or dropped samples -> the capability rejection of Spec 3.7.7;
//    * a continuity capability other than discrete -> the R-05 rejection of Spec 3.7.7, naming the
//      capability and the replacement path (S7B-1);
//    * an amount outside the declared range, or one whose canonical integer is not representable ->
//      the budget_exceeded rejections of Spec 3.7.6 item 3;
//    * an ingress sequence that this session already admitted -> the duplicate-queue rejection of
//      Spec 3.7.4 item 4;
//    * a calibrated clock that moved backwards -> the monotonicity rejection of ABI section
//      "lifecycle and failure invariants" item 4, categorized as invalid_relation. A clock
//      regression is a time-order relation error, which is the Spec 9.3 case invalid_relation
//      covers, while budget_exceeded is reserved for a numeric value leaving a declared range or a
//      budget;
//    * a second, different observation at a tick this session already admitted -> the explicit
//      same-tick behaviour of CM-T10, decided by the canonical identity and never by the arrival
//      order.
//
//  A negative calibrated clock value is representable in the signed 64-bit tick domain and is
//  admitted: the ruling freezes the width and the monotonicity rule, not a signedness restriction
//  on the calibrated session clock. A reversal is rejected, so the negative region is still
//  ordered.
//
//  A refused entry writes nothing to `state`: no observation id is consumed, no ordinal is recorded
//  and no clock is stored.
[[nodiscard]] auto
normalizeObservation(SessionIngressState& state, const InputMappingProfile& mapping,
                     ObservationTick sessionClock, const IngressDeclaration& declaration)
    -> core::Result<NormalizedObservationEntry>;

//  ---------------------------------------------------------------------------------------------
//  Late-policy admission: duplicate queue entries on the canonical identity (Spec 3.7.4 item 4)
//  ---------------------------------------------------------------------------------------------

//  Rejects an entry that is already queued, for the late-policy queue.
//
//  The criterion is the canonical observation identity and nothing else. A subject carries no
//  ingress ordinal and no arrival position, so two subjects compare equal exactly when the
//  canonical observation fields agree, whatever order they arrived in. The function is a pure
//  predicate over the two arguments, so the verdict cannot depend on the order in which a caller
//  happened to place them in a container.
[[nodiscard]] auto admitLateQueueEntry(const CanonicalIngressSubject& alreadyQueued,
                                       const CanonicalIngressSubject& candidate)
    -> core::Result<void>;

//  True when a tick already carries an observation with a different canonical identity.
//
//  This is the same-tick collision question of CM-T10. It is derived from the canonical identity of
//  the two entries, so the answer does not change when the two entries are presented in the other
//  order, and it does not change when their ingress ordinals are swapped.
[[nodiscard]] auto hasDistinctIdentityAtSameTick(const CanonicalIngressSubject& queued,
                                                 const CanonicalIngressSubject& candidate) noexcept
    -> bool;

} // namespace cuexis::judgement
