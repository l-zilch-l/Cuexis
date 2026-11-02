#pragma once

// Shared fixture for the player control case units.

#include "player_control.hpp"
#include "player_log.hpp"

#include "fake_audio_transport.hpp"

#include <cuexis/playback/playback_source.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace player_control_fixture {

using cuexis::audio::AudioClip;
using cuexis::audio::AudioClipHandle;
using cuexis::audio::AudioClipStore;
using cuexis::core::Error;
using cuexis::core::Result;
using cuexis::core::unexpected;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSource;
using cuexis::player::PlayerAudioSeat;
using cuexis::player::PlayerCommand;
using cuexis::player::playerCommandForInput;
using cuexis::player::PlayerController;
using cuexis::player::PlayerControlPorts;
using cuexis::player::PlayerInputAction;
using cuexis::player::playerInputActionName;
using cuexis::player::PlayerLogger;
using cuexis::player::playerSeekStepMs;
using cuexis::player_support::PlaybackClockMode;
using cuexis::player_support::PlayerAppState;
using cuexis::player_support::PlayerCommandKind;
using cuexis::player_support::PlayerReloadPolicy;
using cuexis::presentation_renderer::TestPresentationRenderer;
using cuexis::test_support::FakeAudioTransport;

constexpr std::string_view chartClockProject = "stage3_project";
constexpr std::string_view otherChartClockProject = "demo20s";
constexpr std::string_view audioProject = "stage1d_project";

// A structurally valid Chart v4 document whose portable resources do not exist. Content
// preparation rejects it; the command table never sees the difference.
constexpr std::string_view missingResourceChart = R"json(
{
  "format":"cuexis.chart","version":4,
  "chartId":"019f0000-0000-7abc-8def-0000000009a1","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000},
  "parameters":[],"templates":[],"behaviors":[],
  "animationTemplateImports":[],"animationClips":[],
  "objects":[{
    "id":"019f0000-0000-7abc-8def-0000000009a2","name":"missing_resource",
    "parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,0],
                           "rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.renderable":{"version":1,
                            "mesh":{"domain":"asset","id":"mesh.missing"},
                            "material":{"domain":"asset","id":"material.missing"}}
    },
    "extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

// A structurally valid Chart v4 document with no object, no resource, and no audio track. Loading
// it is a successful transaction whose prepared content is empty.
constexpr std::string_view objectlessChart = R"json(
{
  "format":"cuexis.chart","version":4,
  "chartId":"019f0000-0000-7abc-8def-0000000009b1","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000},
  "parameters":[],"templates":[],"behaviors":[],
  "animationTemplateImports":[],"animationClips":[],
  "objects":[],"requiredExtensions":[],"extensions":{}
}
)json";

inline [[nodiscard]] auto projectSource(std::string_view project) -> Result<PlaybackSource> {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / project;
    return PlaybackSource::fromFilesystemProject(root);
}

// One second of silence. The tests never decode the chart's main music; the clip preparation port
// is what a real decoder would sit behind.
inline [[nodiscard]] auto silentClip() -> Result<AudioClip> {
    constexpr std::uint32_t sampleRate = 48000;
    constexpr std::uint32_t channels = 2;
    std::vector<float> samples(static_cast<std::size_t>(sampleRate) * channels, 0.0F);
    return AudioClip::create(sampleRate, channels, std::move(samples));
}

inline [[nodiscard]] auto contextValue(const Error& error, std::string_view key) -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

struct AudioProbe final {
    std::size_t openCalls{};
    std::size_t unloadCalls{};
    std::size_t prepareReplacementCalls{};
    std::size_t activateReplacementCalls{};
    double lastStagedPositionMs{-1.0};
    double lastStartPositionMs{-1.0};
    double lastGain{0.0};
    bool failOpen{false};
    bool failPrepareReplacement{false};
    bool failActivateReplacement{false};
    FakeAudioTransport* transport{};
};

// The backend-neutral seat the assembly layer normally implements over SDL. Every operation the
// controller can call is counted and can be made to fail, so a staged device failure is
// reproducible without hardware.
class FakeSeat final : public PlayerAudioSeat {
  public:
    FakeSeat(AudioClipStore& store, AudioProbe& probe) : transport_{store}, probe_{probe} {
        probe_.transport = &transport_;
    }

    [[nodiscard]] auto transport() -> cuexis::audio::IAudioTransport& override {
        return transport_;
    }

    [[nodiscard]] auto fake() -> FakeAudioTransport& {
        return transport_;
    }

    [[nodiscard]] auto recheckBoundDevice() -> Result<void> override {
        return {};
    }

    [[nodiscard]] auto prepareReplacement(AudioClipHandle handle, double positionMs)
        -> Result<void> override {
        ++probe_.prepareReplacementCalls;
        probe_.lastStagedPositionMs = positionMs;
        if (probe_.failPrepareReplacement) {
            return unexpected(Error{"test.audio.replacement_prepare_failed",
                                    "Injected replacement preparation failure"});
        }
        return transport_.prepareReplacement(handle, positionMs);
    }

    [[nodiscard]] auto activateReplacement() -> Result<void> override {
        ++probe_.activateReplacementCalls;
        if (probe_.failActivateReplacement) {
            return unexpected(Error{"test.audio.replacement_activate_failed",
                                    "Injected replacement activation failure"});
        }
        return transport_.activateReplacement();
    }

    [[nodiscard]] auto applyGain(float requestedGain) -> Result<void> override {
        probe_.lastGain = static_cast<double>(requestedGain);
        return {};
    }

    [[nodiscard]] auto unload() -> Result<void> override {
        ++probe_.unloadCalls;
        return transport_.unload();
    }

  private:
    FakeAudioTransport transport_;
    AudioProbe& probe_;
};

// Everything a staged failure must leave untouched. `activeResourceCount` is the renderer's
// prepared resource cache for the active presentation, which is the closest no-GPU observable to
// the device-side GPU cache.
struct BundleSnapshot final {
    std::string chartId;
    std::uint32_t chartFormatVersion{};
    double timingOffsetMs{};
    PlaybackMode mode{PlaybackMode::ChartClock};
    std::optional<std::string> mainMusicAssetId;
    std::array<std::uint8_t, 32> configIdentity{};
    PlayerAppState state{PlayerAppState::Empty};
    std::optional<AudioClipHandle> audioHandle;
    std::size_t activeResourceCount{};
    bool presentationActive{false};
    double chartTimeMs{};
};

// A control surface that reports scripted actions, one list per poll, and then asks to quit. It
// lets the real frame loop run without a window, so the discontinuity contract between the
// controller, the timeline, and PlaybackSession is covered by a GPU-free test.
class ScriptedSurface final : public cuexis::player::PlayerSurface {
  public:
    explicit ScriptedSurface(std::vector<std::vector<PlayerInputAction>> script)
        : script_(std::move(script)) {}

    [[nodiscard]] auto pollInput() -> Result<cuexis::player::PlayerInput> override {
        cuexis::player::PlayerInput input;
        if (pollCount_ >= script_.size()) {
            input.quitRequested = true;
            return input;
        }
        input.actions = script_[pollCount_++];
        return input;
    }

    [[nodiscard]] auto drawableSize() -> Result<cuexis::player::PlayerDrawableSize> override {
        return cuexis::player::PlayerDrawableSize{.width = 320, .height = 180};
    }

  private:
    std::vector<std::vector<PlayerInputAction>> script_;
    std::size_t pollCount_{};
};

class Fixture final {
  public:
    Fixture() {
        auto created = PlayerLogger::create("cuexis_player_control_tests");
        REQUIRE(created.has_value());
        logger = std::move(*created);

        PlayerControlPorts ports;
        ports.makeSource = [this]() -> Result<PlaybackSource> {
            if (onMakeSource) {
                onMakeSource();
            }
            if (!explicitChart.empty()) {
                return PlaybackSource::fromChartText(explicitChart);
            }
            return projectSource(sourceProject);
        };
        ports.prepareClip = [this](cuexis::playback::PreparedPlayback& prepared,
                                   AudioClipStore& target) -> Result<AudioClipHandle> {
            ++clipPreparationCalls;
            if (onPrepareClip) {
                onPrepareClip();
            }
            if (failClipPreparation) {
                return unexpected(
                    Error{"test.clip.decode_failed", "Injected decoded-clip preparation failure"});
            }
            if (stealPreparedCandidate) {
                // Staged commit failure: consume the candidate the session is about to commit, so
                // PlaybackSession::commit rejects it before it can touch the active bundle.
                prepared = cuexis::playback::PreparedPlayback{};
            }
            auto clip = silentClip();
            if (!clip) {
                return unexpected(std::move(clip.error()));
            }
            return target.registerClip(std::move(*clip));
        };
        ports.openAudio =
            [this](const std::optional<AudioClipHandle>& clip, double requestedGain,
                   double startPositionMs) -> Result<std::unique_ptr<PlayerAudioSeat>> {
            ++audio.openCalls;
            audio.lastGain = requestedGain;
            audio.lastStartPositionMs = startPositionMs;
            if (audio.failOpen) {
                return unexpected(
                    Error{"test.audio.device_open_failed", "Injected audio device open failure"});
            }
            if (!clip.has_value()) {
                return std::unique_ptr<PlayerAudioSeat>{};
            }
            std::unique_ptr<PlayerAudioSeat> seat = std::make_unique<FakeSeat>(store, audio);
            if (auto loaded = audio.transport->load(*clip); !loaded) {
                return unexpected(std::move(loaded.error()));
            }
            if (startPositionMs > 0.0) {
                if (auto sought = audio.transport->seekMs(startPositionMs); !sought) {
                    return unexpected(std::move(sought.error()));
                }
            }
            return seat;
        };

        controller.emplace(renderer, store, gain, profileCorrectionUs, std::move(ports), *logger);
    }

    Fixture(const Fixture&) = delete;
    auto operator=(const Fixture&) -> Fixture& = delete;
    ~Fixture() = default;

    [[nodiscard]] auto apply(PlayerCommand command) -> Result<void> {
        return controller->apply(std::move(command));
    }

    [[nodiscard]] auto load(std::string_view project) -> Result<void> {
        sourceProject = std::string{project};
        explicitChart.clear();
        return apply(PlayerCommand{.kind = PlayerCommandKind::Load});
    }

    // Loads content that carries a main music track. The clock is named explicitly because the
    // controller refuses to guess a mode from a failed default load.
    [[nodiscard]] auto loadAudio(std::string_view project) -> Result<void> {
        sourceProject = std::string{project};
        explicitChart.clear();
        return apply(
            PlayerCommand{.kind = PlayerCommandKind::Load, .mode = PlaybackMode::CuexisAudio});
    }

    [[nodiscard]] auto play() -> Result<void> {
        return apply(PlayerCommand{.kind = PlayerCommandKind::Play});
    }

    [[nodiscard]] auto pause() -> Result<void> {
        return apply(PlayerCommand{.kind = PlayerCommandKind::Pause});
    }

    [[nodiscard]] auto stop() -> Result<void> {
        return apply(PlayerCommand{.kind = PlayerCommandKind::Stop});
    }

    [[nodiscard]] auto seek(double targetMs) -> Result<void> {
        return apply(PlayerCommand{.kind = PlayerCommandKind::Seek, .seekTargetMs = targetMs});
    }

    [[nodiscard]] auto reload(PlayerReloadPolicy policy = PlayerReloadPolicy::KeepChartTime)
        -> Result<void> {
        return apply(PlayerCommand{.kind = PlayerCommandKind::Reload, .reloadPolicy = policy});
    }

    [[nodiscard]] auto rebuild() -> Result<void> {
        return apply(PlayerCommand{.kind = PlayerCommandKind::Rebuild});
    }

    TestPresentationRenderer renderer;
    AudioClipStore store;
    AudioProbe audio;
    std::unique_ptr<PlayerLogger> logger;
    double gain{1.0};
    std::int64_t profileCorrectionUs{0};

    std::string sourceProject{std::string{chartClockProject}};
    std::string explicitChart;
    bool failClipPreparation{false};
    bool stealPreparedCandidate{false};
    std::size_t clipPreparationCalls{};
    std::function<void()> onMakeSource;
    std::function<void()> onPrepareClip;
    std::optional<PlayerCommand> reentrantCommand;
    std::optional<Error> reentrantError;

    std::optional<PlayerController> controller;
};

inline [[nodiscard]] auto snapshotBundle(Fixture& fixture) -> BundleSnapshot {
    REQUIRE(fixture.controller.has_value());
    BundleSnapshot bundle;
    const auto info = fixture.controller->session().contentInfo();
    REQUIRE(info.has_value());
    bundle.chartId = info->chartId;
    bundle.chartFormatVersion = info->chartFormatVersion;
    bundle.timingOffsetMs = info->timingOffsetMs;
    bundle.mode = info->mode;
    bundle.mainMusicAssetId = info->mainMusicAssetId;
    bundle.configIdentity =
        cuexis::player_support::resolvedSessionConfigIdentity(fixture.controller->sessionConfig());
    bundle.state = fixture.controller->state();
    bundle.audioHandle = fixture.controller->activeAudioHandle();
    bundle.presentationActive = fixture.renderer.status().active;
    bundle.activeResourceCount = fixture.renderer.status().activeResourceCount;
    bundle.chartTimeMs = fixture.controller->lastRuntimeFrame().chartTimeMs;
    return bundle;
}

// Every staged failure must leave the previously active bundle exactly as it was. Only the
// application state may differ, because a device-stage failure publishes Failed.
inline void checkBundleUnchanged(Fixture& fixture, const BundleSnapshot& before,
                                 std::optional<PlayerAppState> expectedState = std::nullopt) {
    const auto after = snapshotBundle(fixture);
    CHECK(after.chartId == before.chartId);
    CHECK(after.chartFormatVersion == before.chartFormatVersion);
    CHECK(after.timingOffsetMs == Catch::Approx(before.timingOffsetMs));
    CHECK(after.mode == before.mode);
    CHECK(after.mainMusicAssetId == before.mainMusicAssetId);
    CHECK(after.configIdentity == before.configIdentity);
    CHECK(after.state == (expectedState.has_value() ? *expectedState : before.state));
    CHECK(after.audioHandle == before.audioHandle);
    CHECK(after.presentationActive == before.presentationActive);
    CHECK(after.activeResourceCount == before.activeResourceCount);
    CHECK(after.chartTimeMs == Catch::Approx(before.chartTimeMs));
}

} // namespace player_control_fixture

using namespace player_control_fixture;
