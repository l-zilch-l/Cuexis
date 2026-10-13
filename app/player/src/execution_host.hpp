#pragma once

#include "../../../engine/playback/src/audio_worker.hpp"
#include "../../../engine/playback/src/capture_route.hpp"
#include "../../../engine/playback/src/renderer_owner.hpp"
#include "../../../engine/playback/src/session_worker.hpp"
#include "snapshot_scene.hpp"
#include <chrono>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <functional>

namespace cuexis::player {
// Application assembly only. The SDK still creates no implicit threads.
struct ExecutionHostOptions final {
    playback::PlaybackMode sourceMode{playback::PlaybackMode::ChartClock};
    playback::detail::SessionWorkerConfiguration session;
    playback::detail::AudioWorkerOptions audio;
    std::size_t hostCapacity{}, captureCapacity{}, captureReserve{};
    std::chrono::milliseconds replyDeadline{};
    std::optional<playback::detail::SessionRealtimeStart> realtime;
    playback::FrameViewport viewport{};
    bool showFps{};
};
struct ExecutionHostPorts final {
    playback::detail::AudioWorker::DeviceFactory audioDevice;
    playback::detail::AudioWorker::DecodeClip decode;
    // Called on main while awaiting owner replies. It must keep polling events and may capture,
    // but must not recursively issue control or advance commands.
    std::function<core::Result<void>()> pumpMain;
};
class ExecutionHost final {
  public:
    using ClockSource = std::function<core::Result<playback::ClockObservation>(
        const playback::detail::HostContext&)>;
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
                            playback::GameplayTick, const ClockSource&) -> core::Result<void>;
    [[nodiscard]] auto sample(std::int64_t presentationTick, playback::RuntimeFrame, bool frozen)
        -> core::Result<playback::detail::FrameEnvelope>;
    [[nodiscard]] auto present(const playback::detail::FrameEnvelope&)
        -> core::Result<playback::detail::PresentReceipt>;
    [[nodiscard]] auto queryGameplay() -> core::Result<playback::GameplayResult>;
    [[nodiscard]] auto archive() -> core::Result<playback::GameplayReplay>;
    [[nodiscard]] auto evaluateReplay(const playback::GameplayReplay&)
        -> core::Result<playback::GameplayReplayEvaluation>;
    [[nodiscard]] auto lag(const playback::ClockObservation&)
        -> core::Result<playback::GameplayExecutionLag>;
    [[nodiscard]] auto replayCut(const playback::GameplayReplay&, playback::GameplayTick)
        -> core::Result<playback::GameplayReplayCut>;
    [[nodiscard]] auto close() -> core::Result<void>;
    [[nodiscard]] auto resize(playback::FrameViewport) -> core::Result<void>;
    [[nodiscard]] auto audioPublication() const -> core::Result<playback::detail::AudioPublication>;
    void waitFor(std::chrono::milliseconds duration);
    [[nodiscard]] auto hasAudio() const noexcept -> bool {
        return bool(audio_);
    }
    [[nodiscard]] auto context() const noexcept -> const playback::detail::HostContext&;
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

  private:
    ExecutionHost(presentation_renderer::IPresentationRenderer&, ExecutionHostOptions,
                  ExecutionHostPorts, std::unique_ptr<playback::detail::SessionWorker>);
    [[nodiscard]] auto session(playback::detail::SessionCommand, bool control = true)
        -> playback::detail::SessionReply;
    [[nodiscard]] auto awaitSession() -> playback::detail::SessionReply;
    [[nodiscard]] auto audio(playback::detail::AudioWorkerCommand)
        -> playback::detail::AudioWorkerReply;
    void pump();
    void checkOwner() const;
    void begin(playback::detail::ControlKind, playback::detail::StepTarget, std::array<bool, 4>);
    void prepareControl(playback::detail::ControlKind, playback::detail::StepTarget,
                        std::array<bool, 4>, std::uint64_t softwareGeneration);
    void postControl();
    void confirmControl(std::uint64_t decision);
    void observed(const playback::detail::StepReceipt&);
    void audioStep(playback::detail::AudioWorkerCommandKind, playback::detail::StepPhase);
    void softwarePrepared();
    void finish();
    void flush(const playback::ClockObservation&, bool normalPrefix);
    void cleanup();
    void finishTail(const playback::TailClockTransition&, playback::TailProfile);
    [[nodiscard]] auto proof() -> playback::detail::ControlReceipt;
    [[nodiscard]] auto step(playback::detail::Participant, playback::detail::StepPhase) const
        -> playback::detail::StepRequest;
    void failed() noexcept;
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
    std::optional<playback::detail::PresentationCandidateInput> candidate_;
    playback::detail::HostContext context_;
    playback::detail::ControlRequest control_;
    std::optional<std::uint64_t> sessionJob_, audioJob_;
    std::optional<playback::detail::StepReceipt> earlyCut_;
    std::vector<playback::GameplayCaptureDelivery> retained_;
    std::optional<playback::detail::CaptureCloseEvidence> closeEvidence_;
    std::optional<core::Error> closeDeliveryError_;
    std::optional<playback::GameplayReplay> retainedArchive_;
    std::optional<playback::ClockObservation> lastObservation_;
    std::optional<playback::TailClockTransition> tail_;
    std::optional<playback::TailProfile> pendingTailProfile_;
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
} // namespace cuexis::player
