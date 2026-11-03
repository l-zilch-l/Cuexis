#pragma once

// Pattern DFA construction: the materialised NFA, repeats and determinisation.
//
// Fragment of the automaton layer: include "pattern_dfa.hpp", never this file directly. The
// parts wire each other in one order and are not individually self-contained.

#include "pattern_dfa.hpp"

namespace cuexis::judgement::detail {

//  A view over a MaterialisedNfa as the determinisation consumes it.
class MaterialisedNfa final {
  public:
    std::size_t symbolCount{0};
    std::size_t start{0};
    std::vector<std::uint8_t> accepting;
    std::vector<std::size_t> transitions;
    std::vector<std::vector<std::size_t>> epsilon;

    //  Appends one child automaton and returns the offset its states start at. The child's
    //  transition targets are relative to the child, so they are shifted by the offset here: a copy
    //  that kept them would send a later child's transition into an earlier child's state.
    [[nodiscard]] auto append(const PartialDfa& child) -> std::size_t {
        const std::size_t offset = accepting.size();
        epsilon.resize(checkedDfaStateSum(offset, child.accepting.size()));
        accepting.insert(accepting.end(), child.accepting.begin(), child.accepting.end());
        const std::size_t first = transitions.size();
        transitions.insert(transitions.end(), child.transitions.begin(), child.transitions.end());
        for (std::size_t entry = first; entry < transitions.size(); ++entry) {
            if (transitions[entry] != kNoTransition) {
                transitions[entry] += offset;
            }
        }
        return offset;
    }

    [[nodiscard]] auto viewStart() const noexcept -> std::size_t {
        return start;
    }
    [[nodiscard]] auto viewAccepting(std::size_t id) const noexcept -> bool {
        return accepting[id] != 0U;
    }
    void viewEpsilonSuccessors(std::size_t id, std::vector<std::size_t>& out) const {
        out.insert(out.end(), epsilon[id].begin(), epsilon[id].end());
    }
    [[nodiscard]] auto viewSymbolSuccessor(std::size_t id, std::size_t symbol) const noexcept
        -> std::size_t {
        return transitions[id * symbolCount + symbol];
    }
    [[nodiscard]] auto viewIdCount() const noexcept -> std::size_t {
        return accepting.size();
    }
    [[nodiscard]] auto viewSymbolCount() const noexcept -> std::size_t {
        return symbolCount;
    }
};

//  Nondeterministic presentation of `child` repeated between `minimum` and `maximum` times.
//
//  A state is a pair (child state, number of copies completed so far), plus one dedicated start
//  state that exists so that "zero copies" is expressible: the start state accepts the empty word
//  exactly when `minimum` is zero, which a pair state cannot say because a pair state can also be
//  reached after consuming symbols. Finishing a copy may either stay inside the same copy or start
//  the next one, which is the nondeterminism the determinisation resolves. The pair space is
//  generated lazily, so a bound of the largest representable value costs the construction bound
//  rather than that many states.
class RepeatNfa final {
  public:
    RepeatNfa(const PartialDfa& child, std::uint64_t minimum, std::uint64_t maximum,
              ConstructionBudget& budget)
        : child_(child), minimum_(minimum), maximum_(maximum), budget_(&budget) {
        //  The pair space of this repeat is `(child states + 1) x (maximum + 1)` pairs, and it is
        //  known before anything is explored. When it does not fit the working-set bound the
        //  measurement is abandoned immediately instead of being explored up to the bound: the
        //  bound is a limit of this module's measurement, and the declared maximum stays legal
        //  content either way.
        if (budget.isMeasurement() &&
            !patternRepeatPairSpace(child.accepting.size(), maximum).has_value()) {
            budget.abandon();
        }
    }

    [[nodiscard]] auto viewStart() const -> std::size_t {
        return kStartId;
    }

    [[nodiscard]] auto viewAccepting(std::size_t id) const -> bool {
        if (id == kStartId) {
            //  Zero copies, which is admissible exactly when the lower bound admits it.
            return minimum_ == 0U;
        }
        const auto [state, copies] = decode(id);
        return child_.accepting[state] != 0U && copies + 1U >= minimum_ && copies + 1U <= maximum_;
    }

    void viewEpsilonSuccessors(std::size_t id, std::vector<std::size_t>& out) const {
        if (id == kStartId) {
            out.push_back(idOf(child_.start, 0U));
            return;
        }
        const auto [state, copies] = decode(id);
        if (child_.accepting[state] != 0U && copies < maximum_) {
            out.push_back(idOf(child_.start, copies + 1U));
        }
    }

    [[nodiscard]] auto viewSymbolSuccessor(std::size_t id, std::size_t symbol) const
        -> std::size_t {
        if (id == kStartId) {
            return kNoTransition;
        }
        const auto [state, copies] = decode(id);
        const std::size_t target = child_.step(state, symbol);
        if (target == kNoTransition) {
            return kNoTransition;
        }
        return idOf(target, copies);
    }

    [[nodiscard]] auto viewIdCount() const noexcept -> std::size_t {
        return byId_.size() + 1U;
    }
    [[nodiscard]] auto viewSymbolCount() const noexcept -> std::size_t {
        return child_.symbolCount;
    }

  private:
    static constexpr std::size_t kStartId = 0U;

    [[nodiscard]] auto idOf(std::size_t state, std::uint64_t copies) const -> std::size_t {
        const auto key = std::pair{state, copies};
        const auto found = ids_.find(key);
        if (found != ids_.end()) {
            return found->second;
        }
        if (budget_->isMeasurement() &&
            byId_.size() + 1U >= static_cast<std::size_t>(kPatternDfaConstructionStates)) {
            //  The measurement is over; the caller abandons it. Returning an existing id keeps this
            //  function total so that no partially built subset can be interpreted as a real one.
            budget_->chargeState();
            return byId_.empty() ? kStartId : kStartId + 1U;
        }
        const std::size_t id = kStartId + byId_.size() + 1U;
        ids_.emplace(key, id);
        byId_.push_back(key);
        budget_->chargeState();
        return id;
    }

    [[nodiscard]] auto decode(std::size_t id) const -> std::pair<std::size_t, std::uint64_t> {
        if (id > kStartId && id - kStartId - 1U < byId_.size()) {
            return byId_[id - kStartId - 1U];
        }
        return {child_.start, 0U};
    }

    const PartialDfa& child_;
    std::uint64_t minimum_{0};
    std::uint64_t maximum_{0};
    ConstructionBudget* budget_{nullptr};
    //  The pair space, allocated lazily, in id order. `mutable` because the view interface is
    //  const.
    mutable std::map<std::pair<std::size_t, std::uint64_t>, std::size_t> ids_;
    mutable std::vector<std::pair<std::size_t, std::uint64_t>> byId_;
};

//  The epsilon closure of `seeds`, sorted and duplicate free.
template <class View>
void epsilonClosure(const View& view, const std::vector<std::size_t>& seeds,
                    std::vector<std::size_t>& out, ConstructionBudget& budget) {
    std::vector<std::uint8_t> marked;
    std::vector<std::size_t> pending;
    const auto mark = [&marked](std::size_t id) {
        if (id >= marked.size()) {
            marked.resize(id + 1U, 0U);
        }
        if (marked[id] != 0U) {
            return false;
        }
        marked[id] = 1U;
        return true;
    };
    for (const std::size_t seed : seeds) {
        if (mark(seed)) {
            pending.push_back(seed);
        }
    }
    std::vector<std::size_t> successors;
    while (!pending.empty()) {
        const std::size_t id = pending.back();
        pending.pop_back();
        out.push_back(id);
        budget.chargeWork();
        if (!budget.ok()) {
            return;
        }
        successors.clear();
        view.viewEpsilonSuccessors(id, successors);
        for (const std::size_t successor : successors) {
            if (mark(successor)) {
                pending.push_back(successor);
            }
        }
    }
    std::sort(out.begin(), out.end());
}

//  The subset construction of a lazily presented nondeterministic automaton. The subsets are keyed
//  in an ordered map, so one input always produces the same automaton.
template <class View>
[[nodiscard]] auto determinise(const View& view, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    PartialDfa dfa;
    dfa.symbolCount = view.viewSymbolCount();
    std::map<std::vector<std::size_t>, std::size_t> index;
    std::vector<std::vector<std::size_t>> subsets;

    std::vector<std::size_t> startSubset;
    epsilonClosure(view, {view.viewStart()}, startSubset, budget);
    if (!budget.ok()) {
        return std::nullopt;
    }
    index.emplace(startSubset, 0U);
    subsets.push_back(std::move(startSubset));
    dfa.start = 0U;
    budget.chargeState();

    std::vector<std::size_t> moved;
    std::vector<std::size_t> closure;
    for (std::size_t cursor = 0; cursor < subsets.size(); ++cursor) {
        const std::vector<std::size_t> subset = subsets[cursor];
        bool accepting = false;
        for (const std::size_t id : subset) {
            accepting = accepting || view.viewAccepting(id);
        }
        dfa.accepting.push_back(accepting ? 1U : 0U);
        dfa.transitions.resize(
            checkedDfaTableEntries(checkedDfaStateSum(cursor, 1U), dfa.symbolCount), kNoTransition);

        for (std::size_t symbol = 0; symbol < dfa.symbolCount; ++symbol) {
            moved.clear();
            for (const std::size_t id : subset) {
                const std::size_t target = view.viewSymbolSuccessor(id, symbol);
                if (target == kNoTransition) {
                    continue;
                }
                if (std::find(moved.begin(), moved.end(), target) == moved.end()) {
                    moved.push_back(target);
                }
            }
            if (moved.empty()) {
                continue;
            }
            closure.clear();
            epsilonClosure(view, moved, closure, budget);
            if (!budget.ok()) {
                return std::nullopt;
            }
            if (closure.empty()) {
                continue;
            }
            const auto [found, inserted] = index.try_emplace(closure, subsets.size());
            if (inserted) {
                subsets.push_back(closure);
                budget.chargeState();
                if (!budget.ok()) {
                    return std::nullopt;
                }
            }
            dfa.transitions[cursor * dfa.symbolCount + symbol] = found->second;
        }
        if (!budget.ok()) {
            return std::nullopt;
        }
    }
    return dfa;
}

} // namespace cuexis::judgement::detail
