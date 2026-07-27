#include <cuexis/audio_sdl/wav_decoder.hpp>

#include <cuexis/core/error.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::audio_sdl {
namespace {

[[nodiscard]] auto byteAt(std::span<const std::byte> data, std::size_t offset) noexcept
    -> std::uint8_t {
    return std::to_integer<std::uint8_t>(data[offset]);
}

[[nodiscard]] auto readU16(std::span<const std::byte> data, std::size_t offset) noexcept
    -> std::uint16_t {
    return static_cast<std::uint16_t>(byteAt(data, offset)) |
           static_cast<std::uint16_t>(byteAt(data, offset + 1)) << 8U;
}

[[nodiscard]] auto readU32(std::span<const std::byte> data, std::size_t offset) noexcept
    -> std::uint32_t {
    return static_cast<std::uint32_t>(byteAt(data, offset)) |
           static_cast<std::uint32_t>(byteAt(data, offset + 1)) << 8U |
           static_cast<std::uint32_t>(byteAt(data, offset + 2)) << 16U |
           static_cast<std::uint32_t>(byteAt(data, offset + 3)) << 24U;
}

[[nodiscard]] auto hasTag(std::span<const std::byte> data, std::size_t offset,
                          std::string_view tag) noexcept -> bool {
    if (offset > data.size() || tag.size() > data.size() - offset) {
        return false;
    }
    for (std::size_t index = 0; index < tag.size(); ++index) {
        if (byteAt(data, offset + index) != static_cast<std::uint8_t>(tag[index])) {
            return false;
        }
    }
    return true;
}

struct WavFormat final {
    std::uint16_t tag{};
    std::uint16_t channels{};
    std::uint32_t sampleRate{};
    std::uint16_t blockAlign{};
    std::uint16_t bitsPerSample{};
};

[[nodiscard]] auto decodePcmSample(std::span<const std::byte> data, std::size_t offset,
                                   std::uint16_t bits) noexcept -> float {
    if (bits == 8) {
        return (static_cast<float>(byteAt(data, offset)) - 128.0F) / 128.0F;
    }
    if (bits == 16) {
        const auto value = static_cast<std::int16_t>(readU16(data, offset));
        return static_cast<float>(value) / 32768.0F;
    }
    if (bits == 24) {
        std::int32_t value = static_cast<std::int32_t>(byteAt(data, offset)) |
                             static_cast<std::int32_t>(byteAt(data, offset + 1)) << 8U |
                             static_cast<std::int32_t>(byteAt(data, offset + 2)) << 16U;
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xFF000000U);
        }
        return static_cast<float>(value) / 8388608.0F;
    }
    const auto value = static_cast<std::int32_t>(readU32(data, offset));
    return static_cast<float>(static_cast<double>(value) / 2147483648.0);
}

} // namespace

auto WavDecoder::decode(std::span<const std::byte> encoded) -> core::Result<audio::AudioClip> {
    if (encoded.size() > maxEncodedWavBytes) {
        return core::unexpected(
            core::Error{"audio.wav.encoded_limit", "WAV source exceeds the encoded byte limit"});
    }
    if (encoded.size() < 12 || !hasTag(encoded, 0, "RIFF") || !hasTag(encoded, 8, "WAVE")) {
        return core::unexpected(
            core::Error{"audio.wav.container_invalid", "Expected a RIFF/WAVE container"});
    }
    const auto riffSize = readU32(encoded, 4);
    if (riffSize < 4 || static_cast<std::uint64_t>(riffSize) + 8U > encoded.size()) {
        return core::unexpected(
            core::Error{"audio.wav.riff_size_invalid", "RIFF size exceeds the encoded source"});
    }
    const auto riffEnd = static_cast<std::size_t>(riffSize) + 8U;

    std::optional<WavFormat> format;
    std::span<const std::byte> pcmData;
    bool dataSeen = false;
    std::size_t cursor = 12;
    while (cursor < riffEnd) {
        if (riffEnd - cursor < 8) {
            return core::unexpected(
                core::Error{"audio.wav.chunk_truncated", "WAV chunk header is truncated"});
        }
        const auto chunkSize = static_cast<std::size_t>(readU32(encoded, cursor + 4));
        const auto payload = cursor + 8;
        if (chunkSize > riffEnd - payload) {
            return core::unexpected(
                core::Error{"audio.wav.chunk_size_invalid", "WAV chunk exceeds the RIFF bounds"});
        }
        if (hasTag(encoded, cursor, "fmt ")) {
            if (format || chunkSize < 16) {
                return core::unexpected(core::Error{"audio.wav.format_invalid",
                                                    "WAV must contain one complete format chunk"});
            }
            format = WavFormat{readU16(encoded, payload), readU16(encoded, payload + 2),
                               readU32(encoded, payload + 4), readU16(encoded, payload + 12),
                               readU16(encoded, payload + 14)};
            const auto byteRate = readU32(encoded, payload + 8);
            const auto bytesPerSample = static_cast<std::uint32_t>(format->bitsPerSample) / 8U;
            const auto expectedAlign =
                static_cast<std::uint32_t>(format->channels) * bytesPerSample;
            const auto expectedByteRate =
                static_cast<std::uint64_t>(format->sampleRate) * expectedAlign;
            const bool pcm =
                format->tag == 1 && (format->bitsPerSample == 8 || format->bitsPerSample == 16 ||
                                     format->bitsPerSample == 24 || format->bitsPerSample == 32);
            const bool ieeeFloat = format->tag == 3 && format->bitsPerSample == 32;
            if ((!pcm && !ieeeFloat) || format->channels < 1 || format->channels > 2 ||
                format->sampleRate < 8000 || format->sampleRate > 192000 ||
                format->bitsPerSample % 8 != 0 || format->blockAlign != expectedAlign ||
                expectedByteRate > std::numeric_limits<std::uint32_t>::max() ||
                byteRate != expectedByteRate) {
                return core::unexpected(core::Error{
                    "audio.wav.format_unsupported",
                    "WAV must be mono/stereo PCM or IEEE F32 with a supported sample rate"});
            }
        } else if (hasTag(encoded, cursor, "data")) {
            if (dataSeen) {
                return core::unexpected(
                    core::Error{"audio.wav.data_duplicate", "WAV contains multiple data chunks"});
            }
            dataSeen = true;
            pcmData = encoded.subspan(payload, chunkSize);
        }
        const auto paddedSize = chunkSize + (chunkSize & 1U);
        if (paddedSize > riffEnd - payload) {
            return core::unexpected(
                core::Error{"audio.wav.chunk_padding_invalid", "WAV chunk padding is truncated"});
        }
        cursor = payload + paddedSize;
    }
    if (!format || pcmData.empty()) {
        return core::unexpected(core::Error{"audio.wav.required_chunk_missing",
                                            "WAV requires format and non-empty data chunks"});
    }
    if (pcmData.size() % format->blockAlign != 0) {
        return core::unexpected(core::Error{"audio.wav.frame_alignment_invalid",
                                            "WAV data must contain complete source frames"});
    }
    const auto frameCount = pcmData.size() / format->blockAlign;
    if (frameCount >
        audio::maxDecodedClipBytes / (static_cast<std::size_t>(format->channels) * sizeof(float))) {
        return core::unexpected(
            core::Error{"audio.wav.decoded_limit", "Decoded WAV exceeds the PCM byte limit"});
    }
    const auto sampleCount = frameCount * format->channels;
    std::vector<float> samples;
    samples.reserve(sampleCount);
    const auto bytesPerSample = static_cast<std::size_t>(format->bitsPerSample / 8U);
    for (std::size_t index = 0; index < sampleCount; ++index) {
        const auto offset = index * bytesPerSample;
        float sample{};
        if (format->tag == 3) {
            sample = std::bit_cast<float>(readU32(pcmData, offset));
        } else {
            sample = decodePcmSample(pcmData, offset, format->bitsPerSample);
        }
        if (!std::isfinite(sample)) {
            return core::unexpected(core::Error{"audio.wav.sample_non_finite",
                                                "Decoded WAV contains a non-finite sample"});
        }
        samples.push_back(sample);
    }
    return audio::AudioClip::create(format->sampleRate, format->channels, std::move(samples));
}

} // namespace cuexis::audio_sdl
