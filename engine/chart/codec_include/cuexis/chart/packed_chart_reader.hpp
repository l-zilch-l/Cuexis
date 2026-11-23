#pragma once

// Packed Chart file bridge: reads a bounded artifact into the canonical semantic model.
// This header belongs to the codec library. It never declares the Writer or the source
// lowering: those stay in cuexis_chart_tools so the default Playback closure cannot link them.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <filesystem>
#include <span>

namespace cuexis::chart {

class PackedChartReader final {
  public:
    [[nodiscard]] static auto read(const std::filesystem::path& source,
                                   PackedChartLimits limits = {})
        -> core::Result<CanonicalSemanticChart>;

    [[nodiscard]] static auto decode(std::span<const std::byte> bytes,
                                     PackedChartLimits limits = {})
        -> core::Result<CanonicalSemanticChart>;
};

} // namespace cuexis::chart
