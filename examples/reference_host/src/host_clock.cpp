#include "host_clock.hpp"

#include <limits>

namespace cuexis_reference_host {

auto transportName(Transport transport) -> std::string_view {
    switch (transport) {
    case Transport::Empty:
        return "Empty";
    case Transport::Paused:
        return "Paused";
    case Transport::Playing:
        return "Playing";
    case Transport::Terminated:
        return "Terminated";
    }
    // Unreachable: the switch covers every enumerator. The empty view keeps the
    // fallback visibly wrong instead of inventing a plausible transport name.
    return {};
}

auto Clock::advanceOneTick() -> bool {
    // The comparison happens before the addition, so the sum can never wrap. A
    // refused tick leaves the whole clock untouched, including discontinuityId.
    if (chartTimeMs > maxChartTimeMs || tickStepMs > (maxChartTimeMs - chartTimeMs)) {
        return false;
    }
    chartTimeMs += tickStepMs;
    return true;
}

auto Clock::applySeek(std::uint64_t target) -> bool {
    // A seek at the end of the id space is refused rather than wrapped: the id
    // has to stay a strict, non-repeating record of discontinuities.
    if (discontinuityId == std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    discontinuityId += 1U;
    chartTimeMs = target;
    return true;
}

} // namespace cuexis_reference_host
