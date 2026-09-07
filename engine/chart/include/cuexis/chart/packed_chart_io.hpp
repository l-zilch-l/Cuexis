#pragma once

// Candidate Packed Chart sizing, atomic file output, and reader bridge.

#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace cuexis::chart {

struct PackedSectionSize final {
    std::string type;
    std::size_t offset{};
    std::size_t encodedBytes{};
    std::size_t decodedBytes{};
    std::size_t recordCount{};
};

struct PackedChartSizing final {
    packed::PackedChartStatistics statistics;
    std::size_t headerBytes{};
    std::size_t directoryBytes{};
    std::vector<PackedSectionSize> sections;
};

struct PackedChartWriteOptions final {
    packed::PackedChartProfile profile{};
    PackedChartLimits limits{};
};

class PackedChartWriter final {
  public:
    [[nodiscard]] static auto size(const CanonicalSemanticChart& chart,
                                   packed::PackedChartProfile profile = {},
                                   PackedChartLimits limits = {})
        -> core::Result<PackedChartSizing>;

    [[nodiscard]] static auto writeAtomic(const CanonicalSemanticChart& chart,
                                          const std::filesystem::path& target,
                                          PackedChartWriteOptions options = {})
        -> core::Result<PackedChartSizing>;
};

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
