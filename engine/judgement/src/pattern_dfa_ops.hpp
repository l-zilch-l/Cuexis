#pragma once

// Pattern DFA combinators: union, concatenation, complement and repeats.

#include "pattern_dfa_reduce.hpp"

namespace cuexis::judgement::detail {

//  The minimal partial automaton of the language of `source`. `refine == false` stops after the
//  trimming, which keeps the language of every intermediate node of a declaration exactly and
//  leaves the merging to the single refinement of the root.
[[nodiscard]] inline auto minimise(const PartialDfa& source, ConstructionBudget& budget,
                                   bool refine = true) -> std::optional<PartialDfa> {
    const auto trimmed = trim(source, budget);
    if (!trimmed.has_value()) {
        return std::nullopt;
    }
    if (!refine) {
        return trimmed;
    }
    return mergeEquivalent(*trimmed, budget);
}

//  The union of the children, as a deterministic partial automaton.
[[nodiscard]] inline auto unionOf(const std::vector<const PartialDfa*>& children,
                                  std::size_t symbolCount, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    MaterialisedNfa nfa;
    nfa.symbolCount = symbolCount;
    nfa.start = 0U;
    nfa.epsilon.resize(1U);
    for (const PartialDfa* child : children) {
        const std::size_t offset = nfa.append(*child);
        //  The union only needs one epsilon edge from the new start state to each child start.
        nfa.epsilon[0].push_back(offset + child->start);
    }
    const auto determinised = determinise(nfa, budget);
    if (!determinised.has_value()) {
        return std::nullopt;
    }
    return minimise(*determinised, budget, false);
}

//  The concatenation of the children, in order, as a deterministic partial automaton. The empty
//  child list is the language of the empty word.
[[nodiscard]] inline auto concatenationOf(const std::vector<const PartialDfa*>& children,
                                          std::size_t symbolCount, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    if (children.empty()) {
        return epsilonDfa(symbolCount);
    }
    MaterialisedNfa nfa;
    nfa.symbolCount = symbolCount;
    std::vector<std::size_t> offsets;
    offsets.reserve(children.size());
    for (const PartialDfa* child : children) {
        offsets.push_back(nfa.append(*child));
        //  The concatenation accepts only through the last child, so every earlier child's
        //  accepting flags are cleared and its accepting states get an epsilon edge into the next
        //  child.
        for (std::size_t state = 0; state < child->accepting.size(); ++state) {
            nfa.accepting[offsets.back() + state] = 0U;
        }
    }
    for (std::size_t index = 0; index + 1U < children.size(); ++index) {
        const PartialDfa& child = *children[index];
        const std::size_t nextStart = offsets[index + 1U] + children[index + 1U]->start;
        for (std::size_t state = 0; state < child.accepting.size(); ++state) {
            if (child.accepting[state] != 0U) {
                nfa.epsilon[offsets[index] + state].push_back(nextStart);
            }
        }
    }
    const PartialDfa& last = *children.back();
    for (std::size_t state = 0; state < last.accepting.size(); ++state) {
        nfa.accepting[offsets.back() + state] = last.accepting[state];
    }
    nfa.start = offsets.front() + children.front()->start;
    const auto determinised = determinise(nfa, budget);
    if (!determinised.has_value()) {
        return std::nullopt;
    }
    return minimise(*determinised, budget, false);
}

//  The complement of the union of the children within the whole symbol alphabet. Complementing
//  needs a total function, so the automaton is completed with a dead state first and the accepting
//  flags are flipped afterwards, which is exactly what turns "the dead state rejects" into "every
//  word from here is accepted by the complement".
[[nodiscard]] inline auto complementOf(const std::vector<const PartialDfa*>& children,
                                       std::size_t symbolCount, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    const auto united = unionOf(children, symbolCount, budget);
    if (!united.has_value()) {
        return std::nullopt;
    }
    PartialDfa total = *united;
    const std::size_t states = total.accepting.size();
    std::size_t sink = kNoTransition;
    for (std::size_t state = 0; state < states && sink == kNoTransition; ++state) {
        for (std::size_t symbol = 0; symbol < symbolCount; ++symbol) {
            if (total.transitions[state * symbolCount + symbol] == kNoTransition) {
                sink = states;
                break;
            }
        }
    }
    if (sink != kNoTransition) {
        total.accepting.resize(checkedDfaStateSum(states, 1U), 0U);
        total.transitions.resize(
            checkedDfaTableEntries(checkedDfaStateSum(states, 1U), symbolCount), kNoTransition);
        for (std::size_t state = 0; state < states; ++state) {
            for (std::size_t symbol = 0; symbol < symbolCount; ++symbol) {
                if (total.transitions[state * symbolCount + symbol] == kNoTransition) {
                    total.transitions[state * symbolCount + symbol] = sink;
                }
            }
        }
        for (std::size_t symbol = 0; symbol < symbolCount; ++symbol) {
            total.transitions[sink * symbolCount + symbol] = sink;
        }
        budget.chargeState();
    }
    for (std::uint8_t& flag : total.accepting) {
        flag = flag != 0U ? 0U : 1U;
    }
    return minimise(total, budget, false);
}

//  The language of `source` is exactly the empty word: its start state accepts and no accepting
//  state is reachable from it through at least one symbol. A construction that materialised the
//  copy index cannot decide the count of a repeat of such a child (its copy chain never
//  terminates), but the language itself is decided here, in one pass over the states.
[[nodiscard]] inline auto isExactEmptyWordLanguage(const PartialDfa& source) -> bool {
    if (source.accepting.empty() || source.start >= source.accepting.size() ||
        source.accepting[source.start] == 0U) {
        return false;
    }
    std::vector<std::uint8_t> reached(source.accepting.size(), 0U);
    std::vector<std::size_t> pending;
    for (std::size_t symbol = 0; symbol < source.symbolCount; ++symbol) {
        const std::size_t target = source.step(source.start, symbol);
        if (target != kNoTransition && reached[target] == 0U) {
            reached[target] = 1U;
            pending.push_back(target);
        }
    }
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        if (source.accepting[state] != 0U) {
            return false;
        }
        for (std::size_t symbol = 0; symbol < source.symbolCount; ++symbol) {
            const std::size_t target = source.step(state, symbol);
            if (target != kNoTransition && reached[target] == 0U) {
                reached[target] = 1U;
                pending.push_back(target);
            }
        }
    }
    return true;
}

//  The union over k in [minimum, maximum] of k copies of `child`.
//
//  The copy index is part of the state, so a nullable child is handled exactly like any other
//  child: the finite bound never depends on a copy consuming an element, only on the copy count.
[[nodiscard]] inline auto repeatOf(const PartialDfa& child, std::uint64_t minimum,
                                   std::uint64_t maximum, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    if (minimum > maximum) {
        return PartialDfa{.symbolCount = child.symbolCount,
                          .start = 0U,
                          .accepting = {0U},
                          .transitions =
                              std::vector<std::size_t>(child.symbolCount, kNoTransition)};
    }
    if (isExactEmptyWordLanguage(child)) {
        //  Every admissible copy count contributes the same one-word language, so the union is that
        //  word whatever the declared bounds are, and the minimised automaton is the single start
        //  state. The declared bounds remain content and identity; they do not change the language,
        //  and they must not turn a count this construction can state exactly into a gap either.
        return epsilonDfa(child.symbolCount);
    }
    const RepeatNfa view{child, minimum, maximum, budget};
    const auto determinised = determinise(view, budget);
    if (!determinised.has_value()) {
        return std::nullopt;
    }
    return minimise(*determinised, budget, false);
}

} // namespace cuexis::judgement::detail
