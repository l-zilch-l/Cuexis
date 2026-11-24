#pragma once

// Candidate Packed Chart sizing and atomic file output. The Reader bridge is declared by the
// codec header; this tools header re-exports it so existing file-consumer spelling keeps working.

#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/packed_chart_reader.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/chart/packed_chart_writer.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
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

} // namespace cuexis::chart
