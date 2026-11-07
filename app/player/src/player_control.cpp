#include "player_control.hpp"

#include "player_control_internal.hpp"
#include "player_log.hpp"

#include <cuexis/filesystem/secure_file.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::player {

namespace {

using control_internal::TransactionGuard;

constexpr std::size_t chartInputMaxBytes = 16U * 1024U * 1024U;

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
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (options.gameplayConfiguration && !options.gameplayPacked) {
        auto profile = readPlayerGameplay(options);
        if (!profile)
            return core::unexpected(std::move(profile.error()));
        const auto& b = profile->budget;
        const playback::GameplayGraphDecodeBudget graph{b.maxBytes,
                                                        b.maxDepth,
                                                        b.maxStringBytes,
                                                        b.maxValues,
                                                        b.maxContainerElements,
                                                        b.maxContainerElements,
                                                        b.maxStringBytes};
        return options.cxcPath
                   ? playback::PlaybackSource::fromCxcFileGameplayEntry(
                         *options.cxcPath, *options.candidateEntry, profile->configuration, b,
                         graph, playback::GameplayPrepareIntent::Presentation)
                   : playback::PlaybackSource::fromFilesystemGameplayEntry(
                         *options.projectPath, *options.candidateEntry, profile->configuration, b,
                         graph, playback::GameplayPrepareIntent::Presentation);
    }
#endif
    if (options.cxcPath) {
        return options.candidateEntry ? playback::PlaybackSource::fromCxcFileEntry(
                                            *options.cxcPath, *options.candidateEntry)
                                      : playback::PlaybackSource::fromCxcFile(*options.cxcPath);
    }
    if (options.projectPath.has_value()) {
        auto source = options.candidateEntry
                          ? playback::PlaybackSource::fromFilesystemProjectEntry(
                                *options.projectPath, *options.candidateEntry)
                          : playback::PlaybackSource::fromFilesystemProject(*options.projectPath);
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
        command.reloadPolicy = player_support::PlayerReloadPolicy::RestartAtZero;
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

auto PlayerController::audio() noexcept -> PlayerAudioSeat* {
    return audioSeat_.get();
}

auto PlayerController::gain() const noexcept -> double {
    return gain_;
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

auto preparePlayerLoadBundle(playback::PlaybackSource source, playback::PlaybackMode mode,
                             std::int64_t profileCorrectionUs) -> core::Result<PlayerLoadBundle> {
    auto session = std::make_unique<playback::PlaybackSession>();
    player_support::ResolvedSessionConfig config;
    config.clockMode = clockModeFor(mode);
    config.outputCorrectionUs =
        player_support::consumedOutputCorrectionUs(config.clockMode, profileCorrectionUs);
    auto prepared = session->prepareLoad(std::move(source), mode);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return PlayerLoadBundle{std::move(session), std::move(*prepared), mode, config};
}

auto PlayerController::adoptStartupLoad(PlayerLoadBundle&& load) -> core::Result<void> {
    if (session_ != nullptr) {
        return core::unexpected(core::Error{"player.command.duplicate",
                                            "A startup bundle can only be adopted into an Empty "
                                            "application"});
    }
    if (load.session == nullptr) {
        return core::unexpected(
            core::Error{"player.playback.prepared_invalid", "The startup bundle has no session"});
    }
    TransactionGuard guard{transactionInProgress_};
    // The session object outlives this call because `commitBundle` takes ownership of it. Reading
    // the raw pointer first keeps the reference independent of argument evaluation order.
    auto* const loaded = load.session.get();
    if (auto committed = commitBundle(*loaded, std::move(load.session), std::move(load.prepared),
                                      load.mode, load.config, true);
        !committed) {
        return committed;
    }
    // apply() publishes the decision state after a command. Adoption bypasses the command table
    // only because the startup Load is not a user command, so it publishes the same state itself.
    state_ = player_support::PlayerAppState::Loaded;
    return {};
}

} // namespace cuexis::player
