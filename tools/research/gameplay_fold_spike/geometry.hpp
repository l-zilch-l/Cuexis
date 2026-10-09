// Research spike: geometry, frame sampling and quantized rotation (defect D6, Program IR 5.4/5.5).
// All decision-side arithmetic is integer; the only double math here is the reference computation
// used to measure error. Not part of the product build. See README.md in this directory.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace spike::geo {

using Tick = std::int64_t;
using Angle = std::int32_t; // 1/65536 turn
inline constexpr Angle kFullTurn = 65536;
inline constexpr int kPosUnit = 1024; // positions are 1/1024 px

struct Point {
    std::int32_t x = 0;
    std::int32_t y = 0;
};

// Fixed-point sine/cosine over the quantized angle. Values are Q16 (65536 == 1.0).
std::int32_t sinQ16(Angle a);
std::int32_t cosQ16(Angle a);
// Maximum absolute deviation of the table from the exact value, in Q16 units.
int tableMaxErrorQ16();

// Round-half-even integer division by 2^shift. Negative values round symmetrically.
std::int64_t roundShiftEaven(std::int64_t value, int shift);

struct Transform {
    Point origin{};
    Angle angle = 0;
    std::int32_t scaleQ16 = 65536;
};

// One quantized affine operation: translate, rotate, scale, round once.
Point apply(const Transform& t, Point p);

enum class SegKind : std::uint8_t { Const, Lerp };
struct Segment {
    Tick t = 0;
    SegKind kind = SegKind::Const;
    Point origin{};
    Angle angle = 0;
    std::int32_t scaleQ16 = 65536;
    // For Lerp: the value at the end of this segment (the next node).
    Point originNext{};
    Angle angleNext = 0;
    std::int32_t scaleNextQ16 = 65536;
};

struct Frame {
    std::vector<Segment> segments; // strictly increasing t, contiguous coverage
    // Binary search; cost is O(log N).
    Transform at(Tick t) const;
    mutable std::uint64_t evalOps = 0; // measured comparisons, for the cost budget
};

enum class ShapeKind : std::uint8_t { Segment, Strip, Rect, Disc, Corridor };

struct Region {
    ShapeKind kind = ShapeKind::Rect;
    bool hasFrame = false;
    std::uint32_t frameIndex = 0;
    // Segment: zero-width line from a to b. Strip: infinite line through a with direction d, half
    // width w. Rect: centre a, half extents h. Disc: centre a, radius r.
    Point a{};
    Point b{};
    std::int32_t halfWidth = 0;
    // Corridor: polyline with per-node half widths.
    std::vector<Point> path;
    std::vector<std::int32_t> pathHalfWidths;
    // Inclusive bounds test result, reported by the report.
    bool contains(Point local, std::uint64_t* ops) const;
};

// Input resampling: continuous quantities are evaluated on a grid anchored at the requirement's
// arm tick (defect D1). Returns the sample offsets, in grid steps, within [0, end - arm).
std::vector<Tick> sampleGrid(Tick arm, Tick end, Tick period);

// Count of grid samples for a domain of the given duration and concurrent contacts.
std::uint64_t resampleCount(Tick duration, Tick period, int contacts);

// L1 model: raw contact positions at irregular instants, and the piecewise-linear reconstruction
// that answers a position query at any instant (IR 5.1). Whether the query instant lies on the
// normalized resampling grid is irrelevant to the value.
struct RawTrack {
    std::vector<Tick> t;
    std::vector<Point> p;
    Point at(Tick query) const;
};

// First instant at or after "from" on a grid anchored at "anchor".
Tick firstOnGrid(Tick anchor, Tick period, Tick from);

// Does a level predicate's truth, evaluated only at the given wake-up instants, ever lag more than
// the declared period? Returns the worst observed delay.
struct PredicateObservation {
    Tick worstDelay = 0;
    std::uint64_t evaluations = 0;
};

PredicateObservation observePredicate(const std::vector<Tick>& wakeups,
                                      const std::function<bool(Tick)>& predicate, Tick trueFrom,
                                      Tick trueUntil);

} // namespace spike::geo
