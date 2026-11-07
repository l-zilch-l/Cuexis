#pragma once

// Shared owner/protocol fixture for the RPA B4 case units.

#include "../../app/execution_host/audio_owner.hpp"
#include "../../app/execution_host/capture_route.hpp"
#include "../../app/execution_host/frame_observation.hpp"
#include "../../app/execution_host/host_candidate.hpp"
#include "../../app/execution_host/renderer_owner.hpp"
#include "../audio/fake_audio_transport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace cuexis;
using namespace playback;
using namespace playback::detail;

namespace rpa_b4_fixture {
inline auto context() -> HostContext {
    HostContext c;
    c.sessionScope = 1;
    c.clockSegment = 1;
    c.runEpoch = 1;
    c.controlSequence = 1;
    c.resourceGeneration = 1;
    c.rendererGeneration = 1;
    return c;
}
inline auto step(Participant p, StepPhase phase, std::uint64_t id = 1) -> StepRequest {
    return {{1, id, p, phase, 0}, {context(), 1, 1, 0, {}, {}, {}, {}}};
}
inline auto control(ControlKind kind = ControlKind::resume) -> ControlRequest {
    auto c = context();
    return {c, 1, 0, kind, {c, 1, 1, 0, {}, {}, {}, {}}, {true, true, true, true}};
}
inline auto applied(const ControlRequest& r, Participant p, StepPhase phase) -> StepReceipt {
    return {{{r.context.sessionScope, r.operationId, p, phase, 0}, r.target},
            StepStatus::applied,
            {},
            {}};
}
inline auto preparedProject(PlaybackSession& session) -> PreparedPlayback {
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    auto p = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
    REQUIRE(p);
    return std::move(*p);
}
inline auto event(std::int64_t t, GameplayInputAction a = GameplayInputAction::Press)
    -> GameplayInput {
    return {{t}, 1, a, "lane.one", "domain.binding.one", "keyboard", {}};
}
class PartialLoadTransport final : public audio::IAudioTransport {
  public:
    explicit PartialLoadTransport(audio::AudioClipStore& store) : inner(store) {}
    test_support::FakeAudioTransport inner;
    unsigned stopCalls{}, pauseCalls{};
    bool rejectStop{};
    auto load(audio::AudioClipHandle handle) -> core::Result<void> override {
        auto loaded = inner.load(handle);
        if (!loaded)
            return loaded;
        auto playing = inner.play();
        if (!playing)
            return playing;
        return core::unexpected(
            core::Error{"audio.test.partial_load", "Load acted before failing"});
    }
    auto play() -> core::Result<void> override {
        return inner.play();
    }
    auto pause() -> core::Result<void> override {
        ++pauseCalls;
        return inner.pause();
    }
    auto stop() -> core::Result<void> override {
        ++stopCalls;
        if (rejectStop)
            return core::unexpected(core::Error{"audio.test.stop_failed", "Stop did not act"});
        return inner.stop();
    }
    auto seekMs(double value) -> core::Result<void> override {
        return inner.seekMs(value);
    }
    auto unload() -> core::Result<void> override {
        return inner.unload();
    }
    auto service() -> core::Result<void> override {
        return inner.service();
    }
    auto snapshot() const noexcept -> audio::AudioClockSnapshot override {
        return inner.snapshot();
    }
    auto metrics() const noexcept -> audio::AudioMetricsSnapshot override {
        return inner.metrics();
    }
    auto effectiveSettings() const noexcept -> audio::EffectiveAudioSettings override {
        return inner.effectiveSettings();
    }
};
} // namespace rpa_b4_fixture

using namespace rpa_b4_fixture;
