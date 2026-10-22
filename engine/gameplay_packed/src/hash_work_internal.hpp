#pragma once

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>

namespace cuexis::gameplay_packed::detail {

struct HashWorkLimits final {
    std::uint64_t maxSemanticPreimageBytes{134217728};
    std::uint64_t maxExpandedReferenceVisits{1000000};
    std::uint64_t maxProjectionKeyBytes{4096};
    std::uint64_t maxHashScratchBytes{8388608};
};

inline constexpr HashWorkLimits kAcceptedHashWork{};
inline constexpr std::string_view kHashWorkProfile = "capsule4.hash-work.v1";

inline auto effectiveHashWork(HashWorkLimits requested) noexcept -> HashWorkLimits {
    return {
        std::min(requested.maxSemanticPreimageBytes, kAcceptedHashWork.maxSemanticPreimageBytes),
        std::min(requested.maxExpandedReferenceVisits,
                 kAcceptedHashWork.maxExpandedReferenceVisits),
        std::min(requested.maxProjectionKeyBytes, kAcceptedHashWork.maxProjectionKeyBytes),
        std::min(requested.maxHashScratchBytes, kAcceptedHashWork.maxHashScratchBytes)};
}

struct HashWorkFailure final {
    core::Error error;
};

[[noreturn]] inline void hashBudget(std::string_view budget, std::string_view field,
                                    std::uint64_t limit, std::uint64_t required, bool exact = true,
                                    std::string_view phase = "count",
                                    std::string_view origin = "semanticHash.total") {
    throw HashWorkFailure{
        core::Error{"capability.budget_insufficient", "Capsule hash work exceeds accepted profile"}
            .withContext("category", "budget_exceeded")
            .withContext("severity", "error")
            .withContext("faulted", "false")
            .withContext("profile", std::string{kHashWorkProfile})
            .withContext("budgetId", std::string{budget})
            .withContext("fieldPath", std::string{field})
            .withContext("originPath", std::string{origin})
            .withContext("phase", std::string{phase})
            .withContext("limit", std::to_string(limit))
            .withContext(exact ? "required" : "requiredAtLeast", std::to_string(required))
            .withContext("exact", exact ? "true" : "false")};
}

// All limits are clamped below UINT64_MAX, so limit+1 is a representable lower bound.
inline auto cappedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t limit) noexcept
    -> std::uint64_t {
    return left > limit || right > limit - left ? limit + 1 : left + right;
}

inline auto cappedMultiply(std::uint64_t left, std::uint64_t right, std::uint64_t limit) noexcept
    -> std::uint64_t {
    return left != 0 && right > limit / left ? limit + 1 : left * right;
}

struct HashCost final {
    std::uint64_t bytes{}, visits{}, maxKey{};
};

struct HashCostSlot final {
    HashCost cost;
    std::uint32_t ordinal{}, flags{};
};
static_assert(sizeof(HashCostSlot) == 32);

inline void checkHashCost(const HashCost& cost, HashWorkLimits limits) {
    if (cost.maxKey > limits.maxProjectionKeyBytes)
        hashBudget("maxProjectionKeyBytes", "semanticHash.projectionKeyBytes",
                   limits.maxProjectionKeyBytes, cost.maxKey, false);
    if (cost.visits > limits.maxExpandedReferenceVisits)
        hashBudget("maxExpandedReferenceVisits", "semanticHash.expandedReferenceVisits",
                   limits.maxExpandedReferenceVisits, cost.visits, false);
    if (cost.bytes > limits.maxSemanticPreimageBytes)
        hashBudget("maxSemanticPreimageBytes", "semanticHash.preimageBytes",
                   limits.maxSemanticPreimageBytes, cost.bytes, false);
}

// Charge capacities before requesting storage. Fixed control, stack-local SHA/chunks and
// bounded diagnostics are conservatively reserved separately from dynamic allocations.
struct HashAllocationStats {
    std::uint64_t scratchHighWater{}, dynamicLiveBytes{}, allocationCount{}, allocationBytes{};
};

class HashScratch final : public std::pmr::memory_resource {
  public:
    explicit HashScratch(HashWorkLimits requested,
                         std::pmr::memory_resource* upstream = std::pmr::new_delete_resource(),
                         HashAllocationStats* observation = nullptr)
        : limit_{effectiveHashWork(requested).maxHashScratchBytes}, upstream_{upstream},
          observation_{observation} {
        charge(65536);
    }
    void setStage(std::string_view phase, std::string_view origin) noexcept {
        phase_ = phase;
        origin_ = origin;
    }
    [[nodiscard]] auto highWater() const noexcept -> std::uint64_t {
        return peak_;
    }
    [[nodiscard]] auto liveBytes() const noexcept -> std::uint64_t {
        return live_;
    }
    void charge(std::uint64_t bytes) {
        if (bytes > limit_ - live_)
            hashBudget("maxHashScratchBytes", "semanticHash.scratchBytes", limit_,
                       cappedAdd(live_, bytes, limit_), false, phase_, origin_);
        live_ += bytes;
        peak_ = std::max(peak_, live_);
        if (observation_)
            observation_->scratchHighWater = peak_;
    }
    void release(std::uint64_t bytes) noexcept {
        live_ -= bytes;
    }
    // Transfer an already charged allocation to the operation's returned value.
    // Its peak remains recorded; the result's resource must outlive that value.
    auto transferAllocation(std::size_t bytes) noexcept -> std::pmr::memory_resource* {
        if (observation_)
            observation_->dynamicLiveBytes -= bytes;
        release(bytes);
        return upstream_;
    }

  private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        charge(bytes);
        try {
            auto* result = upstream_->allocate(bytes, alignment);
            if (observation_) {
                ++observation_->allocationCount;
                observation_->allocationBytes += bytes;
                observation_->dynamicLiveBytes += bytes;
            }
            return result;
        } catch (...) {
            release(bytes);
            throw;
        }
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        upstream_->deallocate(pointer, bytes, alignment);
        if (observation_)
            observation_->dynamicLiveBytes -= bytes;
        release(bytes);
    }
    auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override {
        return this == &other;
    }
    std::uint64_t limit_{}, live_{}, peak_{};
    std::pmr::memory_resource* upstream_;
    HashAllocationStats* observation_;
    std::string_view phase_{"count"}, origin_{"semanticHash.total"};
};

struct HashBytesDeleter final {
    std::pmr::memory_resource* resource{};
    std::size_t capacity{};
    void operator()(std::byte* bytes) const noexcept {
        resource->deallocate(bytes, capacity, alignof(std::byte));
    }
};
using HashOwnedBytes = std::unique_ptr<std::byte, HashBytesDeleter>;

class HashWorkspaceBuffer final {
  public:
    explicit HashWorkspaceBuffer(HashScratch& resource) : resource_{resource} {}
    ~HashWorkspaceBuffer() {
        if (data_)
            resource_.deallocate(data_, capacity_, alignof(std::byte));
    }
    HashWorkspaceBuffer(const HashWorkspaceBuffer&) = delete;
    auto operator=(const HashWorkspaceBuffer&) -> HashWorkspaceBuffer& = delete;
    void allocate(std::uint64_t capacity) {
        // Check before narrowing or allocation, including a 32-bit size_t build.
        resource_.charge(capacity);
        resource_.release(capacity);
        if (capacity == 0)
            return;
        auto* data = static_cast<std::byte*>(
            resource_.allocate(static_cast<std::size_t>(capacity), alignof(std::byte)));
        data_ = data;
        capacity_ = static_cast<std::size_t>(capacity);
    }
    auto data() noexcept -> std::byte* {
        return data_;
    }
    auto capacity() const noexcept -> std::size_t {
        return capacity_;
    }
    auto take() noexcept -> HashOwnedBytes {
        auto* upstream = resource_.transferAllocation(capacity_);
        auto* data = data_;
        data_ = nullptr;
        return HashOwnedBytes{data, HashBytesDeleter{upstream, capacity_}};
    }

  private:
    HashScratch& resource_;
    std::byte* data_{};
    std::size_t capacity_{};
};

} // namespace cuexis::gameplay_packed::detail
