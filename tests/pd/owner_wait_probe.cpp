#include "../../app/execution_host/owner_channel.hpp"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <latch>
#include <string_view>
#include <thread>

namespace {
using Channel = cuexis::playback::detail::OwnerChannel<int, int>;
using Clock = std::chrono::steady_clock;
constexpr int commandValue = 7, replyValue = 42;

void characterize(const char* scenario, std::chrono::milliseconds timeout) {
    Channel channel{4, 1};
    const auto posted = channel.post(commandValue, false);
    REQUIRE(posted);
    const auto id = *posted;
    bool completeBeforeWait = false;
    Clock::time_point completedAt{};
    if (std::string_view{scenario} == "completed_between_poll_and_wait") {
        std::latch taken{1}, releaseCompletion{1}, completed{1};
        bool workerValid = false;
        std::jthread worker{[&] {
            const auto job = channel.take();
            workerValid = job && job->first == id && job->second == commandValue;
            taken.count_down();
            releaseCompletion.wait();
            if (workerValid)
                channel.complete(id, cuexis::core::Result<int>{replyValue});
            completedAt = Clock::now();
            completed.count_down();
        }};
        taken.wait();
        const auto initialPoll = channel.poll(id);
        // Always release/join before assertions so a failed diagnostic cannot strand the worker.
        releaseCompletion.count_down();
        completed.wait();
        worker.join();
        REQUIRE(workerValid);
        REQUIRE(initialPoll);
        REQUIRE_FALSE(initialPoll->has_value());
        completeBeforeWait = true;
    } else if (std::string_view{scenario} == "in_flight_without_reply") {
        const auto job = channel.take();
        REQUIRE(job);
        REQUIRE(job->first == id);
    }
    const auto start = Clock::now();
    if (std::string_view{scenario} == "queued_command_without_reply")
        channel.waitFor(timeout);
    else
        channel.waitForReply(id, timeout);
    const auto end = Clock::now();
    const auto elapsedUs = std::chrono::duration<double, std::micro>(end - start).count();
    const auto after = channel.poll(id);
    REQUIRE(after);
    REQUIRE(after->has_value() == completeBeforeWait);
    if (completeBeforeWait) {
        REQUIRE(completedAt <= start);
        REQUIRE(**after);
        REQUIRE(***after == replyValue);
    } else {
        if (std::string_view{scenario} == "queued_command_without_reply") {
            const auto job = channel.take();
            REQUIRE(job);
            REQUIRE(job->first == id);
        }
        channel.complete(id, cuexis::core::Result<int>{replyValue});
    }
    const auto final = channel.poll(id);
    REQUIRE(final);
    REQUIRE(final->has_value());
    REQUIRE(**final);
    REQUIRE(***final == replyValue);
    REQUIRE(channel.ack(id));
    REQUIRE(channel.fault() == Channel::Fault::none);
    if (timeout.count() && std::string_view{scenario} == "in_flight_without_reply")
        REQUIRE(elapsedUs >= timeout.count() * 1000.0 * 0.75);
    if (completeBeforeWait && timeout.count() == 100)
        REQUIRE(elapsedUs < 50000);
    std::cout << "PD_WAIT {\"scenario\":\"" << scenario << "\",\"requested_ms\":" << timeout.count()
              << ",\"elapsed_us\":" << elapsedUs
              << ",\"complete_before_wait\":" << (completeBeforeWait ? "true" : "false")
              << ",\"reply_preserved\":true,\"acknowledged\":true}\n";
}
} // namespace

TEST_CASE("PD owner channel completed reply wait characterization", "[.][pd][wait-diagnostic]") {
    std::cout << std::setprecision(17);
    for (const auto timeout : {0, 1, 20, 100})
        for (const auto* scenario : {"queued_command_without_reply", "in_flight_without_reply",
                                     "completed_between_poll_and_wait"})
            characterize(scenario, std::chrono::milliseconds{timeout});
    std::cout << "PD_WAIT_ID {\"observations\":12,\"channel_capacity\":4,\"reserve\":1,"
                 "\"reply_value\":42,\"synchronization\":\"latches and join; no sleep\","
                 "\"production_patch\":true}\n";
}
