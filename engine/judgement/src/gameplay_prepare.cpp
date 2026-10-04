#include <cuexis/judgement/gameplay_prepare.hpp>

#include "claim_key_internal.hpp"
#include "source_codes.hpp"

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <exception>
#include <map>
#include <set>

namespace cuexis::judgement {
namespace {

auto invalid(std::string_view path, std::string_view summary) -> core::Error {
    return toError(Diagnostic{
        .code = codes::kDeclarationIncompleteCode,
        .category = codes::kInvalidRelationCategory,
        .severity = codes::kErrorSeverity,
        .faulted = false,
        .summary = summary,
        .context =
            DiagnosticContext{.fieldPath = {.section = codes::kGraphSection, .path = path},
                              .requirement = {.kind = codes::kAbsent, .identity = codes::kAbsent},
                              .identity = {.component = codes::kAbsent, .token = codes::kAbsent},
                              .budget = nullptr,
                              .capabilityId = codes::kAbsent,
                              .remediation = codes::kAbsent,
                              .rawTime = codes::kAbsent}});
}

auto validateTiming(const RequirementRecord& requirement) -> core::Result<Tick> {
    if (requirement.preparedGrace.span().value() < 0) {
        return core::unexpected(invalid("requirements.preparedGrace",
                                        "frozen grace must be a nonnegative tick displacement"));
    }
    switch (requirement.grace.policy) {
    case GraceResolutionPolicy::explicitDeclaration:
    case GraceResolutionPolicy::defaultDeclaration:
        if (!requirement.grace.inheritedFromDeclarationId.empty()) {
            return core::unexpected(invalid("requirements.grace.inheritedFromDeclarationId",
                                            "only inherited provenance may carry a literal id"));
        }
        break;
    case GraceResolutionPolicy::inheritedDeclaration:
        if (requirement.grace.inheritedFromDeclarationId.empty()) {
            return core::unexpected(invalid("requirements.grace.inheritedFromDeclarationId",
                                            "inherited provenance must carry its literal id"));
        }
        break;
    default:
        return core::unexpected(invalid("requirements.grace.policy",
                                        "frozen grace provenance must name a registered source"));
    }
    if (!requirement.timing) {
        return core::unexpected(
            invalid("requirements.timing", "a prepared requirement needs timing"));
    }
    const auto& timing = *requirement.timing;
    const bool hasTail =
        std::any_of(requirement.phases.begin(), requirement.phases.end(),
                    [](const auto& phase) { return phase.kind == PhaseKind::tail; });
    const auto deadline = hasTail ? offsetTicks(timing.end, requirement.preparedGrace.span())
                                  : core::Result<Tick>{timing.end};
    if (!deadline) {
        return core::unexpected(deadline.error());
    }
    if (timing.successWindows.empty()) {
        return core::unexpected(invalid("requirements.timing.successWindows",
                                        "success windows must be explicitly declared"));
    }
    for (std::size_t i = 0; i < timing.successWindows.size(); ++i) {
        const auto& window = timing.successWindows[i];
        if (std::find(requirement.phases.begin(), requirement.phases.end(), window.phase) ==
            requirement.phases.end()) {
            return core::unexpected(invalid("requirements.timing.successWindows.phase",
                                            "a success window must reference a declared phase"));
        }
        if (window.start >= window.end || window.end > *deadline ||
            (i > 0 && timing.successWindows[i - 1].phase == window.phase &&
             timing.successWindows[i - 1].end > window.start)) {
            return core::unexpected(invalid("requirements.timing.successWindows",
                                            "windows must be disjoint, nonempty and end by D"));
        }
    }
    const bool hasBody =
        std::any_of(requirement.phases.begin(), requirement.phases.end(),
                    [](const auto& phase) { return phase.kind == PhaseKind::body; });
    if (hasBody != timing.body.has_value()) {
        return core::unexpected(invalid("requirements.timing.body",
                                        "body timing and body phase must be declared together"));
    }
    if (timing.body && (timing.body->start >= timing.body->end || timing.body->end > timing.end)) {
        return core::unexpected(
            invalid("requirements.timing.body",
                    "body must be nonempty and end no later than the declared end"));
    }
    if (timing.body) {
        const auto bodyStart = timing.body->start;
        const auto bodyEnd = timing.body->end;
        bool hasHeadCoverage = false;
        for (const auto& window : timing.successWindows) {
            if (window.phase.kind == PhaseKind::head && window.phase.declarationOrdinal == 1U &&
                window.start <= bodyStart) {
                hasHeadCoverage = true;
            }
            if (window.phase.kind == PhaseKind::tail &&
                (window.start < bodyEnd || window.end > *deadline)) {
                return core::unexpected(
                    invalid("requirements.timing.successWindows",
                            "tail windows must begin after body completion and end by D"));
            }
        }
        if (!hasHeadCoverage) {
            return core::unexpected(
                invalid("requirements.timing.successWindows",
                        "the head window must establish coverage no later than body start"));
        }
    }
    return *deadline;
}

auto validateSolverProfiles(const CanonicalGameplayGraph& graph) -> core::Result<void> {
    for (const auto& profile : graph.solverProfiles) {
        if (profile.algorithmToken != "coordinator.policy.greedy_v1" ||
            profile.objective != std::vector<std::string>{"first-eligible"} ||
            profile.tieBreak != std::vector<std::string>{"priority.asc", "tieRank.asc"} ||
            !profile.rejectIfNonUnique) {
            return core::unexpected(
                invalid("solverProfiles",
                        "the prepared solver profile is outside the closed Stage 7A subset"));
        }
    }
    return {};
}

} // namespace

namespace detail {
class PreparedGameplayStorage final {
  public:
    PreparedGameplayStorage(const TimebaseProfile& profile, const LatePolicyParameters& policy)
        : profileId(profile.profileId), unitToken(profile.unitToken), timebase(profile),
          latePolicy(policy) {
        timebase.profileId = profileId;
        timebase.unitToken = unitToken;
    }
    std::string profileId;
    std::string unitToken;
    TimebaseProfile timebase;
    LatePolicyParameters latePolicy;
    std::optional<AssembledGameplay> assembled;
    std::vector<PreparedRequirement> requirements;
    std::vector<PreparedResource> resources;
};
} // namespace detail

auto PreparedGameplay::assembled() const noexcept -> const AssembledGameplay& {
    return *storage_->assembled;
}

auto PreparedGameplay::requirements() const noexcept -> std::span<const PreparedRequirement> {
    return storage_->requirements;
}

auto PreparedGameplay::resources() const noexcept -> std::span<const PreparedResource> {
    return storage_->resources;
}

auto PreparedGameplay::admitsSuccess(std::size_t index, Tick tick) const noexcept -> bool {
    if (index >= storage_->requirements.size() || tick >= storage_->requirements[index].deadline) {
        return false;
    }
    const auto& windows = storage_->assembled->graph.requirements[index].timing->successWindows;
    return std::any_of(windows.begin(), windows.end(), [tick](const auto& window) {
        return tick >= window.start && tick < window.end;
    });
}

namespace {

auto prepareStorage(const GameplayPrepareRequest& request, bool resolved)
    -> core::Result<std::shared_ptr<const detail::PreparedGameplayStorage>> try {
    if (!request.assembly.timebase || !request.assembly.latePolicy) {
        return core::unexpected(invalid("timebaseRef", "prepare requires profile and late policy"));
    }
    // Enforce accepted compiler limits before the assembler performs its semantic proofs.
    for (const auto& source : request.assembly.sources) {
        for (const auto& requirement : source.document.requirements) {
            auto compiled = compilePattern(requirement.pattern, request.patternBudget);
            if (!compiled) {
                return core::unexpected(std::move(compiled.error()));
            }
        }
    }
    auto storage = std::make_shared<detail::PreparedGameplayStorage>(*request.assembly.timebase,
                                                                     *request.assembly.latePolicy);
    auto assembly = request.assembly;
    assembly.timebase = &storage->timebase;
    assembly.latePolicy = &storage->latePolicy;
    if (!resolved) {
        std::map<StableDeclarationId, const GraceResolutionInputs*> bindings;
        for (const auto& binding : request.graceInputs) {
            if (!bindings.emplace(binding.requirement, &binding.inputs).second) {
                return core::unexpected(
                    invalid("graceInputs", "duplicate requirement grace binding"));
            }
        }
        std::map<std::string, RationalDuration> named;
        for (const auto& source : request.namedGraceDurations) {
            if (source.declarationId.empty() ||
                !named.emplace(source.declarationId, source.duration).second) {
                return core::unexpected(
                    invalid("namedGraceDurations", "duplicate or empty literal id"));
            }
        }
        std::set<StableDeclarationId> used;
        for (auto& source : assembly.sources) {
            for (auto& requirement : source.document.requirements) {
                const auto binding = bindings.find(requirement.stableId);
                if (binding == bindings.end()) {
                    return core::unexpected(
                        invalid("graceInputs", "missing requirement grace binding"));
                }
                used.insert(requirement.stableId);
                auto inputs = *binding->second;
                if (requirement.grace.policy == GraceResolutionPolicy::inheritedDeclaration) {
                    const auto literal = named.find(requirement.grace.inheritedFromDeclarationId);
                    if (literal == named.end() || (inputs.inheritedDuration &&
                                                   *inputs.inheritedDuration != literal->second)) {
                        return core::unexpected(
                            invalid("graceInputs.inheritedDuration",
                                    "inheritance must resolve one named literal"));
                    }
                    inputs.inheritedDuration = literal->second;
                } else if (inputs.inheritedDuration) {
                    return core::unexpected(
                        invalid("graceInputs.inheritedDuration",
                                "non-inherited grace cannot supply inheritance"));
                }
                // The stored value is a tick displacement. Other quantization units require
                // explicit offline lowering, not relabelling a canonical unit count as ticks.
                if (inputs.unitInTicks.numerator() != inputs.unitInTicks.denominator()) {
                    return core::unexpected(
                        invalid("graceInputs.unitInTicks",
                                "prepared tick grace requires one tick per unit"));
                }
                auto grace = resolvePreparedGrace(requirement.grace, inputs);
                if (!grace) {
                    return core::unexpected(grace.error());
                }
                requirement.preparedGrace = *grace;
            }
        }
        if (used.size() != bindings.size()) {
            return core::unexpected(
                invalid("graceInputs", "grace binding references no requirement"));
        }
    }
    auto assembled = assembleGameplay(assembly);
    if (!assembled) {
        return core::unexpected(assembled.error());
    }
    auto solverProfiles = validateSolverProfiles(assembled->graph);
    if (!solverProfiles) {
        return core::unexpected(std::move(solverProfiles.error()));
    }
    for (const auto& requirement : assembled->graph.requirements) {
        const auto pattern = compilePattern(requirement.pattern, request.patternBudget);
        if (!pattern) {
            return core::unexpected(pattern.error());
        }
        if (!pattern->atomRefs().empty() && requirement.patternArmRefs.empty()) {
            return core::unexpected(invalid("requirements.patternArmRefs",
                                            "nonempty patterns require explicit arm membership"));
        }
        if (std::adjacent_find(requirement.patternArmRefs.begin(),
                               requirement.patternArmRefs.end()) !=
            requirement.patternArmRefs.end()) {
            return core::unexpected(
                invalid("requirements.patternArmRefs", "duplicate arm reference"));
        }
        const auto measure = compileMeasure(
            requirement.measure, MeasurePhaseContext{.declaredPhases = requirement.phases,
                                                     .requiresReleaseTailSemantics =
                                                         requirement.requiresReleaseTailSemantics,
                                                     .fieldPathPrefix = "requirements"});
        if (!measure) {
            return core::unexpected(measure.error());
        }
        const auto deadline = validateTiming(requirement);
        if (!deadline) {
            return core::unexpected(deadline.error());
        }
        if (requirement.resourceClaims.size() > 1) {
            return core::unexpected(
                invalid("requirements.resourceClaims", "Stage 7A fanout is one"));
        }
        storage->requirements.push_back({requirement.stableId, *pattern, *measure, *deadline});
    }
    for (const auto& resource : assembled->graph.resources) {
        ResourceClaimResolutionInputs input{.declaredCapacity = resource.declaredCapacity,
                                            .intents = {},
                                            .candidates = {},
                                            .preparedGrace = PreparedGrace{TickSpan{0}}};
        std::map<std::string, std::size_t> owners;
        std::vector<std::size_t> observeIndices;
        for (std::size_t i = 0; i < assembled->graph.requirements.size(); ++i) {
            for (const auto& claim : assembled->graph.requirements[i].resourceClaims) {
                if (claim.resourceRef.resourceId != resource.ref.resourceId) {
                    continue;
                }
                if (claim.intent != ResourceClaimIntent::observe &&
                    !claim.claimPolicy.competition) {
                    return core::unexpected(
                        invalid("resourceClaims.competition",
                                "occupying candidates require an explicit pair"));
                }
                const auto claimKey =
                    claim.intent == ResourceClaimIntent::observe
                        ? claim.claimPolicy.claimKeyToken
                        : detail::structuralClaimKey(claim.claimPolicy.claimKeyToken,
                                                     assembled->graph.requirements[i].identity);
                if (claim.intent != ResourceClaimIntent::observe &&
                    claim.claimPolicy.claimKeyToken.empty()) {
                    return core::unexpected(invalid("resourceClaims.explicitNamespace",
                                                    "an occupying namespace must be nonempty"));
                }
                input.candidates.push_back({claim.intent, claim.claimPolicy.policyToken, claimKey,
                                            claim.graceOverride.mode,
                                            claim.claimPolicy.competition});
                if (claim.intent == ResourceClaimIntent::observe) {
                    observeIndices.push_back(i);
                } else {
                    owners.emplace(claimKey, i);
                }
            }
        }
        auto claims = resolveResourceClaims(input);
        if (!claims) {
            return core::unexpected(claims.error());
        }
        std::vector<std::size_t> indices;
        std::sort(observeIndices.begin(), observeIndices.end(),
                  [&assembled](std::size_t left, std::size_t right) {
                      const auto& l = assembled->graph.requirements[left].resourceClaims.front();
                      const auto& r = assembled->graph.requirements[right].resourceClaims.front();
                      return std::pair{l.claimPolicy.policyToken, left} <
                             std::pair{r.claimPolicy.policyToken, right};
                  });
        std::size_t observer = 0;
        // Observe rows do not compete. Retain their canonical requirement order separately from
        // the pair-ordered occupying rows; no identity is ever used to break an occupying tie.
        for (const auto& candidate : claims->candidates()) {
            if (candidate.intent == ResourceClaimIntent::observe) {
                indices.push_back(observeIndices[observer++]);
            } else {
                indices.push_back(owners.at(candidate.claimKeyToken));
            }
        }
        storage->resources.push_back({resource.ref.resourceId, *claims, std::move(indices)});
    }
    storage->assembled = std::move(*assembled);
    return std::shared_ptr<const detail::PreparedGameplayStorage>{std::move(storage)};
} catch (const std::exception&) {
    return core::unexpected(invalid("prepare", "prepare could not produce an owning publication"));
}

} // namespace

auto prepareGameplay(const GameplayPrepareRequest& request) -> core::Result<PreparedGameplay> {
    auto storage = prepareStorage(request, false);
    if (!storage) {
        return core::unexpected(std::move(storage.error()));
    }
    return PreparedGameplay{std::move(*storage)};
}

auto prepareResolvedGameplay(const ResolvedGameplayPrepareRequest& request)
    -> core::Result<PreparedGameplay> {
    auto storage = prepareStorage(
        GameplayPrepareRequest{request.assembly, {}, {}, request.patternBudget}, true);
    if (!storage) {
        return core::unexpected(std::move(storage.error()));
    }
    return PreparedGameplay{std::move(*storage)};
}

auto prepareInto(PreparedGameplayPublication& publication, const GameplayPrepareRequest& request)
    -> core::Result<void> {
    auto prepared = prepareGameplay(request);
    if (!prepared) {
        return core::unexpected(prepared.error());
    }
    publication.active_ = std::move(*prepared);
    return {};
}

auto prepareResolvedInto(PreparedGameplayPublication& publication,
                         const ResolvedGameplayPrepareRequest& request) -> core::Result<void> {
    auto prepared = prepareResolvedGameplay(request);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    publication.active_ = std::move(*prepared);
    return {};
}

} // namespace cuexis::judgement
