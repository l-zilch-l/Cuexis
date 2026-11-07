#pragma once

// Pattern DFA node description and the minimal automaton builder.
//
// Fragment of the automaton layer: include "pattern_dfa.hpp", never this file directly. The
// parts wire each other in one order and are not individually self-contained.

#include "pattern_dfa_ops.hpp"

namespace cuexis::judgement::detail {

//  The kind of one node of the description this header consumes.
enum class DfaNodeKind : std::uint8_t {
    atom,
    sequence,
    choice,
    repeat,
    epsilon,
    complement,
};

//  One node of a pattern description, in a form the compiled Pattern can hand over without exposing
//  its own representation. Operands are indexes into the same table and are always smaller than the
//  node that holds them, so the table is processed bottom-up.
struct DfaNode final {
    DfaNodeKind kind{DfaNodeKind::epsilon};
    std::size_t atomSymbol{0};
    std::uint64_t minimum{0};
    std::uint64_t maximum{0};
    std::vector<std::size_t> operands;
};

//  The minimised, determinised state count of `nodes[rootIndex]` over `symbolCount` symbols.
[[nodiscard]] inline auto buildMinimalPatternDfa(const std::vector<DfaNode>& nodes,
                                                 std::size_t rootIndex, std::size_t symbolCount,
                                                 bool measurement = true) -> PatternDfaResult {
    //  A node's automaton is needed only by the nodes that reference it, so it is released as soon
    //  as its last parent has been built. Without that, a deep declaration would cost the sum of
    //  every intermediate automaton instead of the live set.
    std::vector<std::size_t> uses(nodes.size(), 0U);
    for (const DfaNode& node : nodes) {
        for (const std::size_t operand : node.operands) {
            if (operand < uses.size()) {
                ++uses[operand];
            }
        }
    }

    std::vector<PartialDfa> built(nodes.size());
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const DfaNode& node = nodes[index];
        ConstructionBudget budget{measurement};
        std::vector<const PartialDfa*> operands;
        operands.reserve(node.operands.size());
        for (const std::size_t operand : node.operands) {
            if (operand >= index) {
                return PatternDfaResult{};
            }
            operands.push_back(&built[operand]);
        }
        switch (node.kind) {
        case DfaNodeKind::atom:
            built[index] = atomDfa(node.atomSymbol, symbolCount);
            break;
        case DfaNodeKind::epsilon:
            built[index] = epsilonDfa(symbolCount);
            break;
        case DfaNodeKind::choice: {
            const auto result = unionOf(operands, symbolCount, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        case DfaNodeKind::sequence: {
            const auto result = concatenationOf(operands, symbolCount, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        case DfaNodeKind::complement: {
            const auto result = complementOf(operands, symbolCount, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        case DfaNodeKind::repeat: {
            if (operands.size() != 1U) {
                return PatternDfaResult{};
            }
            const auto result = repeatOf(*operands.front(), node.minimum, node.maximum, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        }
        if (index == rootIndex) {
            //  The intermediates were only trimmed, so the one refinement that produces the
            //  minimised automaton of Spec 8.2 happens here, once.
            const auto refined = minimise(built[index], budget, true);
            if (!refined.has_value()) {
                return PatternDfaResult{};
            }
            PatternDfaResult result;
            result.available = true;
            result.dfa = *refined;
            return result;
        }
        for (const std::size_t operand : node.operands) {
            if (operand < uses.size() && uses[operand] > 0U) {
                --uses[operand];
                if (uses[operand] == 0U) {
                    built[operand] = PartialDfa{};
                }
            }
        }
    }
    return PatternDfaResult{};
}

} // namespace cuexis::judgement::detail
