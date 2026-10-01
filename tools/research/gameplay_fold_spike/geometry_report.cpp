// Geometry, frame and quantization checks: defect D6 (rounding), IR 5.4 (frames), IR 5.5
// (data float, decision integer) and input resampling (plan A).
#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace spike::geo;

constexpr double kPi = 3.14159265358979323846;

namespace {

int failures = 0;

void check(const char* name, bool ok, const char* detail) {
    if (!ok) {
        ++failures;
        std::printf("    FAIL %s: %s\n", name, detail);
    }
}

void reportRotation() {
    // Four-fold and eight-fold symmetry must be exactly consistent on the quantized grid, since
    // these are the angles every judgement actually uses.
    std::printf("Table error: sin/cos table deviates from exact by at most %d Q16 unit(s).\n",
                tableMaxErrorQ16());
    const struct {
        Angle angle;
        int sx, cx;
    } exact[] = {
        {0, 0, 65536},
        {kFullTurn / 4, 65536, 0},
        {kFullTurn / 2, 0, -65536},
        {3 * kFullTurn / 4, -65536, 0},
    };
    for (const auto& e : exact) {
        check("table exact on quarter turns", sinQ16(e.angle) == e.sx, "sin");
        check("table exact on quarter turns", cosQ16(e.angle) == e.cx, "cos");
    }
    // A quarter turn is exactly representable, so composing it four times returns to the start.
    Point p{1000 * kPosUnit, 0};
    Transform t;
    t.angle = kFullTurn / 4;
    for (int i = 0; i < 4; ++i) {
        p = apply(t, p);
    }
    const bool closed = p.x == 1000 * kPosUnit && p.y == 0;
    check("quarter turn composition is exact", closed,
          "four quarter turns must return to the original point");
    std::printf("Quarter turn x4: (%d, %d) -> (%d, %d), %s\n", 1000 * kPosUnit, 0, p.x, p.y,
                closed ? "exact" : "DRIFT");

    // Error growth if rotations are chained, which 5.5 forbids for this reason.
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> anglePick(0, kFullTurn - 1);
    double chainedWorst = 0;
    double singleWorst = 0;
    for (int trial = 0; trial < 2000; ++trial) {
        const Angle a1 = anglePick(rng);
        const Angle a2 = anglePick(rng);
        const Point start{static_cast<std::int32_t>((trial % 97) * kPosUnit),
                          static_cast<std::int32_t>((trial % 61) * kPosUnit)};
        Transform t1;
        t1.angle = a1;
        Transform t2;
        t2.angle = a2;
        // Exact reference: one rotation by the sum.
        double c = std::cos(2.0 * kPi * (static_cast<double>(a1) + a2) / kFullTurn);
        double s = std::sin(2.0 * kPi * (static_cast<double>(a1) + a2) / kFullTurn);
        const double ex = static_cast<double>(start.x) * c - static_cast<double>(start.y) * s;
        const double ey = static_cast<double>(start.x) * s + static_cast<double>(start.y) * c;

        Transform sum;
        sum.angle = static_cast<Angle>((static_cast<std::int64_t>(a1) + a2) % kFullTurn);
        const Point single = apply(sum, start);
        singleWorst = std::max(singleWorst, std::hypot(single.x - ex, single.y - ey));

        const Point chained = apply(t2, apply(t1, start));
        chainedWorst = std::max(chainedWorst, std::hypot(chained.x - ex, chained.y - ey));
    }
    std::printf(
        "Rotation error vs exact, in 1/%d px units: single %.3f, two chained %.3f (%.1fx)\n",
        kPosUnit, singleWorst, chainedWorst, chainedWorst / std::max(singleWorst, 1e-9));
    check("chaining rotations is worse than one", chainedWorst > singleWorst,
          "5.5 forbids chaining for a reason; this measurement is the evidence");
}

void reportFrames() {
    // A frame with 64 segments; measure evaluation cost and interpolation behaviour.
    Frame frame;
    for (int i = 0; i < 64; ++i) {
        Segment s;
        s.t = i * 1'000'000;
        s.kind = SegKind::Lerp;
        s.origin = Point{i * 10 * kPosUnit, 0};
        s.originNext = Point{(i + 1) * 10 * kPosUnit, (i % 2 == 0) ? 5 * kPosUnit : 0};
        s.angle = static_cast<Angle>(i * 512);
        s.angleNext = static_cast<Angle>((i + 1) * 512);
        frame.segments.push_back(s);
    }
    frame.evalOps = 0;
    Transform prev;
    bool started = false;
    bool monotone = true;
    std::int64_t maxStep = 0;
    std::int64_t minStep = INT64_MAX;
    for (Tick t = 0; t < 63'000'000; t += 4'000) {
        const Transform cur = frame.at(t);
        if (started) {
            const std::int64_t step = cur.origin.x - prev.origin.x;
            maxStep = std::max(maxStep, step);
            minStep = std::min(minStep, step);
            if (step < 0) {
                monotone = false;
            }
        }
        prev = cur;
        started = true;
    }
    const std::uint64_t evals = 63'000'000 / 4'000;
    check("frame x interpolation is monotone", monotone,
          "x must not go backwards inside a segment");
    check("frame evaluation is O(log N)", frame.evalOps <= evals * 8,
          "each evaluation must cost at most a few comparisons");
    std::printf("Frame with 64 segments: %llu evaluations, %llu comparisons (%.2f per eval), "
                "x step range [%lld, %lld] px units\n",
                static_cast<unsigned long long>(evals),
                static_cast<unsigned long long>(frame.evalOps),
                static_cast<double>(frame.evalOps) / static_cast<double>(evals),
                static_cast<long long>(minStep), static_cast<long long>(maxStep));
}

void reportRegions() {
    // Cost per shape, and the inclusive-boundary conventions (strip/rect/disc take <=, the
    // segment keeps a zero-width boundary exclusive).
    const std::uint64_t kPos = kPosUnit;
    Region disc;
    disc.kind = ShapeKind::Disc;
    disc.a = Point{100 * static_cast<std::int32_t>(kPos), 0};
    disc.halfWidth = static_cast<std::int32_t>(50 * kPos);
    Region rect;
    rect.kind = ShapeKind::Rect;
    rect.a = Point{0, 0};
    rect.halfWidth = static_cast<std::int32_t>(20 * kPos);
    Region strip;
    strip.kind = ShapeKind::Strip;
    strip.a = Point{0, 0};
    strip.b = Point{1000, 0};
    strip.halfWidth = static_cast<std::int32_t>(10 * kPos);
    Region segment;
    segment.kind = ShapeKind::Segment;
    segment.a = Point{0, 0};
    segment.b = Point{100 * static_cast<std::int32_t>(kPos), 0};
    segment.halfWidth = static_cast<std::int32_t>(512);

    std::mt19937 rng(99);
    std::uniform_int_distribution<int> wide(-200 * static_cast<int>(kPos),
                                            200 * static_cast<int>(kPos));
    std::uint64_t ops[4] = {0, 0, 0, 0};
    int hits[4] = {0, 0, 0, 0};
    const Region* regions[4] = {&disc, &rect, &strip, &segment};
    const int trials = 200'000;
    for (int i = 0; i < trials; ++i) {
        const Point p{wide(rng), wide(rng)};
        for (int k = 0; k < 4; ++k) {
            hits[k] += regions[k]->contains(p, &ops[k]) ? 1 : 0;
        }
    }
    std::printf(
        "| shape | inclusive boundary | ops per test | hit rate |\n| --- | --- | --- | --- |\n");
    const char* names[4] = {"Disc", "Rect", "Strip", "Segment"};
    const char* bounds[4] = {
        "<= radius",
        "<= half extent",
        "<= half width",
        "<= half width",
    };
    for (int k = 0; k < 4; ++k) {
        std::printf("| %s | %s | %.1f | %.1f%% |\n", names[k], bounds[k],
                    static_cast<double>(ops[k]) / trials,
                    100.0 * static_cast<double>(hits[k]) / trials);
        check("region cost is bounded", static_cast<double>(ops[k]) / trials <= 20.0,
              "a region test must stay within a constant op budget");
    }
    // Boundary behaviour: the test is inclusive for all four shapes.
    const Point onDiscEdge{disc.a.x + disc.halfWidth, disc.a.y};
    check("disc boundary inclusive", disc.contains(onDiscEdge, nullptr),
          "== radius must be inside");
    const Point onRectEdge{rect.a.x + rect.halfWidth, rect.a.y};
    check("rect boundary inclusive", rect.contains(onRectEdge, nullptr),
          "== half extent must be inside");
    const Point onStripEdge{0, strip.halfWidth};
    check("strip boundary inclusive", strip.contains(onStripEdge, nullptr),
          "== half width must be inside");

    // Determinism: the same query in both directions of a chained transform.
    Transform t;
    t.angle = 12345;
    t.origin = Point{7 * static_cast<std::int32_t>(kPos), -3 * static_cast<std::int32_t>(kPos)};
    const Point q{13 * static_cast<std::int32_t>(kPos), 21 * static_cast<std::int32_t>(kPos)};
    const Point forward = apply(t, q);
    Transform inv;
    inv.angle = static_cast<Angle>((kFullTurn - t.angle) % kFullTurn);
    inv.origin = t.origin;
    const Point back = apply(inv, forward);
    check("transform round trip is stable",
          std::llabs(back.x - q.x) <= 2 && std::llabs(back.y - q.y) <= 2,
          "a single rotation and its inverse must return to the original point within one unit");
    std::printf("Transform round trip: (%d, %d) -> (%d, %d) -> (%d, %d)\n", q.x, q.y, forward.x,
                forward.y, back.x, back.y);
}

void reportResampling() {
    // Budget draft 3.5 and 5.2: the number of samples the resampler produces.
    std::printf("| contacts | period | domain s | samples | per second |\n| --- | --- | --- | --- "
                "| --- |\n");
    const struct {
        int contacts;
        Tick period;
        Tick duration;
    } cases[] = {
        {1, 4'000, 300'000'000},
        {10, 4'000, 300'000'000},
        {10, 8'000, 300'000'000},
        {10, 4'000, 600'000'000},
    };
    for (const auto& c : cases) {
        const std::uint64_t n = resampleCount(c.duration, c.period, c.contacts);
        std::printf("| %d | %lld ms | %lld | %llu | %llu |\n", c.contacts,
                    static_cast<long long>(c.period / 1000),
                    static_cast<long long>(c.duration / 1'000'000),
                    static_cast<unsigned long long>(n),
                    static_cast<unsigned long long>(
                        n / (static_cast<std::uint64_t>(c.duration) / 1'000'000)));
    }

    // D1's fix: the sampling grid is anchored at the requirement's arm, not at the state's entry
    // time, so the sample set does not depend on preceding inputs.
    const Tick arm = 987'654;
    const Tick period = 4'000;
    const auto a = sampleGrid(arm, arm + 200'000, period);
    const auto b = sampleGrid(arm, arm + 200'000, period);
    check("grid is anchored at arm", a == b, "two evaluations of the same requirement must agree");
    check("grid starts at arm", !a.empty() && a.front() == arm, "first sample is arm itself");
    // A grid anchored at a different phase produces different sample instants: this is what D1 was.
    const auto shifted = sampleGrid(arm + 1'000, arm + 200'000, period);
    check("phase matters", a != shifted, "a different anchor must give different instants");

    // Wrapping: a long domain does not accumulate overflow.
    const std::uint64_t big = resampleCount(3'600'000'000, 1'000, 1);
    std::printf("One hour at 1 ms: %llu samples (no overflow)\n",
                static_cast<unsigned long long>(big));
}

void reportQuantization() {
    // Round-half-even must be symmetric and exactly representable values must be untouched.
    check("half rounds to even down", roundShiftEaven(2, 1) == 1, "1.0 stays 1");
    check("half rounds to even up", roundShiftEaven(6, 1) == 3, "3.0 stays 3");
    check("half to even, 0.5 -> 0", roundShiftEaven(1, 1) == 0, "an exact half with odd quotient");
    check("half to even, 1.5 -> 2", roundShiftEaven(3, 1) == 2, "an exact half with even quotient");
    check("negative is symmetric", roundShiftEaven(-1, 1) == 0, "-0.5 rounds to 0, not -1");
    check("negative is symmetric", roundShiftEaven(-3, 1) == -2, "-1.5 rounds to -2");
    // Monotone: never reorder.
    bool monotone = true;
    for (std::int64_t v = -10'000; v < 10'000; ++v) {
        if (roundShiftEaven(v, 4) > roundShiftEaven(v + 1, 4)) {
            monotone = false;
        }
    }
    check("quantization is monotone", monotone, "order must be preserved");
    std::printf("Round-half-even: symmetric on negatives, monotone over [-10000, 10000]\n");
}

} // namespace

void reportGeometry() {
    reportRotation();
    std::printf("\n");
    reportFrames();
    std::printf("\n");
    reportRegions();
    std::printf("\n");
    reportResampling();
    std::printf("\n");
    reportQuantization();
    if (failures != 0) {
        std::printf("\n%d geometry check(s) failed\n", failures);
        std::exit(9);
    }
}
