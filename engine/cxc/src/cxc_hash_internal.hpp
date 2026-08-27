#pragma once

#include <cuexis_internal/sha256.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace cuexis::cxc::detail {

using cuexis::core::detail::sha256;
using cuexis::core::detail::sha256Hex;

[[nodiscard]] auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t;

} // namespace cuexis::cxc::detail
