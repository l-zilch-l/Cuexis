#include <cuexis/audio_sdl/wav_decoder.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

void appendTag(std::vector<std::byte>& bytes, std::string_view tag) {
    for (char character : tag) {
        bytes.push_back(static_cast<std::byte>(character));
    }
}

void appendU16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void appendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

auto pcm16Wav() -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    appendTag(bytes, "RIFF");
    appendU32(bytes, 40);
    appendTag(bytes, "WAVE");
    appendTag(bytes, "fmt ");
    appendU32(bytes, 16);
    appendU16(bytes, 1);
    appendU16(bytes, 1);
    appendU32(bytes, 48000);
    appendU32(bytes, 96000);
    appendU16(bytes, 2);
    appendU16(bytes, 16);
    appendTag(bytes, "data");
    appendU32(bytes, 4);
    appendU16(bytes, 0);
    appendU16(bytes, 16384);
    return bytes;
}

auto float32Wav(std::uint32_t sampleBits) -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    appendTag(bytes, "RIFF");
    appendU32(bytes, 40);
    appendTag(bytes, "WAVE");
    appendTag(bytes, "fmt ");
    appendU32(bytes, 16);
    appendU16(bytes, 3);
    appendU16(bytes, 1);
    appendU32(bytes, 48000);
    appendU32(bytes, 192000);
    appendU16(bytes, 4);
    appendU16(bytes, 32);
    appendTag(bytes, "data");
    appendU32(bytes, 4);
    appendU32(bytes, sampleBits);
    return bytes;
}

} // namespace

TEST_CASE("WavDecoder accepts bounded PCM and produces immutable F32", "[audio][wav]") {
    const auto wav = pcm16Wav();
    const auto decoded = cuexis::audio_sdl::WavDecoder::decode(wav);
    REQUIRE(decoded.has_value());
    CHECK(decoded->sampleRate() == 48000);
    CHECK(decoded->channels() == 1);
    CHECK(decoded->frameCount() == 2);
    REQUIRE(decoded->samples().size() == 2);
    CHECK(decoded->samples()[0] == Catch::Approx(0.0F));
    CHECK(decoded->samples()[1] == Catch::Approx(0.5F));
}

TEST_CASE("WavDecoder accepts finite IEEE F32", "[audio][wav]") {
    const auto decoded = cuexis::audio_sdl::WavDecoder::decode(float32Wav(0x3F000000U));
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->samples().size() == 1);
    CHECK(decoded->samples()[0] == Catch::Approx(0.5F));
}

TEST_CASE("WavDecoder rejects truncation compression and malicious sizes", "[audio][wav]") {
    auto truncated = pcm16Wav();
    truncated.pop_back();
    CHECK_FALSE(cuexis::audio_sdl::WavDecoder::decode(truncated).has_value());

    auto compressed = pcm16Wav();
    compressed[20] = static_cast<std::byte>(2);
    CHECK_FALSE(cuexis::audio_sdl::WavDecoder::decode(compressed).has_value());

    auto malicious = pcm16Wav();
    malicious[40] = static_cast<std::byte>(0xFF);
    malicious[41] = static_cast<std::byte>(0xFF);
    malicious[42] = static_cast<std::byte>(0xFF);
    malicious[43] = static_cast<std::byte>(0x7F);
    CHECK_FALSE(cuexis::audio_sdl::WavDecoder::decode(malicious).has_value());

    auto rf64 = pcm16Wav();
    rf64[0] = static_cast<std::byte>('R');
    rf64[1] = static_cast<std::byte>('F');
    rf64[2] = static_cast<std::byte>('6');
    rf64[3] = static_cast<std::byte>('4');
    CHECK_FALSE(cuexis::audio_sdl::WavDecoder::decode(rf64).has_value());

    CHECK_FALSE(cuexis::audio_sdl::WavDecoder::decode(float32Wav(0x7FC00000U)).has_value());
}

TEST_CASE("WavDecoder rejects duplicate data chunks even when the first is empty", "[audio][wav]") {
    std::vector<std::byte> bytes;
    appendTag(bytes, "RIFF");
    appendU32(bytes, 48);
    appendTag(bytes, "WAVE");
    appendTag(bytes, "fmt ");
    appendU32(bytes, 16);
    appendU16(bytes, 1);
    appendU16(bytes, 1);
    appendU32(bytes, 48000);
    appendU32(bytes, 96000);
    appendU16(bytes, 2);
    appendU16(bytes, 16);
    appendTag(bytes, "data");
    appendU32(bytes, 0);
    appendTag(bytes, "data");
    appendU32(bytes, 4);
    appendU16(bytes, 0);
    appendU16(bytes, 1);
    CHECK_FALSE(cuexis::audio_sdl::WavDecoder::decode(bytes).has_value());
}
