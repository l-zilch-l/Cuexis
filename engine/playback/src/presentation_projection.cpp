#include "presentation_projection.hpp"

#include <algorithm>

namespace cuexis::playback::detail {
namespace {
auto invalid(std::string_view message) -> core::Error {
    return core::Error{"playback.gameplay.invalid", std::string{message}}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}
template <typename Identity>
auto sourceKey(const Identity& s, GameplayPhase phase, GameplayOutcome outcome,
               GameplayTimingClass timing) -> ProjectionSourceKey {
    std::vector<std::pair<std::string, std::uint64_t>> path;
    path.reserve(s.emissionPath.size());
    for (const auto& step : s.emissionPath)
        path.emplace_back(step.nodeId, step.repeatIndex);
    return {s.chartEntryId,  s.invocationId, s.moduleId, s.exportId, s.requirementLocalId,
            std::move(path), phase,          outcome,    timing};
}
auto rowKey(const GameplayFactBinding& b) {
    return std::tuple{b.bindingId,   b.target,
                      b.start.value, b.end ? std::optional{b.end->value} : std::nullopt,
                      b.visible,     sourceKey(b.source, b.phase, b.outcome, b.timing),
                      b.aggregation, b.groupMembers};
}
} // namespace

auto PresentationProjectionAdapter::prepare(std::span<const GameplayFactBinding> bindings,
                                            std::map<std::string, std::size_t, std::less<>> targets)
    -> core::Result<PreparedProjection> try {
    PreparedProjection prepared;
    prepared.targets = std::move(targets);
    struct Declaration final {
        GameplayFactBinding binding;
        std::size_t order;
    };
    std::vector<Declaration> sorted;
    sorted.reserve(bindings.size());
    for (std::size_t i = 0; i < bindings.size(); ++i)
        sorted.push_back({bindings[i], i});
    for (auto& declaration : sorted) {
        auto& b = declaration.binding;
        if (b.phase > GameplayPhase::Tail || b.outcome > GameplayOutcome::Miss ||
            b.timing > GameplayTimingClass::Late ||
            b.aggregation > GameplayAggregation::GroupCommit)
            return core::unexpected(invalid("Invalid FactBinding declaration"));
        std::sort(b.groupMembers.begin(), b.groupMembers.end());
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        const auto left = rowKey(a.binding), right = rowKey(b.binding);
        return left != right ? left < right : a.order < b.order;
    });
    // Exact duplicates collapse; disjoint windows and aliases remain independent rows.
    sorted.erase(std::unique(sorted.begin(), sorted.end(),
                             [](const auto& a, const auto& b) {
                                 return rowKey(a.binding) == rowKey(b.binding);
                             }),
                 sorted.end());
    for (const auto& [b, order] : sorted)
        prepared.members.try_emplace(b.bindingId, prepared.members.size());
    prepared.memberGroups.resize(prepared.members.size());
    std::map<std::vector<std::string>, std::size_t> groups;
    for (const auto& [b, order] : sorted)
        if (!b.groupMembers.empty())
            groups.try_emplace(b.groupMembers, 0);
    for (auto& [names, slot] : groups) {
        slot = prepared.groups.size();
        PreparedProjection::Group group{names, {}};
        for (const auto& name : names) {
            auto member = prepared.members.find(name);
            if (member == prepared.members.end())
                return core::unexpected(invalid("FactBinding group membership closure incomplete"));
            group.members.push_back(member->second);
            prepared.memberGroups[member->second] = slot;
        }
        prepared.groups.push_back(std::move(group));
    }
    for (auto& [b, order] : sorted) {
        const auto row = prepared.rows.size();
        prepared.sources[sourceKey(b.source, b.phase, b.outcome, b.timing)].push_back(row);
        std::optional<std::size_t> target;
        if (const auto found = prepared.targets.find(b.target); found != prepared.targets.end())
            target = found->second;
        const auto member = prepared.members.at(b.bindingId);
        const auto group =
            b.groupMembers.empty() ? std::nullopt : std::optional{groups.at(b.groupMembers)};
        prepared.rows.push_back({std::move(b), member, order, group, target});
    }
    return prepared;
} catch (...) {
    return core::unexpected(invalid("Gameplay projection allocation failed"));
}

auto PresentationProjectionAdapter::project(
    const PreparedProjection& prepared, const ProjectionState& current,
    const judgement::detail::PublicationMetadata& publication, GameplayPresentationTick t,
    const ReadFact& readFact, const HasTarget& hasTarget) -> core::Result<ProjectionCandidate> try {
    if (!publication.mayProject || (current.scope && current.scope != publication.runScope) ||
        current.cursor != publication.fromCursor ||
        current.cursor > publication.publishableCursor ||
        (current.lastTick && t.value < current.lastTick->value))
        return core::unexpected(invalid("Invalid Gameplay projection prefix or Tick"));
    ProjectionCandidate candidate{current, {}, {}};
    auto& state = candidate.state;
    state.scope = publication.runScope;
    state.seenMembers.resize(prepared.members.size());
    state.rejectedGroups.resize(prepared.groups.size());
    state.matchedGroupMembers.resize(prepared.groups.size());
    for (; state.cursor < publication.publishableCursor; ++state.cursor) {
        auto fact = readFact(state.cursor);
        if (!fact)
            return core::unexpected(std::move(fact.error()));
        if (fact->kind == judgement::FactKind::stray ||
            fact->kind == judgement::FactKind::consumeEmpty)
            continue;
        if (fact->kind != judgement::FactKind::phaseOutcome || !fact->requirement || !fact->phase ||
            !fact->outcome)
            return core::unexpected(invalid("Unsupported publication Fact"));
        GameplayPhase phase;
        switch (*fact->phase) {
        case judgement::PhaseKind::tap:
            phase = GameplayPhase::Tap;
            break;
        case judgement::PhaseKind::head:
            phase = GameplayPhase::Head;
            break;
        case judgement::PhaseKind::body:
            phase = GameplayPhase::Body;
            break;
        case judgement::PhaseKind::tail:
            phase = GameplayPhase::Tail;
            break;
        default:
            return core::unexpected(invalid("Unsupported publication phase"));
        }
        if (*fact->outcome != judgement::Outcome::hit && *fact->outcome != judgement::Outcome::miss)
            return core::unexpected(invalid("Unsupported publication outcome"));
        if (prepared.sources.empty())
            continue;
        const auto outcome = *fact->outcome == judgement::Outcome::hit ? GameplayOutcome::Hit
                                                                       : GameplayOutcome::Miss;
        auto key = sourceKey(*fact->requirement, phase, outcome, GameplayTimingClass::Any);
        std::vector<std::size_t> matches;
        const auto append = [&] {
            if (const auto found = prepared.sources.find(key); found != prepared.sources.end())
                matches.insert(matches.end(), found->second.begin(), found->second.end());
        };
        append();
        if (fact->error) {
            std::get<8>(key) = fact->error->value() < 0   ? GameplayTimingClass::Early
                               : fact->error->value() > 0 ? GameplayTimingClass::Late
                                                          : GameplayTimingClass::Exact;
            append();
        }
        std::sort(matches.begin(), matches.end());
        for (auto row : matches) {
            ++candidate.matchedRows;
            const auto& definition = prepared.rows[row];
            const auto& b = definition.binding;
            if (!state.seenMembers[definition.member]) {
                state.seenMembers[definition.member] = true;
                if (const auto group = prepared.memberGroups[definition.member])
                    ++state.matchedGroupMembers[*group];
            }
            if (b.end && b.end->value <= t.value)
                continue;
            ProjectionEffect effect{fact->factId, state.cursor, row};
            if (b.start.value > t.value)
                state.future.emplace(ProjectionState::FutureKey{b.start.value, state.cursor, row},
                                     effect);
            else
                state.active.push_back(effect);
        }
    }
    while (!state.future.empty() && std::get<0>(state.future.begin()->first) <= t.value) {
        state.active.push_back(state.future.begin()->second);
        state.future.erase(state.future.begin());
    }
    std::erase_if(state.active, [&](const auto& effect) {
        const auto& b = prepared.rows[effect.row].binding;
        return b.end && b.end->value <= t.value;
    });
    // Preserve Fact order across future promotion and subsequent incremental reads.
    std::sort(state.active.begin(), state.active.end(), [](const auto& a, const auto& b) {
        return std::tie(a.ordinal, a.row) < std::tie(b.ordinal, b.row);
    });
    const auto ready = [&](std::size_t group) {
        return state.matchedGroupMembers[group] == prepared.groups[group].members.size();
    };
    std::vector<std::pair<bool, bool>> attempts(prepared.groups.size());
    std::vector<bool> validTargets(state.active.size(), false);
    for (std::size_t i = 0; i < state.active.size(); ++i) {
        const auto& effect = state.active[i];
        const auto& row = prepared.rows[effect.row];
        const auto& b = row.binding;
        if (b.aggregation != GameplayAggregation::Any && (!row.group || !ready(*row.group)))
            continue;
        if (row.group && state.rejectedGroups[*row.group] && !effect.published)
            continue;
        if (row.targetLayout) {
            auto exists = hasTarget(*row.targetLayout, b.target);
            if (!exists)
                return core::unexpected(std::move(exists.error()));
            validTargets[i] = *exists;
        }
        if (b.aggregation == GameplayAggregation::GroupCommit &&
            !state.rejectedGroups[*row.group]) {
            auto& attempt = attempts[*row.group];
            (validTargets[i] ? attempt.first : attempt.second) = true;
        }
    }
    for (std::size_t i = 0; i < attempts.size(); ++i)
        if (attempts[i].first && attempts[i].second)
            candidate.diagnostics.add(core::Diagnostic{
                core::DiagnosticSeverity::Error, "partial-group",
                "Gameplay group retained committed members and rejects new members",
                prepared.groups[i].names.front()});
    // Stable identity folds writes; rows still retain independent windows and feedback sources.
    std::map<std::tuple<std::uint64_t, std::uint64_t, std::string>, bool> tokens;
    for (std::size_t i = 0; i < state.active.size(); ++i) {
        auto& effect = state.active[i];
        const auto& row = prepared.rows[effect.row];
        const auto& b = row.binding;
        if ((b.aggregation != GameplayAggregation::Any && (!row.group || !ready(*row.group))) ||
            (row.group && state.rejectedGroups[*row.group] && !effect.published))
            continue;
        if (!validTargets[i]) {
            candidate.diagnostics.add(
                core::Diagnostic{core::DiagnosticSeverity::Error, "presentation-target-missing",
                                 "Gameplay presentation target missing", b.target});
            continue;
        }
        const auto [token, unique] = tokens.emplace(
            std::tuple{effect.factId.commitId, effect.factId.localOrdinal, b.target}, b.visible);
        if (!unique && token->second != b.visible)
            return core::unexpected(invalid("Conflicting active Gameplay writes"));
        const auto [write, inserted] = candidate.writes.emplace(b.target, b.visible);
        if (!inserted && write->second != b.visible)
            return core::unexpected(invalid("Conflicting active Gameplay writes"));
        effect.published = true;
    }
    for (std::size_t i = 0; i < attempts.size(); ++i)
        if (attempts[i].first && attempts[i].second)
            state.rejectedGroups[i] = true;
    state.lastTick = t;
    // Queries historically choose the first declared window for overlapping same-value aliases.
    // Candidate processing above stays canonical; only the installed observation order differs.
    std::sort(state.active.begin(), state.active.end(), [&](const auto& a, const auto& b) {
        return std::tie(a.ordinal, prepared.rows[a.row].declarationOrder) <
               std::tie(b.ordinal, prepared.rows[b.row].declarationOrder);
    });
    return candidate;
} catch (...) {
    return core::unexpected(invalid("Gameplay projection allocation failed"));
}
} // namespace cuexis::playback::detail
