#pragma once

// Offline media import for Stage 6 E1/E2.
//
// This is an internal tool library: it is not installed, it is not part of the Cuexis SDK, and
// Playback/Player never link it. It converts one bounded encoded source into one canonical
// artifact: a Portable Presentation Texture2D v1 payload (CXPRES01) for images, or a canonical
// RIFF/WAVE PCM S16LE file for audio.
//
// The profile, budgets and rejection rules are fixed by ADR 0042 (S6-D06) and
// docs/formats/STAGE6_CONFIG_AND_MEDIA.md.

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::media_import {

// The v1 hard limits. They are the stricter of the media profile and the existing resource gates.
struct MediaBudget final {
    std::uint64_t maxEncodedBytes{64ULL * 1024ULL * 1024ULL};
    std::uint32_t maxImageDimension{8192};
    std::uint64_t maxImagePixels{8'388'608ULL};
    std::uint64_t maxImageBytes{32ULL * 1024ULL * 1024ULL};
    std::uint32_t minSampleRate{8'000};
    std::uint32_t maxSampleRate{192'000};
    std::uint32_t maxChannels{2};
    std::uint64_t maxAudioSeconds{1'800ULL};
    std::uint64_t maxWavBytes{64ULL * 1024ULL * 1024ULL};
    std::uint64_t maxWorkingBytes{128ULL * 1024ULL * 1024ULL};
};

// Measured work for one import. Working bytes exclude the source and the canonical output, which
// are counted separately by the caller.
struct MediaUsage final {
    std::uint64_t sourceBytes{};
    std::uint64_t decodedBytes{};
    std::uint64_t peakWorkingBytes{};
};

struct ImageImportInfo final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t sourceWidth{};
    std::uint32_t sourceHeight{};
    std::uint16_t orientation{1};
    std::string decoder;
};

struct AudioImportInfo final {
    std::uint32_t sampleRate{};
    std::uint32_t channels{};
    std::uint64_t frames{};
    std::string decoder;
};

struct ImageImportResult final {
    std::vector<std::byte> portableTexture;
    ImageImportInfo info;
    MediaUsage usage;
};

struct AudioImportResult final {
    std::vector<std::byte> canonicalWav;
    AudioImportInfo info;
    MediaUsage usage;
};

// The conversion profile identity covers the profile version and every decoder build that can
// change canonical bytes. It enters the E3 cache key; it is not a content identity.
[[nodiscard]] auto mediaProfileIdentity() -> std::string;

// SHA-256 of the canonical output, lowercase hexadecimal.
[[nodiscard]] auto contentIdentity(std::span<const std::byte> bytes) -> std::string;

// Bounded, format-explicit detection. Detection never falls back to a second decoder.
[[nodiscard]] auto isPngSource(std::span<const std::byte> source) noexcept -> bool;
[[nodiscard]] auto isJpegSource(std::span<const std::byte> source) noexcept -> bool;
[[nodiscard]] auto isMp3Source(std::span<const std::byte> source) noexcept -> bool;
[[nodiscard]] auto isOggVorbisSource(std::span<const std::byte> source) noexcept -> bool;
[[nodiscard]] auto isFlacSource(std::span<const std::byte> source) noexcept -> bool;

// PNG or JPEG to CXPRES01 RGBA8 sRGB. Every failure leaves no partially written artifact.
[[nodiscard]] auto importImage(std::span<const std::byte> source, const MediaBudget& budget = {})
    -> core::Result<ImageImportResult>;

// MP3, Ogg Vorbis or FLAC to canonical RIFF/WAVE PCM S16LE.
[[nodiscard]] auto importAudio(std::span<const std::byte> source, const MediaBudget& budget = {})
    -> core::Result<AudioImportResult>;

} // namespace cuexis::media_import
