#pragma once

// Bounded in-memory RIFF/WAVE decoder for PCM and IEEE F32 source data.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <span>

namespace cuexis::audio_sdl {

inline constexpr std::size_t maxEncodedWavBytes = 64U * 1024U * 1024U;

class WavDecoder final {
  public:
    [[nodiscard]] static auto decode(std::span<const std::byte> encoded)
        -> core::Result<audio::AudioClip>;
};

} // namespace cuexis::audio_sdl
