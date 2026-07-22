#pragma once

#include <cuexis/core/diagnostic.hpp>

#include <cstddef>
#include <string_view>

namespace cuexis::project::detail {

[[nodiscard]] bool validatePortablePath(std::string_view value, std::size_t maxBytes,
                                        core::Diagnostics& diagnostics, std::string_view fieldPath);

} // namespace cuexis::project::detail
