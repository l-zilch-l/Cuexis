#pragma once

#include "gameplay_test_fixture.hpp"
#include <cuexis/judgement/gameplay_prepare.hpp>

namespace cuexis::judgement::testing {
inline auto executionLate(LateEventPolicy mode = LateEventPolicy::queueNextTick)
    -> LatePolicyParameters {
    return {MeasuredParameter<TickSpan>::measured(TickSpan{8}),
            MeasuredParameter<TickSpan>::measured(
                TickSpan{mode == LateEventPolicy::queueNextTick ? 4 : 0}),
            MeasuredParameter<TickSpan>::measured(TickSpan{3}),
            MeasuredParameter<TickSpan>::measured(TickSpan{2}), mode};
}
inline void executionFields(AssemblyRequest& assembly) {
    assembly.graphRevision = 2;
    assembly.executionProfile = "gameplay.execution.t4-k4.v1";
    assembly.normalizationProfileToken =
        assembly.identityDeclarations.session.normalizationProfileToken;
    assembly.coordinatorPolicyToken = "coordinator.policy.greedy_v1";
    auto& engine = assembly.identityDeclarations.engine;
    engine = {"judgement.t4-k4.v1",      "fact.semantic.phase-local.v1",
              "fixed-point.none.v1",     "coordination.six-eight.t4-k4.v1",
              assembly.executionProfile, "late.window.logical.v1"};
    auto& r = assembly.sources.front().document.requirements.front();
    r.phases = {{PhaseKind::head, 1}, {PhaseKind::body, 2}, {PhaseKind::tail, 3}};
    r.requiresReleaseTailSemantics = true;
    r.measure.components.push_back({PhaseKind::tail, "hold_tail", {}});
    r.localClosePolicyToken = "local.close.t4.v1";
    r.patternArmRefs = {"atom.one"};
    r.timing = RequirementRecord::Timing{Tick{1000},
                                         {{Tick{850}, Tick{901}, {PhaseKind::head, 1}},
                                          {Tick{900}, Tick{1000}, {PhaseKind::body, 2}},
                                          {Tick{1000}, Tick{1020}, {PhaseKind::tail, 3}}},
                                         TimeInterval{Tick{900}, Tick{1000}},
                                         {{PhaseKind::head, Tick{900}},
                                          {PhaseKind::body, Tick{1000}},
                                          {PhaseKind::tail, Tick{1000}}}};
    r.atomBindings = {{"atom.one",
                       std::string{r.domainBinding.token()},
                       "keyboard",
                       "lane.one",
                       InputAction::press,
                       {},
                       false},
                      {"tail.release",
                       std::string{r.domainBinding.token()},
                       "keyboard",
                       "lane.one",
                       InputAction::release,
                       {},
                       true}};
    r.preparedGrace = PreparedGrace{TickSpan{40}};
    r.resourceClaims.front().claimPolicy.competition =
        ClaimPolicyDeclaration::CompetitionKey{-1, 7};
}
} // namespace cuexis::judgement::testing
