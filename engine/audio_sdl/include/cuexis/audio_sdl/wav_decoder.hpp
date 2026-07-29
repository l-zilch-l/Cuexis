#pragma once

// Bounded in-memory RIFF/WAVE decoder for PCM and IEEE F32 source data.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio_sdl/audio_sdl_export.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <span>

namespace cuexis::audio_sdl {

inline constexpr std::size_t maxEncodedWavBytes = 64U * 1024U * 1024U;

class CUEXIS_AUDIO_SDL_API WavDecoder final {
  public:
    [[nodiscard]] static auto decode(std::span<const std::byte> encoded)
        -> core::Result<audio::AudioClip>;
};

} // namespace cuexis::audio_sdl
