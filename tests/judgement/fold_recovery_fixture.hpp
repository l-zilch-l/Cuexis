#pragma once

// Fixtures shared by the Fold recovery test units: the ruleset declaration, the Fold fixture
// and the oracle helpers.

#include "../../engine/judgement/src/diagnostic_policy.hpp"
#include "../../engine/judgement/src/execution_kernel.hpp"
#include "../../engine/judgement/src/execution_testing.hpp"
#include "../../engine/judgement/src/recovery_wire.hpp"
#include "execution_reference.hpp"
#include "execution_test_fixture.hpp"
#include <catch2/matchers/catch_matchers.hpp>
#include <cuexis/judgement/recovery_codec.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;

namespace fold_fixture {

inline auto declaration(bool hooks = false) -> RulesetDeclaration {
    RulesetDeclaration d{"cuexis.ruleset.finite",
                         "1",
                         "cuexis.finite-fold.1",
                         "loadout.fixture",
                         ProgramPolicy::locked,
                         OutcomeScope::declared,
                         {{"score", "1", "cuexis.finite-fold.1"},
                          {"combo", "1", "cuexis.finite-fold.1"},
                          {"statistics", "1", "cuexis.finite-fold.1"}},
                         {0, INT64_MIN, INT64_MAX, ArithmeticPolicy::checked, 0, {}},
                         {},
                         false,
                         false};
    for (auto phase : {PhaseKind::tap, PhaseKind::head, PhaseKind::body, PhaseKind::tail}) {
        d.score.rules.push_back({phase, Outcome::hit, {}, 2, true});
        d.score.rules.push_back({phase, Outcome::miss, {}, -1, false});
    }
    if (hooks) {
        d.modules.push_back({"hook", "1", "cuexis.finite-fold.1"});
        d.hooks.push_back(
            {"fold.bonus.u64", HookConsumer::fold, CombineOperator::maximum, {"hook"}, 3, false});
    }
    return d;
}
struct FoldFixture final {
    Fixture base;
    AssemblyRequest assembly;
    RulesetDeclaration rules;
    FoldFixture(bool hooks = false) : assembly(base.request()), rules(declaration(hooks)) {
        base.latePolicy = executionLate();
        executionFields(assembly);
    }
    auto configuration() -> SessionConfiguration {
        InputMappingProfile m{"mapping",
                              "v1",
                              *SourceClass::fromToken("keyboard"),
                              {{"domain.binding.one",
                                {makeDuration(2, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
        auto r = prepareRuleset(rules);
        REQUIRE(r);
        return {OwnedInputMappingProfile{m},
                OwnedTimebaseProfile{base.timebase},
                base.latePolicy,
                assembly.identityDeclarations,
                "calibration.fixture",
                assembly.executionProfile,
                "late.window.logical.v1",
                "fact.semantic.phase-local.v1",
                *r};
    }
    auto prepared() -> PreparedGameplay {
        auto p = prepareResolvedGameplay({assembly, {}});
        REQUIRE(p);
        return *p;
    }
    auto inputs() -> RecoveryInputs {
        return {configuration(), prepared()};
    }
    auto session() -> JudgementSession {
        auto s = JudgementSession::create();
        REQUIRE(s);
        REQUIRE(s->configure(configuration()));
        REQUIRE(s->prepare(prepared()));
        return std::move(*s);
    }
    void tap() {
        auto& r = assembly.sources[0].document.requirements[0];
        r.phases = {{PhaseKind::tap, 1}};
        r.requiresReleaseTailSemantics = false;
        r.measure.components = {{PhaseKind::tap, "tap", {}, {}}};
        r.timing = RequirementRecord::Timing{
            Tick{8}, {{Tick{0}, Tick{8}, {PhaseKind::tap, 1}}}, {}, {{PhaseKind::tap, Tick{5}}}};
        r.preparedGrace = PreparedGrace{TickSpan{0}};
        r.atomBindings.resize(1);
    }
};
inline auto raw(std::int64_t tick, std::uint64_t sequence, InputAction action = InputAction::press,
                std::optional<RationalBeat> amount = {}) -> ClockedIngress {
    return {ObservationTick{Tick{tick}},
            {IngressSequence{sequence},
             action,
             *ChannelRef::fromToken("lane.one"),
             "domain.binding.one",
             {},
             {false, false, false},
             ContinuityKind::discrete,
             amount}};
}
inline auto canonical(std::int64_t tick, std::uint64_t sequence,
                      std::optional<std::int64_t> amount = {}) -> CanonicalInput {
    return {{ObservationTick{Tick{tick}}, "domain.binding.one", "keyboard", "lane.one",
             InputAction::press, amount},
            IngressSequence{sequence}};
}
// Independent Fold oracle: consumes a canonical Fact stream, uses explicit fixture constants,
// and never calls production sorting, reducer, checkedScore or transaction helpers.
inline auto oracle(const auto& facts) -> std::array<std::int64_t, 5> {
    std::array<std::int64_t, 5> out{};
    for (const auto& fact : facts) {
        if (const auto* f = std::get_if<PhaseOutcomeFact>(&fact)) {
            if (f->outcome == Outcome::hit) {
                out[0] += 2;
                ++out[1];
                ++out[3];
            } else {
                --out[0];
                out[1] = 0;
                ++out[4];
            }
            out[2] = std::max(out[2], out[1]);
        }
    }
    return out;
}
inline void expectOracle(const KernelProjection& p) {
    REQUIRE(p.fold);
    auto o = oracle(p.facts);
    CHECK(p.fold->score == o[0]);
    CHECK(p.fold->combo == static_cast<std::uint64_t>(o[1]));
    CHECK(p.fold->maxCombo == static_cast<std::uint64_t>(o[2]));
    CHECK(p.fold->hits == static_cast<std::uint64_t>(o[3]));
    CHECK(p.fold->misses == static_cast<std::uint64_t>(o[4]));
    for (const auto& count : p.fold->counts) {
        std::uint64_t expected = 0;
        for (const auto& fact : p.facts) {
            const auto* phase = std::get_if<PhaseOutcomeFact>(&fact);
            if (phase && phase->phase == count.phase && phase->outcome == count.outcome &&
                phase->grade == count.grade) {
                ++expected;
            }
        }
        CHECK(count.count == expected);
    }
    std::uint64_t stray = 0, consumeEmpty = 0;
    for (const auto& fact : p.facts) {
        if (const auto* receipt = std::get_if<ReceiptFact>(&fact)) {
            if (receipt->kind == FactKind::stray) {
                ++stray;
            } else {
                ++consumeEmpty;
            }
        }
    }
    CHECK(p.fold->strayCount == stray);
    CHECK(p.fold->consumeEmptyCount == consumeEmpty);
}

} // namespace fold_fixture
