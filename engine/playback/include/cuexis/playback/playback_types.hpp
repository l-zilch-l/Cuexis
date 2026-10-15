#pragma once
#include <array>
#include <cstdint>
namespace cuexis::playback {
struct RuntimeFrame final {
    double chartTimeMs{};
    double simulationDeltaTimeMs{};
    std::uint64_t timeDiscontinuityId{};
};

struct PreparedSemanticIdentity final {
    std::array<std::uint8_t, 32> sha256{};

    friend bool operator==(const PreparedSemanticIdentity&,
                           const PreparedSemanticIdentity&) = default;
};

} // namespace cuexis::playback
