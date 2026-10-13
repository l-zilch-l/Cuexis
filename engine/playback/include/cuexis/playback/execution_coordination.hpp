#pragma once

#include <cuexis/playback/gameplay_candidate.hpp>
#include <cuexis/playback/realtime_clock.hpp>
#include <variant>

namespace cuexis::playback {
CUEXIS_ABI_WARNING_PUSH
struct GameplayExecutionContext final {
    std::uint64_t sessionScope, clockSegment, runEpoch;
    friend auto operator==(GameplayExecutionContext, GameplayExecutionContext) -> bool = default;
};
struct GameplayOccurrence final {
    GameplayInput input;
    std::uint64_t sourceLocalPosition;
};
struct GameplayClosedBatch final {
    GameplayTick originalTick, observedTick;
    std::uint64_t batchId, fragmentIndex, fragmentCount, totalMembers;
    std::vector<GameplayOccurrence> members;
    bool epochTerminated{};
    std::uint64_t pauseEpoch{};
};
struct GameplayCapturePrefix final {
    GameplayTick exclusiveTick;
};
struct GameplayEpochEnd final {
    GameplayTick inclusiveTick;
    std::uint64_t pauseEpoch;
};
struct GameplayCaptureDelivery final {
    GameplayExecutionContext context;
    std::uint64_t deliverySequence;
    std::variant<GameplayClosedBatch, GameplayCapturePrefix, GameplayEpochEnd> payload;
};
struct GameplayAdvanceRequest final {
    GameplayExecutionContext context;
    std::uint64_t requestId, commandOrdinal;
    GameplayTick horizon;
};
enum class GameplayAdvanceDisposition : std::uint8_t {
    Waiting,
    BusyNotAccepted,
    CanceledNotExecuted,
    ClockInvalidNotExecuted,
    Retired,
    Executed,
    Failed
};
struct GameplayAdvanceDecision final {
    GameplayAdvanceRequest request;
    GameplayAdvanceDisposition disposition;
    std::uint64_t decisionOrdinal;
    std::optional<GameplayTick> verifiedExclusive;
    GameplayResult result;
    std::optional<core::Error> error;
};
struct GameplayPauseTransition final {
    std::uint64_t sequence;
    GameplayOccurrence occurrence;
};
struct GameplayReconciliation final {
    GameplayTick cutTick;
    std::uint64_t controlOrdinal, pauseEpoch, finalTraceCut;
    std::vector<GameplayPauseTransition> trace;
};
struct GameplayExecutionLag final {
    GameplayTick clockHorizon;
    std::optional<std::int64_t> horizonLag, frontierLag;
    std::optional<GameplayTick> foldedWorkTick;
    std::uint64_t sealedFactCursor, foldedFactCursor;
};
CUEXIS_ABI_WARNING_POP
} // namespace cuexis::playback
