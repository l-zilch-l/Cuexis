#include "rpa_b2_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

namespace cuexis::rpa::testing {
namespace {

auto record(std::uint64_t id, std::uint64_t ordinal, std::int64_t tick) -> ReplayRecord {
    return ReplayRecord{.id = id,
                        .canonicalOrdinals = {ordinal},
                        .admissions = {BatchAdmission{.canonicalOrdinal = ordinal,
                                                      .accepted = true,
                                                      .dispatchTick = tick,
                                                      .forwarded = true,
                                                      .frontier = tick,
                                                      .horizon = tick}},
                        .horizon = tick,
                        .result = KernelProjection{.score = static_cast<std::int64_t>(id * 10),
                                                   .combo = id,
                                                   .facts = {"fact:" + std::to_string(ordinal)},
                                                   .folds = {"fold:" + std::to_string(tick) + ":" +
                                                             std::to_string(ordinal)}}};
}

auto payload() -> EvaluationPayload {
    return EvaluationPayload{
        .projection = KernelProjection{.score = 30,
                                       .combo = 3,
                                       .facts = {"fact:0", "fact:1", "fact:2"},
                                       .folds = {"fold:100:0", "fold:101:1", "fold:102:2"}},
        .fault = FaultPrefix{.stage = "fold",
                             .code = "fold.injected_failure",
                             .category = "error",
                             .severity = 2,
                             .faulted = true,
                             .recordIds = {1, 2, 3}}};
}

auto expectedEvaluation(std::uint64_t archiveNumber, std::uint64_t cut, std::int64_t horizon,
                        std::string profile) -> ReplayEvaluation {
    return ReplayEvaluation{.archiveIdentity = "archive:" + std::to_string(archiveNumber),
                            .profileIdentity = std::move(profile),
                            .factSemantic = "fact.semantic.rpa.v1",
                            .eventCodecId = "canonical.discrete.rpa.le.v1",
                            .stateSchemaRevision = 2,
                            .cut = cut,
                            .horizon = horizon,
                            .eventCount = 3,
                            .records = {record(1, 0, 100), record(2, 1, 101), record(3, 2, 102)},
                            .projection = payload().projection,
                            .fault = payload().fault};
}

} // namespace

TEST_CASE("B2 stores an immutable prefix while the active tail continues", "[rpa][b2]") {
    B2Store store(2, "gameplay.execution.t4-k4.rpa.v1;late=window.batch.v1");
    REQUIRE(store.append(record(1, 0, 100)));
    REQUIRE(store.append(record(2, 1, 101)));
    auto first = store.publish(2, 101, payload());
    REQUIRE(first.has_value());
    REQUIRE(store.append(record(3, 2, 102)));

    CHECK(first->evaluation().records.size() == 2);
    CHECK(first->evaluation().records.back().id == 2);
    CHECK(store.activeRecordCount() == 3);
    CHECK(store.metrics().copiedRecordCount == 2);
}

TEST_CASE("B2 publish cut excludes an existing active tail", "[rpa][b2]") {
    B2Store store(2, "profile-rpa");
    REQUIRE(store.append(record(1, 0, 100)));
    REQUIRE(store.append(record(2, 1, 101)));
    REQUIRE(store.append(record(3, 2, 102)));
    auto prefix = store.publish(2, 101, EvaluationPayload{.projection = KernelProjection{}});
    REQUIRE(prefix.has_value());
    REQUIRE(prefix->evaluation().records.size() == 2);
    CHECK(prefix->evaluation().records.back().id == 2);
    CHECK(store.activeRecordCount() == 3);
}

TEST_CASE("B2 compares complete replay evaluation and fault prefix fields", "[rpa][b2]") {
    B2Store store(4, "gameplay.execution.t4-k4.rpa.v1;late=window.batch.v1");
    REQUIRE(store.append(record(1, 0, 100)));
    REQUIRE(store.append(record(2, 1, 101)));
    REQUIRE(store.append(record(3, 2, 102)));
    auto result = store.publish(3, 102, payload());
    REQUIRE(result.has_value());

    const auto expected = expectedEvaluation(1, 3, 102, result->evaluation().profileIdentity);
    CHECK(compareCompleteReplay(expected, result->evaluation()));
    CHECK(compareFaultPrefix(expected.fault, result->evaluation().fault));

    auto altered = result->evaluation();
    altered.records[1].admissions[0].frontier = 999;
    CHECK_FALSE(compareCompleteReplay(expected, altered));
}

TEST_CASE("B2 checkpoint identity keeps archive, cut and horizon distinct", "[rpa][b2]") {
    B2Store store(2, "profile-rpa");
    REQUIRE(store.append(record(1, 0, 100)));
    auto result =
        store.publish(1, 100, EvaluationPayload{.projection = record(1, 0, 100).result.value()});
    REQUIRE(result.has_value());
    const auto handle = store.handleFor(*result);
    const auto first = store.checkpoint(handle, 1, 100);
    REQUIRE(first.has_value());
    const auto second = store.checkpoint(handle, 2, 100);
    REQUIRE(second.has_value());
    const auto third = store.checkpoint(handle, 1, 101);
    REQUIRE(third.has_value());
    CHECK_FALSE(*first == *second);
    CHECK_FALSE(*first == *third);
    CHECK(first->profileIdentity == "profile-rpa");
}

TEST_CASE("B2 reserve failure is atomic and retry is explicit", "[rpa][b2]") {
    B2Store store(2, "profile-rpa");
    REQUIRE(store.append(record(1, 0, 100)));
    store.failNextReserve();
    CHECK_FALSE(store.append(record(2, 1, 101)));
    REQUIRE(store.lastFailure().has_value());
    CHECK(store.lastFailure()->stage == "append");
    CHECK(store.lastFailure()->position == 1);
    CHECK(store.lastFailure()->rolledBack);
    CHECK(store.activeRecordCount() == 1);
    REQUIRE(store.append(record(2, 1, 101)));
    CHECK(store.activeRecordCount() == 2);
    CHECK(store.lastFailure()->retried);
}

TEST_CASE("B2 publish reserve failure is atomic", "[rpa][b2]") {
    B2Store store(2, "profile-rpa");
    REQUIRE(store.append(record(1, 0, 100)));
    store.failNextReserve();
    CHECK_FALSE(store.publish(1, 100, EvaluationPayload{.projection = KernelProjection{}}));
    REQUIRE(store.lastFailure().has_value());
    CHECK(store.lastFailure()->stage == "publish");
    CHECK(store.activeRecordCount() == 1);
}

TEST_CASE("B2 allocation failure is recorded before publication", "[rpa][b2]") {
    B2Store store(2, "profile-rpa");
    REQUIRE(store.append(record(1, 0, 100)));
    store.failNextAllocation();
    CHECK_FALSE(store.publish(1, 100, EvaluationPayload{.projection = KernelProjection{}}));
    REQUIRE(store.lastFailure().has_value());
    CHECK(store.lastFailure()->stage == "allocation");
    CHECK(store.activeRecordCount() == 1);
}

TEST_CASE("B2 old handles are rejected after Close but owning results remain readable",
          "[rpa][b2]") {
    B2Store store(2, "profile-rpa");
    REQUIRE(store.append(record(1, 0, 100)));
    auto result =
        store.publish(1, 100, EvaluationPayload{.projection = record(1, 0, 100).result.value()});
    REQUIRE(result.has_value());
    const auto handle = store.handleFor(*result);
    store.close();

    CHECK(store.isClosed());
    CHECK(store.read(handle).status == HandleReadStatus::closed);
    CHECK(result->valid());
    CHECK(result->evaluation().records.size() == 1);
    CHECK(store
              .restore(CheckpointKey{.archiveNumber = 1,
                                     .cut = 1,
                                     .horizon = 100,
                                     .profileIdentity = "profile-rpa"},
                       *result)
              .has_value());
}

TEST_CASE("B2 owning results survive store destruction", "[rpa][b2]") {
    OwningReplayResult result;
    {
        B2Store store(2, "profile-rpa");
        REQUIRE(store.append(record(1, 0, 100)));
        auto published = store.publish(
            1, 100, EvaluationPayload{.projection = record(1, 0, 100).result.value()});
        REQUIRE(published.has_value());
        result = *published;
    }
    CHECK(result.valid());
    CHECK(result.evaluation().archiveIdentity == "archive:1");
    CHECK(result.evaluation().records.size() == 1);
}

TEST_CASE("B2 read and restore retain explicit archive identity", "[rpa][b2]") {
    B2Store store(2, "profile-rpa");
    REQUIRE(store.append(record(1, 0, 100)));
    auto result =
        store.publish(1, 100, EvaluationPayload{.projection = record(1, 0, 100).result.value()});
    REQUIRE(result.has_value());
    const auto handle = store.handleFor(*result);
    const auto checkpoint = store.checkpoint(handle, 1, 100);
    REQUIRE(checkpoint.has_value());

    const auto read = store.read(handle);
    REQUIRE(read.evaluation.has_value());
    CHECK(read.status == HandleReadStatus::accepted);
    const auto restored = store.restore(*checkpoint, *result);
    REQUIRE(restored.has_value());
    CHECK(compareCompleteReplay(result->evaluation(), restored->evaluation()));
    CHECK(store.metrics().readCount == 1);
    CHECK(store.metrics().restoreCount == 1);
}

TEST_CASE("B2 long history records peak, copy and final release work", "[rpa][b2]") {
    B2Store store(16, "profile-rpa");
    for (std::uint64_t index = 0; index != 128; ++index) {
        REQUIRE(store.append(record(index + 1, index, static_cast<std::int64_t>(index))));
    }
    {
        auto result = store.publish(128, 127, EvaluationPayload{.projection = KernelProjection{}});
        REQUIRE(result.has_value());
        CHECK(store.metrics().peakActiveRecordCount == 128);
        CHECK(store.metrics().copiedRecordCount == 128);
        CHECK(store.metrics().releaseCount == 0);
        const auto handle = store.handleFor(*result);
        REQUIRE(store.read(handle).evaluation.has_value());
        const auto checkpoint = store.checkpoint(handle, 128, 127);
        REQUIRE(checkpoint.has_value());
        REQUIRE(store.restore(*checkpoint, *result).has_value());
    }
    CHECK(store.metrics().readCount == 1);
    CHECK(store.metrics().restoreCount == 1);
    CHECK(store.metrics().releaseCount == 1);
    CHECK(store.metrics().releaseTailOperations > 128);
}

} // namespace cuexis::rpa::testing
