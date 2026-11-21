#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace spike::geo {

namespace {

using SignedWide = __int128_t;
using Wide = __uint128_t;

Wide absWide(SignedWide value) {
    return value < 0 ? static_cast<Wide>(-(value + 1)) + 1 : static_cast<Wide>(value);
}

Wide squareChecked(Wide value) {
    const Wide max = ~Wide{0};
    if (value != 0 && value > max / value) {
        throw std::overflow_error("geometry square exceeds declared integer range");
    }
    return value * value;
}

Wide multiplyChecked(Wide lhs, Wide rhs) {
    const Wide max = ~Wide{0};
    if (lhs != 0 && rhs > max / lhs) {
        throw std::overflow_error("geometry product exceeds declared integer range");
    }
    return lhs * rhs;
}

Wide addChecked(Wide lhs, Wide rhs) {
    const Wide max = ~Wide{0};
    if (rhs > max - lhs) {
        throw std::overflow_error("geometry sum exceeds declared integer range");
    }
    return lhs + rhs;
}

Wide crossMagnitude(std::int64_t dx, std::int64_t dy, std::int64_t px, std::int64_t py) {
    const SignedWide cross = static_cast<SignedWide>(dx) * py - static_cast<SignedWide>(dy) * px;
    return absWide(cross);
}

Wide lengthSquared(std::int64_t dx, std::int64_t dy) {
    const Wide x = absWide(static_cast<SignedWide>(dx));
    const Wide y = absWide(static_cast<SignedWide>(dy));
    return addChecked(multiplyChecked(x, x), multiplyChecked(y, y));
}

bool withinHalfWidth(Wide cross, Wide halfWidth, Wide len2) {
    if (len2 == 0) {
        return cross == 0;
    }
    return squareChecked(cross) <= multiplyChecked(squareChecked(halfWidth), len2);
}

// Generated at load time from double math, then frozen for the run. The report measures the
// deviation of the frozen table from the exact values, which is what a shipped table would have.
const std::vector<std::int32_t>& sinTable() {
    static const std::vector<std::int32_t> table = [] {
        std::vector<std::int32_t> t(kFullTurn + 1);
        for (int i = 0; i <= kFullTurn; ++i) {
            const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i) /
                                 static_cast<double>(kFullTurn);
            t[static_cast<std::size_t>(i)] =
                static_cast<std::int32_t>(std::llround(std::sin(angle) * 65536.0));
        }
        return t;
    }();
    return table;
}

Angle wrap(Angle a) {
    a %= kFullTurn;
    if (a < 0) {
        a += kFullTurn;
    }
    return a;
}

} // namespace

std::int32_t sinQ16(Angle a) {
    return sinTable()[static_cast<std::size_t>(wrap(a))];
}

std::int32_t cosQ16(Angle a) {
    return sinTable()[static_cast<std::size_t>(wrap(a + kFullTurn / 4))];
}

int tableMaxErrorQ16() {
    int worst = 0;
    for (int i = 0; i <= kFullTurn; ++i) {
        const double angle =
            2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(kFullTurn);
        const double exact = std::sin(angle) * 65536.0;
        const int deviation = static_cast<int>(
            std::llabs(std::llround(exact) - sinTable()[static_cast<std::size_t>(i)]));
        worst = std::max(worst, deviation);
    }
    return worst;
}

std::int64_t roundShiftEaven(std::int64_t value, int shift) {
    if (shift <= 0) {
        return value;
    }
    const std::int64_t half = std::int64_t{1} << (shift - 1);
    const std::int64_t sign = value < 0 ? -1 : 1;
    const std::int64_t magnitude = value < 0 ? -value : value;
    const std::int64_t quotient = magnitude >> shift;
    const std::int64_t remainder = magnitude - (quotient << shift);
    std::int64_t result = quotient;
    if (remainder > half || (remainder == half && (quotient & 1) != 0)) {
        ++result;
    }
    return sign * result;
}

Point apply(const Transform& t, Point p) {
    const std::int64_t dx = static_cast<std::int64_t>(p.x) - t.origin.x;
    const std::int64_t dy = static_cast<std::int64_t>(p.y) - t.origin.y;
    const std::int64_t c = cosQ16(t.angle);
    const std::int64_t s = sinQ16(t.angle);
    // One quantized affine operation: rotate, scale, round once (D6's fix).
    const std::int64_t rx = roundShiftEaven((dx * c - dy * s) * t.scaleQ16, 32);
    const std::int64_t ry = roundShiftEaven((dx * s + dy * c) * t.scaleQ16, 32);
    // Translation is in the parent frame, so it does not rotate.
    return Point{static_cast<std::int32_t>(rx + t.origin.x),
                 static_cast<std::int32_t>(ry + t.origin.y)};
}

Transform Frame::at(Tick t) const {
    Transform out;
    if (segments.empty()) {
        return out;
    }
    ++evalOps;
    // Binary search over segment starts.
    std::size_t lo = 0;
    std::size_t hi = segments.size();
    while (lo + 1 < hi) {
        const std::size_t mid = (lo + hi) / 2;
        ++evalOps;
        if (segments[mid].t <= t) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    const Segment& seg = segments[lo];
    if (seg.kind == SegKind::Const || lo + 1 >= segments.size()) {
        out.origin = seg.origin;
        out.angle = seg.angle;
        out.scaleQ16 = seg.scaleQ16;
        return out;
    }
    const Tick span = segments[lo + 1].t - seg.t;
    if (span <= 0) {
        out.origin = seg.origin;
        out.angle = seg.angle;
        out.scaleQ16 = seg.scaleQ16;
        return out;
    }
    const Tick elapsed = t - seg.t;
    // Integer linear interpolation with one rounding; monotone and order preserving.
    auto lerp = [&](std::int32_t from, std::int32_t to) {
        const std::int64_t delta = static_cast<std::int64_t>(to) - from;
        const std::int64_t scaled = roundShiftEaven(delta * elapsed, 0);
        return static_cast<std::int32_t>(from + scaled / span);
    };
    out.origin = Point{lerp(seg.origin.x, seg.originNext.x), lerp(seg.origin.y, seg.originNext.y)};
    out.angle = lerp(seg.angle, seg.angleNext);
    out.scaleQ16 = lerp(seg.scaleQ16, seg.scaleNextQ16);
    return out;
}

bool Region::contains(Point local, std::uint64_t* ops) const {
    auto add = [&](std::uint64_t n) {
        if (ops != nullptr) {
            *ops += n;
        }
    };
    switch (kind) {
    case ShapeKind::Disc: {
        const std::int64_t dx = static_cast<std::int64_t>(local.x) - a.x;
        const std::int64_t dy = static_cast<std::int64_t>(local.y) - a.y;
        add(5);
        return dx * dx + dy * dy <= static_cast<std::int64_t>(halfWidth) * halfWidth;
    }
    case ShapeKind::Rect: {
        add(4);
        return std::llabs(static_cast<std::int64_t>(local.x) - a.x) <= halfWidth &&
               std::llabs(static_cast<std::int64_t>(local.y) - a.y) <= halfWidth;
    }
    case ShapeKind::Strip: {
        // Distance to the infinite line through a with direction (b - a), half width w.
        const std::int64_t dx = static_cast<std::int64_t>(b.x) - a.x;
        const std::int64_t dy = static_cast<std::int64_t>(b.y) - a.y;
        const std::int64_t px = static_cast<std::int64_t>(local.x) - a.x;
        const std::int64_t py = static_cast<std::int64_t>(local.y) - a.y;
        const Wide cross = crossMagnitude(dx, dy, px, py);
        const Wide len2 = lengthSquared(dx, dy);
        add(9);
        return withinHalfWidth(cross, absWide(halfWidth), len2);
    }
    case ShapeKind::Segment: {
        const std::int64_t dx = static_cast<std::int64_t>(b.x) - a.x;
        const std::int64_t dy = static_cast<std::int64_t>(b.y) - a.y;
        const std::int64_t px = static_cast<std::int64_t>(local.x) - a.x;
        const std::int64_t py = static_cast<std::int64_t>(local.y) - a.y;
        const Wide len2 = lengthSquared(dx, dy);
        const SignedWide dot = static_cast<SignedWide>(dx) * px + static_cast<SignedWide>(dy) * py;
        add(8);
        return dot >= 0 && dot <= len2 &&
               withinHalfWidth(crossMagnitude(dx, dy, px, py), absWide(halfWidth), len2);
    }
    case ShapeKind::Corridor: {
        if (path.size() < 2) {
            return false;
        }
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            const std::int64_t dx = static_cast<std::int64_t>(path[i + 1].x) - path[i].x;
            const std::int64_t dy = static_cast<std::int64_t>(path[i + 1].y) - path[i].y;
            const std::int64_t px = static_cast<std::int64_t>(local.x) - path[i].x;
            const std::int64_t py = static_cast<std::int64_t>(local.y) - path[i].y;
            const Wide len2 = lengthSquared(dx, dy);
            const SignedWide dot =
                static_cast<SignedWide>(dx) * px + static_cast<SignedWide>(dy) * py;
            add(12);
            if (dot < 0 || dot > len2) {
                continue;
            }
            const Wide w = absWide(pathHalfWidths[i]);
            if (withinHalfWidth(crossMagnitude(dx, dy, px, py), w, len2)) {
                return true;
            }
        }
        return false;
    }
    }
    return false;
}

std::vector<Tick> sampleGrid(Tick arm, Tick end, Tick period) {
    std::vector<Tick> out;
    if (period <= 0) {
        return out;
    }
    for (Tick t = arm; t < end; t += period) {
        out.push_back(t);
    }
    return out;
}

Point RawTrack::at(Tick query) const {
    if (t.empty()) {
        return Point{};
    }
    if (query <= t.front()) {
        return p.front();
    }
    if (query >= t.back()) {
        return p.back();
    }
    std::size_t lo = 0;
    std::size_t hi = t.size() - 1;
    while (lo + 1 < hi) {
        const std::size_t mid = (lo + hi) / 2;
        if (t[mid] <= query) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    const Tick span = t[lo + 1] - t[lo];
    if (span <= 0) {
        return p[lo];
    }
    const Tick elapsed = query - t[lo];
    auto lerp = [&](std::int32_t a, std::int32_t b) {
        return static_cast<std::int32_t>(
            a + roundShiftEaven(static_cast<std::int64_t>(b - a) * elapsed, 0) / span);
    };
    return Point{lerp(p[lo].x, p[lo + 1].x), lerp(p[lo].y, p[lo + 1].y)};
}

Tick firstOnGrid(Tick anchor, Tick period, Tick from) {
    if (period <= 0 || from <= anchor) {
        return anchor;
    }
    const Tick k = (from - anchor + period - 1) / period;
    return anchor + k * period;
}

PredicateObservation observePredicate(const std::vector<Tick>& wakeups,
                                      const std::function<bool(Tick)>& predicate, Tick trueFrom,
                                      Tick trueUntil) {
    // Detection latency: how late the first wake-up that sees the transition arrives, relative to
    // the instant the transition actually happened. Bounded by one sampling period.
    PredicateObservation out;
    bool seen = false;
    for (const Tick t : wakeups) {
        ++out.evaluations;
        const bool got = predicate(t);
        if (got && !seen) {
            seen = true;
            const Tick delay = (t > trueFrom) ? t - trueFrom : 0;
            out.worstDelay = std::max(out.worstDelay, delay);
        }
    }
    (void)trueUntil;
    return out;
}

std::uint64_t resampleCount(Tick duration, Tick period, int contacts) {
    if (period <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(duration / period) * static_cast<std::uint64_t>(contacts);
}

} // namespace spike::geo
