#pragma once

#include "host_protocol.hpp"
#include <cuexis/audio/audio_transport.hpp>
#include <functional>

namespace cuexis::playback::detail {
// The transport and isolation callback must outlive the owner. All calls are owner-thread only.
class AudioOwner final {
  public:
    using IsolationProof = std::function<core::Result<bool>()>;
    [[nodiscard]] static auto create(std::uint64_t scope, audio::IAudioTransport&, IsolationProof,
                                     std::size_t capacity, std::size_t controlReserve)
        -> core::Result<AudioOwner>;
    AudioOwner(AudioOwner&&) noexcept = default;
    AudioOwner(const AudioOwner&) = delete;
    [[nodiscard]] auto prepare(const StepRequest&, audio::AudioClipLease)
        -> core::Result<StepReceipt>;
    // Confirms the existing stopped/paused binding for Seek without reloading its clip.
    [[nodiscard]] auto prepareActive(const StepRequest&) -> core::Result<StepReceipt>;
    // target.audioPositionMs is a physical source position, independent of judgement Tick.
    [[nodiscard]] auto activate(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto pause(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto stop(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto isolate(const StepRequest&) -> core::Result<StepReceipt>;
    // candidateId selects one candidate; zero explicitly selects all retained clips.
    [[nodiscard]] auto retire(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto close(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto cancel(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto steps() noexcept -> OwnerStepLedger& {
        return steps_;
    }

  private:
    AudioOwner(audio::IAudioTransport&, IsolationProof, OwnerStepLedger);
    [[nodiscard]] auto execute(const StepRequest&, StepPhase, audio::AudioClipLease = {},
                               bool activeReadiness = false) -> core::Result<StepReceipt>;
    core::ThreadChecker owner_;
    audio::IAudioTransport* transport_;
    IsolationProof isolationProof_;
    OwnerStepLedger steps_;
    audio::AudioClipLease lease_;
    audio::AudioClipLease prepared_;
    std::optional<StepRequest> preparedBinding_;
    std::uint64_t generation_{};
    std::optional<std::uint64_t> candidate_;
    std::optional<std::uint64_t> activeCandidate_;
    std::optional<std::uint64_t> operation_;
    std::optional<StepTarget> operationTarget_;
    bool loaded_{}, stopped_{true}, isolated_{true}, retired_{true}, closed_{}, uncertain_{};
};
} // namespace cuexis::playback::detail
