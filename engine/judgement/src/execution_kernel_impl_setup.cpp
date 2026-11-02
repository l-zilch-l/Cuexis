#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

auto validateSessionConfiguration(const SessionConfiguration& c) -> core::Result<void> try {
    const auto& engine = c.identityDeclarations.engine;
    if (c.executionProfileToken.empty() || c.lateAlgorithmToken.empty() ||
        c.factSemanticRevision.empty() || c.calibrationIdentityToken.empty() ||
        (!engine.executionProfileToken || engine.executionProfileToken->empty()) ||
        (!engine.lateAlgorithmToken || engine.lateAlgorithmToken->empty()) ||
        engine.judgementSemanticRevision.empty() || engine.factSemanticRevision.empty() ||
        engine.fixedPointTableId.empty() || engine.coordinationPhaseOrderToken.empty()) {
        return core::unexpected(core::Error{"judgement.execution.profile_incomplete",
                                            "execution configuration tokens are incomplete"}
                                    .withContext("category", "identity_closure_incomplete")
                                    .withContext("severity", "error")
                                    .withContext("faulted", "false"));
    }
    if ((c.executionProfileToken != "gameplay.execution.t4-k4.v1" &&
         c.executionProfileToken != "gameplay.execution.t4-k4.rpa.v1") ||
        c.lateAlgorithmToken != (c.executionProfileToken == "gameplay.execution.t4-k4.rpa.v1"
                                     ? "late.window.batch.v1"
                                     : "late.window.logical.v1") ||
        c.factSemanticRevision != (c.executionProfileToken == "gameplay.execution.t4-k4.rpa.v1"
                                       ? "fact.semantic.rpa.v1"
                                       : "fact.semantic.phase-local.v1") ||
        c.calibrationIdentityToken.empty() ||
        engine.executionProfileToken != c.executionProfileToken ||
        engine.lateAlgorithmToken != c.lateAlgorithmToken ||
        engine.judgementSemanticRevision != "judgement.t4-k4.v1" ||
        engine.factSemanticRevision != c.factSemanticRevision ||
        engine.fixedPointTableId != "fixed-point.none.v1" ||
        engine.coordinationPhaseOrderToken != "coordination.six-eight.t4-k4.v1") {
        return core::unexpected(
            core::Error{"judgement.execution.profile_unsupported",
                        "execution configuration tokens are unsupported or inconsistent"}
                .withContext("category", "capability_disabled")
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    }
    auto mapping = c.inputMapping.view();
    if (auto valid = validateInputMapping(mapping); !valid) {
        return core::unexpected(valid.error());
    }
    if (auto valid = validatePrepare(c.timebase.view(), c.latePolicy); !valid) {
        return core::unexpected(valid.error());
    }
    return validateExecutionLateParameters(c.latePolicy);
} catch (const std::exception&) {
    return core::unexpected(error("execution.relation_invalid", "configuration allocation failed"));
}

ExecutionKernel::Impl::Impl(SessionConfiguration c, PreparedGameplay p,
                            std::shared_ptr<const RuntimeState> state,
                            std::shared_ptr<const ExecutionPreparation> cached)
    : configuration(std::move(c)), prepared(std::move(p)), active(std::move(state)),
      faultReserve(std::make_shared<RuntimeState>(*active)),
      failureResult(
          error("kernel.transaction_failed", "the unsealed Tick transaction failed", true)),
      foldFailureResult(
          core::Error{"ruleset.transaction_failed", "the sealed Tick Fold transaction failed"}
              .withContext("category", "invalid_relation")
              .withContext("severity", "error")
              .withContext("faulted", "true")) {
    const bool reused = static_cast<bool>(cached);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaObservation::seekAdd(SeekPhase::s2CandidatePrep, &SeekWork::initialReserveCopies);
    rpaObservation::SeekWorkTimer indexTimer{SeekPhase::s2CandidatePrep, &SeekWork::derivedIndexUs};
    if (!reused)
        rpaObservation::seekAdd(SeekPhase::s2CandidatePrep, &SeekWork::derivedIndexBuilds);
#endif
    if (cached) {
        preparation = std::move(cached);
        recoveryInputs = preparation->inputs;
    } else {
        auto plan = std::make_shared<ExecutionPreparation>();
        plan->initial = active;
        std::map<RequirementIdentity, std::size_t> indices;
        plan->phaseOffsets.push_back(0);
        for (std::size_t i = 0; i < graph().requirements.size(); ++i) {
            const auto& r = graph().requirements[i];
            if (!indices.emplace(r.identity, i).second)
                throw std::logic_error{"duplicate prepared requirement"};
            plan->phaseOffsets.push_back(plan->phaseOffsets.back() + r.phases.size());
        }
        for (const auto& timer : prepared.timers()) {
            const auto found = indices.find(timer.requirement);
            if (found == indices.end())
                throw std::logic_error{"timer requirement missing"};
            plan->timerRequirements.push_back(found->second);
        }
        plan->inputs =
            std::make_shared<const RecoveryInputs>(RecoveryInputs{configuration, prepared});
        recoveryInputs = plan->inputs;
        preparation = std::move(plan);
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    indexTimer.finish();
#endif
    if (!reused)
        validatePhaseLayout(*active);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (!reused)
        rpaObservation::seekAdd(SeekPhase::s2CandidatePrep, &SeekWork::recoveryConfigCopies);
#endif
    recording =
        std::make_shared<const ReplayData>(ReplayData{{}, 0, active->projection.judgementIdentity});
    faultReserve->projection.state = KernelSessionState::Faulted;
    faultReserve->projection.faultDiagnostic = failureResult;
}

auto ExecutionKernel::Impl::recordingCandidate() -> std::shared_ptr<ReplayData> {
    if (active->projection.rpa)
        reservedLog = reserveLogAppend(logTail);
    return std::make_shared<ReplayData>(*recording);
}

void ExecutionKernel::Impl::publishRecording(std::shared_ptr<ReplayData> value) noexcept {
    if (active->projection.rpa) {
        reservedLog->records.push_back(std::move(value->records.back()));
        logTail = std::move(reservedLog);
        value->records.clear();
    }
    recording = std::move(value);
    ++appliedRecords;
    // Capture existing immutable owners, not a DTO/encoding or a second dynamic state.
    // Optional allocation failure cannot retroactively fail a sealed operation.
    if (nativeAnchorEligible && active->projection.rpa && !testControls && pending.empty() &&
        active->projection.state == KernelSessionState::Prepared &&
        active->projection.lastAdvanceHorizon && active->signalVisibility.empty() &&
        (nativePoints.empty() || appliedRecords % ExecutionLogChunk::capacity == 0)) {
        try {
            auto point = std::make_shared<const NativeRecoveryPoint>(
                NativeRecoveryPoint{active, logTail, recording, appliedRecords});
            if (nativePoints.size() == NativeRecoveryIndex::maxAnchors) {
                // Preserve the earliest anchor; evict the oldest later anchor. Eviction
                // affects only acceleration, never validity or the owning source history.
                nativePoints.erase(nativePoints.begin() + 1);
            }
            nativePoints.push_back(std::move(point));
        } catch (const std::exception&) {
        }
    }
}

void ExecutionKernel::Impl::releaseSeekSource() noexcept {
    seekSource.reset();
    seekNativeSource.reset();
    seekCanonicalCache.reset();
}

auto ExecutionKernel::Impl::reserveFault(const RuntimeState& value) const
    -> std::shared_ptr<RuntimeState> {
    auto backup = std::make_shared<RuntimeState>(value);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaObservation::seekAdd(SeekPhase::s4InternalExecution, &SeekWork::faultReserveCopies);
#endif
    backup->projection.state = KernelSessionState::Faulted;
    backup->projection.faultDiagnostic = failureResult;
    return backup;
}

auto ExecutionKernel::Impl::graph() const -> const CanonicalGameplayGraph& {
    return prepared.assembled().graph;
}

void ExecutionKernel::Impl::failIfRequested(Tick tick, KernelFailurePoint point) const {
    if (testControls && testControls->failedTick == tick && testControls->failurePoint == point) {
        throw std::bad_alloc{};
    }
}

} // namespace cuexis::judgement::detail
