#include "contacts.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace spike::mt {

namespace {

constexpr std::uint32_t kNone = 0xFFFFFFFFu;
constexpr int kSlots = 32;

struct Inst {
    std::uint8_t phase = 0; // 0 head pending, 1 body
    bool settled = false;
    std::uint16_t count = 0;
    std::uint32_t contact = kNone; // claimed contact
    int slot = -1;
    std::int64_t holeStart = 0;
};

struct Contact {
    std::uint32_t id = 0;
    bool live = false;
    Tick endedAt = 0;
};

// Slot ownership survives contact termination until the owner releases it or the slot is reused,
// which is what "handles are never reclaimed" means operationally.
struct SlotOwner {
    std::uint32_t contact = kNone; // kNone when free
    std::uint32_t owner = kNone;   // requirement holding it
    bool terminated = false;       // the owning contact has ended
};

class World {
  public:
    World(const Config& cfg, const std::vector<Req>& reqs)
        : cfg_(cfg), reqs_(reqs), insts_(reqs.size()) {}

    Result run(const std::vector<ContactEvent>& events) {
        std::vector<ContactEvent> ordered = events;
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const ContactEvent& a, const ContactEvent& b) { return a.t < b.t; });
        auto emit = [&](Tick t, std::uint32_t req, Outcome outcome, std::uint8_t phase,
                        bool observed) {
            facts_.push_back(Fact{t, req, outcome, phase, observed});
        };

        for (const ContactEvent& ev : ordered) {
            if (ev.begin) {
                begin(ev, emit);
            } else {
                end(ev);
            }
        }
        // Settle leftovers: holds whose owner stopped being touched, rolls out of count, taps
        // already settled. Uses the scenario's own end rather than a timeline.
        for (std::size_t i = 0; i < reqs_.size(); ++i) {
            const Req& r = reqs_[i];
            Inst& inst = insts_[i];
            if (inst.settled) {
                continue;
            }
            if (r.kind == Kind::Hold && inst.phase == 1) {
                // The hole starts when the owner stops touching and runs to the body's end, since
                // nothing may re-touch it (sticky) unless handoff took over.
                const Tick hole = contactLive(inst.contact) ? 0 : r.end - inst.holeStart;
                if (hole > cfg_.holdGraceUs) {
                    emit(inst.holeStart + cfg_.holdGraceUs + 1, r.id, Outcome::Broken, 1, false);
                } else {
                    emit(r.end, r.id, Outcome::Hit, 1, false);
                }
                inst.settled = true;
                release(inst.slot, r.id);
            } else if (r.kind == Kind::Roll) {
                const bool ok = inst.count >= r.rollTarget;
                emit(r.end, r.id, ok ? Outcome::Hit : Outcome::Miss, 0, false);
                inst.settled = true;
            } else if (r.kind == Kind::Tap || r.kind == Kind::Bomb) {
                emit(r.anchor, r.id, r.kind == Kind::Bomb ? Outcome::Avoided : Outcome::Miss, 0,
                     false);
                inst.settled = true;
            }
        }

        // The ruleset fold language folds same-tick facts in a canonical order.
        std::stable_sort(facts_.begin(), facts_.end(), [](const Fact& a, const Fact& b) {
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
        Result result;
        result.facts = facts_;
        result.strays = strays_;
        result.trace = trace_;
        return result;
    }

  private:
    bool contactLive(std::uint32_t contact) const {
        for (const Contact& c : contacts_) {
            if (c.id == contact) {
                return c.live;
            }
        }
        return false;
    }

    Tick contactEndedAt(std::uint32_t contact) const {
        for (const Contact& c : contacts_) {
            if (c.id == contact) {
                return c.endedAt;
            }
        }
        return -1;
    }

    Contact& contact(std::uint32_t id) {
        for (Contact& c : contacts_) {
            if (c.id == id) {
                return c;
            }
        }
        contacts_.push_back(Contact{id, false, 0});
        return contacts_.back();
    }

    bool slotUsable(int slot, std::uint32_t contactId, std::uint32_t owner) const {
        const SlotOwner& o = slots_[static_cast<std::size_t>(slot)];
        if (o.contact == kNone) {
            return true;
        }
        if (o.owner == owner && o.contact == contactId) {
            return true; // re-claiming your own slot with the same contact is idempotent
        }
        return o.terminated; // a terminated holder is replaced implicitly
    }

    void claim(int slot, std::uint32_t contactId, std::uint32_t owner) {
        slots_[static_cast<std::size_t>(slot)] = SlotOwner{contactId, owner, false};
    }

    void release(int slot, std::uint32_t owner) {
        if (slot < 0) {
            return;
        }
        SlotOwner& o = slots_[static_cast<std::size_t>(slot)];
        if (o.owner == owner) {
            o = SlotOwner{};
        }
    }

    void begin(const ContactEvent& ev, const auto& emit) {
        Contact& c = contact(ev.contactId);
        c.live = true;
        candidates_.clear();
        participated_ = false;

        // Observe edges see every contact, claimed or not, and never own it.
        for (std::size_t i = 0; i < reqs_.size(); ++i) {
            const Req& r = reqs_[i];
            if (r.kind != Kind::Bomb || insts_[i].settled || r.lane != ev.lane) {
                continue;
            }
            participated_ = true;
            if (ev.t >= r.anchor - cfg_.goodUs && ev.t <= r.anchor + cfg_.goodUs) {
                emit(ev.t, r.id, Outcome::Detonated, 0, true);
                insts_[i].settled = true;
            }
        }

        // Claim edges compete, in canonical instance order.
        auto consider = [&](std::size_t i) {
            const Req& r = reqs_[i];
            Inst& inst = insts_[i];
            if (inst.settled || r.kind == Kind::Bomb || r.lane != ev.lane) {
                return;
            }
            participated_ = true;
            if (r.kind == Kind::Roll) {
                if (ev.t < r.anchor || ev.t >= r.end) {
                    return;
                }
                const int slot = static_cast<int>(r.id) % kSlots;
                if (!slotUsable(slot, ev.contactId, r.id)) {
                    return;
                }
                candidates_.push_back(static_cast<std::uint32_t>(i));
                return;
            }
            if (inst.phase != 0) {
                // Body in flight. A sticky owner keeps it; a handoff owner yields within grace.
                if (inst.contact == ev.contactId) {
                    return;
                }
                if (r.grip != Grip::Handoff || contactLive(inst.contact) ||
                    ev.t - inst.holeStart > cfg_.holdGraceUs) {
                    return;
                }
                candidates_.push_back(
                    static_cast<std::uint32_t>(i)); // takeover, see the loop below
                return;
            }
            if (ev.t > r.anchor + cfg_.goodUs || ev.t < r.anchor - cfg_.goodUs) {
                return;
            }
            const int slot = static_cast<int>(r.id) % kSlots;
            if (!slotUsable(slot, ev.contactId, r.id)) {
                return;
            }
            candidates_.push_back(static_cast<std::uint32_t>(i));
        };
        for (std::size_t k = 0; k < reqs_.size(); ++k) {
            consider(k);
        }
        // Candidate order is canonical by construction; enumeration order cannot change it.

        std::sort(candidates_.begin(), candidates_.end(), [&](std::uint32_t a, std::uint32_t b) {
            if (cfg_.fanoutAllOnEqualAnchor) {
                const Tick ea = std::llabs(ev.t - reqs_[a].anchor);
                const Tick eb = std::llabs(ev.t - reqs_[b].anchor);
                const bool sameTick = reqs_[a].anchor == reqs_[b].anchor;
                (void)ea;
                (void)eb;
                if (sameTick) {
                    return a < b;
                }
            }
            const Tick ka =
                reqs_[a].kind == Kind::Roll ? (1 << 30) : std::llabs(ev.t - reqs_[a].anchor);
            const Tick kb =
                reqs_[b].kind == Kind::Roll ? (1 << 30) : std::llabs(ev.t - reqs_[b].anchor);
            if (ka != kb) {
                return ka < kb;
            }
            return a < b;
        });

        std::size_t taken = 0;
        for (const std::uint32_t idx : candidates_) {
            if (taken > 0 && !(cfg_.fanoutAllOnEqualAnchor &&
                               reqs_[idx].anchor == reqs_[candidates_.front()].anchor)) {
                break;
            }
            const Req& r = reqs_[idx];
            Inst& inst = insts_[idx];
            if (inst.phase == 1) {
                // Handoff takeover: the new contact continues the same body.
                release(inst.slot, r.id);
                inst.contact = ev.contactId;
                claim(inst.slot, ev.contactId, r.id);
                inst.holeStart = 0;
                ++taken;
                continue;
            }
            const int slot = static_cast<int>(r.id) % kSlots;
            if (!slotUsable(slot, ev.contactId, r.id)) {
                continue;
            }
            inst.contact = ev.contactId;
            inst.slot = slot;
            claim(slot, ev.contactId, r.id);
            if (r.kind == Kind::Roll) {
                ++inst.count;
            } else {
                emit(ev.t, r.id, Outcome::Hit, 0, false);
                if (r.kind == Kind::Tap) {
                    inst.settled = true;
                } else {
                    inst.phase = 1;
                    inst.holeStart = 0; // 0 = the owner is touching it
                }
            }
            ++taken;
        }
        if (taken == 0) {
            const bool consumeEmpty = candidates_.empty();
            const bool stray =
                cfg_.strayWhen == StrayRule::ConsumeEmpty ? consumeEmpty : !participated_;
            if (stray) {
                ++strays_;
            }
        }
    }

    void end(const ContactEvent& ev) {
        Contact& c = contact(ev.contactId);
        c.live = false;
        c.endedAt = ev.t;
        for (SlotOwner& o : slots_) {
            if (o.contact == ev.contactId) {
                o.terminated = true;
            }
        }
        // Holds whose owner stopped touching: remember when the hole started.
        for (std::size_t i = 0; i < reqs_.size(); ++i) {
            Inst& inst = insts_[i];
            if (!inst.settled && inst.contact == ev.contactId && inst.phase == 1) {
                inst.holeStart = ev.t;
            }
        }
    }

    const Config& cfg_;
    const std::vector<Req>& reqs_;
    std::vector<Inst> insts_;
    std::vector<Contact> contacts_;
    std::array<SlotOwner, kSlots> slots_{};
    std::vector<std::uint32_t> candidates_;
    std::vector<Fact> facts_;
    std::size_t strays_ = 0;
    std::string trace_;
    bool participated_ = false;
};

} // namespace

std::string describe(const Result& r) {
    std::string s;
    for (const Fact& f : r.facts) {
        static const char* kNames[] = {"Hit", "Miss", "Broken", "Avoided", "Detonated", "Stray"};
        s += kNames[static_cast<int>(f.outcome)];
        s += "(req";
        s += std::to_string(f.req);
        if (f.observed) {
            s += ",observe";
        }
        if (f.phase == 1) {
            s += ",body"; // the body component of a multi-component Measure
        }
        s += ") ";
    }
    if (r.strays != 0) {
        s += "strays=" + std::to_string(r.strays);
    }
    return s;
}

Result run(const Config& cfg, const std::vector<Req>& reqs,
           const std::vector<ContactEvent>& events) {
    World w(cfg, reqs);
    return w.run(events);
}

} // namespace spike::mt
