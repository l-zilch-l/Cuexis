#pragma once

#include <cstddef>
#include <cuexis/core/result.hpp>
#include <memory>
#include <mutex>

namespace cuexis::playback::detail {
struct StorageMetrics final {
    std::size_t liveCount{}, liveBytes{}, peakCount{}, peakBytes{};
    std::size_t reservedCount{}, reservedBytes{}, retiredCount{}, retiredBytes{};
};
class ConsumerStorage;
class StorageReservation final {
  public:
    StorageReservation() = default;
    ~StorageReservation() noexcept;
    StorageReservation(StorageReservation&&) noexcept;
    auto operator=(StorageReservation&&) noexcept -> StorageReservation&;
    StorageReservation(const StorageReservation&) = delete;
    [[nodiscard]] auto bytes() const noexcept -> std::size_t {
        return bytes_;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return bool(account_);
    }

  private:
    friend class ConsumerStorage;
    StorageReservation(std::shared_ptr<ConsumerStorage>, std::size_t) noexcept;
    void release() noexcept;
    std::shared_ptr<ConsumerStorage> account_;
    std::size_t bytes_{};
};
class FrameStorage final : public std::enable_shared_from_this<FrameStorage> {
  public:
    [[nodiscard]] static auto create(std::size_t count, std::size_t bytes)
        -> core::Result<std::shared_ptr<FrameStorage>>;
    [[nodiscard]] auto registerConsumer(std::size_t count, std::size_t bytes)
        -> core::Result<std::shared_ptr<ConsumerStorage>>;
    [[nodiscard]] auto metrics() const -> StorageMetrics;

  private:
    friend class ConsumerStorage;
    FrameStorage(std::size_t count, std::size_t bytes) : maxCount_(count), maxBytes_(bytes) {}
    mutable std::mutex mutex_;
    std::size_t maxCount_, maxBytes_;
    StorageMetrics metrics_;
};
// This storage account has no Session, Worker, GL or SDL pointer. It may outlive Close.
class ConsumerStorage final : public std::enable_shared_from_this<ConsumerStorage> {
  public:
    ~ConsumerStorage() noexcept;
    [[nodiscard]] auto reserve(std::size_t bytes) -> core::Result<StorageReservation>;
    void retire() noexcept;
    [[nodiscard]] auto liveCount() const -> std::size_t;

  private:
    friend class FrameStorage;
    friend class StorageReservation;
    ConsumerStorage(std::shared_ptr<FrameStorage> global, std::size_t count, std::size_t bytes)
        : global_(std::move(global)), maxCount_(count), maxBytes_(bytes) {}
    void release(std::size_t bytes) noexcept;
    std::shared_ptr<FrameStorage> global_;
    std::size_t maxCount_, maxBytes_, liveCount_{}, liveBytes_{};
    bool retired_{true};
};
} // namespace cuexis::playback::detail
