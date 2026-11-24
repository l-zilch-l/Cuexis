#include <cuexis/judgement/compile/legacy_prepared_view.hpp>

#include <cuexis/judgement/gameplay_assembler_identity.hpp>
#include <cuexis/judgement/gameplay_assembler_publication.hpp>
#include <cuexis/judgement/prepared_gameplay_records.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::judgement::compile {
namespace {

auto conversionError(std::string message, std::string reason) -> core::Error {
    return core::Error{"judgement.execution.relation_invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason));
}

auto convertSpan(const MeasuredParameter<TickSpan>& source)
    -> runtime::MeasuredParameter<TickSpan> {
    if (source.isMeasured()) {
        return runtime::MeasuredParameter<TickSpan>::measured(
            TickSpan{source.measuredValue()->value()});
    }
    return {};
}

auto convertPolicy(const LatePolicyParameters& source) -> runtime::LatePolicyParameters {
    runtime::LatePolicyParameters converted;
    converted.finalizationWatermark = convertSpan(source.finalizationWatermark);
    converted.maxQueueHop = convertSpan(source.maxQueueHop);
    converted.windowCloseThreshold = convertSpan(source.windowCloseThreshold);
    converted.windowOpenThreshold = convertSpan(source.windowOpenThreshold);
    if (source.policy) {
        converted.policy = static_cast<runtime::LateEventPolicy>(*source.policy);
    }
    return converted;
}

auto convertTiming(const RequirementRecord::Timing& source) -> runtime::RequirementTiming {
    runtime::RequirementTiming converted;
    converted.end = source.end;
    for (const auto& window : source.successWindows) {
        converted.successWindows.push_back(
            runtime::SuccessWindow{window.start, window.end, window.phase});
    }
    converted.body = source.body;
    converted.phaseTargets.reserve(source.phaseTargets.size());
    for (const auto& target : source.phaseTargets) {
        converted.phaseTargets.push_back(runtime::PhaseTarget{target.phase, target.chartTick});
    }
    return converted;
}

auto convertMeasure(const MeasureSpecDeclaration& source)
    -> std::vector<runtime::CompiledMeasureComponent> {
    std::vector<runtime::CompiledMeasureComponent> converted;
    converted.reserve(source.components.size());
    for (const auto& component : source.components) {
        runtime::CompiledMeasureComponent out;
        out.phase = component.phase;
        out.category = factCategoryOfPhase(component.phase);
        out.grade = component.gradeTable.has_value() ? runtime::GradePresence::declared
                                                     : runtime::GradePresence::absent;
        out.declaredGradeTokens = component.declaredGradeTokens;
        converted.push_back(std::move(out));
    }
    return converted;
}

auto convertAtomBindings(const std::vector<AtomBinding>& source)
    -> std::vector<runtime::AtomBinding> {
    std::vector<runtime::AtomBinding> converted;
    converted.reserve(source.size());
    for (const auto& binding : source) {
        runtime::AtomBinding out;
        out.atomRef = binding.atomRef;
        out.domainToken = binding.domainToken;
        out.sourceClass = binding.sourceClass;
        out.channelToken = binding.channelToken;
        out.action = static_cast<runtime::InputAction>(binding.action);
        if (binding.amountRange) {
            out.amountRange = runtime::AmountMatchRange{binding.amountRange->minimum,
                                                        binding.amountRange->maximum};
        }
        out.tailOnly = binding.tailOnly;
        converted.push_back(std::move(out));
    }
    return converted;
}

auto convertClaims(const std::vector<ResourceClaimDeclaration>& source)
    -> std::vector<runtime::ResourceClaimDeclaration> {
    std::vector<runtime::ResourceClaimDeclaration> converted;
    converted.reserve(source.size());
    for (const auto& claim : source) {
        runtime::ResourceClaimDeclaration out;
        out.resourceId = claim.resourceRef.resourceId;
        out.claimPolicy.policyToken = claim.claimPolicy.policyToken;
        out.claimPolicy.claimKeyToken = claim.claimPolicy.claimKeyToken;
        if (claim.claimPolicy.competition) {
            out.claimPolicy.competition = runtime::ClaimPolicyDeclaration::CompetitionKey{
                claim.claimPolicy.competition->priority, claim.claimPolicy.competition->tieRank};
        }
        out.intent = static_cast<runtime::ResourceClaimIntent>(claim.intent);
        out.graceOverride.mode = static_cast<runtime::GraceOverrideMode>(claim.graceOverride.mode);
        out.graceOverride.overrideToken = claim.graceOverride.overrideToken;
        converted.push_back(std::move(out));
    }
    return converted;
}

auto convertProgram(const PatternExecutionProgram& source) -> runtime::PatternExecutionProgram {
    runtime::PatternExecutionProgram converted;
    converted.atomRefs = source.atomRefs;
    converted.start = source.start;
    converted.accepting = source.accepting;
    converted.transitions = source.transitions;
    converted.live = source.live;
    return converted;
}

auto convertTimebase(const TimebaseProfile& source) -> runtime::PreparedTimebase {
    //  The runtime prepared record keeps the engine's integer microsecond rational. Legacy profiles
    //  carry an exact rational tick scale; the bridge preserves the declared numerator/denominator
    //  and records zero origin because preparation has already mapped author beats to ticks.
    return runtime::PreparedTimebase{std::string{source.profileId}, source.tickScale.numerator(),
                                     source.tickScale.denominator(), Tick{0}};
}

} // namespace

auto preparedGameplayView(const PreparedGameplay& prepared)
    -> core::Result<runtime::PreparedGameplay> {
    const auto& assembled = prepared.assembled();
    const auto& graph = assembled.graph;
    if (graph.timebase == nullptr || graph.latePolicy == nullptr) {
        return core::unexpected(conversionError(
            "the prepared gameplay has no timebase or late policy", "prepared_profile_missing"));
    }
    const auto requirements = prepared.requirements();
    if (requirements.size() != graph.requirements.size()) {
        return core::unexpected(conversionError(
            "the prepared requirement list and its graph disagree", "prepared_requirement_count"));
    }

    std::vector<runtime::PreparedRequirement> convertedRequirements;
    convertedRequirements.reserve(requirements.size());
    for (std::size_t index = 0; index < requirements.size(); ++index) {
        const auto& source = graph.requirements[index];
        const auto& preparedRequirement = requirements[index];
        if (!source.timing) {
            return core::unexpected(
                conversionError("a prepared requirement has no timing record", "prepared_timing"));
        }
        runtime::PreparedRequirement out;
        out.stableId = source.stableId;
        out.identity = source.identity;
        out.phases = source.phases;
        out.timing = convertTiming(*source.timing);
        out.deadline = preparedRequirement.deadline;
        out.measure = convertMeasure(source.measure);
        out.atomBindings = convertAtomBindings(source.atomBindings);
        out.resourceClaims = convertClaims(source.resourceClaims);
        if (source.independentCompetition) {
            out.independentCompetition = runtime::ClaimPolicyDeclaration::CompetitionKey{
                source.independentCompetition->priority, source.independentCompetition->tieRank};
        }
        if (preparedRequirement.executionProgram) {
            out.executionProgram = convertProgram(*preparedRequirement.executionProgram);
        }
        convertedRequirements.push_back(std::move(out));
    }

    std::vector<runtime::PreparedResource> convertedResources;
    convertedResources.reserve(prepared.resources().size());
    for (const auto& source : prepared.resources()) {
        runtime::PreparedResource out;
        out.resourceId = source.resourceId;
        const auto resource = std::find_if(graph.resources.begin(), graph.resources.end(),
                                           [&source](const auto& candidate) {
                                               return candidate.ref.resourceId == source.resourceId;
                                           });
        if (resource != graph.resources.end()) {
            out.slotToken = resource->slotToken;
            out.terminalAfterTermination = resource->terminalAfterTermination;
        }
        for (const auto requirementIndex : source.requirementIndices) {
            if (requirementIndex > std::numeric_limits<std::uint32_t>::max()) {
                return core::unexpected(conversionError(
                    "a prepared resource index has no uint32 representation", "resource_index"));
            }
            out.requirementIndices.push_back(static_cast<std::uint32_t>(requirementIndex));
        }
        convertedResources.push_back(std::move(out));
    }

    std::vector<runtime::PreparedTimerKey> convertedTimers;
    convertedTimers.reserve(prepared.timers().size());
    for (const auto& source : prepared.timers()) {
        runtime::PreparedTimerKey out;
        out.requirement = source.requirement;
        out.phase = source.phase;
        out.kind = static_cast<runtime::PreparedTimerKind>(source.kind);
        out.tick = source.tick;
        out.windowStart = source.windowStart;
        out.windowEnd = source.windowEnd;
        convertedTimers.push_back(std::move(out));
    }

    const auto timebase = convertTimebase(*graph.timebase);
    const auto policy = convertPolicy(*graph.latePolicy);
    const auto contentIdentity = assembled.content.canonicalBytes().toHex();
    const auto preparedIdentity = assembled.prepared.canonicalBytes().toHex();
    const runtime::CompiledGameplayView view{
        timebase,
        policy,
        prepared.identityDeclarations(),
        contentIdentity,
        preparedIdentity,
        graph.executionProfile,
        nullptr,
        nullptr,
        convertedRequirements,
        convertedResources,
        convertedTimers,
    };

    //  `prepareCompiledGameplay` copies every record again into the owning runtime storage. The
    //  temporary view and the legacy handle may die immediately afterwards.
    runtime::PrepareGameplayLimits limits{};
    return runtime::prepareCompiledGameplay(view, limits);
}

} // namespace cuexis::judgement::compile
