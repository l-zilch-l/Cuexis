#include <cuexis/audio/audio_config.hpp>

#include <cuexis/core/error.hpp>

#include <cmath>

namespace cuexis::audio {

AudioDeviceRequest ValidatedAudioConfig::deviceRequest() const noexcept {
    return deviceRequest_;
}

std::uint32_t ValidatedAudioConfig::targetQueueMs() const noexcept {
    return targetQueueMs_;
}

std::uint32_t ValidatedAudioConfig::refillLowWaterMs() const noexcept {
    return refillLowWaterMs_;
}

float ValidatedAudioConfig::gain() const noexcept {
    return gain_;
}

auto validateAudioConfig(const AudioConfig& config) -> core::Result<ValidatedAudioConfig> {
    if (config.deviceRequest != AudioDeviceRequest::DefaultPlayback) {
        return core::unexpected(core::Error{"audio.config.device_request_invalid",
                                            "Only the default playback route is supported"});
    }
    if (config.targetQueueMs < 40 || config.targetQueueMs > 1000) {
        return core::unexpected(core::Error{"audio.config.target_queue_invalid",
                                            "targetQueueMs must be in [40, 1000]"});
    }
    if (config.refillLowWaterMs < 10 || config.refillLowWaterMs >= config.targetQueueMs) {
        return core::unexpected(
            core::Error{"audio.config.low_water_invalid",
                        "refillLowWaterMs must be at least 10 and less than targetQueueMs"});
    }
    if (!std::isfinite(config.gain) || config.gain < 0.0F || config.gain > 1.0F) {
        return core::unexpected(
            core::Error{"audio.config.gain_invalid", "gain must be finite and in [0, 1]"});
    }

    ValidatedAudioConfig result;
    result.deviceRequest_ = config.deviceRequest;
    result.targetQueueMs_ = config.targetQueueMs;
    result.refillLowWaterMs_ = config.refillLowWaterMs;
    result.gain_ = config.gain;
    return result;
}

} // namespace cuexis::audio
