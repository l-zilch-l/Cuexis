// Application command-layer tests for the reference Player.
//
// These tests never create a GPU context, a window, or a real audio device. The controller drives
// a TestPresentationRenderer, a synthetic clip store, and a fake audio seat, so the command table,
// the fixed transaction order, and every staged failure are exercised on the owner thread alone.
// The GPU smoke test in `cuexis_player` covers what only a device can prove.

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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

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

[[nodiscard]] auto projectSource(std::string_view project) -> Result<PlaybackSource> {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / project;
    return PlaybackSource::fromFilesystemProject(root);
}

// One second of silence. The tests never decode the chart's main music; the clip preparation port
// is what a real decoder would sit behind.
[[nodiscard]] auto silentClip() -> Result<AudioClip> {
    constexpr std::uint32_t sampleRate = 48000;
    constexpr std::uint32_t channels = 2;
    std::vector<float> samples(static_cast<std::size_t>(sampleRate) * channels, 0.0F);
    return AudioClip::create(sampleRate, channels, std::move(samples));
}

[[nodiscard]] auto contextValue(const Error& error, std::string_view key) -> std::string {
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

[[nodiscard]] auto snapshotBundle(Fixture& fixture) -> BundleSnapshot {
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
void checkBundleUnchanged(Fixture& fixture, const BundleSnapshot& before,
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

TEST_CASE("PlayerController loads ChartClock content and publishes one bundle",
          "[player][control][command]") {
    Fixture fixture;

    REQUIRE(fixture.load(chartClockProject).has_value());

    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    CHECK(fixture.controller->sessionConfig().clockMode == PlaybackClockMode::ChartClock);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());
    CHECK_FALSE(fixture.controller->lastRuntimeFrame().chartTimeMs > 0.0);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.renderer.status().activeResourceCount > 0);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);
    CHECK(fixture.audio.openCalls == 0);

    // A transaction publishes the replaced bundle exactly once.
    CHECK(fixture.controller->consumeBundleReplaced());
    CHECK_FALSE(fixture.controller->consumeBundleReplaced());

    const auto bundle = snapshotBundle(fixture);
    CHECK(bundle.mode == PlaybackMode::ChartClock);
    CHECK_FALSE(bundle.mainMusicAssetId.has_value());
    CHECK(bundle.chartFormatVersion >= 1);
}

TEST_CASE("PlayerController rejects transport commands before content is loaded",
          "[player][control][command]") {
    Fixture fixture;

    CHECK(fixture.controller->state() == PlayerAppState::Empty);

    // The command table owns the decision, so a transport command without content is rejected
    // before any target validation runs.
    for (const auto kind :
         {PlayerCommandKind::Play, PlayerCommandKind::Pause, PlayerCommandKind::Stop,
          PlayerCommandKind::Seek, PlayerCommandKind::Reload}) {
        const auto rejected = fixture.apply(PlayerCommand{.kind = kind});
        REQUIRE_FALSE(rejected.has_value());
        INFO("command: " << cuexis::player_support::playerCommandName(kind));
        CHECK(rejected.error().code() == "player.command.not_loaded");
    }
    const auto negativeBeforeLoad = fixture.seek(-1.0);
    REQUIRE_FALSE(negativeBeforeLoad.has_value());
    CHECK(negativeBeforeLoad.error().code() == "player.command.not_loaded");

    // Rebuild without content and without a failure has nothing to recreate.
    const auto rebuild = fixture.rebuild();
    REQUIRE_FALSE(rebuild.has_value());
    CHECK(rebuild.error().code() == "player.command.not_loaded");
    CHECK(fixture.controller->state() == PlayerAppState::Empty);
    CHECK_FALSE(fixture.renderer.status().active);

    REQUIRE(fixture.load(chartClockProject).has_value());

    // Seek target validation is what rejects an unusable target once content is loaded.
    const auto negative = fixture.seek(-1.0);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "player.command.seek_negative");

    const auto notFinite = fixture.seek(std::nan(""));
    REQUIRE_FALSE(notFinite.has_value());
    CHECK(notFinite.error().code() == "player.command.seek_not_finite");

    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
}

TEST_CASE("PlayerController rejects a re-entrant command while a transaction owns the application",
          "[player][control][command]") {
    Fixture fixture;

    // The source port runs inside the load transaction, which is exactly the window in which a
    // second command must not be allowed to interleave.
    fixture.onMakeSource = [&fixture]() {
        if (fixture.reentrantCommand.has_value()) {
            const auto nested = fixture.controller->apply(std::move(*fixture.reentrantCommand));
            fixture.reentrantCommand.reset();
            if (!nested) {
                fixture.reentrantError = nested.error();
            }
        }
    };
    fixture.reentrantCommand.emplace(PlayerCommand{.kind = PlayerCommandKind::Load});

    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.reentrantError.has_value());
    CHECK(fixture.reentrantError->code() == "player.command.transaction_in_progress");
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.renderer.status().active);
}

TEST_CASE("PlayerController reports a repeated command as a duplicate rejection",
          "[player][control][command]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());

    const auto again = fixture.play();
    REQUIRE_FALSE(again.has_value());
    CHECK(again.error().code() == "player.command.duplicate");
    CHECK(contextValue(again.error(), "duplicate") == "true");
    CHECK(fixture.controller->state() == PlayerAppState::Playing);

    REQUIRE(fixture.pause().has_value());
    const auto pausedAgain = fixture.pause();
    REQUIRE_FALSE(pausedAgain.has_value());
    CHECK(pausedAgain.error().code() == "player.command.duplicate");
    CHECK(contextValue(pausedAgain.error(), "duplicate") == "true");

    // A duplicate that is not a state repetition reports the real transition error instead.
    const auto notPlaying = fixture.seek(250.0);
    REQUIRE(notPlaying.has_value());
    const auto stopped = fixture.stop();
    REQUIRE(stopped.has_value());
    const auto stoppedAgain = fixture.stop();
    REQUIRE_FALSE(stoppedAgain.has_value());
    CHECK(stoppedAgain.error().code() == "player.command.duplicate");
    const auto pausedFromStopped = fixture.pause();
    REQUIRE_FALSE(pausedFromStopped.has_value());
    CHECK(pausedFromStopped.error().code() == "player.command.not_playing");
    CHECK(contextValue(pausedFromStopped.error(), "duplicate") == "false");
}

TEST_CASE("PlayerController keeps the active bundle when content preparation fails",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 4200.0, .simulationDeltaTimeMs = 16.0});
    const auto before = snapshotBundle(fixture);

    auto source = PlaybackSource::fromChartText(std::string{missingResourceChart});
    REQUIRE(source.has_value());
    PlayerCommand command{.kind = PlayerCommandKind::Reload};
    command.source = std::move(*source);

    const auto rejected = fixture.apply(std::move(command));
    REQUIRE_FALSE(rejected.has_value());
    INFO("content preparation rejection: " << rejected.error().code());
    CHECK(rejected.error().code() != "player.command.not_loaded");
    CHECK_FALSE(rejected.error().code() == "player.command.transaction_in_progress");

    checkBundleUnchanged(fixture, before);
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
}

TEST_CASE("PlayerController keeps the active bundle when the renderer candidate fails",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 900.0, .simulationDeltaTimeMs = 16.0});
    const auto before = snapshotBundle(fixture);

    // A lost surface fails the renderer candidate before anything is activated.
    fixture.renderer.markDeviceLost();
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "presentation.renderer.surface.lost");
    CHECK(contextValue(rejected.error(), "operation") == "prepare_presentation");

    checkBundleUnchanged(fixture, before);
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);

    // Rebuild is the documented recovery for a lost device.
    REQUIRE(fixture.rebuild().has_value());
    CHECK_FALSE(fixture.renderer.status().deviceLost);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.renderer.status().activeResourceCount == before.activeResourceCount);
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
}

TEST_CASE("PlayerController rejects a candidate that went stale before the precheck",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    const auto before = snapshotBundle(fixture);

    // A renderer generation change between candidate preparation and the precheck makes the
    // candidate stale. The precheck must reject it instead of activating a token the renderer no
    // longer knows. The clip port is the only application port that runs inside that window.
    bool rebuiltOnce = false;
    fixture.onPrepareClip = [&fixture, &rebuiltOnce]() {
        if (!rebuiltOnce) {
            rebuiltOnce = true;
            REQUIRE(fixture.renderer.rebuild().has_value());
        }
    };

    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rebuiltOnce);
    CHECK(rejected.error().code() == "presentation.renderer.token.stale");
    CHECK(contextValue(rejected.error(), "stage") == "precheck");

    // The bundle, its identity, and its audio handle are untouched, nothing reached the device,
    // and no candidate leaks.
    const auto after = snapshotBundle(fixture);
    CHECK(after.chartId == before.chartId);
    CHECK(after.configIdentity == before.configIdentity);
    CHECK(after.audioHandle == before.audioHandle);
    CHECK(after.state == before.state);
    CHECK(fixture.audio.activateReplacementCalls == 0);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);

    // Rebuild republishes a complete bundle after the injected generation change.
    fixture.onPrepareClip = {};
    REQUIRE(fixture.rebuild().has_value());
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.renderer.status().activeResourceCount == before.activeResourceCount);
    CHECK(fixture.audio.openCalls == 2);
}

TEST_CASE("PlayerController enters Failed when audio activation fails and keeps the old bundle",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    REQUIRE(fixture.play().has_value());
    REQUIRE(fixture.audio.openCalls == 1);
    CHECK(fixture.audio.lastGain == Catch::Approx(fixture.gain));
    CHECK(fixture.audio.lastStartPositionMs == Catch::Approx(0.0));

    const auto before = snapshotBundle(fixture);
    REQUIRE(before.audioHandle.has_value());

    // The device is already open, so a reload stages a replacement stream on it.
    fixture.audio.failActivateReplacement = true;
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "test.audio.replacement_activate_failed");
    CHECK(contextValue(rejected.error(), "stage") == "audio_activate");
    CHECK(fixture.audio.prepareReplacementCalls == 1);
    CHECK(fixture.audio.activateReplacementCalls == 1);

    // A physical device failure is the one step the transaction cannot roll back: the application
    // reports Failed and keeps the published bundle, identity, and handles.
    checkBundleUnchanged(fixture, before, PlayerAppState::Failed);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK(fixture.audio.unloadCalls == 0);
    CHECK(fixture.renderer.status().active);
}

TEST_CASE("PlayerController enters Failed when the commit fails and keeps the published handles",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    const auto before = snapshotBundle(fixture);
    REQUIRE(before.audioHandle.has_value());

    // The clip port consumes the candidate between preparation and the commit, which is the only
    // application port that runs inside that window.
    fixture.stealPreparedCandidate = true;
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "playback.prepared.invalid");
    CHECK(contextValue(rejected.error(), "stage") == "commit");

    checkBundleUnchanged(fixture, before, PlayerAppState::Failed);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.clipPreparationCalls == 2);
    // The rejected clip was discarded; the active clip and the seat survive.
    CHECK(fixture.store.metrics().registeredClips == 1);
}

TEST_CASE("PlayerController enters Failed when the audio device cannot be opened",
          "[player][control][command][failure]") {
    Fixture fixture;
    fixture.audio.failOpen = true;

    const auto rejected = fixture.load(audioProject);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "test.audio.device_open_failed");
    CHECK(contextValue(rejected.error(), "stage") == "audio_activate");
    CHECK(fixture.controller->state() == PlayerAppState::Failed);
    CHECK(fixture.audio.openCalls == 1);
    // Nothing was published: no presentation, no clip, and no device.
    CHECK_FALSE(fixture.renderer.status().active);
    CHECK(fixture.store.metrics().registeredClips == 0);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());

    // Rebuild is still the recovery from Failed, even when the failure happened before the first
    // bundle existed.
    fixture.audio.failOpen = false;
    REQUIRE(fixture.rebuild().has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK(fixture.store.metrics().registeredClips == 1);
}

TEST_CASE("PlayerController leaves Failed only through Load or Rebuild",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.audio.failActivateReplacement = true;
    REQUIRE_FALSE(fixture.reload().has_value());
    REQUIRE(fixture.controller->state() == PlayerAppState::Failed);

    for (const auto kind :
         {PlayerCommandKind::Play, PlayerCommandKind::Pause, PlayerCommandKind::Stop,
          PlayerCommandKind::Seek, PlayerCommandKind::Reload}) {
        const auto rejected = fixture.apply(PlayerCommand{.kind = kind});
        REQUIRE_FALSE(rejected.has_value());
        INFO("command: " << cuexis::player_support::playerCommandName(kind));
        CHECK(rejected.error().code() == "player.command.failed");
    }
    CHECK(fixture.controller->state() == PlayerAppState::Failed);

    // Rebuild repairs the renderer and the device path, and publishes the state the table gives
    // the recovery: Loaded, so the caller resumes with an explicit Play.
    fixture.audio.failActivateReplacement = false;
    REQUIRE(fixture.rebuild().has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.audio.unloadCalls == 1);
    CHECK(fixture.audio.openCalls == 2);
    CHECK(fixture.audio.prepareReplacementCalls == 1);
    CHECK(fixture.audio.activateReplacementCalls == 1);
    CHECK(fixture.store.metrics().registeredClips == 1);
    REQUIRE(fixture.play().has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Playing);

    // Load is the other accepted exit.
    fixture.audio.failActivateReplacement = true;
    REQUIRE_FALSE(fixture.reload().has_value());
    REQUIRE(fixture.controller->state() == PlayerAppState::Failed);
    REQUIRE(fixture.load(chartClockProject).has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    // Loading content without an audio track closes the previous seat through the bundle swap.
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());
    CHECK(fixture.store.metrics().registeredClips == 0);
}

TEST_CASE("PlayerController ChartClock transport commands publish one discontinuity each",
          "[player][control][command][clock]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());

    const auto start = fixture.controller->chartClock().sample(0.0);
    REQUIRE(start.has_value());
    const auto startDiscontinuity = start->discontinuityId;

    REQUIRE(fixture.play().has_value());
    auto target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(0.0));
    CHECK_FALSE(fixture.controller->consumeChartSeekTarget().has_value());

    // Pause freezes the last presented chart time; Play resumes from it.
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 1750.0, .simulationDeltaTimeMs = 16.0});
    REQUIRE(fixture.pause().has_value());
    CHECK_FALSE(fixture.controller->consumeChartSeekTarget().has_value());
    REQUIRE(fixture.play().has_value());
    target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(1750.0));

    // Stop rewinds a ChartClock session to chart time zero.
    REQUIRE(fixture.stop().has_value());
    target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(0.0));

    // Seeking while stopped is accepted and keeps the stopped state.
    REQUIRE(fixture.seek(2500.0).has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Stopped);
    target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(2500.0));

    const auto end = fixture.controller->chartClock().sample(0.0);
    REQUIRE(end.has_value());
    CHECK(end->discontinuityId > startDiscontinuity);
}

TEST_CASE("PlayerController preserves the chart time across consecutive reloads",
          "[player][control][command][reload]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 3000.0, .simulationDeltaTimeMs = 16.0});

    const auto before = snapshotBundle(fixture);
    for (int attempt = 0; attempt < 3; ++attempt) {
        fixture.sourceProject = (attempt % 2 == 0) ? std::string{otherChartClockProject}
                                                   : std::string{chartClockProject};
        const auto reloaded = fixture.reload();
        INFO("reload attempt " << attempt << " of " << fixture.sourceProject << " -> "
                               << (reloaded ? std::string{"ok"}
                                            : std::string{reloaded.error().code()}));
        REQUIRE(reloaded.has_value());
        CHECK(fixture.controller->state() == PlayerAppState::Playing);
        CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
        CHECK(fixture.controller->consumeBundleReplaced());
        CHECK_FALSE(fixture.controller->consumeBundleReplaced());
        // KeepChartTime resumes where the previous bundle stopped.
        auto target = fixture.controller->consumeChartSeekTarget();
        REQUIRE(target.has_value());
        CHECK(*target == Catch::Approx(before.chartTimeMs));
        fixture.controller->noteRuntimeFrame(
            cuexis::playback::RuntimeFrame{.chartTimeMs = 3000.0, .simulationDeltaTimeMs = 16.0});
    }

    // RestartAtZero is the explicit exception: it rewinds the chart clock instead.
    const auto restarted = fixture.reload(PlayerReloadPolicy::RestartAtZero);
    REQUIRE(restarted.has_value());
    const auto target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(0.0));
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
    CHECK(fixture.renderer.status().active);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);
}

TEST_CASE("PlayerController reload never switches the active PlaybackMode",
          "[player][control][command][reload]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    const auto before = snapshotBundle(fixture);

    // Content without an audio track cannot be reloaded into an audio-clock session.
    fixture.sourceProject = std::string{chartClockProject};
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.mode_change_requires_load");
    CHECK(contextValue(rejected.error(), "command") == "reload");
    checkBundleUnchanged(fixture, before);

    // Load is the explicit switch, and it closes the previous audio seat.
    REQUIRE(fixture.load(chartClockProject).has_value());
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK(fixture.store.metrics().registeredClips == 0);
}

TEST_CASE("PlayerController seeks an audio session in the source domain",
          "[player][control][command][audio]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    REQUIRE(fixture.audio.transport != nullptr);

    // The clip is one second long, so a one-second seek is inside the playable range.
    REQUIRE(fixture.seek(500.0).has_value());
    CHECK(fixture.audio.transport->snapshot().source.positionMs == Catch::Approx(500.0));

    const auto outside = fixture.seek(2500.0);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().code() == "audio.transport.seek_range");
    CHECK(contextValue(outside.error(), "command") == "seek");
    // The rejected seek did not move the transport.
    CHECK(fixture.audio.transport->snapshot().source.positionMs == Catch::Approx(500.0));

    const auto negative = fixture.seek(-10.0);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "player.command.seek_negative");
}

TEST_CASE("PlayerController folds device observations into the application state",
          "[player][control][command][audio]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.play().has_value());

    // End of stream stops a playing application without losing its position.
    fixture.controller->observeTransport(cuexis::audio::SourceClockSample{
        .positionMs = 1000.0, .state = cuexis::audio::PlaybackState::Ended, .discontinuityId = 2});
    CHECK(fixture.controller->state() == PlayerAppState::Stopped);

    // A stopped application keeps its state while the device keeps reporting the end.
    fixture.controller->observeTransport(cuexis::audio::SourceClockSample{
        .positionMs = 1000.0, .state = cuexis::audio::PlaybackState::Ended, .discontinuityId = 2});
    CHECK(fixture.controller->state() == PlayerAppState::Stopped);

    // A device error is the observation that fails the application.
    fixture.controller->observeTransport(cuexis::audio::SourceClockSample{
        .positionMs = 1000.0, .state = cuexis::audio::PlaybackState::Error, .discontinuityId = 3});
    CHECK(fixture.controller->state() == PlayerAppState::Failed);

    const auto rejected = fixture.play();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.failed");
}

TEST_CASE("PlayerController loads content with no audio track and no object",
          "[player][control][command]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    CHECK(fixture.store.metrics().registeredClips == 1);

    // Switching to content without an audio track is an explicit Load, and it drops the device.
    fixture.sourceProject.clear();
    fixture.explicitChart = std::string{objectlessChart};
    const auto loaded = fixture.apply(PlayerCommand{.kind = PlayerCommandKind::Load});
    INFO("objectless load: " << (loaded ? std::string{"ok"} : std::string{loaded.error().code()}));
    REQUIRE(loaded.has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK(fixture.audio.unloadCalls == 1);
    CHECK(fixture.store.metrics().registeredClips == 0);
    CHECK(fixture.renderer.status().active);

    const auto info = fixture.controller->session().chartInfo();
    REQUIRE(info.has_value());
    CHECK(info->objectCount == 0);

    // A frame over content with no object is still a valid frame.
    fixture.controller->noteRuntimeFrame(cuexis::playback::RuntimeFrame{.chartTimeMs = 0.0});
    REQUIRE(fixture.controller->session().update({.chartTimeMs = 0.0}).has_value());
    const auto snapshot = fixture.controller->session().extractFrame({.width = 64, .height = 64});
    REQUIRE(snapshot.has_value());
    CHECK(snapshot->objects.empty());
}

TEST_CASE("PlayerController translates user input into the shared command layer",
          "[player][control][command][input]") {
    // PlayPause is the only state-dependent action: it pauses what plays and plays everything else.
    CHECK(playerCommandForInput(PlayerInputAction::PlayPause, PlayerAppState::Playing, 0.0).kind ==
          PlayerCommandKind::Pause);
    for (const auto state : {PlayerAppState::Empty, PlayerAppState::Loaded, PlayerAppState::Paused,
                             PlayerAppState::Stopped, PlayerAppState::Failed}) {
        CHECK(playerCommandForInput(PlayerInputAction::PlayPause, state, 0.0).kind ==
              PlayerCommandKind::Play);
    }

    // Relative seek clamps at zero, because a negative chart time is never a legal target, and
    // never clamps above, because the transport owns the upper bound.
    const auto backward =
        playerCommandForInput(PlayerInputAction::SeekBackward, PlayerAppState::Playing, 3000.0);
    CHECK(backward.kind == PlayerCommandKind::Seek);
    CHECK(backward.seekTargetMs == Catch::Approx(0.0));
    const auto backwardInside =
        playerCommandForInput(PlayerInputAction::SeekBackward, PlayerAppState::Playing, 12000.0);
    CHECK(backwardInside.seekTargetMs == Catch::Approx(12000.0 - playerSeekStepMs));
    const auto forward =
        playerCommandForInput(PlayerInputAction::SeekForward, PlayerAppState::Playing, 1000.0);
    CHECK(forward.seekTargetMs == Catch::Approx(1000.0 + playerSeekStepMs));

    CHECK(playerCommandForInput(PlayerInputAction::Stop, PlayerAppState::Playing, 0.0).kind ==
          PlayerCommandKind::Stop);
    CHECK(playerCommandForInput(PlayerInputAction::Reload, PlayerAppState::Playing, 0.0)
              .reloadPolicy == PlayerReloadPolicy::KeepChartTime);
    CHECK(playerCommandForInput(PlayerInputAction::Rebuild, PlayerAppState::Playing, 0.0).kind ==
          PlayerCommandKind::Rebuild);

    // The produced commands are the same values a smoke script or the startup path hands over, so a
    // user action runs through the transaction, not around it.
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(
        fixture
            .apply(playerCommandForInput(PlayerInputAction::PlayPause, fixture.controller->state(),
                                         fixture.controller->lastRuntimeFrame().chartTimeMs))
            .has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
    REQUIRE(
        fixture
            .apply(playerCommandForInput(PlayerInputAction::PlayPause, fixture.controller->state(),
                                         fixture.controller->lastRuntimeFrame().chartTimeMs))
            .has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Paused);

    // A rejected action reports the same stable code the command table would report directly.
    Fixture empty;
    const auto rejected = empty.apply(
        playerCommandForInput(PlayerInputAction::Reload, empty.controller->state(), 0.0));
    CHECK_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.not_loaded");

    // Every action has a stable trace name.
    CHECK(playerInputActionName(PlayerInputAction::PlayPause) == "play_pause");
    CHECK(playerInputActionName(PlayerInputAction::SeekBackward) == "seek_backward");
    CHECK(playerInputActionName(PlayerInputAction::Quit) == "quit");
}

TEST_CASE("PlayerController keeps the discontinuity contract when input replaces the bundle",
          "[player][control][command][input][discontinuity]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());

    // Frame 2 replaces the bundle from the input path, which runs before that frame samples its
    // clock. The frame the session receives is therefore the re-advanced one, and it still has to
    // obey the rule that the first frame of a new discontinuity reports a zero delta.
    std::vector<std::vector<PlayerInputAction>> script(5);
    script[2] = {PlayerInputAction::Reload};
    ScriptedSurface surface{std::move(script)};

    cuexis::player::PlayerFrameLoop loop{.controller = *fixture.controller,
                                         .renderer = fixture.renderer,
                                         .surface = surface,
                                         .smokeTest = false,
                                         .audioSmokeTest = false,
                                         .diagnostics = nullptr,
                                         .hooks = {},
                                         .logger = *fixture.logger};
    const auto ran = runPlayerFrameLoop(loop);
    INFO("frame loop: " << (ran ? std::string{"ok"} : std::string{ran.error().code()}));
    REQUIRE(ran.has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
}

TEST_CASE("PlayerController shuts audio, the clip, and the session down in order",
          "[player][control][command][shutdown]") {
    Fixture fixture;
    REQUIRE(fixture.load(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    CHECK(fixture.store.metrics().registeredClips == 1);

    REQUIRE(fixture.controller->shutdown().has_value());

    CHECK(fixture.audio.unloadCalls == 1);
    CHECK(fixture.store.metrics().registeredClips == 0);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());
    // The runtime session is released and the application owns nothing, so no later command can
    // report content it no longer has.
    CHECK(fixture.controller->state() == PlayerAppState::Empty);
    CHECK_FALSE(fixture.controller->session().chartInfo().has_value());
    const auto rejected = fixture.play();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.not_loaded");
}

} // namespace
