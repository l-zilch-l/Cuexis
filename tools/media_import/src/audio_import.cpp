#include "media_internal.hpp"
#include "minimp3_config.hpp"

#include <cuexis/media_import/media_import.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <FLAC/stream_decoder.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

namespace cuexis::media_import {
namespace {

constexpr std::string_view mp3DecoderName = "minimp3-2021-11-30-no-simd";
constexpr std::string_view vorbisDecoderName = "libvorbis-1.3.7-pinned-mpi-libogg-1.3.6";
constexpr std::string_view flacDecoderName = "libflac-1.5.0-native";
constexpr std::size_t oggInputChunkBytes = 64U * 1024U;

// ---------------------------------------------------------------------------------------------
// Shared audio plumbing
// ---------------------------------------------------------------------------------------------

[[nodiscard]] auto validateAudioFormat(std::uint32_t sampleRate, std::uint32_t channels,
                                       const MediaBudget& budget) -> core::Result<void> {
    if (sampleRate < budget.minSampleRate || sampleRate > budget.maxSampleRate) {
        return core::unexpected(detail::budgetError(
            "media.audio.rate_unsupported", "Audio sample rate is outside the media profile",
            budget.maxSampleRate, sampleRate));
    }
    if (channels < 1 || channels > budget.maxChannels) {
        return core::unexpected(detail::budgetError(
            "media.audio.channels_unsupported", "Audio channel count is outside the media profile",
            budget.maxChannels, channels));
    }
    return {};
}

// Accumulates canonical S16 samples while enforcing the duration and final-WAV limits. The PCM
// output is the final artifact, so it is not charged to the conversion working budget.
class AudioAccumulator final {
  public:
    AudioAccumulator(std::uint32_t sampleRate, std::uint32_t channels, const MediaBudget& budget)
        : sampleRate_(sampleRate), channels_(channels) {
        const std::uint64_t bytesPerFrame = 2ULL * channels;
        const std::uint64_t wavFrames =
            budget.maxWavBytes > 44ULL ? (budget.maxWavBytes - 44ULL) / bytesPerFrame : 0;
        const std::uint64_t durationFrames =
            static_cast<std::uint64_t>(sampleRate) * budget.maxAudioSeconds;
        wavFrames_ = wavFrames;
        durationFrames_ = durationFrames;
        maxFrames_ = std::min(wavFrames, durationFrames);
    }

    [[nodiscard]] auto append(std::span<const std::int16_t> interleaved) -> core::Result<void> {
        if (interleaved.empty()) {
            return {};
        }
        const std::uint64_t frames = interleaved.size() / channels_;
        if (frames > maxFrames_ - frames_) {
            if (wavFrames_ <= durationFrames_) {
                return core::unexpected(detail::budgetError(
                    "media.audio.wav_limit", "Canonical WAV would exceed the media profile budget",
                    wavFrames_, frames_ + frames));
            }
            return core::unexpected(detail::budgetError(
                "media.audio.duration_limit", "Audio duration exceeds the media profile budget",
                durationFrames_, frames_ + frames));
        }
        samples_.insert(samples_.end(), interleaved.begin(), interleaved.end());
        frames_ += frames;
        return {};
    }

    void truncateToFrames(std::uint64_t frames) {
        if (frames >= frames_) {
            return;
        }
        samples_.resize(static_cast<std::size_t>(frames) * channels_);
        frames_ = frames;
    }

    [[nodiscard]] auto samples() const noexcept -> const std::vector<std::int16_t>& {
        return samples_;
    }
    [[nodiscard]] auto frames() const noexcept -> std::uint64_t {
        return frames_;
    }
    [[nodiscard]] auto sampleRate() const noexcept -> std::uint32_t {
        return sampleRate_;
    }
    [[nodiscard]] auto channels() const noexcept -> std::uint32_t {
        return channels_;
    }

  private:
    std::uint32_t sampleRate_{};
    std::uint32_t channels_{};
    std::uint64_t wavFrames_{};
    std::uint64_t durationFrames_{};
    std::uint64_t maxFrames_{};
    std::uint64_t frames_{};
    std::vector<std::int16_t> samples_;
};

[[nodiscard]] auto finalizeAudio(AudioAccumulator&& accumulator, std::string decoder,
                                 const MediaBudget& budget) -> core::Result<AudioImportResult> {
    std::uint64_t dataBytes = 0;
    std::uint64_t totalBytes = 0;
    if (!detail::checkedMul(accumulator.samples().size(), 2ULL, dataBytes) ||
        !detail::checkedAdd(dataBytes, 44ULL, totalBytes) || totalBytes > budget.maxWavBytes) {
        return core::unexpected(detail::budgetError(
            "media.audio.wav_limit", "Canonical WAV exceeds the media profile budget",
            budget.maxWavBytes, totalBytes));
    }
    AudioImportResult result;
    result.canonicalWav = detail::writeCanonicalWav(accumulator.sampleRate(),
                                                    accumulator.channels(), accumulator.samples());
    result.info.sampleRate = accumulator.sampleRate();
    result.info.channels = accumulator.channels();
    result.info.frames = accumulator.frames();
    result.info.decoder = std::move(decoder);
    result.usage.decodedBytes = result.canonicalWav.size();
    return result;
}

// ---------------------------------------------------------------------------------------------
// MP3 (minimp3, Layer III only, scalar build, library-provided delay/padding trim)
// ---------------------------------------------------------------------------------------------

[[nodiscard]] auto decodeMp3(std::span<const std::byte> source, const MediaBudget& budget)
    -> core::Result<AudioImportResult> {
    mp3dec_ex_t decoder;
    std::memset(&decoder, 0, sizeof(decoder));
    const int status =
        mp3dec_ex_open_buf(&decoder, reinterpret_cast<const std::uint8_t*>(source.data()),
                           source.size(), MP3D_SEEK_TO_SAMPLE);
    if (status != 0) {
        return core::unexpected(detail::mediaError("media.audio.container_invalid",
                                                   "minimp3 could not open the MP3 stream"));
    }
    const auto fail = [&decoder](std::string code, std::string message) {
        mp3dec_ex_close(&decoder);
        return core::unexpected(detail::mediaError(std::move(code), std::move(message)));
    };
    if (decoder.info.layer != 3) {
        return fail("media.audio.format_unsupported", "Only MPEG Layer III audio is supported");
    }
    const auto sampleRate = static_cast<std::uint32_t>(decoder.info.hz);
    const auto channels = static_cast<std::uint32_t>(decoder.info.channels);
    if (auto valid = validateAudioFormat(sampleRate, channels, budget); !valid) {
        mp3dec_ex_close(&decoder);
        return core::unexpected(valid.error());
    }

    AudioAccumulator accumulator{sampleRate, channels, budget};
    std::vector<std::int16_t> chunk(MINIMP3_MAX_SAMPLES_PER_FRAME);
    while (true) {
        const std::size_t read = mp3dec_ex_read(&decoder, chunk.data(), chunk.size());
        if (read == 0) {
            break;
        }
        if (auto appended = accumulator.append(std::span<const std::int16_t>{chunk.data(), read});
            !appended) {
            mp3dec_ex_close(&decoder);
            return core::unexpected(appended.error());
        }
    }
    if (decoder.last_error != 0) {
        return fail("media.audio.decode_failed", "minimp3 reported a decoder error");
    }
    // A Xing or Info header declares the stream length. Fewer decoded samples than that means the
    // file was cut and the gapless trim cannot be trusted.
    std::uint64_t decodedSamples = 0;
    if (!detail::checkedMul(accumulator.frames(), static_cast<std::uint64_t>(channels),
                            decodedSamples)) {
        return fail("media.audio.duration_limit", "MP3 sample count overflowed");
    }
    if (decoder.detected_samples != 0 && decodedSamples < decoder.detected_samples) {
        return fail("media.audio.truncated",
                    "MP3 stream ended before the sample count its Xing header declares");
    }
    if (decoder.offset < decoder.file.size) {
        return fail("media.audio.truncated", "MP3 stream ended inside a frame");
    }
    if (accumulator.frames() == 0) {
        return fail("media.audio.container_invalid", "MP3 stream produced no audio frames");
    }
    mp3dec_ex_close(&decoder);
    return finalizeAudio(std::move(accumulator), std::string{mp3DecoderName}, budget);
}

// ---------------------------------------------------------------------------------------------
// Ogg Vorbis (libvorbis + libogg, single logical stream, granule-end truncation)
// ---------------------------------------------------------------------------------------------

struct VorbisState final {
    ogg_sync_state sync{};
    ogg_stream_state stream{};
    vorbis_info info{};
    vorbis_comment comment{};
    vorbis_dsp_state dsp{};
    vorbis_block block{};
    bool streamInitialized{false};
    bool dspInitialized{false};
    bool blockInitialized{false};
    bool sawEndOfStream{false};
    long serialNumber{-1};
    std::uint64_t granuleEnd{0};
    bool granuleValid{false};
    int headerPackets{0};

    ~VorbisState() {
        if (blockInitialized) {
            vorbis_block_clear(&block);
        }
        if (dspInitialized) {
            vorbis_dsp_clear(&dsp);
        }
        if (streamInitialized) {
            ogg_stream_clear(&stream);
        }
        vorbis_comment_clear(&comment);
        vorbis_info_clear(&info);
        ogg_sync_clear(&sync);
    }
};

[[nodiscard]] auto decodeVorbis(std::span<const std::byte> source, const MediaBudget& budget)
    -> core::Result<AudioImportResult> {
    VorbisState state;
    ogg_sync_init(&state.sync);
    vorbis_info_init(&state.info);
    vorbis_comment_init(&state.comment);
    std::optional<AudioAccumulator> accumulator;
    std::vector<std::int16_t> interleaved;

    const auto fail = [](std::string code, std::string message) {
        return core::unexpected(detail::mediaError(std::move(code), std::move(message)));
    };

    std::size_t offset = 0;
    while (true) {
        ogg_page page;
        const int pageStatus = ogg_sync_pageout(&state.sync, &page);
        if (pageStatus == 0) {
            if (offset >= source.size()) {
                break; // no complete page left in the source
            }
            const std::size_t take = std::min(oggInputChunkBytes, source.size() - offset);
            char* buffer = ogg_sync_buffer(&state.sync, static_cast<long>(take));
            if (buffer == nullptr) {
                return fail("media.audio.decode_failed", "libogg could not allocate a sync buffer");
            }
            std::memcpy(buffer, source.data() + offset, take);
            if (ogg_sync_wrote(&state.sync, static_cast<long>(take)) != 0) {
                return fail("media.audio.decode_failed", "libogg rejected the input block");
            }
            offset += take;
            continue;
        }
        if (pageStatus < 0) {
            return fail("media.audio.container_invalid", "Ogg page framing is corrupt");
        }

        const long serial = ogg_page_serialno(&page);
        if (state.sawEndOfStream) {
            return fail("media.audio.chained_stream",
                        "Ogg stream continues after the end of its logical bitstream");
        }
        if (!state.streamInitialized) {
            if (ogg_stream_init(&state.stream, serial) != 0) {
                return fail("media.audio.decode_failed", "libogg could not initialize the stream");
            }
            state.streamInitialized = true;
            state.serialNumber = serial;
        } else if (serial != state.serialNumber) {
            return fail("media.audio.chained_stream",
                        "Ogg stream carries more than one logical bitstream");
        }
        if (ogg_stream_pagein(&state.stream, &page) != 0) {
            return fail("media.audio.container_invalid", "Ogg page does not match its bitstream");
        }

        const ogg_int64_t granule = ogg_page_granulepos(&page);
        if (granule >= 0) {
            state.granuleEnd = static_cast<std::uint64_t>(granule);
            state.granuleValid = true;
        }
        if (ogg_page_eos(&page) != 0) {
            state.sawEndOfStream = true;
        }

        ogg_packet packet;
        while (ogg_stream_packetout(&state.stream, &packet) == 1) {
            if (state.headerPackets < 3) {
                if (vorbis_synthesis_headerin(&state.info, &state.comment, &packet) != 0) {
                    return fail("media.audio.container_invalid", "Vorbis header packet is invalid");
                }
                ++state.headerPackets;
                if (state.headerPackets == 3) {
                    const auto sampleRate = static_cast<std::uint32_t>(state.info.rate);
                    const auto channels = static_cast<std::uint32_t>(state.info.channels);
                    if (auto valid = validateAudioFormat(sampleRate, channels, budget); !valid) {
                        return core::unexpected(valid.error());
                    }
                    if (vorbis_synthesis_init(&state.dsp, &state.info) != 0) {
                        return fail("media.audio.decode_failed", "libvorbis could not initialize");
                    }
                    state.dspInitialized = true;
                    if (vorbis_block_init(&state.dsp, &state.block) != 0) {
                        return fail("media.audio.decode_failed",
                                    "libvorbis could not allocate a block");
                    }
                    state.blockInitialized = true;
                    accumulator.emplace(sampleRate, channels, budget);
                }
                continue;
            }
            if (!accumulator.has_value()) {
                return fail("media.audio.container_invalid", "Vorbis audio precedes its headers");
            }
            if (vorbis_synthesis(&state.block, &packet) == 0) {
                if (vorbis_synthesis_blockin(&state.dsp, &state.block) != 0) {
                    return fail("media.audio.decode_failed", "libvorbis rejected a block");
                }
            }
            const auto channelCount = static_cast<std::uint32_t>(state.info.channels);
            int available = 0;
            float** pcm = nullptr;
            while ((available = vorbis_synthesis_pcmout(&state.dsp, &pcm)) > 0) {
                interleaved.resize(static_cast<std::size_t>(available) * channelCount);
                for (int frame = 0; frame < available; ++frame) {
                    for (std::uint32_t channel = 0; channel < channelCount; ++channel) {
                        const float value = pcm[channel][frame];
                        if (!std::isfinite(value)) {
                            return fail("media.audio.sample_non_finite",
                                        "Vorbis produced a non-finite sample");
                        }
                        interleaved[static_cast<std::size_t>(frame) * channelCount + channel] =
                            detail::quantizeToS16(static_cast<double>(value));
                    }
                }
                if (auto appended = accumulator->append(interleaved); !appended) {
                    return core::unexpected(appended.error());
                }
                vorbis_synthesis_read(&state.dsp, available);
            }
        }
    }

    if (!state.sawEndOfStream) {
        return fail("media.audio.truncated", "Ogg stream has no end-of-stream page");
    }
    if (state.headerPackets < 3 || !accumulator.has_value()) {
        return fail("media.audio.truncated", "Ogg stream ended before its Vorbis headers");
    }
    if (!state.granuleValid) {
        return fail("media.audio.truncated", "Ogg stream has no final granule position");
    }
    if (state.granuleEnd > accumulator->frames()) {
        return fail("media.audio.truncated", "Vorbis granule end exceeds the decoded sample count");
    }
    accumulator->truncateToFrames(state.granuleEnd);
    if (accumulator->frames() == 0) {
        return fail("media.audio.container_invalid", "Vorbis stream produced no audio frames");
    }
    return finalizeAudio(std::move(*accumulator), std::string{vorbisDecoderName}, budget);
}

// ---------------------------------------------------------------------------------------------
// FLAC (libFLAC native stream, integer conversion)
// ---------------------------------------------------------------------------------------------

struct FlacClient final {
    std::span<const std::byte> source;
    std::size_t offset{};
    const MediaBudget* budget{};
    std::optional<AudioAccumulator> accumulator;
    std::uint32_t channels{};
    std::uint32_t bitsPerSample{};
    std::uint64_t declaredSamples{};
    std::uint64_t expectedSampleNumber{};
    std::string failureCode;
    std::string failureMessage;
    bool failed{};
    bool discontinuity{};

    void fail(std::string code, std::string message) {
        if (failureCode.empty()) {
            failureCode = std::move(code);
            failureMessage = std::move(message);
        }
        failed = true;
    }
};

FLAC__StreamDecoderReadStatus flacRead(const FLAC__StreamDecoder*, FLAC__byte buffer[],
                                       size_t* bytes, void* clientData) {
    auto* client = static_cast<FlacClient*>(clientData);
    if (*bytes == 0) {
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }
    const std::size_t remaining = client->source.size() - client->offset;
    if (remaining == 0) {
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    const std::size_t take = std::min(remaining, *bytes);
    std::memcpy(buffer, client->source.data() + client->offset, take);
    client->offset += take;
    *bytes = take;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

// The source is a complete in-memory span, so the decoder is given the seek, tell, length and eof
// callbacks as well. Without them libFLAC cannot distinguish the end of the last frame from a
// corrupt stream and reports a spurious lost sync on the final frame boundary.
FLAC__StreamDecoderSeekStatus flacSeek(const FLAC__StreamDecoder*, FLAC__uint64 absoluteByteOffset,
                                       void* clientData) {
    auto* client = static_cast<FlacClient*>(clientData);
    if (absoluteByteOffset > client->source.size()) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }
    client->offset = static_cast<std::size_t>(absoluteByteOffset);
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus flacTell(const FLAC__StreamDecoder*, FLAC__uint64* absoluteByteOffset,
                                       void* clientData) {
    const auto* client = static_cast<const FlacClient*>(clientData);
    *absoluteByteOffset = client->offset;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus flacLength(const FLAC__StreamDecoder*, FLAC__uint64* streamLength,
                                           void* clientData) {
    const auto* client = static_cast<const FlacClient*>(clientData);
    *streamLength = client->source.size();
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool flacEof(const FLAC__StreamDecoder*, void* clientData) {
    const auto* client = static_cast<const FlacClient*>(clientData);
    return client->offset >= client->source.size();
}

// The STREAMINFO block is delivered before the first audio frame, which is where the accumulator
// and every format decision are fixed.
void flacMetadata(const FLAC__StreamDecoder*, const FLAC__StreamMetadata* metadata,
                  void* clientData) {
    auto* client = static_cast<FlacClient*>(clientData);
    if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
        return;
    }
    const auto& info = metadata->data.stream_info;
    const auto sampleRate = static_cast<std::uint32_t>(info.sample_rate);
    const auto channels = static_cast<std::uint32_t>(info.channels);
    const auto bits = static_cast<std::uint32_t>(info.bits_per_sample);
    if (sampleRate == 0 || channels < 1 || channels > 8 || bits < 4 || bits > 32) {
        client->fail("media.audio.container_invalid", "FLAC stream parameters are invalid");
        return;
    }
    if (sampleRate < client->budget->minSampleRate || sampleRate > client->budget->maxSampleRate) {
        client->fail("media.audio.rate_unsupported",
                     "Audio sample rate is outside the media profile");
        return;
    }
    if (channels > client->budget->maxChannels) {
        client->fail("media.audio.channels_unsupported",
                     "Audio channel count is outside the media profile");
        return;
    }
    client->channels = channels;
    client->bitsPerSample = bits;
    client->declaredSamples = info.total_samples;
    client->accumulator.emplace(sampleRate, channels, *client->budget);
}

FLAC__StreamDecoderWriteStatus flacWrite(const FLAC__StreamDecoder*, const FLAC__Frame* frame,
                                         const FLAC__int32* const buffer[], void* clientData) {
    auto* client = static_cast<FlacClient*>(clientData);
    if (!client->accumulator.has_value()) {
        client->fail("media.audio.container_invalid", "FLAC audio frame precedes STREAMINFO");
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    const auto channels = frame->header.channels;
    const auto bits = frame->header.bits_per_sample;
    const auto blockSize = frame->header.blocksize;
    if (channels != client->channels || bits != client->bitsPerSample) {
        client->fail("media.audio.container_invalid", "FLAC frame parameters changed mid-stream");
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    // A hole or a reordered frame is a decoder error, never a silent fill.
    if (frame->header.number.sample_number != client->expectedSampleNumber) {
        client->discontinuity = true;
        client->fail("media.audio.decode_failed", "FLAC frame sequence has a hole or a reordering");
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    client->expectedSampleNumber += blockSize;

    std::vector<std::int16_t> interleaved(static_cast<std::size_t>(blockSize) * channels);
    for (FLAC__uint32 frameIndex = 0; frameIndex < blockSize; ++frameIndex) {
        for (FLAC__uint32 channel = 0; channel < channels; ++channel) {
            interleaved[static_cast<std::size_t>(frameIndex) * channels + channel] =
                detail::narrowToS16(buffer[channel][frameIndex], bits);
        }
    }
    if (auto appended = client->accumulator->append(interleaved); !appended) {
        client->fail(std::string{appended.error().code()}, std::string{appended.error().message()});
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void flacError(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus status,
               void* clientData) {
    auto* client = static_cast<FlacClient*>(clientData);
    const char* name = "an unknown error";
    switch (status) {
    case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
        name = "lost sync";
        break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
        name = "a bad frame header";
        break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
        name = "a frame CRC mismatch";
        break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
        name = "an unparseable stream";
        break;
    default:
        break;
    }
    // A sync or header failure at the very end of the source is a cut stream, not a corrupt one.
    if (client->offset >= client->source.size()) {
        client->fail("media.audio.truncated",
                     "FLAC stream ended before its last frame was complete");
        return;
    }
    client->fail("media.audio.decode_failed", std::string{"libFLAC reported "} + name);
}

[[nodiscard]] auto decodeFlac(std::span<const std::byte> source, const MediaBudget& budget)
    -> core::Result<AudioImportResult> {
    FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
    if (decoder == nullptr) {
        return core::unexpected(detail::mediaError("media.audio.decode_failed",
                                                   "libFLAC could not allocate a decoder"));
    }
    FlacClient client;
    client.source = source;
    client.budget = &budget;

    const auto fail = [decoder](std::string code, std::string message) {
        FLAC__stream_decoder_delete(decoder);
        return core::unexpected(detail::mediaError(std::move(code), std::move(message)));
    };

    // Native FLAC only: an Ogg-mapped FLAC stream fails this initialization.
    if (FLAC__stream_decoder_init_stream(decoder, flacRead, flacSeek, flacTell, flacLength, flacEof,
                                         flacWrite, flacMetadata, flacError,
                                         &client) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        return fail("media.audio.container_invalid",
                    "libFLAC could not initialize a native FLAC stream");
    }
    const bool processed = FLAC__stream_decoder_process_until_end_of_stream(decoder);
    const auto state = FLAC__stream_decoder_get_state(decoder);
    // finish() re-runs the frame CRC and stream MD5 checks.
    const bool verified = FLAC__stream_decoder_finish(decoder);
    FLAC__stream_decoder_delete(decoder);

    if (!client.failureCode.empty()) {
        return core::unexpected(detail::mediaError(client.failureCode, client.failureMessage));
    }
    if (state != FLAC__STREAM_DECODER_END_OF_STREAM) {
        // The decoder never reached the end of the stream: either the source stops inside a frame
        // or the metadata is unusable.
        return core::unexpected(
            detail::mediaError("media.audio.container_invalid",
                               "FLAC stream stopped before its end of stream marker"));
    }
    if (!processed || !verified || client.failed) {
        if (!client.accumulator.has_value()) {
            // No STREAMINFO was ever delivered, so the metadata itself is unusable.
            return core::unexpected(detail::mediaError("media.audio.container_invalid",
                                                       "FLAC metadata could not be read"));
        }
        // The stream MD5 covers every sample. A decoded sample count below the STREAMINFO total
        // means the source was cut or its declared length lies; either way nothing is published.
        if (client.declaredSamples != 0 && client.accumulator->frames() < client.declaredSamples) {
            return core::unexpected(detail::mediaError(
                "media.audio.truncated",
                "FLAC stream ended before the sample count its STREAMINFO declares"));
        }
        return core::unexpected(detail::mediaError("media.audio.decode_failed",
                                                   "libFLAC rejected the stream or its checksum"));
    }
    if (!client.accumulator.has_value()) {
        return core::unexpected(detail::mediaError("media.audio.container_invalid",
                                                   "FLAC stream has no STREAMINFO block"));
    }
    if (client.accumulator->frames() == 0) {
        return core::unexpected(detail::mediaError("media.audio.container_invalid",
                                                   "FLAC stream produced no audio frames"));
    }
    return finalizeAudio(std::move(*client.accumulator), std::string{flacDecoderName}, budget);
}

} // namespace

auto importAudio(std::span<const std::byte> source, const MediaBudget& budget)
    -> core::Result<AudioImportResult> {
    if (source.empty()) {
        return core::unexpected(detail::mediaError("media.source.empty", "Audio source is empty"));
    }
    if (source.size() > budget.maxEncodedBytes) {
        return core::unexpected(detail::budgetError("media.source.limit",
                                                    "Encoded audio exceeds the source budget",
                                                    budget.maxEncodedBytes, source.size()));
    }

    if (isMp3Source(source)) {
        auto result = decodeMp3(source, budget);
        if (result) {
            result->usage.sourceBytes = source.size();
        }
        return result;
    }
    if (isOggVorbisSource(source)) {
        auto result = decodeVorbis(source, budget);
        if (result) {
            result->usage.sourceBytes = source.size();
        }
        return result;
    }
    if (isFlacSource(source)) {
        auto result = decodeFlac(source, budget);
        if (result) {
            result->usage.sourceBytes = source.size();
        }
        return result;
    }
    return core::unexpected(detail::mediaError(
        "media.audio.format_unsupported",
        "Source is not MP3, Ogg Vorbis or native FLAC; no fallback decoder is used"));
}

} // namespace cuexis::media_import
