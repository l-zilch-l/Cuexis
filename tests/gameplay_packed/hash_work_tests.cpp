#include "hash_work_internal.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis_internal/pmr_buffer.hpp>
#include <limits>
#include <memory_resource>

namespace {
using namespace cuexis::gameplay_packed::detail;
struct BufferUpstream final : std::pmr::memory_resource {
    bool fail{};
    std::size_t allocations{}, deallocations{}, live{}, peak{};
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        if (fail)
            throw std::bad_alloc{};
        auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++allocations;
        live += bytes;
        peak = std::max(peak, live);
        return result;
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
        ++deallocations;
        live -= bytes;
    }
    auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override {
        return this == &other;
    }
};
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

TEST_CASE("POD PMR workspace preserves allocation guarantees and move ownership",
          "[gameplay_packed][capsule4][work]") {
    using cuexis::core::detail::PmrBuffer;
    BufferUpstream upstream;
    {
        PmrBuffer<std::uint32_t> values{&upstream};
        CHECK(upstream.allocations == 0);
        values.reserve(4);
        CHECK(values.capacity() == 4);
        values.push_back(7);
        values.push_back(11);
        const auto* previous = values.data();
        upstream.fail = true;
        CHECK_THROWS_AS(values.reserve(8), std::bad_alloc);
        CHECK(values.data() == previous);
        CHECK(values.capacity() == 4);
        CHECK(values.size() == 2);
        CHECK(values.front() == 7);
        CHECK(values.back() == 11);
        CHECK(upstream.allocations == 1);
        CHECK(upstream.live == 4 * sizeof(std::uint32_t));
        upstream.fail = false;
        CHECK_THROWS_AS(values.reserve(SIZE_MAX), std::bad_array_new_length);
        CHECK(values.data() == previous);
        values.reserve(8);
        CHECK(values.capacity() == 8);
        CHECK(values.front() == 7);
        CHECK(values.back() == 11);
        CHECK(upstream.peak == 12 * sizeof(std::uint32_t));
        CHECK(upstream.live == 8 * sizeof(std::uint32_t));
        PmrBuffer<std::uint32_t> moved{std::move(values)};
        CHECK(values.empty());
        CHECK(values.data() == nullptr);
        CHECK(values.capacity() == 0);
        CHECK(moved.size() == 2);
        BufferUpstream other;
        PmrBuffer<std::uint32_t> destination{&other};
        destination.resize(2);
        CHECK(destination.front() == 0);
        destination = std::move(moved);
        CHECK(other.live == 0);
        CHECK(other.allocations == other.deallocations);
        CHECK(moved.empty());
        CHECK(moved.data() == nullptr);
        CHECK(destination.front() == 7);
        CHECK(destination.back() == 11);
        CHECK(upstream.live == 8 * sizeof(std::uint32_t));
    }
    CHECK(upstream.live == 0);
    CHECK(upstream.allocations == upstream.deallocations);
}

TEST_CASE("POD PMR workspace migration honors the exact scratch overlap boundary",
          "[gameplay_packed][capsule4][work]") {
    for (bool enough : {false, true}) {
        auto limits = kAcceptedHashWork;
        limits.maxHashScratchBytes = 65536 + 12 * sizeof(std::uint32_t) - (enough ? 0 : 1);
        BufferUpstream upstream;
        HashAllocationStats observed;
        HashScratch scratch{limits, &upstream, &observed};
        {
            cuexis::core::detail::PmrBuffer<std::uint32_t> values{&scratch};
            CHECK(upstream.allocations == 0);
            values.resize(4);
            values[0] = 7;
            const auto* previous = values.data();
            if (enough) {
                values.reserve(8);
                CHECK(observed.scratchHighWater == 65536 + 12 * sizeof(std::uint32_t));
                CHECK(observed.dynamicLiveBytes == 8 * sizeof(std::uint32_t));
                CHECK(upstream.peak == 12 * sizeof(std::uint32_t));
            } else {
                CHECK_THROWS_AS(values.reserve(8), HashWorkFailure);
                CHECK(values.data() == previous);
                CHECK(values.capacity() == 4);
                CHECK(observed.scratchHighWater == 65536 + 4 * sizeof(std::uint32_t));
                CHECK(observed.dynamicLiveBytes == 4 * sizeof(std::uint32_t));
            }
            CHECK(values.size() == 4);
            CHECK(values.front() == 7);
        }
        CHECK(observed.dynamicLiveBytes == 0);
        CHECK(scratch.liveBytes() == 65536);
        CHECK(upstream.live == 0);
        CHECK(upstream.allocations == upstream.deallocations);
    }
}
