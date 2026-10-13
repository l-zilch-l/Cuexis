#include "rpa_b1_fixture.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <sstream>
#include <tuple>

namespace cuexis::rpa::testing {
namespace {
using Wide = __int128_t;

auto gcdWide(Wide a, Wide b) -> Wide {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        const Wide remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

auto fitsInt64(Wide value) -> bool {
    return value >= static_cast<Wide>(std::numeric_limits<std::int64_t>::min()) &&
           value <= static_cast<Wide>(std::numeric_limits<std::int64_t>::max());
}

auto checkedRational(Wide numerator, Wide denominator) -> std::optional<Rational> {
    if (denominator == 0) {
        return {};
    }
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    const Wide divisor = gcdWide(numerator, denominator);
    numerator /= divisor;
    denominator /= divisor;
    if (!fitsInt64(numerator) || !fitsInt64(denominator)) {
        return {};
    }
    return Rational(static_cast<std::int64_t>(numerator), static_cast<std::int64_t>(denominator));
}
} // namespace

TraceRecorder::TraceRecorder(std::uint64_t sessionScope, std::uint64_t runEpoch,
                             std::uint64_t clockSegment)
    : context_{.sessionScope = sessionScope, .runEpoch = runEpoch, .clockSegment = clockSegment} {}

auto TraceRecorder::nextIdentity(PipelineStage stage, EventKind kind,
                                 std::optional<std::uint64_t> commandOrdinal,
                                 std::optional<std::uint64_t> decisionOrdinal) -> EventIdentity {
    return EventIdentity{.sessionScope = context_.sessionScope,
                         .runEpoch = context_.runEpoch,
                         .clockSegment = context_.clockSegment,
                         .deliverySequence = nextDeliverySequence_++,
                         .stage = stage,
                         .kind = kind,
                         .commandOrdinal = commandOrdinal.value_or(nextCommandOrdinal_++),
                         .decisionOrdinal = decisionOrdinal.value_or(nextDecisionOrdinal_++)};
}

auto TraceRecorder::record(PipelineStage stage, EventKind kind, std::string_view subject,
                           std::int64_t tick, std::string_view payload,
                           std::optional<std::uint64_t> commandOrdinal,
                           std::optional<std::uint64_t> decisionOrdinal) -> const TraceEvent& {
    events_.push_back(
        TraceEvent{.identity = nextIdentity(stage, kind, commandOrdinal, decisionOrdinal),
                   .subject = std::string(subject),
                   .tick = tick,
                   .payload = std::string(payload)});
    return events_.back();
}

auto TraceRecorder::events() const -> const std::vector<TraceEvent>& {
    return events_;
}

auto EvidenceManifest::validate() const -> bool {
    return !sourceHead.empty() && !dirtyDiffHash.empty() && !semanticRevision.empty() &&
           !configuration.empty() && !contentIdentity.empty() && !hardwareIdentity.empty();
}

auto EvidenceManifest::canonicalText() const -> std::string {
    std::ostringstream output;
    output << "source_head=" << sourceHead << '\n'
           << "dirty_diff_hash=" << dirtyDiffHash << '\n'
           << "semantic_revision=" << semanticRevision << '\n'
           << "configuration=" << configuration << '\n'
           << "content_identity=" << contentIdentity << '\n'
           << "hardware_identity=" << hardwareIdentity << '\n'
           << "missing_sample_count=" << missingSampleCount << '\n'
           << "observed_event_count=" << observedEventCount << '\n'
           << "observation_overhead_ns=" << observationOverheadNanoseconds << '\n';
    return output.str();
}

FakeClock::FakeClock(std::int64_t startMonotonicUs) : nowUs_(startMonotonicUs) {}

auto FakeClock::now() const -> std::int64_t {
    return nowUs_;
}

auto FakeClock::segment() const -> std::uint64_t {
    return segment_;
}

auto FakeClock::advance(std::int64_t deltaUs) -> bool {
    const Wide candidate = static_cast<Wide>(nowUs_) + deltaUs;
    if (deltaUs < 0 || !fitsInt64(candidate)) {
        return false;
    }
    nowUs_ = static_cast<std::int64_t>(candidate);
    return true;
}

auto FakeClock::observe(std::int64_t validForUs, std::int64_t errorBoundUs) -> ClockObservation {
    const Wide validUntil = static_cast<Wide>(nowUs_) + validForUs;
    const bool bounded = validForUs >= 0 && errorBoundUs >= 0 && fitsInt64(validUntil);
    return ClockObservation{.segment = segment_,
                            .publicationSequence = publicationSequence_++,
                            .sampleMonotonicUs = nowUs_,
                            .validUntilMonotonicUs =
                                bounded ? static_cast<std::int64_t>(validUntil) : nowUs_,
                            .errorBoundUs = errorBoundUs,
                            .valid = valid_ && bounded};
}

auto FakeClock::invalidate() -> void {
    valid_ = false;
}

auto FakeClock::startNewSegment() -> void {
    ++segment_;
    valid_ = true;
    ++publicationSequence_;
}

auto ControlledDelivery::enqueue(std::uint64_t canonicalOrdinal, std::int64_t captureTick,
                                 std::string_view payload) -> std::uint64_t {
    const auto sequence = nextDeliverySequence_++;
    pending_.push_back(DeliveryMessage{.deliverySequence = sequence,
                                       .canonicalOrdinal = canonicalOrdinal,
                                       .captureTick = captureTick,
                                       .payload = std::string(payload)});
    return sequence;
}

auto ControlledDelivery::deliverInOrder(const std::vector<std::uint64_t>& deliverySequences)
    -> std::vector<DeliveryMessage> {
    std::vector<DeliveryMessage> delivered;
    delivered.reserve(deliverySequences.size());
    for (const auto sequence : deliverySequences) {
        const auto found = std::find_if(pending_.begin(), pending_.end(),
                                        [sequence](const DeliveryMessage& message) {
                                            return message.deliverySequence == sequence;
                                        });
        if (found == pending_.end()) {
            continue;
        }
        delivered.push_back(*found);
        pending_.erase(found);
    }
    return delivered;
}

auto ControlledDelivery::pending() const -> const std::vector<DeliveryMessage>& {
    return pending_;
}

auto Rational::create(std::int64_t numerator, std::int64_t denominator) -> std::optional<Rational> {
    return checkedRational(numerator, denominator);
}

auto Rational::numerator() const -> std::int64_t {
    return numerator_;
}

auto Rational::denominator() const -> std::int64_t {
    return denominator_;
}

auto add(const Rational& left, const Rational& right) -> std::optional<Rational> {
    return checkedRational(static_cast<Wide>(left.numerator_) * right.denominator_ +
                               static_cast<Wide>(right.numerator_) * left.denominator_,
                           static_cast<Wide>(left.denominator_) * right.denominator_);
}

auto multiply(const Rational& left, const Rational& right) -> std::optional<Rational> {
    return checkedRational(static_cast<Wide>(left.numerator_) * right.numerator_,
                           static_cast<Wide>(left.denominator_) * right.denominator_);
}

auto roundHalfEven(const Rational& value) -> std::optional<std::int64_t> {
    const Wide numerator = value.numerator();
    const Wide denominator = value.denominator();
    const Wide quotient = numerator / denominator;
    const Wide remainder = numerator % denominator;
    const Wide absoluteRemainder = remainder < 0 ? -remainder : remainder;
    const Wide twiceRemainder = absoluteRemainder * 2;
    Wide rounded = quotient;
    if (twiceRemainder > denominator || (twiceRemainder == denominator && (quotient % 2 != 0))) {
        rounded += numerator < 0 ? -1 : 1;
    }
    if (!fitsInt64(rounded)) {
        return {};
    }
    return static_cast<std::int64_t>(rounded);
}

auto mapTime(const TimeOracleInput& input) -> std::optional<TimeOracleResult> {
    if (input.sourceRate <= 0) {
        return {};
    }
    const auto sourceUs =
        checkedRational(static_cast<Wide>(input.sourceFrame) * 1000000, input.sourceRate);
    if (!sourceUs) {
        return {};
    }
    const auto observedOffset =
        checkedRational(-static_cast<Wide>(input.timingOffsetUs) - input.inputCompensationUs, 1);
    const auto presentedOffset =
        checkedRational(-static_cast<Wide>(input.timingOffsetUs) + input.displayLeadUs, 1);
    if (!observedOffset || !presentedOffset) {
        return {};
    }
    const auto observed = add(*sourceUs, *observedOffset);
    const auto presented = add(*sourceUs, *presentedOffset);
    if (!observed || !presented) {
        return {};
    }
    const auto sessionOffset = checkedRational(-static_cast<Wide>(input.timingOffsetUs), 1);
    const auto sessionTime =
        sessionOffset ? add(*sourceUs, *sessionOffset) : std::optional<Rational>{};
    const auto horizonTick =
        sessionTime ? roundHalfEven(*sessionTime) : std::optional<std::int64_t>{};
    const auto observationTick = roundHalfEven(*observed);
    const auto presentationTick = roundHalfEven(*presented);
    if (!observationTick || !presentationTick || !horizonTick) {
        return {};
    }
    return TimeOracleResult{.observationTick = *observationTick,
                            .horizonTick = *horizonTick,
                            .presentationTick = *presentationTick};
}

auto scanExecutionOracle(const std::vector<OracleInput>& inputs,
                         const std::vector<std::int64_t>& observedTicks,
                         std::int64_t prefixExclusiveTick) -> OracleResult {
    OracleResult result;
    std::vector<OracleInput> ordered = inputs;
    std::sort(ordered.begin(), ordered.end(),
              [](const OracleInput& left, const OracleInput& right) {
                  return std::tie(left.originalTick, left.canonicalOrdinal) <
                         std::tie(right.originalTick, right.canonicalOrdinal);
              });
    for (const auto observedTick : observedTicks) {
        const bool closesBatch =
            std::any_of(ordered.begin(), ordered.end(), [observedTick](const OracleInput& input) {
                return input.originalTick < observedTick;
            });
        if (closesBatch) {
            result.closedBatchTicks.push_back(observedTick);
        }
    }
    for (const auto& input : ordered) {
        if (input.originalTick >= prefixExclusiveTick || !input.admitted) {
            continue;
        }
        result.executionOrder.push_back(input.canonicalOrdinal);
        result.facts.push_back("fact:" + std::to_string(input.canonicalOrdinal));
        result.folds.push_back("fold:" + std::to_string(input.originalTick) + ":" +
                               std::to_string(input.canonicalOrdinal));
    }
    return result;
}

} // namespace cuexis::rpa::testing
