#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace cuexis::rpa::testing {

// Independent, test-only signed magnitude arithmetic. Four base-2^32 limbs
// cover every intermediate of the i64 rational oracle, including INT64_MIN.
// No production arithmetic or compiler-specific integer extension is used.
class OracleInteger {
  public:
    OracleInteger(std::int64_t value = 0) : negative_(value < 0) {
        const auto magnitude = negative_ ? std::uint64_t{0} - static_cast<std::uint64_t>(value)
                                         : static_cast<std::uint64_t>(value);
        limbs_[0] = static_cast<std::uint32_t>(magnitude);
        limbs_[1] = static_cast<std::uint32_t>(magnitude >> 32);
    }

    explicit operator std::int64_t() const {
        const auto magnitude = (std::uint64_t{limbs_[1]} << 32) | limbs_[0];
        if (limbs_[2] || limbs_[3] ||
            magnitude > (negative_ ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << 63) - 1)) {
            throw std::overflow_error("Oracle integer narrowing");
        }
        if (negative_ && magnitude == (std::uint64_t{1} << 63)) {
            return -INT64_MAX - 1;
        }
        return negative_ ? -static_cast<std::int64_t>(magnitude)
                         : static_cast<std::int64_t>(magnitude);
    }

    friend auto operator==(const OracleInteger&, const OracleInteger&) -> bool = default;
    friend auto operator<(const OracleInteger& a, const OracleInteger& b) -> bool {
        if (a.negative_ != b.negative_) {
            return a.negative_;
        }
        return a.negative_ ? compareMagnitude(a, b) > 0 : compareMagnitude(a, b) < 0;
    }
    friend auto operator>(const OracleInteger& a, const OracleInteger& b) -> bool {
        return b < a;
    }
    friend auto operator<=(const OracleInteger& a, const OracleInteger& b) -> bool {
        return !(b < a);
    }
    friend auto operator>=(const OracleInteger& a, const OracleInteger& b) -> bool {
        return !(a < b);
    }

    auto operator-() const -> OracleInteger {
        auto result = *this;
        if (result != OracleInteger{}) {
            result.negative_ = !result.negative_;
        }
        return result;
    }
    friend auto operator+(const OracleInteger& a, const OracleInteger& b) -> OracleInteger {
        OracleInteger result;
        if (a.negative_ == b.negative_) {
            std::uint64_t carry = 0;
            for (std::size_t i = 0; i < 4; ++i) {
                const auto sum = std::uint64_t{a.limbs_[i]} + b.limbs_[i] + carry;
                result.limbs_[i] = static_cast<std::uint32_t>(sum);
                carry = sum >> 32;
            }
            if (carry) {
                throw std::overflow_error("Oracle integer addition");
            }
            result.negative_ = a.negative_;
        } else {
            const bool aLarger = compareMagnitude(a, b) >= 0;
            const auto& large = aLarger ? a : b;
            const auto& small = aLarger ? b : a;
            std::uint64_t borrow = 0;
            for (std::size_t i = 0; i < 4; ++i) {
                const auto sub = std::uint64_t{small.limbs_[i]} + borrow;
                result.limbs_[i] = static_cast<std::uint32_t>(large.limbs_[i] - sub);
                borrow = std::uint64_t{large.limbs_[i]} < sub;
            }
            result.negative_ = large.negative_;
        }
        result.normalize();
        return result;
    }
    friend auto operator-(const OracleInteger& a, const OracleInteger& b) -> OracleInteger {
        return a + (-b);
    }
    friend auto operator*(const OracleInteger& a, const OracleInteger& b) -> OracleInteger {
        std::array<std::uint32_t, 8> product{};
        for (std::size_t i = 0; i < 4; ++i) {
            std::uint64_t carry = 0;
            for (std::size_t j = 0; j < 4; ++j) {
                const auto value =
                    std::uint64_t{a.limbs_[i]} * b.limbs_[j] + product[i + j] + carry;
                product[i + j] = static_cast<std::uint32_t>(value);
                carry = value >> 32;
            }
            product[i + 4] = static_cast<std::uint32_t>(carry);
        }
        OracleInteger result;
        for (std::size_t i = 0; i < 4; ++i) {
            if (product[i + 4]) {
                throw std::overflow_error("Oracle integer multiplication");
            }
            result.limbs_[i] = product[i];
        }
        result.negative_ = a.negative_ != b.negative_;
        result.normalize();
        return result;
    }
    friend auto operator/(const OracleInteger& a, const OracleInteger& b) -> OracleInteger {
        return divide(a, b).first;
    }
    friend auto operator%(const OracleInteger& a, const OracleInteger& b) -> OracleInteger {
        return divide(a, b).second;
    }
    auto operator+=(const OracleInteger& b) -> OracleInteger& {
        return *this = *this + b;
    }
    auto operator/=(const OracleInteger& b) -> OracleInteger& {
        return *this = *this / b;
    }

  private:
    std::array<std::uint32_t, 4> limbs_{};
    bool negative_{};

    void normalize() {
        if (limbs_ == std::array<std::uint32_t, 4>{}) {
            negative_ = false;
        }
    }
    static auto compareMagnitude(const OracleInteger& a, const OracleInteger& b) -> int {
        for (std::size_t i = 4; i-- > 0;) {
            if (a.limbs_[i] != b.limbs_[i]) {
                return a.limbs_[i] < b.limbs_[i] ? -1 : 1;
            }
        }
        return 0;
    }
    static auto divide(OracleInteger a, OracleInteger b)
        -> std::pair<OracleInteger, OracleInteger> {
        if (b == OracleInteger{}) {
            throw std::domain_error("Oracle integer division by zero");
        }
        const bool quotientNegative = a.negative_ != b.negative_;
        const bool remainderNegative = a.negative_;
        a.negative_ = b.negative_ = false;
        OracleInteger quotient, remainder;
        for (unsigned bit = 128; bit-- > 0;) {
            std::uint32_t carry = (a.limbs_[bit / 32] >> (bit % 32)) & 1;
            for (auto& limb : remainder.limbs_) {
                const auto next = limb >> 31;
                limb = (limb << 1) | carry;
                carry = next;
            }
            if (compareMagnitude(remainder, b) >= 0) {
                remainder = remainder - b;
                quotient.limbs_[bit / 32] |= std::uint32_t{1} << (bit % 32);
            }
        }
        quotient.negative_ = quotientNegative;
        remainder.negative_ = remainderNegative;
        quotient.normalize();
        remainder.normalize();
        return {quotient, remainder};
    }
};
} // namespace cuexis::rpa::testing
