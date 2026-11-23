// RR-F419 / rr_frame_diagnostics: the presented-frame render duration is a measured, non-negative
// interval and it is assigned before the notePresented hook consumes it. The renderer is a neutral
// wrapper around the test presentation renderer whose submit and present take a controllable
// duration, so the positive sample is deterministic instead of depending on machine speed. No
// window, GL context, or audio device is created.

#include "player_control_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using cuexis::core::Error;
using cuexis::core::Result;
using cuexis::core::unexpected;
using cuexis::player::PlayerController;
using cuexis::player::PlayerControlPorts;
using cuexis::player::PlayerFrameLoop;
using cuexis::player::PlayerHooks;
using cuexis::player::PlayerInputAction;
using cuexis::player::PlayerLogger;
using cuexis::player::PlayerPresentedFrame;
using cuexis::presentation_renderer::IPresentationRenderer;
using cuexis::presentation_renderer::PreparedPresentation;
using cuexis::presentation_renderer::TestPresentationRenderer;

// A renderer whose submit and present each block for a fixed duration. Everything else delegates to
// the neutral test renderer, so the frame pipeline stays the product one.
class SlowRenderer final : public IPresentationRenderer {
  public:
    explicit SlowRenderer(std::chrono::microseconds duration) : duration_(duration) {}

    [[nodiscard]] auto capabilities() const noexcept
        -> const cuexis::playback::PresentationCapabilities& override {
        return inner_.capabilities();
    }
    [[nodiscard]] auto identity() const noexcept
        -> cuexis::presentation_renderer::RendererIdentity override {
        return inner_.identity();
    }
    [[nodiscard]] auto prepare(cuexis::playback::PreparedPlayback& prepared,
                               const cuexis::playback::PresentationRequest& request = {})
        -> Result<PreparedPresentation> override {
        return inner_.prepare(prepared, request);
    }
    [[nodiscard]] auto prepareOwned(const cuexis::playback::PresentationCandidateInput& input)
        -> Result<PreparedPresentation> override {
        return inner_.prepareOwned(input);
    }
    [[nodiscard]] auto accepts(const PreparedPresentation& candidate) const
        -> Result<void> override {
        return inner_.accepts(candidate);
    }
    void activate(PreparedPresentation&& candidate) noexcept override {
        inner_.activate(std::move(candidate));
    }
    void discard(PreparedPresentation&& candidate) noexcept override {
        inner_.discard(std::move(candidate));
    }
    [[nodiscard]] auto hasActivePresentation() const noexcept -> bool override {
        return inner_.hasActivePresentation();
    }
    [[nodiscard]] auto submit(const cuexis::playback::FrameSnapshot& snapshot,
                              const cuexis::render::RenderScene* debugScene = nullptr)
        -> Result<cuexis::presentation_renderer::DrawSummary> override {
        burn();
        ++submits;
        return inner_.submit(snapshot, debugScene);
    }
    [[nodiscard]] auto present() -> Result<void> override {
        burn();
        ++presents;
        return inner_.present();
    }
    [[nodiscard]] auto resize(std::uint32_t width, std::uint32_t height) -> Result<void> override {
        return inner_.resize(width, height);
    }
    [[nodiscard]] auto rebuild() -> Result<void> override {
        return inner_.rebuild();
    }
    [[nodiscard]] auto close() -> Result<void> override {
        return inner_.close();
    }

    std::size_t submits{}, presents{};

  private:
    void burn() const {
        if (duration_.count() > 0) {
            std::this_thread::sleep_for(duration_);
        }
    }
    void detachCandidate(std::uint64_t, std::uint64_t,
                         const cuexis::playback::PresentationCandidateToken&) noexcept override {}
    TestPresentationRenderer inner_;
    std::chrono::microseconds duration_;
};

// The smallest control harness the frame loop needs: one ChartClock source and no audio device.
struct DiagnosticFixture final {
    explicit DiagnosticFixture(std::chrono::microseconds duration) : renderer(duration) {
        auto created = PlayerLogger::create("cuexis_player_control_tests");
        REQUIRE(created.has_value());
        logger = std::move(*created);
        PlayerControlPorts ports;
        ports.makeSource = [] { return projectSource(chartClockProject); };
        ports.prepareClip =
            [](cuexis::playback::PreparedPlayback&,
               cuexis::audio::AudioClipStore&) -> Result<cuexis::audio::AudioClipHandle> {
            return unexpected(Error{"test.clip.unexpected", "The chart has no audio track"});
        };
        ports.openAudio = [](const std::optional<cuexis::audio::AudioClipHandle>&, double,
                             double) -> Result<std::unique_ptr<cuexis::player::PlayerAudioSeat>> {
            return std::unique_ptr<cuexis::player::PlayerAudioSeat>{};
        };
        controller.emplace(renderer, store, 1.0, std::int64_t{0}, std::move(ports), *logger);
    }

    SlowRenderer renderer;
    cuexis::audio::AudioClipStore store;
    std::unique_ptr<PlayerLogger> logger;
    std::optional<PlayerController> controller;
};

struct PresentedProbe final {
    std::vector<std::uint32_t> notedFrames;
    std::vector<double> notedMicroseconds;
    std::vector<double> validatedMicroseconds;
};

[[nodiscard]] auto runDiagnosticLoop(DiagnosticFixture& fixture, std::size_t frames)
    -> PresentedProbe {
    REQUIRE(fixture.controller.has_value());
    auto& controller = *fixture.controller;
    REQUIRE(controller
                .apply(cuexis::player::PlayerCommand{
                    .kind = cuexis::player_support::PlayerCommandKind::Load})
                .has_value());
    REQUIRE(controller
                .apply(cuexis::player::PlayerCommand{
                    .kind = cuexis::player_support::PlayerCommandKind::Play})
                .has_value());

    PresentedProbe probe;
    PlayerHooks hooks;
    hooks.notePresented = [&probe](const PlayerPresentedFrame& presented) -> Result<void> {
        probe.notedFrames.push_back(presented.renderedFrames);
        probe.notedMicroseconds.push_back(presented.renderMicroseconds);
        return {};
    };
    hooks.validatePresented = [&probe](const PlayerPresentedFrame& presented) -> Result<void> {
        probe.validatedMicroseconds.push_back(presented.renderMicroseconds);
        return {};
    };
    std::vector<std::vector<PlayerInputAction>> script(frames);
    ScriptedSurface surface{std::move(script)};
    PlayerFrameLoop loop{.controller = controller,
                         .renderer = fixture.renderer,
                         .surface = surface,
                         .smokeTest = false,
                         .audioSmokeTest = false,
                         .diagnostics = nullptr,
                         .hooks = std::move(hooks),
                         .logger = *fixture.logger};
    const auto ran = runPlayerFrameLoop(loop);
    INFO("frame loop: " << (ran ? std::string{"ok"} : std::string{ran.error().code()}));
    REQUIRE(ran.has_value());
    REQUIRE_FALSE(probe.notedMicroseconds.empty());
    return probe;
}

} // namespace

TEST_CASE("rr_frame_diagnostics", "[player][control][frame][diagnostics]") {
    SECTION("ordinary path reports a real non-negative measurement") {
        // The ordinary path only guarantees a real measurement: never a sentinel such as -1, and
        // never something larger than any plausible single frame. A zero reading is the microsecond
        // quantization of a measured interval, not a marker for "no measurement".
        DiagnosticFixture fixture{std::chrono::microseconds{0}};
        const auto probe = runDiagnosticLoop(fixture, 4);
        CHECK(probe.notedFrames.size() == probe.notedMicroseconds.size());
        for (std::size_t index = 0; index < probe.notedMicroseconds.size(); ++index) {
            const double microseconds = probe.notedMicroseconds[index];
            INFO("presented frame " << probe.notedFrames[index] << " duration_us " << microseconds);
            CHECK(std::isfinite(microseconds));
            CHECK(microseconds >= 0.0);
            CHECK(microseconds < 500000.0);
        }
        CHECK(probe.notedMicroseconds == probe.validatedMicroseconds);
    }

    SECTION("render duration is assigned before notePresented consumes it") {
        // Each submitted frame takes two fixed sleeps, so the first presented frame must report a
        // duration at least as large as one of them. A hook that observed the field before the loop
        // assigned it would see the default zero, which is exactly the F419 defect.
        constexpr auto sleepDuration = std::chrono::milliseconds{2};
        DiagnosticFixture fixture{sleepDuration};
        const auto probe = runDiagnosticLoop(fixture, 3);
        REQUIRE(probe.notedMicroseconds.size() >= 2);
        REQUIRE(probe.validatedMicroseconds.size() == probe.notedMicroseconds.size());
        const double floorMicroseconds = 1000.0; // one 2 ms sleep, with room for scheduler slack
        for (std::size_t index = 0; index < probe.notedMicroseconds.size(); ++index) {
            INFO("noted frame " << index << " duration_us " << probe.notedMicroseconds[index]);
            CHECK(probe.notedMicroseconds[index] >= floorMicroseconds);
            CHECK(probe.notedMicroseconds[index] == probe.validatedMicroseconds[index]);
        }
        CHECK(probe.notedFrames.front() == 0);
        CHECK(fixture.renderer.submits == probe.notedMicroseconds.size());
        CHECK(fixture.renderer.presents == probe.notedMicroseconds.size());
    }
}
