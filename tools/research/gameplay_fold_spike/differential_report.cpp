// Differential test: the compiled Pattern must agree with a directly written reference predicate
// over every trace up to a small length. This is what validates the claim that the runtime's
// hand-written entries and the compiled automata mean the same thing.
#include "differential.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int failures = 0;

void dumpMismatches(const spike::diff::Report& report) {
    for (const auto& m : report.mismatches) {
        std::string trace;
        for (const int s : m.trace) {
            trace += std::to_string(s);
        }
        std::printf("    trace `%s`: compiled=%d reference=%d\n", trace.c_str(), m.compiled ? 1 : 0,
                    m.reference ? 1 : 0);
    }
}

void line(const char* name, const spike::diff::Spec spec, int steps) {
    const spike::diff::Report report = spike::diff::check(spec, steps);
    const bool ok = report.ok();
    if (!ok) {
        ++failures;
    }
    std::printf("| %s | %d | %d | %s |\n", name, steps, report.cases, ok ? "agree" : "DISAGREE");
    dumpMismatches(report);
}

void randomLine(const char* name, const spike::diff::Spec spec, int trials, int maxLen,
                std::uint64_t seed) {
    const spike::diff::Report report = spike::diff::randomCheck(spec, trials, 1, maxLen, seed);
    const bool ok = report.ok();
    if (!ok) {
        ++failures;
    }
    std::printf("| %s | %d | %d | %s |\n", name, maxLen, report.cases, ok ? "agree" : "DISAGREE");
    dumpMismatches(report);
}

} // namespace

void reportDifferential() {
    using namespace spike::diff;
    std::printf("Symbols: 0=press 1=release 2=held 3=not-held 4=idle. The reference predicate is\n");
    std::printf("written from the entry's stated rule; the compiled side is the Pattern the IR draft\n");
    std::printf("gives. Disagreement at any trace length means one of the two is wrong.\n\n");
    std::printf("| entry | trace length | traces | verdict |\n| --- | --- | --- | --- |\n");
    line("Tap, window 3", Spec{Entry::Tap, 3, 0, 0}, 5);
    line("Hold body, grace 0", Spec{Entry::HoldBody, 0, 0, 0}, 5);
    line("Hold body, grace 1", Spec{Entry::HoldBody, 0, 1, 0}, 5);
    line("Hold body, grace 2", Spec{Entry::HoldBody, 0, 2, 0}, 5);
    line("Bomb, window 3", Spec{Entry::Bomb, 3, 0, 0}, 5);
    line("Bomb, window 1", Spec{Entry::Bomb, 1, 0, 0}, 5);
    line("Roll, 2 presses", Spec{Entry::Roll, 0, 0, 2}, 5);
    line("Roll, 3 presses", Spec{Entry::Roll, 0, 0, 3}, 5);

    std::printf("\nRandomised long traces, 200,000 per entry. This is a probabilistic argument, not a\n");
    std::printf("proof: an exhaustive check is exponential in trace length.\n\n");
    std::printf("| entry | max length | traces | verdict |\n| --- | --- | --- | --- |\n");
    randomLine("Tap, window 40", Spec{Entry::Tap, 40, 0, 0}, 200'000, 400, 11);
    randomLine("Hold body, grace 3", Spec{Entry::HoldBody, 0, 3, 0}, 200'000, 400, 12);
    randomLine("Hold body, grace 0", Spec{Entry::HoldBody, 0, 0, 0}, 200'000, 400, 13);
    randomLine("Bomb, window 40", Spec{Entry::Bomb, 40, 0, 0}, 200'000, 400, 14);
    randomLine("Roll, 6 presses", Spec{Entry::Roll, 0, 0, 6}, 200'000, 400, 15);
    if (failures != 0) {
        std::printf("\n%d entry(ies) disagree with their reference predicate\n", failures);
        std::exit(8);
    }
}
