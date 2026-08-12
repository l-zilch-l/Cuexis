#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace cuexis::chart::detail {

class Sha256 final {
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

[[nodiscard]] auto sha256(std::string_view bytes) noexcept -> std::array<std::uint8_t, 32>;
[[nodiscard]] auto sha256Hex(const std::array<std::uint8_t, 32>& digest) -> std::string;

} // namespace cuexis::chart::detail
