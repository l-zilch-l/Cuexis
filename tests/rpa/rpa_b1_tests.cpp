#include "rpa_b1_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

namespace cuexis::rpa::testing {

TEST_CASE("B1.1 records stage identity separately from canonical input identity", "[rpa][b1]") {
    TraceRecorder recorder(7, 3, 11);
    recorder.record(PipelineStage::capture, EventKind::input, "press", 100, "source=A");
    recorder.record(PipelineStage::fold, EventKind::fact, "press", 100, "grade=hit", 4, 9);

    REQUIRE(recorder.events().size() == 2);
    const auto& events = recorder.events();
    CHECK(events[0].identity.sessionScope == 7);
    CHECK(events[0].identity.runEpoch == 3);
    CHECK(events[0].identity.clockSegment == 11);
    CHECK(events[0].identity.deliverySequence == 0);
    CHECK(events[1].identity.deliverySequence == 1);
    CHECK(events[1].identity.commandOrdinal == 4);
    CHECK(events[1].identity.decisionOrdinal == 9);
    CHECK(events[0].identity.kind != events[1].identity.kind);
    CHECK(events[0].identity.stage != events[1].identity.stage);
}

TEST_CASE("B1.1 manifest keeps identity, missing samples and observation cost visible",
          "[rpa][b1]") {
    const EvidenceManifest manifest{.sourceHead = "7ca1fc6",
                                    .dirtyDiffHash = "ba316274",
                                    .semanticRevision = "rpa-b1-fixture-v1",
                                    .configuration = "Debug;STATIC;headless",
                                    .contentIdentity = "fixture:two-source-press-release",
                                    .hardwareIdentity = "test-only:none",
                                    .missingSampleCount = 2,
                                    .observedEventCount = 9,
                                    .observationOverheadNanoseconds = 17};

    REQUIRE(manifest.validate());
    const auto text = manifest.canonicalText();
    CHECK(text.find("missing_sample_count=2") != std::string::npos);
    CHECK(text.find("observation_overhead_ns=17") != std::string::npos);
    CHECK(text.find("dirty_diff_hash=ba316274") != std::string::npos);
}

TEST_CASE("B1.2 controlled delivery changes transport order without changing captured identity",
          "[rpa][b1]") {
    ControlledDelivery delivery;
    const auto first = delivery.enqueue(0, 100, "press:A");
    const auto second = delivery.enqueue(1, 100, "press:B");
    const auto third = delivery.enqueue(2, 101, "release:A");

    const auto reordered = delivery.deliverInOrder({third, first, second});
    REQUIRE(reordered.size() == 3);
    CHECK(reordered[0].deliverySequence == third);
    CHECK(reordered[0].canonicalOrdinal == 2);
    CHECK(reordered[1].canonicalOrdinal == 0);
    CHECK(reordered[2].canonicalOrdinal == 1);
    CHECK(reordered[1].captureTick == 100);
    CHECK(delivery.pending().empty());
}

TEST_CASE("B1.2 fake clock exposes validity, publication and segment transitions", "[rpa][b1]") {
    FakeClock clock(1000);
    const auto first = clock.observe(50, 2);
    REQUIRE(first.valid);
    CHECK(first.segment == 1);
    CHECK(first.publicationSequence == 0);
    CHECK(first.validUntilMonotonicUs == 1050);

    REQUIRE(clock.advance(10));
    clock.invalidate();
    CHECK_FALSE(clock.observe(50, 2).valid);
    clock.startNewSegment();
    const auto next = clock.observe(10, 1);
    CHECK(next.valid);
    CHECK(next.segment == 2);
    CHECK(next.publicationSequence > first.publicationSequence);
}

TEST_CASE("B1.2 rational time oracle applies offsets once and rounds ties to even", "[rpa][b1]") {
    const auto mapped = mapTime(TimeOracleInput{.sourceFrame = 441,
                                                .sourceRate = 44100,
                                                .timingOffsetUs = 1000,
                                                .inputCompensationUs = 500,
                                                .displayLeadUs = 250});
    REQUIRE(mapped.has_value());
    CHECK(mapped->observationTick == 8500);
    CHECK(mapped->horizonTick == mapped->observationTick);
    CHECK(mapped->presentationTick == 9250);

    const auto oddTie = Rational::create(5, 2);
    const auto evenTie = Rational::create(6, 2);
    REQUIRE(oddTie.has_value());
    REQUIRE(evenTie.has_value());
    REQUIRE(roundHalfEven(*oddTie).has_value());
    REQUIRE(roundHalfEven(*evenTie).has_value());
    CHECK(*roundHalfEven(*oddTie) == 2);
    CHECK(*roundHalfEven(*evenTie) == 3);
    const auto negativeTie = Rational::create(-5, 2);
    REQUIRE(negativeTie.has_value());
    REQUIRE(roundHalfEven(*negativeTie).has_value());
    CHECK(*roundHalfEven(*negativeTie) == -2);
}

TEST_CASE("B1.2 scan oracle derives closure, execution and Fact/Fold independently", "[rpa][b1]") {
    const std::vector<OracleInput> inputs{
        {.canonicalOrdinal = 1, .originalTick = 100, .admitted = true},
        {.canonicalOrdinal = 0, .originalTick = 100, .admitted = true},
        {.canonicalOrdinal = 2, .originalTick = 101, .admitted = false}};
    const auto result = scanExecutionOracle(inputs, {100, 102, 103}, 102);

    REQUIRE(result.closedBatchTicks == std::vector<std::int64_t>{102, 103});
    REQUIRE(result.executionOrder == std::vector<std::uint64_t>{0, 1});
    REQUIRE(result.facts == std::vector<std::string>{"fact:0", "fact:1"});
    CHECK(result.folds == std::vector<std::string>{"fold:100:0", "fold:100:1"});
}

} // namespace cuexis::rpa::testing
