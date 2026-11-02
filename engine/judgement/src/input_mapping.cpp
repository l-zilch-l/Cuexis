#include "ingress_transaction.hpp"
#include "source_codes.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cuexis/judgement/diagnostic.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "input_helpers.hpp"

namespace cuexis::judgement {

using namespace input_detail;

auto ChannelRef::fromToken(std::string_view token) -> core::Result<ChannelRef> {
    if (token.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDomainTokenPath, "a logical channel must be declared by a non-empty token"));
    }
    return ChannelRef{token};
}

auto SourceClass::fromToken(std::string_view token) -> core::Result<SourceClass> {
    if (token.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kMappingSourceClassPath,
            "a source class must be declared by a non-empty token without a serial number"));
    }
    return SourceClass{token};
}

//  ---------------------------------------------------------------------------------------------
//  AmountSpec quantization (Spec 3.7.6)
//  ---------------------------------------------------------------------------------------------

auto quantizeAmount(const AmountSpec& spec, RationalBeat quantity)
    -> core::Result<QuantizedAmount> {
    if (!isUsableSpec(spec)) {
        //  The declaration cannot be honoured: the scale has no meaningful sign, or the range is
        //  empty by construction. That is a declaration fault, not a value fault.
        return core::unexpected(declarationInvalidError(
            codes::kAmountScalePath, "the declared amount specification cannot be honoured"));
    }

    const std::int64_t numerator = quantity.numerator();
    const std::int64_t denominator = quantity.denominator();
    const std::int64_t scaleNumerator = spec.scale.numerator();
    const std::int64_t scaleDenominator = spec.scale.denominator();
    if (!isRepresentableRational(numerator, denominator) ||
        !isRepresentableRational(scaleNumerator, scaleDenominator)) {
        return core::unexpected(amountValueError(
            codes::kAmountNarrowedCode, codes::kAmountPath,
            "the exact quantity is not a representable rational in the frozen 64-bit "
            "representation"));
    }

    //  canonicalInteger = roundHalfToEven(quantity / scale)
    //                    = roundHalfToEven((quantity.n * scale.d) / (quantity.d * scale.n))
    //  computed as one exact ratio, so the single rounding step happens once, in the exact domain.
    const auto exactNumerator = checkedMultiply(numerator, scaleDenominator);
    const auto exactDenominator = checkedMultiply(denominator, scaleNumerator);
    if (!exactNumerator.has_value() || !exactDenominator.has_value()) {
        //  The exact scaled ratio left the signed 64-bit representation. This is the narrowing case
        //  of Spec 3.7.6 item 3: reject, never saturate and never truncate.
        return core::unexpected(amountValueError(
            codes::kAmountNarrowedCode, codes::kAmountPath,
            "the exact canonical quotient is not representable in the declared integer type"));
    }
    if (*exactDenominator <= 0) {
        return core::unexpected(declarationInvalidError(
            codes::kAmountScalePath, "the declared amount specification cannot be honoured"));
    }
    if (*exactNumerator == kInt64Min) {
        //  |INT64_MIN| has no signed 64-bit representation, so the exact quotient -2^63 cannot be
        //  rounded here. It is a narrowing rejection of the same kind as any other value the frozen
        //  integer type cannot hold exactly.
        return core::unexpected(amountValueError(
            codes::kAmountNarrowedCode, codes::kAmountPath,
            "the exact canonical quotient is not representable in the declared integer type"));
    }
    const auto rounded = roundHalfToEvenRatio(*exactNumerator, *exactDenominator);
    if (!rounded.has_value()) {
        return core::unexpected(amountValueError(
            codes::kAmountNarrowedCode, codes::kAmountPath,
            "the exact canonical quotient is not representable in the declared integer type"));
    }

    //  An exclusive boundary policy moves both declared bounds outside the representable set.
    if (spec.boundaryPolicy == AmountBoundaryPolicy::exclusive) {
        if (*rounded == spec.minimum || *rounded == spec.maximum) {
            return core::unexpected(amountValueError(
                codes::kAmountOutOfRangeCode, codes::kAmountRangePath,
                "the quantized amount is outside the declared representable range"));
        }
    }
    //  The range is checked on the rounded value, so the two rejections stay distinguishable: a
    //  representable value outside the declared range is an out-of-range rejection, and a value the
    //  canonical integer cannot hold at all is a narrowing rejection. No clamping happens here.
    if (*rounded < spec.minimum || *rounded > spec.maximum) {
        return core::unexpected(
            amountValueError(codes::kAmountOutOfRangeCode, codes::kAmountRangePath,
                             "the quantized amount is outside the declared representable range"));
    }

    return QuantizedAmount{.canonicalInteger = *rounded, .spec = &spec};
}

//  ---------------------------------------------------------------------------------------------
//  InputMapping profile (CM-T09)
//  ---------------------------------------------------------------------------------------------

auto contributesToSameSessionIdentity(const InputMappingProfile& left,
                                      const InputMappingProfile& right) noexcept -> bool {
    //  The comparison covers everything that can change the canonical content of a
    //  NormalizedObservation: the three declared identity components and the whole domain
    //  declaration set, compared by token in normalized (sorted-by-token) order. The declaration
    //  order of the table carries no semantics, so a reordered table is the same identity; adding
    //  or removing a declaration, or changing any field of a declaration's AmountSpec, is not.
    return left.profileId == right.profileId && left.profileVersion == right.profileVersion &&
           left.sourceClass == right.sourceClass &&
           sameDomainDeclarationSet(left.domains, right.domains);
}

auto validateInputMapping(const InputMappingProfile& mapping) -> core::Result<void> {
    if (mapping.profileId.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kMappingProfileIdPath, "the mapping profile declares no profile identity"));
    }
    if (mapping.profileVersion.empty()) {
        return core::unexpected(declarationInvalidError(codes::kMappingProfileVersionPath,
                                                        "the mapping profile declares no version"));
    }
    if (mapping.sourceClass.token().empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kMappingSourceClassPath, "the mapping profile declares no source class"));
    }

    for (std::size_t index = 0; index < mapping.domains.size(); ++index) {
        const InputDomainDeclaration& declaration = mapping.domains[index];
        if (declaration.domainToken.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDomainTokenPath, "an input domain declares no domain identity token"));
        }
        if (!isUsableSpec(declaration.amount)) {
            return core::unexpected(declarationInvalidError(
                codes::kAmountPath, "the declared amount specification cannot be honoured"));
        }
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (mapping.domains[earlier].domainToken == declaration.domainToken) {
                //  A duplicated domain declaration is rejected rather than resolved by table
                //  position, the same rule the timebase profile applies to a duplicated section.
                return core::unexpected(declarationInvalidError(
                    codes::kMappingDomainsPath,
                    "an input domain is declared more than once in the mapping profile"));
            }
        }
    }
    return {};
}

auto rejectRuntimeMappingChange(const InputMappingProfile& current,
                                const InputMappingProfile& candidate) -> core::Result<void> {
    if (contributesToSameSessionIdentity(current, candidate)) {
        return {};
    }
    //  The mapping is a session component: a running session never adopts a new one, and the
    //  candidate is written nowhere, so a refused change leaves the session unchanged. The refusal
    //  covers the same components the session identity covers - the three declared identity
    //  components and the domain declaration set - so a per-domain quantization change is refused
    //  exactly like a version change, and the diagnostic names the component that differs.
    return core::unexpected(
        runtimeMutationError(firstDifferingIdentityPath(current, candidate),
                             "the input mapping profile of a running session cannot be changed"));
}

//  ---------------------------------------------------------------------------------------------
//  SessionIngressState
//  ---------------------------------------------------------------------------------------------

auto SessionIngressState::lastObservedTick() const noexcept -> const ObservationTick* {
    return lastObservedTick_.has_value() ? &lastObservedTick_.value() : nullptr;
}

void SessionIngressState::reset() noexcept {
    lastObservedTick_.reset();
    nextObservationId_ = 0;
    observationIdsExhausted_ = false;
    admittedSequences_.clear();
    admittedSubjects_.clear();
    ownedSubjects_.clear();
}

} // namespace cuexis::judgement
