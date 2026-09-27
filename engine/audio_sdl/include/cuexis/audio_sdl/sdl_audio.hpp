#pragma once

// SDL3 audio subsystem and single-clip transport.
// create() keeps the default route. createForDevice() opens one enumerated device.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/audio_sdl/audio_sdl_export.hpp>
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cuexis::audio_sdl {

CUEXIS_ABI_WARNING_PUSH

class SdlAudioTransport;

struct PlaybackDeviceRecord final {
    std::uint32_t instanceId{0};
    std::string driver{};
    std::string deviceName{};
};

struct PlaybackDeviceTarget final {
    std::uint32_t instanceId{0};
    std::string driver{};
    std::string deviceName{};
};

class CUEXIS_AUDIO_SDL_API SdlAudioSubsystem final {
  public:
    [[nodiscard]] static auto create() -> core::Result<SdlAudioSubsystem>;
    // Current-process playback devices. instanceId is valid only until the next enumeration.
    [[nodiscard]] auto enumeratePlaybackDevices() const
        -> core::Result<std::vector<PlaybackDeviceRecord>>;
    ~SdlAudioSubsystem();

    SdlAudioSubsystem(const SdlAudioSubsystem&) = delete;
    auto operator=(const SdlAudioSubsystem&) -> SdlAudioSubsystem& = delete;
    SdlAudioSubsystem(SdlAudioSubsystem&& other) noexcept;
    auto operator=(SdlAudioSubsystem&& other) noexcept -> SdlAudioSubsystem&;

  private:
    friend class SdlAudioTransport;
    struct State;
    explicit SdlAudioSubsystem(std::shared_ptr<State> state) noexcept;

    std::shared_ptr<State> state_;
};

class CUEXIS_AUDIO_SDL_API SdlAudioTransport final : public audio::IAudioTransport {
  public:
    [[nodiscard]] static auto create(SdlAudioSubsystem& subsystem, audio::AudioClipStore& store,
                                     const audio::ValidatedAudioConfig& config)
        -> core::Result<SdlAudioTransport>;
    // Opens the enumerated instance after checking that driver and name still match once.
    // Does not change AudioConfig or the IAudioTransport vtable. The old create() stays default.
    [[nodiscard]] static auto
    createForDevice(SdlAudioSubsystem& subsystem, audio::AudioClipStore& store,
                    const audio::ValidatedAudioConfig& config, const PlaybackDeviceTarget& target)
        -> core::Result<SdlAudioTransport>;
    ~SdlAudioTransport() override;

    SdlAudioTransport(const SdlAudioTransport&) = delete;
    auto operator=(const SdlAudioTransport&) -> SdlAudioTransport& = delete;
    SdlAudioTransport(SdlAudioTransport&& other) noexcept;
    auto operator=(SdlAudioTransport&& other) noexcept -> SdlAudioTransport&;

    [[nodiscard]] auto load(audio::AudioClipHandle handle) -> core::Result<void> override;
    [[nodiscard]] auto play() -> core::Result<void> override;
    [[nodiscard]] auto pause() -> core::Result<void> override;
    [[nodiscard]] auto stop() -> core::Result<void> override;
    [[nodiscard]] auto seekMs(double positionMs) -> core::Result<void> override;
    [[nodiscard]] auto unload() -> core::Result<void> override;
    [[nodiscard]] auto service() -> core::Result<void> override;
    [[nodiscard]] audio::AudioClockSnapshot snapshot() const noexcept override;
    [[nodiscard]] audio::AudioMetricsSnapshot metrics() const noexcept override;
    [[nodiscard]] audio::EffectiveAudioSettings effectiveSettings() const noexcept override;

    [[nodiscard]] auto prepareReplacement(audio::AudioClipHandle handle, double positionMs)
        -> core::Result<void>;
    [[nodiscard]] auto activateReplacement() -> core::Result<void>;
    void cancelReplacement() noexcept;
    // Dynamic gain apply. This is not an IAudioTransport method.
    [[nodiscard]] auto applyGain(float gain) -> core::Result<void>;
    // Exact-device transports fail when the bound name is no longer a unique output.
    // The default route does not switch devices here.
    [[nodiscard]] auto recheckBoundDevice() -> core::Result<void>;

  private:
    struct Impl;
    explicit SdlAudioTransport(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::audio_sdl
