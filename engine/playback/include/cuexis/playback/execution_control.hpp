#pragma once

// Candidate-only owning control proofs. Host ledgers and threads are not SDK types.
#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/playback/execution_coordination.hpp>
#include <cuexis/playback/presentation_candidate.hpp>
namespace cuexis::playback {
CUEXIS_ABI_WARNING_PUSH
enum class Participant : std::uint8_t { capture, software, audio, renderer };
enum class StepPhase : std::uint8_t {
    prepare,
    fence,
    cut,
    resumeRoute,
    softwareCommit,
    activate,
    stop,
    isolate,
    retire,
    close
};
struct StepKey final {
    std::uint64_t sessionScope{}, operationId{};
    Participant participant{};
    StepPhase phase{};
    std::uint64_t attempt{};
    friend auto operator==(StepKey, StepKey) -> bool = default;
};
struct StepTarget final {
    HostContext context;
    std::uint64_t expectedGeneration{}, candidateId{}, commandOrdinal{};
    std::optional<std::int64_t> tick;
    std::optional<std::uint64_t> cut, pauseEpoch;
    std::optional<PreparedSemanticIdentity> archiveIdentity;
    std::optional<HostContext> nextContext;
    std::optional<PreparedSemanticIdentity> payloadIdentity;
    std::optional<double> audioPositionMs;
    std::optional<audio::AudioClipHandle> audioClip;
    std::array<std::optional<std::uint64_t>, 4> participantGenerations;
    std::optional<std::uint64_t> reconciliationOrdinal;
    std::optional<std::int64_t> seekHorizon;
    std::optional<std::uint64_t> seekCompleteRecords;
    std::optional<std::int64_t> seekPartialHorizon;
    std::optional<std::int64_t> tailPauseAt;
    friend auto operator==(const StepTarget&, const StepTarget&) -> bool = default;
};
[[nodiscard]] inline auto generationFor(const StepTarget& target, Participant participant)
    -> std::uint64_t {
    const auto index = static_cast<std::size_t>(participant);
    return index < target.participantGenerations.size()
               ? target.participantGenerations[index].value_or(target.expectedGeneration)
               : target.expectedGeneration;
}
struct ResumeTraceBinding final {
    std::uint64_t finalCut{};
    PreparedSemanticIdentity identity{};
    friend auto operator==(const ResumeTraceBinding&, const ResumeTraceBinding&) -> bool = default;
};
struct StepRequest final {
    StepKey key;
    StepTarget target;
    // Physical generation may change only in an isolated retry of this immutable target.
    std::optional<std::uint64_t> retryGeneration;
    std::optional<ResumeTraceBinding> resumeTrace;
    std::optional<std::int64_t> tailResumeAt;
    friend auto operator==(const StepRequest&, const StepRequest&) -> bool = default;
};
enum class StepStatus : std::uint8_t {
    pending,
    inProgress,
    rejectedNotApplied,
    canceledNotApplied,
    applied,
    failed,
    unknown,
    retired
};
struct StepEffects final {
    std::uint64_t generation{};
    std::optional<std::int64_t> position;
    std::optional<std::uint64_t> cut, pauseEpoch;
    bool callbackIsolated{}, referencesReleased{};
    std::optional<std::uint64_t> sealedFactCursor, foldedFactCursor;
    std::optional<PreparedSemanticIdentity> payloadIdentity;
    friend auto operator==(const StepEffects&, const StepEffects&) -> bool = default;
};
struct StepReceipt final {
    StepRequest request;
    StepStatus status{StepStatus::pending};
    StepEffects effects;
    std::optional<core::Error> cause;
};
enum class ControlKind : std::uint8_t { load, reload, pause, resume, seek, stop, reset, close };
struct ControlRequest final {
    HostContext context;
    std::uint64_t operationId{}, commandOrdinal{};
    ControlKind kind{};
    StepTarget target;
    std::array<bool, 4> participants{true, true, true, true};
    friend auto operator==(const ControlRequest&, const ControlRequest&) -> bool = default;
};
enum class ControlStatus : std::uint8_t {
    preparing,
    softwareApplied,
    complete,
    suspended,
    failed,
    busyNotAccepted,
    retired
};
struct ControlReceipt final {
    ControlRequest request;
    ControlStatus status{};
    std::uint64_t decisionOrdinal{};
    std::array<std::array<std::optional<StepReceipt>, 10>, 4> steps;
    bool directResumeAllowed{true}, cleanupComplete{}, softwareApplied{};
    std::optional<HostContext> finalizedContext;
};
struct FeedbackId final {
    std::uint64_t projectionScope{}, commitId{}, localOrdinal{}, activationOrdinal{};
    std::string targetId, bindingId;
    friend auto operator==(const FeedbackId&, const FeedbackId&) -> bool = default;
};
enum class FeedbackStatus : std::uint8_t {
    created,
    confirmedPresent,
    expiredUnconfirmed,
    canceledByControl,
    unknownEvidence
};
struct FeedbackObservation final {
    FeedbackId id;
    std::int64_t created{}, expiry{};
    FeedbackStatus status{FeedbackStatus::created};
    bool laterConfirmed{};
};
// Copyable proof token. Only PlaybackSession can create a valid token.
class ExecutionFence final {
  public:
    ExecutionFence() noexcept = default;
    [[nodiscard]] auto generation() const noexcept -> std::uint64_t {
        return generation_;
    }
    [[nodiscard]] auto scope() const noexcept -> std::uint64_t {
        return scope_;
    }
    [[nodiscard]] auto candidateId() const noexcept -> std::uint64_t {
        return candidateId_;
    }
    [[nodiscard]] auto controlSequence() const noexcept -> std::uint64_t {
        return controlSequence_;
    }
    friend auto operator==(const ExecutionFence&, const ExecutionFence&) -> bool = default;

  private:
    friend class PlaybackSession;
    ExecutionFence(std::uint64_t scope, std::uint64_t generation, std::uint64_t candidate,
                   std::uint64_t sequence) noexcept
        : scope_(scope), generation_(generation), candidateId_(candidate),
          controlSequence_(sequence) {}
    std::uint64_t scope_{}, generation_{}, candidateId_{}, controlSequence_{};
};
struct ExecutionOwnerState final {
    std::uint64_t sessionScope{}, generation{};
    std::optional<RuntimeFrame> runtimeFrame;
    std::optional<ControlRequest> acceptedControl;
    std::optional<ExecutionFence> fence;
    std::optional<GameplayEpochEnd> pendingEpochEnd;
    std::optional<std::uint64_t> reconciliationOrdinal;
};
inline auto controlRequiredPhase(ControlKind, Participant) -> StepPhase;
inline auto confirmedExecutionStep(const ControlReceipt& control, Participant participant,
                                   StepPhase phase) -> const StepReceipt* {
    const auto participantIndex = static_cast<std::size_t>(participant);
    const auto phaseIndex = static_cast<std::size_t>(phase);
    if (participantIndex >= control.steps.size() || phaseIndex >= control.steps[0].size())
        return nullptr;
    const auto& step = control.steps[participantIndex][phaseIndex];
    if (!step || step->status != StepStatus::applied ||
        step->request.key.sessionScope != control.request.context.sessionScope ||
        step->request.key.operationId != control.request.operationId ||
        step->request.key.participant != participant || step->request.key.phase != phase ||
        step->request.target != control.request.target)
        return nullptr;
    return &*step;
}
inline auto executionOwnersApplied(const ControlReceipt& control) -> bool {
    for (std::size_t p = 0; p < control.request.participants.size(); ++p) {
        if (!control.request.participants[p])
            continue;
        const auto participant = static_cast<Participant>(p);
        if (!confirmedExecutionStep(control, participant,
                                    controlRequiredPhase(control.request.kind, participant)))
            return false;
        if (participant == Participant::audio && (control.request.kind == ControlKind::stop ||
                                                  control.request.kind == ControlKind::reset)) {
            const auto* isolated = confirmedExecutionStep(control, participant, StepPhase::isolate);
            if (!isolated || !isolated->effects.callbackIsolated)
                return false;
        }
    }
    return true;
}
inline auto controlRequiredPhase(ControlKind kind, Participant participant) -> StepPhase {
    if (kind == ControlKind::close)
        return StepPhase::close;
    if (participant == Participant::software)
        return StepPhase::softwareCommit;
    const bool stopped = kind == ControlKind::stop || kind == ControlKind::reset;
    if (participant == Participant::capture)
        return kind == ControlKind::pause || stopped ? StepPhase::cut : StepPhase::resumeRoute;
    if (participant == Participant::audio)
        return stopped                      ? StepPhase::stop
               : kind == ControlKind::pause ? StepPhase::fence
                                            : StepPhase::activate;
    return kind == ControlKind::pause || stopped ? StepPhase::fence : StepPhase::activate;
}
inline auto controlReadyForCommit(const ControlReceipt& control) -> bool {
    if (control.status != ControlStatus::preparing)
        return false;
    for (std::size_t p = 0; p < control.request.participants.size(); ++p)
        if (control.request.participants[p] &&
            (!confirmedExecutionStep(control, static_cast<Participant>(p), StepPhase::prepare) ||
             !confirmedExecutionStep(control, static_cast<Participant>(p), StepPhase::fence)))
            return false;
    return true;
}
inline auto controlFullyApplied(const ControlReceipt& control) -> bool {
    if (control.status != ControlStatus::complete || !control.cleanupComplete)
        return false;
    if (control.request.kind == ControlKind::close) {
        for (std::size_t p = 0; p < control.request.participants.size(); ++p)
            if (control.request.participants[p] &&
                !confirmedExecutionStep(control, static_cast<Participant>(p), StepPhase::close))
                return false;
        const auto active = [&](Participant p) {
            return control.request.participants[static_cast<std::size_t>(p)];
        };
        if (active(Participant::capture) &&
            !confirmedExecutionStep(control, Participant::capture, StepPhase::cut))
            return false;
        if (active(Participant::audio)) {
            const auto* isolated =
                confirmedExecutionStep(control, Participant::audio, StepPhase::isolate);
            if (!confirmedExecutionStep(control, Participant::audio, StepPhase::stop) ||
                !isolated || !isolated->effects.callbackIsolated)
                return false;
        }
        for (auto p : {Participant::software, Participant::renderer}) {
            const auto* retired = confirmedExecutionStep(control, p, StepPhase::retire);
            if (active(p) && (!retired || !retired->effects.referencesReleased))
                return false;
        }
        return true;
    }
    if (!control.softwareApplied ||
        (control.request.participants[static_cast<std::size_t>(Participant::software)] &&
         !confirmedExecutionStep(control, Participant::software, StepPhase::softwareCommit)))
        return false;
    return executionOwnersApplied(control);
}
[[nodiscard]] CUEXIS_PLAYBACK_API auto
computeGameplayReconciliationIdentity(const GameplayReconciliation&)
    -> core::Result<PreparedSemanticIdentity>;
CUEXIS_ABI_WARNING_POP
} // namespace cuexis::playback
