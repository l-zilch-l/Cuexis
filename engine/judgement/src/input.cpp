//  Judgement typed kernel - S7A-2 input normalization implementation.
//
//  This file is the input half of S7A-2: the typed AmountSpec and its exact integer quantization,
//  the session-owned InputMapping profile, the entry boundary that captures observationTick once
//  from the calibrated session clock, and the explicit CM-T10 behaviors.
//
//  Four rules shape every function below.
//
//    1. observationTick has exactly one canonical source. normalizeObservation takes the calibrated
//       session clock as its own argument and it is the only thing that can become the tick. The
//       raw device, host, audio and render timestamps travel in the declaration, reach the
//       diagnostic context and have no route back into a tick (Spec 3.7.3).
//
//    2. Nothing approximates. An incoming quantity is an exact rational and the quantization
//    divides
//       it by the declared scale exactly, so the canonical integer comes out of exact integer
//       arithmetic with one round-half-to-even step. No float and no host math library take part
//       (Spec 3.7.6 item 2).
//
//    3. Duplication is decided by the canonical observation identity. The canonical subject this
//       file derives has no ingress ordinal member at all, so the duplicate-queue verdict cannot
//       depend on arrival order (Spec 3.7.4 item 4).
//
//    4. Everything a declaration cannot honour is a stable rejection. No function saturates,
//       truncates, clamps or falls back to a default quantity, and no exception crosses the
//       boundary: every failure path returns core::Result with an owning diagnostic.

#include <cuexis/judgement/input_boundary.hpp>

#include "ingress_transaction.hpp"
#include "source_codes.hpp"

#include <cuexis/judgement/diagnostic.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace cuexis::judgement {
namespace {

constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();

//  |value| as an unsigned magnitude. Safe for INT64_MIN, whose magnitude needs the unsigned range.
[[nodiscard]] constexpr auto magnitude(std::int64_t value) noexcept -> std::uint64_t {
    return value < 0 ? (~static_cast<std::uint64_t>(value) + 1U)
                     : static_cast<std::uint64_t>(value);
}

//  left * right, or nullopt when the exact product is not representable.
[[nodiscard]] constexpr auto checkedMultiply(std::int64_t left, std::int64_t right) noexcept
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
[[nodiscard]] auto rejection(const DiagnosticTokens& tokens) -> core::Error {
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
[[nodiscard]] auto declarationInvalidError(std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = codes::kInputMappingInvalidCode,
                                      .category = codes::kInvalidRelationCategory,
                                      .summary = summary,
                                      .path = path});
}

//  A declared value left the domain its declaration requires, or the canonical integer cannot hold
//  it. Spec 3.7.6 item 3 and Spec 3.7.8 put both in budget_exceeded.
[[nodiscard]] auto amountValueError(std::string_view code, std::string_view path,
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
[[nodiscard]] auto capabilityUnsupportedError(std::string_view code, std::string_view path,
                                              std::string_view summary, std::string_view rawTime)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kCapabilityDisabledCategory,
                                      .summary = summary,
                                      .path = path,
                                      .capabilityId = codes::kContinuousCapabilityId,
                                      .remediation = codes::kContinuousRemediation,
                                      .rawTime = rawTime});
}

//  A session component that a running session may not change (CM-T09).
[[nodiscard]] auto runtimeMutationError(std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = codes::kRuntimeMappingChangeCode,
                                      .category = codes::kInvalidRelationCategory,
                                      .summary = summary,
                                      .path = path});
}

//  A late-policy condition: a duplicated queue entry or a duplicated ingress ordinal. Spec 3.7.4
//  item 4 and Spec 3.7.8 put both in late_policy_incomplete.
[[nodiscard]] auto lateRejectionError(std::string_view code, std::string_view summary,
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
[[nodiscard]] auto timeReversalError(std::string_view rawTime) -> core::Error {
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
[[nodiscard]] auto sameTickCollisionError(std::string_view rawTime) -> core::Error {
    return rejection(DiagnosticTokens{
        .code = codes::kSameTickCollisionCode,
        .category = codes::kInvalidRelationCategory,
        .summary = "a canonically identical observation was already admitted at this tick",
        .path = codes::kSameTickCollisionPath,
        .rawTime = rawTime});
}

//  Renders the raw timestamps into one opaque diagnostic token. They are context, never a second
//  time semantic: nothing reads this text back into a tick.
[[nodiscard]] auto describeRawTime(const RawIngressTimestamps& timestamps) -> std::string {
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
[[nodiscard]] constexpr auto isUsableSpec(const AmountSpec& spec) noexcept -> bool {
    return spec.scale.numerator() > 0 && spec.minimum <= spec.maximum;
}

//  The public amount factories produce a reduced rational, so the denominator is positive and the
//  numerator is never INT64_MIN. The check stays in place anyway, because a future change to that
//  representation must not silently become undefined behaviour here.
[[nodiscard]] constexpr auto isRepresentableRational(std::int64_t numerator,
                                                     std::int64_t denominator) noexcept -> bool {
    return numerator != kInt64Min && denominator > 0;
}

//  The round-half-to-even step of the quantization, applied once to the exact ratio
//  numerator / denominator. Preconditions established by quantizeAmount: denominator > 0 and
//  numerator != INT64_MIN.
[[nodiscard]] constexpr auto roundHalfToEvenRatio(std::int64_t numerator,
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
[[nodiscard]] auto deriveCanonicalSubject(const NormalizedObservation& observation) noexcept
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
[[nodiscard]] auto resolveDomain(const InputMappingProfile& mapping, std::string_view statedToken)
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
[[nodiscard]] auto
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
[[nodiscard]] auto
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
[[nodiscard]] auto firstDifferingIdentityPath(const InputMappingProfile& current,
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
[[nodiscard]] auto collidesAtTick(const std::vector<CanonicalIngressSubject>& admitted,
                                  const CanonicalIngressSubject& candidate) noexcept -> bool {
    for (const CanonicalIngressSubject& subject : admitted) {
        if (subject == candidate) {
            return true;
        }
    }
    return false;
}

//  Whether an ingress ordinal was already admitted by this session.
[[nodiscard]] auto sequenceAdmitted(const std::vector<IngressSequence>& admitted,
                                    IngressSequence candidate) noexcept -> bool {
    for (const IngressSequence& sequence : admitted) {
        if (sequence == candidate) {
            return true;
        }
    }
    return false;
}

} // namespace

//  ---------------------------------------------------------------------------------------------
//  ChannelRef / SourceClass
//  ---------------------------------------------------------------------------------------------

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

//  ---------------------------------------------------------------------------------------------
//  The entry boundary (Spec 3.7.3, CM-T05, CM-T10)
//  ---------------------------------------------------------------------------------------------

namespace {
auto buildNormalizedEntry(const InputMappingProfile& mapping, ObservationTick sessionClock,
                          const IngressDeclaration& declaration, ObservationId id,
                          bool resolveDomains) -> core::Result<NormalizedObservationEntry> {
    //  The mapping is re-validated here, so an entry on a mapping that validateInputMapping would
    //  refuse is a stable rejection rather than undefined behaviour.
    const auto mappingStatus = validateInputMapping(mapping);
    if (!mappingStatus.has_value()) {
        return core::unexpected(mappingStatus.error());
    }

    const std::string rawTime = describeRawTime(declaration.rawTimestamps);

    //  Continuous input capability is a stable 7A rejection that points at S7B-1 (Spec 3.7.7).
    if (declaration.continuity != ContinuityKind::discrete) {
        return core::unexpected(capabilityUnsupportedError(
            codes::kInputContinuousUnsupportedCode, codes::kContinuityPath,
            "continuous input capability is not enabled in Stage 7A", rawTime));
    }

    //  A crossing, a reconnect or a dropped sample has no 7A representation either: Spec 3.7.7 puts
    //  the discontinuity representation in the same "continuous input capability" row as the
    //  trajectory and rolls its admission into S7B-1, so the explicit CM-T10 behavior for crossing
    //  a sampling hole is the same R-05 capability rejection. It keeps the frozen R-05 code and
    //  capability id and is distinguished from a declared trajectory by its field path, because the
    //  ABI code vocabulary lists no discontinuity code and this batch adds no ABI code.
    if (declaration.discontinuity.crossedSamplingGap || declaration.discontinuity.reconnected ||
        declaration.discontinuity.droppedSamples) {
        return core::unexpected(capabilityUnsupportedError(
            codes::kInputContinuousUnsupportedCode, codes::kDiscontinuityPath,
            "discontinuity representation is not enabled in Stage 7A", rawTime));
    }

    //  The calibrated session clock is the only source of observationTick (Spec 3.7.3). A negative
    //  value is representable in the frozen signed 64-bit tick domain and is admitted; only a
    //  reversal is rejected below.
    const ObservationTick tick = sessionClock;

    std::optional<NormalizedAmount> amount;
    std::string_view domainToken{};
    if (declaration.quantity.has_value() || resolveDomains) {
        const auto domain = resolveDomain(mapping, declaration.domainToken);
        if (!domain.has_value()) {
            return core::unexpected(domain.error());
        }
        if (declaration.quantity) {
            const auto quantized = quantizeAmount((*domain)->amount, *declaration.quantity);
            if (!quantized) {
                return core::unexpected(quantized.error());
            }
            amount = NormalizedAmount{.value = *quantized, .domainToken = (*domain)->domainToken};
        }
        //  The quantized amount borrows the specification from the session's mapping profile, the
        //  same non-owning prepared-view convention the ABI fixes.
        domainToken = (*domain)->domainToken;
    }

    const NormalizedObservation observation{
        .observationId = id,
        .observationTick = tick,
        .ingressSequence = declaration.ingressSequence,
        .domainToken = domainToken,
        .action = declaration.action,
        .channel = declaration.channel,
        //  The source class is the session's declared device class: the mapping decides which class
        //  a session is bound to, and the canonical observation carries it.
        .sourceClass = mapping.sourceClass,
        .amount = amount,
    };

    //  The canonical subject is derived here and never supplied by a caller. It carries no ingress
    //  ordinal, which is the type-level statement of Spec 3.7.4 item 4.
    const CanonicalIngressSubject subject = deriveCanonicalSubject(observation);

    return NormalizedObservationEntry{.subject = subject, .observation = observation};
}
} // namespace

auto detail::prepareIngressBatch(const SessionIngressState& state,
                                 const InputMappingProfile& mapping,
                                 std::span<const ClockedIngress> batch, bool resolveDomains)
    -> core::Result<IngressJournal> try {
    IngressJournal journal{
        {}, {}, state.lastObservedTick_, state.nextObservationId_, state.observationIdsExhausted_};
    journal.entries.reserve(batch.size());
    journal.owners.reserve(batch.size());
    // Sequence duplication has precedence over canonical and Tick collisions throughout the batch.
    std::vector<IngressSequence> sequences;
    for (const auto& item : batch) {
        auto sequence = item.declaration.ingressSequence;
        if (sequenceAdmitted(state.admittedSequences_, sequence) ||
            sequenceAdmitted(sequences, sequence)) {
            return core::unexpected(lateRejectionError(
                codes::kIngressSequenceDuplicateCode, "this ingress sequence was already admitted",
                codes::kIngressSequencePath, describeRawTime(item.declaration.rawTimestamps)));
        }
        sequences.push_back(sequence);
    }
    for (const auto& item : batch) {
        if (resolveDomains && item.declaration.action != InputAction::press &&
            item.declaration.action != InputAction::release &&
            item.declaration.action != InputAction::update) {
            return core::unexpected(
                declarationInvalidError(codes::kDomainTokenPath, "unknown input action"));
        }
        auto entry = buildNormalizedEntry(mapping, item.observationTick, item.declaration,
                                          ObservationId{}, resolveDomains);
        if (!entry) {
            return core::unexpected(entry.error());
        }
        if (item.declaration.channel.token().empty()) {
            return core::unexpected(
                declarationInvalidError(codes::kDomainTokenPath, "empty channel"));
        }
        auto owner = std::make_unique<const OwnedIngressSubject>(
            OwnedIngressSubject{std::string{entry->observation.domainToken},
                                std::string{entry->observation.channel.token()},
                                std::string{entry->observation.sourceClass.token()}});
        entry->observation.domainToken = owner->domain;
        entry->observation.channel = *ChannelRef::fromToken(owner->channel);
        entry->observation.sourceClass = *SourceClass::fromToken(owner->source);
        if (entry->observation.amount) {
            entry->observation.amount->domainToken = owner->domain;
        }
        entry->subject = deriveCanonicalSubject(entry->observation);
        journal.owners.push_back(std::move(owner));
        journal.entries.push_back(*entry);
    }
    for (std::size_t i = 0; i < journal.entries.size(); ++i) {
        const auto& candidate = journal.entries[i].subject;
        const auto duplicate = [&]() {
            return resolveDomains ? lateRejectionError(codes::kDuplicateQueueCode,
                                                       "canonical subject was already queued",
                                                       codes::kCanonicalQueueKeyPath, {})
                                  : sameTickCollisionError({});
        };
        if (collidesAtTick(state.admittedSubjects_, candidate)) {
            return core::unexpected(duplicate());
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (journal.entries[j].subject == candidate) {
                return core::unexpected(duplicate());
            }
        }
    }
    if (resolveDomains) {
        for (std::size_t i = 0; i < journal.entries.size(); ++i) {
            auto tick = journal.entries[i].subject.observationTick;
            for (const auto& subject : state.admittedSubjects_) {
                if (tick == subject.observationTick) {
                    return core::unexpected(sameTickCollisionError({}));
                }
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (tick == journal.entries[j].subject.observationTick) {
                    return core::unexpected(sameTickCollisionError({}));
                }
            }
        }
    }
    for (const auto& item : batch) {
        if (state.lastObservedTick_ && item.observationTick < *state.lastObservedTick_) {
            return core::unexpected(
                timeReversalError(describeRawTime(item.declaration.rawTimestamps)));
        }
        if (!journal.latestTick || *journal.latestTick < item.observationTick) {
            journal.latestTick = item.observationTick;
        }
    }
    std::sort(journal.entries.begin(), journal.entries.end(), [](const auto& l, const auto& r) {
        const auto key = [](const auto& v) {
            return std::tuple{v.observationTick,       v.domainToken, v.sourceClass.token(),
                              v.channel.token(),       v.action,      v.hasAmount,
                              v.amountCanonicalInteger};
        };
        return key(l.subject) < key(r.subject);
    });
    for (auto& entry : journal.entries) {
        if (journal.exhausted) {
            return core::unexpected(
                declarationInvalidError(codes::kDomainTokenPath, "observation ID exhausted"));
        }
        entry.observation.observationId = ObservationId{journal.nextId};
        if (journal.nextId == UINT64_MAX) {
            journal.exhausted = true;
        } else {
            ++journal.nextId;
        }
    }
    return journal;
} catch (const std::exception&) {
    return core::unexpected(
        declarationInvalidError(codes::kDomainTokenPath, "ingress journal allocation failed"));
}

auto detail::reserveIngressBatch(SessionIngressState& state, const IngressJournal& journal)
    -> core::Result<void> try {
    const auto count = journal.entries.size();
    if (count > state.admittedSequences_.max_size() - state.admittedSequences_.size() ||
        count > state.admittedSubjects_.max_size() - state.admittedSubjects_.size() ||
        count > state.ownedSubjects_.max_size() - state.ownedSubjects_.size()) {
        return core::unexpected(
            declarationInvalidError(codes::kDomainTokenPath, "ingress storage size overflow"));
    }
    state.admittedSequences_.reserve(state.admittedSequences_.size() + count);
    state.admittedSubjects_.reserve(state.admittedSubjects_.size() + count);
    state.ownedSubjects_.reserve(state.ownedSubjects_.size() + count);
    return {};
} catch (const std::exception&) {
    return core::unexpected(
        declarationInvalidError(codes::kDomainTokenPath, "ingress reservation failed"));
}

void detail::commitIngressBatch(SessionIngressState& state, IngressJournal&& journal) noexcept {
    for (auto& owner : journal.owners) {
        state.ownedSubjects_.push_back(std::move(owner));
    }
    for (const auto& entry : journal.entries) {
        state.admittedSequences_.push_back(entry.observation.ingressSequence);
        state.admittedSubjects_.push_back(entry.subject);
    }
    state.lastObservedTick_ = journal.latestTick;
    state.nextObservationId_ = journal.nextId;
    state.observationIdsExhausted_ = journal.exhausted;
}

auto normalizeObservation(SessionIngressState& state, const InputMappingProfile& mapping,
                          ObservationTick tick, const IngressDeclaration& declaration)
    -> core::Result<NormalizedObservationEntry> {
    const ClockedIngress input{tick, declaration};
    auto journal = detail::prepareIngressBatch(state, mapping, std::span{&input, 1}, false);
    if (!journal) {
        return core::unexpected(journal.error());
    }
    auto reserved = detail::reserveIngressBatch(state, *journal);
    if (!reserved) {
        return core::unexpected(reserved.error());
    }
    const auto result = journal->entries.front();
    detail::commitIngressBatch(state, std::move(*journal));
    return result;
}

//  ---------------------------------------------------------------------------------------------
//  Late-policy admission (Spec 3.7.4 item 4, CM-T10)
//  ---------------------------------------------------------------------------------------------

auto admitLateQueueEntry(const CanonicalIngressSubject& alreadyQueued,
                         const CanonicalIngressSubject& candidate) -> core::Result<void> {
    if (!(alreadyQueued == candidate)) {
        return {};
    }
    return core::unexpected(lateRejectionError(
        codes::kDuplicateQueueCode,
        "the canonical observation identity is already queued, so it is not queued again",
        codes::kCanonicalQueueKeyPath, codes::kAbsent));
}

auto hasDistinctIdentityAtSameTick(const CanonicalIngressSubject& queued,
                                   const CanonicalIngressSubject& candidate) noexcept -> bool {
    if (queued.observationTick != candidate.observationTick) {
        return false;
    }
    return !(queued == candidate);
}

} // namespace cuexis::judgement
