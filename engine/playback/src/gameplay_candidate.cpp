#include "gameplay_internal.hpp"
#include <algorithm>
#include <cuexis/core/error.hpp>
#include <cuexis/judgement/recovery_codec.hpp>
#include <exception>

namespace cuexis::playback {
namespace {
auto invalid(std::string_view message) -> core::Error {
    return core::Error{"playback.gameplay.invalid", std::string{message}}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}
auto phase(GameplayPhase p) -> judgement::PhaseKind {
    switch (p) {
    case GameplayPhase::Tap:
        return judgement::PhaseKind::tap;
    case GameplayPhase::Head:
        return judgement::PhaseKind::head;
    case GameplayPhase::Body:
        return judgement::PhaseKind::body;
    case GameplayPhase::Tail:
        return judgement::PhaseKind::tail;
    }
    throw std::invalid_argument{"unknown Gameplay phase"};
}
} // namespace

auto gameplayCapabilities() -> core::Result<std::vector<GameplayCapability>> try {
    return std::vector<GameplayCapability>{{"cuexis.gameplay.t4-k4.v1",
                                            "1",
                                            "gameplay.execution.t4-k4.v1",
                                            {"discrete.phase-local",
                                             "chart.5/gameplay.2/capsule.3",
                                             "pending.content",
                                             "pending.snapshot",
                                             "canonical.discrete.journal",
                                             {"declared.static"},
                                             "capability.budget_insufficient"}}};
} catch (...) {
    return core::unexpected(invalid("Capability registry allocation failed"));
}

namespace {
template <typename Decoder>
auto makeGameplayContent(const GameplayConfiguration& c, Decoder decoder)
    -> core::Result<GameplayContent> try {
    using namespace judgement;
    const std::array<std::string, 6> actual{
        "judgement.t4-k4.v1",          "fact.semantic.phase-local.v1",
        "fixed-point.none.v1",         "coordination.six-eight.t4-k4.v1",
        "gameplay.execution.t4-k4.v1", "late.window.logical.v1"};
    if (c.engine != actual || !c.testOnly) {
        return core::unexpected(
            invalid("Explicit compiled engine and test-only candidate profile required"));
    }
    for (const auto& s : c.session) {
        if (s.empty())
            return core::unexpected(invalid("Session identity declaration missing"));
    }
    if (c.rulesetInterfaceProjection.empty() || c.rulesetBuild.empty() ||
        c.rulesetModuleOrder.empty()) {
        return core::unexpected(invalid("Compile Ruleset identity declaration missing"));
    }
    PreparedIdentityDeclarations identities{
        {c.engine[0], c.engine[1], c.engine[2], c.engine[3], c.engine[4], c.engine[5]},
        {c.rulesetInterfaceProjection, c.rulesetModuleOrder, c.rulesetBuild},
        {c.session[0], c.session[1], c.session[2], c.session[3]}};
    // This static registry records the actual kernel supported by this SDK, not author features.
    auto registry = gameplayCapabilities();
    if (!registry)
        return core::unexpected(detail::projectGameplayError(std::move(registry.error())));
    std::vector<std::string> registered;
    for (const auto& capability : *registry)
        registered.push_back(capability.id);
    auto enabledCapabilities = c.enabledCapabilities;
    std::sort(enabledCapabilities.begin(), enabledCapabilities.end());
    for (const auto& id : enabledCapabilities) {
        if (std::find(registered.begin(), registered.end(), id) == registered.end()) {
            return core::unexpected(
                core::Error{"capability.unknown", "Unknown enabled Gameplay capability"}
                    .withContext("category", "unknown_capability")
                    .withContext("severity", "error")
                    .withContext("faulted", "false")
                    .withContext("capabilityId", id)
                    .withContext("fieldPath", "enabledCapabilities"));
        }
    }
    const gameplay_packed::DecodeContext context{
        {registered, c.enabledCapabilities}, {}, {}, identities, "coordinator.policy.greedy_v1", 3};
    auto decoded = decoder(context);
    if (!decoded)
        return core::unexpected(detail::projectGameplayError(std::move(decoded.error())));
    for (const auto& requirement :
         decoded->gameplay.assembled().graph.derivedCapabilities.capabilities) {
        const auto capabilityRecord =
            std::find_if(registry->begin(), registry->end(), [&](const auto& capability) {
                return capability.id == requirement.capabilityId;
            });
        if (capabilityRecord == registry->end() ||
            capabilityRecord->revision != requirement.revision) {
            return core::unexpected(
                core::Error{"capability.revision_mismatch", "Compiled capability revision mismatch"}
                    .withContext("category", "unknown_capability")
                    .withContext("severity", "error")
                    .withContext("faulted", "false")
                    .withContext("capabilityId", requirement.capabilityId)
                    .withContext("fieldPath", "derivedCapabilities.revision"));
        }
    }
    auto source = SourceClass::fromToken(c.sourceClass);
    if (!source)
        return core::unexpected(detail::projectGameplayError(std::move(source.error())));
    InputMappingProfile mapping{c.mappingId, c.mappingVersion, *source, {}};
    for (const auto& d : c.domains) {
        auto scale = RationalDuration::create(d.scale.numerator, d.scale.denominator);
        if (!scale)
            return core::unexpected(detail::projectGameplayError(std::move(scale.error())));
        mapping.domains.push_back(
            {d.id,
             {*scale, d.minimum, d.maximum,
              d.inclusive ? AmountBoundaryPolicy::inclusive : AmountBoundaryPolicy::exclusive}});
    }
    if (auto valid = validateInputMapping(mapping); !valid)
        return core::unexpected(detail::projectGameplayError(std::move(valid.error())));
    if (c.programPolicy > GameplayProgramPolicy::Open ||
        c.outcomeScope > GameplayOutcomeScope::Extended || c.arithmetic > GameplayArithmetic::Clamp)
        return core::unexpected(invalid("Unknown Ruleset policy"));
    const auto policy = c.programPolicy == GameplayProgramPolicy::Locked ? ProgramPolicy::locked
                        : c.programPolicy == GameplayProgramPolicy::Extendable
                            ? ProgramPolicy::extendable
                            : ProgramPolicy::open;
    RulesetDeclaration rules{
        c.actualRulesetInterface,
        c.actualRulesetRevision,
        c.actualRulesetBuild,
        c.loadout,
        policy,
        c.outcomeScope == GameplayOutcomeScope::Declared ? OutcomeScope::declared
                                                         : OutcomeScope::extended,
        {},
        {c.initialScore,
         c.minimumScore,
         c.maximumScore,
         c.arithmetic == GameplayArithmetic::Checked ? ArithmeticPolicy::checked
                                                     : ArithmeticPolicy::clamp,
         c.initialCombo,
         {}},
        {},
        c.externalPackage,
        c.life};
    for (const auto& m : c.modules)
        rules.modules.push_back({m.id, m.revision, m.build});
    for (const auto& h : c.hooks) {
        if (h.consumer > GameplayHookConsumer::Shared || h.combine > GameplayCombine::BitOr)
            return core::unexpected(invalid("Unknown Hook consumer or combine"));
        const auto consumer = h.consumer == GameplayHookConsumer::Fold     ? HookConsumer::fold
                              : h.consumer == GameplayHookConsumer::Kernel ? HookConsumer::kernel
                                                                           : HookConsumer::shared;
        rules.hooks.push_back({h.target, consumer,
                               h.combine == GameplayCombine::Maximum ? CombineOperator::maximum
                                                                     : CombineOperator::bitOr,
                               h.contributors, h.value, h.stepRoute});
    }
    for (const auto& r : c.scoreRules) {
        if (r.outcome != GameplayOutcome::Hit && r.outcome != GameplayOutcome::Miss)
            return core::unexpected(invalid("Unknown Gameplay outcome"));
        rules.score.rules.push_back(
            {phase(r.phase), r.outcome == GameplayOutcome::Hit ? Outcome::hit : Outcome::miss,
             r.grade, r.delta, r.incrementsCombo});
    }
    auto preparedRules = prepareRuleset(rules);
    if (!preparedRules)
        return core::unexpected(detail::projectGameplayError(std::move(preparedRules.error())));
    const auto& g = decoded->gameplay.assembled().graph;
    auto bindings = c.bindings;
    for (auto& binding : bindings)
        std::sort(binding.groupMembers.begin(), binding.groupMembers.end());
    for (const auto& binding : bindings) {
        for (const auto& alias : bindings) {
            if (alias.bindingId != binding.bindingId)
                continue;
            if (alias.aggregation != binding.aggregation ||
                alias.groupMembers != binding.groupMembers ||
                (!binding.groupMembers.empty() &&
                 (alias.source != binding.source || alias.phase != binding.phase ||
                  alias.outcome != binding.outcome || alias.timing != binding.timing)))
                return core::unexpected(invalid(
                    "FactBinding group member aliases must share membership and source predicate"));
        }
        if (binding.bindingId.empty() || binding.target.empty() ||
            (binding.end && binding.start.value >= binding.end->value) ||
            binding.timing > GameplayTimingClass::Late || binding.outcome > GameplayOutcome::Miss ||
            binding.aggregation > GameplayAggregation::GroupCommit ||
            (binding.groupMembers.empty() && binding.aggregation != GameplayAggregation::Any) ||
            std::adjacent_find(binding.groupMembers.begin(), binding.groupMembers.end()) !=
                binding.groupMembers.end())
            return core::unexpected(invalid("Invalid FactBinding declaration"));
        if (!binding.groupMembers.empty()) {
            if (!std::binary_search(binding.groupMembers.begin(), binding.groupMembers.end(),
                                    binding.bindingId))
                return core::unexpected(invalid("FactBinding group must include itself"));
            for (const auto& member : binding.groupMembers) {
                const auto matching =
                    std::find_if(bindings.begin(), bindings.end(),
                                 [&](const auto& row) { return row.bindingId == member; });
                if (matching == bindings.end() || matching->aggregation != binding.aggregation ||
                    matching->groupMembers != binding.groupMembers)
                    return core::unexpected(
                        invalid("FactBinding group membership closure incomplete"));
            }
        }
        static_cast<void>(phase(binding.phase));
        const auto matches = [&](const RequirementRecord& r) {
            const auto& e = r.identity;
            const auto& s = binding.source;
            if (e.chartEntryId != s.chartEntryId || e.invocationId != s.invocationId ||
                e.moduleId != s.moduleId || e.exportId != s.exportId ||
                e.requirementLocalId != s.requirementLocalId ||
                e.emissionPath.size() != s.emissionPath.size())
                return false;
            for (std::size_t i = 0; i < e.emissionPath.size(); ++i)
                if (e.emissionPath[i].nodeId != s.emissionPath[i].nodeId ||
                    e.emissionPath[i].repeatIndex != s.emissionPath[i].repeatIndex)
                    return false;
            if (std::none_of(r.phases.begin(), r.phases.end(),
                             [&](const auto& p) { return p.kind == phase(binding.phase); }))
                return false;
            return std::find(r.factBindingRefs.begin(), r.factBindingRefs.end(),
                             binding.bindingId) != r.factBindingRefs.end();
        };
        if (std::none_of(g.requirements.begin(), g.requirements.end(), matches))
            return core::unexpected(
                invalid("FactBinding source or judgement-side reference missing"));
        for (const auto& other : bindings) {
            const auto sameSource = [&](const GameplayRequirementRef& a,
                                        const GameplayRequirementRef& b) {
                if (a.chartEntryId != b.chartEntryId || a.invocationId != b.invocationId ||
                    a.moduleId != b.moduleId || a.exportId != b.exportId ||
                    a.requirementLocalId != b.requirementLocalId ||
                    a.emissionPath.size() != b.emissionPath.size())
                    return false;
                for (std::size_t i = 0; i < a.emissionPath.size(); ++i)
                    if (a.emissionPath[i].nodeId != b.emissionPath[i].nodeId ||
                        a.emissionPath[i].repeatIndex != b.emissionPath[i].repeatIndex)
                        return false;
                return true;
            };
            if (other.target == binding.target && other.visible != binding.visible &&
                !(sameSource(other.source, binding.source) && other.phase == binding.phase &&
                  (other.outcome != binding.outcome ||
                   (other.timing != binding.timing && other.timing != GameplayTimingClass::Any &&
                    binding.timing != GameplayTimingClass::Any))) &&
                (!other.end || binding.start.value < other.end->value) &&
                (!binding.end || other.start.value < binding.end->value))
                return core::unexpected(invalid(
                    "Potential conflicting FactBinding writes for the same Fact and target"));
        }
    }
    SessionConfiguration configuration{OwnedInputMappingProfile{mapping},
                                       OwnedTimebaseProfile{*g.timebase},
                                       *g.latePolicy,
                                       identities,
                                       c.calibration,
                                       c.engine[4],
                                       c.engine[5],
                                       c.engine[1],
                                       *preparedRules};
    // Validate the complete combination before publishing the immutable content handle.
    auto check = JudgementSession::create();
    if (!check)
        return core::unexpected(detail::projectGameplayError(std::move(check.error())));
    if (auto configured = check->configure(configuration); !configured)
        return core::unexpected(detail::projectGameplayError(std::move(configured.error())));
    if (auto prepared = check->prepare(decoded->gameplay); !prepared)
        return core::unexpected(detail::projectGameplayError(std::move(prepared.error())));
    auto publicConfiguration = encodeGameplayConfiguration(c);
    if (!publicConfiguration)
        return core::unexpected(std::move(publicConfiguration.error()));
    return detail::GameplayAccess::content(std::move(*decoded), std::move(configuration),
                                           std::move(bindings), c.enabledCapabilities,
                                           std::move(*publicConfiguration));
} catch (const std::exception& e) {
    return core::unexpected(invalid(e.what()));
} catch (...) {
    return core::unexpected(invalid("Gameplay content construction failed"));
}
} // namespace

auto GameplayContent::fromPacked(std::span<const std::byte> bytes, const GameplayConfiguration& c)
    -> core::Result<GameplayContent> {
    return makeGameplayContent(
        c, [&](const auto& context) { return gameplay_packed::decode(bytes, context); });
}
auto GameplayContent::fromGraph(std::string_view text, const GameplayConfiguration& c,
                                GameplayGraphDecodeBudget budget) -> core::Result<GameplayContent> {
    return makeGameplayContent(c, [&](const auto& context) {
        return gameplay_packed::decodeGraph(text, context,
                                            {budget.maxBytes, budget.maxDepth,
                                             budget.maxStringBytes, budget.maxValues,
                                             budget.maxContainerElements, budget.maxRowAtoms,
                                             budget.maxRowDecodedStringBytes, c.testOnly});
    });
}

auto GameplayContent::valid() const noexcept -> bool {
    return !!storage_;
}
auto GameplayReplay::cutAt(GameplayTick horizon) const -> core::Result<GameplayReplayCut> try {
    if (!storage_)
        return core::unexpected(invalid("Empty Replay"));
    const auto cut = judgement::replayCutAt(storage_->archive, judgement::Tick{horizon.value});
    return GameplayReplayCut{cut.completeRecords,
                             cut.partialHorizon
                                 ? std::optional{GameplayTick{cut.partialHorizon->value()}}
                                 : std::nullopt};
} catch (...) {
    return core::unexpected(invalid("Replay cut could not be derived"));
}

auto GameplayReplay::valid() const noexcept -> bool {
    return !!storage_;
}
auto GameplaySnapshot::valid() const noexcept -> bool {
    return !!storage_;
}
auto GameplayResult::state() const -> core::Result<GameplayState> {
    if (!storage_)
        return core::unexpected(invalid("Empty Gameplay result"));
    return storage_->state;
}
auto GameplayResult::score() const -> core::Result<GameplayScore> {
    if (!storage_ || !storage_->kernel.fold)
        return core::unexpected(invalid("Gameplay Fold absent"));
    const auto& f = *storage_->kernel.fold;
    return GameplayScore{f.score, f.combo, f.maxCombo, f.hits, f.misses};
}
auto GameplayResult::factCount() const -> core::Result<std::uint64_t> {
    if (!storage_)
        return core::unexpected(invalid("Empty Gameplay result"));
    return storage_->kernel.sealedFactCursor;
}
auto GameplayResult::publishableFactCount() const -> core::Result<std::uint64_t> {
    if (!storage_)
        return core::unexpected(invalid("Empty Gameplay result"));
    return storage_->kernel.fold ? storage_->kernel.fold->factCursor
                                 : storage_->kernel.sealedFactCursor;
}
auto GameplayResult::sameResult(const GameplayResult& other) const -> core::Result<bool> {
    if (!storage_ || !other.storage_)
        return core::unexpected(invalid("Empty Gameplay result"));
    return judgement::sameKernelResult(storage_->kernel, other.storage_->kernel);
}

auto GameplayCheckpoint::valid() const noexcept -> bool {
    return !!storage_;
}
auto GameplayReplay::fromBytes(std::span<const std::byte> bytes, const GameplayContent& content,
                               GameplayCodecBudget budget) -> core::Result<GameplayReplay> try {
    const auto* c = detail::GameplayAccess::content(content);
    if (!c || !budget.testOnly)
        return core::unexpected(invalid("Explicit content and test-only CodecBudget required"));
    auto replay =
        judgement::decodeReplay(bytes, {c->configuration, c->capsule.gameplay},
                                judgement::CodecBudget{budget.maxBytes, budget.maxRecords,
                                                       budget.maxElements, budget.testOnly});
    if (!replay)
        return core::unexpected(detail::projectGameplayError(std::move(replay.error())));
    return detail::GameplayAccess::replay(std::move(*replay));
} catch (...) {
    return core::unexpected(invalid("Replay decode failed"));
}
auto GameplayReplay::toBytes() const -> core::Result<std::vector<std::byte>> try {
    if (!storage_)
        return core::unexpected(invalid("Empty Replay"));
    return judgement::encodeReplay(storage_->archive);
} catch (...) {
    return core::unexpected(invalid("Replay encode failed"));
}
auto GameplaySnapshot::fromBytes(std::span<const std::byte> bytes, const GameplayContent& content,
                                 GameplayCodecBudget budget) -> core::Result<GameplaySnapshot> try {
    const auto* c = detail::GameplayAccess::content(content);
    if (!c || !budget.testOnly)
        return core::unexpected(invalid("Explicit content and test-only CodecBudget required"));
    auto snapshot =
        judgement::decodeSnapshot(bytes, {c->configuration, c->capsule.gameplay},
                                  judgement::CodecBudget{budget.maxBytes, budget.maxRecords,
                                                         budget.maxElements, budget.testOnly});
    if (!snapshot)
        return core::unexpected(detail::projectGameplayError(std::move(snapshot.error())));
    return detail::GameplayAccess::snapshot(std::move(*snapshot));
} catch (...) {
    return core::unexpected(invalid("Snapshot decode failed"));
}
auto GameplaySnapshot::toBytes() const -> core::Result<std::vector<std::byte>> try {
    if (!storage_)
        return core::unexpected(invalid("Empty Snapshot"));
    return judgement::encodeSnapshot(storage_->snapshot);
} catch (...) {
    return core::unexpected(invalid("Snapshot encode failed"));
}
} // namespace cuexis::playback
