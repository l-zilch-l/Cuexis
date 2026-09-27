#include "player_control.hpp"

#include "frame_diagnostics.hpp"
#include "player_log.hpp"
#include "snapshot_scene.hpp"

#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/render/render_scene.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

namespace cuexis::player {
namespace {

constexpr std::uint32_t smokeTestFrameCount = 6;
constexpr std::uint32_t audioSmokeTestFrameCount = 90;
constexpr std::size_t chartInputMaxBytes = 16U * 1024U * 1024U;
constexpr std::array<double, smokeTestFrameCount> presentationSmokeTimes{0.0,    625.0,  1250.0,
                                                                         1750.0, 1750.0, 625.0};

// Wall clock for a ChartClock session. A seek or a resume rebases the origin so the sampled chart
// time continues from the requested target.
class PlayerClock final {
  public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] double nextChartTime(bool deterministic, std::uint32_t frameIndex) {
        if (deterministic) {
            last_ = presentationSmokeTimes[std::min<std::size_t>(
                frameIndex, presentationSmokeTimes.size() - 1U)];
            return last_;
        }
        last_ = base_ + elapsedMs();
        return last_;
    }

    void rebase(double targetMs) noexcept {
        base_ = targetMs - elapsedMs();
        last_ = targetMs;
    }

    [[nodiscard]] double lastChartTime() const noexcept {
        return last_;
    }

  private:
    [[nodiscard]] double elapsedMs() const noexcept {
        return std::chrono::duration<double, std::milli>(Clock::now() - started_).count();
    }

    const Clock::time_point started_{Clock::now()};
    double base_{};
    double last_{};
};

struct NullJudgeSystem final {
    void update(double) const noexcept {}
};

// Marks a load, reload, or rebuild as owning the application, so a re-entrant command is
// rejected instead of interleaving two transactions.
class TransactionGuard final {
  public:
    explicit TransactionGuard(bool& flag) noexcept : flag_(flag) {
        flag_ = true;
    }
    ~TransactionGuard() {
        flag_ = false;
    }

    TransactionGuard(const TransactionGuard&) = delete;
    auto operator=(const TransactionGuard&) -> TransactionGuard& = delete;
    TransactionGuard(TransactionGuard&&) = delete;
    auto operator=(TransactionGuard&&) -> TransactionGuard& = delete;

  private:
    bool& flag_;
};

[[nodiscard]] auto clockModeFor(playback::PlaybackMode mode) -> player_support::PlaybackClockMode {
    switch (mode) {
    case playback::PlaybackMode::CuexisAudio:
        return player_support::PlaybackClockMode::CuexisAudio;
    case playback::PlaybackMode::HostClock:
        return player_support::PlaybackClockMode::HostClock;
    case playback::PlaybackMode::ChartClock:
        return player_support::PlaybackClockMode::ChartClock;
    }
    return player_support::PlaybackClockMode::ChartClock;
}

[[nodiscard]] auto reloadPolicyFor(player_support::PlayerReloadPolicy policy)
    -> playback::ReloadPolicy {
    return policy == player_support::PlayerReloadPolicy::RestartAtZero
               ? playback::ReloadPolicy::RestartAtZero
               : playback::ReloadPolicy::KeepChartTime;
}
[[nodiscard]] auto identityText(const std::array<std::uint8_t, 32>& identity) -> std::string {
    std::string text(identity.size() * 2, '0');
    for (std::size_t index = 0; index < identity.size(); ++index) {
        char pair[3] = {};
        std::snprintf(pair, sizeof(pair), "%02x", identity[index]);
        text[index * 2] = pair[0];
        text[index * 2 + 1] = pair[1];
    }
    return text;
}

[[nodiscard]] auto readBoundedFile(const std::filesystem::path& path,
                                   const std::filesystem::path& root, std::size_t maxBytes,
                                   std::string_view errorPrefix, std::string_view description)
    -> core::Result<std::string> {
    const auto prefix = std::string{errorPrefix};
    auto contents = filesystem::readBoundedTextFile(
        path, {.root = root,
               .maxBytes = maxBytes,
               .errors = {.rootUnavailable = prefix + ".open_failed",
                          .rootChanged = prefix + ".root_changed",
                          .openFailed = prefix + ".open_failed",
                          .outsideRoot = prefix + ".outside_root",
                          .notRegular = prefix + ".open_failed",
                          .tooLarge = prefix + ".file_too_large",
                          .readFailed = prefix + ".read_failed",
                          .changedDuringRead = prefix + ".changed_during_read"}});
    if (!contents) {
        return core::unexpected(
            std::move(contents.error()).withContext("description", std::string{description}));
    }
    return std::move(contents->text);
}

} // namespace

auto openConfiguredPlaybackSource(const PlayerOptions& options)
    -> core::Result<playback::PlaybackSource> {
    if (options.projectPath.has_value()) {
        auto source = playback::PlaybackSource::fromFilesystemProject(*options.projectPath);
        if (!source) {
            return core::unexpected(core::Error{"player.project.load_failed",
                                                "Filesystem Playback source loading failed"}
                                        .withCause(std::move(source.error())));
        }
        return source;
    }
    const auto chartPath = *options.chartPath;
    const auto chartRoot =
        chartPath.parent_path().empty() ? std::filesystem::path{"."} : chartPath.parent_path();
    auto chartText =
        readBoundedFile(chartPath, chartRoot, chartInputMaxBytes, "player.chart", "chart");
    if (!chartText) {
        return core::unexpected(std::move(chartText.error()));
    }
    return playback::PlaybackSource::fromChartText(std::move(*chartText));
}

auto playerInputActionName(PlayerInputAction action) -> std::string_view {
    switch (action) {
    case PlayerInputAction::PlayPause:
        return "play_pause";
    case PlayerInputAction::Stop:
        return "stop";
    case PlayerInputAction::SeekBackward:
        return "seek_backward";
    case PlayerInputAction::SeekForward:
        return "seek_forward";
    case PlayerInputAction::Reload:
        return "reload";
    case PlayerInputAction::Rebuild:
        return "rebuild";
    case PlayerInputAction::Quit:
        return "quit";
    }
    return "unknown";
}

auto playerCommandForInput(PlayerInputAction action, player_support::PlayerAppState state,
                           double currentChartTimeMs) -> PlayerCommand {
    PlayerCommand command;
    switch (action) {
    case PlayerInputAction::PlayPause:
        command.kind = state == player_support::PlayerAppState::Playing
                           ? player_support::PlayerCommandKind::Pause
                           : player_support::PlayerCommandKind::Play;
        return command;
    case PlayerInputAction::Stop:
        command.kind = player_support::PlayerCommandKind::Stop;
        return command;
    case PlayerInputAction::SeekBackward:
        command.kind = player_support::PlayerCommandKind::Seek;
        command.seekTargetMs = std::max(0.0, currentChartTimeMs - playerSeekStepMs);
        return command;
    case PlayerInputAction::SeekForward:
        command.kind = player_support::PlayerCommandKind::Seek;
        command.seekTargetMs = currentChartTimeMs + playerSeekStepMs;
        return command;
    case PlayerInputAction::Reload:
        command.kind = player_support::PlayerCommandKind::Reload;
        command.reloadPolicy = player_support::PlayerReloadPolicy::KeepChartTime;
        return command;
    case PlayerInputAction::Rebuild:
        command.kind = player_support::PlayerCommandKind::Rebuild;
        return command;
    case PlayerInputAction::Quit:
        // Quit is not a content command. The frame loop consumes it and never calls this function
        // with it; the fallback keeps the function total.
        command.kind = player_support::PlayerCommandKind::Stop;
        return command;
    }
    return command;
}

PlayerController::PlayerController(presentation_renderer::IPresentationRenderer& renderer,
                                   audio::AudioClipStore& audioStore, double gain,
                                   std::int64_t profileCorrectionUs, PlayerControlPorts ports,
                                   PlayerLogger& logger)
    : renderer_(renderer), audioStore_(audioStore), gain_(gain),
      profileCorrectionUs_(profileCorrectionUs), ports_(std::move(ports)), logger_(logger) {}

PlayerController::~PlayerController() = default;

auto PlayerController::state() const noexcept -> player_support::PlayerAppState {
    return state_;
}

auto PlayerController::session() -> playback::PlaybackSession& {
    return *session_;
}

auto PlayerController::timeline() -> playback::RuntimeTimeline& {
    return *timeline_;
}

auto PlayerController::chartClock() -> playback::ChartClock& {
    return *chartClock_;
}

auto PlayerController::mode() const noexcept -> playback::PlaybackMode {
    return activeMode_;
}

auto PlayerController::sessionConfig() const noexcept
    -> const player_support::ResolvedSessionConfig& {
    return sessionConfig_;
}

auto PlayerController::timingOffsetMs() const noexcept -> double {
    return timingOffsetMs_;
}

auto PlayerController::audio() noexcept -> PlayerAudioSeat* {
    return audioSeat_.get();
}

auto PlayerController::activeAudioHandle() const noexcept -> std::optional<audio::AudioClipHandle> {
    return activeClip_;
}

auto PlayerController::lastRuntimeFrame() const noexcept -> const playback::RuntimeFrame& {
    return lastFrame_;
}

auto PlayerController::consumeChartSeekTarget() noexcept -> std::optional<double> {
    auto target = pendingChartSeekMs_;
    pendingChartSeekMs_.reset();
    return target;
}

auto PlayerController::consumeBundleReplaced() noexcept -> bool {
    const bool replaced = bundleReplaced_;
    bundleReplaced_ = false;
    return replaced;
}

void PlayerController::noteRuntimeFrame(const playback::RuntimeFrame& frame) noexcept {
    lastFrame_ = frame;
}

void PlayerController::observeTransport(const audio::SourceClockSample& sample) noexcept {
    lastSourcePositionMs_ = sample.positionMs;
    player_support::PlayerTransportObservation observation =
        player_support::PlayerTransportObservation::None;
    switch (sample.state) {
    case audio::PlaybackState::Empty:
        observation = player_support::PlayerTransportObservation::None;
        break;
    case audio::PlaybackState::Stopped:
        observation = player_support::PlayerTransportObservation::Stopped;
        break;
    case audio::PlaybackState::Playing:
        observation = player_support::PlayerTransportObservation::Playing;
        break;
    case audio::PlaybackState::Paused:
        observation = player_support::PlayerTransportObservation::Paused;
        break;
    case audio::PlaybackState::Ended:
        observation = player_support::PlayerTransportObservation::Ended;
        break;
    case audio::PlaybackState::Error:
        observation = player_support::PlayerTransportObservation::Error;
        break;
    }
    state_ = player_support::observeTransportState(state_, observation);
}

void PlayerController::markFailed(std::string_view reason) {
    publishFailure(reason);
}

void PlayerController::publishFailure(std::string_view reason) noexcept {
    state_ = player_support::PlayerAppState::Failed;
    logger_.warn("player.command",
                 std::string{"Application entered Failed: "} + std::string{reason});
}

auto PlayerController::apply(PlayerCommand command) -> core::Result<void> {
    auto decision = player_support::evaluatePlayerCommand(
        {.state = state_, .transactionInProgress = transactionInProgress_}, command.kind);
    if (!decision) {
        return core::unexpected(std::move(decision.error()));
    }
    core::Result<void> result = {};
    switch (command.kind) {
    case player_support::PlayerCommandKind::Load:
        result = runLoad(command);
        break;
    case player_support::PlayerCommandKind::Reload:
        result = runReload(command);
        break;
    case player_support::PlayerCommandKind::Rebuild:
        result = runRebuild(command);
        break;
    case player_support::PlayerCommandKind::Play:
        result = runPlay();
        break;
    case player_support::PlayerCommandKind::Pause:
        result = runPause();
        break;
    case player_support::PlayerCommandKind::Stop:
        result = runStop();
        break;
    case player_support::PlayerCommandKind::Seek:
        result = runSeek(command.seekTargetMs);
        break;
    }
    if (!result) {
        return core::unexpected(std::move(result.error()));
    }
    state_ = decision->state;
    return {};
}

auto PlayerController::sourceFor(PlayerCommand& command) -> core::Result<playback::PlaybackSource> {
    if (command.source.has_value()) {
        auto source = std::move(*command.source);
        command.source.reset();
        return source;
    }
    if (!ports_.makeSource) {
        return core::unexpected(core::Error{"player.command.source_unavailable",
                                            "No playback source factory is configured"});
    }
    return ports_.makeSource();
}

auto PlayerController::freezeConfig(playback::PlaybackMode mode) const
    -> player_support::ResolvedSessionConfig {
    player_support::ResolvedSessionConfig config;
    config.clockMode = clockModeFor(mode);
    config.outputCorrectionUs =
        player_support::consumedOutputCorrectionUs(config.clockMode, profileCorrectionUs_);
    return config;
}

auto PlayerController::discardClip(const std::optional<audio::AudioClipHandle>& clip) -> void {
    if (!clip.has_value()) {
        return;
    }
    if (auto removed = audioStore_.remove(*clip); !removed) {
        logger_.warn("player.audio", "A discarded audio clip could not be removed");
    }
}

auto PlayerController::runLoad(PlayerCommand& command) -> core::Result<void> {
    const bool explicitSource = command.source.has_value();
    TransactionGuard guard{transactionInProgress_};

    auto source = sourceFor(command);
    if (!source) {
        return core::unexpected(std::move(source.error()));
    }
    auto session = std::make_unique<playback::PlaybackSession>();
    auto mode = command.mode.value_or(playback::PlaybackMode::ChartClock);
    auto config = freezeConfig(mode);
    auto prepared = session->prepareLoad(std::move(*source), mode);
    if (!prepared && !command.mode.has_value() &&
        prepared.error().code() == "playback.mode.content_mismatch") {
        if (explicitSource) {
            // An explicit source cannot be re-read, so the mode must be named instead of probed.
            return core::unexpected(
                core::Error{"player.command.mode_required",
                            "An explicit source that needs an audio clock must name its mode"}
                    .withCause(std::move(prepared.error())));
        }
        // The configured source is re-read and prepared once with the audio clock. The mode is
        // never guessed from a failed file.
        auto retry = sourceFor(command);
        if (!retry) {
            return core::unexpected(std::move(retry.error()));
        }
        mode = playback::PlaybackMode::CuexisAudio;
        config = freezeConfig(mode);
        prepared = session->prepareLoad(std::move(*retry), mode);
    }
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()).withContext("command", "load"));
    }
    // The session object outlives this call because `adopt` takes ownership of it. Reading the raw
    // pointer first keeps the reference independent of argument evaluation order, which is
    // unspecified between the two parameters.
    auto* const loaded = session.get();
    return commitBundle(*loaded, std::move(session), std::move(*prepared), mode, config, true);
}

auto PlayerController::runReload(PlayerCommand& command) -> core::Result<void> {
    if (session_ == nullptr || !timeline_.has_value() || !chartClock_.has_value()) {
        return core::unexpected(core::Error{"player.command.not_loaded", "No content is loaded"});
    }
    TransactionGuard guard{transactionInProgress_};

    auto source = sourceFor(command);
    if (!source) {
        return core::unexpected(std::move(source.error()));
    }
    const auto policy = reloadPolicyFor(command.reloadPolicy);
    auto prepared = session_->prepareReload(std::move(*source), lastFrame_, policy);
    if (!prepared) {
        auto error = std::move(prepared.error());
        if (error.code() == "playback.mode.content_mismatch") {
            // Reload never changes PlaybackMode. Switching between content with and without an
            // audio track is an explicit Load.
            error = core::Error{"player.command.mode_change_requires_load",
                                "Reload keeps the active PlaybackMode; use Load to switch"}
                        .withCause(std::move(error));
        }
        return core::unexpected(std::move(error).withContext("command", "reload"));
    }
    // The restart policy also decides where the audio replacement and the chart clock resume.
    const bool restartTimeline = policy == playback::ReloadPolicy::RestartAtZero;
    return commitBundle(*session_, nullptr, std::move(*prepared), activeMode_, sessionConfig_,
                        restartTimeline);
}

auto PlayerController::runRebuild(PlayerCommand& command) -> core::Result<void> {
    if (session_ == nullptr) {
        if (state_ != player_support::PlayerAppState::Failed) {
            return core::unexpected(
                core::Error{"player.command.not_loaded", "No content is loaded"});
        }
        // The application failed before the first bundle existed, so there is nothing to rebuild.
        // Reopening the configured source is the recovery the command table promises for Failed.
        return runLoad(command);
    }
    TransactionGuard guard{transactionInProgress_};

    // The rebuild invalidates the active presentation and every renderer token, so the rest of
    // this transaction must succeed for the application to keep a renderable frame.
    if (auto rebuilt = renderer_.rebuild(); !rebuilt) {
        publishFailure("renderer_rebuild");
        return core::unexpected(std::move(rebuilt.error()).withContext("command", "rebuild"));
    }
    // A rebuild also recreates the device path. Closing the seat here is what makes Rebuild the
    // recovery for a device failure that left the transport in its error state: the transaction
    // then opens a fresh seat instead of reusing the failed one.
    if (audioSeat_ != nullptr) {
        if (auto unloaded = audioSeat_->unload(); !unloaded) {
            logger_.warn("player.audio", "The rebuilt audio seat could not be closed cleanly");
        }
        audioSeat_.reset();
    }
    auto source = sourceFor(command);
    if (!source) {
        publishFailure("renderer_rebuild_source");
        return core::unexpected(std::move(source.error()));
    }
    auto prepared = session_->prepareReload(std::move(*source), lastFrame_,
                                            playback::ReloadPolicy::KeepChartTime);
    if (!prepared) {
        publishFailure("renderer_rebuild_prepare");
        return core::unexpected(std::move(prepared.error()).withContext("command", "rebuild"));
    }
    auto result =
        commitBundle(*session_, nullptr, std::move(*prepared), activeMode_, sessionConfig_, false);
    if (!result) {
        publishFailure("renderer_rebuild_commit");
    }
    return result;
}

auto PlayerController::runPlay() -> core::Result<void> {
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        if (auto played = audioSeat_->transport().play(); !played) {
            return core::unexpected(std::move(played.error()).withContext("command", "play"));
        }
        return {};
    }
    // A ChartClock session has no transport: the frame loop resumes from the frozen chart time.
    pendingChartSeekMs_ = chartResumeMs_;
    if (chartClock_.has_value()) {
        chartClock_->markDiscontinuity();
    }
    return {};
}

auto PlayerController::runPause() -> core::Result<void> {
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        if (auto paused = audioSeat_->transport().pause(); !paused) {
            return core::unexpected(std::move(paused.error()).withContext("command", "pause"));
        }
        return {};
    }
    // The frame loop stops sampling the wall clock while the application is not playing, so the
    // frozen chart time is what a later Play resumes from.
    chartResumeMs_ = lastFrame_.chartTimeMs;
    return {};
}

auto PlayerController::runStop() -> core::Result<void> {
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        if (auto stopped = audioSeat_->transport().stop(); !stopped) {
            return core::unexpected(std::move(stopped.error()).withContext("command", "stop"));
        }
        return {};
    }
    // A ChartClock session rewinds to chart time zero. This is a transport rewind, not the
    // RestartAtZero reload policy, and it publishes one discontinuity.
    return seekChartClock(0.0);
}

auto PlayerController::runSeek(double targetMs) -> core::Result<void> {
    // The application layer does not know the playable duration; the audio transport enforces the
    // upper bound against the decoded clip, and a ChartClock session has no duration to check.
    if (auto valid = player_support::validateSeekTargetMs(targetMs, -1.0); !valid) {
        return core::unexpected(std::move(valid.error()).withContext("command", "seek"));
    }
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        return seekAudio(targetMs);
    }
    return seekChartClock(targetMs);
}

auto PlayerController::seekChartClock(double targetMs) -> core::Result<void> {
    pendingChartSeekMs_ = targetMs;
    chartResumeMs_ = targetMs;
    if (chartClock_.has_value()) {
        chartClock_->markDiscontinuity();
    }
    return {};
}

auto PlayerController::seekAudio(double targetMs) -> core::Result<void> {
    const auto targetUs = static_cast<std::int64_t>(std::llround(targetMs * 1000.0));
    const auto offsetUs = static_cast<std::int64_t>(std::llround(timingOffsetMs_ * 1000.0));
    auto sourceUs = player_support::reverseSeekSourcePositionUs(targetUs, offsetUs,
                                                                sessionConfig_.outputCorrectionUs);
    if (!sourceUs) {
        return core::unexpected(std::move(sourceUs.error()).withContext("command", "seek"));
    }
    if (*sourceUs < 0) {
        return core::unexpected(
            core::Error{"player.command.seek_outside",
                        "The corrected seek source is outside the playable range"}
                .withContext("target_ms", std::to_string(targetMs)));
    }
    auto& transport = audioSeat_->transport();
    if (auto sought = transport.seekMs(static_cast<double>(*sourceUs) / 1000.0); !sought) {
        return core::unexpected(std::move(sought.error()).withContext("command", "seek"));
    }
    return {};
}

auto PlayerController::commitBundle(playback::PlaybackSession& session,
                                    std::unique_ptr<playback::PlaybackSession> adopt,
                                    playback::PreparedPlayback&& prepared,
                                    playback::PlaybackMode mode,
                                    player_support::ResolvedSessionConfig config,
                                    bool restartTimeline) -> core::Result<void> {
    // Step 1: validate the content metadata and freeze this operation's time and configuration.
    const auto* info = prepared.contentInfo();
    if (info == nullptr) {
        return core::unexpected(core::Error{"player.playback.prepared_invalid",
                                            "Prepared Playback content metadata is unavailable"});
    }
    const auto timingOffsetMs = info->timingOffsetMs;
    if (!std::isfinite(timingOffsetMs)) {
        return core::unexpected(core::Error{"player.playback.timing_offset_invalid",
                                            "The prepared timing offset must be finite"});
    }
    // Captured before the frame record is reset, because a keeping transaction resumes here.
    const double preservedChartTimeMs = lastFrame_.chartTimeMs;
    std::optional<playback::RuntimeTimeline> nextTimeline;
    if (timeline_.has_value()) {
        nextTimeline = *timeline_;
        if (auto reset = nextTimeline->reset(timingOffsetMs); !reset) {
            return core::unexpected(std::move(reset.error()));
        }
    } else {
        auto created = playback::RuntimeTimeline::create(timingOffsetMs);
        if (!created) {
            return core::unexpected(std::move(created.error()));
        }
        nextTimeline = std::move(*created);
    }

    // Step 2: prepare the renderer candidate and the audio replacement. Nothing is activated yet.
    auto candidate = renderer_.prepare(prepared, {.enableDebugPass = true});
    if (!candidate) {
        return core::unexpected(
            std::move(candidate.error()).withContext("operation", "prepare_presentation"));
    }

    const bool needsAudio = mode == playback::PlaybackMode::CuexisAudio;
    const bool deviceChanges =
        needsAudio ? (audioSeat_ == nullptr || activeMode_ != mode) : (audioSeat_ != nullptr);
    // The source position the new stream must start at, whether it replaces the stream on the
    // current device or is the first stream of a freshly opened one.
    const double startPositionMs = restartTimeline ? 0.0 : lastSourcePositionMs_;
    std::optional<audio::AudioClipHandle> replacementClip;
    if (needsAudio) {
        auto clip = ports_.prepareClip(prepared, audioStore_);
        if (!clip) {
            renderer_.discard(std::move(*candidate));
            return core::unexpected(std::move(clip.error()).withContext("stage", "audio_prepare"));
        }
        replacementClip = *clip;
        // A same-device replacement completes its stream preparation here, so no allocation is
        // deferred until after the previous stream is closed.
        if (!deviceChanges && audioSeat_ != nullptr) {
            if (auto staged = audioSeat_->prepareReplacement(*replacementClip, startPositionMs);
                !staged) {
                discardClip(replacementClip);
                renderer_.discard(std::move(*candidate));
                return core::unexpected(
                    std::move(staged.error()).withContext("stage", "audio_prepare"));
            }
        }
    }

    // Step 3: precheck every owner, generation, and token. Nothing after this point may fail
    // because a candidate went stale.
    if (auto accepted = renderer_.accepts(*candidate); !accepted) {
        discardClip(replacementClip);
        renderer_.discard(std::move(*candidate));
        return core::unexpected(std::move(accepted.error()).withContext("stage", "precheck"));
    }

    // Step 4: activate audio. This is the last step allowed to fail on a physical device.
    std::unique_ptr<PlayerAudioSeat> stagedSeat;
    if (needsAudio && deviceChanges) {
        auto opened = ports_.openAudio(replacementClip, gain_, startPositionMs);
        if (!opened) {
            discardClip(replacementClip);
            renderer_.discard(std::move(*candidate));
            publishFailure("audio_activation");
            return core::unexpected(
                std::move(opened.error()).withContext("stage", "audio_activate"));
        }
        stagedSeat = std::move(*opened);
    } else if (needsAudio && audioSeat_ != nullptr) {
        if (auto activated = audioSeat_->activateReplacement(); !activated) {
            discardClip(replacementClip);
            renderer_.discard(std::move(*candidate));
            publishFailure("audio_activation");
            return core::unexpected(
                std::move(activated.error()).withContext("stage", "audio_activate"));
        }
    }

    // Step 5: the no-failure content exchange.
    if (auto committed = session.commit(std::move(prepared)); !committed) {
        discardClip(replacementClip);
        renderer_.discard(std::move(*candidate));
        publishFailure("commit");
        return core::unexpected(std::move(committed.error()).withContext("stage", "commit"));
    }
    renderer_.activate(std::move(*candidate));

    // Step 6: publish the active bundle, the effective settings, and one discontinuity.
    if (adopt != nullptr) {
        session_ = std::move(adopt);
    }
    timeline_ = std::move(*nextTimeline);
    chartClock_.emplace(timingOffsetMs);
    activeMode_ = mode;
    sessionConfig_ = config;
    timingOffsetMs_ = timingOffsetMs;
    // A ChartClock bundle has no transport to reposition, so the frame loop rebases the chart
    // clock on its next sample: to zero for a restart, or to the pre-transaction time for a keep.
    if (mode == playback::PlaybackMode::ChartClock) {
        const double targetMs = restartTimeline ? 0.0 : preservedChartTimeMs;
        pendingChartSeekMs_ = targetMs;
        chartResumeMs_ = targetMs;
    } else {
        pendingChartSeekMs_.reset();
        chartResumeMs_ = 0.0;
    }
    // The frame record stays as it was: it is the last frame the session actually received, and it
    // is both the reload target and the discontinuity the next frame must be measured against.
    // Clearing it here would claim a frame the session never saw.
    if (restartTimeline) {
        lastSourcePositionMs_ = 0.0;
    }
    if (deviceChanges || (needsAudio && audioSeat_ == nullptr)) {
        // The previous device is released only after the new stream is active. A release failure
        // cannot roll the committed bundle back, so it is reported and the transaction stands.
        if (audioSeat_ != nullptr && audioSeat_ != stagedSeat) {
            if (auto unloaded = audioSeat_->unload(); !unloaded) {
                logger_.warn("player.audio", "The previous audio device could not be released");
            }
        }
        audioSeat_ = std::move(stagedSeat);
    }

    const auto previousClip = activeClip_;
    activeClip_ = replacementClip;
    if (previousClip.has_value() && (!activeClip_.has_value() || *previousClip != *activeClip_)) {
        if (auto removed = audioStore_.remove(*previousClip); !removed) {
            logger_.warn("player.audio", "The previous audio clip could not be removed");
        }
    }

    bundleReplaced_ = true;
    const auto identity = player_support::resolvedSessionConfigIdentity(sessionConfig_);
    logger_.info("player.config", std::string{"Session identity "} + identityText(identity));
    if (auto chartInfo = session_->chartInfo(); chartInfo) {
        if (chartInfo->objectCount == 0) {
            logger_.warn("player.chart", "The committed chart contains no objects");
        }
        logger_.info("player.playback",
                     std::string{"Prepared objects: "} + std::to_string(chartInfo->objectCount) +
                         ", behaviors: " + std::to_string(chartInfo->behaviorCount) +
                         ", resources: " + std::to_string(chartInfo->resourceCount));
    } else {
        logger_.warn("player.playback", "Committed chart metadata is unavailable");
    }
    return {};
}

auto PlayerController::shutdown() -> core::Result<void> {
    core::Result<void> first = {};
    if (audioSeat_ != nullptr) {
        if (auto unloaded = audioSeat_->unload(); !unloaded) {
            first = core::unexpected(std::move(unloaded.error()));
        }
        audioSeat_.reset();
    }
    if (activeClip_.has_value()) {
        if (auto removed = audioStore_.remove(*activeClip_); !removed) {
            if (first.has_value()) {
                first = core::unexpected(std::move(removed.error()));
            }
        }
        activeClip_.reset();
    }
    if (session_ != nullptr) {
        if (auto unloaded = session_->unload(); !unloaded) {
            if (first.has_value()) {
                first = core::unexpected(std::move(unloaded.error()));
            }
        }
    }
    // The application owns no bundle after this point, so no later command may report content.
    state_ = player_support::PlayerAppState::Empty;
    return first;
}

auto runPlayerFrameLoop(PlayerFrameLoop& input) -> core::Result<void> {
    PlayerClock clock;
    const NullJudgeSystem judgeSystem;
    const auto diagnosticsStarted = std::chrono::steady_clock::now();
    std::uint32_t renderedFrames = 0;
    bool quitRequested = false;
    playback::FrameSnapshot snapshot;
    render::RenderScene scene;
    auto& controller = input.controller;

    // Samples the active clock and advances the timeline. Called once per frame, and again when a
    // command inside a hook replaced the active bundle.
    const auto advanceFrame = [&](playback::RuntimeFrame& frame,
                                  audio::AudioClockSnapshot& audioClock) -> core::Result<void> {
        if (auto* audio = controller.audio(); audio != nullptr) {
            if (auto serviced = audio->transport().service(); !serviced) {
                return core::unexpected(std::move(serviced.error()));
            }
            if (auto rebound = audio->recheckBoundDevice(); !rebound) {
                return core::unexpected(std::move(rebound.error()));
            }
            audioClock = audio->transport().snapshot();
            controller.observeTransport(audioClock.source);
            auto consumedSource = audioClock.source;
            if (controller.sessionConfig().outputCorrectionUs != 0) {
                auto corrected = player_support::correctConsumedAudioPositionMs(
                    consumedSource.positionMs, controller.sessionConfig().outputCorrectionUs);
                if (!corrected) {
                    return core::unexpected(std::move(corrected.error()));
                }
                consumedSource.positionMs = *corrected;
            }
            auto advanced = controller.timeline().advance(consumedSource);
            if (!advanced) {
                return core::unexpected(std::move(advanced.error()));
            }
            frame = *advanced;
            return {};
        }
        double chartTimeMs = 0.0;
        if (auto target = controller.consumeChartSeekTarget(); target.has_value()) {
            clock.rebase(*target);
            chartTimeMs = *target;
        } else if (controller.state() == player_support::PlayerAppState::Playing) {
            chartTimeMs = clock.nextChartTime(input.smokeTest, renderedFrames);
        } else {
            chartTimeMs = clock.lastChartTime();
        }
        auto source = controller.chartClock().sample(chartTimeMs);
        if (!source) {
            return core::unexpected(std::move(source.error()));
        }
        auto advanced = controller.timeline().advance(*source);
        if (!advanced) {
            return core::unexpected(std::move(advanced.error()));
        }
        frame = *advanced;
        return {};
    };

    while (!quitRequested) {
        // The discontinuity the session last received. A command below may replace the bundle, and
        // the frame this iteration delivers is then the first frame of a new discontinuity.
        const auto deliveredDiscontinuityId = controller.lastRuntimeFrame().timeDiscontinuityId;
        auto observedInput = input.surface.pollInput();
        if (!observedInput) {
            return core::unexpected(std::move(observedInput.error()));
        }
        quitRequested = observedInput->quitRequested;
        if (quitRequested) {
            break;
        }
        // Every user action enters through the same command entry as the smoke scripts. A rejected
        // action is a warning, not a failure: pressing Pause before content exists must not stop
        // the player.
        for (const auto action : observedInput->actions) {
            if (action == PlayerInputAction::Quit) {
                quitRequested = true;
                break;
            }
            auto command = playerCommandForInput(action, controller.state(),
                                                 controller.lastRuntimeFrame().chartTimeMs);
            if (auto applied = controller.apply(std::move(command)); !applied) {
                input.logger.warn("player.command", std::string{"Input action rejected: "} +
                                                        std::string{applied.error().code()});
            } else {
                input.logger.info("player.input", std::string{"Applied user action: "} +
                                                      std::string{playerInputActionName(action)});
            }
        }
        if (quitRequested) {
            break;
        }

        if (input.hooks.beforeAudioService) {
            if (auto stepped = input.hooks.beforeAudioService(renderedFrames); !stepped) {
                return core::unexpected(std::move(stepped.error()));
            }
        }

        audio::AudioClockSnapshot audioClockSnapshot;
        playback::RuntimeFrame advancedFrame{};
        auto advanceResult = advanceFrame(advancedFrame, audioClockSnapshot);
        core::Result<playback::RuntimeFrame> runtimeFrameResult =
            advanceResult ? core::Result<playback::RuntimeFrame>{advancedFrame}
                          : core::unexpected(std::move(advanceResult.error()));
        if (runtimeFrameResult) {
            controller.noteRuntimeFrame(*runtimeFrameResult);
        }
        if (input.hooks.afterTimelineAdvance) {
            PlayerClockContext clockContext{.renderedFrames = renderedFrames,
                                            .controller = controller,
                                            .renderer = input.renderer,
                                            .runtimeFrame = runtimeFrameResult,
                                            .audioClock = audioClockSnapshot,
                                            .logger = input.logger};
            if (auto stepped = input.hooks.afterTimelineAdvance(clockContext); !stepped) {
                return core::unexpected(std::move(stepped.error()));
            }
        }
        if (!runtimeFrameResult) {
            return core::unexpected(std::move(runtimeFrameResult.error()));
        }
        // A transaction inside the hook replaced the active bundle, so this frame must belong to
        // the new content rather than to the bundle that was active when the frame started.
        if (controller.consumeBundleReplaced()) {
            auto refreshed = advanceFrame(advancedFrame, audioClockSnapshot);
            if (!refreshed) {
                return core::unexpected(std::move(refreshed.error()));
            }
            // The re-advanced frame is the one the session receives, so it carries the first frame
            // of the new bundle. When it also carries a new discontinuity it must report a zero
            // delta, exactly like any other first frame after a discontinuity: the session compares
            // it against the frame the previous iteration delivered, not against the superseded
            // sample this iteration took first.
            if (advancedFrame.timeDiscontinuityId != deliveredDiscontinuityId) {
                advancedFrame.simulationDeltaTimeMs = 0.0;
            }
            runtimeFrameResult = advancedFrame;
        }
        const auto runtimeFrame = *runtimeFrameResult;
        controller.noteRuntimeFrame(runtimeFrame);
        judgeSystem.update(runtimeFrame.chartTimeMs);
        if (auto result = controller.session().update(runtimeFrame); !result) {
            return core::unexpected(std::move(result.error()));
        }

        auto drawableSizeResult = input.surface.drawableSize();
        if (!drawableSizeResult) {
            return core::unexpected(std::move(drawableSizeResult.error())
                                        .withContext("operation", "query_drawable_size"));
        }
        const auto width = static_cast<std::uint32_t>(std::max(drawableSizeResult->width, 0));
        const auto height = static_cast<std::uint32_t>(std::max(drawableSizeResult->height, 0));
        if (auto resized = input.renderer.resize(width, height); !resized) {
            return core::unexpected(
                std::move(resized.error()).withContext("operation", "resize_presentation"));
        }
        if (auto result =
                controller.session().extractFrame({.width = width, .height = height}, snapshot);
            !result) {
            return core::unexpected(std::move(result.error()));
        }

        scene.clear();
        if (auto result = appendSnapshotAxes(snapshot, scene); !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (input.diagnostics != nullptr) {
            input.diagnostics->captureFrame(renderedFrames, runtimeFrame, snapshot);
            if (auto* audio = controller.audio(); audio != nullptr) {
                const double wallClockMs =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                              diagnosticsStarted)
                        .count();
                input.diagnostics->captureAudio(renderedFrames, wallClockMs, audioClockSnapshot,
                                                audio->transport().metrics());
            }
        }

        if (input.hooks.beforeSubmit) {
            if (auto stepped = input.hooks.beforeSubmit(snapshot, renderedFrames); !stepped) {
                return core::unexpected(std::move(stepped.error()));
            }
        }

        const auto renderStarted = std::chrono::steady_clock::now();
        auto submitted = input.renderer.submit(snapshot, &scene);
        if (!submitted) {
            if (submitted.error().code() == "presentation.renderer.surface.zero_size") {
                continue;
            }
            controller.markFailed("present_submit");
            return core::unexpected(
                std::move(submitted.error()).withContext("frame", std::to_string(renderedFrames)));
        }
        if (auto presented = input.renderer.present(); !presented) {
            controller.markFailed("present");
            return core::unexpected(
                std::move(presented.error()).withContext("frame", std::to_string(renderedFrames)));
        }
        PlayerPresentedFrame presented{.renderedFrames = renderedFrames,
                                       .snapshot = snapshot,
                                       .summary = *submitted,
                                       .renderMicroseconds = 0.0,
                                       .renderer = input.renderer,
                                       .controller = controller};
        if (input.hooks.notePresented) {
            if (auto noted = input.hooks.notePresented(presented); !noted) {
                return core::unexpected(std::move(noted.error()));
            }
        }
        presented.renderMicroseconds = std::chrono::duration<double, std::micro>(
                                           std::chrono::steady_clock::now() - renderStarted)
                                           .count();
        if (renderedFrames == 0) {
            input.logger.info(
                "player.snapshot",
                std::string{"Objects: "} + std::to_string(snapshot.objects.size()) +
                    ", opaque draws: " + std::to_string(submitted->opaque.size()) +
                    ", transparent draws: " + std::to_string(submitted->transparent.size()) +
                    ", debug commands: " + std::to_string(submitted->debugCommandCount));
        }
        if (input.hooks.validatePresented) {
            if (auto validated = input.hooks.validatePresented(presented); !validated) {
                return core::unexpected(std::move(validated.error()));
            }
        }

        ++renderedFrames;
        if (input.smokeTest && renderedFrames >= smokeTestFrameCount) {
            quitRequested = true;
        }
        if (input.audioSmokeTest && renderedFrames >= audioSmokeTestFrameCount) {
            quitRequested = true;
        }
    }

    if (input.smokeTest && renderedFrames != smokeTestFrameCount) {
        return core::unexpected(
            core::Error{"player.smoke_test.incomplete",
                        "Smoke test ended before rendering required frames"}
                .withContext("expected_frames", std::to_string(smokeTestFrameCount))
                .withContext("rendered_frames", std::to_string(renderedFrames)));
    }
    if (input.smokeTest) {
        input.logger.info("player.smoke_test",
                          std::string{"Completed frames: "} + std::to_string(renderedFrames));
    }
    if (input.audioSmokeTest && renderedFrames != audioSmokeTestFrameCount) {
        return core::unexpected(
            core::Error{"player.audio_smoke_test.incomplete",
                        "Audio smoke test ended before rendering required frames"}
                .withContext("expected_frames", std::to_string(audioSmokeTestFrameCount))
                .withContext("rendered_frames", std::to_string(renderedFrames)));
    }
    if (input.audioSmokeTest) {
        input.logger.info("player.audio_smoke_test",
                          std::string{"Completed frames: "} + std::to_string(renderedFrames));
    }
    return {};
}

} // namespace cuexis::player
