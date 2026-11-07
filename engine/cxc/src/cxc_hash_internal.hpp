#pragma once

#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace cuexis::cxc::detail {

using cuexis::core::detail::sha256;
using cuexis::core::detail::sha256Hex;

[[nodiscard]] inline auto isSha256(std::string_view value) noexcept -> bool {
    return value.size() == 64U && std::ranges::all_of(value, [](const char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

[[nodiscard]] auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t;

} // namespace cuexis::cxc::detail
