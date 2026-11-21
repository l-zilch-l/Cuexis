#include "runtime.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace spike::rt {

namespace {

constexpr std::uint32_t kCore = 0xFFFFFFFFu;

bool timerLess(const auto& a, const auto& b) {
    if (a.t != b.t) {
        return a.t < b.t;
    }
    if (a.ownerId != b.ownerId) {
        return a.ownerId < b.ownerId;
    }
    return a.edge < b.edge;
}

// std heap helpers want a max-heap comparator; invert to get the earliest timer on top.
struct TimerAfter {
    bool operator()(const auto& a, const auto& b) const {
        return timerLess(b, a);
    }
};

Tick scaled(Tick us, int permille) {
    return us * permille / 1000;
}

class Writer {
  public:
    explicit Writer(std::vector<std::uint8_t>& out) : out_(out) {}
    template <class T> void put(const T& value) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(&value);
        out_.insert(out_.end(), p, p + sizeof(T));
    }

  private:
    std::vector<std::uint8_t>& out_;
};

class Reader {
  public:
    explicit Reader(const std::vector<std::uint8_t>& in) : in_(in) {}
    template <class T> T get() {
        if (pos_ + sizeof(T) > in_.size()) {
            throw std::runtime_error("snapshot truncated");
        }
        T value;
        std::memcpy(&value, in_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return value;
    }

  private:
    const std::vector<std::uint8_t>& in_;
    std::size_t pos_ = 0;
};

} // namespace

int loadoutMaxScale(const Interface& iface, const Loadout& loadout) {
    const int raw = 1000 + (loadout.boostModule ? loadout.boostPermille : 0);
    return std::clamp(raw, iface.scaleMinPermille, iface.scaleMaxPermille);
}

void prepare(std::vector<Requirement>& reqs, const Interface& iface, int maxScalePermille) {
    const Tick good = scaled(iface.goodUs, maxScalePermille);
    for (auto& r : reqs) {
        switch (r.kind) {
        case Kind::Tap:
            r.arm = r.anchor - good;
            r.headClose = r.anchor + good;
            r.deadline = r.headClose;
            break;
        case Kind::Hold:
            r.arm = r.anchor - good;
            r.headClose = r.anchor + good;
            r.deadline = std::max(r.headClose, r.end);
            break;
        case Kind::Bomb:
            r.arm = r.anchor - iface.bombHalfWidthUs;
            r.headClose = r.anchor + iface.bombHalfWidthUs;
            r.deadline = r.headClose;
            break;
        case Kind::Roll:
            r.arm = r.anchor;
            r.headClose = r.end;
            r.deadline = r.end;
            break;
        }
        r.arm = std::max<Tick>(r.arm, 0);
    }
}

std::uint64_t sweepActivityPeak(const std::vector<Requirement>& reqs) {
    std::vector<std::pair<Tick, int>> points;
    points.reserve(reqs.size() * 2);
    for (const auto& r : reqs) {
        points.emplace_back(r.arm, +1);
        points.emplace_back(r.deadline + 1, -1);
    }
    std::sort(points.begin(), points.end());
    std::int64_t live = 0;
    std::int64_t peak = 0;
    for (const auto& [t, delta] : points) {
        live += delta;
        peak = std::max(peak, live);
    }
    return static_cast<std::uint64_t>(peak);
}

Session::Session(const Interface& iface, const Loadout& loadout,
                 const std::vector<Requirement>& reqs, const std::vector<InputEvent>& inputs)
    : iface_(iface), loadout_(loadout), reqs_(reqs), inputs_(inputs), insts_(reqs.size()) {
    armOrder_.resize(reqs.size());
    for (std::uint32_t i = 0; i < reqs.size(); ++i) {
        if (reqs[i].id != i) {
            throw std::runtime_error("requirement ids must be dense and ordered");
        }
        armOrder_[i] = i;
    }
    std::stable_sort(armOrder_.begin(), armOrder_.end(),
                     [&](std::uint32_t a, std::uint32_t b) { return reqs_[a].arm < reqs_[b].arm; });
}

Tick Session::nextEventTick() const {
    Tick next = INT64_MAX;
    if (nextArm_ < armOrder_.size()) {
        next = std::min(next, reqs_[armOrder_[nextArm_]].arm);
    }
    if (inputCursor_ < inputs_.size()) {
        next = std::min(next, inputs_[inputCursor_].t);
    }
    if (!timers_.empty()) {
        next = std::min(next, timers_.front().t);
    }
    return next;
}

void Session::runUntil(Tick through) {
    while (true) {
        const Tick t = nextEventTick();
        if (t == INT64_MAX || t > through) {
            break;
        }
        processTick(t);
    }
    processedThrough_ = std::max(processedThrough_, through);
}

void Session::pushTimer(const Timer& timer) {
    timers_.push_back(timer);
    std::push_heap(timers_.begin(), timers_.end(), TimerAfter{});
}

Tick Session::nextSampleAfter(const Requirement& r, Tick t) const {
    // Sampling grid anchored at the requirement's own arm tick (defect D1's fix).
    const Tick p = iface_.samplePeriodUs;
    const Tick k = (t - r.arm) / p + 1;
    return r.arm + k * p;
}

Grade Session::gradeOf(Tick absErr) const {
    const int scale = fold_.windowScale;
    if (absErr <= scaled(iface_.perfectUs, scale)) {
        return Grade::Perfect;
    }
    if (absErr <= scaled(iface_.greatUs, scale)) {
        return Grade::Great;
    }
    if (absErr <= scaled(iface_.goodUs, scale)) {
        return Grade::Good;
    }
    return Grade::None;
}

void Session::emit(Tick t, std::uint32_t req, Category cat, Outcome outcome, Grade grade,
                   std::uint8_t phase, std::int32_t err) {
    pending_.push_back(Fact{t, req, cat, outcome, grade, phase, err});
}

void Session::processTick(Tick t) {
    ++stats_.ticks;
    opsThisTick_ = 0;
    pending_.clear();

    // 1. Activation.
    while (nextArm_ < armOrder_.size() && reqs_[armOrder_[nextArm_]].arm == t) {
        const std::uint32_t id = armOrder_[nextArm_++];
        const Requirement& r = reqs_[id];
        active_.push_back(id);
        ++stats_.activations;
        count(1);
        pushTimer({r.headClose, id, Close});
        if (r.kind == Kind::Hold) {
            pushTimer({r.end, id, BodyEnd});
        }
    }

    // 2. Timers, in (t, ownerId, edge) order.
    bool coreExpired = false;
    while (!timers_.empty() && timers_.front().t == t) {
        std::pop_heap(timers_.begin(), timers_.end(), TimerAfter{});
        const Timer timer = timers_.back();
        timers_.pop_back();
        if (timer.ownerId == kCore) {
            coreExpired = true; // handled in the fold phase, where hooks are written
            continue;
        }
        handleTimer(timer, t);
    }

    // 3. Inputs.
    while (inputCursor_ < inputs_.size() && inputs_[inputCursor_].t == t) {
        dispatch(inputs_[inputCursor_], t);
        ++inputCursor_;
        ++stats_.inputs;
    }

    // 4. Fold: L3 timers first, then facts in emission order.
    if (coreExpired && fold_.boostActive != 0 && fold_.boostExpiry == t) {
        fold_.boostActive = 0;
    }
    // Emission order inside a tick depends on instance enumeration (one press can detonate two
    // bombs). Fold in a canonical order instead: (requirementId, phase, outcome). Without this
    // the fact stream, and any order-sensitive handler, differs between enumeration orders.
    if (canonicalFactOrder_) {
        std::stable_sort(pending_.begin(), pending_.end(), [](const Fact& a, const Fact& b) {
            if (a.req != b.req) {
                return a.req < b.req;
            }
            if (a.phase != b.phase) {
                return a.phase < b.phase;
            }
            return a.outcome < b.outcome;
        });
    }
    for (const Fact& fact : pending_) {
        fold(fact);
    }
    const int raw = 1000 + (fold_.boostActive != 0 ? loadout_.boostPermille : 0);
    fold_.windowScale = std::clamp(raw, iface_.scaleMinPermille, iface_.scaleMaxPermille);

    // Retire settled instances; they are never touched again.
    std::erase_if(active_, [&](std::uint32_t id) { return insts_[id].settled != 0; });

    stats_.maxActive = std::max<std::uint64_t>(stats_.maxActive, active_.size());
    stats_.maxOpsInTick = std::max(stats_.maxOpsInTick, opsThisTick_);
    const auto second = static_cast<std::size_t>(t / 1'000'000);
    if (stats_.opsPerSecond.size() <= second) {
        stats_.opsPerSecond.resize(second + 1, 0);
    }
    stats_.opsPerSecond[second] += opsThisTick_;
}

void Session::handleTimer(const Timer& timer, Tick t) {
    const std::uint32_t id = timer.ownerId;
    Inst& inst = insts_[id];
    const Requirement& r = reqs_[id];
    count(1);
    if (inst.settled != 0) {
        ++stats_.staleTimers;
        return;
    }
    ++stats_.timersFired;
    switch (timer.edge) {
    case Close:
        if (r.kind == Kind::Tap || (r.kind == Kind::Hold && inst.phase == 0)) {
            emit(t, id, Category::Note, Outcome::Miss, Grade::None, 0, 0);
            inst.settled = 1;
        } else if (r.kind == Kind::Bomb) {
            emit(t, id, Category::Penalty, Outcome::Avoided, Grade::None, 0, 0);
            inst.settled = 1;
        } else if (r.kind == Kind::Roll) {
            const bool ok = inst.count >= r.rollTarget;
            emit(t, id, Category::Note, ok ? Outcome::Hit : Outcome::Miss,
                 ok ? Grade::Perfect : Grade::None, 0, inst.count);
            inst.settled = 1;
        }
        break;
    case Sample: {
        ++stats_.samples;
        const auto second = static_cast<std::size_t>(t / 1'000'000);
        if (stats_.samplesPerSecond.size() <= second) {
            stats_.samplesPerSecond.resize(second + 1, 0);
        }
        ++stats_.samplesPerSecond[second];
        if (laneHeld_[r.lane] != 0) {
            inst.holeStart = -1;
        } else {
            if (inst.holeStart < 0) {
                inst.holeStart = t;
            }
            if (t - inst.holeStart > iface_.holdGraceUs) {
                emit(t, id, Category::Note, Outcome::Broken, Grade::None, 1, 0);
                inst.settled = 1;
                break;
            }
        }
        const Tick next = nextSampleAfter(r, t);
        if (next < r.end) {
            pushTimer({next, id, Sample});
        }
        break;
    }
    case BodyEnd:
        if (inst.phase == 1) {
            emit(t, id, Category::Note, Outcome::Hit, Grade::Perfect, 1, 0);
            inst.settled = 1;
        }
        break;
    default:
        break;
    }
}

void Session::dispatch(const InputEvent& ev, Tick t) {
    // Guards read controller/lane state as it was before this event (defect D5's fix).
    if (ev.press) {
        candidates_.clear();
        // Observe edges first: bombs see the press regardless of who consumes it.
        auto visit = [&](std::uint32_t id) {
            const Requirement& r = reqs_[id];
            count(1);
            if (insts_[id].settled != 0 || r.lane != ev.lane) {
                return;
            }
            if (r.kind == Kind::Bomb) {
                if (t >= r.arm && t <= r.headClose) {
                    emit(t, id, Category::Penalty, Outcome::Detonated, Grade::None, 0, 0);
                    insts_[id].settled = 1;
                }
                return;
            }
            const bool headOpen = (r.kind == Kind::Tap || r.kind == Kind::Hold) &&
                                  insts_[id].phase == 0 && t <= r.headClose;
            const bool rollOpen = r.kind == Kind::Roll && t >= r.anchor && t < r.end;
            if (headOpen || rollOpen) {
                candidates_.push_back(id);
            }
        };
        if (reverse_) {
            for (auto it = active_.rbegin(); it != active_.rend(); ++it) {
                visit(*it);
            }
        } else {
            for (const std::uint32_t id : active_) {
                visit(id);
            }
        }
        // Arbitration: claimKey = |err| for heads, rolls rank after heads; tie by id; fanout 1.
        auto key = [&](std::uint32_t id) -> std::pair<Tick, std::uint32_t> {
            const Requirement& r = reqs_[id];
            const Tick k = r.kind == Kind::Roll ? INT64_MAX / 2 : std::llabs(t - r.anchor);
            return {k, id};
        };
        std::uint32_t chosen = kCore;
        for (const std::uint32_t id : candidates_) {
            count(1);
            const Requirement& r = reqs_[id];
            if (r.kind != Kind::Roll && gradeOf(std::llabs(t - r.anchor)) == Grade::None) {
                continue; // inside the arm span but outside the live window
            }
            if (chosen == kCore || key(id) < key(chosen)) {
                chosen = id;
            }
        }
        if (chosen == kCore) {
            emit(t, 0, Category::Penalty, Outcome::Stray, Grade::None, 0, 0);
        } else {
            const Requirement& r = reqs_[chosen];
            Inst& inst = insts_[chosen];
            if (r.kind == Kind::Roll) {
                ++inst.count;
            } else {
                const Tick err = t - r.anchor;
                const Grade g = gradeOf(std::llabs(err));
                emit(t, chosen, Category::Note, Outcome::Hit, g, 0, static_cast<std::int32_t>(err));
                if (r.kind == Kind::Tap) {
                    inst.settled = 1;
                } else {
                    inst.phase = 1;
                    const Tick next = nextSampleAfter(r, t);
                    if (next < r.end) {
                        pushTimer({next, chosen, Sample});
                    }
                }
            }
        }
    }
    laneHeld_[ev.lane] = ev.press ? 1 : 0;
}

void Session::fold(const Fact& fact) {
    ++stats_.facts;
    ++fold_.factCount;
    std::uint64_t h = fold_.factHash;
    auto mix = [&h](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (i * 8)) & 0xFF;
            h *= 0x100000001b3ull;
        }
    };
    mix(static_cast<std::uint64_t>(fact.t));
    mix(fact.req);
    mix(static_cast<std::uint64_t>(fact.cat) << 16 | static_cast<std::uint64_t>(fact.outcome) << 8 |
        static_cast<std::uint64_t>(fact.grade));
    mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(fact.err)));
    fold_.factHash = h;

    ++fold_.counts[static_cast<int>(fact.cat)][static_cast<int>(fact.outcome)];
    switch (fact.outcome) {
    case Outcome::Hit: {
        static constexpr std::int64_t kPoints[] = {1000, 700, 300, 0};
        fold_.score += kPoints[static_cast<int>(fact.grade)];
        ++fold_.combo;
        fold_.maxCombo = std::max(fold_.maxCombo, fold_.combo);
        if (loadout_.boostModule && fold_.combo % loadout_.boostComboStep == 0) {
            fold_.boostActive = 1;
            fold_.boostExpiry = fact.t + loadout_.boostDurationUs;
            pushTimer({fold_.boostExpiry, kCore, Close});
        }
        break;
    }
    case Outcome::Miss:
    case Outcome::Broken:
        fold_.combo = 0;
        fold_.life -= 50;
        break;
    case Outcome::Detonated:
        fold_.combo = 0;
        fold_.life -= 100;
        break;
    default:
        break;
    }
    if (fold_.life <= 0) {
        fold_.failed = 1;
    }
}

Summary Session::summary() const {
    Summary s;
    s.score = fold_.score;
    s.maxCombo = fold_.maxCombo;
    s.life = fold_.life;
    s.failed = fold_.failed;
    std::memcpy(s.counts, fold_.counts, sizeof(s.counts));
    return s;
}

std::vector<std::uint8_t> Session::snapshot() const {
    std::vector<std::uint8_t> out;
    Writer w(out);
    w.put(processedThrough_);
    w.put(static_cast<std::uint64_t>(nextArm_));
    w.put(static_cast<std::uint64_t>(inputCursor_));
    for (const std::uint8_t held : laneHeld_) {
        w.put(held);
    }
    // Only live instances carry state: unarmed ones are default, retired ones are never read.
    w.put(static_cast<std::uint32_t>(active_.size()));
    for (const std::uint32_t id : active_) {
        const Inst& inst = insts_[id];
        w.put(id);
        w.put(inst.phase);
        w.put(inst.settled);
        w.put(inst.count);
        w.put(inst.holeStart);
    }
    // Pending timers, including every(period) sampling phase (budget draft 4.2).
    std::vector<Timer> sorted = timers_;
    std::sort(sorted.begin(), sorted.end(),
              [](const Timer& a, const Timer& b) { return timerLess(a, b); });
    w.put(static_cast<std::uint32_t>(sorted.size()));
    for (const Timer& timer : sorted) {
        w.put(timer.t);
        w.put(timer.ownerId);
        w.put(timer.edge);
    }
    w.put(fold_.score);
    w.put(fold_.combo);
    w.put(fold_.maxCombo);
    w.put(fold_.life);
    w.put(fold_.failed);
    for (const auto& row : fold_.counts) {
        for (const std::uint32_t c : row) {
            w.put(c);
        }
    }
    w.put(fold_.boostActive);
    w.put(fold_.boostExpiry);
    w.put(fold_.windowScale);
    w.put(fold_.factHash);
    w.put(fold_.factCount);
    return out;
}

void Session::restore(const std::vector<std::uint8_t>& bytes) {
    Reader r(bytes);
    processedThrough_ = r.get<Tick>();
    nextArm_ = static_cast<std::size_t>(r.get<std::uint64_t>());
    inputCursor_ = static_cast<std::size_t>(r.get<std::uint64_t>());
    for (std::uint8_t& held : laneHeld_) {
        held = r.get<std::uint8_t>();
    }
    std::fill(insts_.begin(), insts_.end(), Inst{});
    active_.clear();
    const auto activeCount = r.get<std::uint32_t>();
    for (std::uint32_t i = 0; i < activeCount; ++i) {
        const auto id = r.get<std::uint32_t>();
        Inst& inst = insts_[id];
        inst.phase = r.get<std::uint8_t>();
        inst.settled = r.get<std::uint8_t>();
        inst.count = r.get<std::uint16_t>();
        inst.holeStart = r.get<Tick>();
        active_.push_back(id);
    }
    // Instances armed before the snapshot but already retired must stay settled.
    for (std::size_t i = 0; i < nextArm_; ++i) {
        const std::uint32_t id = armOrder_[i];
        if (std::find(active_.begin(), active_.end(), id) == active_.end()) {
            insts_[id].settled = 1;
        }
    }
    timers_.clear();
    const auto timerCount = r.get<std::uint32_t>();
    for (std::uint32_t i = 0; i < timerCount; ++i) {
        Timer timer;
        timer.t = r.get<Tick>();
        timer.ownerId = r.get<std::uint32_t>();
        timer.edge = r.get<std::uint8_t>();
        timers_.push_back(timer);
    }
    std::make_heap(timers_.begin(), timers_.end(), TimerAfter{});
    fold_.score = r.get<std::int64_t>();
    fold_.combo = r.get<std::uint32_t>();
    fold_.maxCombo = r.get<std::uint32_t>();
    fold_.life = r.get<std::int32_t>();
    fold_.failed = r.get<std::uint32_t>();
    for (auto& row : fold_.counts) {
        for (std::uint32_t& c : row) {
            c = r.get<std::uint32_t>();
        }
    }
    fold_.boostActive = r.get<std::uint32_t>();
    fold_.boostExpiry = r.get<Tick>();
    fold_.windowScale = r.get<std::int32_t>();
    fold_.factHash = r.get<std::uint64_t>();
    fold_.factCount = r.get<std::uint64_t>();
}

std::uint64_t Session::digest() const {
    const std::vector<std::uint8_t> bytes = snapshot();
    std::uint64_t h = 0xcbf29ce484222325ull;
    // processedThrough_ depends on how far the caller asked to run, not on gameplay; skip it.
    for (std::size_t i = sizeof(Tick); i < bytes.size(); ++i) {
        h ^= bytes[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

} // namespace spike::rt
