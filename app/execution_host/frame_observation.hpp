#pragma once
#include "frame_storage.hpp"
#include "host_protocol.hpp"
#include <atomic>
#include <cuexis/playback/experimental/frame_layout.hpp>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace cuexis::player {
class ExecutionHost;
}
namespace cuexis::playback::detail {
// A resize uses the retained owning values, never the current Runtime or a new Fact prefix.
[[nodiscard]] auto rematerializeRetainedSnapshot(const FrameSnapshot&, FrameViewport)
    -> core::Result<FrameSnapshot>;
// One owning frame value. The variant keeps the legacy snapshot and the committed laid-out frame
// in the same payload slot, so no path stores both or copies the frame object.
struct FramePayload final {
    // Declared first, destroyed last: storage credit is returned after physical arrays/strings.
    StorageReservation storage;
    std::variant<FrameSnapshot, experimental::LaidOutFrame> frame;
    std::vector<PortableResourcePtr> resources;
    std::vector<FeedbackId> membership;
    [[nodiscard]] auto snapshotView() const noexcept -> const FrameSnapshot& {
        return std::visit(
            []<typename Value>(const Value& value) -> const FrameSnapshot& {
                if constexpr (std::is_same_v<Value, FrameSnapshot>)
                    return value;
                else
                    return value.snapshot();
            },
            frame);
    }
    [[nodiscard]] auto layoutView() const noexcept -> const experimental::FrameLayout* {
        const auto* laidOut = std::get_if<experimental::LaidOutFrame>(&frame);
        return laidOut != nullptr ? &laidOut->layout() : nullptr;
    }
    [[nodiscard]] auto laidOutView() const noexcept -> const experimental::LaidOutFrame* {
        return std::get_if<experimental::LaidOutFrame>(&frame);
    }
};
enum class DispatchState : std::uint8_t { ready, transferred, notSubmitted };
struct DispatchToken final {
    std::atomic<DispatchState> state{DispatchState::ready};
    // Immutable dispatch metadata: the original request deadline. Zero means no deadline was
    // attached (legacy callers and hand-built test attempts).
    std::int64_t deadlineMonotonicUs{};
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
            return payload->snapshotView();
        return retainedSnapshot ? *retainedSnapshot : snapshot;
    }
    // The committed layout of a laid-out frame; empty for a legacy snapshot frame.
    [[nodiscard]] auto layoutView() const noexcept -> const experimental::FrameLayout* {
        return payload ? payload->layoutView() : nullptr;
    }
    [[nodiscard]] auto laidOutView() const noexcept -> const experimental::LaidOutFrame* {
        return payload ? payload->laidOutView() : nullptr;
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
enum class PresentStatus : std::uint8_t { succeeded, failed, unknown, dropped };
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
[[nodiscard]] inline auto sameFrameTarget(const experimental::FrameTarget& left,
                                          const experimental::FrameTarget& right) noexcept -> bool {
    return left.extent.width == right.extent.width && left.extent.height == right.extent.height &&
           left.host.x == right.host.x && left.host.y == right.host.y &&
           left.host.width == right.host.width && left.host.height == right.host.height &&
           left.rotation == right.rotation;
}
[[nodiscard]] inline auto staleAttempt(std::string_view reason) -> core::Error {
    return core::Error{"host.rpa.context_stale", "The frame attempt is no longer current"}
        .withContext("reason", std::string{reason});
}
[[nodiscard]] inline auto layoutUnsupported(std::string_view reason) -> core::Error {
    return core::Error{"playback.presentation.layout.unsupported",
                       "This renderer path cannot submit a laid-out frame"}
        .withContext("reason", std::string{reason});
}
// The renderer adapter owns this call. This bridge never advances Playback or Gameplay. The
// current target is required by a laid-out frame and forbidden for a legacy snapshot frame. A
// stale deadline or changed target ends the attempt without clearing or drawing: the receipt is
// dropped, no submit/present ran, and the host keeps its previous retained presentation.
template <typename Renderer, typename MonotonicNow>
[[nodiscard]] auto presentOwnedFrame(const FrameEnvelope& frame, const HostContext& expected,
                                     std::uint64_t scope, std::uint64_t rendererInstance,
                                     Renderer& renderer, MonotonicNow&& now,
                                     std::optional<experimental::FrameTarget> currentTarget)
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
        const auto entered = now();
        receipt.callMonotonicUs = entered;
        if (frame.dispatch && frame.dispatch->deadlineMonotonicUs &&
            !(entered < frame.dispatch->deadlineMonotonicUs)) {
            receipt.status = PresentStatus::dropped;
            receipt.cause = staleAttempt("frame_deadline");
            return receipt;
        }
        const auto* laidOut = frame.laidOutView();
        if (laidOut != nullptr) {
            if (!currentTarget) {
                return core::unexpected(layoutUnsupported("target_absent"));
            }
            if (!sameFrameTarget(laidOut->layout().target, *currentTarget)) {
                receipt.status = PresentStatus::dropped;
                receipt.cause = staleAttempt("frame_target_changed");
                return receipt;
            }
            if constexpr (requires { renderer.submitLaidOut(*laidOut, *currentTarget); }) {
                auto submitted = renderer.submitLaidOut(*laidOut, *currentTarget);
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
            } else {
                return core::unexpected(layoutUnsupported("no_bridge"));
            }
        }
        if (currentTarget) {
            return core::unexpected(layoutUnsupported("legacy_with_target"));
        }
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
    } catch (const std::bad_alloc&) {
        // The operation did not complete. Report the actual failure stage instead of claiming
        // success or an unknown present; a bad_alloc remains distinguishable from other faults.
        receipt.status = PresentStatus::failed;
        receipt.cause = core::Error{"host.rpa.allocation_failed",
                                    "Renderer submit or present could not allocate"};
        return receipt;
    } catch (const std::exception& exception) {
        receipt.status = PresentStatus::failed;
        receipt.cause =
            core::Error{"host.rpa.device_failed", "Renderer submit or present failed"}.withContext(
                "exception", exception.what());
        return receipt;
    } catch (...) {
        receipt.status = PresentStatus::failed;
        receipt.cause = core::Error{"host.rpa.device_failed", "Renderer submit or present failed"};
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
#if defined(CUEXIS_FRAME_OBSERVATION_FAULT_INJECTION)
    // Test seam: the prebuilt owning error is exposed so the copy cost can be measured rather
    // than presumed safe.
    [[nodiscard]] auto allocationFailureTemplateForTest() const noexcept -> const core::Error& {
        return allocationFailure_;
    }
#endif

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
    void reportAllocationFailure(FaultSlot::Stage stage) const noexcept {
        fault_ = {FaultSlot::Reason::allocation, {}, stage};
    }
    void reportCapacityFailure(FaultSlot::Stage stage) const noexcept {
        fault_ = {FaultSlot::Reason::capacity, {}, stage};
    }
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
    mutable core::Error allocationFailure_{"host.rpa.allocation_failed",
                                           "Frame observation operation could not allocate"};
};
#if defined(CUEXIS_FRAME_OBSERVATION_FAULT_INJECTION)
// Test-only switches consumed by the exact production allocation branch they fault. They live in
// the test closure only; no alternative implementation is compiled into it.
void armFrameObservationAllocationFault(FaultSlot::Stage) noexcept;
void armFrameObservationCapacityFault(FaultSlot::Stage) noexcept;
[[nodiscard]] auto takeFrameObservationInjectedFault(FaultSlot::Stage) noexcept -> bool;
[[nodiscard]] auto frameObservationInjectedFaultAttempts() noexcept -> std::uint64_t;
#endif
} // namespace cuexis::playback::detail
