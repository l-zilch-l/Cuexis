#pragma once

// F135 test-only allocation probe. The matching translation unit replaces the replaceable global
// allocation functions with counting forwarders to malloc/free, so a test can verify that a path
// performs zero allocations or keeps working while every allocation fails. The replacement is
// process-wide; callers enable counting or failure only around the exact operation under test.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace f135_probe {

inline std::atomic<bool> tracking{};
inline std::atomic<bool> failAll{};
inline std::atomic<std::uint64_t> attempts{};
inline std::atomic<std::uint64_t> successes{};

inline void reset() noexcept {
    attempts.store(0, std::memory_order_relaxed);
    successes.store(0, std::memory_order_relaxed);
}
inline auto attemptCount() noexcept -> std::uint64_t {
    return attempts.load(std::memory_order_relaxed);
}
inline auto successCount() noexcept -> std::uint64_t {
    return successes.load(std::memory_order_relaxed);
}

// Counts allocations during the scope; no allocations should happen in the scope setup itself.
struct CountScope final {
    CountScope() noexcept {
        reset();
        tracking.store(true, std::memory_order_relaxed);
    }
    ~CountScope() {
        tracking.store(false, std::memory_order_relaxed);
    }
    CountScope(const CountScope&) = delete;
    auto operator=(const CountScope&) -> CountScope& = delete;
};

// Fails every allocation during the scope, emulating a sustained out-of-memory state.
struct FailScope final {
    FailScope() noexcept {
        reset();
        tracking.store(true, std::memory_order_relaxed);
        failAll.store(true, std::memory_order_relaxed);
    }
    ~FailScope() {
        failAll.store(false, std::memory_order_relaxed);
        tracking.store(false, std::memory_order_relaxed);
    }
    FailScope(const FailScope&) = delete;
    auto operator=(const FailScope&) -> FailScope& = delete;
};

} // namespace f135_probe
