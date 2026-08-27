#pragma once

#include <cuexis/core/core_export.hpp>

#include <string>
#include <string_view>

namespace cuexis::core::detail {

[[nodiscard]] CUEXIS_CORE_API auto foldAscii(std::string_view value) -> std::string;
[[nodiscard]] CUEXIS_CORE_API auto isWindowsReservedSegment(std::string_view segment) -> bool;

} // namespace cuexis::core::detail
