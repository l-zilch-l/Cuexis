#pragma once

//  Judgement input boundary - declared input domains and their exact quantization.

#include "timebase.hpp"

#include <cuexis/core/result.hpp>

#include <compare>
#include <cstdint>
#include <string_view>

namespace cuexis::judgement {

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

} // namespace cuexis::judgement
