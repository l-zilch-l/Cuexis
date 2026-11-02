#pragma once

// The compiled-pattern storage shapes and the checked budget arithmetic shared by the compile
// and pattern units.

#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {
namespace detail {

enum class CompiledStateKind : std::uint8_t {
    atom,
    sequence,
    choice,
    repeat,
    skip,
    instant,
    complement,
};

//  One interned state. `operands` are state numbers in declared order, which is what makes the
//  leftmost-first arm order observable: arm order is operand order and nothing normalizes it.
struct CompiledState final {
    CompiledStateKind kind{CompiledStateKind::skip};
    std::string atomRef;
    //  The declared repeat bounds. Present for `repeat` only; every other kind leaves them at 0.
    std::uint64_t minimum{0};
    std::uint64_t maximum{0};
    std::vector<std::size_t> operands;
};

//  The representation of a compiled Pattern. It is deliberately private to this module: no field
//  width, no state number and no transition-table encoding of it is part of the judgement
//  interface, and the counts the interface exposes are measurements the compiler took rather than
//  promises about storage.
class CompiledPatternStorage final {
  public:
    std::vector<CompiledState> states;
    std::size_t root{0};
    std::uint64_t declarationDepth{0};
    //  The complete prepare-time expansion of the declaration tree (Spec 3.8.4). Absent exactly
    //  when that count has no representable value, which is a measurement gap: it is never a
    //  refusal by itself, and against an accepted bound it is a proven lower bound of at least
    //  2^64.
    std::optional<std::uint64_t> fullyExpandedCount{std::nullopt};
    //  The `maximum x child` upper bound of one admissible expansion branch. Absent when it has no
    //  representable value. This count has no budget dimension of its own, so its absence is only
    //  ever a measurement gap: nothing is refused on behalf of it.
    std::optional<std::uint64_t> largestBranchExpansion{std::nullopt};
    //  The number of distinct interned subpatterns, which is the size of `states`. It is not the
    //  state count of Spec 8.2 and the two are reported as separate measurements.
    std::uint64_t internedSubpatternCount{0};
    //  The deterministic work count of the compile and the accounting size of the compiled
    //  representation. Both are absent when they have no representable value; in that state a
    //  budget dimension a measurement accepted is still exceeded by them, and a dimension with no
    //  accepted bound refuses nothing.
    std::optional<std::uint64_t> evaluationSteps{std::nullopt};
    std::optional<std::uint64_t> compiledBytes{std::nullopt};
    std::vector<std::string> atomRefs;
    //  The shortest acceptable atom trace. Absent when it has no representable value, which is a
    //  measurement gap: it is never a refusal and never a substituted number.
    std::optional<std::uint64_t> minimumTraceLength{std::nullopt};
    //  Absent when this measurement has no finite upper bound to report: a `complement` consumes a
    //  prefix whose length is not bounded by the declaration, so no finite upper bound is
    //  available. Absent is "no bound is available", never "proven unbounded".
    std::optional<std::uint64_t> maximumTraceLength{std::nullopt};
    //  True when that longest acceptable length is FINITE and has no `uint64` representation: the
    //  exact checked arithmetic that produced it means the true length is at least 2^64, so the
    //  Spec 8.2 state count is at least 2^64 + 1 and therefore above every representable accepted
    //  bound. It is the one entry of this storage that is a PROVEN LOWER BOUND of another count.
    bool maximumTraceLengthGap{false};
};

//  The representation of a compiled Measure.
class CompiledMeasureStorage final {
  public:
    std::vector<CompiledMeasureComponent> components;
};

class ResourceClaimResolutionStorage final {
  public:
    std::vector<ResourceClaimResolutionInputs::Candidate> candidates;
};

} // namespace detail

[[nodiscard]] constexpr auto checkedAdd(std::uint64_t left, std::uint64_t right)
    -> std::optional<std::uint64_t> {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        return std::nullopt;
    }
    return left + right;
}

[[nodiscard]] constexpr auto checkedMultiply(std::uint64_t left, std::uint64_t right)
    -> std::optional<std::uint64_t> {
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return std::nullopt;
    }
    return left * right;
}

[[nodiscard]] inline auto addChecked(std::uint64_t left, std::uint64_t right, bool& overflow)
    -> std::uint64_t {
    const auto sum = checkedAdd(left, right);
    if (!sum.has_value()) {
        overflow = true;
        return 0U;
    }
    return *sum;
}

[[nodiscard]] inline auto multiplyChecked(std::uint64_t left, std::uint64_t right, bool& overflow)
    -> std::uint64_t {
    const auto product = checkedMultiply(left, right);
    if (!product.has_value()) {
        overflow = true;
        return 0U;
    }
    return *product;
}

//  The sum over `k` in `[minimum, maximum]` of `k`, in checked 64-bit arithmetic: the number of
//  copies the complete prepare-time expansion of a bounded repeat materialises. The caller has
//  already checked the declared order of the bounds, so the range is never empty.
[[nodiscard]] inline auto copyCountChecked(std::uint64_t minimum, std::uint64_t maximum,
                                           bool& overflow) -> std::uint64_t {
    const auto count = checkedAdd(maximum - minimum, 1U);
    const auto total = checkedAdd(minimum, maximum);
    if (!count.has_value() || !total.has_value()) {
        overflow = true;
        return 0U;
    }
    //  One of the two factors is even, so halving the even one before multiplying keeps every
    //  intermediate step exact and never rounds a count down.
    if (*count % 2U == 0U) {
        return multiplyChecked(*count / 2U, *total, overflow);
    }
    return multiplyChecked(*count, *total / 2U, overflow);
}

//  The interior accounting of one compiled state, in this module's own units. It is an accounting
//  of an internal representation: not a wire size, not an encoding, and not a claim about what a
//  host allocator spends.
constexpr std::uint64_t kStateAccountingBytes = 24U;
constexpr std::uint64_t kOperandAccountingBytes = 8U;

//  The sum of two counts that may each already have no representable value. Once a count has none
//  it stays that way: no later step brings it back, and the sum is carried as a gap rather than as
//  a wrapped or saturated number.
[[nodiscard]] inline auto addCounts(std::uint64_t left, bool leftGap, std::uint64_t right,
                                    bool rightGap, bool& gap) -> std::uint64_t {
    if (leftGap || rightGap) {
        gap = true;
        return 0U;
    }
    return addChecked(left, right, gap);
}

//  The product of two counts with the same rule: an operand that already has no representable value
//  makes the product a gap as well.
//
//  A factor that is exactly zero is excluded from that rule, because zero times anything is exactly
//  zero. This is not a nicety: `repeat(X, 0, 0)` contains no copy of `X` at all, so a count with no
//  representable value inside `X` cannot make the repeat's own count unrepresentable. A gap is
//  reported against a budget dimension as a certain overrun, so propagating one through a zero
//  factor would turn a count of zero into an apparent overrun -- exactly the kind of false refusal
//  this batch refuses to freeze.
[[nodiscard]] inline auto multiplyCounts(std::uint64_t left, bool leftGap, std::uint64_t right,
                                         bool rightGap, bool& gap) -> std::uint64_t {
    const bool leftIsZero = !leftGap && left == 0U;
    const bool rightIsZero = !rightGap && right == 0U;
    if (leftIsZero || rightIsZero) {
        return 0U;
    }
    if (leftGap || rightGap) {
        gap = true;
        return 0U;
    }
    return multiplyChecked(left, right, gap);
}

//  The RELATIVE early stop of one budgeted count: the running count is compared with the accepted
//  upper bound of its dimension while it is accumulated, so content that has already passed a bound
//  a measurement accepted stops the compile at that point instead of being carried further. A
//  dimension with no accepted bound has nothing to compare against and refuses nothing.
//
//  `gap` reports a count that has no representable value at all. The accumulation that produced it
//  is exact and monotone, so such a count is a PROVEN LOWER BOUND of at least 2^64: against an
//  accepted bound it is a certain overrun, because every representable bound is smaller than it.
//  Without an accepted bound it stays the measurement gap it is. The accepted bound is never
//  substituted into the count.
[[nodiscard]] inline auto earlyStopAgainst(std::uint64_t measured, bool gap,
                                           const MeasuredParameter<std::uint64_t>& limit,
                                           std::string_view path) -> core::Result<void> {
    const std::uint64_t* accepted = limit.measuredValue();
    if (accepted == nullptr) {
        return {};
    }
    if (gap || measured > *accepted) {
        return core::unexpected(
            valueError(codes::kPatternBudgetExceededCode, codes::kGraphSection,
                       std::string{codes::kPatternPath} + "." + std::string{path},
                       "the compiled pattern exceeds a budget dimension a measurement accepted"));
    }
    return {};
}

//  The measured shape of one compiled subpattern.
//
//  Two different expansion numbers are measured and they answer different questions:
//
//    * `largestBranch` is the `maximum x child` upper bound of one repeat copy set: the largest
//      number of primitive instances a single admissible expansion branch contains. It is the
//      coarse static bound of the declaration, and it has NO budget dimension of its own, so an
//      unrepresentable value is only ever a gap in this measurement;
//    * `fullyExpanded` is the number of primitive instances of the complete prepare-time expansion
//      (Spec 3.8.4): the sum over every admissible copy count of a bounded repeat. For
//      `repeat(atom, 2, 4)` it is 2 + 3 + 4 = 9 while the largest branch is 4; for
//      `repeat(atom, 1, UINT64_MAX)` the sum has no representable value while the largest branch is
//      representable. `fullyExpandedGap` records exactly that case, and it is NOT a refusal by
//      itself: the count is reported as an absent measurement, and a budget dimension a measurement
//      accepted is still exceeded by it, because the arithmetic is exact and monotone, so the true
//      count is at least 2^64 and every representable accepted limit is smaller than it.
//
//  A repeat contributes its copies and not itself; every other node contributes itself plus its
//  operands, so a leaf is 1 and `sequence(atom, atom)` is 3 in both numbers. All of the arithmetic
//  is checked, and a count that has no representable value is a gap rather than a refusal.
//
//  The longest acceptable length is the one entry here that also serves as a PROVEN LOWER BOUND of
//  another count, because the state count of the minimal automaton is at least one more than it
//  whenever that length is finite (see `PatternStateCountMeasurement`). It therefore has three
//  states and not two: a value, "no representable value while still finite" (a proven lower bound
//  of at least 2^64 of the state count) and "no finite length at all" (no bound proven, and no

struct PatternStateCountMeasurement final {
    //  The count itself. Absent when the construction did not complete inside its own working-set
    //  bound, which is a measurement gap of this module: the module's own measurement bound is
    //  never a content limit and never a refusal.
    std::optional<std::uint64_t> value;
    //  The proven lower bound of the count. Absent when the declaration has no finite longest
    //  acceptable length, in which case no bound at all is proven and nothing may be compared.
    std::optional<std::uint64_t> provenLowerBound;
    //  True when the proven lower bound itself has no `uint64` representation. The count is then at
    //  least 2^64 and therefore above every representable accepted bound.
    bool provenLowerBoundNotRepresentable{false};
};

class PatternTraceEvaluator final {
  public:
    PatternTraceEvaluator(const detail::CompiledPatternStorage& storage,
                          const std::vector<std::string>& trace)
        : storage_(storage), trace_(trace), width_(trace.size() + 1U),
          bits_(storage.states.size() * width_, std::vector<std::uint8_t>(width_, 0U)) {
        for (std::size_t state = 0; state < storage_.states.size(); ++state) {
            for (std::size_t position = 0; position < width_; ++position) {
                evaluate(state, position);
            }
        }
    }

    [[nodiscard]] auto accepts(std::size_t state, std::size_t position) const -> bool {
        return bits_[state * width_ + position][trace_.size()] != 0U;
    }

  private:
    [[nodiscard]] auto cell(std::size_t state, std::size_t position) const
        -> const std::vector<std::uint8_t>& {
        return bits_[state * width_ + position];
    }

    void unite(std::vector<std::uint8_t>& into, const std::vector<std::uint8_t>& from) const {
        for (std::size_t index = 0; index < width_; ++index) {
            if (from[index] != 0U) {
                into[index] = 1U;
            }
        }
    }

    //  Advances every position of `current` by one copy of `child`.
    [[nodiscard]] auto advance(std::size_t child, const std::vector<std::uint8_t>& current,
                               bool& any) const -> std::vector<std::uint8_t> {
        std::vector<std::uint8_t> next(width_, 0U);
        for (std::size_t from = 0; from < width_; ++from) {
            if (current[from] == 0U) {
                continue;
            }
            any = true;
            unite(next, cell(child, from));
        }
        return next;
    }

    void evaluate(std::size_t stateIndex, std::size_t position) {
        const detail::CompiledState& state = storage_.states[stateIndex];
        std::vector<std::uint8_t>& target = bits_[stateIndex * width_ + position];
        switch (state.kind) {
        case detail::CompiledStateKind::atom:
            if (position < trace_.size() && trace_[position] == state.atomRef) {
                target[position + 1U] = 1U;
            }
            return;
        case detail::CompiledStateKind::skip:
        case detail::CompiledStateKind::instant:
            target[position] = 1U;
            return;
        case detail::CompiledStateKind::sequence: {
            std::vector<std::uint8_t> current(width_, 0U);
            current[position] = 1U;
            for (const std::size_t child : state.operands) {
                bool any = false;
                current = advance(child, current, any);
            }
            target = std::move(current);
            return;
        }
        case detail::CompiledStateKind::choice:
            for (const std::size_t arm : state.operands) {
                unite(target, cell(arm, position));
            }
            return;
        case detail::CompiledStateKind::complement: {
            //  A complement accepts exactly the prefixes that none of its operands reaches, so it
            //  consumes a prefix and composes with whatever follows it: from `position`, every end
            //  `end` such that no operand accepts `trace[position, end)` is accepted. Reading it as
            //  "the whole remaining trace" would make `sequence(complement(X), Y)` lose every split
            //  in which `Y` consumes something.
            for (std::size_t end = position; end < width_; ++end) {
                bool operandAccepts = false;
                for (const std::size_t child : state.operands) {
                    if (cell(child, position)[end] != 0U) {
                        operandAccepts = true;
                        break;
                    }
                }
                if (!operandAccepts) {
                    target[end] = 1U;
                }
            }
            return;
        }
        case detail::CompiledStateKind::repeat: {
            //  The copy order is the increasing copy index `k = 0, 1, 2, ...`, and the accepted set
            //  is the union over `k` in `[minimum, maximum]` of `k` copies of the operand. The
            //  union does not depend on the order the copies are visited in, so the deterministic
            //  order is a property of the expansion and not of the result.
            const std::size_t child = state.operands.front();
            std::vector<std::uint8_t> current(width_, 0U);
            current[position] = 1U;
            std::uint64_t copies = 0U;
            while (true) {
                if (copies >= state.minimum) {
                    unite(target, current);
                }
                if (copies >= state.maximum) {
                    return;
                }
                bool any = false;
                std::vector<std::uint8_t> next = advance(child, current, any);
                if (!any) {
                    //  The position set is empty, so no further copy can accept anything.
                    return;
                }
                if (next == current) {
                    //  One more copy reaches exactly the same positions, so every later copy does
                    //  too. A copy does NOT have to consume an element: when the operand accepts
                    //  the empty word this is the fixed point that ends the expansion, and the
                    //  acceptance of every still acceptable copy count is the acceptance of this
                    //  set, which is why it is united here whether or not `minimum` was already
                    //  reached. The declared finite bound therefore never depends on a copy
                    //  consuming anything, and the loop runs at most `minimum`/`maximum` or one
                    //  step per trace element, whichever is smaller, because each step either
                    //  advances a position or stops.
                    unite(target, current);
                    return;
                }
                current = std::move(next);
                ++copies;
            }
        }
        }
    }

    const detail::CompiledPatternStorage& storage_;
    const std::vector<std::string>& trace_;
    std::size_t width_;
    std::vector<std::vector<std::uint8_t>> bits_;
};

[[nodiscard]] auto buildCompiledPattern(const PatternDeclaration& pattern,
                                        const PatternCompileBudget& budget)
    -> core::Result<detail::CompiledPatternStorage>;
[[nodiscard]] auto measurePatternStateCount(const detail::CompiledPatternStorage& storage)
    -> PatternStateCountMeasurement;
[[nodiscard]] auto enforcePatternBudget(const detail::CompiledPatternStorage& storage,
                                        const PatternCompileBudget& budget) -> core::Result<void>;
[[nodiscard]] auto buildCompiledMeasure(const MeasureSpecDeclaration& measure,
                                        const MeasurePhaseContext& context)
    -> core::Result<detail::CompiledMeasureStorage>;

} // namespace cuexis::judgement
