#include "continuity.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
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

} // namespace

void reportContinuity() {
    std::printf("## I. Continuity grace: Hold and Slider\n\n");
    std::printf("Research-only minimum implementation: handoff reuses Gap + holdGrace; Slider keeps seg and head fact.\n\n");
    reportDirected();
    reportStress();
    if (failures != 0) {
        std::printf("\nFAIL: %d continuity check(s) failed\n", failures);
        std::exit(7);
    }
    std::printf("\nContinuity checks passed.\n");
}
