#pragma once

//  S7A-3 second half (first part): the automaton layer behind the compiled Pattern's determinised
//  and minimised state count.
//
//  Spec 8.2 counts a Pattern's states after determinisation and minimisation and never by NFA size,
//  so the compiled product needs a real construction to report that number. This header owns that
//  construction and nothing else: it is internal to the judgement module, it is not part of any
//  interface, and the compiled Pattern keeps its own interned representation.
//
//  The number it produces is a MEASUREMENT, not a limit. The construction is therefore bounded by
//  its own working set and reports "unavailable" instead of refusing a declaration, and no caller
//  may turn that bound into a content threshold. The state budget is enforced on the number this
//  header produced, or on a PROVEN LOWER BOUND of that number that the declaration itself yields
//  (the host of this construction derives it from the longest acceptable trace length, which is
//  content): an available count above an accepted bound is an overrun, a proven lower bound above
//  an accepted bound is an overrun, and an unavailable count below or without such a bound is only
//  a measurement gap. "Unavailable" from this header alone therefore never decides anything.
//
//  The alphabet is finite by construction. Every atom reference the pattern names is one symbol,
//  and one extra symbol stands for every reference the pattern does not name: an atom matches its
//  own reference and nothing else, so all unnamed references behave identically and a single extra
//  symbol class is exact rather than an approximation.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cuexis::judgement::detail {

[[nodiscard]] inline auto checkedDfaTableEntries(std::size_t states, std::size_t symbols)
    -> std::size_t {
    const auto maximum = std::vector<std::size_t>{}.max_size();
    if (symbols != 0 && states > maximum / symbols) {
        throw std::length_error{"DFA table size is not representable"};
    }
    return states * symbols;
}
[[nodiscard]] inline auto checkedDfaStateSum(std::size_t left, std::size_t right) -> std::size_t {
    const auto maximum = std::vector<std::uint8_t>{}.max_size();
    if (left > maximum || right > maximum - left) {
        throw std::length_error{"DFA state count is not representable"};
    }
    return left + right;
}

//  The "no such state" sentinel of a partial automaton. An undefined transition rejects.
inline constexpr std::size_t kNoTransition = std::numeric_limits<std::size_t>::max();

//  The working-set bound of one measurement: `kPatternDfaConstructionStates` bounds how many
//  automaton states one construction may materialise, and `kPatternDfaConstructionWork` bounds how
//  many construction steps it may take. They exist so that a measurement can never exhaust memory
//  or time, and they are NOT limits on any content: a declaration that needs a larger construction
//  is not refused on their behalf, its state count is simply reported as unavailable, and nothing
//  else about the compile changes. Whether the state budget is exceeded is decided by the count or
//  by a proven lower bound of it, never by these bounds.
inline constexpr std::uint64_t kPatternDfaConstructionStates = 1U << 16;
inline constexpr std::uint64_t kPatternDfaConstructionWork = 1U << 24;

//  The pair space of a repeat, `(child states + 1) x (maximum + 1)` pairs, or nothing when it has
//  no representable value or does not fit the working-set bound of one construction. Knowing it
//  before the exploration starts keeps a repeat whose pair space cannot fit from being explored up
//  to the bound, and it is a limit of the measurement and never of the declared bound.
[[nodiscard]] inline auto patternRepeatPairSpace(std::size_t childStates, std::uint64_t maximum)
    -> std::optional<std::uint64_t> {
    if (maximum >= kPatternDfaConstructionStates ||
        childStates > static_cast<std::size_t>(kPatternDfaConstructionStates)) {
        return std::nullopt;
    }
    const auto pairs = static_cast<std::uint64_t>(childStates + 1U) * (maximum + 1U);
    if (pairs > kPatternDfaConstructionStates) {
        return std::nullopt;
    }
    return pairs;
}

//  The shared counter of one `buildMinimalPatternDfa` call. It is charged per constructed state and
//  per construction step; once it is exceeded the caller abandons the measurement.
class ConstructionBudget final {
  public:
    explicit ConstructionBudget(bool measurement = true) : measurement_(measurement) {}
    [[nodiscard]] auto isMeasurement() const noexcept -> bool {
        return measurement_;
    }
    [[nodiscard]] auto ok() const noexcept -> bool {
        return !exceeded_;
    }

    void chargeState() noexcept {
        if (states_ == std::numeric_limits<std::uint64_t>::max()) {
            exceeded_ = true;
            return;
        }
        ++states_;
        if (measurement_ && states_ > kPatternDfaConstructionStates) {
            exceeded_ = true;
        }
    }

    void chargeWork(std::uint64_t units = 1U) noexcept {
        if (!measurement_) {
            return;
        }
        if (units > std::numeric_limits<std::uint64_t>::max() - work_) {
            exceeded_ = true;
            return;
        }
        work_ += units;
        if (measurement_ && work_ > kPatternDfaConstructionWork) {
            exceeded_ = true;
        }
    }

    //  Gives up the whole measurement at once, for a construction whose size is known before it
    //  starts to be explored. It is the same outcome as reaching the bound the hard way: the state
    //  count is reported as unavailable, and no declaration is refused on behalf of this bound. The
    //  state budget is still decided on what the content proves, which this call does not touch.
    void abandon() noexcept {
        states_ = kPatternDfaConstructionStates + 1U;
        exceeded_ = true;
    }

    [[nodiscard]] auto states() const noexcept -> std::uint64_t {
        return states_;
    }

  private:
    bool measurement_;
    std::uint64_t states_{0};
    std::uint64_t work_{0};
    bool exceeded_{false};
};

//  A deterministic PARTIAL automaton: an undefined transition is a rejection, and the implicit dead
//  state behind it is not one of the states. The state count of this type is the number of states a
//  trace can occupy, which is the counting object of Spec 8.2 and not an NFA size.
struct PartialDfa final {
    std::size_t symbolCount{0};
    std::size_t start{0};
    std::vector<std::uint8_t> accepting;
    //  `state * symbolCount + symbol`, or `kNoTransition`.
    std::vector<std::size_t> transitions;

    [[nodiscard]] auto stateCount() const noexcept -> std::size_t {
        return accepting.size();
    }

    [[nodiscard]] auto step(std::size_t state, std::size_t symbol) const noexcept -> std::size_t {
        return transitions[state * symbolCount + symbol];
    }

    [[nodiscard]] auto accepts(const std::vector<std::size_t>& word) const noexcept -> bool {
        std::size_t state = start;
        for (const std::size_t symbol : word) {
            state = step(state, symbol);
            if (state == kNoTransition) {
                return false;
            }
        }
        return accepting[state] != 0U;
    }
};

//  A deterministic automaton, as this header hands it to its caller: `available == false` means the
//  construction did not complete inside its own bound, which is a measurement gap and never a
//  statement about the declaration. It says nothing about whether the state budget was exceeded:
//  that is decided by a caller on an available count or on a proven lower bound of the count, and
//  this flag distinguishes neither of the reasons a construction can stop.
struct PatternDfaResult final {
    bool available{false};
    PartialDfa dfa{};
};

//  The language of the empty word only. Stage 7A `skip` and `instant` both compile to it.
[[nodiscard]] inline auto epsilonDfa(std::size_t symbolCount) -> PartialDfa {
    PartialDfa dfa;
    dfa.symbolCount = symbolCount;
    dfa.start = 0U;
    dfa.accepting.assign(1U, 1U);
    dfa.transitions.assign(symbolCount, kNoTransition);
    return dfa;
}

//  The language of the single one-symbol word `symbol`.
[[nodiscard]] inline auto atomDfa(std::size_t symbol, std::size_t symbolCount) -> PartialDfa {
    PartialDfa dfa;
    dfa.symbolCount = symbolCount;
    dfa.start = 0U;
    dfa.accepting = {0U, 1U};
    dfa.transitions.assign(checkedDfaTableEntries(2U, symbolCount), kNoTransition);
    dfa.transitions[symbol] = 1U;
    return dfa;
}

} // namespace cuexis::judgement::detail

#include "pattern_dfa_node.hpp"
