// Packed Chart Reader bridge. This translation unit belongs to cuexis_chart_codec; the Writer
// and its atomic file output stay in cuexis_chart_tools' packed_chart_io.cpp.

#include <cuexis/chart/packed_chart_reader.hpp>

#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/error.hpp>

#include "packed_limits_internal.hpp"

#include <cstddef>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace cuexis::chart {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] auto error(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

} // namespace

auto PackedChartReader::decode(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    // The file budget is resolved with the frozen Foundation ceilings, so a caller cannot read an
    // artifact past 16 MiB by passing a larger limit.
    if (bytes.size() > packed::limits_detail::effectiveLimits(limits).maxPackedFileBytes) {
        return core::unexpected(
            error("packed.budget.file_bytes", "Packed chart exceeds the file byte budget"));
    }
    // The file bridge deliberately owns no second section registry or structural decoder: it
    // delegates to packed::decode so inspect(), packed::decode and PackedChartReader::decode
    // cannot disagree about the registered subset or report a misleading diagnostic.
    return packed::decode(bytes, limits);
}

auto PackedChartReader::read(const fs::path& source, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    std::error_code statusError;
    const auto size = fs::file_size(source, statusError);
    if (statusError) {
        return core::unexpected(
            error("packed.io.open_failed", "Packed input file could not be stat'ed"));
    }
    if (size > packed::limits_detail::effectiveLimits(limits).maxPackedFileBytes) {
        return core::unexpected(
            error("packed.budget.file_bytes", "Packed chart exceeds the file byte budget"));
    }
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        return core::unexpected(
            error("packed.io.open_failed", "Packed input file could not be opened"));
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size != 0U) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    if (!input && !input.eof()) {
        return core::unexpected(
            error("packed.io.read_failed", "Packed input file could not be read"));
    }
    return decode(bytes, limits);
}

} // namespace cuexis::chart
