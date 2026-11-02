#pragma once

// Shared pattern compile fixture for the S7A-3 case units.

#include "gameplay_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pattern_compile_fixture {

namespace {

namespace judgement = cuexis::judgement;
namespace testing = cuexis::judgement::testing;

using judgement::CompiledPattern;
using judgement::MatchPolicy;
using judgement::MeasuredParameter;
using judgement::PatternArmBound;
using judgement::PatternCompileBudget;
using judgement::PatternDeclaration;
using judgement::PatternNodeDeclaration;
using judgement::PatternPrimitive;
using judgement::RepeatBoundsDeclaration;
using judgement::UnsupportedContentDeclaration;
using judgement::UnsupportedContentKind;

using testing::contextValue;

//  Asserts the three parts of a stable rejection this suite reads.
inline void expectRejection(const cuexis::core::Error& error, std::string_view code,
                            std::string_view category, std::string_view path) {
    CHECK(error.code() == code);
    CHECK(contextValue(error, "category") == category);
    CHECK(contextValue(error, "severity") == "error");
    CHECK(contextValue(error, "faulted") == "false");
    CHECK(contextValue(error, "field.path") == path);
}

//  --- declaration builders -----------------------------------------------------------------------

inline [[nodiscard]] auto atomNode(std::string atomRef) -> PatternNodeDeclaration {
    return PatternNodeDeclaration{PatternPrimitive::atom, {}, std::move(atomRef), {}, {}};
}

inline [[nodiscard]] auto composeNode(PatternPrimitive primitive,
                                      std::vector<PatternNodeDeclaration> operands)
    -> PatternNodeDeclaration {
    return PatternNodeDeclaration{primitive, std::move(operands), {}, {}, {}};
}

inline [[nodiscard]] auto repeatNode(PatternNodeDeclaration operand, std::uint64_t minimum,
                                     std::uint64_t maximum) -> PatternNodeDeclaration {
    return PatternNodeDeclaration{PatternPrimitive::boundedRepeat,
                                  {std::move(operand)},
                                  {},
                                  RepeatBoundsDeclaration{.minimum = minimum, .maximum = maximum},
                                  {}};
}

inline [[nodiscard]] auto declarationOf(PatternNodeDeclaration root) -> PatternDeclaration {
    return PatternDeclaration{.patternId = {},
                              .matchPolicy = MatchPolicy::leftmostFirst,
                              .root = std::move(root),
                              .required = judgement::RequiredRefs{}};
}

inline [[nodiscard]] auto trace(std::initializer_list<std::string_view> refs)
    -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(refs.size());
    for (const std::string_view ref : refs) {
        result.emplace_back(ref);
    }
    return result;
}

//  A budget in which `depth` is the only measured dimension, so a rejection can be attributed to
//  it.
inline [[nodiscard]] auto depthBudget(std::uint64_t depth) -> PatternCompileBudget {
    PatternCompileBudget budget;
    budget.maxDeclarationDepth = MeasuredParameter<std::uint64_t>::measured(depth);
    return budget;
}

//  A `sequence` chain of `depth` nested nodes over one atom, built iteratively. The declaration
//  type owns its operands recursively, so the chain is kept well inside the host stack of the test
//  process; the property under test is that the *compiler* imposes no depth bound of its own.
inline [[nodiscard]] auto nestedChain(std::size_t depth) -> PatternNodeDeclaration {
    PatternNodeDeclaration chain = atomNode("atom.one");
    for (std::size_t level = 1; level < depth; ++level) {
        chain = composeNode(PatternPrimitive::sequence, {std::move(chain), atomNode("atom.one")});
    }
    return chain;
}

} // namespace

//  --- independent measurement of the minimal state count
//  ------------------------------------------
//
//  Counts the distinct NON-EMPTY residual languages of `pattern` over every word of length up to
//  `window`, each residual characterised by the compiled predicate's own decision on every
//  continuation of length up to `window`. This is the Myhill-Nerode definition stated
//  operationally, and it is read through the compiled predicate rather than through the
//  construction under test, so it is independent evidence for the counts this suite asserts.
//
//  It is exact only while `window` is wide enough to tell the live residuals apart: past that the
//  truncation saturates and the number under-reports. The empty residual is the implicit dead state
//  of a partial automaton and is not counted, which is the same counting object `stateCount` uses.
inline [[nodiscard]] auto distinctNonEmptyResiduals(const CompiledPattern& pattern,
                                                    std::size_t window) -> std::size_t {
    const std::vector<std::string> symbols{"atom.one", "atom.two"};
    std::vector<std::vector<std::string>> words{{}};
    std::vector<std::vector<std::string>> frontier{{}};
    for (std::size_t step = 0; step < window; ++step) {
        std::vector<std::vector<std::string>> next;
        for (const std::vector<std::string>& word : frontier) {
            for (const std::string& symbol : symbols) {
                std::vector<std::string> extended = word;
                extended.push_back(symbol);
                words.push_back(extended);
                next.push_back(std::move(extended));
            }
        }
        frontier = std::move(next);
    }
    std::vector<std::string> signatures;
    for (const std::vector<std::string>& word : words) {
        std::string signature;
        for (const std::vector<std::string>& continuation : words) {
            std::vector<std::string> combined = word;
            combined.insert(combined.end(), continuation.begin(), continuation.end());
            signature.push_back(pattern.matches(combined) ? '1' : '0');
        }
        if (signature.find('1') == std::string::npos) {
            continue;
        }
        if (std::find(signatures.begin(), signatures.end(), signature) == signatures.end()) {
            signatures.push_back(std::move(signature));
        }
    }
    return signatures.size();
}

} // namespace pattern_compile_fixture

using namespace pattern_compile_fixture;
