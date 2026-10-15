#pragma once
#include "host_protocol.hpp"

namespace cuexis::playback::detail {
struct FrameEnvelope final {
    HostContext context;
    std::uint64_t frameId{}, projectionScope{}, foldedFactCursor{};
    std::int64_t presentationTick{};
    std::optional<std::int64_t> foldedWorkTick;
    RuntimeFrame runtimeFrame;
    FrameSnapshot snapshot;
    std::vector<PortableResourcePtr> resources;
    std::vector<FeedbackId> membership;
};
enum class PresentStatus : std::uint8_t { succeeded, failed, unknown };
struct PresentReceipt final {
    HostContext context;
    std::uint64_t frameId{}, projectionScope{};
    PresentStatus status{};
    std::int64_t callMonotonicUs{};
    std::optional<core::Error> cause;
};
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
        auto submitted = renderer.submit(frame.snapshot);
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
    [[nodiscard]] static auto create(HostContext, std::uint64_t projectionScope,
                                     std::size_t frameCapacity, std::size_t feedbackCapacity)
        -> core::Result<FrameObservationLedger>;
    FrameObservationLedger(FrameObservationLedger&&) noexcept = default;
    FrameObservationLedger(const FrameObservationLedger&) = delete;
    [[nodiscard]] auto feedback(FeedbackId, std::int64_t created, std::int64_t expiry)
        -> core::Result<void>;
    [[nodiscard]] auto publish(FrameEnvelope) -> core::Result<void>;
    [[nodiscard]] auto takeLatest() -> core::Result<std::optional<FrameEnvelope>>;
    [[nodiscard]] auto observe(const PresentReceipt&) -> core::Result<void>;
    [[nodiscard]] auto expire(std::int64_t presentationTick) -> core::Result<void>;
    [[nodiscard]] auto activate(HostContext, std::uint64_t projectionScope) -> core::Result<void>;
    [[nodiscard]] auto query(const FeedbackId&) const -> core::Result<FeedbackObservation>;
    [[nodiscard]] auto unknownCount() const noexcept -> std::uint64_t {
        return unknownCount_;
    }
    [[nodiscard]] auto fault() const noexcept -> const FaultSlot& {
        return fault_;
    }

  private:
    struct FrameRecord final {
        FrameEnvelope frame;
        std::optional<PresentReceipt> receipt;
        bool submitted{};
    };
    FrameObservationLedger(HostContext, std::uint64_t, std::vector<std::optional<FrameRecord>>,
                           std::vector<std::optional<FeedbackObservation>>,
                           std::vector<std::uint64_t>);
    void recordMissing() noexcept;
    core::ThreadChecker owner_;
    HostContext context_;
    std::uint64_t scope_, lastFrame_{}, unknownCount_{};
    std::optional<std::int64_t> lastTick_;
    std::vector<std::optional<FrameRecord>> frames_;
    std::vector<std::optional<FeedbackObservation>> feedback_;
    std::vector<std::uint64_t> scopeHistory_;
    std::size_t scopeCount_{1};
    std::optional<std::size_t> latest_;
    mutable FaultSlot fault_;
    mutable core::Error allocationFailure_{"host.rpa.capacity_exceeded",
                                           "Reserved frame observation allocation fault"};
};
} // namespace cuexis::playback::detail
