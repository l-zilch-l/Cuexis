// Research spike driver. Measures the candidate budget figures of
// docs/proposals/GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md and checks three determinism claims.
// Not part of the product build. See README.md in this directory.
#include "pattern.hpp"
#include "runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <string>
#include <vector>

using namespace spike;
using rt::Tick;

namespace {

struct Rng {
    std::uint64_t s;
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    double unit() {
        return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
    }
    Tick range(Tick lo, Tick hi) {
        return lo + static_cast<Tick>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
    // Rough bell curve, sum of four uniforms.
    Tick jitter(Tick spread) {
        double v = 0;
        for (int i = 0; i < 4; ++i) {
            v += unit() - 0.5;
        }
        return static_cast<Tick>(v * static_cast<double>(spread));
    }
};

struct Profile {
    const char* name;
    int notes;
    Tick durationUs;
    int lanes;
    double holdShare;
    double bombShare;
    double rollShare;
    Tick holdMinUs;
    Tick holdMaxUs;
};

struct Chart {
    std::vector<rt::Requirement> reqs;
    std::vector<rt::InputEvent> inputs;
};

Chart generate(const Profile& p, std::uint64_t seed) {
    Rng rng{seed};
    Chart chart;
    std::vector<Tick> laneFree(static_cast<std::size_t>(p.lanes), 0);
    const Tick step = p.durationUs / p.notes;
    for (int i = 0; i < p.notes; ++i) {
        rt::Requirement r;
        r.id = static_cast<std::uint32_t>(i);
        const Tick nominal = 2'000'000 + static_cast<Tick>(i) * step;
        // Keep the timeline fixed: anchors never move. Use the lane that has been free longest;
        // if even that lane is still busy (a long note), the note degrades to a tap on it.
        const auto lane = static_cast<std::size_t>(
            std::min_element(laneFree.begin(), laneFree.end()) - laneFree.begin());
        r.lane = static_cast<std::uint8_t>(lane);
        r.anchor = nominal;
        const bool laneBusy = laneFree[lane] + 60'000 > nominal;
        const double roll = laneBusy ? 1.0 : rng.unit();
        if (roll < p.bombShare) {
            r.kind = rt::Kind::Bomb;
            r.end = r.anchor;
        } else if (roll < p.bombShare + p.rollShare) {
            r.kind = rt::Kind::Roll;
            r.end = r.anchor + rng.range(800'000, 2'000'000);
            r.rollTarget = static_cast<std::uint16_t>((r.end - r.anchor) / 125'000);
        } else if (roll < p.bombShare + p.rollShare + p.holdShare) {
            r.kind = rt::Kind::Hold;
            r.end = r.anchor + rng.range(p.holdMinUs, p.holdMaxUs);
        } else {
            r.kind = rt::Kind::Tap;
            r.end = r.anchor;
        }
        laneFree[lane] = std::max(laneFree[lane], r.end);
        chart.reqs.push_back(r);
    }

    // Synthetic player: mostly accurate, occasionally late, drops holds, sometimes hits bombs.
    auto press = [&](Tick t, std::uint8_t lane, Tick upAt) {
        chart.inputs.push_back({t, lane, true});
        chart.inputs.push_back({upAt, lane, false});
    };
    for (const auto& r : chart.reqs) {
        switch (r.kind) {
        case rt::Kind::Tap:
            if (rng.unit() > 0.03) {
                const Tick t = r.anchor + rng.jitter(140'000);
                press(t, r.lane, t + 40'000);
            }
            break;
        case rt::Kind::Hold:
            if (rng.unit() > 0.03) {
                const Tick t = r.anchor + rng.jitter(140'000);
                if (rng.unit() < 0.15) {
                    // Lets go mid-body; sometimes within grace, sometimes not.
                    const Tick gapAt = t + (r.end - t) / 2;
                    const Tick gap = rng.range(20'000, 140'000);
                    press(t, r.lane, gapAt);
                    press(gapAt + gap, r.lane, r.end + 10'000);
                } else {
                    press(t, r.lane, r.end + 10'000);
                }
            }
            break;
        case rt::Kind::Bomb:
            if (rng.unit() < 0.05) {
                const Tick t = r.anchor + rng.jitter(80'000);
                press(t, r.lane, t + 30'000);
            }
            break;
        case rt::Kind::Roll: {
            for (Tick t = r.anchor + 30'000; t < r.end - 30'000; t += rng.range(90'000, 140'000)) {
                press(t, r.lane, t + 30'000);
            }
            break;
        }
        }
    }
    std::stable_sort(chart.inputs.begin(), chart.inputs.end(),
                     [](const rt::InputEvent& a, const rt::InputEvent& b) { return a.t < b.t; });
    for (auto& ev : chart.inputs) {
        ev.t = std::max<Tick>(ev.t, 0);
    }
    return chart;
}

template <class T> T percentile(std::vector<T> v, double q) {
    if (v.empty()) {
        return T{};
    }
    std::sort(v.begin(), v.end());
    const auto idx = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1));
    return v[idx];
}

double ms(std::chrono::steady_clock::duration d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

// ---------------------------------------------------------------------------------------------
// Section A: pattern compilation sizes.

void reportPatterns() {
    using namespace pattern;
    std::printf("## A. Pattern compilation (budget draft 3.3)\n\n");
    std::printf("Alphabet per sampled step: 0=press 1=release 2=held 3=not-held 4=idle.\n");
    std::printf("Window length is in samples; at 4 ms a 240 ms window is 60 samples.\n\n");
    constexpr int kAlpha = 5;
    const NodePtr any = anyOf(kAlpha);
    const NodePtr press = atom(0);
    const NodePtr notHeld = atom(3);
    const NodePtr held = atom(2);
    const NodePtr idle = atom(4);

    std::printf("| entry | window | NFA | DFA | min DFA | largest complement operand |\n");
    std::printf("| --- | --- | --- | --- | --- | --- |\n");
    auto row = [&](const char* name, int window, const NodePtr& p) {
        const CompileResult r = compile(p, kAlpha, 1 << 16);
        std::printf("| %s | %d | %d | %d | %d | %d |\n", name, window, r.nfaStates, r.dfaStates,
                    r.minStates, r.largestComplementOperand);
    };
    for (const int w : {15, 30, 60, 120}) {
        // Tap: press somewhere inside a bounded window, nothing else matters.
        row("Tap", w, seq(repeat(any, 0, w - 1), seq(press, repeat(any, 0, kInf))));
    }
    for (const int grace : {5, 15, 30}) {
        // Hold body with hole(!held, grace): no run of not-held samples longer than grace.
        const NodePtr breach = seq(
            repeat(any, 0, kInf), seq(repeat(notHeld, grace + 1, grace + 1), repeat(any, 0, kInf)));
        std::string name = "Hold body, grace=" + std::to_string(grace);
        row(name.c_str(), 0, seq(press, negate(breach)));
    }
    for (const int w : {15, 30}) {
        // Bomb: no press anywhere in the window.
        const NodePtr pressed = seq(repeat(any, 0, w - 1), seq(press, repeat(any, 0, kInf)));
        row("Bomb", w, negate(pressed));
    }
    row("Roll, 8 presses", 0,
        seq(repeat(seq(repeat(alt(idle, held), 0, kInf), press), 8, 8), repeat(any, 0, kInf)));

    std::printf("\nAdversarial complement: !(any* . press . any{n}) - the classic 2^n case.\n\n");
    std::printf("| n | outcome | DFA states / failing subexpression |\n| --- | --- | --- |\n");
    for (const int n : {6, 10, 14, 18}) {
        const NodePtr p = negate(seq(repeat(any, 0, kInf), seq(press, repeat(any, n, n))));
        const CompileResult r = compile(p, kAlpha, 4096);
        if (r.withinBudget) {
            std::printf("| %d | ok | %d |\n", n, r.minStates);
        } else {
            std::string where = r.failedAt;
            if (where.size() > 60) {
                where = where.substr(0, 57) + "...";
            }
            std::printf("| %d | rejected at %d states | `%s` |\n", n, r.failedStates,
                        where.c_str());
        }
    }

    // Sanity: semantic spot checks of the compiled Hold body.
    const NodePtr breach =
        seq(repeat(any, 0, kInf), seq(repeat(notHeld, 3, 3), repeat(any, 0, kInf)));
    const CompileResult hold = compile(seq(press, negate(breach)), kAlpha, 1 << 16);
    const bool okShortGap = accepts(hold.dfa, {0, 2, 3, 3, 2, 2});
    const bool rejectsLongGap = !accepts(hold.dfa, {0, 2, 3, 3, 3, 2});
    std::printf("\nSpot check, grace=2: short gap accepted=%s, long gap rejected=%s\n\n",
                okShortGap ? "yes" : "NO", rejectsLongGap ? "yes" : "NO");
    if (!okShortGap || !rejectsLongGap) {
        std::exit(2);
    }
}

// ---------------------------------------------------------------------------------------------
// Section B/C/D: runtime measurements and determinism checks.

Tick chartEnd(const Chart& chart);

struct RunResult {
    std::uint64_t digest = 0;
    rt::Summary summary;
    rt::Stats stats;
    double wallMs = 0;
};

RunResult runFull(const rt::Interface& iface, const rt::Loadout& loadout, const Chart& chart,
                  bool reverse, bool canonical = true) {
    rt::Session s(iface, loadout, chart.reqs, chart.inputs);
    s.setReverseEnumeration(reverse);
    s.setCanonicalFactOrder(canonical);
    const auto t0 = std::chrono::steady_clock::now();
    s.runUntil(INT64_MAX - 1);
    RunResult r;
    r.wallMs = ms(std::chrono::steady_clock::now() - t0);
    r.digest = s.digest();
    r.summary = s.summary();
    r.stats = s.stats();
    return r;
}

Tick chartEnd(const Chart& chart) {
    Tick end = 0;
    for (const auto& r : chart.reqs) {
        end = std::max(end, r.deadline);
    }
    for (const auto& e : chart.inputs) {
        end = std::max(end, e.t);
    }
    return end;
}

void reportProfile(const Profile& p, std::uint64_t seed, bool printHeader) {
    rt::Interface iface;
    rt::Loadout loadout;
    Chart chart = generate(p, seed);
    rt::prepare(chart.reqs, iface, rt::loadoutMaxScale(iface, loadout));
    const std::uint64_t sweep = rt::sweepActivityPeak(chart.reqs);

    const RunResult fwd = runFull(iface, loadout, chart, false);
    const RunResult rev = runFull(iface, loadout, chart, true);

    std::vector<std::uint64_t> ops = fwd.stats.opsPerSecond;
    std::vector<std::uint32_t> samples = fwd.stats.samplesPerSecond;
    if (printHeader) {
        std::printf("| profile | reqs | span s | notes/s | long notes | inputs | sweep peak | "
                    "observed max live | max ops/tick | ops/s p50 | ops/s max | samples/s max | "
                    "facts | run ms | L7 |\n");
        std::printf("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- "
                    "| --- | --- |\n");
    }
    const double span = static_cast<double>(chartEnd(chart)) / 1e6;
    std::size_t longNotes = 0;
    for (const auto& r : chart.reqs) {
        longNotes += (r.kind == rt::Kind::Hold || r.kind == rt::Kind::Roll) ? 1 : 0;
    }
    std::printf("| %s | %zu | %.0f | %.0f | %zu | %zu | %llu | %llu | %llu | %llu | %llu | %u | "
                "%llu | %.1f | %s |\n",
                p.name, chart.reqs.size(), span, static_cast<double>(chart.reqs.size()) / span,
                longNotes, chart.inputs.size(), static_cast<unsigned long long>(sweep),
                static_cast<unsigned long long>(fwd.stats.maxActive),
                static_cast<unsigned long long>(fwd.stats.maxOpsInTick),
                static_cast<unsigned long long>(percentile(ops, 0.5)),
                static_cast<unsigned long long>(percentile(ops, 1.0)), percentile(samples, 1.0),
                static_cast<unsigned long long>(fwd.stats.facts), fwd.wallMs,
                fwd.digest == rev.digest ? "equal" : "DIFFER");
    if (fwd.stats.maxActive > sweep) {
        std::printf("\nFAIL: observed live count exceeds the static sweep for %s\n", p.name);
        std::exit(3);
    }
    if (fwd.digest != rev.digest) {
        std::printf("\nFAIL: enumeration order changed the result for %s\n", p.name);
        std::exit(4);
    }
}

void reportSeek(const Profile& p, std::uint64_t seed) {
    rt::Interface iface;
    rt::Loadout loadout;
    Chart chart = generate(p, seed);
    rt::prepare(chart.reqs, iface, rt::loadoutMaxScale(iface, loadout));
    const Tick end = chartEnd(chart);
    const RunResult reference = runFull(iface, loadout, chart, false);

    std::printf("| snapshot interval | snapshots | bytes max | bytes total | seek p95 ms | seek "
                "max ms | lossless |\n");
    std::printf("| --- | --- | --- | --- | --- | --- | --- |\n");
    for (const Tick interval :
         {Tick{1'000'000}, Tick{5'000'000}, Tick{15'000'000}, Tick{60'000'000}}) {
        rt::Session recorder(iface, loadout, chart.reqs, chart.inputs);
        std::vector<std::pair<Tick, std::vector<std::uint8_t>>> snaps;
        snaps.emplace_back(-1, recorder.snapshot());
        for (Tick t = interval; t <= end; t += interval) {
            recorder.runUntil(t - 1);
            snaps.emplace_back(t - 1, recorder.snapshot());
        }
        recorder.runUntil(INT64_MAX - 1);
        std::size_t maxBytes = 0;
        std::size_t totalBytes = 0;
        for (const auto& [t, bytes] : snaps) {
            maxBytes = std::max(maxBytes, bytes.size());
            totalBytes += bytes.size();
        }

        // Seek to 40 targets spread over the song; each seek restores the nearest earlier snapshot.
        Rng rng{seed ^ static_cast<std::uint64_t>(interval)};
        std::vector<double> seekMs;
        bool lossless = recorder.digest() == reference.digest;
        for (int k = 0; k < 40; ++k) {
            const Tick target = rng.range(0, end);
            const auto it = std::upper_bound(snaps.begin(), snaps.end(), target,
                                             [](Tick t, const auto& s) { return t < s.first; });
            const auto& base = *(it - 1);
            rt::Session seeker(iface, loadout, chart.reqs, chart.inputs);
            const auto t0 = std::chrono::steady_clock::now();
            seeker.restore(base.second);
            seeker.runUntil(target);
            seekMs.push_back(ms(std::chrono::steady_clock::now() - t0));

            // Lossless: continuing from the seek must reach the reference end state bit-for-bit,
            // and the state at the target must equal a continuous run to the same target.
            rt::Session straight(iface, loadout, chart.reqs, chart.inputs);
            straight.runUntil(target);
            if (straight.digest() != seeker.digest()) {
                lossless = false;
            }
            seeker.runUntil(INT64_MAX - 1);
            if (seeker.digest() != reference.digest) {
                lossless = false;
            }
        }
        std::printf("| %lld s | %zu | %zu | %zu | %.2f | %.2f | %s |\n",
                    static_cast<long long>(interval / 1'000'000), snaps.size(), maxBytes,
                    totalBytes, percentile(seekMs, 0.95), percentile(seekMs, 1.0),
                    lossless ? "yes" : "NO");
        if (!lossless) {
            std::printf("\nFAIL: seek is not lossless at interval %lld\n",
                        static_cast<long long>(interval));
            std::exit(5);
        }
    }
}

void reportSweepSignature() {
    // Budget draft 7.5: the activity sweep (and arm) must use the loadout's window scale.
    // Targeted case: 100 perfect taps trigger the boost (window x1.1), then one tap is pressed
    // 125 ms early. 125 ms is outside the base good window (120 ms) and inside the boosted one
    // (132 ms). The two charts differ only in the scale prepare() used to compute arm.
    rt::Interface iface;
    rt::Loadout loadout;
    Chart chart;
    for (std::uint32_t i = 0; i < 101; ++i) {
        rt::Requirement r;
        r.id = i;
        r.lane = static_cast<std::uint8_t>(i % 4);
        r.anchor = 1'000'000 + static_cast<Tick>(i) * 200'000;
        r.end = r.anchor;
        chart.reqs.push_back(r);
        const Tick press = i < 100 ? r.anchor : r.anchor - 125'000;
        chart.inputs.push_back({press, r.lane, true});
        chart.inputs.push_back({press + 30'000, r.lane, false});
    }
    Chart withLoadout = chart;
    Chart baseOnly = chart;
    rt::prepare(withLoadout.reqs, iface, rt::loadoutMaxScale(iface, loadout));
    rt::prepare(baseOnly.reqs, iface, 1000);
    const RunResult a = runFull(iface, loadout, withLoadout, false);
    const RunResult b = runFull(iface, loadout, baseOnly, false);
    std::printf("| arm computed from | last tap | hits | misses | strays | score |\n");
    std::printf("| --- | --- | --- | --- | --- | --- |\n");
    auto line = [](const char* label, const RunResult& r) {
        const bool lastHit = r.summary.counts[0][0] == 101;
        std::printf("| %s | %s | %u | %u | %u | %lld |\n", label,
                    lastHit ? "Good" : "Miss, press became a stray", r.summary.counts[0][0],
                    r.summary.counts[0][1],
                    r.summary.counts[1][static_cast<int>(rt::Outcome::Stray)],
                    static_cast<long long>(r.summary.score));
    };
    line("loadout max scale (1100)", a);
    line("interface base scale (1000)", b);
    if (a.summary.counts[0][0] != 101 || b.summary.counts[0][0] != 100) {
        std::printf("\nFAIL: the targeted sweep-signature case did not behave as predicted\n");
        std::exit(6);
    }
}

} // namespace

void reportContacts();
void reportDifferential();

int main() {
    std::printf("# Gameplay fold spike report\n\n");
    reportPatterns();

    const Profile profiles[] = {
        {"dense-3k", 3'000, 150'000'000, 8, 0.2, 0.03, 0.01, 300'000, 2'000'000},
        {"tap-40k", 40'000, 300'000'000, 16, 0.0, 0.0, 0.0, 0, 0},
        {"mixed-40k", 40'000, 300'000'000, 16, 0.25, 0.05, 0.02, 300'000, 2'000'000},
        {"burst-40k", 40'000, 120'000'000, 16, 0.2, 0.05, 0.0, 300'000, 1'500'000},
        {"long-hold-4k", 4'000, 300'000'000, 16, 0.9, 0.0, 0.0, 4'000'000, 10'000'000},
    };
    std::printf("## B. Steady state and activity (budget draft 3.1-3.4, L7)\n\n");
    bool first = true;
    for (const Profile& p : profiles) {
        reportProfile(p, 0xC0FFEEull, first);
        first = false;
    }

    std::printf("\n## B2. Same-tick fact order (finding D11)\n\n");
    std::printf(
        "One press may settle several requirements in the same tick (e.g. two bombs on a lane).\n");
    std::printf(
        "Their facts are emitted in instance-enumeration order unless the fold sorts them.\n\n");
    std::printf("| profile | fold order | forward vs reverse enumeration |\n| --- | --- | --- |\n");
    for (const Profile& p : profiles) {
        rt::Interface iface;
        rt::Loadout loadout;
        Chart chart = generate(p, 0xC0FFEEull);
        rt::prepare(chart.reqs, iface, rt::loadoutMaxScale(iface, loadout));
        for (const bool canonical : {false, true}) {
            const RunResult f = runFull(iface, loadout, chart, false, canonical);
            const RunResult r = runFull(iface, loadout, chart, true, canonical);
            std::printf("| %s | %s | %s |\n", p.name,
                        canonical ? "canonical (req, phase, outcome)" : "emission",
                        f.digest == r.digest ? "equal" : "DIFFER");
        }
    }

    std::printf("\n## C. Snapshot and seek, profile mixed-40k (budget draft 4)\n\n");
    reportSeek(profiles[2], 0xC0FFEEull);

    std::printf("\n## D. Sweep signature, targeted case (budget draft 7.5)\n\n");
    reportSweepSignature();

    // Determinism across toolchains: run this binary built by different compilers and compare.
    std::printf("\n## D3. Compiled Pattern vs reference predicate (differential)\n\n");
    reportDifferential();

    std::printf("\n## D2. Multi-contact scenarios (Program IR 4.5, D10, slot reuse)\n\n");
    reportContacts();

    std::printf("\n## E. Result digests (compare across compilers)\n\n| profile | digest |\n| --- "
                "| --- |\n");
    for (const Profile& p : profiles) {
        rt::Interface iface;
        rt::Loadout loadout;
        Chart chart = generate(p, 0xC0FFEEull);
        rt::prepare(chart.reqs, iface, rt::loadoutMaxScale(iface, loadout));
        std::printf("| %s | %016llx |\n", p.name,
                    static_cast<unsigned long long>(runFull(iface, loadout, chart, false).digest));
    }
    std::printf("\nAll checks passed.\n");
    return 0;
}
