#include "frame_delivery.hpp"
#include <algorithm>
#include <limits>

namespace cuexis::playback::detail {
namespace {
auto stale() -> core::Error {
    return {"host.rpa.context_stale", "Frame delivery identity is not active"};
}
auto conflict() -> core::Error {
    return {"host.rpa.message_conflict", "Frame delivery identity has different metadata"};
}
auto unavailable() -> core::Error {
    return stale().withContext("delivery_status", "HistoryUnavailable");
}
constexpr std::size_t attemptBytes =
    2 * sizeof(FrameEnvelope) + sizeof(FrameAttemptLease) + sizeof(DispatchToken) + 512;
bool terminal(DeliveryStatus s) {
    return s != DeliveryStatus::pending && s != DeliveryStatus::ready;
}
} // namespace
auto FrameAttemptLease::create(StorageReservation reservation)
    -> std::shared_ptr<const FrameAttemptLease> {
    auto lease = std::make_shared<FrameAttemptLease>();
    lease->storage = std::move(reservation);
    return lease;
}
auto sameRegistration(const ConsumerRegistration& a, const ConsumerRegistration& b) noexcept
    -> bool {
    return a.operationId == b.operationId && a.consumerId == b.consumerId &&
           a.context == b.context && a.profile == b.profile;
}
auto fingerprint(const FrameRequest& r) -> DeliveryFingerprint {
    return {r.consumerId,
            r.requestId,
            r.projectionScope,
            r.retained.frame() ? r.retained.frame()->frameId : 0,
            r.retained.serial(),
            r.context,
            r.viewport.width,
            r.viewport.height,
            r.exactVersion,
            r.deadlineMonotonicUs,
            r.frozen};
}
FrameDelivery::FrameDelivery(DeliveryLimits limits, std::shared_ptr<FrameStorage> storage,
                             std::shared_ptr<FrameStorage> attempts)
    : limits_(limits), storage_(std::move(storage)), attemptStorage_(std::move(attempts)),
      consumers_(limits.consumers) {}
auto FrameDelivery::create(DeliveryLimits limits)
    -> core::Result<std::shared_ptr<FrameDelivery>> try {
    if (!limits.consumers || !limits.requestHistory)
        return core::unexpected(stale());
    auto storage = FrameStorage::create(limits.payloads, limits.bytes);
    if (!storage)
        return core::unexpected(storage.error());
    if (!limits.attempts || limits.attempts > SIZE_MAX / attemptBytes)
        return core::unexpected(stale());
    auto attempts = FrameStorage::create(limits.attempts, limits.attempts * attemptBytes);
    if (!attempts)
        return core::unexpected(attempts.error());
    return std::shared_ptr<FrameDelivery>(
        new FrameDelivery(limits, std::move(*storage), std::move(*attempts)));
} catch (...) {
    return core::unexpected(stale());
}
auto FrameDelivery::find(std::uint64_t id) -> Consumer* {
    for (auto& c : consumers_)
        if (c && c->registration.consumerId == id)
            return &*c;
    return nullptr;
}
auto FrameDelivery::find(std::uint64_t id) const -> const Consumer* {
    for (const auto& c : consumers_)
        if (c && c->registration.consumerId == id)
            return &*c;
    return nullptr;
}
auto FrameDelivery::request(Consumer& c, std::uint64_t id) -> RequestRecord* {
    if (c.current && c.current->info.requestId == id)
        return &*c.current;
    for (auto& h : c.history)
        if (h && h->info.requestId == id)
            return &*h;
    return nullptr;
}
void FrameDelivery::finish(Consumer& c, DeliveryStatus status) {
    if (c.ready && c.ready->dispatch)
        c.ready->dispatch->state.store(DispatchState::notSubmitted, std::memory_order_release);
    c.ready.reset();
    if (c.current)
        c.current->info.status = status;
}
void FrameDelivery::expire(Consumer& c, std::int64_t now) {
    if (c.current && !terminal(c.current->info.status) && now >= c.current->fingerprint.deadline)
        finish(c, DeliveryStatus::expiredRequest);
}
auto FrameDelivery::registerConsumer(std::uint64_t operation, HostContext context,
                                     ConsumerProfile profile)
    -> core::Result<ConsumerRegistration> try {
    std::lock_guard lock(mutex_);
    if (!operation || !context.sessionScope || closing_)
        return core::unexpected(stale());
    for (const auto& c : consumers_)
        if (c && c->registration.operationId == operation) {
            if (c->registration.context != context || c->registration.profile != profile)
                return core::unexpected(conflict());
            return c->registration;
        }
    if (operation <= registerHighWater_)
        return core::unexpected(unavailable());
    if (!profile.maxPayloads || !profile.maxAttempts || profile.maxAttempts > limits_.attempts ||
        (profile.renderer && profile.maxAttempts < 2) || !profile.frameByteBound ||
        profile.maxBytes / profile.maxPayloads < profile.frameByteBound ||
        (profile.renderer && (profile.maxPayloads < 2 || !profile.rendererInstance ||
                              profile.rendererGeneration != context.rendererGeneration)))
        return core::unexpected(stale().withContext("delivery_status", "InvalidProfile"));
    // One Renderer, multiple CPU capture consumers. Reserved and retired actual bytes remain
    // disjoint.
    for (const auto& c : consumers_)
        if (c && c->registration.profile.renderer && profile.renderer &&
            c->registration.state != ConsumerState::retired)
            return core::unexpected(stale());
    auto slot = std::find_if(consumers_.begin(), consumers_.end(), [](const auto& c) {
        return !c || (c->registration.state == ConsumerState::retired &&
                      c->storage->liveCount() == 0 && c->attempts->liveCount() == 0);
    });
    if (slot == consumers_.end() || nextConsumer_ == UINT64_MAX)
        return core::unexpected(stale());
    auto storage = storage_->registerConsumer(profile.maxPayloads, profile.maxBytes);
    if (!storage)
        return core::unexpected(storage.error());
    auto attempts =
        attemptStorage_->registerConsumer(profile.maxAttempts, profile.maxAttempts * attemptBytes);
    if (!attempts)
        return core::unexpected(attempts.error());
    Consumer next;
    next.attempts = std::move(*attempts);
    next.registration = {operation, nextConsumer_, context, profile,
                         profile.renderer ? ConsumerState::registering : ConsumerState::active};
    next.storage = std::move(*storage);
    next.history.resize(limits_.requestHistory);
    slot->emplace(std::move(next));
    registerHighWater_ = operation;
    ++nextConsumer_;
    lastRegister_ = (*slot)->registration;
    return (*slot)->registration;
} catch (...) {
    return core::unexpected(stale());
}
auto FrameDelivery::registration(std::uint64_t id) const -> core::Result<ConsumerRegistration> {
    std::lock_guard lock(mutex_);
    const auto* c = find(id);
    if (!c)
        return core::unexpected(unavailable());
    return c->registration;
}
auto FrameDelivery::confirmRegistration(const RegistrationReceipt& receipt) -> core::Result<void> {
    std::lock_guard lock(mutex_);
    auto* c = find(receipt.registration.consumerId);
    if (!c || !sameRegistration(c->registration, receipt.registration))
        return core::unexpected(conflict());
    if (c->registration.state == ConsumerState::active && receipt.active)
        return {};
    if (c->registration.state != ConsumerState::registering || !receipt.active || closing_)
        return core::unexpected(stale());
    c->registration.state = ConsumerState::active;
    return {};
}
auto FrameDelivery::beginRevoke(std::uint64_t id) -> core::Result<ConsumerRegistration> {
    std::lock_guard lock(mutex_);
    auto* c = find(id);
    if (!c)
        return core::unexpected(unavailable());
    if (c->registration.state == ConsumerState::retired)
        return c->registration;
    c->registration.state = ConsumerState::revoking;
    if (c->current && !terminal(c->current->info.status))
        finish(*c, DeliveryStatus::canceled);
    if (!c->registration.profile.renderer) {
        c->registration.state = ConsumerState::retired;
        c->storage->retire();
        c->attempts->retire();
    }
    return c->registration;
}
auto FrameDelivery::confirmRevoke(const RegistrationReceipt& receipt) -> core::Result<void> {
    std::lock_guard lock(mutex_);
    auto* c = find(receipt.registration.consumerId);
    if (!c || !sameRegistration(c->registration, receipt.registration) || receipt.active)
        return core::unexpected(conflict());
    if (c->registration.state == ConsumerState::retired)
        return {};
    if (c->registration.state != ConsumerState::revoking)
        return core::unexpected(stale());
    c->registration.state = ConsumerState::retired;
    c->storage->retire();
    c->attempts->retire();
    return {};
}
auto FrameDelivery::admit(const FrameRequest& r, std::uint64_t version, std::int64_t now,
                          bool reuse) -> core::Result<DeliveryAdmission> try {
    std::lock_guard lock(mutex_);
    auto* c = find(r.consumerId);
    if (!c || c->registration.state != ConsumerState::active || closing_ || !r.requestId ||
        r.context.sessionScope != c->registration.context.sessionScope || !r.viewport.width ||
        !r.viewport.height)
        return core::unexpected(stale());
    const auto f = fingerprint(r);
    if (auto* old = request(*c, r.requestId)) {
        if (old->fingerprint != f)
            return core::unexpected(conflict());
        expire(*c, now);
        return DeliveryAdmission{old->info, {}, false, {}};
    }
    if (r.requestId <= c->requestHighWater)
        return DeliveryAdmission{
            {r.consumerId, r.requestId, 0, 0, DeliveryStatus::historyUnavailable}, {}, false, {}};
    expire(*c, now);
    if (c->current && !terminal(c->current->info.status)) {
        if (c->registration.profile.mode == ConsumerMode::exactCapture)
            return core::unexpected(stale());
        finish(*c, DeliveryStatus::canceled);
    }
    if (c->current) {
        c->history[c->historyNext] = *c->current;
        c->historyNext = (c->historyNext + 1) % c->history.size();
    }
    c->requestHighWater = r.requestId;
    DeliveryInfo info{r.consumerId, r.requestId, version, 0, DeliveryStatus::pending};
    if (r.frozen) {
        if (!c->registration.profile.renderer || !r.retained.frame() ||
            r.retained.frame()->consumerId != r.consumerId)
            info.status = DeliveryStatus::noRetainedFrame;
        else if (r.retained.frame()->source)
            info.resultVersion = r.retained.frame()->source->resultVersion;
    }
    if (now >= r.deadlineMonotonicUs)
        info.status = DeliveryStatus::expiredRequest;
    else if (r.exactVersion && *r.exactVersion != info.resultVersion)
        info.status = DeliveryStatus::versionUnavailable;
    c->current = RequestRecord{f, info};
    if (info.status != DeliveryStatus::pending)
        return DeliveryAdmission{info, {}, false, {}};
    auto attempt = c->attempts->reserve(attemptBytes);
    if (!attempt) {
        finish(*c, DeliveryStatus::skippedNoCredit);
        return DeliveryAdmission{c->current->info, {}, false, {}};
    }
    StorageReservation reservation;
    if (!reuse) {
        auto reserved = c->storage->reserve(c->registration.profile.frameByteBound);
        if (!reserved) {
            finish(*c, DeliveryStatus::skippedNoCredit);
            return DeliveryAdmission{c->current->info, {}, false, {}};
        }
        reservation = std::move(*reserved);
    }
    if (c->registration.profile.renderer) {
        if (nextFrame_ == UINT64_MAX) {
            finish(*c, DeliveryStatus::rejected);
            return core::unexpected(stale());
        }
        c->current->info.frameId = nextFrame_++;
    }
    return DeliveryAdmission{c->current->info, std::move(reservation), true, std::move(*attempt)};
} catch (...) {
    return core::unexpected(stale());
}
auto FrameDelivery::publish(const FrameRequest& r, FrameEnvelope frame, std::int64_t now)
    -> core::Result<DeliveryInfo> {
    std::lock_guard lock(mutex_);
    auto* c = find(r.consumerId);
    if (!c || !c->current || c->current->fingerprint != fingerprint(r)) {
        if (frame.dispatch)
            frame.dispatch->state.store(DispatchState::notSubmitted, std::memory_order_release);
        return core::unexpected(stale());
    }
    expire(*c, now);
    if (c->registration.state != ConsumerState::active &&
        c->current->info.status == DeliveryStatus::pending)
        finish(*c, DeliveryStatus::canceled);
    if (c->registration.state != ConsumerState::active ||
        c->current->info.status != DeliveryStatus::pending) {
        if (frame.dispatch)
            frame.dispatch->state.store(DispatchState::notSubmitted, std::memory_order_release);
        return c->current->info;
    }
    frame.frameId = c->current->info.frameId;
    frame.consumerId = r.consumerId;
    frame.requestId = r.requestId;
    c->ready.emplace(std::move(frame));
    c->current->info.status = DeliveryStatus::ready;
    return c->current->info;
}
auto FrameDelivery::poll(std::uint64_t consumer, std::uint64_t id, std::int64_t now)
    -> core::Result<DeliveryInfo> {
    std::lock_guard lock(mutex_);
    auto* c = find(consumer);
    if (!c)
        return core::unexpected(unavailable());
    expire(*c, now);
    auto* r = request(*c, id);
    if (!r)
        return DeliveryInfo{consumer, id, 0, 0, DeliveryStatus::historyUnavailable};
    return r->info;
}
auto FrameDelivery::cancel(std::uint64_t consumer, std::uint64_t id, bool expired)
    -> core::Result<DeliveryInfo> {
    return finishRequest(consumer, id,
                         expired ? DeliveryStatus::expiredRequest : DeliveryStatus::canceled);
}
auto FrameDelivery::finishRequest(std::uint64_t consumer, std::uint64_t id, DeliveryStatus status)
    -> core::Result<DeliveryInfo> {
    std::lock_guard lock(mutex_);
    auto* c = find(consumer);
    if (!c)
        return core::unexpected(unavailable());
    auto* r = request(*c, id);
    if (!r)
        return DeliveryInfo{consumer, id, 0, 0, DeliveryStatus::historyUnavailable};
    if (c->current && r == &*c->current && !terminal(r->info.status))
        finish(*c, status);
    return r->info;
}
auto FrameDelivery::take(std::uint64_t consumer, std::uint64_t id, std::int64_t now)
    -> core::Result<std::optional<FrameEnvelope>> {
    std::lock_guard lock(mutex_);
    auto* c = find(consumer);
    if (!c)
        return core::unexpected(unavailable());
    expire(*c, now);
    if (!c->current || c->current->info.requestId != id ||
        c->current->info.status != DeliveryStatus::ready || !c->ready)
        return std::optional<FrameEnvelope>{};
    if (c->ready->dispatch)
        c->ready->dispatch->state.store(DispatchState::transferred, std::memory_order_release);
    auto result = std::move(c->ready);
    c->ready.reset();
    c->current->info.status = DeliveryStatus::transferred;
    return result;
}
void FrameDelivery::cancelRetained(std::uint64_t id, std::uint64_t serial) {
    std::lock_guard lock(mutex_);
    auto* c = find(id);
    if (c && c->current && c->current->fingerprint.frozen &&
        c->current->fingerprint.retainedSerial == serial && !terminal(c->current->info.status))
        finish(*c, DeliveryStatus::noRetainedFrame);
}
void FrameDelivery::expire(std::int64_t now) {
    std::lock_guard lock(mutex_);
    for (auto& c : consumers_)
        if (c)
            expire(*c, now);
}
void FrameDelivery::closeAdmission() {
    std::lock_guard lock(mutex_);
    closing_ = true;
    for (auto& c : consumers_)
        if (c && c->registration.state != ConsumerState::retired) {
            c->registration.state = ConsumerState::revoking;
            if (c->current && !terminal(c->current->info.status))
                finish(*c, DeliveryStatus::canceled);
            if (!c->registration.profile.renderer) {
                c->registration.state = ConsumerState::retired;
                c->storage->retire();
                c->attempts->retire();
            }
        }
}
auto FrameDelivery::transferred(std::uint64_t frame) const -> bool {
    std::lock_guard lock(mutex_);
    for (const auto& c : consumers_)
        if (c) {
            if (c->current && c->current->info.frameId == frame)
                return c->current->info.status == DeliveryStatus::transferred;
            for (const auto& h : c->history)
                if (h && h->info.frameId == frame)
                    return h->info.status == DeliveryStatus::transferred;
        }
    return false;
}
auto FrameDelivery::notSubmitted(std::uint64_t frame) const -> bool {
    std::lock_guard lock(mutex_);
    for (const auto& c : consumers_)
        if (c) {
            const auto check = [&](const RequestRecord& r) {
                return r.info.frameId == frame && terminal(r.info.status) &&
                       r.info.status != DeliveryStatus::transferred;
            };
            if (c->current && check(*c->current))
                return true;
            for (const auto& h : c->history)
                if (h && check(*h))
                    return true;
        }
    return false;
}
auto RendererConsumerGate::activate(const ConsumerRegistration& r, const HostContext& expected,
                                    std::uint64_t instance) -> core::Result<RegistrationReceipt> {
    if (!owner_.isCurrent() || !r.consumerId || !r.profile.renderer)
        return core::unexpected(stale());
    if (registration_ && r.consumerId == frontier_) {
        if (!sameRegistration(*registration_, r))
            return core::unexpected(conflict());
        return RegistrationReceipt{r, active_};
    }
    if (r.consumerId < frontier_)
        return RegistrationReceipt{r, false};
    if (active_)
        return core::unexpected(stale());
    frontier_ = r.consumerId;
    registration_ = r;
    frameHighWater_ = 0;
    active_ = r.context == expected && r.profile.rendererInstance == instance &&
              r.profile.rendererGeneration == expected.rendererGeneration;
    return RegistrationReceipt{r, active_};
}
auto RendererConsumerGate::revoke(const ConsumerRegistration& r)
    -> core::Result<RegistrationReceipt> {
    if (!owner_.isCurrent() || !r.consumerId)
        return core::unexpected(stale());
    if (r.consumerId < frontier_)
        return RegistrationReceipt{r, false};
    if (r.consumerId == frontier_ && registration_ && !sameRegistration(*registration_, r))
        return core::unexpected(conflict());
    if (r.consumerId > frontier_ && active_)
        return core::unexpected(stale());
    frontier_ = r.consumerId;
    registration_ = r;
    active_ = false;
    retainedValid_ = false;
    return RegistrationReceipt{r, false};
}
auto RendererConsumerGate::begin(const FrameEnvelope& f, const HostContext& expected,
                                 std::uint64_t scope, std::uint64_t instance)
    -> core::Result<void> {
    if (!owner_.isCurrent() || !active_ || !registration_ || f.consumerId != frontier_ ||
        f.context != expected || f.projectionScope != scope ||
        registration_->profile.rendererInstance != instance ||
        registration_->profile.rendererGeneration != expected.rendererGeneration ||
        f.context.sessionScope != registration_->context.sessionScope || !f.frameId ||
        f.frameId <= frameHighWater_ || !f.dispatch ||
        f.dispatch->state.load(std::memory_order_acquire) != DispatchState::transferred ||
        retainedSerial_ == UINT64_MAX || (f.retainedSerial && !permitsRetained(f)))
        return core::unexpected(stale());
    frameHighWater_ = f.frameId;
    awaitingSuccess_ = true;
    return {};
}
auto RendererConsumerGate::succeeded(const FrameEnvelope& f) -> core::Result<std::uint64_t> {
    if (!owner_.isCurrent() || !active_ || f.consumerId != frontier_ ||
        f.frameId != frameHighWater_ || !awaitingSuccess_ || retainedSerial_ == UINT64_MAX)
        return core::unexpected(stale());
    awaitingSuccess_ = false;
    retainedFrame_ = f.frameId;
    retainedValid_ = true;
    return ++retainedSerial_;
}
auto RendererConsumerGate::revokeRetained(std::uint64_t id, std::uint64_t serial) -> bool {
    if (!owner_.isCurrent() || id != frontier_ || serial != retainedSerial_)
        return false;
    retainedValid_ = false;
    return true;
}
auto RendererConsumerGate::permitsRetained(const FrameEnvelope& f) const noexcept -> bool {
    return active_ && retainedValid_ && f.consumerId == frontier_ &&
           f.retainedSerial == retainedSerial_ && f.retainedFrameId == retainedFrame_;
}
auto frameByteBound(std::size_t objects, std::size_t resources, std::size_t members)
    -> core::Result<std::size_t> {
    constexpr std::size_t identifierBytes = 256;
    constexpr auto objectBytes =
        2 * sizeof(FrameSnapshot::ObjectSnapshot) + 4 * (2 * identifierBytes + 32);
    constexpr auto memberBytes = 2 * sizeof(FeedbackId) + 2 * (2 * identifierBytes + 32);
    std::size_t bytes = sizeof(FramePayload) + sizeof(FrameEnvelope) + 1024;
    for (const auto [count, unit] :
         {std::pair{objects, objectBytes}, std::pair{resources, 2 * sizeof(PortableResourcePtr)},
          std::pair{members, memberBytes}}) {
        if (count > (SIZE_MAX - bytes) / unit)
            return core::unexpected(stale());
        bytes += count * unit;
    }
    return bytes;
}
auto payloadBytes(const FramePayload& p) noexcept -> std::size_t {
    std::size_t bytes = sizeof(FramePayload) +
                        p.snapshot.objects.capacity() * sizeof(FrameSnapshot::ObjectSnapshot) +
                        p.resources.capacity() * sizeof(PortableResourcePtr) +
                        p.membership.capacity() * sizeof(FeedbackId);
    for (const auto& o : p.snapshot.objects) {
        bytes += o.id.capacity() + 1 + o.materialAssetId.capacity() + 1;
        if (o.mesh)
            bytes += o.mesh->assetId.capacity() + 1;
        if (o.material)
            bytes += o.material->assetId.capacity() + 1;
    }
    for (const auto& m : p.membership)
        bytes += m.targetId.capacity() + 1 + m.bindingId.capacity() + 1;
    return bytes;
}
} // namespace cuexis::playback::detail
