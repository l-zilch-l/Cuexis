#include <cuexis/core/log.hpp>

#include <catch2/catch_test_macros.hpp>

#include <latch>
#include <thread>
#include <vector>

TEST_CASE("Logging rejects an empty application name", "[core][log]") {
    cuexis::core::log::shutdown();

    const auto result = cuexis::core::log::init("");

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == "core.log.invalid_name");
}

TEST_CASE("Logging initializes writes and shuts down idempotently", "[core][log]") {
    const auto firstInit = cuexis::core::log::init("CuexisCoreTests");
    REQUIRE(firstInit.has_value());

    const auto secondInit = cuexis::core::log::init("CuexisCoreTests");
    REQUIRE(secondInit.has_value());

    cuexis::core::log::info("test", "info message");
    cuexis::core::log::warn("test", "warning message");
    cuexis::core::log::error("test", "error message");
    cuexis::core::log::shutdown();
    cuexis::core::log::shutdown();

    const auto reinitialized = cuexis::core::log::init("CuexisCoreTests");
    REQUIRE(reinitialized.has_value());
    cuexis::core::log::shutdown();
}

TEST_CASE("Logging serializes concurrent writes and shutdown", "[core][log][thread]") {
    cuexis::core::log::shutdown();
    REQUIRE(cuexis::core::log::init("CuexisConcurrentLogTests").has_value());

    constexpr int workerCount = 4;
    std::latch ready{workerCount};
    std::latch start{1};
    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    for (int index = 0; index < workerCount; ++index) {
        workers.emplace_back([&ready, &start] {
            ready.count_down();
            start.wait();
            cuexis::core::log::info("test.concurrent", "message");
        });
    }

    ready.wait();
    start.count_down();
    cuexis::core::log::shutdown();
    for (auto& worker : workers) {
        worker.join();
    }

    REQUIRE(cuexis::core::log::init("CuexisConcurrentLogTests").has_value());
    cuexis::core::log::shutdown();
}
