#include "../../app/execution_host/owner_channel.hpp"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <future>
#include <latch>
#include <thread>

namespace {
using Channel = cuexis::playback::detail::OwnerChannel<int, int>;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
auto post(Channel& channel) -> std::uint64_t {
    const auto id = channel.post(7, false);
    REQUIRE(id);
    return *id;
}
auto reply(Channel& channel, std::uint64_t id) -> int {
    const auto polled = channel.poll(id);
    REQUIRE(polled);
    REQUIRE(polled->has_value());
    REQUIRE(**polled);
    return ***polled;
}
auto takeId(Channel& channel) -> std::uint64_t {
    const auto job = channel.take();
    REQUIRE(job);
    return job->first;
}
} // namespace

TEST_CASE("PD reply wait observes completion before entering wait", "[pd][owner-wait]") {
    Channel channel{4, 1};
    const auto id = post(channel);
    REQUIRE(channel.take());
    const auto empty = channel.poll(id);
    REQUIRE(empty);
    REQUIRE_FALSE(empty->has_value());
    std::jthread worker{[&] { channel.complete(id, cuexis::core::Result<int>{42}); }};
    worker.join();
    const auto start = Clock::now();
    channel.waitForReply(id, 100ms);
    REQUIRE(Clock::now() - start < 50ms);
    REQUIRE(reply(channel, id) == 42);
    REQUIRE(reply(channel, id) == 42);
    REQUIRE(channel.ack(id));
}
TEST_CASE("PD reply wait ignores its queued command and unrelated replies", "[pd][owner-wait]") {
    Channel channel{4, 1};
    const auto target = post(channel);
    auto start = Clock::now();
    channel.waitForReply(target, 30ms);
    REQUIRE(Clock::now() - start >= 22ms);
    REQUIRE(takeId(channel) == target);
    const auto other = post(channel);
    REQUIRE(takeId(channel) == other);
    channel.complete(other, cuexis::core::Result<int>{99});
    start = Clock::now();
    channel.waitForReply(target, 30ms);
    REQUIRE(Clock::now() - start >= 22ms);
    channel.complete(target, cuexis::core::Result<int>{42});
    REQUIRE(reply(channel, target) == 42);
    REQUIRE(reply(channel, other) == 99);
    REQUIRE(channel.ack(target));
    REQUIRE(channel.ack(other));
}

TEST_CASE("PD command and reply waiters receive their own readiness", "[pd][owner-wait]") {
    Channel channel{4, 1};
    std::latch workerReady{1};
    auto worker = std::async(std::launch::async, [&] {
        workerReady.count_down();
        const auto deadline = Clock::now() + 3s;
        while (Clock::now() < deadline) {
            if (auto job = channel.take()) {
                channel.complete(job->first, cuexis::core::Result<int>{42});
                return true;
            }
            channel.waitFor(1s);
        }
        return false;
    });
    workerReady.wait();
    const auto id = post(channel);
    channel.waitForReply(id, 2s);
    REQUIRE(worker.wait_for(1s) == std::future_status::ready);
    REQUIRE(worker.get());
    REQUIRE(reply(channel, id) == 42);
    // A retained terminal reply must not make the command worker spin.
    const auto start = Clock::now();
    channel.waitFor(30ms);
    REQUIRE(Clock::now() - start >= 22ms);
    REQUIRE(reply(channel, id) == 42);
    REQUIRE(channel.ack(id));
}

TEST_CASE("PD reply waits retain shutdown errors until explicit acknowledgement",
          "[pd][owner-wait]") {
    Channel channel{2, 1};
    const auto id = post(channel);
    channel.closeAdmission();
    REQUIRE_FALSE(channel.post(9, true));
    const auto start = Clock::now();
    channel.waitForReply(id, 30ms);
    REQUIRE(Clock::now() - start >= 22ms);
    REQUIRE(takeId(channel) == id);
    channel.complete(id, cuexis::core::unexpected(cuexis::core::Error{"probe.closed", "Closed"}));
    const auto ready = Clock::now();
    channel.waitForReply(id, 100ms);
    REQUIRE(Clock::now() - ready < 50ms);
    const auto result = channel.poll(id);
    REQUIRE(result);
    REQUIRE(result->has_value());
    REQUIRE_FALSE(**result);
    REQUIRE((**result).error().code() == "probe.closed");
    REQUIRE(channel.ack(id));
    channel.waitForReply(id, 100ms);
    REQUIRE_FALSE(channel.poll(id));
}
