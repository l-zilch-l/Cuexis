#pragma once

// Pattern DFA reduction: trimming, equivalent-state merging and minimisation.
//
// Fragment of the automaton layer: include "pattern_dfa.hpp", never this file directly. The
// parts wire each other in one order and are not individually self-contained.

#include "pattern_dfa_build.hpp"

namespace cuexis::judgement::detail {

//  Keeps only the states that are reachable from the start and can still reach an accepting state.
//  Every state of the result is live, so an undefined transition and a transition into the implicit
//  dead state are the same thing afterwards, which is what the merging below relies on.
//
//  The empty language keeps one state, because a partial automaton still has its start state.
[[nodiscard]] inline auto trim(const PartialDfa& source, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    const std::size_t symbols = source.symbolCount;
    const std::size_t states = source.accepting.size();

    std::vector<std::uint8_t> reachable(states, 0U);
    std::vector<std::size_t> order;
    std::vector<std::size_t> pending{source.start};
    reachable[source.start] = 1U;
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        order.push_back(state);
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition && reachable[target] == 0U) {
                reachable[target] = 1U;
                pending.push_back(target);
            }
        }
        budget.chargeWork();
        if (!budget.ok()) {
            return std::nullopt;
        }
    }

    std::vector<std::vector<std::size_t>> predecessors(states);
    for (std::size_t state = 0; state < states; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition) {
                predecessors[target].push_back(state);
            }
        }
    }
    std::vector<std::uint8_t> live(states, 0U);
    pending.clear();
    for (std::size_t state = 0; state < states; ++state) {
        if (source.accepting[state] != 0U) {
            live[state] = 1U;
            pending.push_back(state);
        }
    }
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        for (const std::size_t predecessor : predecessors[state]) {
            if (live[predecessor] == 0U) {
                live[predecessor] = 1U;
                pending.push_back(predecessor);
            }
        }
    }

    std::vector<std::size_t> remap(states, kNoTransition);
    std::size_t kept = 0U;
    for (const std::size_t state : order) {
        if (live[state] != 0U) {
            remap[state] = kept;
            ++kept;
        }
    }
    PartialDfa result;
    result.symbolCount = symbols;
    if (kept == 0U) {
        result.start = 0U;
        result.accepting.assign(1U, 0U);
        result.transitions.assign(symbols, kNoTransition);
        return result;
    }
    result.accepting.assign(kept, 0U);
    result.transitions.assign(checkedDfaTableEntries(kept, symbols), kNoTransition);
    for (const std::size_t state : order) {
        const std::size_t index = remap[state];
        if (index == kNoTransition) {
            continue;
        }
        result.accepting[index] = source.accepting[state];
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition && remap[target] != kNoTransition) {
                result.transitions[index * symbols + symbol] = remap[target];
            }
        }
    }
    result.start = remap[source.start];
    return result;
}

//  Merges the equivalent states of a trimmed automaton, that is, it computes the minimal partial
//  automaton of the same language.
//
//  An acyclic automaton is merged bottom-up: the key of a state is its accepting flag plus the
//  classes of its targets, and every target is classed before its predecessors are, so one reverse
//  topological pass is exact and linear. Only an automaton that contains a cycle needs the
//  completion with a dead state and the partition refinement below.
[[nodiscard]] inline auto mergeEquivalent(const PartialDfa& source, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    const std::size_t symbols = source.symbolCount;
    const std::size_t states = source.accepting.size();

    //  Kahn's algorithm over the defined transitions: a topological order exists exactly when the
    //  automaton is acyclic.
    std::vector<std::size_t> indegree(states, 0U);
    for (std::size_t state = 0; state < states; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition) {
                ++indegree[target];
            }
        }
    }
    std::vector<std::size_t> pending;
    for (std::size_t state = 0; state < states; ++state) {
        if (indegree[state] == 0U) {
            pending.push_back(state);
        }
    }
    std::vector<std::size_t> order;
    order.reserve(states);
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        order.push_back(state);
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition && --indegree[target] == 0U) {
                pending.push_back(target);
            }
        }
        budget.chargeWork();
        if (!budget.ok()) {
            return std::nullopt;
        }
    }
    if (order.size() == states) {
        std::map<std::vector<std::size_t>, std::size_t> byKey;
        std::vector<std::size_t> classOf(states, kNoTransition);
        std::vector<std::size_t> key;
        for (auto iterator = order.rbegin(); iterator != order.rend(); ++iterator) {
            const std::size_t state = *iterator;
            key.clear();
            key.reserve(symbols + 1U);
            key.push_back(source.accepting[state]);
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                const std::size_t target = source.transitions[state * symbols + symbol];
                key.push_back(target == kNoTransition ? kNoTransition : classOf[target]);
            }
            const auto [found, inserted] = byKey.try_emplace(key, byKey.size());
            classOf[state] = found->second;
            budget.chargeWork(symbols + 1U);
            if (!budget.ok()) {
                return std::nullopt;
            }
        }
        //  Classes are renumbered by their smallest member state so that the result does not depend
        //  on the order in which the merging happened to discover them.
        std::vector<std::size_t> newIndex(byKey.size(), kNoTransition);
        std::size_t classCount = 0U;
        std::vector<std::size_t> representative(byKey.size(), kNoTransition);
        for (std::size_t state = 0; state < states; ++state) {
            const std::size_t classId = classOf[state];
            if (newIndex[classId] == kNoTransition) {
                newIndex[classId] = classCount;
                ++classCount;
            }
            if (representative[classId] == kNoTransition) {
                representative[classId] = state;
            }
        }
        PartialDfa result;
        result.symbolCount = symbols;
        result.accepting.assign(classCount, 0U);
        result.transitions.assign(checkedDfaTableEntries(classCount, symbols), kNoTransition);
        for (std::size_t classId = 0; classId < byKey.size(); ++classId) {
            if (newIndex[classId] == kNoTransition) {
                continue;
            }
            const std::size_t state = representative[classId];
            result.accepting[newIndex[classId]] = source.accepting[state];
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                const std::size_t target = source.transitions[state * symbols + symbol];
                if (target != kNoTransition) {
                    result.transitions[newIndex[classId] * symbols + symbol] =
                        newIndex[classOf[target]];
                }
            }
        }
        result.start = newIndex[classOf[source.start]];
        return result;
    }

    //  A cyclic automaton. Completing it with a dead state makes the transition function total,
    //  which is what the refinement needs.
    std::vector<std::uint8_t> accepting = source.accepting;
    std::vector<std::size_t> transitions = source.transitions;
    std::size_t total = states;
    bool needsSink = false;
    for (std::size_t state = 0; state < states && !needsSink; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            if (transitions[state * symbols + symbol] == kNoTransition) {
                needsSink = true;
                break;
            }
        }
    }
    if (needsSink) {
        const std::size_t sink = states;
        total = checkedDfaStateSum(states, 1U);
        accepting.resize(total, 0U);
        transitions.resize(checkedDfaTableEntries(total, symbols), kNoTransition);
        for (std::size_t state = 0; state < states; ++state) {
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                if (transitions[state * symbols + symbol] == kNoTransition) {
                    transitions[state * symbols + symbol] = sink;
                }
            }
        }
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            transitions[sink * symbols + symbol] = sink;
        }
        budget.chargeState();
    }

    //  Moore refinement: split the accepting / non-accepting partition by the classes of the
    //  transition targets until the partition stops growing. The signatures are compared through a
    //  sort of a flat key buffer rather than through per-state allocations.
    const std::size_t width = symbols + 1U;
    std::vector<std::size_t> keys(total * width, 0U);
    std::vector<std::size_t> orderByKey(total, 0U);
    std::vector<std::size_t> next(total, 0U);
    std::vector<std::size_t> classes(total, 0U);
    for (std::size_t state = 0; state < total; ++state) {
        classes[state] = accepting[state] != 0U ? 1U : 0U;
    }
    std::size_t classCount =
        (std::find(classes.begin(), classes.end(), 0U) != classes.end()) ? 2U : 1U;
    while (true) {
        for (std::size_t state = 0; state < total; ++state) {
            keys[state * width] = classes[state];
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                keys[state * width + symbol + 1U] = classes[transitions[state * symbols + symbol]];
            }
            orderByKey[state] = state;
        }
        std::sort(orderByKey.begin(), orderByKey.end(),
                  [&keys, width](std::size_t left, std::size_t right) {
                      return std::lexicographical_compare(
                          keys.begin() + static_cast<std::ptrdiff_t>(left * width),
                          keys.begin() + static_cast<std::ptrdiff_t>((left + 1U) * width),
                          keys.begin() + static_cast<std::ptrdiff_t>(right * width),
                          keys.begin() + static_cast<std::ptrdiff_t>((right + 1U) * width));
                  });
        std::size_t nextCount = 0U;
        for (std::size_t index = 0; index < total; ++index) {
            if (index == 0U) {
                ++nextCount;
            } else {
                const std::size_t previous = orderByKey[index - 1U];
                const std::size_t current = orderByKey[index];
                bool same = true;
                for (std::size_t entry = 0; entry < width; ++entry) {
                    if (keys[previous * width + entry] != keys[current * width + entry]) {
                        same = false;
                        break;
                    }
                }
                if (!same) {
                    ++nextCount;
                }
            }
            next[orderByKey[index]] = nextCount - 1U;
        }
        budget.chargeWork(total);
        if (!budget.ok()) {
            return std::nullopt;
        }
        classes = next;
        if (nextCount == classCount) {
            classCount = nextCount;
            break;
        }
        classCount = nextCount;
    }

    //  A class is live when it contains a state that can reach an accepting state. The dead class
    //  is the class of the added dead state, and it is the only one that is not live.
    std::vector<std::vector<std::size_t>> predecessors(total);
    for (std::size_t state = 0; state < total; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            predecessors[transitions[state * symbols + symbol]].push_back(state);
        }
    }
    std::vector<std::uint8_t> live(total, 0U);
    std::vector<std::size_t> worklist;
    for (std::size_t state = 0; state < total; ++state) {
        if (accepting[state] != 0U) {
            live[state] = 1U;
            worklist.push_back(state);
        }
    }
    while (!worklist.empty()) {
        const std::size_t state = worklist.back();
        worklist.pop_back();
        for (const std::size_t predecessor : predecessors[state]) {
            if (live[predecessor] == 0U) {
                live[predecessor] = 1U;
                worklist.push_back(predecessor);
            }
        }
    }

    std::vector<std::size_t> newIndex(classCount, kNoTransition);
    std::size_t liveClasses = 0U;
    std::vector<std::size_t> representative(classCount, kNoTransition);
    for (std::size_t state = 0; state < total; ++state) {
        const std::size_t classId = classes[state];
        if (live[state] == 0U) {
            continue;
        }
        if (newIndex[classId] == kNoTransition) {
            newIndex[classId] = liveClasses;
            ++liveClasses;
        }
        if (representative[classId] == kNoTransition) {
            representative[classId] = state;
        }
    }
    PartialDfa result;
    result.symbolCount = symbols;
    if (liveClasses == 0U) {
        result.start = 0U;
        result.accepting.assign(1U, 0U);
        result.transitions.assign(symbols, kNoTransition);
        return result;
    }
    result.accepting.assign(liveClasses, 0U);
    result.transitions.assign(checkedDfaTableEntries(liveClasses, symbols), kNoTransition);
    //  Class ids and result state numbers are different numberings, so a result state number is
    //  translated back into its class before the representative state is read.
    std::vector<std::size_t> classOfState(liveClasses, kNoTransition);
    for (std::size_t classId = 0; classId < classCount; ++classId) {
        if (newIndex[classId] != kNoTransition) {
            classOfState[newIndex[classId]] = classId;
        }
    }
    for (std::size_t index = 0; index < liveClasses; ++index) {
        const std::size_t state = representative[classOfState[index]];
        result.accepting[index] = accepting[state];
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = transitions[state * symbols + symbol];
            if (live[target] == 0U) {
                continue;
            }
            result.transitions[index * symbols + symbol] = newIndex[classes[target]];
        }
    }
    result.start = newIndex[classes[source.start]];
    return result;
}

} // namespace cuexis::judgement::detail
