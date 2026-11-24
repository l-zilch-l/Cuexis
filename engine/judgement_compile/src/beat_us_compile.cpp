#include <cuexis/judgement/beat_us_compile.hpp>

#include <cuexis/judgement/timebase_tick.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace cuexis::judgement {
namespace {

[[nodiscard]] auto relationError(std::string reason, std::string path, std::string message)
    -> core::Error {
    return core::Error{"judgement.execution.relation_invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason))
        .withContext("fieldPath", std::move(path));
}

[[nodiscard]] auto requirementPath(std::size_t index) -> std::string {
    return "/gameplay/requirements/" + std::to_string(index);
}

[[nodiscard]] auto convertLatePolicy(const LatePolicyParameters& source)
    -> runtime::LatePolicyParameters {
    const auto convertTick =
        [](const MeasuredParameter<TickSpan>& parameter) -> runtime::MeasuredParameter<TickSpan> {
        if (const auto* value = parameter.measuredValue(); value != nullptr) {
            return runtime::MeasuredParameter<TickSpan>::measured(*value);
        }
        return runtime::MeasuredParameter<TickSpan>::pendingMeasurement();
    };
    runtime::LatePolicyParameters converted;
    converted.finalizationWatermark = convertTick(source.finalizationWatermark);
    converted.maxQueueHop = convertTick(source.maxQueueHop);
    converted.windowCloseThreshold = convertTick(source.windowCloseThreshold);
    converted.windowOpenThreshold = convertTick(source.windowOpenThreshold);
    if (source.policy) {
        converted.policy = *source.policy == LateEventPolicy::queueNextTick
                               ? runtime::LateEventPolicy::queueNextTick
                               : runtime::LateEventPolicy::rejectLate;
    }
    return converted;
}

[[nodiscard]] auto convertInputAction(InputAction action) -> runtime::InputAction {
    switch (action) {
    case InputAction::press:
        return runtime::InputAction::press;
    case InputAction::release:
        return runtime::InputAction::release;
    case InputAction::update:
        return runtime::InputAction::update;
    }
    return runtime::InputAction::press;
}

[[nodiscard]] auto convertResourceIntent(ResourceClaimIntent intent)
    -> core::Result<runtime::ResourceClaimIntent> {
    switch (intent) {
    case ResourceClaimIntent::observe:
        return runtime::ResourceClaimIntent::observe;
    case ResourceClaimIntent::claim:
        return runtime::ResourceClaimIntent::claim;
    case ResourceClaimIntent::consume:
        //  The running record mirror carries the observe/claim split only. A consuming claim
        //  cannot be represented without inventing a semantics, so it is rejected instead of being
        //  silently folded into one of the two carried intents.
        return core::unexpected(relationError(
            "unsupported_resource_intent", "/gameplay/requirements/resourceClaims",
            "a consuming resource claim is not representable in the running records"));
    }
    return core::unexpected(relationError("unsupported_resource_intent",
                                          "/gameplay/requirements/resourceClaims",
                                          "the resource intent is outside the accepted set"));
}

[[nodiscard]] auto convertGraceMode(GraceOverrideMode mode) -> runtime::GraceOverrideMode {
    switch (mode) {
    case GraceOverrideMode::none:
        return runtime::GraceOverrideMode::none;
    case GraceOverrideMode::sticky:
        return runtime::GraceOverrideMode::sticky;
    case GraceOverrideMode::observe:
        return runtime::GraceOverrideMode::observe;
    }
    return runtime::GraceOverrideMode::none;
}

[[nodiscard]] auto convertAtomBinding(const AtomBinding& source) -> runtime::AtomBinding {
    runtime::AtomBinding binding;
    binding.atomRef = source.atomRef;
    binding.domainToken = source.domainToken;
    binding.sourceClass = source.sourceClass;
    binding.channelToken = source.channelToken;
    binding.action = convertInputAction(source.action);
    if (source.amountRange) {
        binding.amountRange =
            runtime::AmountMatchRange{source.amountRange->minimum, source.amountRange->maximum};
    }
    binding.tailOnly = source.tailOnly;
    return binding;
}

[[nodiscard]] auto convertClaim(const ResourceClaimDeclaration& source)
    -> core::Result<runtime::ResourceClaimDeclaration> {
    runtime::ResourceClaimDeclaration claim;
    claim.resourceId = source.resourceRef.resourceId;
    claim.claimPolicy.policyToken = source.claimPolicy.policyToken;
    claim.claimPolicy.claimKeyToken = source.claimPolicy.claimKeyToken;
    if (source.claimPolicy.competition) {
        claim.claimPolicy.competition = runtime::ClaimPolicyDeclaration::CompetitionKey{
            source.claimPolicy.competition->priority, source.claimPolicy.competition->tieRank};
    }
    auto intent = convertResourceIntent(source.intent);
    if (!intent) {
        return core::unexpected(std::move(intent.error()));
    }
    claim.intent = *intent;
    claim.graceOverride.mode = convertGraceMode(source.graceOverride.mode);
    claim.graceOverride.overrideToken = source.graceOverride.overrideToken;
    return claim;
}

[[nodiscard]] auto convertProgram(const PatternExecutionProgram& source)
    -> runtime::PatternExecutionProgram {
    runtime::PatternExecutionProgram program;
    program.atomRefs = source.atomRefs;
    program.start = source.start;
    program.accepting = source.accepting;
    program.transitions = source.transitions;
    program.live = source.live;
    return program;
}

[[nodiscard]] auto quantizeMilliseconds(double milliseconds, std::string path)
    -> core::Result<std::int64_t> {
    auto rounded = timing::roundMicroseconds(milliseconds * 1000.0);
    if (!rounded) {
        return core::unexpected(relationError(
            "time_range", std::move(path), "a relative millisecond endpoint is not representable"));
    }
    return rounded->value;
}

[[nodiscard]] auto checkedTickOffset(Tick base, std::int64_t delta, std::string path)
    -> core::Result<Tick> {
    auto result = offsetTicks(base, TickSpan{delta});
    if (!result) {
        return core::unexpected(relationError("tick_overflow", std::move(path),
                                              "a window endpoint leaves the tick range"));
    }
    return *result;
}

[[nodiscard]] auto anchorTick(const timing::TimingMap& map, const timing::RationalBeat& beat,
                              std::string path) -> core::Result<Tick> {
    const auto anchors = map.boundaries().anchors;
    const auto it =
        std::lower_bound(anchors.begin(), anchors.end(), beat,
                         [](const timing::AnchoredBeat& anchor, const timing::RationalBeat& value) {
                             return anchor.beat < value;
                         });
    if (it == anchors.end() || !(it->beat == beat)) {
        return core::unexpected(relationError("timing_anchor_missing", std::move(path),
                                              "a requirement boundary has no saved timing anchor"));
    }
    return Tick{it->at.value};
}

[[nodiscard]] auto buildGradeTables(std::span<const BeatGradeTable> source)
    -> core::Result<std::vector<PreparedGradeTable>> {
    std::vector<PreparedGradeTable> tables;
    tables.reserve(source.size());
    std::set<std::string> ids;
    for (std::size_t index = 0; index < source.size(); ++index) {
        const auto& table = source[index];
        const std::string path = "/gameplay/common/gradeTables/" + std::to_string(index);
        if (table.id.empty()) {
            return core::unexpected(relationError("invalid_grade_table", path + "/id",
                                                  "a grade table needs a non-empty id"));
        }
        if (!ids.insert(table.id).second) {
            return core::unexpected(relationError("duplicate_grade_table", path + "/id",
                                                  "grade table ids must be unique"));
        }
        PreparedGradeTable prepared;
        prepared.id = table.id;
        auto exactStart = quantizeMilliseconds(table.exactStartMs, path + "/exact/startMs");
        if (!exactStart) {
            return core::unexpected(std::move(exactStart.error()));
        }
        auto exactEnd = quantizeMilliseconds(table.exactEndMs, path + "/exact/endMs");
        if (!exactEnd) {
            return core::unexpected(std::move(exactEnd.error()));
        }
        prepared.exactStart = *exactStart;
        prepared.exactEnd = *exactEnd;
        if (!(prepared.exactStart <= 0 && 0 < prepared.exactEnd) ||
            prepared.exactStart >= prepared.exactEnd) {
            return core::unexpected(relationError(
                "exact_band", path + "/exact",
                "a quantized exact band must contain zero and keep a positive width"));
        }
        if (table.intervals.empty()) {
            return core::unexpected(relationError("empty_intervals", path + "/intervals",
                                                  "a grade table needs at least one interval"));
        }
        std::set<std::int64_t> starts;
        for (std::size_t intervalIndex = 0; intervalIndex < table.intervals.size();
             ++intervalIndex) {
            const auto& interval = table.intervals[intervalIndex];
            const std::string intervalPath = path + "/intervals/" + std::to_string(intervalIndex);
            auto start = quantizeMilliseconds(interval.startMs, intervalPath + "/startMs");
            if (!start) {
                return core::unexpected(std::move(start.error()));
            }
            auto end = quantizeMilliseconds(interval.endMs, intervalPath + "/endMs");
            if (!end) {
                return core::unexpected(std::move(end.error()));
            }
            if (*start >= *end) {
                return core::unexpected(
                    relationError("interval_collapsed", intervalPath,
                                  "two interval endpoints quantize onto the same tick"));
            }
            if (!starts.insert(*start).second) {
                return core::unexpected(relationError("duplicate_interval", intervalPath,
                                                      "grade interval starts must be unique"));
            }
            prepared.intervals.push_back(PreparedGradeInterval{*start, *end, interval.grade});
        }
        std::sort(prepared.intervals.begin(), prepared.intervals.end(),
                  [](const PreparedGradeInterval& left, const PreparedGradeInterval& right) {
                      return left.start < right.start;
                  });
        for (std::size_t intervalIndex = 1; intervalIndex < prepared.intervals.size();
             ++intervalIndex) {
            if (prepared.intervals[intervalIndex - 1].end !=
                prepared.intervals[intervalIndex].start) {
                return core::unexpected(
                    relationError("interval_gap", path + "/intervals",
                                  "quantized grade intervals must stay adjacent"));
            }
        }
        tables.push_back(std::move(prepared));
    }
    std::sort(tables.begin(), tables.end(),
              [](const PreparedGradeTable& left, const PreparedGradeTable& right) {
                  return left.id < right.id;
              });
    return tables;
}

[[nodiscard]] auto buildScoreMode(const ScoreMode& source) -> core::Result<ScoreMode> {
    ScoreMode mode;
    mode.id = source.id;
    mode.total = source.total;
    mode.mj = source.mj;
    mode.mc = source.mc;
    mode.onExhausted = source.onExhausted;
    mode.comboRewards = source.comboRewards;
    mode.rules = source.rules;

    if (mode.id.empty()) {
        return core::unexpected(relationError("invalid_score_mode", "/gameplay/common/scoreMode/id",
                                              "a score mode needs a non-empty id"));
    }
    const std::uint64_t weightSum =
        static_cast<std::uint64_t>(mode.mj) + static_cast<std::uint64_t>(mode.mc);
    if (weightSum != static_cast<std::uint64_t>(mode.total)) {
        return core::unexpected(
            relationError("score_weights", "/gameplay/common/scoreMode",
                          "the two normalized weights must add up to the declared total"));
    }
    if (mode.onExhausted != ComboAction::keep && mode.onExhausted != ComboAction::reset) {
        return core::unexpected(relationError("score_exhausted",
                                              "/gameplay/common/scoreMode/onExhausted",
                                              "the exhausted action must be keep or reset"));
    }
    if (mode.comboRewards.empty()) {
        return core::unexpected(relationError("score_rewards", "/gameplay/common/scoreMode",
                                              "the combo reward table must not be empty"));
    }
    if (mode.comboRewards.front().from != 1) {
        return core::unexpected(
            relationError("score_rewards", "/gameplay/common/scoreMode/comboRewards/0/from",
                          "the first combo reward segment must start at combo 1"));
    }
    for (std::size_t index = 1; index < mode.comboRewards.size(); ++index) {
        const auto& previous = mode.comboRewards[index - 1];
        const auto& current = mode.comboRewards[index];
        const std::string path = "/gameplay/common/scoreMode/comboRewards/" + std::to_string(index);
        if (current.from <= previous.from) {
            return core::unexpected(
                relationError("score_rewards", path + "/from",
                              "combo reward segment starts must be strictly increasing"));
        }
        const std::uint64_t span = static_cast<std::uint64_t>(current.from) - 1U -
                                   static_cast<std::uint64_t>(previous.from);
        if (span != 0 && previous.slope != 0 &&
            static_cast<std::uint64_t>(previous.slope) >
                std::numeric_limits<std::uint64_t>::max() / span) {
            return core::unexpected(
                relationError("score_rewards", path, "the previous reward segment overflows"));
        }
        const std::uint64_t growth = static_cast<std::uint64_t>(previous.slope) * span;
        if (growth > std::numeric_limits<std::uint64_t>::max() - previous.rewardAtFrom ||
            previous.rewardAtFrom + growth > current.rewardAtFrom) {
            return core::unexpected(
                relationError("score_rewards", path,
                              "a combo reward segment must not fall below the previous segment"));
        }
    }

    std::set<std::tuple<PhaseKind, Outcome, std::optional<std::string>>> keys;
    for (std::size_t index = 0; index < mode.rules.size(); ++index) {
        auto& rule = mode.rules[index];
        if (!keys.insert(std::make_tuple(rule.phase, rule.outcome, rule.grade)).second) {
            return core::unexpected(relationError(
                "duplicate_score_rule", "/gameplay/common/scoreMode/rules/" + std::to_string(index),
                "score rule keys must be unique"));
        }
        if (!rule.qualifies && (rule.judgePoints != 0 || rule.combo != ComboAction::keep)) {
            return core::unexpected(relationError(
                "score_rule", "/gameplay/common/scoreMode/rules/" + std::to_string(index),
                "a non-qualifying rule must award zero points and keep the combo"));
        }
        switch (rule.combo) {
        case ComboAction::keep:
        case ComboAction::increment:
        case ComboAction::reset:
            break;
        default:
            return core::unexpected(relationError("score_rule",
                                                  "/gameplay/common/scoreMode/rules/" +
                                                      std::to_string(index) + "/combo",
                                                  "the combo action is outside the accepted set"));
        }
    }
    std::sort(mode.rules.begin(), mode.rules.end(),
              [](const ScoreRuleRow& left, const ScoreRuleRow& right) {
                  return std::make_tuple(left.phase, left.outcome, left.grade) <
                         std::make_tuple(right.phase, right.outcome, right.grade);
              });
    return mode;
}

[[nodiscard]] auto declaredPhase(const std::vector<PhaseDeclaration>& phases, PhaseKind kind)
    -> bool {
    return std::any_of(phases.begin(), phases.end(),
                       [kind](const PhaseDeclaration& phase) { return phase.kind == kind; });
}

[[nodiscard]] auto hasQualifiedRule(const ScoreMode& mode, PhaseKind kind) -> bool {
    return std::any_of(mode.rules.begin(), mode.rules.end(), [kind](const ScoreRuleRow& rule) {
        return rule.phase == kind && rule.qualifies;
    });
}

[[nodiscard]] auto findScoreSelection(const BeatRequirementSource& authored, PhaseKind kind)
    -> const BeatScoreSelection* {
    for (const auto& selection : authored.scoring) {
        if (selection.phase == kind) {
            return &selection;
        }
    }
    return nullptr;
}

[[nodiscard]] auto findGradeSelection(const BeatRequirementSource& authored, PhaseKind kind)
    -> const BeatGradeSelection* {
    for (const auto& selection : authored.gradeTables) {
        if (selection.phase == kind) {
            return &selection;
        }
    }
    return nullptr;
}

struct BoundRequirement final {
    const RequirementRecord* source{};
    const BeatRequirementSource* authored{};
    const PatternDeclaration* pattern{};
    std::size_t requirementIndex{};
};

[[nodiscard]] auto buildTimers(const std::vector<runtime::PreparedRequirement>& requirements)
    -> core::Result<std::vector<runtime::PreparedTimerKey>> {
    std::vector<runtime::PreparedTimerKey> timers;
    for (std::size_t index = 0; index < requirements.size(); ++index) {
        const auto& requirement = requirements[index];
        const bool hasTap = declaredPhase(requirement.phases, PhaseKind::tap);
        const bool hasHead = declaredPhase(requirement.phases, PhaseKind::head);
        if (!hasTap && !hasHead) {
            return core::unexpected(relationError(
                "phase_target", requirementPath(index),
                "a requirement must declare a tap or head phase for timer preparation"));
        }
        const PhaseKind root = hasTap ? PhaseKind::tap : PhaseKind::head;
        const auto& timing = requirement.timing;
        std::optional<Tick> activation;
        const auto add = [&](PhaseKind phase, runtime::PreparedTimerKind kind, Tick tick,
                             std::optional<Tick> start = {}, std::optional<Tick> end = {}) {
            timers.push_back(
                runtime::PreparedTimerKey{requirement.identity, phase, kind, tick, start, end});
        };
        for (const auto& window : timing.successWindows) {
            if (window.phase.kind == root && (!activation || window.start < *activation)) {
                activation = window.start;
            }
            add(window.phase.kind, runtime::PreparedTimerKind::windowOpen, window.start,
                window.start, window.end);
            if (window.phase.kind == PhaseKind::body) {
                continue;
            }
            auto close = window.end;
            if (window.phase.kind == PhaseKind::head && timing.body) {
                auto upper = offsetTicks(timing.body->start, TickSpan{1});
                if (!upper) {
                    return core::unexpected(std::move(upper.error()));
                }
                close = std::min(close, *upper);
            }
            if (close > window.start) {
                add(window.phase.kind, runtime::PreparedTimerKind::phaseClose, close, window.start,
                    window.end);
            }
        }
        if (!activation) {
            return core::unexpected(
                relationError("activation", requirementPath(index) + "/timing/successWindows",
                              "a requirement has no window that opens the root phase"));
        }
        add(root, runtime::PreparedTimerKind::activation, *activation);
        if (timing.body) {
            add(PhaseKind::body, runtime::PreparedTimerKind::bodyEnd, timing.body->end);
            add(PhaseKind::body, runtime::PreparedTimerKind::phaseClose, timing.body->end,
                timing.body->start, timing.body->end);
        }
        add(root, runtime::PreparedTimerKind::hardDeadline, requirement.deadline);
    }
    std::sort(timers.begin(), timers.end(),
              [](const runtime::PreparedTimerKey& left, const runtime::PreparedTimerKey& right) {
                  return std::tie(left.tick, left.kind, left.requirement, left.phase,
                                  left.windowStart, left.windowEnd) <
                         std::tie(right.tick, right.kind, right.requirement, right.phase,
                                  right.windowStart, right.windowEnd);
              });
    timers.erase(std::unique(timers.begin(), timers.end()), timers.end());
    return timers;
}

} // namespace

auto compileBeatGameplay(const BeatGameplaySourceView& view, const timing::TimingMap& map,
                         const GameplayCompileContext& context)
    -> core::Result<BeatGameplayProduct> {
    if (!map.matchesSource(view.timing)) {
        return core::unexpected(
            relationError("timing_source_mismatch", "/timing",
                          "the timing map was compiled from a different timing source"));
    }

    BeatGameplayProduct product;
    product.latePolicy = convertLatePolicy(view.latePolicy);

    //  Match every merged source requirement to exactly one authored requirement and reject both a
    //  missing and an unmatched authored row.
    std::vector<bool> matched(view.requirements.size(), false);
    std::vector<BoundRequirement> bound;
    bound.reserve(view.requirements.size());
    for (const auto& source : view.sources) {
        for (const auto& requirement : source.document.requirements) {
            std::optional<std::size_t> found;
            for (std::size_t index = 0; index < view.requirements.size(); ++index) {
                if (matched[index]) {
                    continue;
                }
                if (view.requirements[index].identity == requirement.identity) {
                    found = index;
                    break;
                }
            }
            if (!found) {
                return core::unexpected(
                    relationError("dangling_requirement", "/gameplay/requirements",
                                  "a merged requirement has no authored timing row"));
            }
            matched[*found] = true;
            bound.push_back(
                BoundRequirement{&requirement, &view.requirements[*found], nullptr, *found});
        }
    }
    for (std::size_t index = 0; index < matched.size(); ++index) {
        if (!matched[index]) {
            return core::unexpected(
                relationError("unmatched_requirement", requirementPath(index),
                              "an authored timing row has no merged requirement"));
        }
    }

    //  Named Pattern definitions match exactly one merged pattern declaration each; an unused or
    //  duplicated named definition is a definition-relation failure.
    std::map<std::string, std::vector<std::size_t>> namedPatterns;
    for (std::size_t index = 0; index < view.patternDefinitions.size(); ++index) {
        const auto& definition = view.patternDefinitions[index];
        if (!definition.patternId.empty()) {
            namedPatterns[definition.patternId].push_back(index);
        }
    }
    std::vector<bool> patternUsed(view.patternDefinitions.size(), false);
    for (auto& requirement : bound) {
        if (requirement.source->pattern.patternId.empty()) {
            requirement.pattern = &requirement.source->pattern;
            continue;
        }
        const auto it = namedPatterns.find(requirement.source->pattern.patternId);
        if (it == namedPatterns.end()) {
            return core::unexpected(relationError(
                "missing_pattern_definition", requirementPath(requirement.requirementIndex),
                "a named pattern declaration has no definition body"));
        }
        if (it->second.size() != 1) {
            return core::unexpected(relationError(
                "duplicate_pattern_definition", requirementPath(requirement.requirementIndex),
                "a named pattern declaration matches more than one definition body"));
        }
        requirement.pattern = &view.patternDefinitions[it->second.front()];
        patternUsed[it->second.front()] = true;
    }
    for (const auto& entry : namedPatterns) {
        for (const std::size_t index : entry.second) {
            if (!patternUsed[index]) {
                return core::unexpected(
                    relationError("orphan_pattern_definition",
                                  "/gameplay/common/patternDefinitions/" + std::to_string(index),
                                  "a named pattern definition has no declaration"));
            }
        }
    }

    //  Named Measure definition bodies are validated for a unique name; the requirement-side
    //  correspondence is part of the canonical-row projection, which this delivery does not emit.
    std::set<std::string> measureNames;
    for (std::size_t index = 0; index < view.measureDefinitions.size(); ++index) {
        const auto& definition = view.measureDefinitions[index];
        if (definition.id && !measureNames.insert(*definition.id).second) {
            return core::unexpected(
                relationError("duplicate_measure_definition",
                              "/gameplay/common/measureDefinitions/" + std::to_string(index),
                              "named Measure definition bodies must have unique names"));
        }
    }

    auto scoreMode = buildScoreMode(view.scoreMode);
    if (!scoreMode) {
        return core::unexpected(std::move(scoreMode.error()));
    }
    product.normalizedScore.mode = std::move(*scoreMode);
    auto gradeTables = buildGradeTables(view.gradeTables);
    if (!gradeTables) {
        return core::unexpected(std::move(gradeTables.error()));
    }
    product.normalizedScore.gradeTables = std::move(*gradeTables);

    std::map<std::string, std::uint32_t> tableIndex;
    for (std::size_t index = 0; index < product.normalizedScore.gradeTables.size(); ++index) {
        tableIndex[product.normalizedScore.gradeTables[index].id] =
            static_cast<std::uint32_t>(index);
    }
    std::map<std::string, const BeatNoteType*> noteTypes;
    for (const auto& noteType : view.noteTypes) {
        if (!noteTypes.emplace(noteType.id, &noteType).second) {
            return core::unexpected(relationError("duplicate_note_type",
                                                  "/gameplay/common/noteTypes",
                                                  "note type ids must be unique"));
        }
    }

    product.requirements.reserve(bound.size());
    product.beatKeys.reserve(bound.size());
    for (std::size_t index = 0; index < bound.size(); ++index) {
        const auto& entry = bound[index];
        const auto& source = *entry.source;
        const auto& authored = *entry.authored;
        const std::string path = requirementPath(index);
        if (source.timing) {
            return core::unexpected(
                relationError("duplicate_timing", path + "/timing",
                              "a v3 requirement must not carry the legacy tick timing"));
        }

        runtime::PreparedRequirement requirement;
        requirement.stableId = source.stableId;
        requirement.identity = source.identity;
        requirement.phases = source.phases;

        runtime::RequirementTiming timing;
        auto endTick = anchorTick(map, authored.beats.end, path + "/timing/endBeat");
        if (!endTick) {
            return core::unexpected(std::move(endTick.error()));
        }
        timing.end = *endTick;
        if (authored.beats.body) {
            auto bodyStart =
                anchorTick(map, authored.beats.body->first, path + "/timing/body/startBeat");
            if (!bodyStart) {
                return core::unexpected(std::move(bodyStart.error()));
            }
            auto bodyEnd =
                anchorTick(map, authored.beats.body->second, path + "/timing/body/endBeat");
            if (!bodyEnd) {
                return core::unexpected(std::move(bodyEnd.error()));
            }
            timing.body = TimeInterval{*bodyStart, *bodyEnd};
        }
        for (const auto& target : authored.beats.phaseTargets) {
            auto tick = anchorTick(map, target.second, path + "/timing/phaseTargets");
            if (!tick) {
                return core::unexpected(std::move(tick.error()));
            }
            timing.phaseTargets.push_back(runtime::PhaseTarget{target.first, *tick});
        }
        for (const auto& phase : source.phases) {
            const auto count = static_cast<std::size_t>(std::count_if(
                authored.beats.phaseTargets.begin(), authored.beats.phaseTargets.end(),
                [&phase](const auto& target) { return target.first == phase.kind; }));
            if (count != 1) {
                return core::unexpected(
                    relationError("phase_target", path + "/timing/phaseTargets",
                                  "every declared phase needs exactly one beat target"));
            }
        }

        for (std::size_t windowIndex = 0; windowIndex < authored.successWindows.size();
             ++windowIndex) {
            const auto& window = authored.successWindows[windowIndex];
            const std::string windowPath =
                path + "/timing/successWindows/" + std::to_string(windowIndex);
            const bool declared = std::find(source.phases.begin(), source.phases.end(),
                                            window.phase) != source.phases.end();
            if (!declared) {
                return core::unexpected(relationError(
                    "invalid_relation", windowPath + "/phase",
                    "a success window names a phase the requirement did not declare"));
            }
            const auto target =
                std::find_if(timing.phaseTargets.begin(), timing.phaseTargets.end(),
                             [kind = window.phase.kind](const runtime::PhaseTarget& candidate) {
                                 return candidate.phase == kind;
                             });
            if (target == timing.phaseTargets.end()) {
                return core::unexpected(relationError("phase_target", path + "/timing/phaseTargets",
                                                      "a success window has no phase target"));
            }
            auto start = quantizeMilliseconds(window.startMs, windowPath + "/startMs");
            if (!start) {
                return core::unexpected(std::move(start.error()));
            }
            auto end = quantizeMilliseconds(window.endMs, windowPath + "/endMs");
            if (!end) {
                return core::unexpected(std::move(end.error()));
            }
            auto startTick = checkedTickOffset(target->chartTick, *start, windowPath + "/phase");
            if (!startTick) {
                return core::unexpected(std::move(startTick.error()));
            }
            auto windowEndTick = checkedTickOffset(target->chartTick, *end, windowPath + "/phase");
            if (!windowEndTick) {
                return core::unexpected(std::move(windowEndTick.error()));
            }
            timing.successWindows.push_back(
                runtime::SuccessWindow{*startTick, *windowEndTick, window.phase});
        }
        requirement.timing = std::move(timing);

        const bool hasTail = declaredPhase(source.phases, PhaseKind::tail);
        if (hasTail) {
            auto deadline = offsetTicks(requirement.timing.end, source.preparedGrace.span());
            if (!deadline) {
                return core::unexpected(relationError("tick_overflow", path + "/deadline",
                                                      "the requirement deadline overflows"));
            }
            requirement.deadline = *deadline;
        } else {
            requirement.deadline = requirement.timing.end;
        }

        //  Selection maps. An instance selection must name a declared phase and a declared Measure
        //  component; a note-type selection is ignored for a phase this requirement does not use.
        std::map<PhaseKind, const BeatGradeSelection*> instanceSelections;
        for (std::size_t selectionIndex = 0; selectionIndex < authored.gradeTables.size();
             ++selectionIndex) {
            const auto& selection = authored.gradeTables[selectionIndex];
            const std::string selectionPath =
                path + "/gradeTables/" + std::to_string(selectionIndex);
            if (!instanceSelections.emplace(selection.phase, &selection).second) {
                return core::unexpected(
                    relationError("duplicate_grade_selection", selectionPath,
                                  "a phase may carry at most one grade table selection"));
            }
            if (!declaredPhase(source.phases, selection.phase)) {
                return core::unexpected(relationError(
                    "invalid_grade_selection", selectionPath,
                    "a grade selection names a phase the requirement did not declare"));
            }
            if (tableIndex.find(selection.gradeTable) == tableIndex.end()) {
                return core::unexpected(
                    relationError("dangling_grade_table", selectionPath + "/gradeTable",
                                  "a grade selection names a table the chart does not declare"));
            }
        }
        std::map<PhaseKind, const BeatGradeSelection*> noteSelections;
        if (authored.noteType) {
            const auto note = noteTypes.find(*authored.noteType);
            if (note == noteTypes.end()) {
                return core::unexpected(relationError(
                    "dangling_note_type", path + "/noteType",
                    "a note type selection names a note type the chart does not declare"));
            }
            for (std::size_t selectionIndex = 0; selectionIndex < note->second->gradeTables.size();
                 ++selectionIndex) {
                const auto& selection = note->second->gradeTables[selectionIndex];
                if (!noteSelections.emplace(selection.phase, &selection).second) {
                    return core::unexpected(relationError(
                        "duplicate_grade_selection",
                        "/gameplay/common/noteTypes/" + *authored.noteType + "/gradeTables",
                        "a note type may carry at most one grade table selection per phase"));
                }
                if (tableIndex.find(selection.gradeTable) == tableIndex.end()) {
                    return core::unexpected(relationError(
                        "dangling_grade_table",
                        "/gameplay/common/noteTypes/" + *authored.noteType + "/gradeTables",
                        "a note type names a table the chart does not declare"));
                }
            }
        }
        std::set<PhaseKind> measurePhases;
        for (const auto& component : source.measure.components) {
            if (!declaredPhase(source.phases, component.phase)) {
                return core::unexpected(relationError(
                    "invalid_measure", path + "/measure",
                    "a measure component names a phase the requirement did not declare"));
            }
            if (!measurePhases.insert(component.phase).second) {
                return core::unexpected(
                    relationError("invalid_measure", path + "/measure",
                                  "a requirement may declare at most one component per phase"));
            }
            if (!component.categoryToken.empty() &&
                component.categoryToken !=
                    std::string{factCategoryToken(factCategoryOfPhase(component.phase))}) {
                return core::unexpected(relationError(
                    "invalid_measure", path + "/measure",
                    "a measure category token must equal the phase-derived category"));
            }
            std::optional<std::uint32_t> selectedTable;
            if (const auto instanceTable = instanceSelections.find(component.phase);
                instanceTable != instanceSelections.end()) {
                selectedTable = tableIndex[instanceTable->second->gradeTable];
            } else if (const auto noteTable = noteSelections.find(component.phase);
                       noteTable != noteSelections.end()) {
                selectedTable = tableIndex[noteTable->second->gradeTable];
            }
            runtime::CompiledMeasureComponent prepared;
            prepared.phase = component.phase;
            prepared.category = factCategoryOfPhase(component.phase);
            prepared.grade = runtime::GradePresence::absent;
            prepared.gradeTableIndex = selectedTable;
            requirement.measure.push_back(std::move(prepared));
        }
        for (const auto& selectionEntry : instanceSelections) {
            if (measurePhases.find(selectionEntry.first) == measurePhases.end()) {
                return core::unexpected(
                    relationError("invalid_grade_selection", path + "/gradeTables",
                                  "an instance grade selection has no matching Measure component"));
            }
        }

        requirement.atomBindings.reserve(source.atomBindings.size());
        for (const auto& binding : source.atomBindings) {
            requirement.atomBindings.push_back(convertAtomBinding(binding));
        }
        requirement.resourceClaims.reserve(source.resourceClaims.size());
        for (const auto& claim : source.resourceClaims) {
            auto converted = convertClaim(claim);
            if (!converted) {
                return core::unexpected(std::move(converted.error()));
            }
            requirement.resourceClaims.push_back(std::move(*converted));
        }
        if (source.independentCompetition) {
            requirement.independentCompetition = runtime::ClaimPolicyDeclaration::CompetitionKey{
                source.independentCompetition->priority, source.independentCompetition->tieRank};
        }
        auto compiledPattern = compilePattern(*entry.pattern, context.patternBudget);
        if (!compiledPattern) {
            return core::unexpected(std::move(compiledPattern.error()));
        }
        auto program = compiledPattern->executionProgram();
        if (!program) {
            return core::unexpected(std::move(program.error()));
        }
        requirement.executionProgram = convertProgram(*program);
        requirement.expandedPatternNodes = compiledPattern->fullyExpandedCount();
        product.requirements.push_back(std::move(requirement));
        product.beatKeys.push_back(authored.beats);
    }

    //  Resource rows: declaration order, with requirement indices in canonical requirement order.
    std::map<std::string, std::size_t> resourceIndex;
    for (const auto& source : view.sources) {
        for (const auto& resource : source.document.resources) {
            if (resourceIndex.find(resource.ref.resourceId) != resourceIndex.end()) {
                return core::unexpected(
                    relationError("duplicate_resource", "/gameplay/resources",
                                  "resource ids must be unique across the merged sources"));
            }
            resourceIndex[resource.ref.resourceId] = product.resources.size();
            product.resources.push_back(
                runtime::PreparedResource{resource.ref.resourceId,
                                          resource.slotToken,
                                          {},
                                          resource.terminalAfterTermination});
        }
    }
    for (std::size_t index = 0; index < product.requirements.size(); ++index) {
        for (const auto& claim : product.requirements[index].resourceClaims) {
            const auto it = resourceIndex.find(claim.resourceId);
            if (it == resourceIndex.end()) {
                return core::unexpected(
                    relationError("dangling_resource", requirementPath(index) + "/resourceClaims",
                                  "a resource claim names a resource the source does not declare"));
            }
            product.resources[it->second].requirementIndices.push_back(
                static_cast<std::uint32_t>(index));
        }
    }

    //  Scoring units. A declared phase with an explicit `unit` joins that named unit; an explicit
    //  absent unit disables the phase; otherwise the phase forms a singleton unit exactly when the
    //  mode carries a qualified rule for it.
    std::map<std::string, std::vector<std::uint32_t>> grouped;
    std::vector<ScoreUnit> units;
    std::uint32_t phaseIndex = 0;
    for (std::size_t requirementIndex = 0; requirementIndex < product.requirements.size();
         ++requirementIndex) {
        const auto& requirement = product.requirements[requirementIndex];
        const auto& authored = *bound[requirementIndex].authored;
        std::set<PhaseKind> declaredSelections;
        for (const auto& selection : authored.scoring) {
            const std::string selectionPath = requirementPath(requirementIndex) + "/scoring";
            if (!declaredSelections.insert(selection.phase).second) {
                return core::unexpected(
                    relationError("duplicate_score_selection", selectionPath,
                                  "a phase may carry at most one scoring selection"));
            }
            if (!declaredPhase(requirement.phases, selection.phase)) {
                return core::unexpected(relationError(
                    "invalid_score_selection", selectionPath,
                    "a scoring selection names a phase the requirement did not declare"));
            }
            if (selection.unit && selection.unit->empty()) {
                return core::unexpected(relationError("invalid_score_selection", selectionPath,
                                                      "a scoring unit name must not be empty"));
            }
        }
        for (const auto& phase : requirement.phases) {
            const auto* selection = findScoreSelection(authored, phase.kind);
            if (selection != nullptr) {
                if (selection->unit) {
                    grouped[*selection->unit].push_back(phaseIndex);
                }
            } else if (hasQualifiedRule(product.normalizedScore.mode, phase.kind)) {
                units.push_back(ScoreUnit{{phaseIndex}});
            }
            ++phaseIndex;
        }
    }
    for (auto& entry : grouped) {
        units.push_back(ScoreUnit{std::move(entry.second)});
    }
    std::sort(units.begin(), units.end(), [](const ScoreUnit& left, const ScoreUnit& right) {
        return left.phaseIndices.front() < right.phaseIndices.front();
    });
    product.normalizedScore.units = std::move(units);

    auto basis = runtime::deriveScoreBasis(product.requirements, product.latePolicy,
                                           product.normalizedScore);
    if (!basis) {
        return core::unexpected(std::move(basis.error()));
    }
    product.normalizedScore.basis = *basis;

    auto timers = buildTimers(product.requirements);
    if (!timers) {
        return core::unexpected(std::move(timers.error()));
    }
    product.timers = std::move(*timers);

    //  Canonical GPH0/GPR0/GPD0/GRC0 row emission is not part of this delivery; the row groups stay
    //  empty so the product can only be consumed as an unterminated result.
    return product;
}

} // namespace cuexis::judgement
