#pragma once

// Candidate Packed Chart tables and static component streams.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/packed_chart_primitives.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cuexis::chart::packed {

struct PackedChartProfile final {
    std::uint32_t candidateRevision{1};
    std::uint32_t flags{1};
};

struct PackedChartStatistics final {
    std::size_t packedBytes{};
    std::size_t decodedBytes{};
    std::size_t stringCount{};
    std::size_t referenceCount{};
    std::size_t entityCount{};
    std::size_t requirementCount{};
};

[[nodiscard]] auto encode(const CanonicalSemanticChart& chart, PackedChartProfile profile = {})
    -> core::Result<std::vector<std::byte>>;

[[nodiscard]] auto decode(std::span<const std::byte> bytes, PackedChartLimits limits = {})
    -> core::Result<CanonicalSemanticChart>;

[[nodiscard]] auto inspect(std::span<const std::byte> bytes, PackedChartLimits limits = {})
    -> core::Result<PackedChartStatistics>;

} // namespace cuexis::chart::packed
