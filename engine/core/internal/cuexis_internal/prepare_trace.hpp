// In-tree startup stage tracing for prepare-style one-shot loads. Enabled only by
// CUEXIS_PREPARE_TRACE=1. Internal header: not installed, not a public SDK facility.

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace cuexis::internal {

// Resolved once per process. With the variable unset every scope below is one predictable branch on
// a cached bool: no clock read, no allocation, no output. Setting the variable lets a host or a
// test binary see which stage a slow load is inside, including the stage that was running when the
// load was interrupted.
inline auto prepareTraceEnabled() noexcept -> bool {
    static const bool enabled = []() noexcept {
#ifdef _MSC_VER
        char value[8]{};
        std::size_t length{};
        return getenv_s(&length, value, sizeof(value), "CUEXIS_PREPARE_TRACE") == 0 &&
               length == 2 && value[0] == '1';
#else
        const auto* value = std::getenv("CUEXIS_PREPARE_TRACE");
        return value != nullptr && value[0] == '1' && value[1] == '\0';
#endif
    }();
    return enabled;
}

inline auto prepareTraceMicroseconds() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Emits PREPARE_TRACE_BEGIN <stage> <absolute_us> and PREPARE_TRACE_END <stage> <elapsed_us>
// <absolute_us> on stderr. Every line is flushed, so a process that is still busy or that is killed
// leaves the last reached stage visible. Elapsed time is inclusive of any nested scope, so a caller
// can compare a parent stage against the sum of the stages it contains.
class PrepareTraceScope final {
  public:
    explicit PrepareTraceScope(const char* stage) noexcept
        : enabled_(prepareTraceEnabled()), stage_(stage), beginUs_(0) {
        if (!enabled_) {
            return;
        }
        beginUs_ = prepareTraceMicroseconds();
        std::fprintf(stderr, "PREPARE_TRACE_BEGIN %s %lld\n", stage_,
                     static_cast<long long>(beginUs_));
        std::fflush(stderr);
    }

    PrepareTraceScope(const PrepareTraceScope&) = delete;
    auto operator=(const PrepareTraceScope&) -> PrepareTraceScope& = delete;
    PrepareTraceScope(PrepareTraceScope&&) = delete;
    auto operator=(PrepareTraceScope&&) -> PrepareTraceScope& = delete;

    ~PrepareTraceScope() noexcept {
        if (!enabled_) {
            return;
        }
        const auto endUs = prepareTraceMicroseconds();
        std::fprintf(stderr, "PREPARE_TRACE_END %s %lld %lld\n", stage_,
                     static_cast<long long>(endUs - beginUs_), static_cast<long long>(endUs));
        std::fflush(stderr);
    }

  private:
    bool enabled_;
    const char* stage_;
    std::int64_t beginUs_;
};

} // namespace cuexis::internal
