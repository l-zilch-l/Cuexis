#pragma once
#include "audio_owner.hpp"
#include "owner_channel.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <span>
#include <thread>

namespace cuexis::playback::detail {
struct OwnedAudioClockObservation final {
    audio::AudioClockSnapshot clock;
    std::optional<std::int64_t> sampleMonotonicUs, maximumPresentedFrame;
    std::int64_t errorBoundUs{};
};
// Created and destroyed on the audio service owner, never the real-time callback.
class OwnedAudioDevice {
  public:
    virtual ~OwnedAudioDevice() = default;
    [[nodiscard]] virtual auto transport() noexcept -> audio::IAudioTransport& = 0;
    [[nodiscard]] virtual auto isolateCallbacks() -> core::Result<bool> = 0;
    [[nodiscard]] virtual auto recheckDevice() -> core::Result<void> = 0;
    [[nodiscard]] virtual auto clockObservation() const noexcept
        -> std::optional<OwnedAudioClockObservation> {
        return {};
    }
};
enum class AudioWorkerCommandKind : std::uint8_t {
    decode,
    prepare,
    fence,
    activate,
    stop,
    isolate,
    retire,
    close,
    cancel,
    query,
    prepareActive
};
struct AudioWorkerCommand final {
    AudioWorkerCommandKind kind{};
    StepRequest step;
    std::vector<std::byte> encoded;
    std::uint64_t candidateId{};
    StepKey queryKey;
};
struct AudioWorkerReply final {
    std::optional<audio::AudioClipHandle> clip;
    std::optional<StepReceipt> step;
};
struct AudioPublication final {
    enum class Fault : std::uint8_t { none, device, service, allocation };
    audio::AudioClockSnapshot clock;
    audio::AudioMetricsSnapshot metrics;
    audio::EffectiveAudioSettings settings;
    std::uint64_t sequence{}, timeDomain{};
    std::int64_t sampleMonotonicUs{};
    std::optional<std::int64_t> maximumPresentedFrame;
    std::int64_t errorBoundUs{};
    bool sampleReady{true};
    std::optional<core::Error> serviceError;
    Fault fault{};
};
struct AudioWorkerOptions final {
    std::uint64_t sessionScope{}, timeDomain{};
    bool testOnly{};
    std::size_t capacity{}, controlReserve{};
    audio::AudioClipStoreLimits clipLimits;
    std::chrono::milliseconds serviceInterval{2};
};
class AudioWorker final {
  public:
    using DeviceFactory =
        std::function<core::Result<std::unique_ptr<OwnedAudioDevice>>(audio::AudioClipStore&)>;
    using DecodeClip = std::function<core::Result<audio::AudioClip>(std::span<const std::byte>)>;
    [[nodiscard]] static auto start(AudioWorkerOptions, DeviceFactory, DecodeClip)
        -> core::Result<std::unique_ptr<AudioWorker>>;
    ~AudioWorker();
    AudioWorker(const AudioWorker&) = delete;
    auto operator=(const AudioWorker&) -> AudioWorker& = delete;
    [[nodiscard]] auto post(AudioWorkerCommand, bool control) -> core::Result<std::uint64_t>;
    [[nodiscard]] auto poll(std::uint64_t)
        -> core::Result<std::optional<core::Result<AudioWorkerReply>>>;
    [[nodiscard]] auto acknowledge(std::uint64_t) -> core::Result<void>;
    void waitFor(std::chrono::milliseconds duration);
    [[nodiscard]] auto publication() const -> core::Result<AudioPublication>;
    void requestShutdown();
    [[nodiscard]] auto exited() const noexcept -> bool;
    [[nodiscard]] auto joinExited() -> core::Result<void>;

  private:
    struct State;
    explicit AudioWorker(std::unique_ptr<State>);
    static void run(State&) noexcept;
    std::unique_ptr<State> state_;
};
} // namespace cuexis::playback::detail
