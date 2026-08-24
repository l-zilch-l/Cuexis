#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace cuexis::cxc::detail {

[[nodiscard]] auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t;
[[nodiscard]] auto sha256(std::span<const std::byte> bytes) noexcept
    -> std::array<std::uint8_t, 32>;
[[nodiscard]] auto sha256Hex(std::span<const std::byte> bytes) -> std::string;
[[nodiscard]] auto sha256Hex(const std::array<std::uint8_t, 32>& digest) -> std::string;

} // namespace cuexis::cxc::detail
