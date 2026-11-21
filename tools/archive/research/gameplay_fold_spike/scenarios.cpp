// Research spike: multi-contact scenarios. Each case states a prediction from the design documents
// and fails the run if the prediction does not hold. Not part of the product build.
#include "contacts.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace spike::mt;

namespace {

Tick ms(Tick v) {
    return v * 1000;
}

struct Case {
    const char* name;
    const char* expectation;
    Config config;
    std::vector<Req> reqs;
    std::vector<ContactEvent> events;
    std::string want;
};

int failures = 0;

void runCase(const Case& c) {
    const Result got = run(c.config, c.reqs, c.events);
    const std::string actual = describe(got);
    const bool ok = actual == c.want;
    if (!ok) {
        ++failures;
    }
    std::printf("| %s | %s | `%s` | %s |\n", c.name, c.expectation, actual.c_str(),
                ok ? "as predicted" : "UNEXPECTED");
    if (!ok) {
        std::printf("\nFAIL %s: wanted `%s`, got `%s`\n", c.name, c.want.c_str(), actual.c_str());
    }
}

std::vector<Case> cases() {
    std::vector<Case> out;
    const Tick good = ms(120);
    const Tick grace = ms(60);

    // S1 Phigros-style overlapping vertical bands: two taps, different perfect times, one lane.
    {
        Case c;
        c.name = "S1 two taps, one lane, 30 ms apart";
        c.expectation = "the nearer perfect time claims the press";
        c.reqs = {{0, Kind::Tap, Grip::Sticky, 0, 1'000'000, 1'000'000, 0},
                  {1, Kind::Tap, Grip::Sticky, 0, 1'030'000, 1'030'000, 0}};
        c.events = {{1'000'000, 100, 0, true},
                    {1'010'000, 100, 0, false},
                    {1'030'000, 101, 0, true},
                    {1'040'000, 101, 0, false}};
        c.want = "Hit(req0) Hit(req1) ";
        out.push_back(c);
    }
    // S2 one press, two requirements on the same anchor.
    {
        Case c;
        c.name = "S2 one press, two taps, same anchor";
        c.expectation = "default fanout 1 settles one; all-on-equal-anchor settles both";
        c.reqs = {{0, Kind::Tap, Grip::Sticky, 0, 1'000'000, 1'000'000, 0},
                  {1, Kind::Tap, Grip::Sticky, 0, 1'000'000, 1'000'000, 0}};
        c.events = {{1'000'000, 100, 0, true}, {1'010'000, 100, 0, false}};
        c.want = "Hit(req0) Miss(req1) ";
        out.push_back(c);
        Case b = c;
        b.config.fanoutAllOnEqualAnchor = true;
        b.name = "S2b fanout = all on equal anchor";
        b.expectation = "both requirements settle from the single press";
        b.want = "Hit(req0) Hit(req1) ";
        out.push_back(b);
    }
    // S3 sticky: a second contact cannot take over a hold in flight.
    {
        Case c;
        c.name = "S3 sticky hold, second contact mid-body";
        c.expectation = "no handoff; the body fact is Broken, head was Hit";
        c.config.holdGraceUs = grace;
        c.config.strayWhen = StrayRule::ConsumeEmpty;
        c.reqs = {{0, Kind::Hold, Grip::Sticky, 0, 1'000'000, 2'000'000, 0}};
        c.events = {{1'000'000, 100, 0, true},
                    {1'400'000, 100, 0, false},
                    {1'450'000, 101, 0, true},
                    {1'900'000, 101, 0, false}};
        c.want = "Hit(req0) Broken(req0,body) strays=1";
        out.push_back(c);
        Case b = c;
        b.name = "S3b same, strayWhen = NoCandidate";
        b.expectation = "the hold considered the second press and rejected it, so not a stray";
        b.config.strayWhen = StrayRule::NoCandidate;
        b.want = "Hit(req0) Broken(req0,body) ";
        out.push_back(b);
    }
    // S4 handoff: the same geometry with grip = handoff completes.
    {
        Case c;
        c.name = "S4 handoff hold, second contact within grace";
        c.expectation = "the second contact takes over; both Measure components are Hit";
        c.config.holdGraceUs = grace;
        c.reqs = {{0, Kind::Hold, Grip::Handoff, 0, 1'000'000, 2'000'000, 0}};
        c.events = {{1'000'000, 100, 0, true},
                    {1'400'000, 100, 0, false},
                    {1'450'000, 101, 0, true},
                    {2'000'000, 101, 0, false}};
        c.want = "Hit(req0) Hit(req0,body) ";
        out.push_back(c);
    }
    // S5 observe edge: a bomb and a hold see the same press; only the hold owns it.
    {
        Case c;
        c.name = "S5 bomb (observe) and hold (claim) on one press";
        c.expectation = "both see it; only the hold owns it; the body still breaks";
        c.config.holdGraceUs = grace;
        c.reqs = {{0, Kind::Bomb, Grip::Sticky, 0, 1'000'000, 1'000'000, 0},
                  {1, Kind::Hold, Grip::Sticky, 0, 1'000'000, 2'000'000, 0}};
        c.events = {{1'000'000, 100, 0, true}, {1'300'000, 100, 0, false}};
        c.want = "Detonated(req0,observe) Hit(req1) Broken(req1,body) ";
        out.push_back(c);
    }
    // S6 a press outside every window has no candidate.
    {
        Case c;
        c.name = "S6 press 130 ms late, window is 120 ms";
        c.expectation = "no candidate; it is a stray";
        c.reqs = {{0, Kind::Tap, Grip::Sticky, 0, 1'000'000, 1'000'000, 0}};
        c.events = {{1'130'000, 100, 0, true}, {1'140'000, 100, 0, false}};
        c.want = "Miss(req0) strays=1";
        out.push_back(c);
        Case b = c;
        b.name = "S6b same, strayWhen = NoCandidate";
        b.expectation = "no instance considered it at all, so the two rules agree here";
        b.config.strayWhen = StrayRule::NoCandidate;
        b.want = "Miss(req0) ";
        out.push_back(b);
    }
    // S7 slot reuse. ids 0 and 32 collide on slot 0; the second may only take the slot once the
    // first holder's contact has terminated.
    {
        Case c;
        c.name = "S7 slot reuse after the holder's contact ended";
        c.expectation = "the terminated claim is replaced implicitly";
        c.config.goodUs = good;
        c.reqs = {{0, Kind::Tap, Grip::Sticky, 0, 1'000'000, 1'000'000, 0},
                  {32, Kind::Tap, Grip::Sticky, 0, 1'500'000, 1'500'000, 0}};
        c.events = {{1'000'000, 100, 0, true},
                    {1'100'000, 100, 0, false},
                    {1'500'000, 101, 0, true},
                    {1'510'000, 101, 0, false}};
        c.want = "Hit(req0) Hit(req32) ";
        out.push_back(c);
        Case b = c;
        b.name = "S7b same slot, first contact never ended";
        b.expectation = "the live claim blocks the second requirement";
        b.events = {{1'000'000, 100, 0, true}, {1'500'000, 101, 0, true}};
        b.want = "Hit(req0) Miss(req32) strays=1";
        out.push_back(b);
    }
    return out;
}

} // namespace

void reportContacts() {
    std::printf("| case | expectation | observed | result |\n| --- | --- | --- | --- |\n");
    for (const Case& c : cases()) {
        runCase(c);
    }
    if (failures != 0) {
        std::printf("\n%d multi-contact case(s) did not match the design's prediction\n", failures);
        std::exit(7);
    }
}
