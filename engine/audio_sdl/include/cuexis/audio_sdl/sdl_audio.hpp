#pragma once

// SDL3 default-route audio subsystem and single-clip transport.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/audio_sdl/audio_sdl_export.hpp>
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>

#include <memory>

namespace cuexis::audio_sdl {

CUEXIS_ABI_WARNING_PUSH

class SdlAudioTransport;

class CUEXIS_AUDIO_SDL_API SdlAudioSubsystem final {
  public:
    [[nodiscard]] static auto create() -> core::Result<SdlAudioSubsystem>;
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

  private:
    struct Impl;
    explicit SdlAudioTransport(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::audio_sdl
