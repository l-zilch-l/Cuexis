#include "continuity.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <string>
#include <vector>

namespace {

using namespace spike::continuity;

constexpr Tick kAnchor = 1'000'000;
constexpr Tick kEnd = 2'000'000;
constexpr Tick kGrace = 60'000;

struct Case {
    const char* name;
    const char* expectation;
    Config config;
    std::vector<Requirement> requirements;
    std::vector<Event> events;
    bool (*check)(const Result&, const Result&);
};

int failures = 0;

bool sameJudgement(const Result& a, const Result& b) {
    return a.judgementDigest == b.judgementDigest;
}

int countOutcome(const Result& result, Outcome outcome) {
    return static_cast<int>(std::count_if(result.facts.begin(), result.facts.end(),
                                           [&](const Fact& fact) { return fact.outcome == outcome; }));
}

const char* stageLabel(Stage stage) {
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

const ItemSummary& item(const Result& result, std::size_t index = 0) {
    return result.items[index];
}

void printCase(const Case& test, const Result& actual, const Result& baseline) {
    const bool ok = test.check(actual, baseline);
    if (!ok) {
        ++failures;
    }
    std::printf("| %s | %s | `%s` | %s |\n", test.name, test.expectation,
                describe(actual).c_str(), ok ? "as predicted" : "UNEXPECTED");
    if (!ok) {
        std::printf("\nFAIL %s: judgement=%016llx baseline=%016llx\n", test.name,
                    static_cast<unsigned long long>(actual.judgementDigest),
                    static_cast<unsigned long long>(baseline.judgementDigest));
    }
}

Requirement hold(std::uint32_t id = 0, Grip grip = Grip::Handoff) {
    return Requirement{id, Kind::Hold, grip, 0, kAnchor, kEnd, 1};
}

Requirement slider(std::uint32_t id = 0, Grip grip = Grip::Handoff) {
    return Requirement{id, Kind::Slider, grip, 0, kAnchor, kEnd, 4};
}

Requirement withGrace(Requirement req, Tick grace) {
    req.graceUs = grace;
    req.explicitGrace = true;
    return req;
}

Event press(Tick t, std::uint32_t contact, std::uint8_t lane = 0) {
    return Event{t, contact, lane, EventKind::Press, 0};
}

Event release(Tick t, std::uint32_t contact, std::uint8_t lane = 0) {
    return Event{t, contact, lane, EventKind::Release, 0};
}

Event move(Tick t, std::uint32_t contact, std::uint32_t segment) {
    return Event{t, contact, 0, EventKind::Move, segment};
}

Result runEvents(const Config& config, const std::vector<Requirement>& requirements,
                 const std::vector<Event>& events) {
    return run(config, requirements, events);
}

bool baselineMatch(const Result& actual, const Result& baseline) {
    return sameJudgement(actual, baseline) && actual.strays == baseline.strays;
}

bool oneBreak(const Result& actual, const Result&) {
    return countOutcome(actual, Outcome::Break) == 1 &&
           countOutcome(actual, Outcome::HeadHit) == 1 &&
           countOutcome(actual, Outcome::BodyHit) == 0;
}

bool exactBoundary(const Result& actual, const Result&) {
    return oneBreak(actual, {}) && actual.strays == 1;
}

bool sliderProgress(const Result& actual, const Result& baseline) {
    return sameJudgement(actual, baseline) && actual.strays == 0 &&
           countOutcome(actual, Outcome::HeadHit) == 1 && countOutcome(actual, Outcome::BodyHit) == 1 &&
           item(actual).segment == 4 && item(actual).moves == 4;
}

bool competition(const Result& actual, const Result&) {
    return countOutcome(actual, Outcome::HeadHit) == 2 && countOutcome(actual, Outcome::Break) == 1 &&
           countOutcome(actual, Outcome::BodyHit) == 1 && actual.strays == 0;
}

bool stickyRejects(const Result& actual, const Result&) {
    return countOutcome(actual, Outcome::HeadHit) == 1 && countOutcome(actual, Outcome::Break) == 1 &&
           actual.strays == 1;
}

std::vector<Case> directedCases() {
    Config config;
    config.holdGraceUs = kGrace;
    const std::vector<Event> baselineEvents = {press(kAnchor, 1), release(kEnd + 10'000, 1)};
    std::vector<Case> out;
    out.push_back({"C1 Hold baseline", "head and body are Hit", config, {hold()}, baselineEvents,
                   [](const Result& actual, const Result&) {
                       return countOutcome(actual, Outcome::HeadHit) == 1 &&
                              countOutcome(actual, Outcome::BodyHit) == 1 &&
                              countOutcome(actual, Outcome::Break) == 0 && actual.strays == 0;
                   }});
    out.push_back({"C2 Hold one gap inside grace", "judgement equals continuous hold", config, {hold()},
                   {press(kAnchor, 1), release(1'400'000, 1), press(1'450'000, 2),
                    release(kEnd + 10'000, 2)},
                   baselineMatch});
    out.push_back({"C3 Hold repeated gaps", "multiple recoveries remain continuous", config, {hold()},
                   {press(kAnchor, 1), release(1'300'000, 1), press(1'350'000, 2),
                    release(1'500'000, 2), press(1'550'000, 3), release(kEnd + 10'000, 3)},
                   baselineMatch});
    out.push_back({"C4 Hold exact grace boundary", "strict < rejects boundary recovery", config, {hold()},
                   {press(kAnchor, 1), release(1'400'000, 1), press(1'460'000, 2)}, exactBoundary});
    out.push_back({"C5 Hold late recovery", "timeout emits one Break", config, {hold()},
                   {press(kAnchor, 1), release(1'400'000, 1), press(1'461'000, 2),
                    press(1'700'000, 3)},
                   oneBreak});

    const std::vector<Event> sliderBaseline = {press(kAnchor, 1), move(1'200'000, 1, 1),
                                                move(1'400'000, 1, 2), move(1'600'000, 1, 3),
                                                move(1'800'000, 1, 4), release(kEnd + 10'000, 1)};
    out.push_back({"C6 Slider gap preserves segment", "no head duplicate or progress rollback", config,
                   {slider()},
                   {press(kAnchor, 1), move(1'200'000, 1, 1), move(1'400'000, 1, 2),
                    release(1'500'000, 1), press(1'550'000, 2), move(1'700'000, 2, 3),
                    move(1'800'000, 2, 4), release(kEnd + 10'000, 2)},
                   sliderProgress});

    Requirement competing = hold(1);
    competing.anchor = 1'450'000;
    competing.end = 2'000'000;
    out.push_back({"C7 Gap competes with a new head", "recovery uses normal consume arbitration", config,
                   {hold(0), competing},
                   {press(kAnchor, 1), release(1'400'000, 1), press(1'450'000, 2),
                    release(kEnd + 10'000, 2)},
                   competition});
    out.push_back({"C8 Sticky does not recover", "second contact is stray and body breaks", config,
                   {hold(0, Grip::Sticky)},
                   {press(kAnchor, 1), release(1'400'000, 1), press(1'450'000, 2)}, stickyRejects});
    out.push_back({"C9 snapshot in Gap", "restore preserves releasedAt and continuation", config, {slider()},
                   sliderBaseline, baselineMatch});
    return out;
}

void reportDirected() {
    Config config;
    config.holdGraceUs = kGrace;
    const Result baseline = runEvents(config, {hold()}, {press(kAnchor, 1), release(kEnd + 10'000, 1)});
    std::printf("| case | expected invariant | result | status |\n| --- | --- | --- | --- |\n");
    for (const Case& test : directedCases()) {
        const Result actual = runEvents(test.config, test.requirements, test.events);
        Result reference = baseline;
        if (std::string(test.name) == "C6 Slider gap preserves segment") {
            reference = runEvents(test.config, test.requirements, {press(kAnchor, 1), move(1'200'000, 1, 1),
                                                                  move(1'400'000, 1, 2), move(1'600'000, 1, 3),
                                                                  move(1'800'000, 1, 4), release(kEnd + 10'000, 1)});
        } else if (std::string(test.name) == "C9 snapshot in Gap") {
            Session first(test.config, test.requirements);
            first.apply(test.events[0]);
            first.apply(test.events[1]);
            const auto snap = first.snapshot();
            Session second(test.config, test.requirements);
            second.restore(snap);
            for (std::size_t i = 2; i < test.events.size(); ++i) {
                second.apply(test.events[i]);
            }
            second.finish();
            reference = runEvents(test.config, test.requirements, test.events);
            printCase(test, second.result(), reference);
            continue;
        }
        printCase(test, actual, reference);
    }
}

void reportStress() {
    Config config;
    config.holdGraceUs = kGrace;
    constexpr int count = 2'048;
    constexpr int gaps = 6;
    std::vector<Requirement> requirements;
    std::vector<Event> events;
    requirements.reserve(count);
    events.reserve(static_cast<std::size_t>(count) * (2 + gaps * 2));
    for (int i = 0; i < count; ++i) {
        // Keep each lane's head windows disjoint so this measures continuity state, not head
        // arbitration. The 256-lane fanout still exercises a large active set.
        const Tick anchor = 1'000'000 + static_cast<Tick>(i) * 800'000;
        const Tick end = anchor + 700'000;
        requirements.push_back(Requirement{static_cast<std::uint32_t>(i), Kind::Hold, Grip::Handoff,
                                           static_cast<std::uint8_t>(i % 256), anchor, end, 1});
        const auto lane = static_cast<std::uint8_t>(i % 256);
        std::uint32_t contact = static_cast<std::uint32_t>(i);
        events.push_back(press(anchor, contact, lane));
        for (int gap = 0; gap < gaps; ++gap) {
            const Tick releaseAt = anchor + 90'000 + static_cast<Tick>(gap) * 90'000;
            const Tick resumeAt = releaseAt + 30'000;
            events.push_back(release(releaseAt, contact, lane));
            contact = static_cast<std::uint32_t>(i + (gap + 1) * count);
            events.push_back(press(resumeAt, contact, lane));
        }
        // Leave the final contact held; finish() settles the body at the note end. Releasing just
        // after end would intentionally open a Gap and allow the timer to fire before finish().
    }
    const auto start = std::chrono::steady_clock::now();
    const Result result = runEvents(config, requirements, events);
    const double elapsed = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    const int expectedHeads = count;
    const int expectedBodies = count;
    const bool ok = countOutcome(result, Outcome::HeadHit) == expectedHeads &&
                    countOutcome(result, Outcome::BodyHit) == expectedBodies &&
                    countOutcome(result, Outcome::Break) == 0 && result.strays == 0;
    if (!ok) {
        ++failures;
    }
    std::printf("\n| requirements | gaps/requirement | input events | heads | bodies | breaks | strays | run ms | status |\n");
    std::printf("| --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
    std::printf("| %d | %d | %zu | %d | %d | %d | %zu | %.2f | %s |\n", count, gaps,
                events.size(), countOutcome(result, Outcome::HeadHit),
                countOutcome(result, Outcome::BodyHit), countOutcome(result, Outcome::Break),
                result.strays, elapsed, ok ? "as predicted" : "UNEXPECTED");
    if (!ok) {
        for (std::size_t i = 0; i < std::min<std::size_t>(result.items.size(), 5); ++i) {
            const auto& state = result.items[i];
            std::printf("debug req=%u stage=%u released=%lld contact=%u seg=%u\n", state.req,
                        static_cast<unsigned>(state.stage), static_cast<long long>(state.releasedAt),
                        state.contact, state.segment);
        }
    }
}

void reportPreparedGraceMatrix() {
    std::printf("\n### C10-C14 prepared grace matrix\n\n");
    std::printf("| case | expectation | observed | status |\n| --- | --- | --- | --- |\n");

    Config config;
    config.holdGraceUs = kGrace;
    config.holdGraceMinUs = 0;
    config.holdGraceMaxUs = 120'000;

    // C10: each requirement owns its own prepared grace value.
    Requirement shortGrace = withGrace(hold(10), 20'000);
    Requirement longGrace = withGrace(hold(11), 80'000);
    shortGrace.lane = 0;
    longGrace.lane = 1;
    const Result c10 = runEvents(
        config, {shortGrace, longGrace},
        {press(kAnchor, 1, 0), press(kAnchor, 2, 1), release(1'400'000, 1, 0),
         release(1'400'000, 2, 1), press(1'450'000, 3, 0), press(1'450'000, 4, 1),
         release(kEnd + 10'000, 4, 1)});
    const bool c10Ok = !c10.prepareRejected && countOutcome(c10, Outcome::HeadHit) == 2 &&
                       countOutcome(c10, Outcome::Break) == 1 &&
                       countOutcome(c10, Outcome::BodyHit) == 1 && c10.strays == 1;
    if (!c10Ok) {
        ++failures;
    }
    std::printf("| C10 per-requirement grace | short breaks, long recovers | %s | %s |\n",
                describe(c10).c_str(), c10Ok ? "passed" : "FAILED");

    // C11: zero and maximum values use the same strict boundary rule.
    Requirement zero = withGrace(hold(12), 0);
    const Result c11Zero = runEvents(config, {zero},
                                     {press(kAnchor, 1), release(1'400'000, 1),
                                      press(1'400'000, 2)});
    Config maxConfig = config;
    maxConfig.holdGraceMaxUs = 120'000;
    Requirement maximum = withGrace(hold(13), maxConfig.holdGraceMaxUs);
    const Result c11Max = runEvents(maxConfig, {maximum},
                                    {press(kAnchor, 1), release(1'400'000, 1),
                                     press(1'519'999, 2), release(kEnd + 10'000, 2)});
    const Result c11Rejected = runEvents(config, {withGrace(hold(18, Grip::Sticky), 10'000)},
                                         {press(kAnchor, 1), release(1'400'000, 1),
                                          press(1'405'000, 2)});
    const bool c11Ok = countOutcome(c11Zero, Outcome::Break) == 1 && c11Zero.strays == 1 &&
                       countOutcome(c11Max, Outcome::Break) == 0 &&
                       countOutcome(c11Max, Outcome::BodyHit) == 1 && c11Max.strays == 0 &&
                       c11Rejected.prepareRejected;
    if (!c11Ok) {
        ++failures;
    }
    std::printf("| C11 zero/min/max grace | zero rejects same-tick recovery; max accepts 119,999 us; invalid sticky override rejects | zero=%s; max=%s; rejected=%s | %s |\n",
                describe(c11Zero).c_str(), describe(c11Max).c_str(), c11Rejected.prepareRejected ? "yes" : "no",
                c11Ok ? "passed" : "FAILED");

    // C12: source is content metadata, while effective value + policy define judgement identity.
    const Result inherited = runEvents(config, {hold(14)},
                                       {press(kAnchor, 1), release(1'400'000, 1),
                                        press(1'450'000, 2), release(kEnd + 10'000, 2)});
    const Result explicitSame = runEvents(config, {withGrace(hold(14), kGrace)},
                                          {press(kAnchor, 1), release(1'400'000, 1),
                                           press(1'450'000, 2), release(kEnd + 10'000, 2)});
    const Result explicitDifferent = runEvents(config, {withGrace(hold(14), 80'000)},
                                                {press(kAnchor, 1), release(1'400'000, 1),
                                                 press(1'450'000, 2), release(kEnd + 10'000, 2)});
    Config policy2 = config;
    policy2.graceResolutionPolicy = 2;
    const Result policyChanged = runEvents(policy2, {withGrace(hold(14), kGrace)},
                                           {press(kAnchor, 1), release(1'400'000, 1),
                                            press(1'450'000, 2), release(kEnd + 10'000, 2)});
    const bool c12Ok = inherited.judgementDigest == explicitSame.judgementDigest &&
                       inherited.contentDigest != explicitSame.contentDigest &&
                       inherited.judgementDigest != explicitDifferent.judgementDigest &&
                       explicitSame.judgementDigest != policyChanged.judgementDigest;
    if (!c12Ok) {
        ++failures;
    }
    std::printf("| C12 identity policy/source | same effective value shares judgement identity; source/policy remain distinguishable | jd=%s/%s/%s/%s | %s |\n",
                inherited.judgementDigest == explicitSame.judgementDigest ? "same" : "DIFF",
                inherited.contentDigest != explicitSame.contentDigest ? "content-diff" : "content-SAME",
                inherited.judgementDigest != explicitDifferent.judgementDigest ? "value-diff" : "value-SAME",
                explicitSame.judgementDigest != policyChanged.judgementDigest ? "policy-diff" : "policy-SAME",
                c12Ok ? "passed" : "FAILED");

    // C13: a late Hook update cannot rewrite the prepared value.
    Session c13Session(config, {hold(15)});
    c13Session.apply(press(kAnchor, 1));
    c13Session.apply(release(1'400'000, 1));
    c13Session.setRuntimeHoldGraceForTesting(0);
    c13Session.apply(press(1'450'000, 2));
    c13Session.apply(release(kEnd + 10'000, 2));
    c13Session.finish();
    const Result c13 = c13Session.result();
    const bool c13Ok = countOutcome(c13, Outcome::Break) == 0 &&
                       countOutcome(c13, Outcome::BodyHit) == 1 && c13.strays == 0;
    if (!c13Ok) {
        ++failures;
    }
    std::printf("| C13 post-prepare Hook change | inherited 60 ms remains frozen after Hook becomes 0 | %s | %s |\n",
                describe(c13).c_str(), c13Ok ? "passed" : "FAILED");

    // C14: Slider hard deadline uses max(tail late window, prepared grace).
    Config sliderConfig = config;
    sliderConfig.sliderLateUs = 20'000;
    Requirement sliderReq = withGrace(slider(16), 60'000);
    Session c14Session(sliderConfig, {sliderReq});
    c14Session.apply(press(kAnchor, 1));
    c14Session.apply(release(kEnd - 10'000, 1));
    c14Session.apply(move(kEnd + 45'000, 1, 4));
    const Result c14BeforeDeadline = c14Session.result();
    c14Session.finish();
    const Result c14 = c14Session.result();
    const Tick expectedDeadline = kEnd + 60'000;
    const bool c14Ok = !c14BeforeDeadline.prepareRejected &&
                       item(c14BeforeDeadline).stage == Stage::Gap &&
                       !item(c14BeforeDeadline).breakEmitted &&
                       item(c14BeforeDeadline).deadline == expectedDeadline &&
                       countOutcome(c14, Outcome::BodyHit) == 1 &&
                       countOutcome(c14, Outcome::Break) == 0;
    if (!c14Ok) {
        ++failures;
    }
    std::printf("| C14 Slider deadline | deadline=max(20 ms, 60 ms) and no early break | before=%s; final=%s; deadline=%lld | %s |\n",
                stageLabel(item(c14BeforeDeadline).stage), describe(c14).c_str(),
                static_cast<long long>(item(c14).deadline), c14Ok ? "passed" : "FAILED");
}

struct SliceMeasurement {
    Result baseline;
    std::size_t requirements = 0;
    std::size_t events = 0;
    std::size_t activityPeak = 0;
    std::size_t gapTimerPeak = 0;
    std::size_t maxSnapshotBytes = 0;
    double seekP95Ms = 0.0;
    double seekMaxMs = 0.0;
    bool lossless = false;
};

std::vector<Requirement> representativeRequirements() {
    std::vector<Requirement> requirements;
    requirements.reserve(96);
    for (std::uint32_t i = 0; i < 96; ++i) {
        const bool isSlider = (i % 3u) == 0u;
        Requirement req = isSlider ? slider(1000u + i) : hold(1000u + i);
        req.lane = static_cast<std::uint8_t>(i % 8u);
        req.anchor = 1'000'000 + static_cast<Tick>(i) * 350'000;
        req.end = req.anchor + (isSlider ? 620'000 : 420'000);
        req.segments = isSlider ? 8 : 1;
        if ((i % 4u) == 0u) {
            req = withGrace(req, 20'000);
        } else if ((i % 4u) == 1u) {
            req = withGrace(req, 60'000);
        } else if ((i % 4u) == 2u) {
            req = withGrace(req, 100'000);
        }
        requirements.push_back(req);
    }
    return requirements;
}

std::vector<Event> representativeEvents(const std::vector<Requirement>& requirements) {
    std::vector<Event> events;
    events.reserve(requirements.size() * 7);
    for (const Requirement& req : requirements) {
        const std::uint32_t firstContact = req.id * 2u;
        events.push_back(press(req.anchor, firstContact, req.lane));
        if (req.kind == Kind::Slider) {
            const Tick step = (req.end - req.anchor) / static_cast<Tick>(req.segments);
            for (std::uint32_t segment = 1; segment <= req.segments; ++segment) {
                events.push_back(move(req.anchor + step * static_cast<Tick>(segment), firstContact,
                                      segment));
            }
            const Tick releaseAt = req.end - 80'000;
            events.push_back(release(releaseAt, firstContact, req.lane));
            events.push_back(press(releaseAt + 30'000, firstContact + 1u, req.lane));
            events.push_back(move(req.end, firstContact + 1u, req.segments));
            events.push_back(release(req.end + 10'000, firstContact + 1u, req.lane));
        } else {
            const Tick releaseAt = req.anchor + 200'000;
            events.push_back(release(releaseAt, firstContact, req.lane));
            events.push_back(press(releaseAt + 30'000, firstContact + 1u, req.lane));
            events.push_back(release(req.end + 10'000, firstContact + 1u, req.lane));
        }
    }
    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.t < b.t;
    });
    return events;
}

std::size_t peakIntervals(const std::vector<std::pair<Tick, Tick>>& intervals) {
    std::vector<std::pair<Tick, int>> edges;
    edges.reserve(intervals.size() * 2);
    for (const auto& [begin, end] : intervals) {
        if (begin >= end) {
            continue;
        }
        edges.emplace_back(begin, 1);
        edges.emplace_back(end, -1);
    }
    std::stable_sort(edges.begin(), edges.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) {
            return a.first < b.first;
        }
        return a.second < b.second;
    });
    std::size_t active = 0;
    std::size_t peak = 0;
    for (const auto& edge : edges) {
        if (edge.second > 0) {
            ++active;
            peak = std::max(peak, active);
        } else {
            --active;
        }
    }
    return peak;
}

SliceMeasurement measureRepresentativeSlice() {
    Config config;
    config.holdGraceUs = kGrace;
    config.holdGraceMinUs = 0;
    config.holdGraceMaxUs = 120'000;
    config.sliderLateUs = 20'000;
    const std::vector<Requirement> requirements = representativeRequirements();
    const std::vector<Event> events = representativeEvents(requirements);

    Session baselineSession(config, requirements);
    for (const Event& event : events) {
        baselineSession.apply(event);
    }
    baselineSession.finish();

    std::vector<std::pair<Tick, Tick>> activity;
    std::vector<std::pair<Tick, Tick>> gaps;
    activity.reserve(requirements.size());
    gaps.reserve(requirements.size());
    for (const Requirement& req : requirements) {
        const Tick effectiveGrace = req.explicitGrace ? req.graceUs : config.holdGraceUs;
        const Tick deadline = req.end +
                              (req.kind == Kind::Slider
                                   ? std::max(config.sliderLateUs, effectiveGrace)
                                   : effectiveGrace);
        activity.emplace_back(req.anchor - config.goodUs, deadline);
        const Tick releaseAt = req.kind == Kind::Slider ? req.end - 80'000 : req.anchor + 200'000;
        gaps.emplace_back(releaseAt, releaseAt + effectiveGrace);
    }

    std::vector<double> seekMs;
    seekMs.reserve(24);
    std::size_t maxSnapshotBytes = 0;
    bool lossless = true;
    for (std::size_t target = 1; target <= 24; ++target) {
        const std::size_t split = (events.size() * target) / 25;
        Session recorder(config, requirements);
        for (std::size_t i = 0; i < split; ++i) {
            recorder.apply(events[i]);
        }
        const std::vector<std::uint8_t> snapshot = recorder.snapshot();
        maxSnapshotBytes = std::max(maxSnapshotBytes, snapshot.size());
        Session seeker(config, requirements);
        const auto start = std::chrono::steady_clock::now();
        seeker.restore(snapshot);
        for (std::size_t i = split; i < events.size(); ++i) {
            seeker.apply(events[i]);
        }
        seeker.finish();
        seekMs.push_back(std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - start)
                             .count());
        if (seeker.result().judgementDigest != baselineSession.result().judgementDigest ||
            seeker.result().digest != baselineSession.result().digest) {
            lossless = false;
        }
    }

    SliceMeasurement measurement;
    measurement.baseline = baselineSession.result();
    measurement.requirements = requirements.size();
    measurement.events = events.size();
    measurement.activityPeak = peakIntervals(activity);
    measurement.gapTimerPeak = peakIntervals(gaps);
    measurement.maxSnapshotBytes = maxSnapshotBytes;
    measurement.seekP95Ms = seekMs[static_cast<std::size_t>(0.95 * (seekMs.size() - 1))];
    measurement.seekMaxMs = *std::max_element(seekMs.begin(), seekMs.end());
    measurement.lossless = lossless;
    return measurement;
}

void reportRepresentativeSlice() {
    const SliceMeasurement measurement = measureRepresentativeSlice();
    const bool ok = measurement.lossless && measurement.baseline.prepareRejected == false;
    if (!ok) {
        ++failures;
    }
    std::printf("\n### Representative research slice\n\n");
    std::printf("This is a deterministic research slice, not a production chart fixture.\n\n");
    std::printf("| requirements | input events | activity peak | Gap timer peak | max snapshot bytes | seek p95 ms | seek max ms | lossless | status |\n");
    std::printf("| ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |\n");
    std::printf("| %zu | %zu | %zu | %zu | %zu | %.3f | %.3f | %s | %s |\n",
                measurement.requirements, measurement.events, measurement.activityPeak,
                measurement.gapTimerPeak, measurement.maxSnapshotBytes, measurement.seekP95Ms,
                measurement.seekMaxMs, measurement.lossless ? "yes" : "NO", ok ? "passed" : "FAILED");
}

} // namespace

void reportContinuity() {
    std::printf("## I. Continuity grace: Hold and Slider\n\n");
    std::printf("Research-only minimum implementation: handoff reuses Gap + holdGrace; Slider keeps seg and head fact.\n\n");
    reportDirected();
    reportStress();
    reportPreparedGraceMatrix();
    reportRepresentativeSlice();
    if (failures != 0) {
        std::printf("\nFAIL: %d continuity check(s) failed\n", failures);
        std::exit(7);
    }
    std::printf("\nContinuity checks passed.\n");
}
