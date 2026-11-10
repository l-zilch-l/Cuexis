#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::rpa::testing {

enum class PipelineStage {
    capture,
    delivery,
    admission,
    seal,
    fold,
    publish,
    consume,
    present,
    control,
};

enum class EventKind {
    input,
    batchClosed,
    prefix,
    control,
    fact,
    fold,
    frame,
    presentReceipt,
};

struct EventIdentity final {
    std::uint64_t sessionScope{};
    std::uint64_t runEpoch{};
    std::uint64_t clockSegment{};
    std::uint64_t deliverySequence{};
    PipelineStage stage{PipelineStage::capture};
    EventKind kind{EventKind::input};
    std::uint64_t commandOrdinal{};
    std::uint64_t decisionOrdinal{};

    auto operator==(const EventIdentity&) const -> bool = default;
};

struct TraceEvent final {
    EventIdentity identity{};
    std::string subject;
    std::int64_t tick{};
    std::string payload;
};

class TraceRecorder final {
  public:
    TraceRecorder(std::uint64_t sessionScope, std::uint64_t runEpoch, std::uint64_t clockSegment);

    auto record(PipelineStage stage, EventKind kind, std::string_view subject, std::int64_t tick,
                std::string_view payload = {}, std::optional<std::uint64_t> commandOrdinal = {},
                std::optional<std::uint64_t> decisionOrdinal = {}) -> const TraceEvent&;

    auto events() const -> const std::vector<TraceEvent>&;

  private:
    EventIdentity nextIdentity(PipelineStage stage, EventKind kind,
                               std::optional<std::uint64_t> commandOrdinal,
                               std::optional<std::uint64_t> decisionOrdinal);

    EventIdentity context_{};
    std::uint64_t nextDeliverySequence_{};
    std::uint64_t nextCommandOrdinal_{};
    std::uint64_t nextDecisionOrdinal_{};
    std::vector<TraceEvent> events_;
};

struct EvidenceManifest final {
    std::string sourceHead;
    std::string dirtyDiffHash;
    std::string semanticRevision;
    std::string configuration;
    std::string contentIdentity;
    std::string hardwareIdentity;
    std::uint64_t missingSampleCount{};
    std::uint64_t observedEventCount{};
    std::uint64_t observationOverheadNanoseconds{};

    auto validate() const -> bool;
    auto canonicalText() const -> std::string;
};

struct ClockObservation final {
    std::uint64_t segment{};
    std::uint64_t publicationSequence{};
    std::int64_t sampleMonotonicUs{};
    std::int64_t validUntilMonotonicUs{};
    std::int64_t errorBoundUs{};
    bool valid{};
};

class FakeClock final {
  public:
    explicit FakeClock(std::int64_t startMonotonicUs = 0);

    auto now() const -> std::int64_t;
    auto segment() const -> std::uint64_t;
    auto advance(std::int64_t deltaUs) -> bool;
    auto observe(std::int64_t validForUs, std::int64_t errorBoundUs) -> ClockObservation;
    auto invalidate() -> void;
    auto startNewSegment() -> void;

  private:
    std::int64_t nowUs_{};
    std::uint64_t segment_{1};
    std::uint64_t publicationSequence_{};
    bool valid_{true};
};

struct DeliveryMessage final {
    std::uint64_t deliverySequence{};
    std::uint64_t canonicalOrdinal{};
    std::int64_t captureTick{};
    std::string payload;
};

class ControlledDelivery final {
  public:
    auto enqueue(std::uint64_t canonicalOrdinal, std::int64_t captureTick, std::string_view payload)
        -> std::uint64_t;
    auto deliverInOrder(const std::vector<std::uint64_t>& deliverySequences)
        -> std::vector<DeliveryMessage>;
    auto pending() const -> const std::vector<DeliveryMessage>&;

  private:
    std::uint64_t nextDeliverySequence_{};
    std::vector<DeliveryMessage> pending_;
};

class Rational final {
  public:
    Rational(std::int64_t numerator, std::int64_t denominator)
        : numerator_(numerator), denominator_(denominator) {}

    static auto create(std::int64_t numerator, std::int64_t denominator) -> std::optional<Rational>;

    auto numerator() const -> std::int64_t;
    auto denominator() const -> std::int64_t;
    auto operator==(const Rational&) const -> bool = default;

    friend auto add(const Rational&, const Rational&) -> std::optional<Rational>;
    friend auto multiply(const Rational&, const Rational&) -> std::optional<Rational>;

  private:
    std::int64_t numerator_;
    std::int64_t denominator_;
};

auto roundHalfEven(const Rational&) -> std::optional<std::int64_t>;

struct TimeOracleInput final {
    std::int64_t sourceFrame{};
    std::int64_t sourceRate{};
    std::int64_t timingOffsetUs{};
    std::int64_t inputCompensationUs{};
    std::int64_t displayLeadUs{};
};

struct TimeOracleResult final {
    std::int64_t observationTick{};
    std::int64_t horizonTick{};
    std::int64_t presentationTick{};
};

auto mapTime(const TimeOracleInput&) -> std::optional<TimeOracleResult>;

struct OracleInput final {
    std::uint64_t canonicalOrdinal{};
    std::int64_t originalTick{};
    bool admitted{};
};

struct OracleResult final {
    std::vector<std::int64_t> closedBatchTicks;
    std::vector<std::uint64_t> executionOrder;
    std::vector<std::string> facts;
    std::vector<std::string> folds;
};

auto scanExecutionOracle(const std::vector<OracleInput>& inputs,
                         const std::vector<std::int64_t>& observedTicks,
                         std::int64_t prefixExclusiveTick) -> OracleResult;

} // namespace cuexis::rpa::testing
