#pragma once

//  UUID helpers - placed in Core so format front-ends need not depend on the Chart module
//  Core provides the base validation and generation for both native UUIDv7 creation and
//  deterministic UUIDv5 import used by the chart schemes

#include <cuexis/core/result.hpp>

#include <string>
#include <string_view>

namespace cuexis::core {

[[nodiscard]] auto isUuidV7(std::string_view text) noexcept -> bool;
[[nodiscard]] auto isUuidV5(std::string_view text) noexcept -> bool;

// RFC 4122 UUIDv5. SHA-1 is used only for standardized name-based identifiers, never for
// security purposes
[[nodiscard]] auto uuidV5(std::string_view namespaceUuid, std::string_view name)
    -> Result<std::string>;

} // namespace cuexis::core
