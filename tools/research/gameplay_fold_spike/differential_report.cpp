// Differential test: the compiled Pattern must agree with a directly written reference predicate
// over every trace up to a small length. This is what validates the claim that the runtime's
// hand-written entries and the compiled automata mean the same thing.
#include "differential.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int failures = 0;

void line(const char* name, const spike::diff::Spec spec, int steps) {
    const spike::diff::Report report = spike::diff::check(spec, steps);
    const bool ok = report.ok();
    if (!ok) {
        ++failures;
    }
    std::printf("| %s | %d | %d | %s |\n", name, steps, report.cases, ok ? "agree" : "DISAGREE");
    for (const auto& m : report.mismatches) {
        std::string trace;
        for (const int s : m.trace) {
            trace += std::to_string(s);
        }
        std::printf("    trace `%s`: compiled=%d reference=%d\n", trace.c_str(), m.compiled ? 1 : 0,
                    m.reference ? 1 : 0);
    }
}

} // namespace

void reportDifferential() {
    using namespace spike::diff;
    std::printf(
        "Symbols: 0=press 1=release 2=held 3=not-held 4=idle. The reference predicate is\n");
    std::printf(
        "written from the entry's stated rule; the compiled side is the Pattern the IR draft\n");
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
    if (failures != 0) {
        std::printf("\n%d entry(ies) disagree with their reference predicate\n", failures);
        std::exit(8);
    }
}
