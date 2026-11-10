#include "pattern.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace spike::pattern {

NodePtr atom(int letter) {
    auto n = std::make_shared<Node>();
    n->op = Op::Atom;
    n->atom = letter;
    return n;
}

NodePtr seq(NodePtr a, NodePtr b) {
    auto n = std::make_shared<Node>();
    n->op = Op::Seq;
    n->a = std::move(a);
    n->b = std::move(b);
    return n;
}

NodePtr alt(NodePtr a, NodePtr b) {
    auto n = std::make_shared<Node>();
    n->op = Op::Alt;
    n->a = std::move(a);
    n->b = std::move(b);
    return n;
}

NodePtr repeat(NodePtr p, int lo, int hi) {
    auto n = std::make_shared<Node>();
    n->op = Op::Repeat;
    n->a = std::move(p);
    n->lo = lo;
    n->hi = hi;
    return n;
}

NodePtr negate(NodePtr p) {
    auto n = std::make_shared<Node>();
    n->op = Op::Not;
    n->a = std::move(p);
    return n;
}

NodePtr anyOf(int alphabetSize) {
    NodePtr result = atom(0);
    for (int letter = 1; letter < alphabetSize; ++letter) {
        result = alt(result, atom(letter));
    }
    return result;
}

std::string describe(const NodePtr& node) {
    switch (node->op) {
    case Op::Atom:
        return "a" + std::to_string(node->atom);
    case Op::Seq:
        return "(" + describe(node->a) + " . " + describe(node->b) + ")";
    case Op::Alt:
        return "(" + describe(node->a) + " | " + describe(node->b) + ")";
    case Op::Repeat:
        return describe(node->a) + "{" + std::to_string(node->lo) + "," +
               (node->hi == kInf ? std::string("inf") : std::to_string(node->hi)) + "}";
    case Op::Not:
        return "!" + describe(node->a);
    }
    return "?";
}

namespace {

struct BudgetExceeded {
    std::string where;
    int states = 0;
};

struct Nfa {
    struct State {
        std::vector<std::pair<int, int>> edges; // letter, target
        std::vector<int> eps;
    };
    std::vector<State> states;
    int add() {
        states.emplace_back();
        return static_cast<int>(states.size()) - 1;
    }
};

struct Frag {
    int start = 0;
    int end = 0;
};

class Compiler {
  public:
    Compiler(int alphabet, int budget) : alphabet_(alphabet), budget_(budget) {}

    Dfa compileNode(const NodePtr& node, int* nfaStates, int* dfaStates) {
        Nfa nfa;
        const Frag frag = build(nfa, node);
        Dfa raw = determinize(nfa, frag, describe(node));
        if (nfaStates != nullptr) {
            *nfaStates = static_cast<int>(nfa.states.size());
        }
        if (dfaStates != nullptr) {
            *dfaStates = raw.size();
        }
        return minimize(raw);
    }

    int largestComplementOperand = 0;

  private:
    Frag build(Nfa& nfa, const NodePtr& node) {
        switch (node->op) {
        case Op::Atom: {
            const int s = nfa.add();
            const int e = nfa.add();
            nfa.states[s].edges.emplace_back(node->atom, e);
            return {s, e};
        }
        case Op::Seq: {
            const Frag first = build(nfa, node->a);
            const Frag second = build(nfa, node->b);
            nfa.states[first.end].eps.push_back(second.start);
            return {first.start, second.end};
        }
        case Op::Alt: {
            const int s = nfa.add();
            const Frag left = build(nfa, node->a);
            const Frag right = build(nfa, node->b);
            const int e = nfa.add();
            nfa.states[s].eps.push_back(left.start);
            nfa.states[s].eps.push_back(right.start);
            nfa.states[left.end].eps.push_back(e);
            nfa.states[right.end].eps.push_back(e);
            return {s, e};
        }
        case Op::Repeat: {
            const int s = nfa.add();
            int cur = s;
            for (int i = 0; i < node->lo; ++i) {
                const Frag copy = build(nfa, node->a);
                nfa.states[cur].eps.push_back(copy.start);
                cur = copy.end;
            }
            if (node->hi == kInf) {
                const Frag copy = build(nfa, node->a);
                const int e = nfa.add();
                nfa.states[cur].eps.push_back(copy.start);
                nfa.states[cur].eps.push_back(e);
                nfa.states[copy.end].eps.push_back(copy.start);
                nfa.states[copy.end].eps.push_back(e);
                return {s, e};
            }
            const int e = nfa.add();
            for (int i = node->lo; i < node->hi; ++i) {
                nfa.states[cur].eps.push_back(e);
                const Frag copy = build(nfa, node->a);
                nfa.states[cur].eps.push_back(copy.start);
                cur = copy.end;
            }
            nfa.states[cur].eps.push_back(e);
            return {s, e};
        }
        case Op::Not: {
            // Complement requires a complete deterministic operand.
            const Dfa operand = compileNode(node->a, nullptr, nullptr);
            largestComplementOperand = std::max(largestComplementOperand, operand.size());
            std::vector<int> map(static_cast<std::size_t>(operand.size()));
            for (int i = 0; i < operand.size(); ++i) {
                map[static_cast<std::size_t>(i)] = nfa.add();
            }
            const int e = nfa.add();
            for (int i = 0; i < operand.size(); ++i) {
                const int from = map[static_cast<std::size_t>(i)];
                for (int letter = 0; letter < alphabet_; ++letter) {
                    const int to = map[static_cast<std::size_t>(operand.next[i][letter])];
                    nfa.states[from].edges.emplace_back(letter, to);
                }
                if (!operand.accept[static_cast<std::size_t>(i)]) {
                    nfa.states[from].eps.push_back(e);
                }
            }
            return {map[static_cast<std::size_t>(operand.start)], e};
        }
        }
        return {};
    }

    std::vector<int> closure(const Nfa& nfa, std::vector<int> seeds) const {
        std::vector<char> seen(nfa.states.size(), 0);
        std::vector<int> out;
        while (!seeds.empty()) {
            const int s = seeds.back();
            seeds.pop_back();
            if (seen[static_cast<std::size_t>(s)] != 0) {
                continue;
            }
            seen[static_cast<std::size_t>(s)] = 1;
            out.push_back(s);
            for (const int t : nfa.states[static_cast<std::size_t>(s)].eps) {
                seeds.push_back(t);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    Dfa determinize(const Nfa& nfa, Frag frag, const std::string& where) {
        Dfa dfa;
        dfa.alphabet = alphabet_;
        std::map<std::vector<int>, int> ids;
        std::vector<std::vector<int>> sets;
        auto intern = [&](std::vector<int> set) {
            const auto found = ids.find(set);
            if (found != ids.end()) {
                return found->second;
            }
            if (static_cast<int>(sets.size()) >= budget_) {
                throw BudgetExceeded{where, static_cast<int>(sets.size()) + 1};
            }
            const int id = static_cast<int>(sets.size());
            const bool accepting = std::binary_search(set.begin(), set.end(), frag.end);
            ids.emplace(set, id);
            sets.push_back(std::move(set));
            dfa.next.emplace_back(static_cast<std::size_t>(alphabet_), -1);
            dfa.accept.push_back(accepting);
            return id;
        };
        dfa.start = intern(closure(nfa, {frag.start}));
        for (std::size_t i = 0; i < sets.size(); ++i) {
            const std::vector<int> current = sets[i];
            for (int letter = 0; letter < alphabet_; ++letter) {
                std::vector<int> targets;
                for (const int s : current) {
                    for (const auto& [edgeLetter, to] :
                         nfa.states[static_cast<std::size_t>(s)].edges) {
                        if (edgeLetter == letter) {
                            targets.push_back(to);
                        }
                    }
                }
                const int to = intern(closure(nfa, std::move(targets)));
                dfa.next[i][static_cast<std::size_t>(letter)] = to;
            }
        }
        return dfa;
    }

    static Dfa minimize(const Dfa& dfa) {
        const int n = dfa.size();
        std::vector<int> cls(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            cls[static_cast<std::size_t>(i)] = dfa.accept[static_cast<std::size_t>(i)] ? 1 : 0;
        }
        int classCount = -1;
        while (true) {
            std::map<std::vector<int>, int> signatures;
            std::vector<int> refined(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                std::vector<int> sig;
                sig.reserve(static_cast<std::size_t>(dfa.alphabet) + 1);
                sig.push_back(cls[static_cast<std::size_t>(i)]);
                for (int letter = 0; letter < dfa.alphabet; ++letter) {
                    sig.push_back(cls[static_cast<std::size_t>(dfa.next[i][letter])]);
                }
                const auto inserted =
                    signatures.emplace(std::move(sig), static_cast<int>(signatures.size()));
                refined[static_cast<std::size_t>(i)] = inserted.first->second;
            }
            const int newCount = static_cast<int>(signatures.size());
            cls = std::move(refined);
            if (newCount == classCount) {
                break;
            }
            classCount = newCount;
        }
        Dfa out;
        out.alphabet = dfa.alphabet;
        out.next.assign(static_cast<std::size_t>(classCount),
                        std::vector<int>(static_cast<std::size_t>(dfa.alphabet), -1));
        out.accept.assign(static_cast<std::size_t>(classCount), false);
        out.start = cls[static_cast<std::size_t>(dfa.start)];
        for (int i = 0; i < n; ++i) {
            const int c = cls[static_cast<std::size_t>(i)];
            out.accept[static_cast<std::size_t>(c)] = dfa.accept[static_cast<std::size_t>(i)];
            for (int letter = 0; letter < dfa.alphabet; ++letter) {
                out.next[static_cast<std::size_t>(c)][static_cast<std::size_t>(letter)] =
                    cls[static_cast<std::size_t>(dfa.next[i][letter])];
            }
        }
        return out;
    }

    int alphabet_;
    int budget_;
};

} // namespace

CompileResult compile(const NodePtr& root, int alphabetSize, int stateBudget) {
    CompileResult result;
    Compiler compiler(alphabetSize, stateBudget);
    try {
        result.dfa = compiler.compileNode(root, &result.nfaStates, &result.dfaStates);
        result.minStates = result.dfa.size();
    } catch (const BudgetExceeded& exceeded) {
        result.withinBudget = false;
        result.failedAt = exceeded.where;
        result.failedStates = exceeded.states;
    }
    result.largestComplementOperand = compiler.largestComplementOperand;
    return result;
}

bool accepts(const Dfa& dfa, const std::vector<int>& word) {
    int state = dfa.start;
    for (const int letter : word) {
        state = dfa.next[static_cast<std::size_t>(state)][static_cast<std::size_t>(letter)];
    }
    return dfa.accept[static_cast<std::size_t>(state)];
}

} // namespace spike::pattern
