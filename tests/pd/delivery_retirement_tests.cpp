#include "../../app/execution_host/frame_delivery.hpp"
#include "../../app/execution_host/session_worker.hpp"
#include <barrier>
#include <catch2/catch_test_macros.hpp>
#include <thread>

using namespace cuexis;
using namespace cuexis::playback;
using namespace cuexis::playback::detail;
namespace {
auto context() -> HostContext {
    HostContext c;
    c.sessionScope = c.clockSegment = c.runEpoch = c.controlSequence = c.resourceGeneration =
        c.rendererGeneration = 1;
    return c;
}
auto rendererProfile() -> ConsumerProfile {
    return {ConsumerMode::realtimeLatest, true, 2, 8192, 4096, 1, 1};
}
struct DeliveryFixture {
    std::shared_ptr<FrameDelivery> delivery;
    ConsumerRegistration registration;
    RendererConsumerGate gate;
    DeliveryFixture() {
        auto made = FrameDelivery::create({4, 4, 16384, 2});
        REQUIRE(made);
        delivery = *made;
        auto registered = delivery->registerConsumer(1, context(), rendererProfile());
        REQUIRE(registered);
        registration = *registered;
        auto active = gate.activate(registration, context(), 1);
        REQUIRE(active);
        REQUIRE(active->active);
        REQUIRE(delivery->confirmRegistration(*active));
    }
    auto request(std::uint64_t id) const -> FrameRequest {
        FrameRequest r;
        r.consumerId = registration.consumerId;
        r.requestId = id;
        r.context = context();
        r.projectionScope = 1;
        r.viewport = {640, 480};
        r.deadlineMonotonicUs = 1000000;
        return r;
    }
    auto ready(FrameRequest r) -> FrameEnvelope {
        auto admitted = delivery->admit(r, 7, 0, false);
        REQUIRE(admitted);
        REQUIRE(admitted->materialize);
        auto payload = std::make_shared<FramePayload>();
        payload->storage = std::move(admitted->reservation);
        payload->snapshot.viewportWidth = 640;
        payload->snapshot.viewportHeight = 480;
        FrameEnvelope f;
        auto lease = std::make_shared<FrameAttemptLease>();
        lease->storage = std::move(admitted->attempt);
        f.attemptLease = std::move(lease);
        f.context = r.context;
        f.frameId = admitted->info.frameId;
        f.projectionScope = 1;
        f.consumerId = r.consumerId;
        f.requestId = r.requestId;
        f.payload = std::move(payload);
        f.dispatch = std::make_shared<DispatchToken>();
        REQUIRE(delivery->publish(r, f, 0));
        return f;
    }
};
} // namespace
TEST_CASE("PD4 storage credit follows the last reader across consumer retirement",
          "[pd][pd4][storage]") {
    auto made = FrameStorage::create(2, 8192);
    REQUIRE(made);
    auto storage = *made;
    auto registered = storage->registerConsumer(2, 8192);
    REQUIRE(registered);
    auto consumer = *registered;
    auto reserved = consumer->reserve(4096);
    REQUIRE(reserved);
    auto backing = std::make_shared<FramePayload>();
    backing->storage = std::move(*reserved);
    auto reader = backing;
    consumer->retire();
    backing.reset();
    CHECK(storage->metrics().liveCount == 1);
    CHECK(storage->metrics().retiredCount == 1);
    CHECK_FALSE(storage->registerConsumer(2, 8192));
    reader.reset();
    CHECK(storage->metrics().liveCount == 0);
    CHECK(storage->metrics().retiredBytes == 0);
    CHECK(storage->registerConsumer(2, 8192));
}
TEST_CASE("PD4 Ready take and cancel have one winner and never retract a transfer",
          "[pd][pd4][delivery]") {
    for (bool cancelFirst : {false, true}) {
        DeliveryFixture f;
        auto r = f.request(1);
        auto sent = f.ready(r);
        if (cancelFirst)
            REQUIRE(f.delivery->cancel(r.consumerId, r.requestId));
        auto taken = f.delivery->take(r.consumerId, r.requestId, 1);
        REQUIRE(taken);
        REQUIRE(f.delivery->cancel(r.consumerId, r.requestId));
        CHECK(bool(*taken) == !cancelFirst);
        CHECK(sent.dispatch->state.load() ==
              (cancelFirst ? DispatchState::notSubmitted : DispatchState::transferred));
        CHECK_FALSE(*f.delivery->take(r.consumerId, r.requestId, 1));
        CHECK(f.delivery->poll(r.consumerId, 1, 1)->status ==
              (cancelFirst ? DeliveryStatus::canceled : DeliveryStatus::transferred));
    }
}
TEST_CASE("PD4 concurrent cancellation cannot duplicate or retract an owning take",
          "[pd][pd4][delivery]") {
    for (int n = 0; n < 64; ++n) {
        DeliveryFixture f;
        auto r = f.request(1);
        auto sent = f.ready(r);
        std::barrier start{3};
        std::optional<FrameEnvelope> taken;
        bool canceled = false;
        std::thread a([&] {
            start.arrive_and_wait();
            auto result = f.delivery->take(r.consumerId, 1, 1);
            if (result)
                taken = std::move(*result);
        });
        std::thread b([&] {
            start.arrive_and_wait();
            auto result = f.delivery->cancel(r.consumerId, 1);
            canceled = bool(result);
        });
        start.arrive_and_wait();
        a.join();
        b.join();
        CHECK(canceled);
        CHECK(sent.dispatch->state.load() ==
              (taken ? DispatchState::transferred : DispatchState::notSubmitted));
        CHECK_FALSE(*f.delivery->take(r.consumerId, 1, 1));
    }
}
TEST_CASE("PD4 request deadline and metadata dedup do not retain payloads", "[pd][pd4][delivery]") {
    DeliveryFixture f;
    auto r = f.request(1);
    r.deadlineMonotonicUs = 10;
    auto sent = f.ready(r);
    sent = {};
    CHECK_FALSE(*f.delivery->take(r.consumerId, 1, 10));
    CHECK(f.delivery->poll(r.consumerId, 1, 10)->status == DeliveryStatus::expiredRequest);
    CHECK(f.delivery->storageMetrics().liveCount == 0);
    CHECK_FALSE(f.delivery->admit(r, 99, 11, false)->materialize);
    auto changed = r;
    changed.viewport.width++;
    CHECK_FALSE(f.delivery->admit(changed, 99, 11, false));
    for (std::uint64_t id = 2; id < 7; ++id) {
        auto next = f.request(id);
        auto frame = f.ready(next);
        frame = {};
        REQUIRE(f.delivery->cancel(next.consumerId, id));
    }
    CHECK(f.delivery->admit(r, 7, 11, false)->info.status == DeliveryStatus::historyUnavailable);
    CHECK(f.delivery->storageMetrics().liveCount == 0);
}
TEST_CASE("PD4 no credit is a delivery result and released readers immediately admit again",
          "[pd][pd4][delivery]") {
    DeliveryFixture f;
    auto one = f.ready(f.request(1));
    auto taken = f.delivery->take(one.consumerId, 1, 1);
    REQUIRE(taken);
    REQUIRE(*taken);
    auto two = f.ready(f.request(2));
    auto takenTwo = f.delivery->take(two.consumerId, 2, 1);
    REQUIRE(takenTwo);
    auto denied = f.delivery->admit(f.request(3), 8, 1, false);
    REQUIRE(denied);
    CHECK(denied->info.status == DeliveryStatus::skippedNoCredit);
    CHECK_FALSE(denied->materialize);
    CHECK(f.delivery->storageMetrics().peakCount == 2);
    one = {};
    taken->reset();
    auto admitted = f.delivery->admit(f.request(4), 9, 1, false);
    REQUIRE(admitted);
    CHECK(admitted->materialize);
}
TEST_CASE("PD4 registration revocation frontier survives old activation and frame history eviction",
          "[pd][pd4][registration]") {
    DeliveryFixture f;
    auto first = f.ready(f.request(1));
    auto taken = f.delivery->take(first.consumerId, 1, 1);
    REQUIRE(taken);
    REQUIRE(*taken);
    REQUIRE(f.gate.begin(**taken, context(), 1, 1));
    REQUIRE(f.gate.succeeded(**taken));
    CHECK_FALSE(f.gate.begin(**taken, context(), 1, 1));
    auto retiring = f.delivery->beginRevoke(first.consumerId);
    REQUIRE(retiring);
    auto revoked = f.gate.revoke(*retiring);
    REQUIRE(revoked);
    REQUIRE(f.delivery->confirmRevoke(*revoked));
    CHECK_FALSE(f.gate.activate(f.registration, context(), 1)->active);
    auto second = f.delivery->registerConsumer(2, context(), rendererProfile());
    REQUIRE(second);
    REQUIRE(f.delivery->confirmRegistration(*f.gate.activate(*second, context(), 1)));
    CHECK_FALSE(f.gate.revoke(f.registration)->active);
    CHECK_FALSE(f.gate.begin(first, context(), 1, 1));
    CHECK(f.gate.activate(*second, context(), 1)->active);
    CHECK(f.delivery->storageMetrics().retiredCount == 1);
}
TEST_CASE("PD4 revoke arriving before activation prevents delayed activation",
          "[pd][pd4][registration]") {
    auto d = FrameDelivery::create({2, 2, 8192, 2});
    REQUIRE(d);
    auto r = (*d)->registerConsumer(1, context(), rendererProfile());
    REQUIRE(r);
    RendererConsumerGate gate;
    auto retiring = (*d)->beginRevoke(r->consumerId);
    REQUIRE(retiring);
    auto receipt = gate.revoke(*retiring);
    REQUIRE(receipt);
    REQUIRE((*d)->confirmRevoke(*receipt));
    auto late = gate.activate(*r, context(), 1);
    REQUIRE(late);
    CHECK_FALSE(late->active);
    CHECK_FALSE((*d)->confirmRegistration({*r, true}));
}
TEST_CASE("PD4 CPU captures have independent credits and no presentation attempts",
          "[pd][pd4][capture]") {
    auto made = FrameDelivery::create({3, 2, 8192, 2});
    REQUIRE(made);
    auto d = *made;
    ConsumerProfile profile{ConsumerMode::exactCapture, false, 1, 4096, 4096, 0, 0};
    auto a = d->registerConsumer(1, context(), profile);
    auto b = d->registerConsumer(2, context(), profile);
    REQUIRE(a);
    REQUIRE(b);
    for (const auto& c : {*a, *b}) {
        FrameRequest r;
        r.consumerId = c.consumerId;
        r.requestId = 1;
        r.context = context();
        r.viewport = {1, 1};
        r.deadlineMonotonicUs = 10;
        r.exactVersion = 42;
        auto unavailable = d->admit(r, 41, 0, false);
        REQUIRE(unavailable);
        CHECK(unavailable->info.status == DeliveryStatus::versionUnavailable);
        r.requestId = 2;
        auto admitted = d->admit(r, 42, 0, false);
        REQUIRE(admitted);
        CHECK(admitted->materialize);
        CHECK(admitted->info.frameId == 0);
    }
}
TEST_CASE("PD4 journal distinguishes finite maximum tick from UntilReset", "[pd][pd4][journal]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 2, 4);
    REQUIRE(ledger);
    FeedbackId finite{1, 1, 0, 0, "t", "finite"}, forever{1, 2, 0, 0, "t", "forever"};
    REQUIRE(ledger->feedback(finite, 0, GameplayPresentationTick{INT64_MAX}));
    REQUIRE(ledger->feedback(forever, 0, std::nullopt));
    REQUIRE(ledger->expire(INT64_MAX));
    CHECK(ledger->query(finite)->status == FeedbackStatus::expiredUnconfirmed);
    CHECK(ledger->query(forever)->status == FeedbackStatus::created);
    auto next = context();
    ++next.controlSequence;
    REQUIRE(ledger->activate(next, 2));
    CHECK(ledger->query(forever)->status == FeedbackStatus::canceledByControl);
}
TEST_CASE("PD4 journal metadata stays bounded beyond the historical 4097 failure",
          "[pd][pd4][journal]") {
    DeliveryFixture f;
    auto ledger = FrameObservationLedger::create(context(), 1, 4, 4);
    REQUIRE(ledger);
    FrameObservationLedger::Limits limits;
    limits.bytes = 16384;
    limits.tombstones = 2;
    limits.coverageIntervals = 2;
    REQUIRE(ledger->configure(limits));
    for (std::uint64_t id = 1; id <= 10000; ++id) {
        auto frame = f.ready(f.request(id));
        REQUIRE(ledger->recordAttempt(frame));
        auto taken = f.delivery->take(frame.consumerId, id, 1);
        REQUIRE(taken);
        REQUIRE(*taken);
        REQUIRE(f.gate.begin(**taken, context(), 1, 1));
        REQUIRE(f.gate.succeeded(**taken));
        REQUIRE(ledger->observe({context(), frame.frameId, 1, PresentStatus::succeeded, 1, {}}));
        CHECK(ledger->retainedAttempts() <= 4);
        CHECK(ledger->journalBytes() <= limits.bytes);
        CHECK(f.delivery->storageMetrics().liveCount == 1);
    }
    CHECK(f.delivery->storageMetrics().liveCount == 0);
    CHECK(f.delivery->storageMetrics().peakCount == 1);
    CHECK(ledger->historyUnavailable());
    CHECK(ledger->fault().reason == FaultSlot::Reason::none);
}
TEST_CASE("PD4 early receipt observes atomic transfer and timeout freezes feedback evidence",
          "[pd][pd4][journal]") {
    DeliveryFixture f;
    auto ledger = FrameObservationLedger::create(context(), 1, 4, 4);
    REQUIRE(ledger);
    FrameObservationLedger::Limits limits;
    limits.receiptDeadlineUs = 5;
    limits.retentionUs = 20;
    REQUIRE(ledger->configure(limits));
    const FeedbackId id{1, 1, 0, 0, "t", "b"};
    REQUIRE(ledger->feedback(id, 0, std::nullopt));
    auto request = f.request(1);
    auto frame = f.ready(request); // amend membership for direct protocol test
    frame.payload.reset();
    frame.membership = {id};
    frame.presentationTick = 1;
    REQUIRE(ledger->recordAttempt(frame));
    auto taken = f.delivery->take(request.consumerId, 1, 1);
    REQUIRE(taken);
    REQUIRE(*taken);
    ledger->maintain(5);
    CHECK(ledger->query(id)->status == FeedbackStatus::unknownEvidence);
    REQUIRE(ledger->observe({context(), frame.frameId, 1, PresentStatus::succeeded, 2, {}}));
    CHECK(ledger->query(id)->laterConfirmed);
    REQUIRE(ledger->observe({context(), frame.frameId, 1, PresentStatus::unknown, 0, {}}));
    CHECK_FALSE(ledger->observe({context(), frame.frameId, 1, PresentStatus::failed, 3, {}}));
    ledger->maintain(21);
    CHECK(ledger->retainedAttempts() == 0);
    CHECK_FALSE(
        ledger->query(id)); // both full detail and tombstone obey the monotonic retention window
    REQUIRE(ledger->feedback(id, 0, std::nullopt));
    CHECK(ledger->query(id)->status == FeedbackStatus::unknownEvidence);
}
TEST_CASE("PD4 observation inbox overload does not fault reliable session commands",
          "[pd][pd4][receipt]") {
    SessionWorkerConfiguration config{16, 4, 16, 4, 4, 4, true, {}};
    config.receiptCapacity = 1;
    auto worker = SessionWorker::create(config);
    REQUIRE(worker);
    for (int i = 0; i < 1000; ++i)
        (*worker)->postReceipt({context(), 1, 1, PresentStatus::unknown, 0, {}});
    auto job = (*worker)->post(SessionInspect{}, true);
    REQUIRE(job);
    std::optional<core::Result<SessionReply>> done;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!done && std::chrono::steady_clock::now() < deadline) {
        auto result = (*worker)->poll(*job);
        REQUIRE(result);
        done = std::move(*result);
        if (!done)
            (*worker)->waitForReply(*job, std::chrono::milliseconds{1});
    }
    REQUIRE(done);
    CHECK(*done);
    REQUIRE((*worker)->acknowledge(*job));
    CHECK(((*worker)->fault() == OwnerChannel<SessionCommand, SessionReply>::Fault::none));
}

TEST_CASE("PD4 normal history eviction cannot poison a new activation or scope",
          "[pd][pd4][journal]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 1, 1);
    REQUIRE(ledger);
    FeedbackId old{1, 1, 0, 0, "t", "old"}, fresh{1, 2, 0, 0, "t", "new"};
    REQUIRE(ledger->feedback(old, 0, 10));
    FrameEnvelope f;
    f.context = context();
    f.projectionScope = 1;
    f.frameId = 1;
    f.presentationTick = 1;
    f.membership = {old};
    REQUIRE(ledger->publish(f));
    REQUIRE(ledger->takeLatest());
    REQUIRE(ledger->observe({context(), 1, 1, PresentStatus::succeeded, 1, {}}));
    f.frameId = 2;
    REQUIRE(ledger->publish(f));
    REQUIRE(ledger->feedback(fresh, 20, 30));
    CHECK(ledger->query(fresh)->status == FeedbackStatus::created);
    f.frameId = 3;
    f.presentationTick = 21;
    f.membership = {fresh};
    REQUIRE(ledger->publish(f));
    REQUIRE(ledger->takeLatest());
    REQUIRE(ledger->observe({context(), 3, 1, PresentStatus::succeeded, 3, {}}));
    CHECK(ledger->query(fresh)->status == FeedbackStatus::confirmedPresent);
    auto next = context();
    ++next.controlSequence;
    REQUIRE(ledger->activate(next, 2));
    fresh.projectionScope = 2;
    REQUIRE(ledger->feedback(fresh, 0, 10));
    CHECK(ledger->query(fresh)->status == FeedbackStatus::created);
}
TEST_CASE("PD4 stale context cannot consume renderer execution frontier",
          "[pd][pd4][registration]") {
    DeliveryFixture f;
    auto frame = f.ready(f.request(1));
    auto taken = f.delivery->take(frame.consumerId, 1, 1);
    REQUIRE(taken);
    REQUIRE(*taken);
    auto stale = **taken;
    stale.context.controlSequence++;
    stale.frameId += 10000;
    CHECK_FALSE(f.gate.begin(stale, context(), 1, 1));
    REQUIRE(f.gate.begin(**taken, context(), 1, 1));
    REQUIRE(f.gate.succeeded(**taken));
    CHECK_FALSE(f.gate.succeeded(**taken));
}
TEST_CASE("PD4 late receipt updates a retained feedback tombstone", "[pd][pd4][journal]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 4, 1);
    REQUIRE(ledger);
    FeedbackId a{1, 1, 0, 0, "t", "a"}, b{1, 2, 0, 0, "t", "b"};
    REQUIRE(ledger->feedback(a, 0, 10));
    FrameEnvelope f;
    f.context = context();
    f.projectionScope = 1;
    f.frameId = 1;
    f.presentationTick = 1;
    f.membership = {a};
    REQUIRE(ledger->publish(f));
    REQUIRE(ledger->takeLatest());
    REQUIRE(ledger->expire(10));
    REQUIRE(ledger->feedback(b, 20, 30));
    REQUIRE(ledger->observe({context(), 1, 1, PresentStatus::succeeded, 2, {}}));
    CHECK(ledger->query(a)->status == FeedbackStatus::unknownEvidence);
    CHECK(ledger->query(a)->laterConfirmed);
}

TEST_CASE("PD4 reused payload still consumes bounded attempt leases", "[pd][pd4][delivery]") {
    auto made = FrameDelivery::create({1, 2, 8192, 2, 2});
    REQUIRE(made);
    auto d = *made;
    auto profile = rendererProfile();
    profile.maxAttempts = 2;
    auto registration = d->registerConsumer(1, context(), profile);
    REQUIRE(registration);
    REQUIRE(d->confirmRegistration({*registration, true}));
    std::vector<StorageReservation> held;
    for (std::uint64_t id = 1; id <= 2; ++id) {
        FrameRequest r;
        r.consumerId = registration->consumerId;
        r.requestId = id;
        r.context = context();
        r.viewport = {1, 1};
        r.deadlineMonotonicUs = 10;
        auto admitted = d->admit(r, 1, 0, true);
        REQUIRE(admitted);
        REQUIRE(admitted->materialize);
        held.push_back(std::move(admitted->attempt));
        REQUIRE(d->cancel(r.consumerId, id));
    }
    FrameRequest r;
    r.consumerId = registration->consumerId;
    r.requestId = 3;
    r.context = context();
    r.viewport = {1, 1};
    r.deadlineMonotonicUs = 10;
    CHECK(d->admit(r, 1, 0, true)->info.status == DeliveryStatus::skippedNoCredit);
    CHECK(d->storageMetrics().liveCount == 0);
    CHECK(d->attemptMetrics().liveCount == 2);
    held.clear();
    r.requestId = 4;
    CHECK(d->admit(r, 1, 0, true)->materialize);
}
TEST_CASE("PD4 superseded materialization is NotSubmitted without changing the newer request",
          "[pd][pd4][delivery]") {
    DeliveryFixture f;
    auto a = f.request(1);
    auto admitted = f.delivery->admit(a, 1, 0, false);
    REQUIRE(admitted);
    FrameEnvelope old;
    old.dispatch = std::make_shared<DispatchToken>();
    auto b = f.request(2);
    REQUIRE(f.delivery->admit(b, 2, 0, false));
    CHECK_FALSE(f.delivery->publish(a, old, 0));
    CHECK(old.dispatch->state.load() == DispatchState::notSubmitted);
    CHECK(f.delivery->poll(b.consumerId, b.requestId, 0)->status == DeliveryStatus::pending);
}

TEST_CASE("PD4 old lost receipts do not contaminate a newer scope", "[pd][pd4][journal]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 2, 4);
    REQUIRE(ledger);
    FrameObservationLedger::Limits limits;
    limits.retentionUs = 20;
    REQUIRE(ledger->configure(limits));
    ledger->maintain(
        1'000'000'000); // realistic nonzero steady-clock origin, initialized before the first write
    FeedbackId a{1, 1, 0, 0, "t", "a"};
    REQUIRE(ledger->feedback(a, 0, std::nullopt));
    ledger->maintain(1'000'000'001);
    CHECK(ledger->query(a)->status == FeedbackStatus::created);
    FrameEnvelope f;
    f.context = context();
    f.projectionScope = 1;
    f.frameId = 1;
    f.presentationTick = 1;
    f.membership = {a};
    f.dispatch = std::make_shared<DispatchToken>();
    REQUIRE(ledger->recordAttempt(f));
    ledger->maintain(1'000'000'022);
    CHECK(ledger->coverage().attemptsEvicted);
    CHECK(ledger->coverage().feedbackEvicted);
    CHECK(ledger->coverage().incomplete.size() <= limits.coverageIntervals);
    auto next = context();
    ++next.controlSequence;
    REQUIRE(ledger->activate(next, 2));
    FeedbackId b{2, 2, 0, 0, "t", "b"};
    REQUIRE(ledger->feedback(b, 0, 10));
    ledger->receiptLost(1, 1, 1, 1, 1, 1);
    ledger->historyMiss();
    CHECK(ledger->query(b)->status == FeedbackStatus::created);
    f.context = next;
    f.projectionScope = 2;
    f.frameId = 2;
    f.membership = {b};
    f.dispatch->state = DispatchState::transferred;
    REQUIRE(ledger->recordAttempt(f));
    REQUIRE(ledger->observe({next, 2, 2, PresentStatus::succeeded, 2, {}}));
    CHECK(ledger->query(b)->status == FeedbackStatus::confirmedPresent);
}
TEST_CASE("PD4 Ready eviction before a later transfer leaves incomplete evidence",
          "[pd][pd4][journal]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 2, 4);
    REQUIRE(ledger);
    FrameObservationLedger::Limits limits;
    limits.retentionUs = 20;
    REQUIRE(ledger->configure(limits));
    const FeedbackId id{1, 1, 0, 0, "t", "until"};
    REQUIRE(ledger->feedback(id, 0, std::nullopt));
    FrameEnvelope f;
    f.context = context();
    f.projectionScope = 1;
    f.frameId = 1;
    f.presentationTick = 1;
    f.membership = {id};
    f.dispatch = std::make_shared<DispatchToken>();
    REQUIRE(ledger->recordAttempt(f));
    ledger->maintain(21);
    f.dispatch->state = DispatchState::transferred;
    REQUIRE(ledger->feedback(id, 0, std::nullopt));
    REQUIRE(ledger->expire(INT64_MAX));
    CHECK(ledger->query(id)->status == FeedbackStatus::unknownEvidence);
    CHECK_FALSE(ledger->observe({context(), 1, 1, PresentStatus::succeeded, 22, {}}));
}
TEST_CASE("PD4 receipt replacement accounts the difference and preserves the first known terminal",
          "[pd][pd4][journal]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 2, 4);
    REQUIRE(ledger);
    FrameEnvelope f;
    f.context = context();
    f.projectionScope = 1;
    f.frameId = 1;
    REQUIRE(ledger->publish(f));
    REQUIRE(ledger->takeLatest());
    const auto base = ledger->journalBytes();
    core::Error why{"test.unknown", "bounded uncertainty"};
    why.withContext("key", "value");
    REQUIRE(ledger->observe({context(), 1, 1, PresentStatus::unknown, 1, why}));
    CHECK(ledger->journalBytes() > base);
    REQUIRE(ledger->observe({context(), 1, 1, PresentStatus::succeeded, 2, {}}));
    CHECK(ledger->journalBytes() == base);
    REQUIRE(ledger->observe({context(), 1, 1, PresentStatus::unknown, 1, why}));
    CHECK(ledger->journalBytes() == base);
}

TEST_CASE("PD4 reliable owner work overtakes queued delivery while keeping reliable FIFO",
          "[pd][pd4][priority]") {
    using Job = std::variant<SessionRequestFrame, SessionInspect>;
    OwnerChannel<Job, int> channel{8, 2};
    auto delivery = channel.post(SessionRequestFrame{}, false);
    auto first = channel.post(SessionInspect{}, true);
    auto second = channel.post(SessionInspect{}, true);
    REQUIRE(delivery);
    REQUIRE(first);
    REQUIRE(second);
    auto eligible = [](const Job& job) {
        return !std::holds_alternative<SessionRequestFrame>(job);
    };
    auto one = channel.takeIf(eligible);
    REQUIRE(one);
    CHECK(one->first == *first);
    auto two = channel.takeIf(eligible);
    REQUIRE(two);
    CHECK(two->first == *second);
    auto last = channel.take();
    REQUIRE(last);
    CHECK(last->first == *delivery);
}
