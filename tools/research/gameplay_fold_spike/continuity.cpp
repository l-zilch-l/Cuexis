#include "continuity.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

namespace spike::continuity {

namespace {

struct State {
    Stage stage = Stage::Pending;
    Tick releasedAt = 0;
    std::uint32_t contact = 0;
    std::uint32_t segment = 0;
    std::uint32_t moves = 0;
    bool headEmitted = false;
    bool breakEmitted = false;
};

const char* outcomeName(Outcome outcome) {
    switch (outcome) {
    case Outcome::HeadHit:
        return "HeadHit";
    case Outcome::BodyHit:
        return "BodyHit";
    case Outcome::Break:
        return "Break";
    case Outcome::Miss:
        return "Miss";
    }
    return "?";
}

const char* stageName(Stage stage) {
    switch (stage) {
    case Stage::Pending:
        return "Pending";
    case Stage::Follow:
        return "Follow";
    case Stage::Gap:
        return "Gap";
    case Stage::Broken:
        return "Broken";
    case Stage::Settled:
        return "Settled";
    }
    return "?";
}

template <class T> void append(std::vector<std::uint8_t>& out, const T& value) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), p, p + sizeof(T));
}

template <class T> T read(const std::vector<std::uint8_t>& in, std::size_t& offset) {
    assert(offset + sizeof(T) <= in.size());
    T value{};
    std::memcpy(&value, in.data() + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

} // namespace

struct Session::Impl {
    Config config;
    std::vector<Requirement> requirements;
    std::vector<State> states;
    std::vector<Fact> facts;
    std::size_t strays = 0;
    bool finished = false;

    Impl(const Config& cfg, const std::vector<Requirement>& reqs)
        : config(cfg), requirements(reqs), states(reqs.size()) {}

    void emit(Tick t, std::uint32_t req, Outcome outcome, std::uint8_t phase) {
        facts.push_back(Fact{t, req, outcome, phase});
    }

    void breakItem(std::size_t i, Tick t) {
        State& state = states[i];
        if (state.breakEmitted || state.stage == Stage::Settled) {
            return;
        }
        state.stage = Stage::Broken;
        state.contact = 0;
        state.breakEmitted = true;
        emit(t, requirements[i].id, Outcome::Break, 1);
    }

    void advance(Tick t) {
        for (std::size_t i = 0; i < states.size(); ++i) {
            State& state = states[i];
            if (state.stage == Stage::Gap && t >= state.releasedAt + config.holdGraceUs) {
                breakItem(i, state.releasedAt + config.holdGraceUs);
            }
        }
    }

    struct Candidate {
        std::size_t index = 0;
        Tick distance = 0;
    };

    void press(const Event& event) {
        std::vector<Candidate> candidates;
        for (std::size_t i = 0; i < requirements.size(); ++i) {
            const Requirement& req = requirements[i];
            const State& state = states[i];
            if (req.lane != event.lane || state.stage == Stage::Broken ||
                state.stage == Stage::Settled) {
                continue;
            }
            if (state.stage == Stage::Pending) {
                if (event.t < req.anchor - config.goodUs || event.t > req.anchor + config.goodUs) {
                    continue;
                }
                candidates.push_back({i, std::llabs(event.t - req.anchor)});
            } else if (state.stage == Stage::Gap && req.grip == Grip::Handoff &&
                       event.t < state.releasedAt + config.holdGraceUs) {
                // Gap recovery is a normal consume candidate. A nearby new head may win it.
                candidates.push_back({i, std::llabs(event.t - req.anchor)});
            }
        }
        if (candidates.empty()) {
            ++strays;
            return;
        }
        std::stable_sort(candidates.begin(), candidates.end(), [&](const Candidate& a,
                                                                    const Candidate& b) {
            if (a.distance != b.distance) {
                return a.distance < b.distance;
            }
            return requirements[a.index].id < requirements[b.index].id;
        });
        State& state = states[candidates.front().index];
        const Requirement& req = requirements[candidates.front().index];
        state.stage = Stage::Follow;
        state.contact = event.contact;
        if (!state.headEmitted) {
            state.headEmitted = true;
            emit(event.t, req.id, Outcome::HeadHit, 0);
        }
    }

    void release(const Event& event) {
        for (std::size_t i = 0; i < states.size(); ++i) {
            State& state = states[i];
            const Requirement& req = requirements[i];
            if (req.lane != event.lane || state.stage != Stage::Follow ||
                state.contact != event.contact) {
                continue;
            }
            state.contact = 0;
            // Both grips retain the existing Gap timer. Only handoff admits a new contact;
            // sticky therefore reaches the same timeout but cannot recover from the gap.
            state.stage = Stage::Gap;
            state.releasedAt = event.t;
        }
    }

    void move(const Event& event) {
        for (std::size_t i = 0; i < states.size(); ++i) {
            State& state = states[i];
            const Requirement& req = requirements[i];
            if (req.kind != Kind::Slider || req.lane != event.lane ||
                state.stage != Stage::Follow || state.contact != event.contact) {
                continue;
            }
            state.segment = std::max(state.segment, std::min(event.segment, req.segments));
            ++state.moves;
        }
    }

    void apply(const Event& event) {
        if (finished) {
            return;
        }
        advance(event.t);
        switch (event.kind) {
        case EventKind::Press:
            press(event);
            break;
        case EventKind::Release:
            release(event);
            break;
        case EventKind::Move:
            move(event);
            break;
        }
    }

    void finish() {
        if (finished) {
            return;
        }
        for (std::size_t i = 0; i < states.size(); ++i) {
            State& state = states[i];
            const Requirement& req = requirements[i];
            if (state.stage == Stage::Gap) {
                if (req.end >= state.releasedAt + config.holdGraceUs) {
                    breakItem(i, state.releasedAt + config.holdGraceUs);
                } else {
                    emit(req.end, req.id, Outcome::BodyHit, 1);
                    state.stage = Stage::Settled;
                }
            } else if (state.stage == Stage::Follow) {
                emit(req.end, req.id, Outcome::BodyHit, 1);
                state.stage = Stage::Settled;
                state.contact = 0;
            } else if (state.stage == Stage::Pending) {
                emit(req.anchor, req.id, Outcome::Miss, 0);
                state.stage = Stage::Settled;
            }
        }
        std::stable_sort(facts.begin(), facts.end(), [](const Fact& a, const Fact& b) {
            if (a.t != b.t) {
                return a.t < b.t;
            }
            if (a.req != b.req) {
                return a.req < b.req;
            }
            if (a.phase != b.phase) {
                return a.phase < b.phase;
            }
            return a.outcome < b.outcome;
        });
        finished = true;
    }

    Result result() const {
        Result out;
        out.facts = facts;
        out.strays = strays;
        out.items.reserve(states.size());
        for (std::size_t i = 0; i < states.size(); ++i) {
            const State& state = states[i];
            out.items.push_back(ItemSummary{requirements[i].id, state.stage, state.releasedAt,
                                            state.contact, state.segment, state.moves,
                                            state.headEmitted, state.breakEmitted});
        }
        std::uint64_t hash = 1469598103934665603ull;
        auto mix = [&](std::uint64_t value) {
            hash ^= value;
            hash *= 1099511628211ull;
        };
        for (const Fact& fact : out.facts) {
            mix(static_cast<std::uint64_t>(fact.t));
            mix(fact.req);
            mix(static_cast<std::uint8_t>(fact.outcome));
            mix(fact.phase);
        }
        for (const ItemSummary& item : out.items) {
            mix(item.req);
            mix(static_cast<std::uint8_t>(item.stage));
            mix(static_cast<std::uint64_t>(item.releasedAt));
            mix(item.segment);
            mix(item.moves);
            mix(item.headEmitted);
            mix(item.breakEmitted);
        }
        mix(out.strays);
        out.digest = hash;

        std::uint64_t judgement = 1469598103934665603ull;
        auto mixJudgement = [&](std::uint64_t value) {
            judgement ^= value;
            judgement *= 1099511628211ull;
        };
        for (const Fact& fact : out.facts) {
            mixJudgement(static_cast<std::uint64_t>(fact.t));
            mixJudgement(fact.req);
            mixJudgement(static_cast<std::uint8_t>(fact.outcome));
            mixJudgement(fact.phase);
        }
        mixJudgement(out.strays);
        out.judgementDigest = judgement;
        return out;
    }

    std::vector<std::uint8_t> snapshot() const {
        std::vector<std::uint8_t> bytes;
        const std::uint32_t magic = 0x43475231u; // CGR1
        append(bytes, magic);
        append(bytes, finished);
        const std::uint64_t strayCount = static_cast<std::uint64_t>(strays);
        append(bytes, strayCount);
        const std::uint32_t stateCount = static_cast<std::uint32_t>(states.size());
        append(bytes, stateCount);
        for (const State& state : states) {
            append(bytes, state.stage);
            append(bytes, state.releasedAt);
            append(bytes, state.contact);
            append(bytes, state.segment);
            append(bytes, state.moves);
            append(bytes, state.headEmitted);
            append(bytes, state.breakEmitted);
        }
        const std::uint32_t factCount = static_cast<std::uint32_t>(facts.size());
        append(bytes, factCount);
        for (const Fact& fact : facts) {
            append(bytes, fact.t);
            append(bytes, fact.req);
            append(bytes, fact.outcome);
            append(bytes, fact.phase);
        }
        return bytes;
    }

    void restore(const std::vector<std::uint8_t>& bytes) {
        std::size_t offset = 0;
        const std::uint32_t magic = read<std::uint32_t>(bytes, offset);
        assert(magic == 0x43475231u);
        finished = read<bool>(bytes, offset);
        strays = static_cast<std::size_t>(read<std::uint64_t>(bytes, offset));
        const std::uint32_t stateCount = read<std::uint32_t>(bytes, offset);
        assert(stateCount == states.size());
        for (State& state : states) {
            state.stage = read<Stage>(bytes, offset);
            state.releasedAt = read<Tick>(bytes, offset);
            state.contact = read<std::uint32_t>(bytes, offset);
            state.segment = read<std::uint32_t>(bytes, offset);
            state.moves = read<std::uint32_t>(bytes, offset);
            state.headEmitted = read<bool>(bytes, offset);
            state.breakEmitted = read<bool>(bytes, offset);
        }
        const std::uint32_t factCount = read<std::uint32_t>(bytes, offset);
        facts.clear();
        facts.reserve(factCount);
        for (std::uint32_t i = 0; i < factCount; ++i) {
            facts.push_back(Fact{read<Tick>(bytes, offset), read<std::uint32_t>(bytes, offset),
                                  read<Outcome>(bytes, offset), read<std::uint8_t>(bytes, offset)});
        }
        assert(offset == bytes.size());
    }
};

Session::Session(const Config& config, const std::vector<Requirement>& requirements)
    : impl_(new Impl(config, requirements)) {}
Session::~Session() { delete impl_; }
Session::Session(Session&& other) noexcept : impl_(std::exchange(other.impl_, nullptr)) {}
Session& Session::operator=(Session&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
}
void Session::apply(const Event& event) { impl_->apply(event); }
void Session::finish() { impl_->finish(); }
std::vector<std::uint8_t> Session::snapshot() const { return impl_->snapshot(); }
void Session::restore(const std::vector<std::uint8_t>& bytes) { impl_->restore(bytes); }
Result Session::result() const { return impl_->result(); }

Result run(const Config& config, const std::vector<Requirement>& requirements,
           const std::vector<Event>& events) {
    Session session(config, requirements);
    std::vector<Event> ordered = events;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Event& a, const Event& b) { return a.t < b.t; });
    for (const Event& event : ordered) {
        session.apply(event);
    }
    session.finish();
    return session.result();
}

std::string describe(const Result& result) {
    std::string text;
    for (const Fact& fact : result.facts) {
        text += outcomeName(fact.outcome);
        text += "(req" + std::to_string(fact.req);
        if (fact.phase == 1) {
            text += ",body";
        }
        text += ") ";
    }
    text += "strays=" + std::to_string(result.strays);
    for (const ItemSummary& item : result.items) {
        text += " state(req" + std::to_string(item.req) + ")=" + stageName(item.stage);
        text += " seg=" + std::to_string(item.segment);
    }
    return text;
}

} // namespace spike::continuity
