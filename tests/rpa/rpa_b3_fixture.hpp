#pragma once

// Shared Fixture3 and helpers for the RPA B3 case units.

#include "../../engine/core/internal/cuexis_internal/execution_diagnostics.hpp"
#include "../../engine/core/internal/cuexis_internal/sha256.hpp"
#include "../../engine/judgement/src/execution_kernel.hpp"
#include "../../engine/judgement/src/execution_log.hpp"
#include "../../engine/judgement/src/execution_testing.hpp"
#include "../../engine/judgement/src/recovery_wire.hpp"
#include "../judgement/execution_reference.hpp"
#include "../judgement/execution_test_fixture.hpp"
#include "../support/environment.hpp"
#include <algorithm>
#include <array>
#include <catch2/generators/catch_generators.hpp>
#include <chrono>
#include <cstdlib>
#include <cuexis/judgement/execution_scheduler.hpp>
#include <cuexis/judgement/recovery_codec.hpp>
#include <cuexis/playback/realtime_clock.hpp>
#include <fstream>
#include <iostream>
#include <thread>

namespace cuexis::rpa::testing {

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;

struct Fixture3 {
    Fixture base;
    AssemblyRequest assembly;
    std::optional<RulesetDeclaration> rules;
    Fixture3() : assembly(base.request()) {
        base.latePolicy = executionLate();
        executionFields(assembly);
        assembly.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
        assembly.identityDeclarations.engine.executionProfileToken = assembly.executionProfile;
        assembly.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
        assembly.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    }
    auto prepared() -> PreparedGameplay {
        auto p = prepareResolvedGameplay({assembly, {}});
        INFO((p ? "" : std::string{p.error().message()}));
        REQUIRE(p);
        return *p;
    }
    auto configuration() -> SessionConfiguration {
        InputMappingProfile mapping{
            "mapping",
            "v1",
            *SourceClass::fromToken("keyboard"),
            {{"domain.binding.one",
              {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
        auto result = SessionConfiguration{OwnedInputMappingProfile{mapping},
                                           OwnedTimebaseProfile{base.timebase},
                                           base.latePolicy,
                                           assembly.identityDeclarations,
                                           "calibration.fixture",
                                           assembly.executionProfile,
                                           "late.window.batch.v1",
                                           "fact.semantic.rpa.v1"};
        if (rules) {
            auto preparedRules = prepareRuleset(*rules);
            REQUIRE(preparedRules);
            result.ruleset = *preparedRules;
        }
        return result;
    }
    auto session() -> JudgementSession {
        auto s = JudgementSession::create();
        REQUIRE(s);
        REQUIRE(s->configure(configuration()));
        REQUIRE(s->prepare(prepared()));
        return std::move(*s);
    }
};

auto occurrence(std::int64_t t, std::uint64_t p, InputAction action,
                std::string channel = "lane.one") -> CanonicalInput;
auto view(JudgementSession& s) -> KernelProjection;
auto phase(const KernelProjection& p, PhaseKind kind) -> KernelPhaseState;
constexpr RpaContext scope{1, 1, 1};
auto scheduler(JudgementSession& s, RpaContext context, std::size_t capacity) -> ExecutionScheduler;
auto batch(std::uint64_t seq, std::int64_t tick, std::vector<CanonicalInput> inputs)
    -> CaptureMessage;
auto controlRules(bool hooks = false) -> RulesetDeclaration;

} // namespace cuexis::rpa::testing
