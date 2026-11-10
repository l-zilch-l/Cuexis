#pragma once

#include <cuexis/judgement/gameplay_assembler.hpp>

namespace cuexis::judgement {

enum class PreparedTimerKind : std::uint8_t {
    activation,
    windowOpen,
    bodyEnd,
    phaseClose,
    hardDeadline
};

struct PreparedTimerKey final {
    RequirementIdentity requirement;
    PhaseKind phase;
    PreparedTimerKind kind;
    Tick tick;
    std::optional<Tick> windowStart;
    std::optional<Tick> windowEnd;
    friend auto operator==(const PreparedTimerKey&, const PreparedTimerKey&) -> bool = default;
};

struct LateRoute final {
    Tick dispatchTick;
    bool wasForwarded;
};

[[nodiscard]] auto validateExecutionLateParameters(const LatePolicyParameters& policy)
    -> core::Result<void>;
[[nodiscard]] auto routeExecutionObservation(const LatePolicyParameters& policy, Tick tick,
                                             std::optional<Tick> horizon, bool wasForwarded = false)
    -> core::Result<LateRoute>;
[[nodiscard]] auto validateExecutionRequirement(const RequirementRecord& requirement,
                                                const PatternExecutionProgram& program)
    -> core::Result<void>;
[[nodiscard]] auto makeExecutionTimers(const CanonicalGameplayGraph& graph)
    -> core::Result<std::vector<PreparedTimerKey>>;

} // namespace cuexis::judgement
