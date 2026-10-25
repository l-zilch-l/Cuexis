#include "hash_work_internal.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <memory_resource>

namespace {
using namespace cuexis::gameplay_packed::detail;
auto failureBudget(const HashCost& cost, HashWorkLimits limits) -> std::string {
    try {
        checkHashCost(cost, limits);
    } catch (const HashWorkFailure& failure) {
        REQUIRE(failure.error.code() == "capability.budget_insufficient");
        for (const auto& context : failure.error.context())
            if (context.key == "budgetId")
                return context.value;
    }
    return {};
}
} // namespace

TEST_CASE("Capsule4 accepted work profile clamps requests and treats zero literally",
          "[gameplay_packed][capsule4][work]") {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const auto limits = effectiveHashWork({maximum, maximum, maximum, maximum});
    CHECK(limits.maxSemanticPreimageBytes == 134217728);
    CHECK(limits.maxExpandedReferenceVisits == 1000000);
    CHECK(limits.maxProjectionKeyBytes == 4096);
    CHECK(limits.maxHashScratchBytes == 8388608);
    const auto zero = effectiveHashWork({0, 0, 0, 0});
    CHECK(zero.maxSemanticPreimageBytes == 0);
    CHECK(zero.maxExpandedReferenceVisits == 0);
    CHECK(zero.maxProjectionKeyBytes == 0);
    CHECK(zero.maxHashScratchBytes == 0);
    CHECK(failureBudget({1, 1, 1}, zero) == "maxProjectionKeyBytes");
    CHECK_THROWS_AS(HashScratch{zero}, HashWorkFailure);
}

TEST_CASE("Capsule4 work arithmetic saturates without wrapping and follows rejection order",
          "[gameplay_packed][capsule4][work]") {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    CHECK(cappedAdd(maximum, maximum, 134217728) == 134217729);
    CHECK(cappedAdd(134217727, 1, 134217728) == 134217728);
    CHECK(cappedMultiply(maximum, maximum, 1000000) == 1000001);
    CHECK(cappedMultiply(maximum, 0, 1000000) == 0);
    CHECK(cappedMultiply(40000, 25, 1000000) == 1000000);
    CHECK(failureBudget({134217728, 1000000, 4096}, {}) == "");
    CHECK(failureBudget({134217729, 1000001, 4097}, {}) == "maxProjectionKeyBytes");
    CHECK(failureBudget({134217729, 1000001, 4096}, {}) == "maxExpandedReferenceVisits");
    CHECK(failureBudget({134217729, 1000000, 4096}, {}) == "maxSemanticPreimageBytes");
    CHECK(failureBudget({10, 5, 2}, {9, 5, 2, 65536}) == "maxSemanticPreimageBytes");
}

TEST_CASE("Capsule4 scratch charges simultaneous capacities before allocation",
          "[gameplay_packed][capsule4][work]") {
    HashScratch scratch{{134217728, 1000000, 4096, 67584}};
    CHECK(scratch.liveBytes() == 65536);
    std::pmr::polymorphic_allocator<std::byte> allocator{&scratch};
    auto* first = allocator.allocate(1024);
    auto* second = allocator.allocate(1024);
    CHECK(scratch.highWater() == 67584);
    CHECK_THROWS_AS(allocator.allocate(1), HashWorkFailure);
    CHECK(scratch.liveBytes() == 67584);
    allocator.deallocate(first, 1024);
    CHECK(scratch.liveBytes() == 66560);
    auto* third = allocator.allocate(1024);
    CHECK(scratch.highWater() == 67584);
    allocator.deallocate(third, 1024);
    allocator.deallocate(second, 1024);
    CHECK(scratch.liveBytes() == 65536);
}
