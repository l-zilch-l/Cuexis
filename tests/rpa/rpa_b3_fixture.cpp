#include "rpa_b3_fixture.hpp"

namespace cuexis::rpa::testing {

auto occurrence(std::int64_t t, std::uint64_t p, InputAction action, std::string channel)
    -> CanonicalInput {
    return {{ObservationTick{Tick{t}},
             "domain.binding.one",
             "keyboard",
             std::move(channel),
             action,
             {},
             p},
            IngressSequence{p}};
}
auto view(JudgementSession& s) -> KernelProjection {
    auto p = s.query();
    REQUIRE(p);
    return p->kernelView();
}
auto phase(const KernelProjection& p, PhaseKind kind) -> KernelPhaseState {
    auto it = std::find_if(p.phases.begin(), p.phases.end(),
                           [&](const auto& v) { return v.phase == kind; });
    REQUIRE(it != p.phases.end());
    return it->state;
}
auto scheduler(JudgementSession& s, RpaContext context, std::size_t capacity)
    -> ExecutionScheduler {
    auto result = ExecutionScheduler::create(s, context, TickSpan{3}, capacity);
    REQUIRE(result);
    return std::move(*result);
}
auto batch(std::uint64_t seq, std::int64_t tick, std::vector<CanonicalInput> inputs)
    -> CaptureMessage {
    const auto size = static_cast<std::uint64_t>(inputs.size());
    return {scope, seq,
            ClosedCaptureBatch{Tick{tick}, Tick{tick + 1}, seq, 0, 1, size, std::move(inputs)}};
}

auto controlRules(bool hooks) -> RulesetDeclaration {
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
    for (auto kind : {PhaseKind::head, PhaseKind::body, PhaseKind::tail}) {
        d.score.rules.push_back({kind, Outcome::hit, {}, 2, true});
        d.score.rules.push_back({kind, Outcome::miss, {}, -1, false});
    }
    if (hooks) {
        d.modules.push_back({"hook", "1", "cuexis.finite-fold.1"});
        d.hooks.push_back(
            {"fold.bonus.u64", HookConsumer::fold, CombineOperator::maximum, {"hook"}, 3, false});
    }
    return d;
}

} // namespace cuexis::rpa::testing
