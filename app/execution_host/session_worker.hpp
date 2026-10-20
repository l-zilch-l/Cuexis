#pragma once

#include "frame_delivery.hpp"
#include "frame_observation.hpp"
#include "host_candidate.hpp"
#include "owner_channel.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <variant>

namespace cuexis::playback::detail {
struct SessionPrepareSource final {
    PlaybackSource source;
    PlaybackMode mode{PlaybackMode::ChartClock};
    PresentationCapabilities capabilities;
    PresentationRequest presentation;
    std::uint64_t rendererInstance{}, rendererGeneration{}, controlSequence{};
    std::optional<ReloadPolicy> reload;
    std::optional<GameplayContent> gameplay;
    RuntimeFrame targetFrame{};
};
struct SessionAcceptControl final {
    ControlRequest request;
};
struct SessionSoftwarePrepare final {
    StepRequest request;
};
struct SessionSoftwareFence final {
    StepRequest request;
    ControlReceipt proof;
};
struct SessionSoftwareCommit final {
    StepRequest request;
    ControlReceipt proof;
};
struct SessionRealtimeStart final {
    SchedulingProfile profile;
    ClockMappingConfiguration mapping;
    GameplayExecutionContext context;
    std::string monotonicDomain;
};
struct SessionCaptureDelivery final {
    GameplayCaptureDelivery delivery;
    ClockObservation clock;
};
struct SessionAdvance final {
    GameplayAdvanceRequest request;
    ClockObservation clock;
};
struct SessionCoherentAdvance final {
    GameplayExecutionContext context;
    std::uint64_t requestId{}, commandOrdinal{};
    ClockObservation clock;
    friend auto operator==(const SessionCoherentAdvance&, const SessionCoherentAdvance&)
        -> bool = default;
};
struct SessionQueryAdvance final {
    std::uint64_t requestId{};
};
struct SessionTailStart final {
    ClockObservation eof;
    std::uint64_t segment{};
    TailProfile profile;
};
struct SessionRetireAdvance final {
    std::uint64_t requestId{};
};
struct SessionPause final {
    StepRequest request;
    ControlReceipt proof;
};
struct SessionResume final {
    StepRequest request;
    ControlReceipt proof;
    GameplayReconciliation reconciliation;
    GameplayExecutionContext context;
    ClockObservation clock;
    StepReceipt routeReceipt;
};
struct SessionConfirmControl final {
    ControlReceipt proof;
    std::uint64_t projectionScope{};
};
struct SessionSample final {
    HostContext context;
    std::uint64_t projectionScope{}, frameId{};
    std::int64_t presentationTick{};
    RuntimeFrame runtimeFrame{};
    FrameViewport viewport{};
    bool realtime{};
    bool frozen{};
    RetainedPresentation retained;
};
struct SessionPresentReceipt final {
    PresentReceipt receipt;
};
struct SessionUpdatePresentation final {
    HostContext context;
    std::uint64_t projectionScope{};
    std::int64_t presentationTick{};
    RuntimeFrame runtimeFrame;
    bool realtime{};
};
struct FrameDeliveryLayout final {
    std::size_t bytes{}, objects{}, members{};
};
struct SessionDeliveryLayout final {};
struct SessionRegisterConsumer final {
    std::uint64_t operationId{};
    HostContext context;
    ConsumerProfile profile;
};
struct SessionConfirmConsumer final {
    RegistrationReceipt receipt;
    bool revoke{};
};
struct SessionRevokeConsumer final {
    std::uint64_t consumerId{};
};
struct SessionRequestFrame final {
    FrameRequest request;
};
struct SessionRevokeRetained final {
    std::uint64_t consumerId{}, serial{};
};
struct SessionFeedbackQuery final {
    FeedbackId id;
};
struct SessionObservationQuery final {};
struct ObservationInfo final {
    std::size_t retainedAttempts{}, bytes{}, peakBytes{};
    std::uint64_t retainedFirst{}, retainedLast{}, lossEvents{};
    std::vector<FrameObservationLedger::CoverageInterval> incomplete;
    bool exactLossCount{}, attemptsEvicted{}, feedbackEvicted{}, receiptLoss{}, historyMisses{};
};
struct SessionDeliveryMetrics final {};
struct SessionArchive final {};
struct SessionSnapshot final {};
struct SessionEvaluateReplay final {
    GameplayReplay archive;
};
struct SessionLag final {
    GameplayTick horizon;
};
struct SessionArchiveCut final {
    GameplayReplay archive;
    GameplayTick horizon{};
};
struct SessionSeek final {
    StepRequest request;
    ControlReceipt proof;
    GameplayReplay archive;
    GameplayTick horizon{};
    GameplayReplayCut cut{};
    std::vector<GameplayCheckpoint> checkpoints;
    GameplayPresentationTick presentationTick{};
    RuntimeFrame runtimeFrame{};
};
struct SessionSoftwareRetire final {
    StepRequest request;
    ControlReceipt proof;
};
struct SessionSoftwareClose final {
    StepRequest request;
    ControlReceipt proof;
};
struct SessionCancelStep final {
    StepRequest request;
};
struct SessionQueryStep final {
    StepKey key;
};
struct SessionInspect final {};
struct SessionGameplayResult final {};
struct SessionInspection final {
    SessionState state{};
    std::uint64_t generation{};
    std::thread::id ownerThread;
    std::optional<std::uint64_t> reconciliationOrdinal;
    std::uint64_t sessionScope{};
};
using SessionCommand = std::variant<
    SessionInspect, SessionGameplayResult, SessionPrepareSource, SessionAcceptControl,
    SessionSoftwarePrepare, SessionSoftwareFence, SessionSoftwareCommit, SessionRealtimeStart,
    SessionCaptureDelivery, SessionAdvance, SessionCoherentAdvance, SessionTailStart,
    SessionQueryAdvance, SessionRetireAdvance, SessionPause, SessionResume, SessionConfirmControl,
    SessionSample, SessionPresentReceipt, SessionUpdatePresentation, SessionDeliveryLayout,
    SessionRegisterConsumer, SessionConfirmConsumer, SessionRevokeConsumer, SessionRequestFrame,
    SessionRevokeRetained, SessionDeliveryMetrics, SessionFeedbackQuery, SessionObservationQuery,
    SessionArchive, SessionSnapshot, SessionEvaluateReplay, SessionLag, SessionArchiveCut, SessionSeek,
    SessionSoftwareRetire, SessionSoftwareClose, SessionCancelStep, SessionQueryStep>;
using SessionReply =
    std::variant<std::monostate, PresentationCandidateInput, std::uint64_t, StepReceipt,
                 GameplayAdvanceDecision, FrameEnvelope, GameplayReplay, GameplayReplayCut,
                 SessionInspection, TailClockTransition, GameplayReplayEvaluation, GameplaySnapshot,
                 GameplayExecutionLag, GameplayResult, ConsumerRegistration, DeliveryInfo,
                 FrameDeliveryLayout, StorageMetrics, FeedbackObservation, ObservationInfo>;
struct SessionWorkerConfiguration final {
    std::size_t capacity{}, controlReserve{}, stepCapacity{}, stepReserve{}, frameCapacity{},
        feedbackCapacity{};
    bool testOnly{};
    // One fixed owner clock, not a callback supplied by each job.
    std::function<std::int64_t()> monotonicNow;
    // Explicit test-only assembly limits. No accepted production defaults are implied.
    DeliveryLimits delivery{8, 8, 64U * 1024U * 1024U, 32};
    std::size_t maxFrameMembers{4096}, receiptCapacity{128};
    FrameObservationLedger::Limits observation;
};
class SessionWorker final {
  public:
    [[nodiscard]] static auto create(SessionWorkerConfiguration)
        -> core::Result<std::unique_ptr<SessionWorker>>;
    ~SessionWorker() noexcept;
    SessionWorker(const SessionWorker&) = delete;
    auto operator=(const SessionWorker&) -> SessionWorker& = delete;
    [[nodiscard]] auto post(SessionCommand, bool control = false) -> core::Result<std::uint64_t>;
    [[nodiscard]] auto poll(std::uint64_t)
        -> core::Result<std::optional<core::Result<SessionReply>>>;
    [[nodiscard]] auto acknowledge(std::uint64_t) -> core::Result<void>;
    [[nodiscard]] auto takeDelivery(std::uint64_t consumerId, std::uint64_t requestId)
        -> core::Result<std::optional<FrameEnvelope>>;
    [[nodiscard]] auto pollDelivery(std::uint64_t consumerId, std::uint64_t requestId)
        -> core::Result<DeliveryInfo>;
    [[nodiscard]] auto cancelDelivery(std::uint64_t consumerId, std::uint64_t requestId)
        -> core::Result<DeliveryInfo>;
    // Bounded observation-only inbox; failure records coverage loss, never a reliable-channel
    // fault.
    void postReceipt(const PresentReceipt&) noexcept;
    [[nodiscard]] auto storageMetrics() const -> StorageMetrics {
        return delivery_->storageMetrics();
    }
    void waitFor(std::chrono::milliseconds duration);
    void waitForReply(std::uint64_t id, std::chrono::steady_clock::duration duration);
    void requestShutdown() noexcept;
    [[nodiscard]] auto exited() const noexcept -> bool;
    [[nodiscard]] auto joinExited() -> core::Result<void>;
    [[nodiscard]] auto fault() const noexcept -> OwnerChannel<SessionCommand, SessionReply>::Fault;

  private:
    explicit SessionWorker(SessionWorkerConfiguration);
    void run() noexcept;
    SessionWorkerConfiguration configuration_;
    OwnerChannel<SessionCommand, SessionReply> channel_;
    std::shared_ptr<FrameDelivery> delivery_;
    std::mutex receiptMutex_;
    std::vector<std::optional<PresentReceipt>> receipts_;
    std::size_t receiptRead_{}, receiptWrite_{}, receiptCount_{};
    struct ReceiptLoss final {
        std::uint64_t sessionFirst{}, sessionLast{}, scopeFirst{}, scopeLast{}, frameFirst{},
            frameLast{};
    };
    std::array<std::optional<ReceiptLoss>, 8> receiptLosses_;
    void noteLostReceipt(const PresentReceipt&) noexcept;
    std::atomic<std::uint64_t> lostReceipts_{};
    std::atomic<bool> shutdown_{}, exited_{};
    std::thread thread_;
};
} // namespace cuexis::playback::detail
