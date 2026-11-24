// Research spike: multi-contact claim / grip / visibility rules, with scripted scenarios.
// Complements runtime.cpp, which only models one lane-level press at a time.
// Not part of the product build. See README.md in this directory.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace spike::mt {

using Tick = std::int64_t;

enum class Kind : std::uint8_t { Tap, Hold, Bomb, Roll };
// grip declares whether a requirement may change hands mid-way.
enum class Grip : std::uint8_t { Sticky, Handoff };
enum class Outcome : std::uint8_t { Hit, Miss, Broken, Avoided, Detonated, Stray };

struct Req {
    std::uint32_t id = 0;
    Kind kind = Kind::Tap;
    Grip grip = Grip::Sticky;
    std::uint8_t lane = 0;
    Tick anchor = 0;
    Tick end = 0;
    std::uint16_t rollTarget = 0;
};

struct ContactEvent {
    Tick t = 0;
    std::uint32_t contactId = 0;
    std::uint8_t lane = 0;
    bool begin = true;
};

// Defect D10's axis: when does an unconsumed input count as a stray?
//   ConsumeEmpty  the consume group is empty. A press next to a bomb is never a stray.
//   NoCandidate   no instance listened at all, not even an observe edge.
enum class StrayRule : std::uint8_t { ConsumeEmpty, NoCandidate };

struct Config {
    Tick goodUs = 120'000;
    Tick holdGraceUs = 60'000;
    Tick samplePeriodUs = 4'000;
    bool fanoutAllOnEqualAnchor = false;
    StrayRule strayWhen = StrayRule::ConsumeEmpty;
};

struct Fact {
    Tick t = 0;
    std::uint32_t req = 0;
    Outcome outcome = Outcome::Hit;
    std::uint8_t phase = 0;
    bool observed = false; // produced by an observe edge (bomb), not a claim
};

struct Result {
    std::vector<Fact> facts;
    std::size_t strays = 0;
    std::string trace;
};

// Runs one scenario. Facts are returned in canonical (requirementId, phase, outcome) order, which
// is what the ruleset fold language requires of same-tick facts.
Result run(const Config& cfg, const std::vector<Req>& reqs,
           const std::vector<ContactEvent>& events);

// One-line rendering of a result, for scenario tables.
std::string describe(const Result& result);

} // namespace spike::mt
