// Research spike: Pattern -> NFA -> DFA -> minimized DFA.
// Not part of the product build. See README.md in this directory.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace spike::pattern {

struct Node;
using NodePtr = std::shared_ptr<const Node>;

enum class Op { Atom, Seq, Alt, Repeat, Not };
inline constexpr int kInf = -1;

struct Node {
    Op op = Op::Atom;
    int atom = 0;
    NodePtr a;
    NodePtr b;
    int lo = 0;
    int hi = 0;
};

NodePtr atom(int letter);
NodePtr seq(NodePtr a, NodePtr b);
NodePtr alt(NodePtr a, NodePtr b);
NodePtr repeat(NodePtr p, int lo, int hi);
NodePtr negate(NodePtr p);
NodePtr anyOf(int alphabetSize);

std::string describe(const NodePtr& node);

struct Dfa {
    int alphabet = 0;
    int start = 0;
    std::vector<std::vector<int>> next; // complete: [state][letter] -> state
    std::vector<bool> accept;
    int size() const {
        return static_cast<int>(next.size());
    }
};

struct CompileResult {
    bool withinBudget = true;
    int nfaStates = 0;
    int dfaStates = 0;
    int minStates = 0;
    int largestComplementOperand = 0; // minimized DFA size of the largest negated subexpression
    std::string failedAt;             // subexpression that exceeded the budget
    int failedStates = 0;
    Dfa dfa;
};

CompileResult compile(const NodePtr& root, int alphabetSize, int stateBudget);
bool accepts(const Dfa& dfa, const std::vector<int>& word);

} // namespace spike::pattern
