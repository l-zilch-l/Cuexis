#pragma once

#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/reader.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart::detail {

void addV4Error(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string path);

[[nodiscard]] auto readPortableStableId(const json::Reader& reader, const ChartLimits& limits,
                                        core::Diagnostics& diagnostics, std::string_view purpose)
    -> std::optional<std::string>;

[[nodiscard]] auto readV4Rational(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics, bool requirePositive,
                                  bool allowNegative) -> std::optional<RationalBeat>;

[[nodiscard]] auto readRequiredExtensions(const json::Reader& reader, const ChartLimits& limits,
                                          core::Diagnostics& diagnostics)
    -> std::vector<RequiredExtension>;

} // namespace cuexis::chart::detail
