#pragma once

#include <cstdint>

namespace cuexis::runtime {

struct RuntimeFrame final {
    double chartTimeMs{};
    double simulationDeltaTimeMs{};
    std::uint64_t timeDiscontinuityId{};
};

enum class ReloadPolicy {
    KeepChartTime,
    RestartAtZero,
};

} // namespace cuexis::runtime
