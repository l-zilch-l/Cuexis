#include <cuexis/judgement/normalized_score.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace cuexis::judgement {
namespace {

//  Every failure of the integer helpers is the same stable relation rejection: the content or the
//  caller asked for a score the declared mode cannot express.
auto scoreError(std::string message, std::string reason) -> core::Error {
    return core::Error{"judgement.execution.relation_invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason));
}

auto checkedAdd(std::uint64_t left, std::uint64_t right) -> core::Result<std::uint64_t> {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return core::unexpected(
            scoreError("normalized score arithmetic overflowed", "score_arithmetic_overflow"));
    }
    return left + right;
}

auto checkedMultiply(std::uint64_t left, std::uint64_t right) -> core::Result<std::uint64_t> {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return core::unexpected(
            scoreError("normalized score arithmetic overflowed", "score_arithmetic_overflow"));
    }
    return left * right;
}

auto validateRewardSegments(const std::vector<ComboRewardSegment>& segments) -> core::Result<void> {
    if (segments.empty()) {
        return core::unexpected(
            scoreError("the score mode declares no combo reward segment", "score_reward_missing"));
    }
    std::uint32_t previousFrom = 0;
    std::uint64_t previousLast = 0;
    bool first = true;
    for (const auto& segment : segments) {
        if (first) {
            if (segment.from != 1) {
                return core::unexpected(
                    scoreError("the first combo reward segment must start at combo 1",
                               "score_reward_first_segment"));
            }
            first = false;
        } else if (segment.from <= previousFrom) {
            return core::unexpected(
                scoreError("combo reward segments must start at strictly increasing combo indices",
                           "score_reward_order"));
        } else if (segment.rewardAtFrom < previousLast) {
            return core::unexpected(
                scoreError("combo reward segments must not decrease at the boundary",
                           "score_reward_continuity"));
        }
        //  The last point of this segment is `rewardAtFrom + slope * (nextFrom - 1 - from)`, but
        //  validating one segment on its own only needs the first point; the boundary check above
        //  compares it with the previous segment's last point. The caller that knows the next
        //  segment's start checks the last point in `validatePreparedNormalizedScore`.
        auto firstPoint = comboRewardAt(segment, segment.from);
        if (!firstPoint) {
            return core::unexpected(firstPoint.error());
        }
        previousLast = *firstPoint;
        previousFrom = segment.from;
    }
    for (std::size_t index = 0; index + 1 < segments.size(); ++index) {
        const auto& segment = segments[index];
        const auto& next = segments[index + 1];
        auto lastPoint = comboRewardAt(segment, next.from - 1);
        if (!lastPoint) {
            return core::unexpected(lastPoint.error());
        }
        if (next.rewardAtFrom < *lastPoint) {
            return core::unexpected(
                scoreError("combo reward segments must not decrease at the boundary",
                           "score_reward_continuity"));
        }
    }
    return {};
}

} // namespace

auto comboRewardAt(const ComboRewardSegment& segment, std::uint32_t combo)
    -> core::Result<std::uint64_t> {
    if (combo < segment.from) {
        return core::unexpected(
            scoreError("the combo index lies before the first point of its reward segment",
                       "score_reward_range"));
    }
    const std::uint64_t distance = static_cast<std::uint64_t>(combo) - segment.from;
    auto growth = checkedMultiply(distance, segment.slope);
    if (!growth) {
        return growth;
    }
    return checkedAdd(segment.rewardAtFrom, *growth);
}

auto comboReward(const ScoreMode& mode, std::uint32_t combo) -> core::Result<std::uint64_t> {
    const ComboRewardSegment* selected = nullptr;
    for (const auto& segment : mode.comboRewards) {
        if (segment.from > combo) {
            break;
        }
        selected = &segment;
    }
    if (selected == nullptr) {
        return core::unexpected(
            scoreError("no combo reward segment covers this combo index", "score_reward_range"));
    }
    return comboRewardAt(*selected, combo);
}

auto deriveComboBasis(const ScoreMode& mode, std::uint32_t qualifyingUnits)
    -> core::Result<std::uint64_t> {
    std::uint64_t sum = 0;
    for (std::uint32_t index = 1; index <= qualifyingUnits; ++index) {
        auto reward = comboReward(mode, index);
        if (!reward) {
            return reward;
        }
        auto next = checkedAdd(sum, *reward);
        if (!next) {
            return next;
        }
        sum = *next;
    }
    return sum;
}

auto validatePreparedNormalizedScore(const PreparedNormalizedScore& score) -> core::Result<void> {
    //  Weight split: the addition is done in u64 so `mj + mc == total` cannot wrap.
    const std::uint64_t split =
        static_cast<std::uint64_t>(score.mode.mj) + static_cast<std::uint64_t>(score.mode.mc);
    if (split != score.mode.total) {
        return core::unexpected(scoreError(
            "the score mode weight split does not add up to the total", "score_weight_split"));
    }
    if (score.mode.onExhausted == ComboAction::increment) {
        return core::unexpected(scoreError(
            "the score mode onExhausted action must be keep or reset", "score_on_exhausted"));
    }
    auto rewards = validateRewardSegments(score.mode.comboRewards);
    if (!rewards) {
        return rewards;
    }
    //  Rule keys are unique and a non-qualifying row is forced to zero points and keep.
    for (std::size_t i = 0; i < score.mode.rules.size(); ++i) {
        const auto& rule = score.mode.rules[i];
        if (!rule.qualifies && (rule.judgePoints != 0 || rule.combo != ComboAction::keep)) {
            return core::unexpected(
                scoreError("a non-qualifying score rule must carry zero points and keep the combo",
                           "score_rule_unqualified"));
        }
        for (std::size_t j = 0; j < i; ++j) {
            const auto& other = score.mode.rules[j];
            if (other.phase == rule.phase && other.outcome == rule.outcome &&
                other.grade == rule.grade) {
                return core::unexpected(
                    scoreError("two score rules carry the same (phase, outcome, grade) key",
                               "score_rule_duplicate"));
            }
        }
    }
    //  Grade tables are sorted by id, their exact band contains zero, and their intervals are
    //  ordered, non-empty, positive and gap-free.
    std::string_view previousTableId;
    for (const auto& table : score.gradeTables) {
        if (!previousTableId.empty() && !(previousTableId < table.id)) {
            return core::unexpected(scoreError(
                "grade tables must be sorted by id and carry unique ids", "grade_table_order"));
        }
        previousTableId = table.id;
        if (table.exactStart > 0 || table.exactEnd <= 0 || table.exactStart >= table.exactEnd) {
            return core::unexpected(
                scoreError("the exact grade band must contain zero with a positive width",
                           "grade_exact_band"));
        }
        if (table.intervals.empty()) {
            return core::unexpected(
                scoreError("a grade table has no interval", "grade_table_empty"));
        }
        std::int64_t expectedStart = 0;
        bool first = true;
        for (const auto& interval : table.intervals) {
            if (first) {
                expectedStart = interval.start;
                first = false;
            } else if (interval.start != expectedStart) {
                return core::unexpected(
                    scoreError("grade intervals must be ordered, non-overlapping and gap-free",
                               "grade_interval_order"));
            }
            if (interval.start >= interval.end) {
                return core::unexpected(
                    scoreError("a grade interval is empty", "grade_interval_empty"));
            }
            expectedStart = interval.end;
        }
    }
    //  Units are non-empty, in ascending first-phase order, and each phase instance appears once.
    std::uint32_t previousFirst = 0;
    bool firstUnit = true;
    std::vector<std::uint32_t> seen;
    for (const auto& unit : score.units) {
        if (unit.phaseIndices.empty()) {
            return core::unexpected(scoreError("a scoring unit is empty", "score_unit_empty"));
        }
        for (std::size_t i = 0; i < unit.phaseIndices.size(); ++i) {
            if (i != 0 && unit.phaseIndices[i] <= unit.phaseIndices[i - 1]) {
                return core::unexpected(
                    scoreError("a scoring unit must carry strictly ascending phase indices",
                               "score_unit_order"));
            }
            if (std::find(seen.begin(), seen.end(), unit.phaseIndices[i]) != seen.end()) {
                return core::unexpected(
                    scoreError("a phase instance appears in more than one scoring unit",
                               "score_unit_duplicate"));
            }
            seen.push_back(unit.phaseIndices[i]);
        }
        if (!firstUnit && unit.phaseIndices.front() <= previousFirst) {
            return core::unexpected(scoreError(
                "scoring units must be ordered by their first phase index", "score_unit_order"));
        }
        previousFirst = unit.phaseIndices.front();
        firstUnit = false;
    }
    return {};
}

auto preparedGradeAt(const PreparedGradeTable& table, std::int64_t error) noexcept
    -> const std::string* {
    for (const auto& interval : table.intervals) {
        if (error < interval.start) {
            return nullptr;
        }
        if (error < interval.end) {
            return &interval.grade;
        }
    }
    return nullptr;
}

namespace {

//  A fixed 160-bit unsigned value in five 32-bit limbs (little endian). The normalized score uses
//  at most 160 bits for `mj * j * dc + mc * c * dj` and at most 128 bits for `dj * dc`; the
//  remainder of the restoring division and its doubled comparison stay inside five limbs because
//  the remainder is always below the denominator.
struct Wide final {
    std::uint32_t limb[5]{};

    [[nodiscard]] auto bit(int index) const noexcept -> std::uint32_t {
        return (limb[index / 32] >> (index % 32)) & 1U;
    }

    void shiftLeftOne() noexcept {
        std::uint32_t carry = 0;
        for (auto& part : limb) {
            const std::uint32_t next = part >> 31U;
            part = (part << 1U) | carry;
            carry = next;
        }
    }

    [[nodiscard]] auto isZero() const noexcept -> bool {
        return limb[0] == 0 && limb[1] == 0 && limb[2] == 0 && limb[3] == 0 && limb[4] == 0;
    }
};

void addInto(Wide& left, const Wide& right) noexcept {
    std::uint64_t carry = 0;
    for (int index = 0; index < 5; ++index) {
        const std::uint64_t sum =
            static_cast<std::uint64_t>(left.limb[index]) + right.limb[index] + carry;
        left.limb[index] = static_cast<std::uint32_t>(sum);
        carry = sum >> 32U;
    }
}

[[nodiscard]] auto lessThan(const Wide& left, const Wide& right) noexcept -> bool {
    for (int index = 4; index >= 0; --index) {
        if (left.limb[index] != right.limb[index]) {
            return left.limb[index] < right.limb[index];
        }
    }
    return false;
}

[[nodiscard]] auto equalTo(const Wide& left, const Wide& right) noexcept -> bool {
    for (int index = 4; index >= 0; --index) {
        if (left.limb[index] != right.limb[index]) {
            return false;
        }
    }
    return true;
}

void subtractFrom(Wide& left, const Wide& right) noexcept {
    std::int64_t borrow = 0;
    for (int index = 0; index < 5; ++index) {
        const std::int64_t difference = static_cast<std::int64_t>(left.limb[index]) -
                                        static_cast<std::int64_t>(right.limb[index]) - borrow;
        if (difference < 0) {
            left.limb[index] = static_cast<std::uint32_t>(difference + (INT64_C(1) << 32U));
            borrow = 1;
        } else {
            left.limb[index] = static_cast<std::uint32_t>(difference);
            borrow = 0;
        }
    }
}

//  `out += value * factor` for a 64-bit value and a 32-bit factor, without a 128-bit integer.
void addScaledValue(Wide& out, std::uint64_t value, std::uint32_t factor) noexcept {
    std::uint64_t carry = 0;
    for (int index = 0; index < 5; ++index) {
        const std::uint64_t part =
            index < 2
                ? (static_cast<std::uint64_t>((value >> (index * 32U)) & 0xFFFFFFFFU) * factor)
                : 0;
        const std::uint64_t sum = out.limb[index] + part + carry;
        out.limb[index] = static_cast<std::uint32_t>(sum);
        carry = sum >> 32U;
    }
}

//  `out = value * factor` for a 64-bit value and a 64-bit factor (a 96-bit value times a 64-bit
//  one is the widest intermediate of the normalized score).
void multiplyValues(Wide& out, const Wide& value, std::uint64_t factor) noexcept {
    std::uint32_t factorLimbs[2] = {static_cast<std::uint32_t>(factor),
                                    static_cast<std::uint32_t>(factor >> 32U)};
    for (int left = 0; left < 3; ++left) {
        if (value.limb[left] == 0) {
            continue;
        }
        for (int right = 0; right < 2; ++right) {
            const std::uint64_t product =
                static_cast<std::uint64_t>(value.limb[left]) * factorLimbs[right];
            if (product == 0) {
                continue;
            }
            int position = left + right;
            std::uint64_t carry = product >> 32U;
            std::uint64_t current = out.limb[position] + (product & 0xFFFFFFFFU);
            out.limb[position] = static_cast<std::uint32_t>(current);
            carry += current >> 32U;
            for (++position; carry != 0 && position < 5; ++position) {
                current = out.limb[position] + carry;
                out.limb[position] = static_cast<std::uint32_t>(current);
                carry = current >> 32U;
            }
        }
    }
}

} // namespace

auto normalizedScoreOutput(std::uint32_t mj, std::uint32_t mc, std::uint32_t total, std::uint64_t j,
                           std::uint64_t jMax, std::uint64_t c, std::uint64_t cMax)
    -> core::Result<std::uint32_t> {
    const std::uint64_t split = static_cast<std::uint64_t>(mj) + static_cast<std::uint64_t>(mc);
    if (split != total) {
        return core::unexpected(scoreError("the score weight split does not add up to the total",
                                           "score_weight_split"));
    }
    if (mj != 0 && jMax == 0) {
        return core::unexpected(
            scoreError("an enabled score component has a zero denominator", "score_basis_zero"));
    }
    if (mc != 0 && cMax == 0) {
        return core::unexpected(
            scoreError("an enabled combo component has a zero denominator", "score_basis_zero"));
    }
    if ((mj != 0 && j > jMax) || (mc != 0 && c > cMax)) {
        return core::unexpected(
            scoreError("the score numerator exceeds its denominator", "score_numerator_range"));
    }
    //  An unenabled component contributes exactly zero even when its denominator is zero.
    const std::uint64_t dj = (mj == 0) ? 1U : jMax;
    const std::uint64_t dc = (mc == 0) ? 1U : cMax;
    Wide numerator{};
    if (mj != 0 && j != 0) {
        //  mj * j * dc: first j * mj (at most 96 bits), then that value * dc.
        Wide jTimesMj{};
        addScaledValue(jTimesMj, j, mj);
        Wide scaled{};
        multiplyValues(scaled, jTimesMj, dc);
        addInto(numerator, scaled);
    }
    if (mc != 0 && c != 0) {
        //  mc * c * dj: first c * mc (at most 96 bits), then that value * dj.
        Wide cTimesMc{};
        addScaledValue(cTimesMc, c, mc);
        Wide scaled{};
        multiplyValues(scaled, cTimesMc, dj);
        addInto(numerator, scaled);
    }
    //  Denominator B = dj * dc, at most 128 bits. The full product is needed because either factor
    //  can exceed 32 bits.
    Wide denominator{};
    {
        std::uint32_t djLimbs[2] = {static_cast<std::uint32_t>(dj),
                                    static_cast<std::uint32_t>(dj >> 32U)};
        std::uint32_t dcLimbs[2] = {static_cast<std::uint32_t>(dc),
                                    static_cast<std::uint32_t>(dc >> 32U)};
        for (int left = 0; left < 2; ++left) {
            for (int right = 0; right < 2; ++right) {
                const std::uint64_t product =
                    static_cast<std::uint64_t>(djLimbs[left]) * dcLimbs[right];
                if (product == 0) {
                    continue;
                }
                int position = left + right;
                std::uint64_t carry = product >> 32U;
                std::uint64_t current = denominator.limb[position] + (product & 0xFFFFFFFFU);
                denominator.limb[position] = static_cast<std::uint32_t>(current);
                carry += current >> 32U;
                for (++position; carry != 0 && position < 5; ++position) {
                    current = denominator.limb[position] + carry;
                    denominator.limb[position] = static_cast<std::uint32_t>(current);
                    carry = current >> 32U;
                }
            }
        }
    }
    if (denominator.isZero()) {
        return core::unexpected(
            scoreError("the normalized score denominator is zero", "score_basis_zero"));
    }
    //  Restoring long division from the highest bit. The quotient never exceeds `total`; the
    //  remainder stays below the denominator before the compare.
    std::uint64_t quotient = 0;
    Wide remainder{};
    for (int bit = 159; bit >= 0; --bit) {
        remainder.shiftLeftOne();
        if (numerator.bit(bit) != 0) {
            remainder.limb[0] |= 1U;
        }
        if (!lessThan(remainder, denominator)) {
            subtractFrom(remainder, denominator);
            if (bit >= 32) {
                return core::unexpected(scoreError(
                    "the normalized score quotient exceeds the total", "score_output_range"));
            }
            quotient |= (std::uint64_t{1} << bit);
        }
    }
    if (quotient > total) {
        return core::unexpected(
            scoreError("the normalized score quotient exceeds the total", "score_output_range"));
    }
    //  Round to nearest, ties to even: compare `2r` with `B`. `r < B < 2^128`, so `2r < 2^129`
    //  still fits the five limbs.
    Wide twiceRemainder = remainder;
    twiceRemainder.shiftLeftOne();
    const bool aboveHalf =
        !lessThan(twiceRemainder, denominator) && !equalTo(twiceRemainder, denominator);
    const bool exactlyHalf = equalTo(twiceRemainder, denominator);
    if (aboveHalf || (exactlyHalf && quotient % 2U == 1U)) {
        ++quotient;
    }
    if (quotient > total) {
        return core::unexpected(
            scoreError("the normalized score output exceeds the total", "score_output_range"));
    }
    return static_cast<std::uint32_t>(quotient);
}

} // namespace cuexis::judgement
