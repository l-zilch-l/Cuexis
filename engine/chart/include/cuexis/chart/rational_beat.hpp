#pragma once

//  RationalBeat - normalized rational beat count as numerator/denominator, reduced on store
//  Zero is always stored as 0/1; creation rejects invalid denominators and overflow
//  Comparison: operands are brought to a common denominator first, guaranteeing a
//  deterministic ordering

#include <cuexis/core/result.hpp>

#include <compare>
#include <cstdint>
#include <string>

namespace cuexis::chart {

class RationalBeat final {
  public:
    [[nodiscard]] static auto create(std::int64_t numerator, std::int64_t denominator)
        -> core::Result<RationalBeat>;
    [[nodiscard]] constexpr auto numerator() const noexcept -> std::int64_t {
        return numerator_;
    }
    [[nodiscard]] constexpr auto denominator() const noexcept -> std::int64_t {
        return denominator_;
    }
    [[nodiscard]] auto toDouble() const noexcept -> double;
    [[nodiscard]] auto toString() const -> std::string;

    friend constexpr auto operator==(const RationalBeat&, const RationalBeat&) noexcept
        -> bool = default;
    friend auto operator<=>(const RationalBeat& left, const RationalBeat& right) noexcept
        -> std::strong_ordering;

  private:
    constexpr RationalBeat(std::int64_t numerator, std::int64_t denominator) noexcept
        : numerator_(numerator), denominator_(denominator) {}

    std::int64_t numerator_{};
    std::int64_t denominator_{1};
};

[[nodiscard]] auto addRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat>;
[[nodiscard]] auto rationalBeatMidpoint(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat>;

} // namespace cuexis::chart
