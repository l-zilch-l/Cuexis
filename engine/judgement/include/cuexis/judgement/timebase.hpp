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

#include "timebase_profile.hpp"
#include "timebase_rational.hpp"
#include "timebase_tick.hpp"

#include <cuexis/core/result.hpp>

#include <cstdint>

namespace cuexis::judgement {

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
