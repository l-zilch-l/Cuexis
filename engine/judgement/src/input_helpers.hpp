// Internal input parsing and quantization helpers shared by the input units.
#pragma once

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

namespace cuexis::judgement::input_detail {

inline constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();
inline constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();

//  |value| as an unsigned magnitude. Safe for INT64_MIN, whose magnitude needs the unsigned range.
[[nodiscard]] inline constexpr auto magnitude(std::int64_t value) noexcept -> std::uint64_t {
    return value < 0 ? (~static_cast<std::uint64_t>(value) + 1U)
                     : static_cast<std::uint64_t>(value);
}

//  left * right, or nullopt when the exact product is not representable.
[[nodiscard]] inline constexpr auto checkedMultiply(std::int64_t left, std::int64_t right) noexcept
    -> std::optional<std::int64_t> {
    if (left == 0 || right == 0) {
        return std::int64_t{0};
    }
    const bool negative = (left < 0) != (right < 0);
    const std::uint64_t limit = negative ? static_cast<std::uint64_t>(kInt64Max) + 1U
                                         : static_cast<std::uint64_t>(kInt64Max);
    const std::uint64_t leftMagnitude = magnitude(left);
    const std::uint64_t rightMagnitude = magnitude(right);
    if (leftMagnitude > limit / rightMagnitude) {
        return std::nullopt;
    }
    const std::uint64_t product = leftMagnitude * rightMagnitude;
    if (!negative) {
        return static_cast<std::int64_t>(product);
    }
    if (product == static_cast<std::uint64_t>(kInt64Max) + 1U) {
        return kInt64Min;
    }
    return -static_cast<std::int64_t>(product);
}

//  ---------------------------------------------------------------------------------------------
//  Diagnostics
//  ---------------------------------------------------------------------------------------------

//  One rejection in the carrier form of <cuexis/judgement/diagnostic.hpp>. The category and the
//  code are separate tokens because the Spec owns the category set and this batch only refines
//  which value failed inside it.
struct DiagnosticTokens final {
    std::string_view code;
    std::string_view category;
    std::string_view summary;
    std::string_view path;
    std::string_view capabilityId = codes::kAbsent;
    std::string_view remediation = codes::kAbsent;
    std::string_view rawTime = codes::kAbsent;
};

//  Projects one rejection into the error channel. Not noexcept on purpose: the error owns copies of
//  every token, so building it allocates.
[[nodiscard]] inline auto rejection(const DiagnosticTokens& tokens) -> core::Error {
    const Diagnostic diagnostic{
        .code = tokens.code,
        .category = tokens.category,
        .severity = codes::kErrorSeverity,
        //  None of these rejections mutates session state, so none of them faults the session.
        .faulted = false,
        .summary = tokens.summary,
        .context =
            DiagnosticContext{
                .fieldPath =
                    DiagnosticFieldPath{.section = codes::kInputSection, .path = tokens.path},
                .requirement =
                    DiagnosticRequirementRef{.kind = codes::kAbsent, .identity = codes::kAbsent},
                .identity = DiagnosticIdentityComponent{.component = codes::kAbsent,
                                                        .token = codes::kAbsent},
                .budget = nullptr,
                .capabilityId = tokens.capabilityId,
                .remediation = tokens.remediation,
                .rawTime = tokens.rawTime,
            },
    };
    return toError(diagnostic);
}

//  A declaration is not the valid declaration it claims to be: a missing component or a
//  self-contradicting one. Spec 3.7.8 and Spec 9.3 put that case in invalid_relation.
[[nodiscard]] inline auto declarationInvalidError(std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = codes::kInputMappingInvalidCode,
                                      .category = codes::kInvalidRelationCategory,
                                      .summary = summary,
                                      .path = path});
}

//  A declared value left the domain its declaration requires, or the canonical integer cannot hold
//  it. Spec 3.7.6 item 3 and Spec 3.7.8 put both in budget_exceeded.
[[nodiscard]] inline auto amountValueError(std::string_view code, std::string_view path,
                                           std::string_view summary) -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kBudgetExceededCategory,
                                      .summary = summary,
                                      .path = path});
}

//  A capability is not enabled in Stage 7A. R-05 requires the rejection to name a capability id and
//  a replacement path, so both travel here (Spec 3.7.7, ABI code family "input"). The raw
//  timestamps of the refused event travel as diagnostic context, which is the only place they
//  belong. The category is `capability_disabled`: Spec 9.2 names no bare `capability` category, and
//  the trajectory capability is a recognised id this compile does not enable.
[[nodiscard]] inline auto capabilityUnsupportedError(std::string_view code, std::string_view path,
                                                     std::string_view summary,
                                                     std::string_view rawTime) -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kCapabilityDisabledCategory,
                                      .summary = summary,
                                      .path = path,
                                      .capabilityId = codes::kContinuousCapabilityId,
                                      .remediation = codes::kContinuousRemediation,
                                      .rawTime = rawTime});
}

//  A session component that a running session may not change (CM-T09).
[[nodiscard]] inline auto runtimeMutationError(std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = codes::kRuntimeMappingChangeCode,
                                      .category = codes::kInvalidRelationCategory,
                                      .summary = summary,
                                      .path = path});
}

//  A late-policy condition: a duplicated queue entry or a duplicated ingress ordinal. Spec 3.7.4
//  item 4 and Spec 3.7.8 put both in late_policy_incomplete.
[[nodiscard]] inline auto lateRejectionError(std::string_view code, std::string_view summary,
                                             std::string_view path, std::string_view rawTime)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kLatePolicyIncompleteCategory,
                                      .summary = summary,
                                      .path = path,
                                      .rawTime = rawTime});
}

//  The calibrated session clock moved backwards (ABI "lifecycle and failure invariants" item 4).
//  The raw timestamps travel along as context, which is where they belong (Spec 3.7.3 item 2).
//
//  The category is invalid_relation, not budget_exceeded: a clock regression is a time-order
//  relation error, which is what Spec 9.3 puts in invalid_relation, while budget_exceeded is
//  reserved for a numeric value that leaves a declared range or a budget. The code token and the
//  field path are unchanged, because only the category was misidentified.
[[nodiscard]] inline auto timeReversalError(std::string_view rawTime) -> core::Error {
    return rejection(DiagnosticTokens{
        .code = codes::kTimeReversalCode,
        .category = codes::kInvalidRelationCategory,
        .summary = "the calibrated session clock moved backwards at the judgement pipeline entry",
        .path = codes::kCalibratedClockPath,
        .rawTime = rawTime});
}

//  A canonically identical observation arrived at a tick this session already admitted: the tick is
//  the same, the action, channel, source class and amount are the same, and only the host
//  submission ordinal differs. The verdict is the canonical identity comparison, never the arrival
//  order, and a different canonical observation at the same tick is a second observation rather
//  than a collision (CM-T10).
[[nodiscard]] inline auto sameTickCollisionError(std::string_view rawTime) -> core::Error {
    return rejection(DiagnosticTokens{
        .code = codes::kSameTickCollisionCode,
        .category = codes::kInvalidRelationCategory,
        .summary = "a canonically identical observation was already admitted at this tick",
        .path = codes::kSameTickCollisionPath,
        .rawTime = rawTime});
}

//  Renders the raw timestamps into one opaque diagnostic token. They are context, never a second
//  time semantic: nothing reads this text back into a tick.
[[nodiscard]] inline auto describeRawTime(const RawIngressTimestamps& timestamps) -> std::string {
    return std::string{"device="} + std::to_string(timestamps.rawTicks) +
           " host=" + std::to_string(timestamps.hostArrivalTicks) +
           " audio=" + std::to_string(timestamps.audioFrameTicks) +
           " render=" + std::to_string(timestamps.renderFrameTicks);
}

//  ---------------------------------------------------------------------------------------------
//  Internal predicates
//  ---------------------------------------------------------------------------------------------

//  Whether the declaration itself is usable. A non-positive scale would make the quotient
//  meaningless and a reversed range would admit nothing, so both are declaration faults rather than
//  value faults.
[[nodiscard]] inline constexpr auto isUsableSpec(const AmountSpec& spec) noexcept -> bool {
    return spec.scale.numerator() > 0 && spec.minimum <= spec.maximum;
}

//  The public amount factories produce a reduced rational, so the denominator is positive and the
//  numerator is never INT64_MIN. The check stays in place anyway, because a future change to that
//  representation must not silently become undefined behaviour here.
[[nodiscard]] inline constexpr auto isRepresentableRational(std::int64_t numerator,
                                                            std::int64_t denominator) noexcept
    -> bool {
    return numerator != kInt64Min && denominator > 0;
}

//  The round-half-to-even step of the quantization, applied once to the exact ratio
//  numerator / denominator. Preconditions established by quantizeAmount: denominator > 0 and
//  numerator != INT64_MIN.
[[nodiscard]] inline constexpr auto roundHalfToEvenRatio(std::int64_t numerator,
                                                         std::int64_t denominator) noexcept
    -> std::optional<std::int64_t> {
    std::int64_t quotient = numerator / denominator;
    std::int64_t remainder = numerator % denominator;
    if (remainder < 0) {
        if (quotient == kInt64Min) {
            return std::nullopt;
        }
        --quotient;
        remainder += denominator;
    }
    const auto twice = static_cast<std::uint64_t>(remainder) * 2U;
    const auto denominatorMagnitude = static_cast<std::uint64_t>(denominator);
    if (twice < denominatorMagnitude) {
        return quotient;
    }
    if (twice > denominatorMagnitude) {
        if (quotient == kInt64Max) {
            return std::nullopt;
        }
        return quotient + 1;
    }
    //  An exact tie goes to the even side.
    if (quotient % 2 == 0) {
        return quotient;
    }
    if (quotient == kInt64Max) {
        return std::nullopt;
    }
    return quotient + 1;
}

//  The canonical identity of one normalized observation, derived here and never supplied by a
//  caller. The composition is exactly the canonical fields: the ingress ordinal is not a member, so
//  the two orderings the rulings forbid from deciding duplication are unrepresentable in the
//  comparison rather than merely ignored by it.
[[nodiscard]] inline auto deriveCanonicalSubject(const NormalizedObservation& observation) noexcept
    -> CanonicalIngressSubject {
    return CanonicalIngressSubject{
        .observationTick = observation.observationTick,
        .domainToken = observation.domainToken,
        .action = observation.action,
        .channel = observation.channel,
        .sourceClass = observation.sourceClass,
        .hasAmount = observation.amount.has_value(),
        .amountCanonicalInteger = observation.amount.has_value()
                                      ? observation.amount->value.canonicalInteger
                                      : std::int64_t{0},
    };
}

//  Resolves the domain an event belongs to. An event that states a domain gets it, provided the
//  mapping declares that token; an event that states none resolves only when the mapping declares
//  exactly one domain, and is refused otherwise instead of being guessed by table order.
[[nodiscard]] inline auto resolveDomain(const InputMappingProfile& mapping,
                                        std::string_view statedToken)
    -> core::Result<const InputDomainDeclaration*> {
    if (!statedToken.empty()) {
        for (const InputDomainDeclaration& declaration : mapping.domains) {
            if (declaration.domainToken == statedToken) {
                return &declaration;
            }
        }
        return core::unexpected(declarationInvalidError(
            codes::kDomainTokenPath, "the observation names an input domain the mapping does not "
                                     "declare"));
    }
    if (mapping.domains.size() == 1) {
        return &mapping.domains.front();
    }
    return core::unexpected(declarationInvalidError(
        codes::kMappingDomainsPath,
        "the observation states no input domain and the mapping does not declare exactly one"));
}

//  Whether a domain table repeats a token. A repeated token is not a valid declaration
//  (validateInputMapping refuses it instead of resolving it by position), so it is not a
//  well-defined declaration set either, and the identity comparison below reports "not the same
//  identity" rather than inventing a resolution order for it.
[[nodiscard]] inline auto
hasRepeatedDomainToken(const std::vector<InputDomainDeclaration>& domains) noexcept -> bool {
    for (std::size_t index = 0; index < domains.size(); ++index) {
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (domains[earlier].domainToken == domains[index].domainToken) {
                return true;
            }
        }
    }
    return false;
}

//  Whether two domain tables declare the same set of domains.
//
//  The normalization key is the domain token, compared with the lexicographic byte order of
//  std::string_view. This is the sorted-by-token comparison of the two declaration sets, expressed
//  without materializing a sorted copy: because the tokens are unique in a valid declaration, the
//  two tables carry the same declarations exactly when they have the same size and every token of
//  one table appears in the other with an AmountSpec that compares equal. That makes the result
//  independent of the declaration order, which is the point - the table's order carries no
//  semantics - and it keeps the predicate allocation-free.
//
//  The compared AmountSpec is the whole declared specification: scale, minimum, maximum and
//  boundaryPolicy. AmountSpec carries no separate quantization version field, so there is nothing
//  else a declaration could differ in.
[[nodiscard]] inline auto
sameDomainDeclarationSet(const std::vector<InputDomainDeclaration>& left,
                         const std::vector<InputDomainDeclaration>& right) noexcept -> bool {
    if (left.size() != right.size()) {
        return false;
    }
    if (hasRepeatedDomainToken(left) || hasRepeatedDomainToken(right)) {
        return false;
    }
    for (const InputDomainDeclaration& declaration : left) {
        const InputDomainDeclaration* match = nullptr;
        for (const InputDomainDeclaration& candidate : right) {
            if (candidate.domainToken == declaration.domainToken) {
                match = &candidate;
                break;
            }
        }
        if (match == nullptr || !(match->amount == declaration.amount)) {
            return false;
        }
    }
    return true;
}

//  The field path of the first session identity component that differs, so a refused runtime
//  mapping change names the component that actually changed instead of a fixed one. The order of
//  the checks is the declaration order of the components and carries no further meaning.
[[nodiscard]] inline auto firstDifferingIdentityPath(const InputMappingProfile& current,
                                                     const InputMappingProfile& candidate) noexcept
    -> std::string_view {
    if (current.profileId != candidate.profileId) {
        return codes::kMappingProfileIdPath;
    }
    if (current.profileVersion != candidate.profileVersion) {
        return codes::kMappingProfileVersionPath;
    }
    if (!(current.sourceClass == candidate.sourceClass)) {
        return codes::kMappingSourceClassPath;
    }
    return codes::kMappingDomainsPath;
}

//  Whether a tick was already admitted with a different canonical identity.
//  CM-T10, same-tick case: a second observation that is canonically identical to one this session
//  already admitted at that tick is a collision. The verdict is the canonical identity comparison
//  alone, which already carries the tick, the action, the channel, the source class and the amount;
//  it is never the ingress ordinal and never the arrival order. Two different canonical
//  observations that share a tick are two observations, and both are admitted.
[[nodiscard]] inline auto collidesAtTick(const std::vector<CanonicalIngressSubject>& admitted,
                                         const CanonicalIngressSubject& candidate) noexcept
    -> bool {
    for (const CanonicalIngressSubject& subject : admitted) {
        if (subject == candidate) {
            return true;
        }
    }
    return false;
}

//  Whether an ingress ordinal was already admitted by this session.
[[nodiscard]] inline auto sequenceAdmitted(const std::vector<IngressSequence>& admitted,
                                           IngressSequence candidate) noexcept -> bool {
    for (const IngressSequence& sequence : admitted) {
        if (sequence == candidate) {
            return true;
        }
    }
    return false;
}

} // namespace cuexis::judgement::input_detail
