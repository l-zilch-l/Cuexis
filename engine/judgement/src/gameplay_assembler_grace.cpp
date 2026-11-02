// Gameplay assembler grace: prepared grace resolution and the resource claim plan.

#include "gameplay_assembler_compile_internal.hpp"
#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

[[nodiscard]] constexpr auto magnitude(std::int64_t value) noexcept -> std::uint64_t {
    return value < 0 ? static_cast<std::uint64_t>(-(value + 1)) + 1U
                     : static_cast<std::uint64_t>(value);
}

[[nodiscard]] constexpr auto gcdUnsigned(std::uint64_t left, std::uint64_t right) noexcept
    -> std::uint64_t {
    while (right != 0U) {
        const std::uint64_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

[[nodiscard]] auto checkedUnsignedMultiply(std::uint64_t left, std::uint64_t right)
    -> std::optional<std::uint64_t> {
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return std::nullopt;
    }
    return left * right;
}

[[nodiscard]] auto rationalQuotient(const RationalDuration& value, const RationalDuration& unit)
    -> std::optional<std::pair<std::int64_t, std::int64_t>> {
    std::int64_t numerator = value.numerator();
    std::int64_t denominator = value.denominator();
    std::int64_t unitNumerator = unit.numerator();
    std::int64_t unitDenominator = unit.denominator();

    const std::uint64_t firstCancel = gcdUnsigned(magnitude(numerator), magnitude(unitNumerator));
    if (firstCancel != 0U) {
        numerator /= static_cast<std::int64_t>(firstCancel);
        unitNumerator /= static_cast<std::int64_t>(firstCancel);
    }
    const std::uint64_t secondCancel =
        gcdUnsigned(magnitude(unitDenominator), magnitude(denominator));
    if (secondCancel != 0U) {
        unitDenominator /= static_cast<std::int64_t>(secondCancel);
        denominator /= static_cast<std::int64_t>(secondCancel);
    }

    const auto numeratorMagnitude =
        checkedUnsignedMultiply(magnitude(numerator), magnitude(unitDenominator));
    const auto denominatorMagnitude =
        checkedUnsignedMultiply(magnitude(denominator), magnitude(unitNumerator));
    if (!numeratorMagnitude.has_value() || !denominatorMagnitude.has_value() ||
        *denominatorMagnitude == 0U ||
        *numeratorMagnitude >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        *denominatorMagnitude >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }

    const bool negative = (numerator < 0) != (unitNumerator < 0);
    const auto signedNumerator = negative ? -static_cast<std::int64_t>(*numeratorMagnitude)
                                          : static_cast<std::int64_t>(*numeratorMagnitude);
    return std::pair{signedNumerator, static_cast<std::int64_t>(*denominatorMagnitude)};
}

[[nodiscard]] auto roundHalfToEvenGrace(std::int64_t numerator, std::int64_t denominator)
    -> std::optional<std::int64_t> {
    if (denominator <= 0) {
        return std::nullopt;
    }
    std::int64_t quotient = numerator / denominator;
    std::int64_t remainder = numerator % denominator;
    if (remainder < 0) {
        if (quotient == std::numeric_limits<std::int64_t>::min()) {
            return std::nullopt;
        }
        --quotient;
        remainder += denominator;
    }
    const std::uint64_t twice = static_cast<std::uint64_t>(remainder) * 2U;
    const auto denominatorUnsigned = static_cast<std::uint64_t>(denominator);
    if (twice < denominatorUnsigned) {
        return quotient;
    }
    if (twice > denominatorUnsigned) {
        if (quotient == std::numeric_limits<std::int64_t>::max()) {
            return std::nullopt;
        }
        return quotient + 1;
    }
    if (quotient % 2 == 0) {
        return quotient;
    }
    if (quotient == std::numeric_limits<std::int64_t>::max()) {
        return std::nullopt;
    }
    return quotient + 1;
}

[[nodiscard]] auto graceCandidateValue(const GraceResolutionInputs& inputs,
                                       const std::optional<RationalDuration>& duration)
    -> core::Result<std::int64_t> {
    if (duration.has_value()) {
        const auto quotient = rationalQuotient(*duration, inputs.unitInTicks);
        if (!quotient.has_value()) {
            return core::unexpected(valueError(
                codes::kGraceNotRepresentableCode, codes::kGraphSection, codes::kPreparedGracePath,
                "the grace duration cannot be represented in the canonical rational domain"));
        }
        const auto rounded = roundHalfToEvenGrace(quotient->first, quotient->second);
        if (!rounded.has_value()) {
            return core::unexpected(valueError(
                codes::kGraceNotRepresentableCode, codes::kGraphSection, codes::kPreparedGracePath,
                "the quantized grace value is not representable as a signed TickSpan"));
        }
        return *rounded;
    }
    if (inputs.candidate.has_value()) {
        return inputs.candidate->value();
    }
    return core::unexpected(declarationInvalidError(codes::kGraceValueMissingCode,
                                                    codes::kGraphSection, codes::kPreparedGracePath,
                                                    "the selected grace source supplied no value"));
}

auto resolvePreparedGrace(const GraceDeclaration& declaration, const GraceResolutionInputs& inputs)
    -> core::Result<PreparedGrace> {
    if (declaration.policy == GraceResolutionPolicy::inheritedDeclaration &&
        declaration.inheritedFromDeclarationId.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kGraceInheritanceUndeclaredCode, codes::kGraphSection, codes::kGracePath,
            "an inherited grace must name its source declaration"));
    }
    if (declaration.policy != GraceResolutionPolicy::inheritedDeclaration &&
        !declaration.inheritedFromDeclarationId.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kGraceInheritanceUnexpectedCode, codes::kGraphSection, codes::kGracePath,
            "only an inherited grace may name a source declaration"));
    }
    if (inputs.minimumCanonical > inputs.maximumCanonical) {
        return core::unexpected(valueError(codes::kGraceRangeReversedCode, codes::kGraphSection,
                                           codes::kPreparedGracePath,
                                           "the declared grace range is reversed"));
    }
    if (inputs.unitInTicks.numerator() <= 0) {
        return core::unexpected(valueError(codes::kGraceUnitNotPositiveCode, codes::kGraphSection,
                                           codes::kPreparedGracePath,
                                           "the grace quantization unit must be positive"));
    }
    if ((inputs.chartDuration && inputs.chartDuration->numerator() < 0) ||
        (inputs.inheritedDuration && inputs.inheritedDuration->numerator() < 0) ||
        (inputs.defaultDuration && inputs.defaultDuration->numerator() < 0) ||
        (inputs.candidate && inputs.candidate->value() < 0)) {
        return core::unexpected(valueError(codes::kGraceOutOfRangeCode, codes::kGraphSection,
                                           codes::kPreparedGracePath,
                                           "a negative duration is invalid before quantization"));
    }
    if (inputs.chartDuration.has_value() && inputs.candidate.has_value()) {
        return core::unexpected(declarationInvalidError(
            codes::kGraceValueSourceConflictCode, codes::kGraphSection, codes::kPreparedGracePath,
            "the chart supplied two competing grace values"));
    }
    const bool chartSupplied = inputs.chartDuration.has_value();
    if (chartSupplied && !declaration.allowChartGrace) {
        return core::unexpected(valueError(
            codes::kGraceChartSupplyNotAllowedCode, codes::kGraphSection, codes::kPreparedGracePath,
            "the declaration does not allow a chart-supplied grace value"));
    }

    core::Result<std::int64_t> selected = core::unexpected(declarationInvalidError(
        codes::kGraceValueMissingCode, codes::kGraphSection, codes::kPreparedGracePath,
        "the selected grace source supplied no value"));
    if (chartSupplied) {
        selected = graceCandidateValue(inputs, inputs.chartDuration);
    } else {
        switch (declaration.policy) {
        case GraceResolutionPolicy::explicitDeclaration:
            selected = graceCandidateValue(inputs, std::nullopt);
            break;
        case GraceResolutionPolicy::inheritedDeclaration:
            selected = graceCandidateValue(inputs, inputs.inheritedDuration);
            break;
        case GraceResolutionPolicy::defaultDeclaration:
            selected = graceCandidateValue(inputs, inputs.defaultDuration);
            break;
        }
    }
    if (!selected.has_value()) {
        return core::unexpected(selected.error());
    }
    if (*selected < 0 || *selected < inputs.minimumCanonical ||
        *selected > inputs.maximumCanonical) {
        return core::unexpected(
            valueError(codes::kGraceOutOfRangeCode, codes::kGraphSection, codes::kPreparedGracePath,
                       "the quantized grace value is outside its declared canonical range"));
    }
    return PreparedGrace{TickSpan{*selected}};
}

ResourceClaimResolution::ResourceClaimResolution(
    std::shared_ptr<const detail::ResourceClaimResolutionStorage> storage) noexcept
    : storage_(std::move(storage)) {}

auto ResourceClaimResolution::candidateCount() const noexcept -> std::size_t {
    return storage_ == nullptr ? 0U : storage_->candidates.size();
}

auto ResourceClaimResolution::occupyingCandidateCount() const noexcept -> std::size_t {
    if (storage_ == nullptr) {
        return 0U;
    }
    return static_cast<std::size_t>(
        std::count_if(storage_->candidates.begin(), storage_->candidates.end(),
                      [](const ResourceClaimResolutionInputs::Candidate& candidate) {
                          return candidate.intent != ResourceClaimIntent::observe;
                      }));
}

auto ResourceClaimResolution::hasObserveOnlyCandidates() const noexcept -> bool {
    return storage_ != nullptr && !storage_->candidates.empty() &&
           std::all_of(storage_->candidates.begin(), storage_->candidates.end(),
                       [](const ResourceClaimResolutionInputs::Candidate& candidate) {
                           return candidate.intent == ResourceClaimIntent::observe;
                       });
}

auto ResourceClaimResolution::candidates() const
    -> std::vector<ResourceClaimResolutionInputs::Candidate> {
    return storage_ == nullptr ? std::vector<ResourceClaimResolutionInputs::Candidate>{}
                               : storage_->candidates;
}

auto resolveResourceClaims(const ResourceClaimResolutionInputs& inputs)
    -> core::Result<ResourceClaimResolution> {
    if (inputs.declaredCapacity != 1U) {
        return core::unexpected(capabilityRejection(
            codes::kResourceCapacityUnsupportedCode, codes::kCapabilityDisabledCategory,
            codes::kGraphSection, codes::kCapacityPath,
            "Stage 7A admits only a capacity 1 exclusive resource",
            codes::kResourceCapacityCapabilityId, codes::kLaterBatchRemediation));
    }
    if (!inputs.candidates.empty() && !inputs.intents.empty() &&
        inputs.candidates.size() != inputs.intents.size()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kResourceClaimPath,
            "legacy intents and prepared candidates describe different counts"));
    }

    std::vector<ResourceClaimResolutionInputs::Candidate> candidates = inputs.candidates;
    if (candidates.empty()) {
        candidates.reserve(inputs.intents.size());
        for (const ResourceClaimIntent intent : inputs.intents) {
            candidates.push_back(ResourceClaimResolutionInputs::Candidate{
                .intent = intent, .policyToken = {}, .claimKeyToken = {}, .competition = {}});
        }
    }
    std::set<std::string> occupyingKeys;
    std::set<ClaimPolicyDeclaration::CompetitionKey> competitionKeys;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        auto& candidate = candidates[index];
        if (!inputs.intents.empty() && candidate.intent != inputs.intents[index]) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection,
                std::string{codes::kResourceClaimPath} + "[" + std::to_string(index) + "]",
                "legacy intent and prepared candidate disagree"));
        }
        if (candidate.graceOverrideMode != GraceOverrideMode::none) {
            return core::unexpected(capabilityRejection(
                codes::kResourceHandoffUnsupportedCode, codes::kCapabilityDisabledCategory,
                codes::kGraphSection,
                std::string{codes::kResourceClaimPath} + "[" + std::to_string(index) + "]." +
                    std::string{codes::kGraceOverridePath},
                "sticky and observing grace overrides are outside the Stage 7A resource plan",
                codes::kResourceHandoffCapabilityId, codes::kLaterBatchRemediation));
        }
        if (candidate.intent == ResourceClaimIntent::observe) {
            if (candidate.competition.has_value() || !candidate.claimKeyToken.empty()) {
                return core::unexpected(declarationInvalidError(
                    codes::kResourceClaimConflictCode, codes::kGraphSection,
                    codes::kResourceClaimPath, "observe cannot declare an occupying claim key"));
            }
            continue;
        }
        if (candidate.policyToken.empty() || candidate.claimKeyToken.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection,
                std::string{codes::kResourceClaimPath} + "[" + std::to_string(index) + "]",
                "an occupying candidate requires a policy token and stable claim key"));
        }
        if (!occupyingKeys.insert(candidate.claimKeyToken).second) {
            return core::unexpected(declarationInvalidError(
                codes::kResourceClaimConflictCode, codes::kGraphSection,
                std::string{codes::kResourceClaimPath} + "[" + std::to_string(index) + "]",
                "two occupying candidates share a stable claim key"));
        }
        if (candidate.competition.has_value() &&
            !competitionKeys.insert(*candidate.competition).second) {
            return core::unexpected(declarationInvalidError(
                codes::kResourceClaimConflictCode, codes::kGraphSection, codes::kResourceClaimPath,
                "occupying candidates have a duplicate competition key"));
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const ResourceClaimResolutionInputs::Candidate& left,
                 const ResourceClaimResolutionInputs::Candidate& right) {
                  if (left.competition != right.competition) {
                      return left.competition < right.competition;
                  }
                  if (left.claimKeyToken != right.claimKeyToken) {
                      return left.claimKeyToken < right.claimKeyToken;
                  }
                  if (left.policyToken != right.policyToken) {
                      return left.policyToken < right.policyToken;
                  }
                  return static_cast<std::uint8_t>(left.intent) <
                         static_cast<std::uint8_t>(right.intent);
              });
    auto storage = std::make_shared<detail::ResourceClaimResolutionStorage>();
    storage->candidates = std::move(candidates);
    return ResourceClaimResolution{std::move(storage)};
}

} // namespace cuexis::judgement
