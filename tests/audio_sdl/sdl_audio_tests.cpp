#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/audio_sdl/sdl_audio.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] auto registerClip(cuexis::audio::AudioClipStore& store, std::size_t frameCount,
                                float sample = 0.1F) -> cuexis::audio::AudioClipHandle {
    auto clip =
        cuexis::audio::AudioClip::create(48000, 2, std::vector<float>(frameCount * 2U, sample));
    REQUIRE(clip.has_value());
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle.has_value());
    return *handle;
}

void checkEffectiveSettingsEqual(const cuexis::audio::EffectiveAudioSettings& actual,
                                 const cuexis::audio::EffectiveAudioSettings& expected) {
    CHECK(actual.sourceSampleRate == expected.sourceSampleRate);
    CHECK(actual.sourceChannels == expected.sourceChannels);
    CHECK(actual.deviceSampleRate == expected.deviceSampleRate);
    CHECK(actual.deviceChannels == expected.deviceChannels);
    CHECK(actual.deviceBufferFrames == expected.deviceBufferFrames);
    CHECK(actual.targetQueueMs == expected.targetQueueMs);
    CHECK(actual.refillLowWaterMs == expected.refillLowWaterMs);
    CHECK(actual.estimatedOutputLatencyMs == expected.estimatedOutputLatencyMs);
    CHECK(actual.formatConverted == expected.formatConverted);
    CHECK(actual.defaultRouteMayMigrate == expected.defaultRouteMayMigrate);
}

[[nodiscard]] auto isEmptyEffectiveSettings(const cuexis::audio::EffectiveAudioSettings& settings)
    -> bool {
    return settings.sourceSampleRate == 0 && settings.sourceChannels == 0 &&
           settings.deviceSampleRate == 0 && settings.deviceChannels == 0 &&
           settings.deviceBufferFrames == 0 && settings.targetQueueMs == 0 &&
           settings.refillLowWaterMs == 0 && settings.estimatedOutputLatencyMs == 0.0 &&
           !settings.formatConverted && !settings.defaultRouteMayMigrate;
}

[[nodiscard]] auto isLoadedEffectiveSettings(const cuexis::audio::EffectiveAudioSettings& settings)
    -> bool {
    return settings.sourceSampleRate == 48000 && settings.sourceChannels == 2 &&
           settings.deviceSampleRate > 0 && settings.deviceChannels > 0 &&
           settings.deviceBufferFrames > 0 && settings.targetQueueMs == 40 &&
           settings.refillLowWaterMs == 10 && std::isfinite(settings.estimatedOutputLatencyMs) &&
           settings.estimatedOutputLatencyMs >= 0.0 && settings.defaultRouteMayMigrate;
}

[[nodiscard]] auto audioImplementationSource() -> std::string {
    const auto path =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "engine" / "audio_sdl" / "src" / "sdl_audio.cpp";
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input.good());
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto functionRegion(const std::string& source, std::string_view functionName)
    -> std::optional<std::string_view> {
    const auto functionStart = source.find(functionName);
    if (functionStart == std::string::npos) {
        return std::nullopt;
    }
    const auto openBrace = source.find('{', functionStart);
    if (openBrace == std::string::npos) {
        return std::nullopt;
    }
    std::size_t depth = 0;
    for (std::size_t index = openBrace; index < source.size(); ++index) {
        if (source[index] == '{') {
            ++depth;
        } else if (source[index] == '}' && --depth == 0) {
            return std::string_view{source}.substr(openBrace + 1, index - openBrace - 1);
        }
    }
    return std::nullopt;
}

} // namespace

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

TEST_CASE("SDL effective settings follow load and unload state transitions",
          "[audio][sdl][effective][state]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 1.0F});
    REQUIRE(config.has_value());

    cuexis::audio::AudioClipStore store;
    const auto handle = registerClip(store, 9600);
    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());

    CHECK(isEmptyEffectiveSettings(transport->effectiveSettings()));
    REQUIRE(transport->load(handle).has_value());
    const auto loaded = transport->effectiveSettings();
    CHECK(isLoadedEffectiveSettings(loaded));
    CHECK(loaded.sourceSampleRate == 48000);
    CHECK(loaded.sourceChannels == 2);
    CHECK(loaded.targetQueueMs == 40);
    CHECK(loaded.refillLowWaterMs == 10);

    REQUIRE(transport->unload().has_value());
    CHECK(isEmptyEffectiveSettings(transport->effectiveSettings()));
    CHECK(transport->snapshot().source.state == cuexis::audio::PlaybackState::Empty);
}

TEST_CASE("SDL replacement preparation errors preserve active and effective snapshots",
          "[audio][sdl][replacement][errors]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 1.0F});
    REQUIRE(config.has_value());

    cuexis::audio::AudioClipStore store;
    const auto active = registerClip(store, 9600, 0.1F);
    const auto replacement = registerClip(store, 19200, 0.2F);
    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());
    REQUIRE(transport->load(active).has_value());
    REQUIRE(transport->play().has_value());
    const auto before = transport->snapshot();
    const auto effectiveBefore = transport->effectiveSettings();

    auto invalidHandle = cuexis::audio::AudioClipHandle{};
    auto invalidHandleResult = transport->prepareReplacement(invalidHandle, 0.0);
    REQUIRE_FALSE(invalidHandleResult.has_value());
    CHECK(invalidHandleResult.error().code() == "audio.store.handle_invalid");
    const auto afterInvalidHandle = transport->snapshot();
    CHECK(afterInvalidHandle.source.state == before.source.state);
    CHECK(afterInvalidHandle.source.discontinuityId == before.source.discontinuityId);
    CHECK(afterInvalidHandle.presentedFrame == before.presentedFrame);
    checkEffectiveSettingsEqual(transport->effectiveSettings(), effectiveBefore);

    auto invalidPositionResult = transport->prepareReplacement(replacement, 1000.0);
    REQUIRE_FALSE(invalidPositionResult.has_value());
    CHECK(invalidPositionResult.error().code() == "audio.transport.replacement_position_invalid");
    const auto afterInvalidPosition = transport->snapshot();
    CHECK(afterInvalidPosition.source.state == before.source.state);
    CHECK(afterInvalidPosition.source.discontinuityId == before.source.discontinuityId);
    CHECK(afterInvalidPosition.presentedFrame == before.presentedFrame);
    checkEffectiveSettingsEqual(transport->effectiveSettings(), effectiveBefore);
}

TEST_CASE("SDL replacement cancellation leaves no activatable candidate",
          "[audio][sdl][replacement][cancel]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 1.0F});
    REQUIRE(config.has_value());

    cuexis::audio::AudioClipStore store;
    const auto active = registerClip(store, 9600, 0.1F);
    const auto replacement = registerClip(store, 19200, 0.2F);
    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());
    REQUIRE(transport->load(active).has_value());
    const auto before = transport->snapshot();
    const auto effectiveBefore = transport->effectiveSettings();

    REQUIRE(transport->prepareReplacement(replacement, 50.0).has_value());
    transport->cancelReplacement();
    const auto result = transport->activateReplacement();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "audio.transport.replacement_missing");
    const auto after = transport->snapshot();
    CHECK(after.source.state == before.source.state);
    CHECK(after.source.discontinuityId == before.source.discontinuityId);
    CHECK(after.presentedFrame == before.presentedFrame);
    checkEffectiveSettingsEqual(transport->effectiveSettings(), effectiveBefore);
}

TEST_CASE("SDL empty-state errors and unload are stable", "[audio][sdl][state][unload]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 1.0F});
    REQUIRE(config.has_value());

    cuexis::audio::AudioClipStore store;
    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());
    const auto before = transport->snapshot();

    const auto play = transport->play();
    REQUIRE_FALSE(play.has_value());
    CHECK(play.error().code() == "audio.transport.play_invalid");
    const auto seek = transport->seekMs(0.0);
    REQUIRE_FALSE(seek.has_value());
    CHECK(seek.error().code() == "audio.transport.seek_invalid");
    const auto replacement = transport->prepareReplacement({}, 0.0);
    REQUIRE_FALSE(replacement.has_value());
    CHECK(replacement.error().code() == "audio.transport.replacement_invalid");
    const auto activate = transport->activateReplacement();
    REQUIRE_FALSE(activate.has_value());
    CHECK(activate.error().code() == "audio.transport.replacement_missing");

    REQUIRE(transport->unload().has_value());
    const auto after = transport->snapshot();
    CHECK(after.source.state == cuexis::audio::PlaybackState::Empty);
    CHECK(after.source.discontinuityId == before.source.discontinuityId);
    CHECK(isEmptyEffectiveSettings(transport->effectiveSettings()));
}

TEST_CASE("SDL effective settings remain a coherent published tuple across owner transitions",
          "[audio][sdl][effective][concurrency]") {
    const auto config = cuexis::audio::validateAudioConfig(
        {.targetQueueMs = 40, .refillLowWaterMs = 10, .gain = 1.0F});
    REQUIRE(config.has_value());

    cuexis::audio::AudioClipStore store;
    const auto handle = registerClip(store, 9600);
    auto subsystem = cuexis::audio_sdl::SdlAudioSubsystem::create();
    REQUIRE(subsystem.has_value());
    auto transport = cuexis::audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
    REQUIRE(transport.has_value());

    std::atomic<bool> coherent{true};
    auto reader = std::async(std::launch::async, [&] {
        for (int index = 0; index < 200000; ++index) {
            const auto settings = transport->effectiveSettings();
            if (!isEmptyEffectiveSettings(settings) && !isLoadedEffectiveSettings(settings)) {
                coherent.store(false, std::memory_order_relaxed);
                return;
            }
        }
    });

    for (int index = 0; index < 100; ++index) {
        REQUIRE(transport->load(handle).has_value());
        REQUIRE(transport->unload().has_value());
    }
    reader.get();
    CHECK(coherent.load(std::memory_order_relaxed));
}

TEST_CASE("SDL replacement activation failure enters Error without dead rollback",
          "[audio][sdl][replacement][error][characterization]") {
    const auto source = audioImplementationSource();
    const auto body = functionRegion(source, "SdlAudioTransport::activateReplacement");
    REQUIRE(body.has_value());

    CHECK(body->find("closeStream()") != std::string_view::npos);
    CHECK(body->find("openLease") != std::string_view::npos);
    CHECK(body->find("return impl_->enterError") != std::string_view::npos);
    CHECK(body->find("previousClip") == std::string_view::npos);
    CHECK(body->find("previousState") == std::string_view::npos);
}

TEST_CASE("SDL presented frame update guards invalid clamp bounds",
          "[audio][sdl][clock][overflow][characterization]") {
    const auto source = audioImplementationSource();
    const auto body = functionRegion(source, "updatePresentedFrame");
    REQUIRE(body.has_value());

    CHECK(body->find("std::clamp") == std::string_view::npos);
    CHECK(body->find("upperBound") != std::string_view::npos);
    CHECK(body->find("std::min(std::max") != std::string_view::npos);
    CHECK(body->find("numeric_limits<std::int64_t>::max()") != std::string_view::npos);
}
