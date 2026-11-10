#pragma once

//  Judgement input boundary - input roles, channels and the session mapping profile.

#include "input_boundary_amount.hpp"

#include <cuexis/core/result.hpp>

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement {

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

} // namespace cuexis::judgement
