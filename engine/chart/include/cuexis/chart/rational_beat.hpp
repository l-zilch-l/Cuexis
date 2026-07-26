#pragma once

//  RationalBeat — 规范化有理数拍数，numerator/denominator，保存时约分
//  方案 A 使用有理数；方案 B 的十进制 beat 通过 parseSimple() 根据原始文本精确转换
//  零统一保存为 0/1；溢出检查受 ChartLimits 约束
//  比较：先通分再比较，保证排序确定性

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
