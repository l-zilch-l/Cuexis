#pragma once

#include "../../execution_host/audio_worker.hpp"
#include "../../execution_host/capture_route.hpp"
#include "../../execution_host/renderer_owner.hpp"
#include "../../execution_host/session_worker.hpp"
#include "snapshot_scene.hpp"
#include <chrono>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <cuexis_internal/execution_diagnostics.hpp>
#include <functional>
#include <utility>

namespace cuexis::player {
// Local sequencing only; accepted identities and effects remain owned by the existing ledgers.
enum class ExecutionClosePhase : std::uint8_t {
    idle,
    cleanup,
    revokeConsumer,
    confirmConsumer,
    retainedInput,
    archive,
    inspect,
    prepareTarget,
    prepareControl,
    postAcceptance,
    awaitAcceptance,
    acceptReplyReady,
    confirmAcceptance,
    cut,
    flush,
    audioStop,
    audioIsolate,
    softwareRetire,
    rendererRetire,
    audioRetire,
    exportEvidence,
    captureClose,
    softwareClose,
    rendererClose,
    audioClose,
    finish,
    completedProof,
    confirm,
    shutdown,
    exitWait,
    joined
};
// Application assembly only. The SDK still creates no implicit threads.
enum class SeekIdentityPolicy { canonicalContent, owningSource };
struct ExecutionHostOptions final {
    playback::PlaybackMode sourceMode{playback::PlaybackMode::ChartClock};
    playback::detail::SessionWorkerConfiguration session;
    playback::detail::AudioWorkerOptions audio;
    std::size_t hostCapacity{}, captureCapacity{}, captureReserve{};
    std::chrono::milliseconds replyDeadline{};
    std::optional<playback::detail::SessionRealtimeStart> realtime;
    playback::FrameViewport viewport{};
    bool showFps{};
    SeekIdentityPolicy seekIdentity{SeekIdentityPolicy::owningSource};
};
struct ExecutionHostPorts final {
    playback::detail::AudioWorker::DeviceFactory audioDevice;
    playback::detail::AudioWorker::DecodeClip decode;
    // Called on main while awaiting owner replies. It must keep polling events and may capture,
    // but must not recursively issue control or advance commands.
    std::function<core::Result<void>()> pumpMain;
    // Optional deterministic interruption gate for this explicitly test-only application assembly.
    std::function<core::Result<void>(ExecutionClosePhase)> beforeClosePhase;
    // Optional owner-thread Close clock; defaults to steady_clock. Must not throw or rewind.
    std::function<std::chrono::steady_clock::time_point()> closeNow;
};
class ExecutionHost final {
  public:
    using ClockSource =
        std::function<core::Result<playback::ClockObservation>(const playback::HostContext&)>;
    [[nodiscard]] static auto create(presentation_renderer::IPresentationRenderer&,
                                     ExecutionHostOptions, ExecutionHostPorts)
        -> core::Result<std::unique_ptr<ExecutionHost>>;
    ~ExecutionHost();
    ExecutionHost(const ExecutionHost&) = delete;
    auto operator=(const ExecutionHost&) -> ExecutionHost& = delete;
    [[nodiscard]] auto load(playback::PlaybackSource, std::optional<playback::GameplayContent> = {})
        -> core::Result<void>;
    [[nodiscard]] auto capture(playback::GameplayInput, bool clockValid = true)
        -> core::Result<void>;
    [[nodiscard]] auto advance(const playback::ClockObservation&)
        -> core::Result<playback::GameplayAdvanceDecision>;
    [[nodiscard]] auto beginTail(const playback::ClockObservation&, playback::TailProfile)
        -> core::Result<playback::TailClockTransition>;
    [[nodiscard]] auto inputTicks(const playback::ClockObservation&)
        -> core::Result<playback::ClockTicks>;
    [[nodiscard]] auto tail() const noexcept
        -> const std::optional<playback::TailClockTransition>& {
        return tail_;
    }
    [[nodiscard]] auto pause(const playback::ClockObservation&) -> core::Result<void>;
    [[nodiscard]] auto pause(const ClockSource&) -> core::Result<void>;
    [[nodiscard]] auto resume(const ClockSource&) -> core::Result<void>;
    [[nodiscard]] auto seek(const playback::GameplayReplay&, const playback::GameplayReplayCut&,
                            playback::GameplayTick, const ClockSource&,
                            std::span<const playback::GameplayCheckpoint> checkpoints = {})
        -> core::Result<void>;
    [[nodiscard]] auto sample(std::int64_t presentationTick, playback::RuntimeFrame, bool frozen)
        -> core::Result<playback::detail::FrameEnvelope>;
    [[nodiscard]] auto trySample(std::int64_t presentationTick, playback::RuntimeFrame, bool frozen)
        -> core::Result<std::optional<playback::detail::FrameEnvelope>>;
    [[nodiscard]] auto revokeRetained() -> core::Result<void>;
    [[nodiscard]] auto deliveryStatus() const noexcept -> playback::detail::DeliveryStatus {
        return deliveryStatus_;
    }
    [[nodiscard]] auto storageMetrics() const -> playback::detail::StorageMetrics {
        return session_->storageMetrics();
    }
    [[nodiscard]] auto present(const playback::detail::FrameEnvelope&)
        -> core::Result<playback::detail::PresentReceipt>;
    [[nodiscard]] auto observationInfo() -> core::Result<playback::detail::ObservationInfo>;
    [[nodiscard]] auto queryGameplay() -> core::Result<playback::GameplayResult>;
    [[nodiscard]] auto archive() -> core::Result<playback::GameplayReplay>;
    [[nodiscard]] auto evaluateReplay(const playback::GameplayReplay&)
        -> core::Result<playback::GameplayReplayEvaluation>;
    [[nodiscard]] auto lag(const playback::ClockObservation&)
        -> core::Result<playback::GameplayExecutionLag>;
    [[nodiscard]] auto snapshotGameplay() -> core::Result<playback::GameplaySnapshot>;
    [[nodiscard]] auto replayCut(const playback::GameplayReplay&, playback::GameplayTick)
        -> core::Result<playback::GameplayReplayCut>;
    [[nodiscard]] auto close() -> core::Result<void>;
    [[nodiscard]] auto closePhase() const noexcept -> ExecutionClosePhase {
        return closePhase_;
    }
    [[nodiscard]] auto closeBudgetExceeded() const noexcept -> bool {
        return closeBudgetExceeded_;
    }
    [[nodiscard]] auto closeOwnersExited() const noexcept -> bool {
        return session_->exited() && (!audio_ || audio_->exited());
    }
    [[nodiscard]] auto closeFirstError() const noexcept -> const std::optional<core::Error>& {
        return closeFirstError_;
    }
    [[nodiscard]] auto resize(playback::FrameViewport) -> core::Result<void>;
    [[nodiscard]] auto audioPublication() const -> core::Result<playback::detail::AudioPublication>;
    void waitFor(std::chrono::milliseconds duration);
    [[nodiscard]] auto hasAudio() const noexcept -> bool {
        return bool(audio_);
    }
    [[nodiscard]] auto context() const noexcept -> const playback::HostContext&;
    [[nodiscard]] auto state() const noexcept -> playback::detail::HostState;
    [[nodiscard]] auto pausedCut() const noexcept -> std::optional<std::int64_t> {
        return cut_;
    }
    [[nodiscard]] auto busy() const noexcept -> bool {
        return busy_;
    }
    [[nodiscard]] auto pumpCount() const noexcept -> std::uint64_t {
        return pumpCount_;
    }
    [[nodiscard]] auto retainedCapture() const noexcept
        -> const std::vector<playback::GameplayCaptureDelivery>& {
        return retained_;
    }
    [[nodiscard]] auto closeEvidence() const noexcept
        -> const std::optional<playback::detail::CaptureCloseEvidence>& {
        return closeEvidence_;
    }
    [[nodiscard]] auto closeDeliveryError() const noexcept -> const std::optional<core::Error>& {
        return closeDeliveryError_;
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    [[nodiscard]] auto rpaObservation() const noexcept
        -> const cuexis::internal::execution_diagnostics::rpa::Close& {
        return rpaClose_;
    }
    [[nodiscard]] auto rpaSeekObservation() const noexcept
        -> const cuexis::internal::execution_diagnostics::rpa::SeekLedger& {
        return rpaSeek_;
    }
#endif

  private:
    ExecutionHost(presentation_renderer::IPresentationRenderer&, ExecutionHostOptions,
                  ExecutionHostPorts, std::unique_ptr<playback::detail::SessionWorker>);
    // Materialize the command variant in a short-lived dispatch frame. Large proof-bearing
    // alternatives must not reserve a variant temporary at every call site in Debug builds.
    template <class Command>
    [[nodiscard]] auto session(Command&& command, bool control = true)
        -> playback::detail::SessionReply {
        return sessionCommand(playback::detail::SessionCommand{std::forward<Command>(command)},
                              control);
    }
    [[nodiscard]] auto sessionCommand(playback::detail::SessionCommand, bool control)
        -> playback::detail::SessionReply;
    [[nodiscard]] auto awaitSession() -> playback::detail::SessionReply;
    template <class Command>
    [[nodiscard]] auto closeSession(Command&& command) -> playback::detail::SessionReply {
        return sessionJob_ ? awaitSession() : session(std::forward<Command>(command));
    }
    [[nodiscard]] auto awaitAudio() -> playback::detail::AudioWorkerReply;
    [[nodiscard]] auto closeNow() const -> std::chrono::steady_clock::time_point;
    void closeBoundary(ExecutionClosePhase);
    [[nodiscard]] auto audio(playback::detail::AudioWorkerCommand)
        -> playback::detail::AudioWorkerReply;
    void pump(std::chrono::steady_clock::time_point deadline =
                  std::chrono::steady_clock::time_point::max());
    void checkOwner() const;
    void begin(playback::ControlKind, playback::StepTarget, std::array<bool, 4>);
    void prepareControl(playback::ControlKind, playback::StepTarget, std::array<bool, 4>,
                        std::uint64_t softwareGeneration);
    void postControl();
    void confirmControl(std::uint64_t decision);
    void observed(const playback::StepReceipt&);
    auto audioStep(playback::detail::AudioWorkerCommandKind, playback::StepPhase)
        -> playback::StepReceipt;
    void softwarePrepared();
    void finish();
    void registerRenderer();
    void retireRenderer();
    void invalidateRetained();
    void flush(const playback::ClockObservation&, bool normalPrefix);
    void cleanup();
    void finishTail(const playback::TailClockTransition&, playback::TailProfile);
    [[nodiscard]] auto proof() -> playback::ControlReceipt;
    [[nodiscard]] auto step(playback::Participant, playback::StepPhase) const
        -> playback::StepRequest;
    void failed() noexcept;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    void rpaMark(cuexis::internal::execution_diagnostics::rpa::CloseStage) noexcept;
    void rpaCause(cuexis::internal::execution_diagnostics::rpa::Reason,
                  const core::Error&) noexcept;
    void rpaCaptureFence(
        cuexis::internal::execution_diagnostics::rpa::CloseObservationPart) noexcept;
    void rpaSyncIdentity() noexcept;
    void rpaSeekFetch() noexcept;
    cuexis::internal::execution_diagnostics::rpa::Close rpaClose_{};
    std::optional<std::uint64_t> rpaObservationJob_, rpaSeekJob_;
    cuexis::internal::execution_diagnostics::rpa::CloseObservationPart rpaObservationPart_{};
    cuexis::internal::execution_diagnostics::rpa::SeekLedger rpaSeek_{};
    cuexis::internal::execution_diagnostics::rpa::CloseStage rpaStage_{
        cuexis::internal::execution_diagnostics::rpa::CloseStage::none};
#endif
    core::ThreadChecker owner_;
    presentation_renderer::IPresentationRenderer& renderer_;
    ExecutionHostOptions options_;
    ExecutionHostPorts ports_;
    PlayerFpsMeter fps_;
    std::unique_ptr<playback::detail::SessionWorker> session_;
    std::unique_ptr<playback::detail::AudioWorker> audio_;
    std::optional<playback::detail::RendererOwner<presentation_renderer::IPresentationRenderer>>
        rendererOwner_;
    std::optional<playback::detail::HostCoordinator> host_;
    std::optional<playback::detail::CaptureRouteOwner> capture_;
    std::optional<playback::RealtimeClock> clock_;
    std::optional<playback::PresentationCandidateInput> candidate_;
    playback::HostContext context_;
    playback::ControlRequest control_;
    std::optional<std::uint64_t> sessionJob_, audioJob_;
    std::chrono::steady_clock::time_point sessionDeadline_{}, audioDeadline_{}, closeDeadline_{};
    std::optional<std::chrono::steady_clock::time_point> closeOldSessionDeadline_,
        closeOldAudioDeadline_;
    ExecutionClosePhase closePhase_{ExecutionClosePhase::idle};
    std::optional<playback::detail::RegistrationReceipt> closeConsumerReceipt_;
    std::optional<playback::StepTarget> closeTarget_;
    std::optional<playback::ControlReceipt> closeCompleted_;
    std::optional<core::Error> closeFirstError_;
    std::uint64_t closeInspectionGeneration_{}, closeDecision_{};
    bool closeBudgetExceeded_{};
    std::optional<playback::StepReceipt> earlyCut_;
    std::vector<playback::GameplayCaptureDelivery> retained_;
    std::optional<playback::detail::CaptureCloseEvidence> closeEvidence_;
    std::optional<core::Error> closeDeliveryError_;
    std::optional<playback::GameplayReplay> retainedArchive_;
    std::optional<playback::ClockObservation> lastObservation_;
    std::optional<playback::TailClockTransition> tail_;
    std::optional<playback::TailProfile> pendingTailProfile_;
    playback::detail::RetainedPresentation retainedPresentation_;
    playback::detail::RendererConsumerGate rendererGate_;
    std::optional<playback::detail::ConsumerRegistration> consumer_;
    playback::detail::DeliveryStatus deliveryStatus_{playback::detail::DeliveryStatus::pending};
    std::uint64_t registrationOperation_{1};
    std::shared_ptr<const playback::detail::FrameEnvelope> pendingFrame_;
    std::vector<std::pair<playback::GameplayInput, bool>> clockTransitionInputs_;
    bool tailPreparing_{}, tailFrozen_{};
    std::optional<std::int64_t> lastObservedTick_;
    std::optional<std::uint64_t> waitingAdvance_;
    std::uint64_t command_{}, operation_{1}, advanceId_{1}, frameId_{1}, projectionScope_{1},
        reconciliationOrdinal_{}, pauseEpoch_{}, softwareGeneration_{}, audioGeneration_{},
        pumpCount_{};
    std::optional<audio::AudioClipHandle> clip_;
    std::optional<std::int64_t> cut_;
    bool busy_{}, closed_{}, suspended_{}, routePaused_{}, gpuTouched_{}, pendingControlAccept_{};
};

// Declare after all objects captured by the host callbacks. It drains on the original owner
// during ordinary and exceptional scope exit before releasing that callback closure.
class ExecutionHostLifetime final {
  public:
    explicit ExecutionHostLifetime(std::unique_ptr<ExecutionHost>& host) : host_(host) {}
    ~ExecutionHostLifetime();
    ExecutionHostLifetime(const ExecutionHostLifetime&) = delete;
    auto operator=(const ExecutionHostLifetime&) -> ExecutionHostLifetime& = delete;

  private:
    std::unique_ptr<ExecutionHost>& host_;
};
} // namespace cuexis::player
