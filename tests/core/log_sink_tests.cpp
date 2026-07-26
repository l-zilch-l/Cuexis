#include <cuexis/core/log_sink.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("LogSink instances keep independent host lifetimes", "[core][log]") {
    std::vector<std::string> firstEvents;
    std::vector<std::string> secondEvents;
    const cuexis::core::LogSink first{
        [&](const cuexis::core::LogEvent& event) { firstEvents.emplace_back(event.message); }};
    const cuexis::core::LogSink second{
        [&](const cuexis::core::LogEvent& event) { secondEvents.emplace_back(event.message); }};

    first.write(cuexis::core::LogSeverity::Info, "test", "first");
    second.write(cuexis::core::LogSeverity::Warning, "test", "second");

    REQUIRE(firstEvents == std::vector<std::string>{"first"});
    REQUIRE(secondEvents == std::vector<std::string>{"second"});
}

TEST_CASE("LogSink contains host callback exceptions", "[core][log]") {
    const cuexis::core::LogSink sink{
        [](const cuexis::core::LogEvent&) { throw std::runtime_error{"host logger failed"}; }};

    CHECK_NOTHROW(sink.write(cuexis::core::LogSeverity::Error, "test", "failure"));
}
