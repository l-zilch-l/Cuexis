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
auto prepareIngressBatch(const SessionIngressState&, const InputMappingProfile&,
                         std::span<const ClockedIngress>, bool) -> core::Result<IngressJournal>;
auto reserveIngressBatch(SessionIngressState&, const IngressJournal&) -> core::Result<void>;
void commitIngressBatch(SessionIngressState&, IngressJournal&&) noexcept;
} // namespace detail

//  ---------------------------------------------------------------------------------------------
//  AmountSpec (Spec 3.7.6, CM-T13)
//  ---------------------------------------------------------------------------------------------

//  The declared boundary policy of an amount range: whether the declared minimum and maximum are
//  themselves representable canonical values. The set is minimal on purpose. A "clamp" or "fall
//  back to default" enumerator would contradict Spec 3.7.6 item 3, which forbids saturation,
//  truncation and a fallback to a default quantity, so no such enumerator exists here and a caller
//  cannot select one by accident.
//
//  There is deliberately no default value: a specification that forgot to declare its boundary
//  policy does not compile, instead of silently becoming the first enumerator.
enum class AmountBoundaryPolicy : std::uint8_t {
    //  The declared minimum and maximum are inside the representable set.
    inclusive,
    //  The declared minimum and maximum are outside the representable set: the range is the open
    //  interval between them.
    exclusive,
};

//  The canonical integer quantization of one input domain (Spec 3.7.6 item 1).
//
//  `scale` is the exact rational size of one canonical integer unit, expressed in the domain's own
//  declared quantity unit. It is a rational and not a floating point value, because Spec 3.7.6
//  item 2 requires the quantization to use exact integer arithmetic only. A caller declares the
//  scale explicitly; this batch freezes no business quantity, so there is no default scale.
//
//  The canonical integer width is frozen as a signed 64-bit integer (CM-T13), the same width the
//  round 2 ruling froze for the Tick domains (Spec 3.7.1 item 1), so the module carries one integer
//  boundary and the two sides cannot drift apart. The width is a frozen declaration and not a
//  configurable value: `minimum` and `maximum` are declared in that width, no caller can select
//  another one, and no spec field records a different width.
struct AmountSpec final {
    //  Exact size of one canonical integer unit. Must be a positive rational; a declared spec whose
    //  scale is not positive is rejected by validateInputMapping and by quantizeAmount.
    RationalDuration scale;
    //  The declared representable range of the canonical integer, in canonical integer units.
    std::int64_t minimum;
    std::int64_t maximum;
    //  Whether the two bounds above are themselves representable.
    AmountBoundaryPolicy boundaryPolicy;

    friend constexpr auto operator==(const AmountSpec&, const AmountSpec&) noexcept
        -> bool = default;
};

//  A quantized amount: the canonical integer and the domain and specification that gave it its
//  meaning. The specification is borrowed for the lifetime of the borrowed declaration, the same
//  non-owning convention the ABI fixes for prepared views.
struct QuantizedAmount final {
    std::int64_t canonicalInteger;
    const AmountSpec* spec;
};

//  One input domain declaration: the domain's identity token and the typed AmountSpec the domain
//  carries. The amount specification is a required member, which is the type-level form of Spec
//  3.7.6 item 1: a domain without a declared quantization cannot be spelled.
//
//  The token is the domain's stable identity. The enumeration set behind it belongs to round 6
//  (P2-05) and is not frozen here, so no enumerator is invented; the mapping profile decides which
//  tokens exist for a session.
struct InputDomainDeclaration final {
    //  Stable domain identity token. Empty means "not declared" and is rejected.
    std::string_view domainToken;
    AmountSpec amount;
};

//  Quantizes an exact quantity into the canonical integer of one domain.
//
//  The rule is exact and has no floating point step (Spec 3.7.6 items 1 and 2):
//
//    canonicalInteger = roundHalfToEven(quantity / amount.scale)
//
//  Every intermediate is an exact rational carried as a reduced pair of signed 64-bit integers, the
//  tie rule is round-half-to-even, and the rule is symmetric for negative values. Rejections:
//
//    * the declared specification is not usable (a non-positive scale, or a minimum above its
//      maximum) -> invalid_relation, the "declaration is not a valid declaration" case of Spec 9.3;
//    * the exact quotient is not representable in the canonical integer type -> budget_exceeded,
//    the
//      narrowing case of Spec 3.7.6 item 3. This is where an input whose scaled magnitude leaves
//      the frozen integer width is rejected instead of being saturated or truncated;
//    * the quantized value is representable but outside the declared range -> budget_exceeded, the
//      out-of-range case of Spec 3.7.6 item 3. No clamping and no default fallback happen.
[[nodiscard]] auto quantizeAmount(const AmountSpec& spec, RationalBeat quantity)
    -> core::Result<QuantizedAmount>;

//  ---------------------------------------------------------------------------------------------
//  InputDomain, InputAction, ChannelRef, SourceClass (ABI domain 2)
//  ---------------------------------------------------------------------------------------------

//  The input-side verb. The announced 7A set is press / release / update (ABI domain 2, plan S7A-2
//  item 2). The broader ABI verb list stays open: it names begin / end / press / release / update /
//  step, and whether begin / end are the same verbs under a different spelling or a second set is a
//  question the ABI has not settled. Only the three announced 7A verbs are represented, so a caller
//  cannot select a verb 7A never opened, and there is no default enumerator.
enum class InputAction : std::uint8_t {
    press,
    release,
    update,
};

//  A logical input channel. It is a declared token, never a device scan code (ABI domain 2), so the
//  device layer cannot leak through it.
class ChannelRef final {
  public:
    constexpr ChannelRef() noexcept = default;

    friend constexpr auto operator==(ChannelRef, ChannelRef) noexcept -> bool = default;

    [[nodiscard]] constexpr auto token() const noexcept -> std::string_view {
        return token_;
    }

    //  The only way to name a channel. A request without a name is refused here, at the boundary,
    //  instead of travelling as an unnamed channel that every consumer would read differently.
    [[nodiscard]] static auto fromToken(std::string_view token) -> core::Result<ChannelRef>;

  private:
    explicit constexpr ChannelRef(std::string_view token) noexcept : token_(token) {}

    std::string_view token_{};
};

//  The device class of an input source, without serial numbers (ABI domain 2). It is the V2
//  destination of the Gameplay I InputEvent source and it does take part in the canonical
//  observation identity.
class SourceClass final {
  public:
    constexpr SourceClass() noexcept = default;

    friend constexpr auto operator==(SourceClass, SourceClass) noexcept -> bool = default;

    [[nodiscard]] constexpr auto token() const noexcept -> std::string_view {
        return token_;
    }

    [[nodiscard]] static auto fromToken(std::string_view token) -> core::Result<SourceClass>;

  private:
    explicit constexpr SourceClass(std::string_view token) noexcept : token_(token) {}

    std::string_view token_{};
};

//  The monotonic in-session ingress ordinal supplied by the host adapter before an event enters the
//  session (ABI domain 2, CM-T06). It is the host's single submission order. It is deliberately not
//  an input to the canonical observation identity: Spec 3.7.4 item 4 and Spec 3.7.2 item 4 forbid
//  using it to decide duplication or ordering.
class IngressSequence final {
  public:
    constexpr IngressSequence() noexcept = default;
    explicit constexpr IngressSequence(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t {
        return value_;
    }

    friend constexpr auto operator==(IngressSequence, IngressSequence) noexcept -> bool = default;
    friend constexpr auto operator<=>(IngressSequence, IngressSequence) noexcept = default;

  private:
    std::uint64_t value_{0};
};

//  The session-monotonic observation identity (ABI domain 2). It is derived by the session, never
//  supplied by the host, and it is a session state counter rather than a canonical fact: Spec 5.2
//  keeps in-session counters out of every identity.
class ObservationId final {
  public:
    constexpr ObservationId() noexcept = default;
    explicit constexpr ObservationId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t {
        return value_;
    }

    friend constexpr auto operator==(ObservationId, ObservationId) noexcept -> bool = default;
    friend constexpr auto operator<=>(ObservationId, ObservationId) noexcept
        -> std::strong_ordering = default;

  private:
    std::uint64_t value_{0};
};

//  ---------------------------------------------------------------------------------------------
//  InputMapping profile (CM-T09)
//  ---------------------------------------------------------------------------------------------

//  The device-to-domain mapping profile. It belongs to the session (CM-T09) and it does not change
//  the Canonical Gameplay Graph: the mapping is a session identity component, not chart content,
//  which is exactly the row Spec 5.2 carries for it (session component, judgement identity, runtime
//  mutation rejected).
//
//  `profileId`, `profileVersion` and `sourceClass` are three of the components that enter the
//  session judgement identity, and they are distinct fields because the contract names them
//  distinctly: a mapping whose version changed is a different session, and a mapping whose source
//  class changed is a different session, even when the domain table is identical. The fourth
//  component is the domain table itself, because a declared AmountSpec is the quantization CM-T09
//  requires in the session identity, and a different quantization makes a different canonical
//  observation out of the same incoming quantity. All four are compared by
//  contributesToSameSessionIdentity below, so no consumer has to guess which components the
//  comparison uses.
//
//  The profile owns its domain table. One session declares one mapping, so the table is a value and
//  not a shared pointer; the product sees a mapping profile from one side only.
struct InputMappingProfile final {
    //  Stable mapping profile identity. Empty means "not declared" and is rejected.
    std::string_view profileId;
    //  Mapping profile version. Empty means "not declared" and is rejected. A version change is an
    //  identity change, never an in-place upgrade of a running session.
    std::string_view profileVersion;
    //  The device class this mapping binds. Empty means "not declared" and is rejected.
    SourceClass sourceClass;
    //  One declaration per input domain. Each carries its own typed AmountSpec. The order of the
    //  table is a declaration order and carries no semantics: validateInputMapping rejects a
    //  duplicated domain token instead of resolving it by position, and the session identity
    //  comparison below therefore reads the table as a set keyed by the domain token. The set
    //  itself is a session identity component, because each declared AmountSpec is the quantization
    //  the canonical observation depends on.
    std::vector<InputDomainDeclaration> domains;
};

//  True when the two mapping profiles contribute the same session identity.
//
//  The comparison covers every component that can change the canonical content of a
//  NormalizedObservation: the three declared identity components (profile identity, version and
//  source class) and the whole domain declaration set. CM-T09 requires the mapping's source
//  identity, version and quantization to enter the session identity, and a declared AmountSpec is
//  exactly that quantization - a different scale, a different representable range or a different
//  boundary policy turns the same incoming quantity into a different canonical integer. The domain
//  declaration set is therefore part of the identity and not a later batch's question.
//
//  For every domain declaration the comparison uses the domain token as the normalization key and
//  then the whole declared AmountSpec: `scale`, `minimum`, `maximum` and `boundaryPolicy`.
//  AmountSpec carries no separate quantization version field, so those four fields are the complete
//  declared specification. The key is the domain token compared in its lexicographic byte order,
//  and the comparison is item-wise on the two tables read in that normalized (sorted-by-token)
//  order. The declaration order of the table carries no semantics, so a table that declares the
//  same domains in another order contributes the same identity; adding or removing a declaration,
//  or changing any field of one declaration's AmountSpec, does not.
//
//  A table that repeats a domain token is not a valid declaration rather than a resolvable one
//  (validateInputMapping refuses it), so this comparison reports "not the same identity" for it
//  instead of inventing a resolution order.
//
//  The predicate is allocation-free and compares only the two arguments, so its verdict cannot
//  depend on the order in which a caller presents them.
[[nodiscard]] auto contributesToSameSessionIdentity(const InputMappingProfile& left,
                                                    const InputMappingProfile& right) noexcept
    -> bool;

//  Validates a mapping profile as a declaration, before any observation is normalized against it.
//
//  Checks, in the order the diagnostics are reported:
//
//    * every declared identity component is present: a missing profile identity, version or source
//      class is a structurally incomplete declaration -> invalid_relation (Spec 9.3);
//    * every domain declares a non-empty token and a usable AmountSpec: a non-positive scale or a
//      minimum above its maximum is a declaration that cannot be honoured -> invalid_relation;
//    * no domain token is declared twice: a duplicated declaration is rejected rather than resolved
//      by table position, the same rule the timebase profile applies to a duplicated tempo or stop
//      section -> invalid_relation.
//
//  This is a session-level declaration check, so it is not the prepare gate itself: prepare also
//  validates the TimebaseProfile and the LatePolicyParameters through validatePrepare, and the
//  mapping profile is fed into the session identity when prepare freezes the session.
[[nodiscard]] auto validateInputMapping(const InputMappingProfile& mapping) -> core::Result<void>;

//  Rejects an attempt to change the mapping profile of a session that already holds one (CM-T09,
//  Spec 5.2: the mapping is a session component and a runtime modification is a stable rejection).
//
//  The two-argument form is the type-level statement of the rule: the owner passes the current
//  profile and the candidate, and the refusal covers exactly the components the session identity
//  covers, through the same contributesToSameSessionIdentity comparison. A change to the profile
//  identity, the version, the source class or the domain declaration set - including a per-domain
//  scale, representable range or boundary policy change, and the addition or removal of a
//  declaration - is refused; a pure reordering of the same declarations is not a change and is
//  accepted as the same mapping. The diagnostic names the first identity component that differs.
//  The function never writes to either profile, so a rejected attempt leaves the session unchanged.
[[nodiscard]] auto rejectRuntimeMappingChange(const InputMappingProfile& current,
                                              const InputMappingProfile& candidate)
    -> core::Result<void>;

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
    friend auto detail::prepareIngressBatch(const SessionIngressState&, const InputMappingProfile&,
                                            std::span<const ClockedIngress>, bool)
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
