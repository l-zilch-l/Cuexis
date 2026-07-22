#include <cuexis/core/thread_checker.hpp>

#include <catch2/catch_test_macros.hpp>

#include <thread>

TEST_CASE("ThreadChecker captures its construction thread", "[core][thread]") {
    const cuexis::core::ThreadChecker checker;

    REQUIRE(checker.isCurrent());
    checker.assertCurrent();
}

TEST_CASE("ThreadChecker rejects another thread", "[core][thread]") {
    const cuexis::core::ThreadChecker checker;
    bool workerIsCurrent = true;

    std::thread worker([&checker, &workerIsCurrent] { workerIsCurrent = checker.isCurrent(); });
    worker.join();

    REQUIRE_FALSE(workerIsCurrent);
}
