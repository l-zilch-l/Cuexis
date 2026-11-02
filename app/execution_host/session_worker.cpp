#include "session_worker_internal.hpp"

// SessionWorker facade over the owner channel, frame delivery and the worker thread.

namespace cuexis::playback::detail {

using namespace worker_detail;

SessionWorker::SessionWorker(SessionWorkerConfiguration configuration)
    : configuration_(std::move(configuration)),
      channel_(configuration_.capacity, configuration_.controlReserve),
      receipts_(configuration_.receiptCapacity) {}
auto SessionWorker::create(SessionWorkerConfiguration configuration)
    -> core::Result<std::unique_ptr<SessionWorker>> try {
    if (!configuration.testOnly || configuration.capacity <= configuration.controlReserve ||
        !configuration.controlReserve || configuration.stepCapacity <= configuration.stepReserve ||
        !configuration.stepReserve || !configuration.frameCapacity ||
        !configuration.feedbackCapacity || !configuration.receiptCapacity)
        return core::unexpected(
            workerError("profile_incomplete", "Explicit test-only worker capacities required"));
    auto worker = std::unique_ptr<SessionWorker>{new SessionWorker{std::move(configuration)}};
    auto delivery = FrameDelivery::create(worker->configuration_.delivery);
    if (!delivery)
        return core::unexpected(delivery.error());
    worker->delivery_ = std::move(*delivery);
    worker->thread_ = std::thread{[ptr = worker.get()] { ptr->run(); }};
    return worker;
} catch (...) {
    return core::unexpected(workerError("capacity_exceeded", "Worker startup failed"));
}
SessionWorker::~SessionWorker() noexcept {
    requestShutdown();
    if (thread_.joinable())
        thread_.join();
}
auto SessionWorker::post(SessionCommand command, bool control) -> core::Result<std::uint64_t> {
    if (shutdown_.load(std::memory_order_acquire))
        return core::unexpected(workerError("owner_closed", "Worker admission is closed"));
    return channel_.post(std::move(command), control);
}
auto SessionWorker::poll(std::uint64_t id)
    -> core::Result<std::optional<core::Result<SessionReply>>> {
    return channel_.poll(id);
}
auto SessionWorker::acknowledge(std::uint64_t id) -> core::Result<void> {
    auto result = channel_.ack(id);
    if (result && configuration_.replyAcknowledged)
        configuration_.replyAcknowledged(id);
    return result;
}
auto SessionWorker::takeDelivery(std::uint64_t consumer, std::uint64_t request)
    -> core::Result<std::optional<FrameEnvelope>> {
    return delivery_->take(consumer, request,
                           configuration_.monotonicNow ? configuration_.monotonicNow()
                                                       : monotonicNow());
}
auto SessionWorker::pollDelivery(std::uint64_t consumer, std::uint64_t request)
    -> core::Result<DeliveryInfo> {
    return delivery_->poll(consumer, request,
                           configuration_.monotonicNow ? configuration_.monotonicNow()
                                                       : monotonicNow());
}
auto SessionWorker::cancelDelivery(std::uint64_t consumer, std::uint64_t request)
    -> core::Result<DeliveryInfo> {
    return delivery_->cancel(consumer, request);
}
void SessionWorker::noteLostReceipt(const PresentReceipt& receipt) noexcept {
    try {
        std::lock_guard lock(receiptMutex_);
        ReceiptLoss incoming{receipt.context.sessionScope,
                             receipt.context.sessionScope,
                             receipt.projectionScope,
                             receipt.projectionScope,
                             receipt.frameId,
                             receipt.frameId};
        auto slot = std::find_if(receiptLosses_.begin(), receiptLosses_.end(), [&](const auto& r) {
            return !r ||
                   (r->sessionFirst == incoming.sessionFirst &&
                    r->sessionLast == incoming.sessionLast &&
                    r->scopeFirst == incoming.scopeFirst && r->scopeLast == incoming.scopeLast);
        });
        if (slot == receiptLosses_.end())
            slot = receiptLosses_.begin();
        if (*slot) {
            auto& r = **slot;
            r.sessionFirst = std::min(r.sessionFirst, incoming.sessionFirst);
            r.sessionLast = std::max(r.sessionLast, incoming.sessionLast);
            r.scopeFirst = std::min(r.scopeFirst, incoming.scopeFirst);
            r.scopeLast = std::max(r.scopeLast, incoming.scopeLast);
            r.frameFirst = std::min(r.frameFirst, incoming.frameFirst);
            r.frameLast = std::max(r.frameLast, incoming.frameLast);
        } else
            *slot = incoming;
    } catch (...) {
    }
    auto count = lostReceipts_.load();
    while (count != UINT64_MAX && !lostReceipts_.compare_exchange_weak(count, count + 1)) {
    }
    channel_.wake();
}
void SessionWorker::postReceipt(const PresentReceipt& receipt) noexcept {
    const auto lost = [&]() noexcept { noteLostReceipt(receipt); };
    try {
        std::size_t errorBytes = 0, depth = 0;
        for (auto* cause = receipt.cause ? &*receipt.cause : nullptr; cause;
             cause = cause->cause()) {
            if (++depth > 16) {
                lost();
                return;
            }
            const auto add = [&](std::size_t bytes) {
                if (bytes > 4096 || errorBytes > 4096 - bytes)
                    return false;
                errorBytes += bytes;
                return true;
            };
            if (!add(cause->code().size()) || !add(cause->message().size())) {
                lost();
                return;
            }
            if (cause->context().size() > 64) {
                lost();
                return;
            }
            for (const auto& context : cause->context())
                if (!add(context.key.size()) || !add(context.value.size())) {
                    lost();
                    return;
                }
        }
        auto bounded = copyPresentReceipt(receipt);
        if (!bounded) {
            lost();
            return;
        }
        {
            std::lock_guard lock(receiptMutex_);
            if (receiptCount_ < receipts_.size() && !shutdown_.load()) {
                receipts_[receiptWrite_].emplace(std::move(*bounded));
                receiptWrite_ = (receiptWrite_ + 1) % receipts_.size();
                ++receiptCount_;
                channel_.wake();
                return;
            }
        }
        lost();
    } catch (...) {
        lost();
    }
}
void SessionWorker::waitFor(std::chrono::milliseconds duration) {
    channel_.waitFor(duration);
}
void SessionWorker::waitForReply(std::uint64_t id, std::chrono::steady_clock::duration duration) {
    channel_.waitForReply(id, duration);
}
void SessionWorker::requestShutdown() noexcept {
    internal::execution_diagnostics::Trace diagnostic{"requestShutdown"};
    if (delivery_)
        delivery_->closeAdmission();
    channel_.closeAdmission();
    shutdown_.store(true, std::memory_order_release);
    channel_.wake();
}
auto SessionWorker::exited() const noexcept -> bool {
    return exited_.load(std::memory_order_acquire);
}
auto SessionWorker::joinExited() -> core::Result<void> {
    if (!exited())
        return core::unexpected(workerError("control_pending", "Worker still owns its session"));
    if (thread_.joinable())
        thread_.join();
    return {};
}
auto SessionWorker::fault() const noexcept -> OwnerChannel<SessionCommand, SessionReply>::Fault {
    return channel_.fault();
}
void SessionWorker::run() noexcept {
    internal::execution_diagnostics::Trace diagnostic{"SessionWorker"};
    try {
        SessionOwner owner{configuration_, delivery_};
        auto maintain = [&]() {
            const auto now = owner.now();
            delivery_->expire(now);
            if (owner.frames)
                owner.frames->maintain(now);
            if (lostReceipts_.exchange(0)) {
                std::array<std::optional<ReceiptLoss>, 8> losses;
                {
                    std::lock_guard lock(receiptMutex_);
                    losses = receiptLosses_;
                    receiptLosses_ = {};
                }
                if (owner.frames)
                    for (const auto& r : losses)
                        if (r)
                            owner.frames->receiptLost(r->sessionFirst, r->sessionLast,
                                                      r->scopeFirst, r->scopeLast, r->frameFirst,
                                                      r->frameLast);
            }
            for (std::size_t i = 0; i < 16; ++i) {
                std::optional<PresentReceipt> receipt;
                {
                    std::lock_guard lock(receiptMutex_);
                    if (!receiptCount_)
                        break;
                    receipt = std::move(receipts_[receiptRead_]);
                    receipts_[receiptRead_].reset();
                    receiptRead_ = (receiptRead_ + 1) % receipts_.size();
                    --receiptCount_;
                }
                if (owner.frames) {
                    auto observed = owner.frames->observe(*receipt);
                    if (!observed)
                        owner.frames->historyMiss();
                }
            }
        };
        for (;;) {
            auto next = channel_.takeIf([](const SessionCommand& command) {
                return !std::holds_alternative<SessionRequestFrame>(command);
            });
            if (!next)
                next = channel_.take();
            if (next) {
                try {
                    internal::execution_diagnostics::Trace dispatchDiagnostic{
                        std::holds_alternative<SessionCoherentAdvance>(next->second)
                            ? "SessionCoherentAdvanceDispatch"
                            : "SessionDispatch",
                        0, 0, next->first};
                    if (configuration_.beforeDispatch)
                        configuration_.beforeDispatch(next->first, next->second);
                    channel_.complete(next->first, owner.dispatch(std::move(next->second)));
                } catch (...) {
                    channel_.complete(
                        next->first, core::unexpected(workerError("capacity_exceeded",
                                                                  "Worker job allocation failed")));
                }
                maintain();
                continue;
            }
            maintain();
            if (shutdown_.load(std::memory_order_acquire))
                break;
            channel_.waitFor(std::chrono::milliseconds{20});
        }
    } catch (...) {
        channel_.closeAdmission();
        shutdown_.store(true, std::memory_order_release);
        while (auto next = channel_.take())
            channel_.complete(next->first, core::unexpected(workerError(
                                               "owner_closed", "Worker initialization failed")));
    }
    // Every Playback/Prepared/ledger destructor above has run on this owner.
    exited_.store(true, std::memory_order_release);
}

} // namespace cuexis::playback::detail
