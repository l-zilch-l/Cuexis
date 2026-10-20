#pragma once
#include "frame_observation.hpp"
#include <functional>
#include <mutex>

namespace cuexis::playback::detail {
enum class ConsumerMode : std::uint8_t { realtimeLatest, exactCapture };
enum class ConsumerState : std::uint8_t { registering, active, revoking, retired };
enum class DeliveryStatus : std::uint8_t {
    pending,
    ready,
    transferred,
    canceled,
    expiredRequest,
    skippedNoCredit,
    versionUnavailable,
    noRetainedFrame,
    historyUnavailable,
    rejected
};
struct ConsumerProfile final {
    ConsumerMode mode{ConsumerMode::realtimeLatest};
    bool renderer{};
    std::size_t maxPayloads{}, maxBytes{}, frameByteBound{};
    std::uint64_t rendererInstance{}, rendererGeneration{};
    std::size_t maxAttempts{8};
    friend auto operator==(const ConsumerProfile&, const ConsumerProfile&) -> bool = default;
};
struct ConsumerRegistration final {
    std::uint64_t operationId{}, consumerId{};
    HostContext context;
    ConsumerProfile profile;
    ConsumerState state{ConsumerState::registering};
};
[[nodiscard]] auto sameRegistration(const ConsumerRegistration&,
                                    const ConsumerRegistration&) noexcept -> bool;
struct RegistrationReceipt final {
    ConsumerRegistration registration;
    bool active{};
};
struct FrameRequest final {
    std::uint64_t consumerId{}, requestId{}, projectionScope{};
    HostContext context;
    FrameViewport viewport;
    std::optional<std::uint64_t> exactVersion;
    std::int64_t deadlineMonotonicUs{};
    RetainedPresentation retained;
    bool frozen{};
};
struct DeliveryInfo final {
    std::uint64_t consumerId{}, requestId{}, resultVersion{}, frameId{};
    DeliveryStatus status{DeliveryStatus::pending};
};
// Metadata-only fingerprint; request history must not retain a RetainedPresentation/FramePayload.
struct DeliveryFingerprint final {
    std::uint64_t consumerId{}, requestId{}, projectionScope{}, retainedFrame{}, retainedSerial{};
    HostContext context;
    std::uint32_t width{}, height{};
    std::optional<std::uint64_t> exactVersion;
    std::int64_t deadline{};
    bool frozen{};
    friend auto operator==(const DeliveryFingerprint&, const DeliveryFingerprint&)
        -> bool = default;
};
[[nodiscard]] auto fingerprint(const FrameRequest&) -> DeliveryFingerprint;
struct DeliveryLimits final {
    std::size_t consumers{}, payloads{}, bytes{}, requestHistory{};
    std::size_t attempts{64};
};
struct DeliveryAdmission final {
    DeliveryInfo info;
    StorageReservation reservation;
    bool materialize{};
    StorageReservation attempt;
};
class FrameDelivery final {
  public:
    [[nodiscard]] static auto create(DeliveryLimits)
        -> core::Result<std::shared_ptr<FrameDelivery>>;
    [[nodiscard]] auto registerConsumer(std::uint64_t operationId, HostContext, ConsumerProfile)
        -> core::Result<ConsumerRegistration>;
    [[nodiscard]] auto confirmRegistration(const RegistrationReceipt&) -> core::Result<void>;
    [[nodiscard]] auto beginRevoke(std::uint64_t consumerId) -> core::Result<ConsumerRegistration>;
    [[nodiscard]] auto confirmRevoke(const RegistrationReceipt&) -> core::Result<void>;
    [[nodiscard]] auto registration(std::uint64_t consumerId) const
        -> core::Result<ConsumerRegistration>;
    [[nodiscard]] auto admit(const FrameRequest&, std::uint64_t currentVersion, std::int64_t now,
                             bool reusePayload) -> core::Result<DeliveryAdmission>;
    [[nodiscard]] auto publish(const FrameRequest&, FrameEnvelope, std::int64_t now)
        -> core::Result<DeliveryInfo>;
    [[nodiscard]] auto cancel(std::uint64_t consumerId, std::uint64_t requestId,
                              bool expired = false) -> core::Result<DeliveryInfo>;
    [[nodiscard]] auto finishRequest(std::uint64_t consumerId, std::uint64_t requestId,
                                     DeliveryStatus) -> core::Result<DeliveryInfo>;
    [[nodiscard]] auto poll(std::uint64_t consumerId, std::uint64_t requestId, std::int64_t now)
        -> core::Result<DeliveryInfo>;
    [[nodiscard]] auto take(std::uint64_t consumerId, std::uint64_t requestId, std::int64_t now)
        -> core::Result<std::optional<FrameEnvelope>>;
    void cancelRetained(std::uint64_t consumerId, std::uint64_t serial);
    void expire(std::int64_t now);
    void closeAdmission();
    [[nodiscard]] auto attemptMetrics() const -> StorageMetrics {
        return attemptStorage_->metrics();
    }
    [[nodiscard]] auto storageMetrics() const -> StorageMetrics {
        return storage_->metrics();
    }
    [[nodiscard]] auto transferred(std::uint64_t frameId) const -> bool;
    [[nodiscard]] auto notSubmitted(std::uint64_t frameId) const -> bool;

  private:
    struct RequestRecord final {
        DeliveryFingerprint fingerprint;
        DeliveryInfo info;
    };
    struct Consumer final {
        ConsumerRegistration registration;
        std::shared_ptr<ConsumerStorage> storage, attempts;
        std::uint64_t requestHighWater{};
        std::optional<RequestRecord> current;
        std::optional<FrameEnvelope> ready;
        std::vector<std::optional<RequestRecord>> history;
        std::size_t historyNext{};
    };
    FrameDelivery(DeliveryLimits, std::shared_ptr<FrameStorage>, std::shared_ptr<FrameStorage>);
    [[nodiscard]] auto find(std::uint64_t) -> Consumer*;
    [[nodiscard]] auto find(std::uint64_t) const -> const Consumer*;
    [[nodiscard]] auto request(Consumer&, std::uint64_t) -> RequestRecord*;
    void finish(Consumer&, DeliveryStatus);
    void expire(Consumer&, std::int64_t);
    DeliveryLimits limits_;
    std::shared_ptr<FrameStorage> storage_, attemptStorage_;
    mutable std::mutex mutex_;
    std::vector<std::optional<Consumer>> consumers_;
    std::uint64_t nextConsumer_{1}, registerHighWater_{};
    std::optional<ConsumerRegistration> lastRegister_;
    std::uint64_t nextFrame_{1};
    bool closing_{};
};
// Main-thread execution permission is independent of the evictable observation journal.
class RendererConsumerGate final {
  public:
    [[nodiscard]] auto activate(const ConsumerRegistration&, const HostContext& expected,
                                std::uint64_t rendererInstance)
        -> core::Result<RegistrationReceipt>;
    [[nodiscard]] auto revoke(const ConsumerRegistration&) -> core::Result<RegistrationReceipt>;
    [[nodiscard]] auto begin(const FrameEnvelope&, const HostContext& expected, std::uint64_t scope,
                             std::uint64_t rendererInstance) -> core::Result<void>;
    [[nodiscard]] auto succeeded(const FrameEnvelope&) -> core::Result<std::uint64_t>;
    [[nodiscard]] auto revokeRetained(std::uint64_t consumerId, std::uint64_t serial) -> bool;
    [[nodiscard]] auto permitsRetained(const FrameEnvelope&) const noexcept -> bool;
    [[nodiscard]] auto retainedSerial() const noexcept -> std::uint64_t {
        return retainedSerial_;
    }

  private:
    core::ThreadChecker owner_;
    std::optional<ConsumerRegistration> registration_;
    std::uint64_t frontier_{}, frameHighWater_{}, retainedSerial_{}, retainedFrame_{};
    bool active_{}, retainedValid_{}, awaitingSuccess_{};
};
// Conservative preparation-bound estimate. Identifier limits are explicit host admission limits.
[[nodiscard]] auto frameByteBound(std::size_t objects, std::size_t resources, std::size_t members)
    -> core::Result<std::size_t>;
[[nodiscard]] auto payloadBytes(const FramePayload&) noexcept -> std::size_t;
} // namespace cuexis::playback::detail
