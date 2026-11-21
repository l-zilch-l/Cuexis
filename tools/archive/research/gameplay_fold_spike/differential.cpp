#include "differential.hpp"

#include "pattern.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace spike::diff {

namespace {

using namespace pattern;

// The reference predicates are written directly from each entry's stated rule, independently of
// how the Pattern is built. Disagreement means one of the two is wrong.
bool referenceHolds(const Spec& spec, const std::vector<int>& trace) {
    switch (spec.entry) {
    case Entry::Tap:
        // A press occurs within the first windowSteps samples.
        for (int i = 0; i < std::min<int>(spec.windowSteps, static_cast<int>(trace.size())); ++i) {
            if (trace[static_cast<std::size_t>(i)] == Press) {
                return true;
            }
        }
        return false;
    case Entry::HoldBody: {
        // Starts held, and never sees a run of not-held longer than graceSteps.
        if (trace.empty() || trace[0] != HeldSymbol) {
            return false;
        }
        int run = 0;
        for (const int s : trace) {
            run = (s == NotHeld) ? run + 1 : 0;
            if (run > spec.graceSteps) {
                return false;
            }
        }
        return true;
    }
    case Entry::Bomb:
        // No press anywhere in the first windowSteps samples.
        for (int i = 0; i < std::min<int>(spec.windowSteps, static_cast<int>(trace.size())); ++i) {
            if (trace[static_cast<std::size_t>(i)] == Press) {
                return false;
            }
        }
        return true;
    case Entry::Roll: {
        // repeatCount presses anywhere from the first sample onward, ignoring what is between.
        int seen = 0;
        for (const int s : trace) {
            if (s == Press) {
                ++seen;
            }
        }
        return seen >= spec.repeatCount;
    }
    }
    return false;
}

NodePtr build(const Spec& spec) {
    switch (spec.entry) {
    case Entry::Tap:
        return seq(repeat(anyOf(kAlphabet), 0, spec.windowSteps - 1),
                   seq(atom(Press), repeat(anyOf(kAlphabet), 0, kInf)));
    case Entry::HoldBody: {
        // !(any* . notHeld{grace+1} . any*)
        const NodePtr breach =
            seq(repeat(anyOf(kAlphabet), 0, kInf),
                seq(repeat(atom(NotHeld), spec.graceSteps + 1, spec.graceSteps + 1),
                    repeat(anyOf(kAlphabet), 0, kInf)));
        return seq(atom(HeldSymbol), negate(breach));
    }
    case Entry::Bomb: {
        const NodePtr pressed = seq(repeat(anyOf(kAlphabet), 0, spec.windowSteps - 1),
                                    seq(atom(Press), repeat(anyOf(kAlphabet), 0, kInf)));
        return negate(pressed);
    }
    case Entry::Roll: {
        const NodePtr any = repeat(anyOf(kAlphabet), 0, kInf);
        const NodePtr pressEvent = seq(any, atom(Press));
        NodePtr body = pressEvent;
        for (int i = 1; i < spec.repeatCount; ++i) {
            body = seq(body, pressEvent);
        }
        return seq(body, any);
    }
    }
    return atom(Press);
}

} // namespace

Report check(const Spec& spec, int maxSteps) {
    Report report;
    const CompileResult compiled = compile(build(spec), kAlphabet, 1 << 16);
    if (!compiled.withinBudget) {
        Mismatch m;
        m.trace = {};
        report.mismatches.push_back(m);
        return report;
    }
    const Dfa& dfa = compiled.dfa;

    // Shorter traces are prefixes of longer ones: an entry that matches early still matches.
    // Full enumeration is only feasible for short traces, hence maxSteps is small.
    std::vector<int> trace;
    for (int n = 0; n <= maxSteps; ++n) {
        int count = 1;
        for (int i = 0; i < n; ++i) {
            count *= kAlphabet;
        }
        trace.assign(static_cast<std::size_t>(n), 0);
        for (int code = 0; code < count; ++code) {
            int value = code;
            for (int i = n - 1; i >= 0; --i) {
                trace[static_cast<std::size_t>(i)] = value % kAlphabet;
                value /= kAlphabet;
            }
            ++report.cases;
            const bool got = accepts(dfa, trace);
            const bool want = referenceHolds(spec, trace);
            if (got != want && report.mismatches.size() < 8) {
                report.mismatches.push_back(Mismatch{trace, got, want});
            }
        }
    }
    return report;
}

Report randomCheck(const Spec& spec, int trials, int minLen, int maxLen, std::uint64_t seed) {
    Report report;
    const CompileResult compiled = compile(build(spec), kAlphabet, 1 << 16);
    if (!compiled.withinBudget) {
        report.mismatches.push_back(Mismatch{});
        return report;
    }
    const Dfa& dfa = compiled.dfa;

    // Lengths are drawn uniformly from [minLen, maxLen] rather than always using maxLen: a
    // mismatch is usually reachable by a short suffix, and short traces find it sooner.
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> lenPick(minLen, maxLen);
    std::uniform_int_distribution<int> symPick(0, kAlphabet - 1);
    std::vector<int> trace;
    for (int i = 0; i < trials; ++i) {
        const int n = lenPick(rng);
        trace.assign(static_cast<std::size_t>(n), 0);
        for (int k = 0; k < n; ++k) {
            trace[static_cast<std::size_t>(k)] = symPick(rng);
        }
        ++report.cases;
        const bool got = accepts(dfa, trace);
        const bool want = referenceHolds(spec, trace);
        if (got != want && report.mismatches.size() < 8) {
            report.mismatches.push_back(Mismatch{trace, got, want});
        }
    }
    return report;
}

} // namespace spike::diff
