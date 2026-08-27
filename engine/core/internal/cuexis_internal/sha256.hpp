#pragma once

#include <cuexis/core/core_export.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace cuexis::core::detail {

class CUEXIS_CORE_API Sha256 final {
  public:
    void update(std::span<const std::byte> bytes) noexcept;
    [[nodiscard]] auto finish() const noexcept -> std::array<std::uint8_t, 32>;

  private:
    void transform(const std::array<std::uint8_t, 64>& block) noexcept;

    std::array<std::uint32_t, 8> state_{0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
                                        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
    std::array<std::uint8_t, 64> block_{};
    std::size_t blockSize_{};
    std::uint64_t totalBytes_{};
};

[[nodiscard]] CUEXIS_CORE_API auto sha256(std::span<const std::byte> bytes) noexcept
    -> std::array<std::uint8_t, 32>;
[[nodiscard]] CUEXIS_CORE_API auto sha256(std::string_view bytes) noexcept
    -> std::array<std::uint8_t, 32>;
[[nodiscard]] CUEXIS_CORE_API auto sha256Hex(const std::array<std::uint8_t, 32>& digest)
    -> std::string;
[[nodiscard]] CUEXIS_CORE_API auto sha256Hex(std::span<const std::byte> bytes) -> std::string;

} // namespace cuexis::core::detail
