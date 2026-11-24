// T-10 third round: the legacy prepared -> runtime view bridge (judgement_compile).
//
// The bridge is the compile-side input for the runtime kernel switch. It must copy every runtime
// record, preserve the legacy prepared/content identity bytes, and keep the view alive after the
// legacy prepared handle dies. It intentionally carries no normalized score (the legacy path has
// none); the test asserts that absence instead of inventing a basis.

#include <cuexis/judgement/compile/legacy_prepared_view.hpp>

#include <catch2/catch_test_macros.hpp>

#include "fold_recovery_fixture.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace {

using namespace cuexis::judgement;

} // namespace

TEST_CASE("cuexis_legacy_prepared_view", "[judgement][compile]") {
    fold_fixture::FoldFixture fixture;
    const auto legacy = fixture.prepared();
    const auto expectedPreparedIdentity = legacy.assembled().prepared.canonicalBytes().toHex();
    const auto expectedContentIdentity = legacy.assembled().content.canonicalBytes().toHex();

    std::optional<runtime::PreparedGameplay> converted;
    {
        const auto result = compile::preparedGameplayView(legacy);
        REQUIRE(result);
        converted = std::move(*result);
    }
    //  The legacy handle stays alive above, but the bridge already owns everything it exposes.
    CHECK(converted->requirements().size() == legacy.requirements().size());
    CHECK(converted->resources().size() == legacy.resources().size());
    CHECK(converted->timers().size() == legacy.timers().size());
    CHECK(converted->identity() == expectedPreparedIdentity);
    CHECK(converted->records().contentIdentity == expectedContentIdentity);
    CHECK(converted->records().executionProfile == legacy.assembled().graph.executionProfile);
    CHECK(converted->records().normalizedScore == nullptr);
    CHECK(converted->records().ruleset == nullptr);
    CHECK(converted->records().latePolicy.maxQueueHop.isMeasured());
    CHECK(converted->records().latePolicy.policy.has_value());

    std::size_t expectedCapacity = 0;
    for (std::size_t index = 0; index < legacy.requirements().size(); ++index) {
        const auto& source = legacy.assembled().graph.requirements[index];
        const auto& target = converted->requirements()[index];
        expectedCapacity += source.phases.size();
        CHECK(target.stableId == source.stableId);
        CHECK(target.identity == source.identity);
        CHECK(target.phases == source.phases);
        REQUIRE(source.timing.has_value());
        CHECK(target.timing.end == source.timing->end);
        CHECK(target.timing.successWindows.size() == source.timing->successWindows.size());
        CHECK(target.timing.phaseTargets.size() == source.timing->phaseTargets.size());
        for (std::size_t targetIndex = 0; targetIndex < target.timing.phaseTargets.size();
             ++targetIndex) {
            CHECK(target.timing.phaseTargets[targetIndex].phase ==
                  source.timing->phaseTargets[targetIndex].phase);
            CHECK(target.timing.phaseTargets[targetIndex].chartTick ==
                  source.timing->phaseTargets[targetIndex].chartTick);
        }
        CHECK(target.deadline == legacy.requirements()[index].deadline);
        CHECK(target.executionProgram.has_value() ==
              legacy.requirements()[index].executionProgram.has_value());
        CHECK(target.atomBindings.size() == source.atomBindings.size());
        CHECK(target.resourceClaims.size() == source.resourceClaims.size());
    }
    CHECK(converted->settlementCapacity() == expectedCapacity);
    CHECK(converted->settlementDictionary().phases.size() == expectedCapacity);
    CHECK(converted->settlementDictionary().grades.empty());
}
