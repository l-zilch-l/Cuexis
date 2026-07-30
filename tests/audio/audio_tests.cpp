#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio/audio_transport.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

TEST_CASE("AudioConfig validates the complete stage 1D range", "[audio][config]") {
    const auto defaults = cuexis::audio::validateAudioConfig({});
    REQUIRE(defaults.has_value());
    CHECK(defaults->targetQueueMs() == 200);
    CHECK(defaults->refillLowWaterMs() == 100);
    CHECK(defaults->gain() == 1.0F);

    CHECK_FALSE(cuexis::audio::validateAudioConfig({.targetQueueMs = 39}).has_value());
    CHECK_FALSE(cuexis::audio::validateAudioConfig({.targetQueueMs = 1001}).has_value());
    CHECK_FALSE(cuexis::audio::validateAudioConfig({.refillLowWaterMs = 9}).has_value());
    CHECK_FALSE(cuexis::audio::validateAudioConfig({.targetQueueMs = 100, .refillLowWaterMs = 100})
                    .has_value());
    CHECK_FALSE(
        cuexis::audio::validateAudioConfig({.gain = std::numeric_limits<float>::quiet_NaN()})
            .has_value());
    CHECK_FALSE(cuexis::audio::validateAudioConfig({.gain = std::numeric_limits<float>::infinity()})
                    .has_value());
    CHECK_FALSE(cuexis::audio::validateAudioConfig({.gain = 1.01F}).has_value());
}

TEST_CASE("AudioClipStore enforces clip count and byte budgets", "[audio][store][limits]") {
    cuexis::audio::AudioClipStore store{{.maxClips = 1, .maxClipBytes = 16, .maxTotalBytes = 16}};
    auto first = cuexis::audio::AudioClip::create(8000, 1, std::vector<float>(4, 0.0F));
    REQUIRE(first.has_value());
    REQUIRE(store.registerClip(std::move(*first)).has_value());

    auto second = cuexis::audio::AudioClip::create(8000, 1, std::vector<float>(1, 0.0F));
    REQUIRE(second.has_value());
    const auto rejected = store.registerClip(std::move(*second));
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "audio.store.capacity");
}

TEST_CASE("AudioClip and store enforce immutable typed lifetime", "[audio][clip][store]") {
    auto clip = cuexis::audio::AudioClip::create(48000, 2, std::vector<float>(960, 0.25F));
    REQUIRE(clip.has_value());
    CHECK(clip->frameCount() == 480);
    CHECK(clip->durationMs() == Catch::Approx(10.0));

    cuexis::audio::AudioClipStore first;
    cuexis::audio::AudioClipStore second;
    const auto handle = first.registerClip(std::move(*clip));
    REQUIRE(handle.has_value());
    CHECK(handle->storeToken == first.storeToken());
    CHECK_FALSE(second.lease(*handle).has_value());

    auto lease = first.lease(*handle);
    REQUIRE(lease.has_value());
    REQUIRE(first.remove(*handle).has_value());
    CHECK_FALSE(first.lease(*handle).has_value());
    CHECK(lease->get()->frameCount() == 480);

    auto replacement = cuexis::audio::AudioClip::create(44100, 1, std::vector<float>(441, 0.5F));
    REQUIRE(replacement.has_value());
    const auto replacementHandle = first.registerClip(std::move(*replacement));
    REQUIRE(replacementHandle.has_value());
    CHECK(replacementHandle->index == handle->index);
    CHECK(replacementHandle->generation != handle->generation);
}

TEST_CASE("AudioClip rejects malformed PCM and non-finite samples", "[audio][clip]") {
    CHECK_FALSE(cuexis::audio::AudioClip::create(7999, 1, {0.0F}).has_value());
    CHECK_FALSE(cuexis::audio::AudioClip::create(48000, 3, {0.0F, 0.0F, 0.0F}).has_value());
    CHECK_FALSE(cuexis::audio::AudioClip::create(48000, 2, {0.0F}).has_value());
    CHECK_FALSE(cuexis::audio::AudioClip::create(48000, 1, {std::numeric_limits<float>::infinity()})
                    .has_value());
}

TEST_CASE("HostClock rejects source regressions within a segment", "[audio][clock]") {
    cuexis::audio::HostClock clock;
    REQUIRE(clock.submit({100.0, cuexis::audio::PlaybackState::Playing, 0}).has_value());
    REQUIRE(clock.submit({125.0, cuexis::audio::PlaybackState::Playing, 0}).has_value());
    CHECK_FALSE(clock.submit({124.0, cuexis::audio::PlaybackState::Playing, 0}).has_value());
    REQUIRE(clock.submit({50.0, cuexis::audio::PlaybackState::Playing, 1}).has_value());
    CHECK(clock.snapshot().positionMs == Catch::Approx(50.0));
}

TEST_CASE("Source clock validation rejects invalid states and stopped positions",
          "[audio][clock]") {
    REQUIRE(
        cuexis::audio::validateSourceClockSample({-25.0, cuexis::audio::PlaybackState::Playing, 0})
            .has_value());
    REQUIRE(
        cuexis::audio::validateSourceClockSample({-10.0, cuexis::audio::PlaybackState::Paused, 0})
            .has_value());
    CHECK_FALSE(
        cuexis::audio::validateSourceClockSample({1.0, cuexis::audio::PlaybackState::Stopped, 0})
            .has_value());
    CHECK_FALSE(
        cuexis::audio::validateSourceClockSample({0.0, cuexis::audio::PlaybackState::Empty, 0})
            .has_value());
    CHECK_FALSE(cuexis::audio::validateSourceClockSample(
                    {0.0, static_cast<cuexis::audio::PlaybackState>(255), 0})
                    .has_value());

    cuexis::audio::HostClock clock;
    REQUIRE(clock.submit({100.0, cuexis::audio::PlaybackState::Playing, 0}).has_value());
    CHECK_FALSE(clock.submit({99.0, cuexis::audio::PlaybackState::Paused, 0}).has_value());
}
