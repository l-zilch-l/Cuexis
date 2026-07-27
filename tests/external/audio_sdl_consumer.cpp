#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio_sdl/wav_decoder.hpp>

#include <iostream>
#include <span>

int main() {
    const auto config = cuexis::audio::validateAudioConfig({});
    if (!config) {
        return 1;
    }
    const auto rejected = cuexis::audio_sdl::WavDecoder::decode(std::span<const std::byte>{});
    if (rejected) {
        return 1;
    }
    std::cout << "Cuexis AudioSDL external consumer passed\n";
    return 0;
}
