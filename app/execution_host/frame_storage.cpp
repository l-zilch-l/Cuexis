#include "frame_storage.hpp"
#include <algorithm>
#include <utility>

namespace cuexis::playback::detail {
namespace {
auto noCredit() -> core::Error {
    return {"host.rpa.context_stale", "Delivery has no storage credit"};
}
} // namespace
StorageReservation::StorageReservation(std::shared_ptr<ConsumerStorage> account,
                                       std::size_t bytes) noexcept
    : account_(std::move(account)), bytes_(bytes) {}
StorageReservation::~StorageReservation() noexcept {
    release();
}
StorageReservation::StorageReservation(StorageReservation&& other) noexcept
    : account_(std::move(other.account_)), bytes_(std::exchange(other.bytes_, 0)) {}
auto StorageReservation::operator=(StorageReservation&& other) noexcept -> StorageReservation& {
    if (this != &other) {
        release();
        account_ = std::move(other.account_);
        bytes_ = std::exchange(other.bytes_, 0);
    }
    return *this;
}
void StorageReservation::release() noexcept {
    if (account_) {
        account_->release(bytes_);
        account_.reset();
        bytes_ = 0;
    }
}
auto FrameStorage::create(std::size_t count, std::size_t bytes)
    -> core::Result<std::shared_ptr<FrameStorage>> try {
    if (!count || !bytes)
        return core::unexpected(noCredit());
    return std::shared_ptr<FrameStorage>(new FrameStorage(count, bytes));
} catch (...) {
    return core::unexpected(noCredit());
}
auto FrameStorage::registerConsumer(std::size_t count, std::size_t bytes)
    -> core::Result<std::shared_ptr<ConsumerStorage>> try {
    // Allocate before locking: shared_ptr control-block failure must not run retire under this
    // lock.
    auto account =
        std::shared_ptr<ConsumerStorage>(new ConsumerStorage(shared_from_this(), count, bytes));
    std::lock_guard lock(mutex_);
    if (!count || !bytes || metrics_.reservedCount > maxCount_ ||
        metrics_.retiredCount > maxCount_ - metrics_.reservedCount ||
        count > maxCount_ - metrics_.reservedCount - metrics_.retiredCount ||
        metrics_.reservedBytes > maxBytes_ ||
        metrics_.retiredBytes > maxBytes_ - metrics_.reservedBytes ||
        bytes > maxBytes_ - metrics_.reservedBytes - metrics_.retiredBytes)
        return core::unexpected(noCredit());
    metrics_.reservedCount += count;
    metrics_.reservedBytes += bytes;
    account->retired_ = false;
    return account;
} catch (...) {
    return core::unexpected(noCredit());
}
auto FrameStorage::metrics() const -> StorageMetrics {
    std::lock_guard lock(mutex_);
    return metrics_;
}
ConsumerStorage::~ConsumerStorage() noexcept {
    retire();
}
auto ConsumerStorage::reserve(std::size_t bytes) -> core::Result<StorageReservation> {
    std::lock_guard lock(global_->mutex_);
    if (retired_ || !bytes || liveCount_ >= maxCount_ || liveBytes_ > maxBytes_ ||
        bytes > maxBytes_ - liveBytes_)
        return core::unexpected(noCredit());
    ++liveCount_;
    liveBytes_ += bytes;
    auto& m = global_->metrics_;
    ++m.liveCount;
    m.liveBytes += bytes;
    m.peakCount = std::max(m.peakCount, m.liveCount);
    m.peakBytes = std::max(m.peakBytes, m.liveBytes);
    return StorageReservation{shared_from_this(), bytes};
}
void ConsumerStorage::release(std::size_t bytes) noexcept {
    std::lock_guard lock(global_->mutex_);
    --liveCount_;
    liveBytes_ -= bytes;
    auto& m = global_->metrics_;
    --m.liveCount;
    m.liveBytes -= bytes;
    if (retired_) {
        --m.retiredCount;
        m.retiredBytes -= bytes;
    }
}
void ConsumerStorage::retire() noexcept {
    std::lock_guard lock(global_->mutex_);
    if (retired_)
        return;
    retired_ = true;
    auto& m = global_->metrics_;
    m.reservedCount -= maxCount_;
    m.reservedBytes -= maxBytes_;
    m.retiredCount += liveCount_;
    m.retiredBytes += liveBytes_;
}
auto ConsumerStorage::liveCount() const -> std::size_t {
    std::lock_guard lock(global_->mutex_);
    return liveCount_;
}
} // namespace cuexis::playback::detail
