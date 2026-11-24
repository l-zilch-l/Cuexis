#include <cuexis/judgement/prepared_gameplay.hpp>

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::judgement::runtime {
namespace detail {

class PreparedGameplayStorage final {
  public:
    PreparedTimebase timebase;
    LatePolicyParameters latePolicy;
    PreparedIdentityDeclarations identities;
    std::string contentIdentity, preparedIdentity, executionProfile;
    std::vector<PreparedRequirement> requirements;
    std::vector<PreparedResource> resources;
    std::vector<PreparedTimerKey> timers;
    std::optional<PreparedNormalizedScore> normalizedScore;
    SettlementDictionary dictionary;
};

} // namespace detail

namespace {

auto prepareError(std::string message, std::string reason, std::string path = {}) -> core::Error {
    auto error = core::Error{"judgement.execution.relation_invalid", std::move(message)}
                     .withContext("category", "invalid_relation")
                     .withContext("severity", "error")
                     .withContext("faulted", "false")
                     .withContext("reason", std::move(reason));
    if (!path.empty()) {
        error = error.withContext("fieldPath", std::move(path));
    }
    return error;
}

auto budgetError(std::string message, std::string path) -> core::Error {
    return core::Error{"capability.budget_insufficient", std::move(message)}
        .withContext("category", "budget_exceeded")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("fieldPath", std::move(path));
}

auto profileError(std::string message, std::string path) -> core::Error {
    return core::Error{"judgement.execution.profile_incomplete", std::move(message)}
        .withContext("category", "identity_closure_incomplete")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("fieldPath", std::move(path));
}

auto checkedAdd(std::uint64_t left, std::uint64_t right) -> core::Result<std::uint64_t> {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return core::unexpected(budgetError("the prepared record count overflowed", "/limits"));
    }
    return left + right;
}

auto checkLimit(const MeasuredParameter<std::uint64_t>& limit, std::uint64_t count,
                const char* path) -> core::Result<void> {
    if (const auto* value = limit.measuredValue(); value != nullptr && count > *value) {
        return core::unexpected(
            budgetError("the prepared record count exceeds the measured limit", path));
    }
    return {};
}

auto validateLatePolicy(const LatePolicyParameters& policy) -> core::Result<void> {
    if (!policy.finalizationWatermark.isMeasured() || !policy.maxQueueHop.isMeasured() ||
        !policy.windowCloseThreshold.isMeasured() || !policy.windowOpenThreshold.isMeasured() ||
        !policy.policy) {
        return core::unexpected(
            profileError("the late policy is not fully measured or selected", "/latePolicy"));
    }
    return {};
}

auto validateRequirementShape(const PreparedRequirement& requirement) -> core::Result<void> {
    for (const auto& phase : requirement.phases) {
        std::size_t targets = 0;
        for (const auto& target : requirement.timing.phaseTargets) {
            if (target.phase == phase.kind) {
                ++targets;
            }
        }
        if (targets != 1) {
            return core::unexpected(prepareError(
                "each declared phase requires exactly one phase target", "phase_target_missing"));
        }
    }
    for (const auto& window : requirement.timing.successWindows) {
        if (window.end < window.start) {
            return core::unexpected(
                prepareError("a success window ends before it starts", "success_window_order"));
        }
    }
    if (requirement.timing.body && requirement.timing.body->end < requirement.timing.body->start) {
        return core::unexpected(
            prepareError("the body interval ends before it starts", "body_interval"));
    }
    return {};
}

auto findRequirementIndex(std::span<const PreparedRequirement> requirements,
                          const RequirementIdentity& identity) -> std::optional<std::size_t> {
    for (std::size_t index = 0; index < requirements.size(); ++index) {
        if (requirements[index].identity == identity) {
            return index;
        }
    }
    return std::nullopt;
}

auto buildDictionary(const std::vector<PreparedRequirement>& requirements,
                     const std::optional<PreparedNormalizedScore>& score) -> SettlementDictionary {
    SettlementDictionary dictionary;
    for (const auto& requirement : requirements) {
        for (const auto& phase : requirement.phases) {
            dictionary.phases.push_back(SettlementPhase{requirement.identity, phase.kind});
        }
    }
    if (score) {
        std::set<std::string, std::less<>> grades;
        for (const auto& table : score->gradeTables) {
            for (const auto& interval : table.intervals) {
                grades.insert(interval.grade);
            }
        }
        dictionary.grades.assign(grades.begin(), grades.end());
    }
    return dictionary;
}

auto buildStorage(const CompiledGameplayView& view) -> detail::PreparedGameplayStorage {
    detail::PreparedGameplayStorage storage;
    storage.timebase = view.timebase;
    storage.latePolicy = view.latePolicy;
    storage.identities = view.identities;
    storage.contentIdentity = std::string{view.contentIdentity};
    storage.preparedIdentity = std::string{view.preparedIdentity};
    storage.executionProfile = std::string{view.executionProfile};
    storage.requirements.assign(view.requirements.begin(), view.requirements.end());
    storage.resources.assign(view.resources.begin(), view.resources.end());
    storage.timers.assign(view.timers.begin(), view.timers.end());
    if (view.normalizedScore != nullptr) {
        storage.normalizedScore = *view.normalizedScore;
    }
    storage.dictionary = buildDictionary(storage.requirements, storage.normalizedScore);
    return storage;
}

} // namespace

PreparedGameplay::PreparedGameplay(std::shared_ptr<const detail::PreparedGameplayStorage> storage)
    : storage_(std::move(storage)) {}

auto PreparedGameplay::records() const noexcept -> CompiledGameplayView {
    return CompiledGameplayView{
        storage_->timebase,
        storage_->latePolicy,
        storage_->identities,
        storage_->contentIdentity,
        storage_->preparedIdentity,
        storage_->executionProfile,
        nullptr,
        storage_->normalizedScore ? &*storage_->normalizedScore : nullptr,
        storage_->requirements,
        storage_->resources,
        storage_->timers,
    };
}

auto PreparedGameplay::requirements() const noexcept -> std::span<const PreparedRequirement> {
    return storage_->requirements;
}

auto PreparedGameplay::resources() const noexcept -> std::span<const PreparedResource> {
    return storage_->resources;
}

auto PreparedGameplay::timers() const noexcept -> std::span<const PreparedTimerKey> {
    return storage_->timers;
}

auto PreparedGameplay::identityDeclarations() const noexcept
    -> const PreparedIdentityDeclarations& {
    return storage_->identities;
}

auto PreparedGameplay::identity() const noexcept -> std::string_view {
    return storage_->preparedIdentity;
}

auto PreparedGameplay::admitsSuccess(std::size_t requirementIndex, Tick tick) const noexcept
    -> bool {
    if (requirementIndex >= storage_->requirements.size()) {
        return false;
    }
    const auto& requirement = storage_->requirements[requirementIndex];
    if (tick >= requirement.deadline) {
        return false;
    }
    return std::any_of(
        requirement.timing.successWindows.begin(), requirement.timing.successWindows.end(),
        [tick](const auto& window) { return tick >= window.start && tick < window.end; });
}

auto PreparedGameplay::settlementCapacity() const noexcept -> std::size_t {
    std::size_t capacity = 0;
    for (const auto& requirement : storage_->requirements) {
        capacity += requirement.phases.size();
    }
    return capacity;
}

auto PreparedGameplay::settlementDictionary() const noexcept
    -> const cuexis::judgement::SettlementDictionary& {
    return storage_->dictionary;
}

auto prepareCompiledGameplay(const CompiledGameplayView& view, const PrepareGameplayLimits& limits)
    -> core::Result<PreparedGameplay> {
    auto policy = validateLatePolicy(view.latePolicy);
    if (!policy) {
        return core::unexpected(policy.error());
    }

    //  Counts and measured limits. A pending dimension is not a number and is not enforced.
    std::uint64_t measureComponents = 0;
    std::uint64_t expandedNodes = 0;
    std::uint64_t automatonStates = 0;
    for (const auto& requirement : view.requirements) {
        auto shape = validateRequirementShape(requirement);
        if (!shape) {
            return core::unexpected(shape.error());
        }
        auto measure =
            checkedAdd(measureComponents, static_cast<std::uint64_t>(requirement.measure.size()));
        if (!measure) {
            return core::unexpected(measure.error());
        }
        measureComponents = *measure;
        if (requirement.expandedPatternNodes) {
            auto expanded = checkedAdd(expandedNodes, *requirement.expandedPatternNodes);
            if (!expanded) {
                return core::unexpected(expanded.error());
            }
            expandedNodes = *expanded;
        }
        if (requirement.executionProgram) {
            automatonStates = std::max<std::uint64_t>(
                automatonStates, requirement.executionProgram->accepting.size());
        }
    }
    if (auto check =
            checkLimit(limits.maxRequirements, view.requirements.size(), "/limits/maxRequirements");
        !check) {
        return core::unexpected(check.error());
    }
    if (auto check = checkLimit(limits.maxResources, view.resources.size(), "/limits/maxResources");
        !check) {
        return core::unexpected(check.error());
    }
    if (auto check = checkLimit(limits.maxMeasureComponents, measureComponents,
                                "/limits/maxMeasureComponents");
        !check) {
        return core::unexpected(check.error());
    }
    if (auto check = checkLimit(limits.maxExpandedPatternNodes, expandedNodes,
                                "/limits/maxExpandedPatternNodes");
        !check) {
        return core::unexpected(check.error());
    }
    if (auto check =
            checkLimit(limits.maxAutomatonStates, automatonStates, "/limits/maxAutomatonStates");
        !check) {
        return core::unexpected(check.error());
    }

    //  Resource rows: every requirement index resolves and no index appears twice.
    for (const auto& resource : view.resources) {
        std::vector<std::uint32_t> seen;
        for (const auto index : resource.requirementIndices) {
            if (index >= view.requirements.size()) {
                return core::unexpected(
                    prepareError("a prepared resource names a requirement outside the prepared set",
                                 "resource_requirement_range"));
            }
            if (std::find(seen.begin(), seen.end(), index) != seen.end()) {
                return core::unexpected(prepareError(
                    "a prepared resource lists one requirement twice", "resource_requirement_dup"));
            }
            seen.push_back(index);
        }
    }

    //  Timer rows: the requirement resolves and the phase is declared on it.
    for (const auto& timer : view.timers) {
        const auto index = findRequirementIndex(view.requirements, timer.requirement);
        if (!index) {
            return core::unexpected(
                prepareError("a prepared timer names an unknown requirement", "timer_requirement"));
        }
        const auto& requirement = view.requirements[*index];
        const bool declared =
            std::any_of(requirement.phases.begin(), requirement.phases.end(),
                        [&timer](const auto& phase) { return phase.kind == timer.phase; });
        if (!declared) {
            return core::unexpected(prepareError(
                "a prepared timer names a phase the requirement does not declare", "timer_phase"));
        }
    }

    //  Phase capacity P and the score cross-checks.
    std::uint64_t phaseCount = 0;
    for (const auto& requirement : view.requirements) {
        auto next = checkedAdd(phaseCount, requirement.phases.size());
        if (!next) {
            return core::unexpected(next.error());
        }
        phaseCount = *next;
    }
    if (phaseCount > std::numeric_limits<std::uint32_t>::max()) {
        return core::unexpected(
            budgetError("the prepared phase count has no uint32 representation", "/phases"));
    }
    if (view.normalizedScore != nullptr) {
        auto scoreValid = validatePreparedNormalizedScore(*view.normalizedScore);
        if (!scoreValid) {
            return core::unexpected(scoreValid.error());
        }
        auto derived = deriveScoreBasis(view.requirements, view.latePolicy, *view.normalizedScore);
        if (!derived) {
            return core::unexpected(derived.error());
        }
        if (derived->jMax != view.normalizedScore->basis.jMax) {
            return core::unexpected(
                prepareError("the declared jMax does not match the derived score basis",
                             "basis_mismatch", "/scoreBasis/jMax"));
        }
        if (derived->cMax != view.normalizedScore->basis.cMax) {
            return core::unexpected(
                prepareError("the declared cMax does not match the derived score basis",
                             "basis_mismatch", "/scoreBasis/cMax"));
        }
    }

    auto storage = std::make_shared<detail::PreparedGameplayStorage>(buildStorage(view));
    return PreparedGameplay{
        std::shared_ptr<const detail::PreparedGameplayStorage>{std::move(storage)}};
}

} // namespace cuexis::judgement::runtime
