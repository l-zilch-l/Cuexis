#pragma once

// The host clock and the host-side transport.
//
// ADR 0027 puts time on the host: the SDK is a pure step consumer, so the
// reference host computes chart time and hands each step to the session. This
// header owns the time-related limits and the bounded integer millisecond clock
// they apply to; the parser and the executor share both, so the two cannot
// drift apart.

#include <cstdint>
#include <string_view>

namespace cuexis_reference_host {

// R9 section 3.2. Endpoints are inclusive, and the constants are defined exactly
// once in the host's private headers.
inline constexpr std::uint64_t tickStepMs = 250;
inline constexpr std::uint64_t maxChartTimeMs = 9'007'199'254'740'991ULL;

// The host transport. It is deliberately not the SDK's SessionState: the SDK has
// no pause concept, so "paused" is a host-side state that means "loaded and not
// advancing".
enum class Transport { Empty, Paused, Playing, Terminated };

[[nodiscard]] auto transportName(Transport transport) -> std::string_view;

// Counters recorded at the real call sites. A counter that is incremented only
// where the work actually happens cannot be inflated by reporting.
struct Counters final {
    std::uint64_t tickAttempts{0};
    std::uint64_t suppressedTicks{0};
    std::uint64_t emittedTickFrames{0};
    std::uint64_t publicUpdateAttempts{0};
    std::uint64_t publicUpdateSuccesses{0};
    std::uint64_t frameCount{0};
};

// Bounded integer milliseconds. The value stays an integer here and is converted
// to double only at the SDK boundary, where every integer up to 2^53-1 is exactly
// representable.
struct Clock final {
    std::uint64_t chartTimeMs{0};
    std::uint64_t discontinuityId{0};

    // Advances one tick step. Returns false when the result would leave the
    // representable range, and leaves the clock untouched so the caller can
    // report the overflow without having advanced anything.
    [[nodiscard]] auto advanceOneTick() -> bool;

    // Applies an explicit seek target and bumps the discontinuity id. The id is
    // uint64 and must not wrap, so a target at the maximum is refused rather than
    // wrapped. Returns false when the id would wrap.
    [[nodiscard]] auto applySeek(std::uint64_t target) -> bool;
};

} // namespace cuexis_reference_host
