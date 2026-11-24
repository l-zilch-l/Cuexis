#include <cuexis/judgement/prepared_gameplay_records.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <tuple>

namespace cuexis::judgement::runtime {
namespace {

auto scoreError(std::string message, std::string reason) -> core::Error {
    return core::Error{"judgement.execution.relation_invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason));
}

auto overflowError() -> core::Error {
    return core::Error{"judgement.timebase.tick_overflow", "score basis arithmetic overflowed"}
        .withContext("category", "budget_exceeded")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}

//  One local scoring key of one phase instance. `(phase, outcome, grade)` is exactly the key space
//  of `ScoreRuleRow`.
struct LocalKey final {
    PhaseKind phase;
    Outcome outcome;
    std::optional<std::string> grade;

    friend auto operator==(const LocalKey&, const LocalKey&) -> bool = default;
};

void addKey(std::vector<LocalKey>& keys, PhaseKind phase, Outcome outcome,
            std::optional<std::string> grade = std::nullopt) {
    const LocalKey key{phase, outcome, std::move(grade)};
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

auto targetOf(const RequirementTiming& timing, PhaseKind phase) -> const PhaseTarget* {
    for (const auto& target : timing.phaseTargets) {
        if (target.phase == phase) {
            return &target;
        }
    }
    return nullptr;
}

auto windowsOf(const RequirementTiming& timing, const PhaseDeclaration& phase)
    -> std::vector<const SuccessWindow*> {
    std::vector<const SuccessWindow*> windows;
    for (const auto& window : timing.successWindows) {
        if (window.phase == phase) {
            windows.push_back(&window);
        }
    }
    return windows;
}

auto tableOf(const PreparedRequirement& requirement, PhaseKind phase,
             const PreparedNormalizedScore& score) -> const PreparedGradeTable* {
    for (const auto& component : requirement.measure) {
        if (component.phase != phase) {
            continue;
        }
        if (!component.gradeTableIndex) {
            return nullptr;
        }
        if (*component.gradeTableIndex >= score.gradeTables.size()) {
            return nullptr;
        }
        return &score.gradeTables[*component.gradeTableIndex];
    }
    return nullptr;
}

auto overlaps(std::int64_t leftStart, std::int64_t leftEnd, std::int64_t rightStart,
              std::int64_t rightEnd) noexcept -> bool {
    return leftStart < rightEnd && rightStart < leftEnd;
}

auto measuredTick(const MeasuredParameter<TickSpan>& parameter) noexcept
    -> std::optional<std::int64_t> {
    const auto* value = parameter.measuredValue();
    return value == nullptr ? std::nullopt : std::optional<std::int64_t>{value->value()};
}

auto addGradedKeys(std::vector<LocalKey>& keys, PhaseKind phase, Outcome outcome,
                   const PreparedGradeTable& table, std::int64_t target, std::int64_t sourceStart,
                   std::int64_t sourceEnd) -> core::Result<void> {
    for (const auto& interval : table.intervals) {
        if (interval.start > 0 &&
            target > std::numeric_limits<std::int64_t>::max() - interval.start) {
            return core::unexpected(overflowError());
        }
        if (interval.start < 0 &&
            target < std::numeric_limits<std::int64_t>::min() - interval.start) {
            return core::unexpected(overflowError());
        }
        if (interval.end > 0 && target > std::numeric_limits<std::int64_t>::max() - interval.end) {
            return core::unexpected(overflowError());
        }
        if (interval.end < 0 && target < std::numeric_limits<std::int64_t>::min() - interval.end) {
            return core::unexpected(overflowError());
        }
        const std::int64_t start = target + interval.start;
        const std::int64_t end = target + interval.end;
        if (overlaps(start, end, sourceStart, sourceEnd)) {
            addKey(keys, phase, outcome, interval.grade);
        }
    }
    return {};
}

//  The local keys of one phase instance. The windows are half-open tick extents; the domains are
//  built only where the phase can actually produce that outcome, which is why an empty window list
//  never manufactures a hit key.
auto collectLocalKeys(const PreparedRequirement& requirement, const PhaseDeclaration& phase,
                      const PreparedNormalizedScore& score, std::int64_t hop)
    -> core::Result<std::vector<LocalKey>> {
    std::vector<LocalKey> keys;
    const auto* target = targetOf(requirement.timing, phase.kind);
    if (target == nullptr) {
        return core::unexpected(scoreError("a declared phase has no phase target", "phase_target"));
    }
    const auto windows = windowsOf(requirement.timing, phase);
    const auto* table = tableOf(requirement, phase.kind, score);
    const std::int64_t targetTick = target->chartTick.value();

    switch (phase.kind) {
    case PhaseKind::tap:
    case PhaseKind::head: {
        addKey(keys, phase.kind, Outcome::miss);
        if (table != nullptr) {
            for (const auto* window : windows) {
                auto added = addGradedKeys(keys, phase.kind, Outcome::hit, *table, targetTick,
                                           window->start.value(), window->end.value());
                if (!added) {
                    return core::unexpected(added.error());
                }
            }
        } else {
            for (const auto* window : windows) {
                if (window->start < window->end) {
                    addKey(keys, phase.kind, Outcome::hit);
                    break;
                }
            }
        }
        break;
    }
    case PhaseKind::body: {
        addKey(keys, phase.kind, Outcome::hit);
        addKey(keys, phase.kind, Outcome::miss);
        if (table != nullptr) {
            if (!requirement.timing.body) {
                return core::unexpected(
                    scoreError("a body phase has no body interval", "body_interval"));
            }
            //  The body release dispatch starts at the earliest window from which a head can be
            //  acquired; the conservative envelope subtracts the queue hop once.
            std::int64_t dispatchStart = requirement.timing.body->start.value();
            if (const auto* head = targetOf(requirement.timing, PhaseKind::head)) {
                dispatchStart = head->chartTick.value();
                for (const auto& window : requirement.timing.successWindows) {
                    if (window.phase.kind == PhaseKind::head) {
                        dispatchStart = std::min(dispatchStart, window.start.value());
                    }
                }
            }
            const auto observation =
                releaseObservationDomain(dispatchStart, requirement.timing.body->end.value(), hop);
            if (observation) {
                auto added = addGradedKeys(keys, phase.kind, Outcome::miss, *table, targetTick,
                                           observation->start, observation->end);
                if (!added) {
                    return core::unexpected(added.error());
                }
            }
        }
        break;
    }
    case PhaseKind::tail: {
        addKey(keys, phase.kind, Outcome::miss);
        if (table != nullptr) {
            for (const auto* window : windows) {
                auto added = addGradedKeys(keys, phase.kind, Outcome::hit, *table, targetTick,
                                           window->start.value(), window->end.value());
                if (!added) {
                    return core::unexpected(added.error());
                }
            }
        } else {
            for (const auto* window : windows) {
                if (window->start < window->end) {
                    addKey(keys, phase.kind, Outcome::hit);
                    break;
                }
            }
        }
        if (table != nullptr) {
            std::int64_t dispatchStart =
                requirement.timing.body ? requirement.timing.body->end.value() : targetTick;
            std::optional<std::int64_t> lastTailWindowEnd;
            for (const auto& window : requirement.timing.successWindows) {
                if (window.phase.kind != PhaseKind::tail) {
                    continue;
                }
                lastTailWindowEnd = lastTailWindowEnd
                                        ? std::max(*lastTailWindowEnd, window.end.value())
                                        : window.end.value();
            }
            if (lastTailWindowEnd) {
                const std::int64_t dispatchEnd =
                    std::min(requirement.deadline.value(), *lastTailWindowEnd);
                const auto observation = releaseObservationDomain(dispatchStart, dispatchEnd, hop);
                if (observation) {
                    auto added = addGradedKeys(keys, phase.kind, Outcome::miss, *table, targetTick,
                                               observation->start, observation->end);
                    if (!added) {
                        return core::unexpected(added.error());
                    }
                }
            }
        }
        break;
    }
    }
    return keys;
}

auto findRule(const ScoreMode& mode, const LocalKey& key) -> const ScoreRuleRow* {
    for (const auto& rule : mode.rules) {
        if (rule.phase == key.phase && rule.outcome == key.outcome && rule.grade == key.grade) {
            return &rule;
        }
    }
    return nullptr;
}

auto isObserveOnly(const PreparedRequirement& requirement) noexcept -> bool {
    return !requirement.resourceClaims.empty() &&
           requirement.resourceClaims.front().intent == ResourceClaimIntent::observe;
}

} // namespace

auto releaseObservationDomain(std::int64_t dispatchStart, std::int64_t dispatchEnd,
                              std::int64_t hop) -> std::optional<ObservationDomain> {
    if (dispatchStart >= dispatchEnd) {
        return std::nullopt;
    }
    std::int64_t start = dispatchStart;
    if (hop > 0) {
        if (dispatchStart < std::numeric_limits<std::int64_t>::min() + hop) {
            start = std::numeric_limits<std::int64_t>::min();
        } else {
            start = dispatchStart - hop;
        }
    } else if (hop < 0) {
        if (dispatchStart > std::numeric_limits<std::int64_t>::max() + hop) {
            start = std::numeric_limits<std::int64_t>::max();
        } else {
            start = dispatchStart - hop;
        }
    }
    return ObservationDomain{start, dispatchEnd};
}

auto deriveScoreBasis(std::span<const PreparedRequirement> requirements,
                      const LatePolicyParameters& policy, const PreparedNormalizedScore& score)
    -> core::Result<ScoreBasis> {
    auto validated = validatePreparedNormalizedScore(score);
    if (!validated) {
        return core::unexpected(validated.error());
    }
    const auto hopValue = measuredTick(policy.maxQueueHop);
    if (!policy.policy || !hopValue) {
        return core::unexpected(
            scoreError("the late policy is incomplete for a score-domain derivation",
                       "late_policy_incomplete"));
    }
    const std::int64_t hop =
        *policy.policy == LateEventPolicy::queueNextTick ? *hopValue : INT64_C(0);

    //  Expansion order: requirement order, then the phase declaration order inside the requirement.
    //  The unit indices are indices into exactly this expansion.
    struct ExpandedPhase final {
        const PreparedRequirement* requirement;
        const PhaseDeclaration* phase;
    };
    std::vector<ExpandedPhase> phases;
    for (const auto& requirement : requirements) {
        for (const auto& phase : requirement.phases) {
            phases.push_back(ExpandedPhase{&requirement, &phase});
        }
    }
    if (phases.size() > std::numeric_limits<std::uint32_t>::max()) {
        return core::unexpected(overflowError());
    }

    std::uint64_t jMax = 0;
    std::uint32_t qualifyingUnits = 0;
    std::vector<bool> inUnit(phases.size(), false);
    for (std::size_t unitIndex = 0; unitIndex < score.units.size(); ++unitIndex) {
        const auto& unit = score.units[unitIndex];
        std::uint64_t unitMax = 0;
        bool unitQualifies = false;
        bool unitIncrements = false;
        for (const std::uint32_t index : unit.phaseIndices) {
            if (index >= phases.size()) {
                return core::unexpected(
                    scoreError("a scoring unit names a phase index outside the expansion",
                               "score_unit_range"));
            }
            if (inUnit[index]) {
                return core::unexpected(
                    scoreError("a phase instance appears in more than one scoring unit",
                               "score_unit_duplicate"));
            }
            inUnit[index] = true;
            const auto& expanded = phases[index];
            if (isObserveOnly(*expanded.requirement)) {
                return core::unexpected(
                    scoreError("an ObserveOnly requirement cannot join a scoring unit",
                               "score_observe_only_unit"));
            }
            auto keys = collectLocalKeys(*expanded.requirement, *expanded.phase, score, hop);
            if (!keys) {
                return core::unexpected(keys.error());
            }
            if (keys->empty()) {
                continue;
            }
            for (const auto& key : *keys) {
                const auto* rule = findRule(score.mode, key);
                if (rule == nullptr) {
                    return core::unexpected(scoreError(
                        "a local scoring key has no exact score rule", "score_rule_missing"));
                }
                if (!rule->qualifies) {
                    continue;
                }
                unitMax = std::max(unitMax, rule->judgePoints);
                unitQualifies = true;
                if (rule->combo == ComboAction::increment) {
                    unitIncrements = true;
                }
            }
        }
        if (!unitQualifies) {
            return core::unexpected(
                scoreError("a scoring unit has no local qualified key", "score_unit_unqualified"));
        }
        if (unitIncrements) {
            ++qualifyingUnits;
        }
        if (jMax > std::numeric_limits<std::uint64_t>::max() - unitMax) {
            return core::unexpected(overflowError());
        }
        jMax += unitMax;
    }
    auto cMax = deriveComboBasis(score.mode, qualifyingUnits);
    if (!cMax) {
        return core::unexpected(cMax.error());
    }
    return ScoreBasis{jMax, *cMax};
}

} // namespace cuexis::judgement::runtime
