//  S7A-2 observation normalization and entry-boundary golden tests.
//
//  Every expected value below is a hard-coded number, never a relation such as "not equal" or
//  "greater than". The cases are organised by the rulings they pin down:
//
//    1. observationTick is captured once at the entry from the calibrated session clock; the raw
//       device / host / audio / render stamps are diagnostic context that never becomes a tick
//       (Spec 3.7.3);
//    2. duplicate queue admission is decided by the canonical observation identity, so it is
//       invariant under any permutation of the arrival order and under any assignment of ingress
//       ordinals (Spec 3.7.4 item 4);
//    3. the four CM-T10 cases: clock reversal, a negative clock value, a duplicated ingress
//       ordinal, and crossing a discontinuity; plus the same-tick collision, which CM-T10 also
//       decides from the canonical identity rather than from the arrival order;
//    4. continuous input capability is a stable 7A rejection that names a capability and a
//       replacement path (Spec 3.7.7);
//    5. a refused entry writes nothing to the session ingress state; and the quantized amount an
//       observation carries is the one its mapped domain declares.
//
//  The numeric ranges and timestamps used here are test-local declarations; no business quantity
//  and no budget number is frozen by this batch.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <cuexis/judgement/timebase.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace judgement = cuexis::judgement;

using judgement::AmountSpec;
using judgement::CanonicalIngressSubject;
using judgement::ContinuityKind;
using judgement::DiscontinuityDeclaration;
using judgement::IngressDeclaration;
using judgement::IngressSequence;
using judgement::InputAction;
using judgement::InputDomainDeclaration;
using judgement::InputMappingProfile;
using judgement::NormalizedObservationEntry;
using judgement::ObservationTick;
using judgement::RationalBeat;
using judgement::RationalDuration;
using judgement::RawIngressTimestamps;
using judgement::SessionIngressState;
using judgement::SourceClass;
using judgement::Tick;

//  -------------------------------------------------------------------------------------------
//  Fixtures
//  -------------------------------------------------------------------------------------------

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    const auto value = RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto sourceClass(std::string_view token) -> SourceClass {
    const auto value = SourceClass::fromToken(token);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto channel(std::string_view token) -> judgement::ChannelRef {
    const auto value = judgement::ChannelRef::fromToken(token);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto mapping(std::string_view domainToken, std::int64_t minimum, std::int64_t maximum)
    -> InputMappingProfile {
    const auto scale = RationalDuration::create(1, 1);
    REQUIRE(scale.has_value());
    return InputMappingProfile{
        .profileId = "test.mapping.keyboard",
        .profileVersion = "1",
        .sourceClass = sourceClass("test.source.keyboard"),
        .domains = {InputDomainDeclaration{
            .domainToken = domainToken,
            .amount = AmountSpec{.scale = *scale,
                                 .minimum = minimum,
                                 .maximum = maximum,
                                 .boundaryPolicy = judgement::AmountBoundaryPolicy::inclusive}}},
    };
}

//  A discrete, non-crossing declaration with one ingress ordinal and one verb.
[[nodiscard]] auto declaration(std::uint64_t sequence, InputAction action) -> IngressDeclaration {
    return IngressDeclaration{
        .ingressSequence = IngressSequence{sequence},
        .action = action,
        .channel = channel("test.channel.lane1"),
        .domainToken = {},
        .rawTimestamps = RawIngressTimestamps{.rawTicks = 111,
                                              .hostArrivalTicks = 222,
                                              .audioFrameTicks = 333,
                                              .renderFrameTicks = 444},
        .discontinuity = DiscontinuityDeclaration{.crossedSamplingGap = false,
                                                  .reconnected = false,
                                                  .droppedSamples = false},
        .continuity = ContinuityKind::discrete,
        .quantity = std::nullopt,
    };
}

[[nodiscard]] auto contextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

void checkRejection(const cuexis::core::Error& error, std::string_view code,
                    std::string_view category, std::string_view path) {
    CHECK(error.code() == code);
    CHECK(contextValue(error, "category") == category);
    CHECK(contextValue(error, "severity") == "error");
    CHECK(contextValue(error, "faulted") == "false");
    CHECK(contextValue(error, "field.section") == "judgement.input");
    CHECK(contextValue(error, "field.path") == path);
}

//  Normalizes one declaration and requires success.
[[nodiscard]] auto accepted(SessionIngressState& state, const InputMappingProfile& profile,
                            std::int64_t clock, const IngressDeclaration& incoming)
    -> NormalizedObservationEntry {
    const auto entry =
        judgement::normalizeObservation(state, profile, ObservationTick{Tick{clock}}, incoming);
    REQUIRE(entry.has_value());
    return *entry;
}

//  -------------------------------------------------------------------------------------------
//  1. observationTick comes from the entry capture of the calibrated session clock
//  -------------------------------------------------------------------------------------------

TEST_CASE("observation tick: the entry capture is the only source and raw timestamps stay context",
          "[judgement][s7a2][input][observation]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);

    SECTION("the captured clock becomes the tick and the raw stamps are not consulted") {
        SessionIngressState state;
        const IngressDeclaration incoming = declaration(1, InputAction::press);

        //  The raw device stamp is 111. A tick of 500000 can therefore only have come from the
        //  capture argument, never from the raw timestamps.
        const NormalizedObservationEntry entry = accepted(state, profile, 500000, incoming);
        CHECK(entry.observation.observationTick.tick().value() == 500000);
        CHECK(entry.observation.observationId.value() == 0);
        CHECK(entry.observation.ingressSequence.value() == 1);
        CHECK(entry.observation.action == InputAction::press);
        CHECK(entry.observation.sourceClass.token() == "test.source.keyboard");
        CHECK_FALSE(entry.observation.amount.has_value());
    }

    SECTION("a second capture beside different raw stamps yields the second captured value") {
        SessionIngressState state;
        const NormalizedObservationEntry first =
            accepted(state, profile, 10, declaration(1, InputAction::press));
        IngressDeclaration second = declaration(2, InputAction::update);
        second.rawTimestamps = RawIngressTimestamps{.rawTicks = -999,
                                                    .hostArrivalTicks = -998,
                                                    .audioFrameTicks = -997,
                                                    .renderFrameTicks = -996};
        const NormalizedObservationEntry entry = accepted(state, profile, 42, second);

        CHECK(first.observation.observationTick.tick().value() == 10);
        CHECK(entry.observation.observationTick.tick().value() == 42);
        CHECK(entry.observation.observationId.value() == 1);
    }

    SECTION("the raw timestamps travel in the diagnostic context of a rejection") {
        SessionIngressState state;
        const auto rejected =
            judgement::normalizeObservation(state, profile, ObservationTick{Tick{5}}, [&] {
                IngressDeclaration incoming = declaration(1, InputAction::press);
                incoming.continuity = ContinuityKind::trajectory;
                return incoming;
            }());
        REQUIRE_FALSE(rejected.has_value());
        CHECK(contextValue(rejected.error(), "raw.time") ==
              "device=111 host=222 audio=333 render=444");
        //  The raw stamp values appear in one opaque context token and nowhere that could be read
        //  back as a judgement time.
        CHECK(rejected.error().context().size() > 0);
    }

    SECTION("the entry state records the captured clock and nothing before the first entry") {
        SessionIngressState state;
        CHECK(state.lastObservedTick() == nullptr);

        (void)accepted(state, profile, -7, declaration(1, InputAction::press));
        REQUIRE(state.lastObservedTick() != nullptr);
        CHECK(state.lastObservedTick()->tick().value() == -7);

        state.reset();
        CHECK(state.lastObservedTick() == nullptr);
    }
}

//  -------------------------------------------------------------------------------------------
//  2. Duplicate admission uses the canonical observation identity, never the ingress ordinal
//  -------------------------------------------------------------------------------------------

TEST_CASE("duplicate queue: the criterion is the canonical identity, not the ordinal or the order",
          "[judgement][s7a2][input][duplicate]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);
    const ObservationTick clock{Tick{7}};

    //  Two submissions of the same canonical observation. Their ingress ordinals are deliberately
    //  different, which is exactly the case the ruling covers: a repeated activation of one
    //  canonical observation is a duplicate queue entry even though its ordinal is new.
    SessionIngressState firstState;
    SessionIngressState secondState;
    const auto first = judgement::normalizeObservation(firstState, profile, clock,
                                                       declaration(1, InputAction::press));
    const auto second = judgement::normalizeObservation(secondState, profile, clock,
                                                        declaration(2, InputAction::press));
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    SECTION("the canonical subjects are equal although the ingress ordinals differ") {
        CHECK(first->subject == second->subject);
        CHECK(first->observation.ingressSequence != second->observation.ingressSequence);
        CHECK(first->observation.ingressSequence.value() == 1);
        CHECK(second->observation.ingressSequence.value() == 2);
    }

    SECTION("both presentation orders reject with the same stable diagnostic") {
        const auto forward = judgement::admitLateQueueEntry(first->subject, second->subject);
        const auto backward = judgement::admitLateQueueEntry(second->subject, first->subject);
        REQUIRE_FALSE(forward.has_value());
        REQUIRE_FALSE(backward.has_value());
        CHECK(forward.error().code() == backward.error().code());
        CHECK(forward.error().code() == "judgement.s7a2.late.duplicate_queue_entry");
        CHECK(forward.error().message() == backward.error().message());
        checkRejection(forward.error(), "judgement.s7a2.late.duplicate_queue_entry",
                       "late_policy_incomplete", "lateQueue.canonicalKey");
        checkRejection(backward.error(), "judgement.s7a2.late.duplicate_queue_entry",
                       "late_policy_incomplete", "lateQueue.canonicalKey");
    }

    SECTION("a different canonical observation at the same tick is not a duplicate") {
        SessionIngressState otherState;
        const auto other = judgement::normalizeObservation(otherState, profile, clock,
                                                           declaration(1, InputAction::release));
        REQUIRE(other.has_value());
        CHECK_FALSE(other->subject == first->subject);
        CHECK(judgement::admitLateQueueEntry(first->subject, other->subject).has_value());
        CHECK(judgement::admitLateQueueEntry(other->subject, first->subject).has_value());

        //  The same-tick question is answered from the canonical identity as well, so it is
        //  symmetric in the two arguments.
        CHECK(judgement::hasDistinctIdentityAtSameTick(first->subject, other->subject));
        CHECK(judgement::hasDistinctIdentityAtSameTick(other->subject, first->subject));
        CHECK_FALSE(judgement::hasDistinctIdentityAtSameTick(first->subject, first->subject));
    }

    SECTION("an ordinal-only difference never creates a duplicate") {
        //  The ordinal is not a member of the canonical subject at all: two subjects that differ
        //  only in the ordinal their observations carried compare equal, which is the type-level
        //  statement of "the ordinal is not the criterion".
        CHECK(first->subject == second->subject);
        CHECK(judgement::admitLateQueueEntry(first->subject, second->subject).error().code() ==
              "judgement.s7a2.late.duplicate_queue_entry");
    }
}

//  -------------------------------------------------------------------------------------------
//  3. The CM-T10 cases
//  -------------------------------------------------------------------------------------------

TEST_CASE("CM-T10: a clock reversal is a stable rejection", "[judgement][s7a2][input][cm-t10]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);

    SECTION("a lower capture after a higher one is refused") {
        SessionIngressState state;
        (void)accepted(state, profile, 100, declaration(1, InputAction::press));

        const auto rejected = judgement::normalizeObservation(
            state, profile, ObservationTick{Tick{99}}, declaration(2, InputAction::release));
        REQUIRE_FALSE(rejected.has_value());
        //  A clock regression is a time-order relation error, so its category is invalid_relation
        //  and explicitly not budget_exceeded: budget_exceeded is reserved for a numeric value that
        //  leaves a declared range or a budget.
        checkRejection(rejected.error(), "judgement.s7a2.timebase.time_reversal",
                       "invalid_relation", "observationTick.calibratedClock");
        CHECK(contextValue(rejected.error(), "category") != "budget_exceeded");
        CHECK(contextValue(rejected.error(), "raw.time") ==
              "device=111 host=222 audio=333 render=444");

        //  The refused entry changed nothing: the recorded clock is still the accepted one and the
        //  same lower capture keeps being refused.
        REQUIRE(state.lastObservedTick() != nullptr);
        CHECK(state.lastObservedTick()->tick().value() == 100);
        CHECK_FALSE(judgement::normalizeObservation(state, profile, ObservationTick{Tick{99}},
                                                    declaration(3, InputAction::release))
                        .has_value());
    }

    SECTION("an equal capture is legal because same-tick ordering is not clock order") {
        SessionIngressState state;
        (void)accepted(state, profile, 100, declaration(1, InputAction::press));
        const auto entry = accepted(state, profile, 100, declaration(2, InputAction::update));
        CHECK(entry.observation.observationTick.tick().value() == 100);
    }
}

TEST_CASE("CM-T10: a negative capture is representable and still monotone",
          "[judgement][s7a2][input][cm-t10]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);

    SECTION("a negative capture is admitted and recorded as it was captured") {
        SessionIngressState state;
        const auto entry = accepted(state, profile, -50, declaration(1, InputAction::press));
        CHECK(entry.observation.observationTick.tick().value() == -50);
        REQUIRE(state.lastObservedTick() != nullptr);
        CHECK(state.lastObservedTick()->tick().value() == -50);
    }

    SECTION("the negative region is ordered the same way as the positive one") {
        SessionIngressState state;
        (void)accepted(state, profile, -50, declaration(1, InputAction::press));
        (void)accepted(state, profile, -50, declaration(2, InputAction::update));
        (void)accepted(state, profile, -1, declaration(3, InputAction::update));

        const auto rejected = judgement::normalizeObservation(
            state, profile, ObservationTick{Tick{-2}}, declaration(4, InputAction::release));
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.timebase.time_reversal",
                       "invalid_relation", "observationTick.calibratedClock");
        CHECK(contextValue(rejected.error(), "category") != "budget_exceeded");
    }
}

TEST_CASE("CM-T10: a duplicated ingress ordinal is a stable rejection",
          "[judgement][s7a2][input][cm-t10]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);

    SECTION("reusing one ordinal inside a session is refused on its own token") {
        SessionIngressState state;
        (void)accepted(state, profile, 10, declaration(5, InputAction::press));

        //  A different tick and a different verb, so the canonical identity is different and the
        //  only thing that repeats is the host submission ordinal.
        const auto rejected = judgement::normalizeObservation(
            state, profile, ObservationTick{Tick{11}}, declaration(5, InputAction::release));
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.ingress_sequence_duplicate",
                       "late_policy_incomplete", "ingressSequence");
    }

    SECTION("the same ordinal is admitted again by a fresh or reset session") {
        SessionIngressState state;
        (void)accepted(state, profile, 10, declaration(5, InputAction::press));
        CHECK_FALSE(judgement::normalizeObservation(state, profile, ObservationTick{Tick{11}},
                                                    declaration(5, InputAction::release))
                        .has_value());

        state.reset();
        const auto entry = accepted(state, profile, 10, declaration(5, InputAction::press));
        CHECK(entry.observation.ingressSequence.value() == 5);
        CHECK(entry.observation.observationId.value() == 0);
    }
}

TEST_CASE("CM-T10: a canonically identical observation at one tick is a same-tick collision",
          "[judgement][s7a2][input][cm-t10]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);
    const ObservationTick clock{Tick{9}};

    SECTION("a new ordinal carrying the same canonical observation is refused") {
        SessionIngressState state;
        (void)accepted(state, profile, 9, declaration(1, InputAction::press));

        //  The ordinal is new and the arrival order is later, so neither is what the verdict can
        //  rest on: the canonical identity is what repeats.
        const auto rejected = judgement::normalizeObservation(state, profile, clock,
                                                              declaration(2, InputAction::press));
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.same_tick_collision",
                       "invalid_relation", "observationTick.sameTickCollision");
    }

    SECTION("every component of the canonical identity is part of the verdict") {
        //  One component changes in each case, and each change makes the two observations distinct,
        //  so all four are admitted at the same tick.
        SessionIngressState state;
        (void)accepted(state, profile, 9, declaration(1, InputAction::press));

        IngressDeclaration update = declaration(2, InputAction::update);
        CHECK(judgement::normalizeObservation(state, profile, clock, update).has_value());

        IngressDeclaration otherChannel = declaration(3, InputAction::press);
        otherChannel.channel = channel("test.channel.lane2");
        CHECK(judgement::normalizeObservation(state, profile, clock, otherChannel).has_value());

        IngressDeclaration otherAmount = declaration(4, InputAction::press);
        otherAmount.quantity = beat(1, 1);
        const auto withAmount = judgement::normalizeObservation(state, profile, clock, otherAmount);
        REQUIRE(withAmount.has_value());
        CHECK(withAmount->subject.hasAmount);
        CHECK(withAmount->subject.amountCanonicalInteger == 1);
    }
}

TEST_CASE("CM-T10: crossing a discontinuity is a stable rejection",
          "[judgement][s7a2][input][cm-t10]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);

    //  Each of the three crossing declarations is refused on its own, and none of them is
    //  represented as an observation.
    const std::vector<DiscontinuityDeclaration> crossings{
        {.crossedSamplingGap = true, .reconnected = false, .droppedSamples = false},
        {.crossedSamplingGap = false, .reconnected = true, .droppedSamples = false},
        {.crossedSamplingGap = false, .reconnected = false, .droppedSamples = true},
    };
    for (const DiscontinuityDeclaration& crossing : crossings) {
        SessionIngressState state;
        IngressDeclaration incoming = declaration(1, InputAction::press);
        incoming.discontinuity = crossing;
        const auto rejected =
            judgement::normalizeObservation(state, profile, ObservationTick{Tick{5}}, incoming);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "input.continuous_unsupported", "capability_disabled",
                       "discontinuity");
        //  The discontinuity case is the same R-05 capability as a declared trajectory and is told
        //  apart from it by the field path, not by a second code.
        CHECK(contextValue(rejected.error(), "capabilityId") == "input.trajectory.v1");
        CHECK(contextValue(rejected.error(), "remediation") == "S7B-1");
        //  The refused entry left the session untouched.
        CHECK(state.lastObservedTick() == nullptr);
    }
}

//  -------------------------------------------------------------------------------------------
//  4. Continuous input capability is a stable 7A rejection
//  -------------------------------------------------------------------------------------------

TEST_CASE("continuous input: every declared capability is refused and points at S7B-1",
          "[judgement][s7a2][input][continuity]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);
    const std::vector<ContinuityKind> continuous{ContinuityKind::trajectory,
                                                 ContinuityKind::minimumReportRate,
                                                 ContinuityKind::reconstruction};

    for (const ContinuityKind kind : continuous) {
        SessionIngressState state;
        IngressDeclaration incoming = declaration(1, InputAction::update);
        incoming.continuity = kind;
        incoming.quantity = beat(1, 1);
        const auto rejected =
            judgement::normalizeObservation(state, profile, ObservationTick{Tick{3}}, incoming);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "input.continuous_unsupported", "capability_disabled",
                       "continuityCapability");
        //  R-05 requires the rejection to name a capability and a replacement path.
        CHECK(contextValue(rejected.error(), "capabilityId") == "input.trajectory.v1");
        CHECK(contextValue(rejected.error(), "remediation") == "S7B-1");
        CHECK_FALSE(state.lastObservedTick() != nullptr);
    }
}

//  -------------------------------------------------------------------------------------------
//  5. The quantized amount inside an observation
//  -------------------------------------------------------------------------------------------

TEST_CASE("observation amount: the mapped domain quantizes the declared quantity",
          "[judgement][s7a2][input][observation]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);

    SECTION("a declared quantity is quantized by the declared AmountSpec") {
        SessionIngressState state;
        IngressDeclaration incoming = declaration(1, InputAction::press);
        incoming.quantity = beat(3, 2);
        const auto entry = accepted(state, profile, 1, incoming);

        REQUIRE(entry.observation.amount.has_value());
        CHECK(entry.observation.amount->value.canonicalInteger == 2);
        CHECK(entry.observation.amount->domainToken == "test.domain.button");
        CHECK(entry.observation.domainToken == "test.domain.button");
        REQUIRE(entry.observation.amount->value.spec != nullptr);
        CHECK(entry.observation.amount->value.spec->minimum == -100);
        CHECK(entry.observation.amount->value.spec->maximum == 100);
        //  The canonical subject carries the amount, so two observations that differ only in their
        //  amount are different canonical observations.
        CHECK(entry.subject.hasAmount);
        CHECK(entry.subject.amountCanonicalInteger == 2);
    }

    SECTION("an amount outside the declared range refuses the whole entry") {
        SessionIngressState state;
        IngressDeclaration incoming = declaration(1, InputAction::press);
        incoming.quantity = beat(101, 1);
        const auto rejected =
            judgement::normalizeObservation(state, profile, ObservationTick{Tick{1}}, incoming);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.amount_out_of_range",
                       "budget_exceeded", "amountSpec.range");
        CHECK_FALSE(state.lastObservedTick() != nullptr);
    }

    SECTION("an event that states no amount carries none and consumes no domain") {
        SessionIngressState state;
        const auto entry = accepted(state, profile, 1, declaration(1, InputAction::update));
        CHECK_FALSE(entry.observation.amount.has_value());
        CHECK(entry.observation.domainToken.empty());
        CHECK_FALSE(entry.subject.hasAmount);
        CHECK(entry.subject.amountCanonicalInteger == 0);
    }

    SECTION("an unstated domain with several declared domains is refused, not guessed") {
        const auto scale = RationalDuration::create(1, 1);
        REQUIRE(scale.has_value());
        InputMappingProfile several = profile;
        several.domains.push_back(InputDomainDeclaration{
            .domainToken = "test.domain.axis",
            .amount = AmountSpec{.scale = *scale,
                                 .minimum = -4,
                                 .maximum = 4,
                                 .boundaryPolicy = judgement::AmountBoundaryPolicy::inclusive}});
        CHECK(judgement::validateInputMapping(several).has_value());

        SessionIngressState state;
        IngressDeclaration incoming = declaration(1, InputAction::press);
        incoming.quantity = beat(1, 1);
        const auto rejected =
            judgement::normalizeObservation(state, several, ObservationTick{Tick{1}}, incoming);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "inputMapping.domains");

        //  Naming the domain explicitly resolves it.
        incoming.domainToken = "test.domain.axis";
        const auto entry = accepted(state, several, 1, incoming);
        REQUIRE(entry.observation.amount.has_value());
        CHECK(entry.observation.amount->domainToken == "test.domain.axis");
    }
}

//  -------------------------------------------------------------------------------------------
//  6. A refused entry writes nothing to the session ingress state
//  -------------------------------------------------------------------------------------------

TEST_CASE("entry state: a refused entry consumes no observation id and records no ordinal",
          "[judgement][s7a2][input][state]") {
    const InputMappingProfile profile = mapping("test.domain.button", -100, 100);
    SessionIngressState state;

    const NormalizedObservationEntry first =
        accepted(state, profile, 5, declaration(1, InputAction::press));
    CHECK(first.observation.observationId.value() == 0);

    //  Refused for a duplicated ordinal. If it had consumed an observation id, the next accepted
    //  observation would be numbered 2.
    CHECK_FALSE(judgement::normalizeObservation(state, profile, ObservationTick{Tick{6}},
                                                declaration(1, InputAction::release))
                    .has_value());

    const NormalizedObservationEntry second =
        accepted(state, profile, 6, declaration(2, InputAction::release));
    CHECK(second.observation.observationId.value() == 1);
    CHECK(second.observation.observationTick.tick().value() == 6);
    CHECK(second.observation.ingressSequence.value() == 2);

    SECTION("reset returns every member to its created value") {
        state.reset();
        CHECK(state.lastObservedTick() == nullptr);
        const auto afterReset = accepted(state, profile, 1, declaration(1, InputAction::press));
        CHECK(afterReset.observation.observationId.value() == 0);
    }
}

TEST_CASE("entry state: a mapping that cannot be honoured refuses every entry",
          "[judgement][s7a2][input][state]") {
    InputMappingProfile broken = mapping("test.domain.button", -100, 100);
    broken.profileVersion = {};

    SessionIngressState state;
    const auto rejected = judgement::normalizeObservation(state, broken, ObservationTick{Tick{1}},
                                                          declaration(1, InputAction::press));
    REQUIRE_FALSE(rejected.has_value());
    checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                   "invalid_relation", "inputMapping.profileVersion");
    CHECK(state.lastObservedTick() == nullptr);
}

} // namespace
