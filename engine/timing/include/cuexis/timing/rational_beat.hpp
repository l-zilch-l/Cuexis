#pragma once

//  RationalBeat - normalized rational beat count as numerator/denominator, reduced on store.
//
//  Zero is always stored as 0/1; creation rejects a non-positive denominator and every arithmetic
//  operation is checked: leaving the signed 64-bit range is a stable rejection, never a wrap or a
//  clamp. The ordering below is the exact rational ordering, so two representations of the same
//  value compare equal and no floating-point conversion decides an ordering.
//
//  This is the shared timing value. The old `chart::RationalBeat` keeps its own interface, error
//  codes and epoch; the two are deliberately not interchangeable.

#include <cuexis/core/result.hpp>

#include <compare>
#include <cstdint>
#include <string>

namespace cuexis::timing {

class RationalBeat final {
  public:
    [[nodiscard]] static auto create(std::int64_t numerator, std::int64_t denominator)
        -> core::Result<RationalBeat>;
    [[nodiscard]] static constexpr auto zero() noexcept -> RationalBeat {
        return RationalBeat{0, 1};
    }
    [[nodiscard]] static constexpr auto one() noexcept -> RationalBeat {
        return RationalBeat{1, 1};
    }
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
[[nodiscard]] auto subtractRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat>;
[[nodiscard]] auto multiplyRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat>;
[[nodiscard]] auto divideRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat>;
[[nodiscard]] auto floorRationalBeats(const RationalBeat& value) -> core::Result<std::int64_t>;

} // namespace cuexis::timing
