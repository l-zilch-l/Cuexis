#pragma once
#include "frame_storage.hpp"
#include "host_protocol.hpp"
#include <atomic>

namespace cuexis::player {
class ExecutionHost;
}
namespace cuexis::playback::detail {
// A resize uses the retained owning values, never the current Runtime or a new Fact prefix.
[[nodiscard]] auto rematerializeRetainedSnapshot(const FrameSnapshot&, FrameViewport)
    -> core::Result<FrameSnapshot>;
struct FramePayload final {
    // Declared first, destroyed last: storage credit is returned after physical arrays/strings.
    StorageReservation storage;
    FrameSnapshot snapshot;
    std::vector<PortableResourcePtr> resources;
    std::vector<FeedbackId> membership;
};
enum class DispatchState : std::uint8_t { ready, transferred, notSubmitted };
struct DispatchToken final {
    std::atomic<DispatchState> state{DispatchState::ready};
};
struct FrameAttemptLease final {
    StorageReservation storage;
    [[nodiscard]] static auto create(StorageReservation)
        -> std::shared_ptr<const FrameAttemptLease>;
};
struct FrameEnvelope final {
    HostContext context;
    std::uint64_t frameId{}, projectionScope{};
    std::optional<std::uint64_t> foldedFactCursor;
    std::optional<std::int64_t> presentationTick;
    std::optional<std::int64_t> foldedWorkTick;
    std::optional<RuntimeFrame> runtimeFrame;
    FrameSnapshot snapshot;
    // Retained attempts alias their original owning snapshot; snapshot is empty in that case.
    std::shared_ptr<const FrameSnapshot> retainedSnapshot;
    std::optional<PresentationSourceInfo> source;
    std::vector<PortableResourcePtr> resources;
    std::vector<FeedbackId> membership;
    std::shared_ptr<const FramePayload> payload;
    std::shared_ptr<const FrameAttemptLease> attemptLease;
    std::uint64_t consumerId{}, requestId{}, retainedSerial{}, retainedFrameId{};
    std::shared_ptr<DispatchToken> dispatch;
    [[nodiscard]] auto snapshotView() const noexcept -> const FrameSnapshot& {
        if (payload)
            return payload->snapshot;
        return retainedSnapshot ? *retainedSnapshot : snapshot;
    }
    [[nodiscard]] auto membershipView() const noexcept -> const std::vector<FeedbackId>& {
        return payload ? payload->membership : membership;
    }
    [[nodiscard]] auto resourcesView() const noexcept -> const std::vector<PortableResourcePtr>& {
        return payload ? payload->resources : resources;
    }
};
// Only the presentation owner can mint this handle, after an actual successful present.
class RetainedPresentation final {
  public:
    RetainedPresentation() = default;
    [[nodiscard]] auto frame() const noexcept -> const std::shared_ptr<const FrameEnvelope>& {
        return frame_;
    }
    [[nodiscard]] auto serial() const noexcept -> std::uint64_t {
        return serial_;
    }

  private:
    friend class cuexis::player::ExecutionHost;
    std::shared_ptr<const FrameEnvelope> frame_;
    std::uint64_t serial_{};
};
enum class PresentStatus : std::uint8_t { succeeded, failed, unknown };
struct PresentReceipt final {
    HostContext context;
    std::uint64_t frameId{}, projectionScope{};
    PresentStatus status{};
    std::int64_t callMonotonicUs{};
    std::optional<core::Error> cause;
};
// Copy the complete structured error into bounded owning storage; never retain a foreign cause
// tree.
[[nodiscard]] auto copyPresentReceipt(const PresentReceipt&) -> core::Result<PresentReceipt>;
// The renderer adapter owns this call. This bridge never advances Playback or Gameplay.
template <typename Renderer, typename MonotonicNow>
[[nodiscard]] auto presentOwnedFrame(const FrameEnvelope& frame, const HostContext& expected,
                                     std::uint64_t scope, std::uint64_t rendererInstance,
                                     Renderer& renderer, MonotonicNow&& now)
    -> core::Result<PresentReceipt> {
    PresentReceipt receipt{
        frame.context, frame.frameId, frame.projectionScope, PresentStatus::unknown, 0, {}};
    try {
        const auto identity = renderer.identity();
        if (frame.context != expected || frame.projectionScope != scope || !frame.frameId ||
            !rendererInstance || identity.instance != rendererInstance ||
            identity.generation != frame.context.rendererGeneration)
            return core::unexpected(core::Error{"host.rpa.context_stale",
                                                "Frame does not belong to the active renderer"});
        receipt.callMonotonicUs = now();
        auto submitted = renderer.submit(frame.snapshotView());
        if (!submitted) {
            receipt.status = PresentStatus::failed;
            receipt.cause = submitted.error();
            return receipt;
        }
        receipt.callMonotonicUs = now();
        auto presented = renderer.present();
        receipt.status = presented ? PresentStatus::succeeded : PresentStatus::failed;
        if (!presented)
            receipt.cause = presented.error();
        return receipt;
    } catch (...) {
        receipt.status = PresentStatus::unknown;
        receipt.cause = core::Error{"host.rpa.device_failed", "Renderer call outcome is unknown"};
        return receipt;
    }
}
class FrameObservationLedger final {
  public:
    [[nodiscard]] static constexpr auto frameSlotStorageBytes() noexcept -> std::size_t {
        return sizeof(std::optional<FrameRecord>);
    }
    struct CoverageInterval final {
        std::uint64_t first{}, last{};
    };
    struct CoverageView final {
        std::span<const CoverageInterval> incomplete;
        std::uint64_t retainedFirst{}, retainedLast{}, lossEvents{};
        bool exactLossCount{}, attemptsEvicted{}, feedbackEvicted{}, receiptLoss{}, historyMisses{};
    };
    [[nodiscard]] auto coverage() const noexcept -> CoverageView;
    struct Limits final {
        std::size_t bytes{8U * 1024U * 1024U}, tombstones{128}, coverageIntervals{8}, members{4096};
        std::int64_t retentionUs{60'000'000}, receiptDeadlineUs{5'000'000};
    };
    [[nodiscard]] static auto create(HostContext, std::uint64_t projectionScope,
                                     std::size_t frameCapacity, std::size_t feedbackCapacity)
        -> core::Result<FrameObservationLedger>;
    [[nodiscard]] auto configure(Limits) -> core::Result<void>;
    void maintain(std::int64_t monotonicNow) noexcept;
    [[nodiscard]] auto journalBytes() const noexcept -> std::size_t {
        return journalBytes_;
    }
    [[nodiscard]] auto peakJournalBytes() const noexcept -> std::size_t {
        return peakJournalBytes_;
    }
    FrameObservationLedger(FrameObservationLedger&&) noexcept = default;
    FrameObservationLedger(const FrameObservationLedger&) = delete;
    [[nodiscard]] auto feedback(FeedbackId, std::int64_t created,
                                std::optional<GameplayPresentationTick> expiry)
        -> core::Result<void>;
    [[nodiscard]] auto feedback(FeedbackId id, std::int64_t created, std::int64_t end)
        -> core::Result<void> {
        return feedback(std::move(id), created, GameplayPresentationTick{end});
    }
    [[nodiscard]] auto publish(FrameEnvelope) -> core::Result<void>;
    [[nodiscard]] auto recordAttempt(const FrameEnvelope&) -> core::Result<void>;
    [[nodiscard]] auto publishRetained(FrameEnvelope, const RetainedPresentation&)
        -> core::Result<void>;
    [[nodiscard]] auto takeLatest() -> core::Result<std::optional<FrameEnvelope>>;
    [[nodiscard]] auto observe(const PresentReceipt&) -> core::Result<void>;
    [[nodiscard]] auto expire(std::int64_t presentationTick) -> core::Result<void>;
    [[nodiscard]] auto activate(HostContext, std::uint64_t projectionScope) -> core::Result<void>;
    // Called only after the software owner has validated the complete confirmed context.
    // A control-only transition keeps the same feedback identities and source-time frontier.
    [[nodiscard]] auto rebind(HostContext) -> core::Result<void>;
    [[nodiscard]] auto query(const FeedbackId&) const -> core::Result<FeedbackObservation>;
    [[nodiscard]] auto unknownCount() const noexcept -> std::uint64_t {
        return unknownCount_;
    }
    [[nodiscard]] auto fault() const noexcept -> const FaultSlot& {
        return fault_;
    }
    // Observation loss never stops the execution or owns payload lifetime.
    void coverageLost() noexcept;
    void historyMiss() noexcept {
        historyMisses_ = true;
        recordMissing();
    }
    void receiptLost(std::uint64_t sessionFirst, std::uint64_t sessionLast,
                     std::uint64_t scopeFirst, std::uint64_t scopeLast, std::uint64_t frameFirst,
                     std::uint64_t frameLast) noexcept;
    [[nodiscard]] auto retainedAttempts() const noexcept -> std::size_t;
    [[nodiscard]] auto historyUnavailable() const noexcept -> bool {
        return historyLost_;
    }

  private:
    [[nodiscard]] auto publishImpl(FrameEnvelope, bool retained) -> core::Result<void>;
    struct FrameRecord final {
        HostContext context;
        std::uint64_t frameId{}, projectionScope{};
        std::vector<FeedbackId> membership;
        std::optional<PresentReceipt> receipt;
        bool submitted{};
        std::shared_ptr<DispatchToken> dispatch;
        std::uint64_t consumerId{}, requestId{}, resultVersion{};
        std::optional<std::int64_t> sourceTick;
        std::int64_t receivedAt{}, insertedAt{};
        std::size_t bytes{}, receiptStorageBytes{};
    };
    FrameObservationLedger(HostContext, std::uint64_t, std::vector<std::optional<FrameRecord>>,
                           std::vector<std::optional<FeedbackObservation>>);
    void recordMissing() noexcept;
    void dropAttempt(std::size_t) noexcept;
    void dropFeedback(std::size_t) noexcept;
    void trim(std::size_t needed) noexcept;
    void rememberLoss(std::uint64_t first, std::uint64_t last) noexcept;
    core::ThreadChecker owner_;
    HostContext context_;
    std::uint64_t scope_, lastFrame_{}, unknownCount_{};
    std::optional<std::int64_t> lastTick_;
    std::vector<std::optional<FrameRecord>> frames_;
    std::vector<std::optional<FeedbackObservation>> feedback_;
    Limits limits_;
    std::vector<std::optional<FeedbackObservation>> tombstones_;
    std::vector<std::int64_t> feedbackTimes_, tombstoneTimes_;
    using Coverage = CoverageInterval;
    std::vector<Coverage> coverage_;
    std::size_t tombstoneNext_{}, journalBytes_{}, peakJournalBytes_{};
    std::int64_t now_{};
    std::size_t nextFrame_{}, nextFeedback_{};
    std::optional<std::size_t> latest_;
    std::optional<FrameEnvelope> latestFrame_;
    bool historyLost_{}, attemptsEvicted_{}, feedbackEvicted_{}, receiptLoss_{}, historyMisses_{};
    std::optional<std::int64_t> feedbackLossThrough_;
    std::uint64_t feedbackLossCommitThrough_{};
    mutable FaultSlot fault_;
    mutable core::Error allocationFailure_{"host.rpa.capacity_exceeded",
                                           "Reserved frame observation allocation fault"};
};
} // namespace cuexis::playback::detail
