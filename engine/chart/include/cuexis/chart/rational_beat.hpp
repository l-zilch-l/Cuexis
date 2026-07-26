#pragma once

//  RationalBeat - normalized rational beat count as numerator/denominator, reduced on store
//  Scheme A uses rationals; scheme B decimal beats are converted exactly from the original
//  text by parseSimple()
//  Zero is always stored as 0/1; overflow checks are bounded by ChartLimits
//  Comparison: operands are brought to a common denominator first, guaranteeing a
//  deterministic ordering

#include <cuexis/chart/limits.hpp>
#include <cuexis/core/result.hpp>

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace cuexis::chart {

class RationalBeat final {
  public:
    [[nodiscard]] static auto create(std::int64_t numerator, std::int64_t denominator)
        -> core::Result<RationalBeat>;
    [[nodiscard]] static auto parseSimple(std::string_view text, const ChartLimits& limits = {})
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

} // namespace cuexis::chart
