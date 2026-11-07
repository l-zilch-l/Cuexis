#pragma once

// Shared kernel fixture for the execution kernel case units.

#include "../../engine/judgement/src/execution_testing.hpp"
#include "../../engine/judgement/src/pattern_dfa.hpp"
#include "execution_reference.hpp"
#include "execution_test_fixture.hpp"
#include <algorithm>
#include <cuexis/judgement/judgement_session.hpp>
#include <sstream>

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;

namespace kernel_fixture {

struct KernelFixture {
    Fixture base;
    AssemblyRequest assembly;
    KernelFixture() : assembly(base.request()) {
        base.latePolicy = executionLate();
        executionFields(assembly);
    }
    auto prepared() -> PreparedGameplay {
        auto result = prepareResolvedGameplay({assembly, {}});
        INFO((result ? "" : std::string{result.error().message()}));
        REQUIRE(result);
        return *result;
    }
    auto configuration() -> SessionConfiguration {
        InputMappingProfile mapping{
            "mapping",
            "v1",
            *SourceClass::fromToken("keyboard"),
            {{"domain.binding.one",
              {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
        return {OwnedInputMappingProfile{mapping},
                OwnedTimebaseProfile{base.timebase},
                base.latePolicy,
                assembly.identityDeclarations,
                "calibration.fixture",
                assembly.executionProfile,
                "late.window.logical.v1",
                "fact.semantic.phase-local.v1"};
    }
    auto session() -> JudgementSession {
        auto result = JudgementSession::create();
        REQUIRE(result);
        REQUIRE(result->configure(configuration()));
        REQUIRE(result->prepare(prepared()));
        return std::move(*result);
    }
};
inline auto input(std::int64_t tick, std::uint64_t sequence, InputAction action) -> ClockedIngress {
    return {ObservationTick{Tick{tick}},
            {IngressSequence{sequence},
             action,
             *ChannelRef::fromToken("lane.one"),
             "domain.binding.one",
             {0, 0, 0, 0},
             {false, false, false},
             ContinuityKind::discrete,
             {}}};
}
inline auto state(const KernelProjection& p, PhaseKind phase) -> KernelPhaseState {
    auto found = std::find_if(p.phases.begin(), p.phases.end(),
                              [phase](const auto& v) { return v.phase == phase; });
    REQUIRE(found != p.phases.end());
    return found->state;
}
inline void tap(RequirementRecord& r) {
    r.phases = {{PhaseKind::tap, 1}};
    r.requiresReleaseTailSemantics = false;
    r.timing = RequirementRecord::Timing{Tick{940},
                                         {{Tick{850}, Tick{920}, {PhaseKind::tap, 1}}},
                                         {},
                                         {{PhaseKind::tap, Tick{900}}}};
    r.measure.components = {{PhaseKind::tap, "tap", {}}};
    r.atomBindings.resize(1);
    r.preparedGrace = PreparedGrace{TickSpan{0}};
}

} // namespace kernel_fixture

using namespace kernel_fixture;
