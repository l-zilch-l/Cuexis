// Research spike: tick-driven fold runtime for a small standard requirement library
// (Tap / Hold / Bomb / Roll), with arbitration, every(period) sampling anchored at arm,
// a t+1 derived hook, an L3 fold with one time-bounded module, and lossless snapshot / seek.
// Not part of the product build. See README.md in this directory.
#pragma once

#include <cstdint>
#include <vector>

namespace spike::rt {

using Tick = std::int64_t; // microseconds
inline constexpr int kLanes = 16;
inline constexpr int kOutcomes = 6;

enum class Kind : std::uint8_t { Tap, Hold, Bomb, Roll };
enum class Category : std::uint8_t { Note = 0, Penalty = 1 };
enum class Outcome : std::uint8_t { Hit, Miss, Broken, Avoided, Detonated, Stray };
enum class Grade : std::uint8_t { Perfect, Great, Good, None };

struct Requirement {
    std::uint32_t id = 0;
    Kind kind = Kind::Tap;
    std::uint8_t lane = 0;
    std::uint16_t rollTarget = 0;
    Tick anchor = 0;
    Tick end = 0;
    // Filled by prepare().
    Tick arm = 0;
    Tick headClose = 0;
    Tick deadline = 0;
};

struct InputEvent {
    Tick t = 0;
    std::uint8_t lane = 0;
    bool press = false;
};

struct Interface {
    Tick perfectUs = 40'000;
    Tick greatUs = 80'000;
    Tick goodUs = 120'000;
    Tick bombHalfWidthUs = 50'000;
    Tick holdGraceUs = 60'000;
    Tick samplePeriodUs = 4'000;
    int scaleMinPermille = 750;
    int scaleMaxPermille = 1250;
};

struct Loadout {
    bool boostModule = true;
    int boostPermille = 100;
    Tick boostDurationUs = 5'000'000;
    std::uint32_t boostComboStep = 100;
};

// Upper bound of the window-scale hook under this loadout (budget draft 7.5: the sweep takes the
// loadout value, not only the interface range).
int loadoutMaxScale(const Interface& iface, const Loadout& loadout);
void prepare(std::vector<Requirement>& reqs, const Interface& iface, int maxScalePermille);
std::uint64_t sweepActivityPeak(const std::vector<Requirement>& reqs);

struct Stats {
    std::uint64_t ticks = 0;
    std::uint64_t activations = 0;
    std::uint64_t inputs = 0;
    std::uint64_t timersFired = 0;
    std::uint64_t staleTimers = 0;
    std::uint64_t samples = 0;
    std::uint64_t guardOps = 0;
    std::uint64_t facts = 0;
    std::uint64_t maxActive = 0;
    std::uint64_t maxOpsInTick = 0;
    std::vector<std::uint32_t> samplesPerSecond;
    std::vector<std::uint64_t> opsPerSecond;
};

struct Summary {
    std::int64_t score = 0;
    std::uint32_t maxCombo = 0;
    std::int32_t life = 0;
    std::uint32_t failed = 0;
    std::uint32_t counts[2][kOutcomes] = {};
};

class Session {
  public:
    Session(const Interface& iface, const Loadout& loadout, const std::vector<Requirement>& reqs,
            const std::vector<InputEvent>& inputs);

    void runUntil(Tick through);
    std::vector<std::uint8_t> snapshot() const;
    void restore(const std::vector<std::uint8_t>& bytes);
    std::uint64_t digest() const;

    // L7 check: enumerate live instances in the opposite order. Results must not change.
    void setReverseEnumeration(bool reverse) {
        reverse_ = reverse;
    }
    // Finding D11: off reproduces the emission-order dependence; on is the proposed rule.
    void setCanonicalFactOrder(bool on) {
        canonicalFactOrder_ = on;
    }
    const Stats& stats() const {
        return stats_;
    }
    Summary summary() const;

  private:
    struct Inst {
        std::uint8_t phase = 0; // 0 head pending, 1 hold body
        std::uint8_t settled = 0;
        std::uint16_t count = 0;
        Tick holeStart = -1;
    };
    enum Edge : std::uint8_t { Close = 0, Sample = 1, BodyEnd = 2 };
    struct Timer {
        Tick t = 0;
        std::uint32_t ownerId = 0;
        std::uint8_t edge = 0;
    };
    struct Fact {
        Tick t = 0;
        std::uint32_t req = 0;
        Category cat = Category::Note;
        Outcome outcome = Outcome::Hit;
        Grade grade = Grade::None;
        std::uint8_t phase = 0;
        std::int32_t err = 0;
    };
    struct Fold {
        std::int64_t score = 0;
        std::uint32_t combo = 0;
        std::uint32_t maxCombo = 0;
        std::int32_t life = 1000;
        std::uint32_t failed = 0;
        std::uint32_t counts[2][kOutcomes] = {};
        std::uint32_t boostActive = 0;
        Tick boostExpiry = 0;
        // Derived hook. Written only in the fold phase, so every L2 read in tick t sees the value
        // left by tick t-1: the t+1 rule holds structurally.
        std::int32_t windowScale = 1000;
        std::uint64_t factHash = 0xcbf29ce484222325ull;
        std::uint64_t factCount = 0;
    };

    Tick nextEventTick() const;
    void processTick(Tick t);
    void handleTimer(const Timer& timer, Tick t);
    void dispatch(const InputEvent& ev, Tick t);
    void emit(Tick t, std::uint32_t req, Category cat, Outcome outcome, Grade grade,
              std::uint8_t phase, std::int32_t err);
    void fold(const Fact& fact);
    void pushTimer(const Timer& timer);
    Grade gradeOf(Tick absErr) const;
    Tick nextSampleAfter(const Requirement& r, Tick t) const;
    void count(std::uint64_t ops) {
        stats_.guardOps += ops;
        opsThisTick_ += ops;
    }

    const Interface& iface_;
    const Loadout& loadout_;
    const std::vector<Requirement>& reqs_;
    const std::vector<InputEvent>& inputs_;
    std::vector<std::uint32_t> armOrder_;

    // Session state, canonically serialized by snapshot().
    Tick processedThrough_ = -1;
    std::size_t nextArm_ = 0;
    std::size_t inputCursor_ = 0;
    std::uint8_t laneHeld_[kLanes] = {};
    std::vector<Inst> insts_;
    std::vector<std::uint32_t> active_;
    std::vector<Timer> timers_; // binary min-heap on (t, ownerId, edge)
    Fold fold_;

    // Scratch and measurement, not state.
    std::vector<Fact> pending_;
    std::vector<std::uint32_t> candidates_;
    bool reverse_ = false;
    bool canonicalFactOrder_ = true;
    Stats stats_;
    std::uint64_t opsThisTick_ = 0;
};

} // namespace spike::rt
