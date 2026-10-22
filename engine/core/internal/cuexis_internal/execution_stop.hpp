#pragma once

#include <atomic>
#include <cstdint>
#include <exception>

#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
#include "execution_diagnostics.hpp"
#include <thread>
#endif

namespace cuexis::internal::execution_stop {
class Interrupted final : public std::exception {
  public:
    auto what() const noexcept -> const char* override {
        return "Execution owner shutdown requested";
    }
};
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
// An owning test fixture may hold an unsealed timer batch until explicit shutdown.
struct BoundaryGate final {
    std::atomic<bool> entered{}, released{};
    std::uint64_t timerCursor{64};
};
#endif
struct Context final {
    const std::atomic<bool>* requested{};
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
    BoundaryGate* gate{};
#endif
};
inline thread_local Context current;
class Binding final {
    Context previous_;

  public:
    explicit Binding(const std::atomic<bool>& requested
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
                     ,
                     BoundaryGate* gate = nullptr
#endif
                     ) noexcept
        : previous_(current) {
        current.requested = &requested;
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
        current.gate = gate;
#endif
    }
    Binding(const Binding&) = delete;
    auto operator=(const Binding&) -> Binding& = delete;
    ~Binding() noexcept {
        current = previous_;
    }
};
inline void checkpoint() {
    if (current.requested && current.requested->load(std::memory_order_acquire)) {
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
        if (execution_diagnostics::current)
            execution_diagnostics::current->emit("shutdown-interrupted");
#endif
        throw Interrupted{};
    }
}
inline void timerCheckpoint(std::uint64_t cursor) {
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
    if (current.gate && cursor >= current.gate->timerCursor &&
        !current.gate->released.load(std::memory_order_acquire)) {
        current.gate->entered.store(true, std::memory_order_release);
        if (execution_diagnostics::current) {
            execution_diagnostics::current->inFlight(cursor);
            execution_diagnostics::current->emit("test-boundary-held");
        }
        while (!current.gate->released.load(std::memory_order_acquire) && current.requested &&
               !current.requested->load(std::memory_order_acquire))
            std::this_thread::yield();
    }
#else
    static_cast<void>(cursor);
#endif
    checkpoint();
}
} // namespace cuexis::internal::execution_stop
