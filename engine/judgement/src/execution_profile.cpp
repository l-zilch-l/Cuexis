#include <cuexis/judgement/execution_profile.hpp>

#include <algorithm>
#include <limits>
#include <set>
#include <tuple>

namespace cuexis::judgement {
namespace {
auto rejection(std::string_view suffix, std::string_view category, std::string_view path,
               std::string_view message) -> core::Error {
    return core::Error{"judgement.s7a4." + std::string{suffix}, std::string{message}}
        .withContext("category", std::string{category})
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("field.path", std::string{path});
}
auto relation(std::string_view path, std::string_view message) -> core::Error {
    return rejection("execution.relation_invalid", "invalid_relation", path, message);
}
auto incomplete(std::string_view path) -> core::Error {
    return rejection("execution.profile_incomplete", "identity_closure_incomplete", path,
                     "the execution profile requires an explicit complete field");
}
auto cell(const LatePolicyParameters& p, Tick t) -> core::Result<void> {
    for (auto span :
         {*p.windowCloseThreshold.measuredValue(), *p.finalizationWatermark.measuredValue(),
          TickSpan{1}, TickSpan{-p.windowOpenThreshold.measuredValue()->value()}}) {
        auto result = offsetTicks(t, span);
        if (!result) {
            return core::unexpected(result.error());
        }
    }
    return {};
}
auto hasPhase(const RequirementRecord& r, PhaseKind kind) -> bool {
    return std::any_of(r.phases.begin(), r.phases.end(),
                       [kind](const auto& phase) { return phase.kind == kind; });
}
} // namespace

auto validateExecutionLateParameters(const LatePolicyParameters& p) -> core::Result<void> {
    if (!p.policy || !p.windowOpenThreshold.isMeasured() || !p.windowCloseThreshold.isMeasured() ||
        !p.finalizationWatermark.isMeasured() || !p.maxQueueHop.isMeasured()) {
        return core::unexpected(rejection("late.parameters_invalid", "late_policy_incomplete",
                                          "latePolicy", "all late fields must be measured"));
    }
    const auto o = p.windowOpenThreshold.measuredValue()->value();
    const auto c = p.windowCloseThreshold.measuredValue()->value();
    const auto f = p.finalizationWatermark.measuredValue()->value();
    const auto h = p.maxQueueHop.measuredValue()->value();
    if (o < 0 || c < 0 || f <= c || (*p.policy == LateEventPolicy::rejectLate && h != 0) ||
        (*p.policy == LateEventPolicy::queueNextTick && (h < 1 || h > f - c)) ||
        (*p.policy != LateEventPolicy::rejectLate && *p.policy != LateEventPolicy::queueNextTick)) {
        return core::unexpected(rejection("late.parameters_invalid", "late_policy_incomplete",
                                          "latePolicy", "invalid O/C/F/H relationship"));
    }
    return {};
}

auto routeExecutionObservation(const LatePolicyParameters& p, Tick t, std::optional<Tick> a,
                               bool forwarded) -> core::Result<LateRoute> {
    if (auto valid = validateExecutionLateParameters(p); !valid) {
        return core::unexpected(valid.error());
    }
    if (auto valid = cell(p, t); !valid) {
        return core::unexpected(valid.error());
    }
    const auto close = offsetTicks(t, *p.windowCloseThreshold.measuredValue()).value();
    const auto final = offsetTicks(t, *p.finalizationWatermark.measuredValue()).value();
    if (!a || *a < close) {
        return LateRoute{t, forwarded};
    }
    const auto denied = [&]() -> core::Result<LateRoute> {
        return core::unexpected(rejection("late.rejected", "late_policy_incomplete", "admission",
                                          "closed, finalized or over-hop observation"));
    };
    if (*a >= final || forwarded || *p.policy == LateEventPolicy::rejectLate) {
        return denied();
    }
    const auto frontier =
        offsetTicks(*a, TickSpan{-p.windowCloseThreshold.measuredValue()->value()});
    if (!frontier) {
        return core::unexpected(frontier.error());
    }
    const auto q = offsetTicks(*frontier, TickSpan{1});
    if (!q) {
        return core::unexpected(q.error());
    }
    const auto hop = differenceTicks(*q, t);
    if (!hop) {
        return core::unexpected(hop.error());
    }
    if (hop->value() < 1 || hop->value() > p.maxQueueHop.measuredValue()->value()) {
        return denied();
    }
    return LateRoute{*q, true};
}

auto validateExecutionRequirement(const RequirementRecord& r, const PatternExecutionProgram& p)
    -> core::Result<void> {
    if (!r.timing || r.timing->phaseTargets.empty()) {
        return core::unexpected(incomplete("requirements.timing.phaseTargets"));
    }
    const bool tap = hasPhase(r, PhaseKind::tap), head = hasPhase(r, PhaseKind::head);
    const bool body = hasPhase(r, PhaseKind::body), tail = hasPhase(r, PhaseKind::tail);
    const bool observe = !r.resourceClaims.empty() &&
                         r.resourceClaims.front().intent == ResourceClaimIntent::observe;
    if (!((tap && r.phases.size() == 1) ||
          (!tap && head && body && r.phases.size() == (tail ? 3U : 2U))) ||
        r.requiresReleaseTailSemantics != tail ||
        (observe && (!tap || r.timing->body || r.preparedGrace.span().value() != 0)) ||
        r.resourceClaims.size() > 1) {
        return core::unexpected(relation("requirements.phases", "unsupported phase shape"));
    }
    std::set<PhaseKind> kinds, targets;
    std::set<std::uint32_t> ordinals;
    for (const auto& phase : r.phases) {
        if (!kinds.insert(phase.kind).second || !ordinals.insert(phase.declarationOrdinal).second) {
            return core::unexpected(relation("requirements.phases", "duplicate phase or ordinal"));
        }
    }
    const auto& timing = *r.timing;
    for (const auto& target : timing.phaseTargets) {
        if (!kinds.contains(target.phase) || !targets.insert(target.phase).second) {
            return core::unexpected(
                relation("requirements.phaseTargets", "unknown or duplicate target"));
        }
        if (target.phase == PhaseKind::body) {
            if (!timing.body || target.chartTick != timing.body->end) {
                return core::unexpected(
                    relation("requirements.phaseTargets", "body target must be b1"));
            }
        } else if (!std::any_of(timing.successWindows.begin(), timing.successWindows.end(),
                                [&](const auto& w) {
                                    return w.phase.kind == target.phase &&
                                           target.chartTick >= w.start && target.chartTick < w.end;
                                })) {
            return core::unexpected(
                relation("requirements.phaseTargets", "target is outside its windows"));
        }
        if (target.phase == PhaseKind::head &&
            (!timing.body || target.chartTick > timing.body->start)) {
            return core::unexpected(
                relation("requirements.phaseTargets", "head target is after b0"));
        }
    }
    if (targets != kinds) {
        return core::unexpected(incomplete("requirements.phaseTargets"));
    }
    if (body) {
        std::size_t count = 0;
        for (const auto& window : timing.successWindows) {
            if (window.phase.kind == PhaseKind::body) {
                ++count;
                if (!timing.body || window.start != timing.body->start ||
                    window.end != timing.body->end) {
                    return core::unexpected(
                        relation("requirements.body", "body window must be [b0,b1)"));
                }
            }
        }
        if (count != 1) {
            return core::unexpected(
                relation("requirements.body", "exactly one body window required"));
        }
        if (p.accepting[p.start]) {
            return core::unexpected(
                relation("requirements.pattern", "Hold head may not accept epsilon"));
        }
    }
    std::set<std::string> atoms;
    bool release = false;
    for (const auto& binding : r.atomBindings) {
        if (binding.atomRef.empty() || binding.domainToken.empty() || binding.sourceClass.empty() ||
            binding.channelToken.empty()) {
            return core::unexpected(incomplete("requirements.atomBindings"));
        }
        if (!atoms.insert(binding.atomRef).second ||
            binding.domainToken != r.domainBinding.token() ||
            (binding.amountRange && binding.amountRange->minimum > binding.amountRange->maximum) ||
            (binding.action != InputAction::press && binding.action != InputAction::release &&
             binding.action != InputAction::update) ||
            (binding.tailOnly && (!tail || binding.action != InputAction::release))) {
            return core::unexpected(
                relation("requirements.atomBindings", "invalid binding relation"));
        }
        const auto atom = std::find(p.atomRefs.begin(), p.atomRefs.end(), binding.atomRef);
        if (binding.tailOnly == (atom != p.atomRefs.end())) {
            return core::unexpected(
                relation("requirements.atomBindings", "atom membership and tailOnly disagree"));
        }
        release = release || binding.action == InputAction::release;
        if (head && !binding.tailOnly && binding.action != InputAction::press) {
            const auto index = static_cast<std::size_t>(atom - p.atomRefs.begin());
            for (std::size_t s = 0; s < p.accepting.size(); ++s) {
                const auto next = p.transitions[s * p.atomRefs.size() + index];
                if (p.live[s] && next && p.accepting[*next]) {
                    return core::unexpected(
                        relation("requirements.pattern", "head success must end with press"));
                }
            }
        }
    }
    for (const auto& atom : p.atomRefs) {
        if (!atoms.contains(atom)) {
            return core::unexpected(incomplete("requirements.atomBindings"));
        }
    }
    if (tail && !release) {
        return core::unexpected(incomplete("requirements.atomBindings.release"));
    }
    if (r.resourceClaims.empty()) {
        if (!r.independentCompetition) {
            return core::unexpected(incomplete("requirements.independentCompetition"));
        }
    } else if (r.independentCompetition) {
        return core::unexpected(relation("requirements.independentCompetition",
                                         "real and independent claims are exclusive"));
    }
    return {};
}

auto makeExecutionTimers(const CanonicalGameplayGraph& g)
    -> core::Result<std::vector<PreparedTimerKey>> try {
    if (g.executionProfile != "gameplay.execution.t4-k4.v1" || g.graphRevision != 2 ||
        g.normalizationProfileToken.empty() ||
        g.coordinatorPolicyToken != "coordinator.policy.greedy_v1") {
        return core::unexpected(g.executionProfile.empty()
                                    ? incomplete("executionProfile")
                                    : rejection("execution.profile_unsupported",
                                                "capability_disabled", "executionProfile",
                                                "unknown execution profile or graph constraints"));
    }
    if (!g.latePolicy) {
        return core::unexpected(incomplete("latePolicy"));
    }
    if (auto valid = validateExecutionLateParameters(*g.latePolicy); !valid) {
        return core::unexpected(valid.error());
    }
    std::vector<PreparedTimerKey> timers;
    std::set<ClaimPolicyDeclaration::CompetitionKey> independent;
    for (const auto& resource : g.resources) {
        if (resource.ref.resourceId == "@independent") {
            return core::unexpected(
                relation("resources", "reserved independent resource namespace"));
        }
    }
    for (const auto& r : g.requirements) {
        if (r.independentCompetition && !independent.insert(*r.independentCompetition).second) {
            return core::unexpected(
                relation("requirements.independentCompetition", "independent pair collision"));
        }
        const auto root = hasPhase(r, PhaseKind::tap) ? PhaseKind::tap : PhaseKind::head;
        const auto& timing = *r.timing;
        std::optional<Tick> activation;
        const auto add = [&](PhaseKind phase, PreparedTimerKind kind, Tick tick,
                             std::optional<Tick> start = {}, std::optional<Tick> end = {}) {
            timers.push_back({r.identity, phase, kind, tick, start, end});
        };
        for (const auto& w : timing.successWindows) {
            if (w.phase.kind == root && (!activation || w.start < *activation)) {
                activation = w.start;
            }
            add(w.phase.kind, PreparedTimerKind::windowOpen, w.start, w.start, w.end);
            if (w.phase.kind == PhaseKind::body) {
                continue;
            }
            auto close = w.end;
            if (w.phase.kind == PhaseKind::head) {
                auto upper = offsetTicks(timing.body->start, TickSpan{1});
                if (!upper) {
                    return core::unexpected(upper.error());
                }
                close = std::min(close, *upper);
            }
            if (close > w.start) {
                add(w.phase.kind, PreparedTimerKind::phaseClose, close, w.start, w.end);
            }
        }
        if (!activation) {
            return core::unexpected(incomplete("activation"));
        }
        add(root, PreparedTimerKind::activation, *activation);
        if (timing.body) {
            add(PhaseKind::body, PreparedTimerKind::bodyEnd, timing.body->end);
            add(PhaseKind::body, PreparedTimerKind::phaseClose, timing.body->end,
                timing.body->start, timing.body->end);
        }
        auto d = hasPhase(r, PhaseKind::tail) ? offsetTicks(timing.end, r.preparedGrace.span())
                                              : core::Result<Tick>{timing.end};
        if (!d) {
            return core::unexpected(d.error());
        }
        add(root, PreparedTimerKind::hardDeadline, *d);
    }
    for (const auto& timer : timers) {
        if (auto valid = cell(*g.latePolicy, timer.tick); !valid) {
            return core::unexpected(valid.error());
        }
    }
    std::sort(timers.begin(), timers.end(), [](const auto& a, const auto& b) {
        return std::tie(a.tick, a.kind, a.requirement, a.phase, a.windowStart, a.windowEnd) <
               std::tie(b.tick, b.kind, b.requirement, b.phase, b.windowStart, b.windowEnd);
    });
    timers.erase(std::unique(timers.begin(), timers.end()), timers.end());
    return timers;
} catch (const std::exception&) {
    return core::unexpected(
        relation("timers", "timer preparation could not allocate owning storage"));
}
} // namespace cuexis::judgement
