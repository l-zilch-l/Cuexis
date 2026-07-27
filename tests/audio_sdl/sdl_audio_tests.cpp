#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/audio_sdl/sdl_audio.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <future>
#include <vector>

TEST_CASE("SDL dummy transport implements the owner-thread state machine", "[audio][sdl]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 0.5F});
    REQUIRE(config.has_value());

    cuexis::audio::AudioClipStore store;
    auto clip = cuexis::audio::AudioClip::create(48000, 2, std::vector<float>(9600, 0.1F));
    REQUIRE(clip.has_value());
    const auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle.has_value());

    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());
    CHECK(transport->snapshot().source.state == cuexis::audio::PlaybackState::Empty);
    REQUIRE(transport->load(*handle).has_value());
    CHECK(transport->snapshot().source.state == cuexis::audio::PlaybackState::Stopped);
    REQUIRE(transport->play().has_value());
    REQUIRE(transport->service().has_value());
    REQUIRE(transport->pause().has_value());
    CHECK(transport->snapshot().source.state == cuexis::audio::PlaybackState::Paused);
    REQUIRE(transport->seekMs(25.0).has_value());
    CHECK_FALSE(transport->seekMs(101.0).has_value());
    REQUIRE(transport->stop().has_value());
    REQUIRE(transport->unload().has_value());
    CHECK(transport->snapshot().source.state == cuexis::audio::PlaybackState::Empty);
}

TEST_CASE("SDL dummy transport preserves idempotence and seek endpoints", "[audio][sdl][state]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 1.0F});
    REQUIRE(config.has_value());
    cuexis::audio::AudioClipStore store;
    auto clip = cuexis::audio::AudioClip::create(48000, 2, std::vector<float>(96000, 0.1F));
    REQUIRE(clip.has_value());
    const auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle.has_value());
    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());

    REQUIRE(transport->load(*handle).has_value());
    const auto loadedId = transport->snapshot().source.discontinuityId;
    CHECK(loadedId > 0);
    REQUIRE(transport->stop().has_value());
    CHECK(transport->snapshot().source.discontinuityId == loadedId);
    REQUIRE(transport->play().has_value());
    REQUIRE(transport->play().has_value());
    CHECK(transport->snapshot().source.discontinuityId == loadedId);
    REQUIRE(transport->pause().has_value());
    const auto paused = transport->snapshot();
    REQUIRE(transport->pause().has_value());
    CHECK(transport->snapshot().source.discontinuityId == paused.source.discontinuityId);
    REQUIRE(transport->seekMs(1000.0).has_value());
    const auto endSeek = transport->snapshot();
    CHECK(endSeek.source.positionMs == 1000.0);
    CHECK(endSeek.source.state == cuexis::audio::PlaybackState::Paused);
    CHECK(endSeek.source.discontinuityId != paused.source.discontinuityId);
    REQUIRE(transport->play().has_value());
    REQUIRE(transport->service().has_value());
    CHECK(transport->snapshot().source.state == cuexis::audio::PlaybackState::Ended);
    CHECK_FALSE(transport->play().has_value());
    REQUIRE(transport->stop().has_value());
    CHECK(transport->snapshot().source.state == cuexis::audio::PlaybackState::Stopped);
    CHECK(transport->snapshot().source.positionMs == 0.0);
    REQUIRE(transport->unload().has_value());
    CHECK_FALSE(transport->play().has_value());
}

TEST_CASE("SDL dummy clock snapshot is safe during owner-thread service", "[audio][sdl][clock]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 1.0F});
    REQUIRE(config.has_value());
    cuexis::audio::AudioClipStore store;
    auto clip = cuexis::audio::AudioClip::create(48000, 2, std::vector<float>(96000, 0.1F));
    REQUIRE(clip.has_value());
    const auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle.has_value());
    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());
    REQUIRE(transport->load(*handle).has_value());
    REQUIRE(transport->play().has_value());

    std::atomic<bool> valid{true};
    auto reader = std::async(std::launch::async, [&] {
        for (int index = 0; index < 20000; ++index) {
            const auto clock = transport->snapshot();
            if (!std::isfinite(clock.source.positionMs) || clock.source.positionMs < 0.0 ||
                clock.sampleRate != 48000) {
                valid.store(false, std::memory_order_relaxed);
                break;
            }
        }
    });
    for (int index = 0; index < 100; ++index) {
        REQUIRE(transport->service().has_value());
    }
    reader.get();
    CHECK(valid.load(std::memory_order_relaxed));
}
