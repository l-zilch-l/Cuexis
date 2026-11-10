#pragma once

// Shared host fixture for the RPA B5 case units.

#include "../../app/player/src/execution_host.hpp"
#include "../../engine/core/internal/cuexis_internal/sha256.hpp"
#include "../../engine/playback/src/gameplay_internal.hpp"
#include "../../engine/playback/src/replay_identity.hpp"
#include "../audio/fake_audio_transport.hpp"
#include "../support/environment.hpp"
#include "execution_fixture.hpp"
#include <atomic>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cuexis/judgement/recovery_codec.hpp>
#include <filesystem>
#include <future>
#include <iostream>
#include <new>
#include <span>

using namespace cuexis;
using namespace playback;
using namespace playback::detail;

namespace rpa_b5_host_fixture {
class Device final : public OwnedAudioDevice {
  public:
    explicit Device(audio::AudioClipStore& store) : transport_(store) {}
    auto transport() noexcept -> audio::IAudioTransport& override {
        return transport_;
    }
    auto isolateCallbacks() -> core::Result<bool> override {
        return transport_.snapshot().source.state != audio::PlaybackState::Playing;
    }
    auto recheckDevice() -> core::Result<void> override {
        return {};
    }

  private:
    test_support::FakeAudioTransport transport_;
};
inline auto options() -> player::ExecutionHostOptions {
    player::ExecutionHostOptions result;
    result.session = {64, 16, 128, 32, 16, 64, true, {}};
    result.audio.sessionScope = 1;
    result.audio.timeDomain = 1;
    result.audio.testOnly = true;
    result.audio.capacity = 64;
    result.audio.controlReserve = 16;
    result.audio.clipLimits = {4, 1024 * 1024, 4 * 1024 * 1024};
    result.hostCapacity = 16;
    result.captureCapacity = 64;
    result.captureReserve = 16;
    result.replyDeadline = std::chrono::seconds{5};
    result.viewport = {640, 480};
    return result;
}
inline auto ports(std::atomic<unsigned>& pumps) -> player::ExecutionHostPorts {
    return {[](audio::AudioClipStore& store) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
                return std::unique_ptr<OwnedAudioDevice>{std::make_unique<Device>(store)};
            },
            [](std::span<const std::byte>) {
                return audio::AudioClip::create(48000, 1, std::vector<float>(96000));
            },
            [&]() -> core::Result<void> {
                ++pumps;
                return {};
            }};
}
} // namespace rpa_b5_host_fixture

using namespace rpa_b5_host_fixture;
