// S6-E1/S6-E2 media import tests.
//
// Every positive case is compared against a committed canonical golden in
// tests/fixtures/stage6_e/golden. The golden is the frozen canonical output of the fixed profile;
// a platform difference is a blocked profile decision, never a golden update.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/media_import/media_import.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace media = cuexis::media_import;

[[nodiscard]] auto fixtureRoot() -> const std::filesystem::path& {
    static const std::filesystem::path root =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "stage6_e" / "media";
    return root;
}

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::vector<std::byte> {
    INFO("fixture path: " << path.string());
    std::ifstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    std::vector<std::byte> bytes;
    char buffer[4096];
    while (stream) {
        stream.read(buffer, sizeof(buffer));
        const auto read = stream.gcount();
        for (std::streamsize index = 0; index < read; ++index) {
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(buffer[index])));
        }
    }
    return bytes;
}

[[nodiscard]] auto fixture(std::string_view relative) -> std::vector<std::byte> {
    return readFile(fixtureRoot() / relative);
}

[[nodiscard]] auto readU32Le(const std::vector<std::byte>& bytes, std::size_t offset)
    -> std::uint32_t {
    REQUIRE(offset + 4 <= bytes.size());
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
                 << (8U * index);
    }
    return value;
}

[[nodiscard]] auto golden(std::string_view stem) -> std::string {
    const auto path = fixtureRoot().parent_path() / "golden" / (std::string{stem} + ".json");
    std::ifstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto jsonString(std::string_view json, std::string_view key) -> std::string {
    const std::string needle = "\"" + std::string{key} + "\"";
    const auto keyAt = json.find(needle);
    REQUIRE(keyAt != std::string_view::npos);
    auto valueAt = json.find(':', keyAt + needle.size());
    REQUIRE(valueAt != std::string_view::npos);
    ++valueAt;
    while (valueAt < json.size() && std::isspace(static_cast<unsigned char>(json[valueAt])) != 0) {
        ++valueAt;
    }
    REQUIRE(valueAt < json.size());
    REQUIRE(json[valueAt] == '"');
    ++valueAt;
    const auto end = json.find('"', valueAt);
    REQUIRE(end != std::string_view::npos);
    return std::string{json.substr(valueAt, end - valueAt)};
}

[[nodiscard]] auto jsonUint(std::string_view json, std::string_view key) -> std::uint64_t {
    const std::string needle = "\"" + std::string{key} + "\"";
    const auto keyAt = json.find(needle);
    REQUIRE(keyAt != std::string_view::npos);
    auto valueAt = json.find(':', keyAt + needle.size());
    REQUIRE(valueAt != std::string_view::npos);
    ++valueAt;
    while (valueAt < json.size() && std::isspace(static_cast<unsigned char>(json[valueAt])) != 0) {
        ++valueAt;
    }
    std::uint64_t value = 0;
    std::size_t digits = 0;
    while (valueAt < json.size() && std::isdigit(static_cast<unsigned char>(json[valueAt])) != 0) {
        value = value * 10 + static_cast<std::uint64_t>(json[valueAt] - '0');
        ++valueAt;
        ++digits;
    }
    REQUIRE(digits > 0);
    return value;
}

[[nodiscard]] auto toHex(std::span<const std::byte> bytes) -> std::string {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto value : bytes) {
        const auto byte = std::to_integer<std::uint8_t>(value);
        out.push_back(digits[byte >> 4U]);
        out.push_back(digits[byte & 0x0FU]);
    }
    return out;
}

// Prints an exact, platform-comparable summary of canonical PCM samples. It characterized the
// stereo Ogg Vorbis divergence that the pinned libvorbis M_PI patch resolved: per-block SHA-256
// localizes which samples differ and the integer absSum/sqSum make the difference measurable on
// any platform without transferring the artifact. Note that the aggregates characterize, they do
// not bound, the per-sample difference: sum(abs(x)) - sum(abs(y)) is not sum(abs(x - y)), and a
// permutation of samples can leave every aggregate unchanged. Byte equality stays the acceptance
// criterion, and this dump never replaces it.
void dumpSampleDiagnostics(std::string_view stem, std::span<const std::byte> bytes) {
    constexpr std::size_t headerBytes = 44;
    constexpr std::size_t samplesPerBlock = 4096;
    if (bytes.size() < headerBytes) {
        return;
    }
    const auto dataBytes = bytes.size() - headerBytes;
    const auto sampleCount = dataBytes / 2;
    const auto channelCount = std::to_integer<std::uint16_t>(bytes[22]);
    std::cout << "[s6e-diag] " << stem << " samples=" << sampleCount << " channels=" << channelCount
              << " bytes=" << bytes.size() << "\n";
    std::vector<std::int64_t> samples;
    samples.reserve(sampleCount);
    for (std::size_t index = 0; index < sampleCount; ++index) {
        const auto low = std::to_integer<std::uint16_t>(bytes[headerBytes + index * 2]);
        const auto high = std::to_integer<std::uint16_t>(bytes[headerBytes + index * 2 + 1]);
        samples.push_back(static_cast<std::int16_t>(low | static_cast<std::uint16_t>(high << 8U)));
    }
    for (std::size_t block = 0; block * samplesPerBlock < sampleCount; ++block) {
        const auto begin = block * samplesPerBlock;
        const auto end = std::min(sampleCount, begin + samplesPerBlock);
        std::uint64_t absSum = 0;
        std::uint64_t squareSum = 0;
        std::int64_t minimum = 0;
        std::int64_t maximum = 0;
        std::uint64_t clipped = 0;
        std::uint64_t zeros = 0;
        for (auto index = begin; index < end; ++index) {
            const auto value = samples[index];
            absSum += static_cast<std::uint64_t>(value < 0 ? -value : value);
            squareSum += static_cast<std::uint64_t>(value * value);
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
            clipped += (value == 32767 || value == -32768) ? 1U : 0U;
            zeros += value == 0 ? 1U : 0U;
        }
        std::vector<std::byte> blockBytes(bytes.begin() + static_cast<std::ptrdiff_t>(headerBytes) +
                                              static_cast<std::ptrdiff_t>(begin * 2),
                                          bytes.begin() + static_cast<std::ptrdiff_t>(headerBytes) +
                                              static_cast<std::ptrdiff_t>(end * 2));
        std::cout << "[s6e-diag] " << stem << " block=" << block << " samples=" << (end - begin)
                  << " sha=" << media::contentIdentity(blockBytes) << " absSum=" << absSum
                  << " sqSum=" << squareSum << " min=" << minimum << " max=" << maximum
                  << " clipped=" << clipped << " zeros=" << zeros << "\n";
    }
    std::cout << "[s6e-diag] " << stem << " head=";
    for (std::size_t index = 0; index < std::min<std::size_t>(16, sampleCount); ++index) {
        std::cout << samples[index]
                  << (index + 1 == std::min<std::size_t>(16, sampleCount) ? "" : ",");
    }
    std::cout << " tail=";
    for (auto index = sampleCount > 16 ? sampleCount - 16 : 0; index < sampleCount; ++index) {
        std::cout << samples[index] << (index + 1 == sampleCount ? "" : ",");
    }
    std::cout << "\n";
}

// Asserts that the canonical bytes match the frozen golden and returns them for further checks.
// Compares one canonical artifact against its committed golden. The golden carries the frozen
// profile, digest, byte count and, for small artifacts, the complete byte sequence.
void requireCanonical(std::span<const std::byte> bytes, std::string_view stem) {
    const auto document = golden(stem);
    INFO("golden: " << stem);
    CHECK(media::contentIdentity(bytes) == jsonString(document, "sha256"));
    CHECK(bytes.size() == jsonUint(document, "byteCount"));
    const auto hexAt = document.find("\"bytesHex\"");
    if (hexAt != std::string::npos) {
        CHECK(toHex(bytes) == jsonString(document, "bytesHex"));
    }
}

[[nodiscard]] auto importImageOrFail(std::span<const std::byte> source)
    -> media::ImageImportResult {
    auto result = media::importImage(source);
    REQUIRE(result.has_value());
    return std::move(*result);
}

[[nodiscard]] auto importAudioOrFail(std::span<const std::byte> source)
    -> media::AudioImportResult {
    auto result = media::importAudio(source);
    REQUIRE(result.has_value());
    return std::move(*result);
}

[[nodiscard]] auto importImageCode(std::span<const std::byte> source,
                                   const media::MediaBudget& budget = {}) -> std::string {
    auto result = media::importImage(source, budget);
    REQUIRE_FALSE(result.has_value());
    return std::string{result.error().code()};
}

[[nodiscard]] auto importAudioCode(std::span<const std::byte> source,
                                   const media::MediaBudget& budget = {}) -> std::string {
    auto result = media::importAudio(source, budget);
    REQUIRE_FALSE(result.has_value());
    return std::string{result.error().code()};
}

} // namespace

TEST_CASE("media profile identity is versioned and stable", "[media_import]") {
    const auto first = media::mediaProfileIdentity();
    CHECK(first == media::mediaProfileIdentity());
    CHECK(first.size() == 64);
    CHECK(std::all_of(first.begin(), first.end(), [](char value) {
        return std::isxdigit(static_cast<unsigned char>(value)) != 0;
    }));
}

TEST_CASE("PNG truecolor imports to the canonical portable texture", "[media_import][e1]") {
    const auto source = fixture("image/rgb8.png");
    const auto result = importImageOrFail(source);
    const auto document = golden("image_rgb8");

    CHECK(result.info.decoder == jsonString(document, "decoder"));
    CHECK(result.info.width == jsonUint(document, "width"));
    CHECK(result.info.height == jsonUint(document, "height"));
    CHECK(result.info.orientation == 1);
    CHECK(result.usage.sourceBytes == source.size());
    CHECK(result.usage.decodedBytes == jsonUint(document, "decodedBytes"));

    requireCanonical(result.portableTexture, "image_rgb8");
    const auto& bytes = result.portableTexture;
    // CXPRES01, kind 2, payload version 1, sRGB and straight alpha in top-left order.
    CHECK(std::string_view{reinterpret_cast<const char*>(bytes.data()), 8} == "CXPRES01");
    CHECK(std::to_integer<std::uint32_t>(bytes[8]) == 2);
    CHECK(std::to_integer<std::uint32_t>(bytes[12]) == 1);
    CHECK(bytes.size() == 40 + 3 * 2 * 4);
    CHECK(std::to_integer<std::uint32_t>(bytes[32]) == 2); // sRGB
    CHECK(std::to_integer<std::uint32_t>(bytes[36]) == 0); // reserved
    CHECK(std::to_integer<std::uint8_t>(bytes[40]) == 0x10);
    CHECK(std::to_integer<std::uint8_t>(bytes[43]) == 0xFF);
}

TEST_CASE("PNG alpha, palette tRNS and grayscale expand to RGBA8", "[media_import][e1]") {
    const auto rgba = importImageOrFail(fixture("image/rgba8.png"));
    CHECK(rgba.info.width == 2);
    CHECK(rgba.info.height == 2);
    requireCanonical(rgba.portableTexture, "image_rgba8");

    const auto palette = importImageOrFail(fixture("image/palette_trns.png"));
    CHECK(palette.info.width == 2);
    CHECK(palette.info.height == 1);
    requireCanonical(palette.portableTexture, "image_palette_trns");

    const auto gray = importImageOrFail(fixture("image/gray8.png"));
    CHECK(gray.info.width == 2);
    CHECK(gray.info.height == 1);
    requireCanonical(gray.portableTexture, "image_gray8");

    const auto srgb = importImageOrFail(fixture("image/srgb_chunk.png"));
    requireCanonical(srgb.portableTexture, "image_srgb_chunk");
    CHECK(std::to_integer<std::uint32_t>(srgb.portableTexture[32]) == 2);
}

TEST_CASE("PNG rejects unsupported color metadata, animation and damage", "[media_import][e1]") {
    CHECK(importImageCode(fixture("image/gray_alpha16.png")) ==
          "media.image.bit_depth_unsupported");
    CHECK(importImageCode(fixture("image/apng.png")) == "media.image.format_unsupported");
    CHECK(importImageCode(fixture("image/iccp_non_srgb.png")) == "media.image.icc_unsupported");
    CHECK(importImageCode(fixture("image/gamma_unsupported.png")) ==
          "media.image.gamma_unsupported");
    CHECK(importImageCode(fixture("image/forged_dimensions.png")) ==
          "media.image.dimension_invalid");
    CHECK(importImageCode(fixture("image/corrupt_chunk.png")) == "media.image.decode_failed");
    CHECK(importImageCode(fixture("image/truncated.png")) == "media.image.truncated");
}

TEST_CASE("JPEG baseline, progressive and grayscale import to the canonical texture",
          "[media_import][e1]") {
    const auto baseline = importImageOrFail(fixture("image/baseline.jpg"));
    CHECK(baseline.info.decoder == "libjpeg-turbo-3.2.0");
    CHECK(baseline.info.width == 4);
    CHECK(baseline.info.height == 2);
    requireCanonical(baseline.portableTexture, "image_baseline");

    const auto progressive = importImageOrFail(fixture("image/progressive.jpg"));
    requireCanonical(progressive.portableTexture, "image_progressive");

    const auto gray = importImageOrFail(fixture("image/gray.jpg"));
    requireCanonical(gray.portableTexture, "image_gray");

    // Entropy coding and the identity orientation must not change the canonical pixels.
    CHECK(progressive.portableTexture == baseline.portableTexture);
}

TEST_CASE("JPEG rejects CMYK, YCCK and unsupported frame types", "[media_import][e1]") {
    CHECK(importImageCode(fixture("image/cmyk.jpg")) == "media.image.color_type_unsupported");
    CHECK(importImageCode(fixture("image/ycck.jpg")) == "media.image.color_type_unsupported");
}

TEST_CASE("EXIF orientation is normalized once", "[media_import][e1]") {
    for (const auto orientation : {1, 3, 6, 8}) {
        const auto name = std::string{"orientation"} + std::to_string(orientation);
        const auto result = importImageOrFail(fixture("image/" + name + ".jpg"));
        INFO("orientation " << orientation);
        CHECK(result.info.orientation == orientation);
        CHECK(result.info.sourceWidth == 4);
        CHECK(result.info.sourceHeight == 2);
        if (orientation >= 5) {
            CHECK(result.info.width == 2);
            CHECK(result.info.height == 4);
        } else {
            CHECK(result.info.width == 4);
            CHECK(result.info.height == 2);
        }
        requireCanonical(result.portableTexture, "image_" + name);
    }
    // Orientation 1 is the identity, so it must equal the plain baseline import.
    const auto identity = importImageOrFail(fixture("image/orientation1.jpg"));
    const auto baseline = importImageOrFail(fixture("image/baseline.jpg"));
    CHECK(identity.portableTexture == baseline.portableTexture);
}

TEST_CASE("EXIF orientation outside the range or contradictory is rejected", "[media_import][e1]") {
    CHECK(importImageCode(fixture("image/orientation_invalid.jpg")) ==
          "media.image.orientation_invalid");
    CHECK(importImageCode(fixture("image/orientation_conflict.jpg")) ==
          "media.image.orientation_invalid");
}

TEST_CASE("image budgets reject oversized sources, pixels and working memory",
          "[media_import][e1]") {
    const auto source = fixture("image/rgb8.png");

    media::MediaBudget sourceBudget;
    sourceBudget.maxEncodedBytes = source.size() - 1;
    CHECK(importImageCode(source, sourceBudget) == "media.source.limit");

    media::MediaBudget pixelBudget;
    pixelBudget.maxImagePixels = 4; // the fixture has 6 pixels
    CHECK(importImageCode(source, pixelBudget) == "media.image.pixel_limit");

    media::MediaBudget byteBudget;
    byteBudget.maxImageBytes = 8;
    CHECK(importImageCode(source, byteBudget) == "media.image.byte_limit");

    media::MediaBudget workingBudget;
    workingBudget.maxWorkingBytes = 1;
    CHECK(importImageCode(source, workingBudget) == "media.budget.working_limit");

    CHECK(importImageCode(std::span<const std::byte>{}) == "media.source.empty");
}

TEST_CASE("image import is deterministic and rejects unknown sources", "[media_import][e1]") {
    const auto source = fixture("image/baseline.jpg");
    const auto first = importImageOrFail(source);
    const auto second = importImageOrFail(source);
    CHECK(first.portableTexture == second.portableTexture);
    CHECK(media::contentIdentity(first.portableTexture) ==
          media::contentIdentity(second.portableTexture));

    const std::vector<std::byte> unknown{std::byte{'n'}, std::byte{'o'}, std::byte{'p'},
                                         std::byte{'e'}};
    CHECK(importImageCode(unknown) == "media.image.format_unsupported");
    CHECK(importAudioCode(unknown) == "media.audio.format_unsupported");
}

TEST_CASE("MP3, Ogg Vorbis and FLAC import to the canonical WAV", "[media_import][e2]") {
    const auto mp3 = importAudioOrFail(fixture("audio/mono.mp3"));
    CHECK(mp3.info.decoder == "minimp3-2021-11-30-no-simd");
    CHECK(mp3.info.sampleRate == 8000);
    CHECK(mp3.info.channels == 1);
    requireCanonical(mp3.canonicalWav, "audio_mono_mp3");

    const auto ogg = importAudioOrFail(fixture("audio/mono.ogg"));
    CHECK(ogg.info.decoder == "libvorbis-1.3.7-pinned-mpi-libogg-1.3.6");
    CHECK(ogg.info.sampleRate == 8000);
    CHECK(ogg.info.channels == 1);
    requireCanonical(ogg.canonicalWav, "audio_mono_ogg");

    const auto flac = importAudioOrFail(fixture("audio/mono.flac"));
    CHECK(flac.info.decoder == "libflac-1.5.0-native");
    CHECK(flac.info.sampleRate == 8000);
    CHECK(flac.info.channels == 1);
    requireCanonical(flac.canonicalWav, "audio_mono_flac");

    // FLAC is lossless: the canonical WAV must equal the PCM source the fixture was encoded from,
    // which also pins the canonical header layout against an independently written file.
    CHECK(flac.canonicalWav == fixture("audio/source_mono.wav"));
}

TEST_CASE("audio preserves sample rate and mono or stereo channel count", "[media_import][e2]") {
    const auto mp3 = importAudioOrFail(fixture("audio/stereo.mp3"));
    CHECK(mp3.info.sampleRate == 44100);
    CHECK(mp3.info.channels == 2);
    requireCanonical(mp3.canonicalWav, "audio_stereo_mp3");

    const auto ogg = importAudioOrFail(fixture("audio/stereo.ogg"));
    CHECK(ogg.info.sampleRate == 44100);
    CHECK(ogg.info.channels == 2);
    dumpSampleDiagnostics("audio_stereo_ogg", ogg.canonicalWav);
    requireCanonical(ogg.canonicalWav, "audio_stereo_ogg");

    const auto flac = importAudioOrFail(fixture("audio/stereo.flac"));
    CHECK(flac.info.sampleRate == 44100);
    CHECK(flac.info.channels == 2);
    requireCanonical(flac.canonicalWav, "audio_stereo_flac");
    CHECK(flac.canonicalWav == fixture("audio/source_stereo.wav"));
}

TEST_CASE("canonical WAV holds only the fmt and data chunks", "[media_import][e2]") {
    const auto result = importAudioOrFail(fixture("audio/mono.flac"));
    const auto& bytes = result.canonicalWav;
    REQUIRE(bytes.size() > 44);
    CHECK(readU32Le(bytes, 4) == bytes.size() - 8); // RIFF size
    CHECK(readU32Le(bytes, 16) == 16);              // fmt chunk size
    CHECK(readU32Le(bytes, 24) == 8000);            // sample rate
    CHECK(readU32Le(bytes, 28) == 8000 * 2);        // byte rate
    CHECK(readU32Le(bytes, 40) == result.info.frames * 2);
    CHECK(std::string_view{reinterpret_cast<const char*>(bytes.data()), 4} == "RIFF");
    CHECK(std::string_view{reinterpret_cast<const char*>(bytes.data() + 8), 4} == "WAVE");
    CHECK(std::string_view{reinterpret_cast<const char*>(bytes.data() + 12), 4} == "fmt ");
    CHECK(std::to_integer<std::uint16_t>(bytes[20]) == 1);  // PCM
    CHECK(std::to_integer<std::uint16_t>(bytes[22]) == 1);  // channels
    CHECK(std::to_integer<std::uint16_t>(bytes[32]) == 2);  // block align
    CHECK(std::to_integer<std::uint16_t>(bytes[34]) == 16); // bits per sample
    CHECK(std::string_view{reinterpret_cast<const char*>(bytes.data() + 36), 4} == "data");
    CHECK(bytes.size() == 44 + result.info.frames * 2);
    CHECK(result.usage.decodedBytes == bytes.size());
}

TEST_CASE("audio rejects truncation, bad headers and chained streams", "[media_import][e2]") {
    CHECK(importAudioCode(fixture("audio/truncated.mp3")) == "media.audio.truncated");
    CHECK(importAudioCode(fixture("audio/truncated.ogg")) == "media.audio.truncated");
    CHECK(importAudioCode(fixture("audio/truncated.flac")) == "media.audio.truncated");
    CHECK(importAudioCode(fixture("audio/bad_header.flac")) == "media.audio.container_invalid");
    CHECK(importAudioCode(fixture("audio/chained.ogg")) == "media.audio.chained_stream");
}

TEST_CASE("audio rejects unsupported channel counts and sample rates", "[media_import][e2]") {
    CHECK(importAudioCode(fixture("audio/channels3.flac")) == "media.audio.channels_unsupported");
    CHECK(importAudioCode(fixture("audio/rate_out_of_range.flac")) ==
          "media.audio.rate_unsupported");

    media::MediaBudget narrowRate;
    narrowRate.maxSampleRate = 8000;
    CHECK(importAudioCode(fixture("audio/stereo.flac"), narrowRate) ==
          "media.audio.rate_unsupported");

    media::MediaBudget monoOnly;
    monoOnly.maxChannels = 1;
    CHECK(importAudioCode(fixture("audio/stereo.ogg"), monoOnly) ==
          "media.audio.channels_unsupported");
}

TEST_CASE("a forged FLAC sample count does not change the decoded frames", "[media_import][e2]") {
    const auto honest = importAudioOrFail(fixture("audio/mono.flac"));
    const auto forged = importAudioOrFail(fixture("audio/forged_total_samples.flac"));
    CHECK(forged.info.frames == honest.info.frames);
    CHECK(forged.canonicalWav == honest.canonicalWav);
}

TEST_CASE("audio budgets reject duration, final WAV and source overflow", "[media_import][e2]") {
    const auto source = fixture("audio/mono.flac");

    media::MediaBudget sourceBudget;
    sourceBudget.maxEncodedBytes = source.size() - 1;
    CHECK(importAudioCode(source, sourceBudget) == "media.source.limit");

    media::MediaBudget durationBudget;
    durationBudget.maxAudioSeconds = 0;
    CHECK(importAudioCode(source, durationBudget) == "media.audio.duration_limit");

    media::MediaBudget wavBudget;
    wavBudget.maxWavBytes = 100;
    CHECK(importAudioCode(source, wavBudget) == "media.audio.wav_limit");

    CHECK(importAudioCode(std::span<const std::byte>{}) == "media.source.empty");
}

TEST_CASE("audio import is deterministic", "[media_import][e2]") {
    const auto source = fixture("audio/stereo.ogg");
    const auto first = importAudioOrFail(source);
    const auto second = importAudioOrFail(source);
    CHECK(first.canonicalWav == second.canonicalWav);
    CHECK(first.info.frames == second.info.frames);
}
