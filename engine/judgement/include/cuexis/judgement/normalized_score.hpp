#pragma once

//  Judgement runtime - normalized score records and the shared integer score helpers.
//
//  The author adapter (T-12) and the prepared-artifact Reader (T-13) share these records; the
//  running kernel folds them. Nothing here is a wire format: the layout is local to the build and
//  the values never leave the process through a binary encoding. Generic code, `<cstdint>` and
//  Core only - no author type, no Chart type and no float arithmetic enters the integer helpers.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/phase_types.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement {

//  What one scoring fact does to the combo of its unit.
enum class ComboAction : std::uint8_t { keep, increment, reset };

//  One exact `(phase, outcome, optional grade)` scoring key.
struct ScoreRuleRow final {
    PhaseKind phase;
    Outcome outcome;
    std::optional<std::string> grade;
    bool qualifies;
    std::uint64_t judgePoints;
    ComboAction combo;
};

//  One combo reward segment: from this combo index at `from`, the reward grows by `slope` per
//  further point. `from` is a combo index, never a unit index.
struct ComboRewardSegment final {
    std::uint32_t from;
    std::uint64_t rewardAtFrom;
    std::uint32_t slope;
};

//  One cross-chart mode: the two enabled normalized weights, the total, the combo reward function
//  and the exact scoring keys.
struct ScoreMode final {
    std::string id;
    std::uint32_t total, mj, mc;
    ComboAction onExhausted;
    std::vector<ComboRewardSegment> comboRewards;
    std::vector<ScoreRuleRow> rules;
};

//  One grade interval `[start, end)` in tick units, relative to the phase target.
struct PreparedGradeInterval final {
    std::int64_t start, end;
    std::string grade;
};

//  One grade table: its exact band and its ordered grade intervals.
struct PreparedGradeTable final {
    std::string id;
    std::int64_t exactStart, exactEnd;
    std::vector<PreparedGradeInterval> intervals;
};

//  One scoring unit: the phase instance indices that compete for its single award, ascending.
struct ScoreUnit final {
    std::vector<std::uint32_t> phaseIndices;
};

//  The full-chart denominators, derived from the prepared records by `deriveScoreBasis`.
struct ScoreBasis final {
    std::uint64_t jMax, cMax;
};

//  The complete normalized scoring product one prepared gameplay owns.
struct PreparedNormalizedScore final {
    ScoreMode mode;
    std::vector<PreparedGradeTable> gradeTables;
    std::vector<ScoreUnit> units;
    ScoreBasis basis;
};

//  ---------------------------------------------------------------------------------------------
//  Shared integer helpers (the exact math every producer and the fold use)
//  ---------------------------------------------------------------------------------------------

//  The checked reward of one combo index in `segment`:
//  `rewardAtFrom + slope * (combo - from)`.
[[nodiscard]] auto comboRewardAt(const ComboRewardSegment& segment, std::uint32_t combo)
    -> core::Result<std::uint64_t>;

//  The checked reward of one combo index under the mode's reward table. `combo` must be at least
//  the first segment's `from`.
[[nodiscard]] auto comboReward(const ScoreMode& mode, std::uint32_t combo)
    -> core::Result<std::uint64_t>;

//  `Cmax` for exactly `qualifyingUnits` incrementing units: the checked sum of `reward(k)` for
//  `k = 1 .. qualifyingUnits`. Empty for zero units.
[[nodiscard]] auto deriveComboBasis(const ScoreMode& mode, std::uint32_t qualifyingUnits)
    -> core::Result<std::uint64_t>;

//  Structural validation of one normalized score, without requirements: the weight split, the
//  reward segments, the rule keys and the rule qualification invariant, the grade table ordering
//  and the unit ordering. Unit membership itself is validated against the prepared requirements by
//  `deriveScoreBasis`.
[[nodiscard]] auto validatePreparedNormalizedScore(const PreparedNormalizedScore& score)
    -> core::Result<void>;

//  The grade whose interval contains `error`, or null when no interval does. Intervals are
//  half-open and the prepared table is known to be ordered and gap-free after validation.
[[nodiscard]] auto preparedGradeAt(const PreparedGradeTable& table, std::int64_t error) noexcept
    -> const std::string*;

//  The exact normalized output for one completed run (GP-05):
//
//      dj = (mj == 0 ? 1 : jMax)          dc = (mc == 0 ? 1 : cMax)
//      A  = mj * j * dc + mc * c * dj     B  = dj * dc
//      q, r = A / B, A % B
//      output = q, plus one when r is more than half of B, and on an exact half only when q is odd
//
//  Every input is checked before the division: the weight split is exact, an enabled component
//  needs a positive denominator, the numerators never exceed their denominators, and the result
//  stays within `[0, total]`. The 160-bit numerator and 128-bit denominator are held in fixed
//  32-bit limbs on the stack; no `__int128`, floating-point mode or external big integer is used.
[[nodiscard]] auto normalizedScoreOutput(std::uint32_t mj, std::uint32_t mc, std::uint32_t total,
                                         std::uint64_t j, std::uint64_t jMax, std::uint64_t c,
                                         std::uint64_t cMax) -> core::Result<std::uint32_t>;

} // namespace cuexis::judgement
