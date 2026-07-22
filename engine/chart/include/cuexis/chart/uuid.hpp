#pragma once

#include <cuexis/core/result.hpp>

#include <string>
#include <string_view>

namespace cuexis::chart {

[[nodiscard]] auto isUuidV7(std::string_view text) noexcept -> bool;
[[nodiscard]] auto isUuidV5(std::string_view text) noexcept -> bool;

// RFC 4122 UUIDv5. SHA-1 is used only for standardized name-based identity, not security.
[[nodiscard]] auto uuidV5(std::string_view namespaceUuid, std::string_view name)
    -> core::Result<std::string>;

} // namespace cuexis::chart
