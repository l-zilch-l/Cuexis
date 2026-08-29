#pragma once

#include <string>
#include <string_view>

namespace cuexis::chart::detail {

[[nodiscard]] auto isPortableProjectPath(std::string_view path) noexcept -> bool;
[[nodiscard]] auto isCxtProjectPath(std::string_view path) noexcept -> bool;
[[nodiscard]] auto portableProjectPathCaseKey(std::string_view path) -> std::string;

} // namespace cuexis::chart::detail
