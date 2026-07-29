#pragma once

// Audio configuration is validated before any backend initialization or device operation.

#include <cuexis/audio/audio_export.hpp>
#include <cuexis/core/result.hpp>

#include <cstdint>

namespace cuexis::audio {

enum class AudioDeviceRequest {
    DefaultPlayback,
};

struct AudioConfig final {
    AudioDeviceRequest deviceRequest{AudioDeviceRequest::DefaultPlayback};
    std::uint32_t targetQueueMs{200};
    std::uint32_t refillLowWaterMs{100};
    float gain{1.0F};
};

class CUEXIS_AUDIO_API ValidatedAudioConfig final {
  public:
    [[nodiscard]] AudioDeviceRequest deviceRequest() const noexcept;
    [[nodiscard]] std::uint32_t targetQueueMs() const noexcept;
    [[nodiscard]] std::uint32_t refillLowWaterMs() const noexcept;
    [[nodiscard]] float gain() const noexcept;

  private:
    friend CUEXIS_AUDIO_API auto validateAudioConfig(const AudioConfig&)
        -> core::Result<ValidatedAudioConfig>;

    AudioDeviceRequest deviceRequest_{AudioDeviceRequest::DefaultPlayback};
    std::uint32_t targetQueueMs_{200};
    std::uint32_t refillLowWaterMs_{100};
    float gain_{1.0F};
};

[[nodiscard]] CUEXIS_AUDIO_API auto validateAudioConfig(const AudioConfig& config)
    -> core::Result<ValidatedAudioConfig>;

} // namespace cuexis::audio
