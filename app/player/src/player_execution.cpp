#include "player_execution.hpp"
#include "execution_host.hpp"
#include "player_assembly.hpp"
#include "player_control.hpp"
#include "player_gameplay.hpp"
#include <charconv>
#include <cmath>
#include <cuexis/audio_sdl/sdl_audio.hpp>
#include <cuexis/audio_sdl/wav_decoder.hpp>
#include <deque>
#include <limits>
#include <thread>

namespace cuexis::player {
using namespace playback;
using namespace playback::detail;
namespace {
struct Failure {
    core::Error error;
};
template <class T> auto take(core::Result<T> value) -> T {
    if (!value)
        throw Failure{std::move(value.error())};
    return std::move(*value);
}
void take(core::Result<void> value) {
    if (!value)
        throw Failure{std::move(value.error())};
}
auto nowUs() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
auto error(const char* text) -> core::Error {
    return {"player.execution.test_only", text};
}
class SdlDevice final : public OwnedAudioDevice {
  public:
    SdlDevice(audio_sdl::SdlAudioSubsystem subsystem, audio_sdl::SdlAudioTransport transport)
        : subsystem_(std::move(subsystem)), transport_(std::move(transport)) {}
    auto transport() noexcept -> audio::IAudioTransport& override {
        return transport_;
    }
    auto isolateCallbacks() -> core::Result<bool> override {
        return transport_.isolateCallbacks();
    }
    auto recheckDevice() -> core::Result<void> override {
        return transport_.recheckBoundDevice();
    }
    auto clockObservation() const noexcept -> std::optional<OwnedAudioClockObservation> override {
        auto sample = transport_.presentedClockObservation();
        return OwnedAudioClockObservation{sample.clock, sample.sampleMonotonicUs,
                                          sample.maximumPresentedFrame, sample.errorBoundUs};
    }

  private:
    audio_sdl::SdlAudioSubsystem subsystem_;
    audio_sdl::SdlAudioTransport transport_;
};
} // namespace
auto makeExecutionAudioFactory(const player_support::AudioDeviceProfile& profile, float gain)
    -> AudioWorker::DeviceFactory {
    return [profile,
            gain](audio::AudioClipStore& store) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
        try {
            auto subsystem = audio_sdl::SdlAudioSubsystem::create();
            if (!subsystem)
                return core::unexpected(subsystem.error());
            auto config = audio::validateAudioConfig(
                {audio::AudioDeviceRequest::DefaultPlayback, 100, 50, gain});
            if (!config)
                return core::unexpected(config.error());
            core::Result<audio_sdl::SdlAudioTransport> transport =
                core::unexpected(error("Audio device has not opened"));
            if (profile.selector == player_support::AudioSelectorKind::SystemDefault) {
                transport = audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
            } else {
                auto devices = subsystem->enumeratePlaybackDevices();
                if (!devices)
                    return core::unexpected(devices.error());
                std::optional<audio_sdl::PlaybackDeviceTarget> target;
                for (const auto& device : *devices)
                    if (device.driver == profile.driver &&
                        device.deviceName == profile.deviceName) {
                        if (target)
                            return core::unexpected(error("Audio device is ambiguous"));
                        target = audio_sdl::PlaybackDeviceTarget{device.instanceId, device.driver,
                                                                 device.deviceName};
                    }
                if (!target)
                    return core::unexpected(error("Audio device is unavailable"));
                transport = audio_sdl::SdlAudioTransport::createForDevice(*subsystem, store,
                                                                          *config, *target);
            }
            if (!transport)
                return core::unexpected(transport.error());
            return std::unique_ptr<OwnedAudioDevice>{
                std::make_unique<SdlDevice>(std::move(*subsystem), std::move(*transport))};
        } catch (...) {
            return core::unexpected(error("Audio device allocation failed"));
        }
    };
}
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
auto runExecutionPlayer(const PlayerOptions& options, const player_support::AppConfigLoad& app,
                        PlayerLogger& logger) -> core::Result<void> try {
    if (!options.executionTestOnly)
        return core::unexpected(error("Production budgets are unaccepted"));
    auto profile = take(readPlayerGameplay(options));
    if (!profile.configuration.testOnly ||
        profile.configuration.engine[4] != "gameplay.execution.t4-k4.rpa.v1")
        return core::unexpected(
            error("Explicit testOnly rpa.v1 Gameplay configuration is required"));
    ClockMappingConfiguration mapping{{app.profile.outputCorrectionUs, 1}, {0, 1}, {0, 1}, {1, 1}};
    auto identity = take(RealtimeClock::calibrationIdentity(mapping));
    if (profile.configuration.calibration != identity)
        return core::unexpected(
            error("Gameplay calibration does not match the explicit test clock"));
    logger.info("player.execution", "testOnly: capacities are provisional; error bound 20ms, "
                                    "validity 500ms; no production budget acceptance");
    auto runtime = take(createPlayerRuntime(logger));
    auto window = take(createPlayerWindow(runtime, app.app.requested));
    std::optional<render_opengl::OpenGlBackend> backend;
    backend.emplace(take(createPlayerBackend(runtime, window, app.app.requested.vsync,
                                             options.shaderCacheDirectory, logger)));
    std::vector<std::uint32_t> keyCodes;
    for (const auto& key : profile.keys)
        keyCodes.push_back(key.scanCode);
    auto surface = makePlayerSurface(window, std::move(keyCodes));
    SchedulingProfile scheduling{500000, 20000, 4096, "player.b5.test-only.v1", true};
    ExecutionHostOptions hostOptions;
    hostOptions.showFps = options.showFps;
    if (options.clock == PlayerClockOption::Host)
        throw Failure{error("Execution main music requires the explicit Audio clock")};
    hostOptions.sourceMode = options.clock == PlayerClockOption::Audio ? PlaybackMode::HostClock
                                                                       : PlaybackMode::ChartClock;
    hostOptions.session = {128, 32, 4096, 128, 4096, 4096, true, nowUs};
    hostOptions.audio.sessionScope = 1;
    hostOptions.audio.timeDomain = 1;
    hostOptions.audio.testOnly = true;
    hostOptions.audio.capacity = 128;
    hostOptions.audio.controlReserve = 32;
    hostOptions.audio.clipLimits = {4, audio::maxDecodedClipBytes, 512U * 1024U * 1024U};
    hostOptions.hostCapacity = 256;
    hostOptions.captureCapacity = 4096;
    hostOptions.captureReserve = 128;
    hostOptions.replyDeadline = std::chrono::seconds{5};
    auto drawable = take(surface->drawableSize());
    hostOptions.viewport = {static_cast<std::uint32_t>(drawable.width),
                            static_cast<std::uint32_t>(drawable.height)};
    hostOptions.realtime = SessionRealtimeStart{scheduling, mapping, {1, 1, 1}, "steady.us"};
    std::unique_ptr<ExecutionHost> host;
    std::optional<RealtimeClock> captureClock;
    std::deque<PlayerInputAction> actions;
    bool quitting = false, overflow = false, faulted = false, focusPauseRequested = false;
    std::optional<core::Error> fault;
    std::uint64_t inputSequence = 0, noAudioSequence = 0, captureSegment = 0;
    std::int64_t anchorUs = nowUs(), anchorPositionUs = 0;
    std::optional<std::int64_t> pausedPositionUs;
    std::optional<GameplayReplay> seekHistory;
    std::int64_t seekHistoryHorizon = 0;
    std::optional<std::int64_t> previousPresentationTick;
    std::optional<std::uint64_t> scoreCursor;
    std::uint64_t previousPresentationSegment = 0;
    std::uint64_t backendSegment = 0, logicalSegment = 0, underruns = 0;
    std::uint32_t backendRate = 0;
    auto observation = [&](const HostContext& context) -> core::Result<ClockObservation> {
        try {
            ClockObservation value;
            value.monotonicDomain = "steady.us";
            value.segment = context.clockSegment;
            if (host->hasAudio()) {
                auto publication = host->audioPublication();
                const auto warmupDeadline = nowUs() + 500000;
                while (publication && !publication->sampleReady &&
                       publication->fault == AudioPublication::Fault::none &&
                       !publication->serviceError && nowUs() < warmupDeadline) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                    publication = host->audioPublication();
                }
                if (!publication)
                    return core::unexpected(publication.error());
                if (publication->fault != AudioPublication::Fault::none ||
                    publication->serviceError)
                    return core::unexpected(
                        publication->serviceError.value_or(error("Audio owner fault")));
                if (!publication->sequence || !publication->clock.sampleRate ||
                    !publication->sampleReady)
                    return core::unexpected(error("Audio clock is not ready"));
                if (logicalSegment != context.clockSegment) {
                    logicalSegment = context.clockSegment;
                    backendSegment = publication->clock.source.discontinuityId;
                    underruns = publication->metrics.underrunCount;
                    backendRate = publication->clock.sampleRate;
                }
                value.publicationSequence = publication->sequence;
                value.sampleMonotonicUs = publication->sampleMonotonicUs;
                value.presented = ClockFramePosition{publication->clock.presentedFrame,
                                                     publication->clock.sampleRate};
                if (publication->maximumPresentedFrame)
                    value.maximumPresented = ClockFramePosition{*publication->maximumPresentedFrame,
                                                                publication->clock.sampleRate};
                value.source = value.presented;
                value.deviceChanged = backendSegment != publication->clock.source.discontinuityId;
                value.underrun = publication->metrics.underrunCount != underruns;
                value.rateChanged = backendRate != publication->clock.sampleRate;
                value.playback = publication->clock.source.state == audio::PlaybackState::Playing
                                     ? ClockPlaybackState::Playing
                                 : publication->clock.source.state == audio::PlaybackState::Ended
                                     ? ClockPlaybackState::EndOfStream
                                 : publication->clock.source.state == audio::PlaybackState::Error
                                     ? ClockPlaybackState::Failed
                                     : ClockPlaybackState::Paused;
                value.errorBoundUs = std::max<std::int64_t>(20000, publication->errorBoundUs);
                value.quality = ClockQuality::Estimated;
            } else {
                value.publicationSequence = ++noAudioSequence;
                value.sampleMonotonicUs = nowUs();
                auto frozen = pausedPositionUs;
                if (host->state() == HostState::controlPreparing && host->pausedCut()) {
                    const auto cut = *host->pausedCut();
                    if ((cut > 0 && app.profile.outputCorrectionUs > INT64_MAX - cut) ||
                        (cut < 0 && app.profile.outputCorrectionUs < INT64_MIN - cut))
                        return core::unexpected(error("Paused clock position overflow"));
                    frozen = cut + app.profile.outputCorrectionUs;
                }
                const auto position =
                    frozen.value_or(anchorPositionUs + value.sampleMonotonicUs - anchorUs);
                value.presented = ClockFramePosition{position, 1000000};
                value.source = value.presented;
                value.playback = frozen ? ClockPlaybackState::Paused : ClockPlaybackState::Playing;
                value.errorBoundUs = 0;
                value.quality = ClockQuality::Exact;
            }
            value.validUntilMonotonicUs = value.sampleMonotonicUs + 500000;
            return value;
        } catch (...) {
            return core::unexpected(error("Clock observation failed"));
        }
    };
    auto pumpMain = [&]() -> core::Result<void> {
        try {
            auto input = take(surface->pollInput());
            quitting |= input.quitRequested;
            if (input.focusLost) {
                focusPauseRequested = true;
                logger.info("player.execution.focus",
                            "Focus lost; reliable Pause requested; no synthetic key releases");
            }
            for (const auto action : input.actions) {
                if (action == PlayerInputAction::Quit)
                    quitting = true;
                else if (actions.size() < 64)
                    actions.push_back(action);
                else
                    overflow = true;
            }
            if (!host || faulted || input.keys.empty() || !captureClock ||
                captureSegment != host->context().clockSegment ||
                host->state() == HostState::closing || host->state() == HostState::closed)
                return {};
            auto clock = host->tail() ? core::Result<ClockObservation>{host->tail()->eof}
                                      : observation(host->context());
            if (!clock) {
                faulted = true;
                fault = clock.error();
                return {};
            }
            if (!captureClock || captureSegment != host->context().clockSegment)
                return {};
            // Capture has its own clock replica. Pumping during an owner reply must not advance
            // the coordinator's publication cursor past the observation that reply is using.
            auto ticks = host->tail() ? host->inputTicks(*clock)
                         : clock->playback == ClockPlaybackState::Paused
                             ? captureClock->observePaused(*clock, nowUs())
                         : clock->playback == ClockPlaybackState::EndOfStream
                             ? captureClock->observeEof(*clock, nowUs())
                             : captureClock->sample(*clock, nowUs());
            if (!ticks) {
                faulted = true;
                fault = ticks.error();
                return {};
            }
            for (const auto& key : input.keys)
                for (const auto& mapped : profile.keys)
                    if (mapped.scanCode == key.scanCode) {
                        if (inputSequence == UINT64_MAX ||
                            key.timestampNs > static_cast<std::uint64_t>(INT64_MAX)) {
                            overflow = true;
                            continue;
                        }
                        GameplayInput event{{ticks->observationTick},
                                            ++inputSequence,
                                            key.pressed ? GameplayInputAction::Press
                                                        : GameplayInputAction::Release,
                                            mapped.channel,
                                            mapped.domain,
                                            profile.configuration.sourceClass,
                                            {}};
                        event.rawTimestamps.device = static_cast<std::int64_t>(key.timestampNs);
                        event.rawTimestamps.hostArrival =
                            std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
                        if (host->hasAudio() && !host->tail())
                            event.rawTimestamps.audioFrame = clock->presented->frame;
                        auto captured = host->capture(std::move(event));
                        if (!captured) {
                            if (host->tail() &&
                                captured.error().code() == "host.rpa.control_pending") {
                                logger.info(
                                    "player.execution.tail.input_closed",
                                    "Captured key is outside the declared tail input window");
                                continue;
                            }
                            faulted = true;
                            fault = captured.error();
                        }
                    }
            return {};
        } catch (...) {
            return core::unexpected(error("Main event pump failed"));
        }
    };
    ExecutionHostPorts ports{
        makeExecutionAudioFactory(app.profile, static_cast<float>(app.app.requested.gain)),
        audio_sdl::WavDecoder::decode, pumpMain};
    auto load = [&]() {
        host = take(ExecutionHost::create(*backend, hostOptions, ports));
        std::optional<GameplayContent> gameplay;
        if (options.gameplayPacked)
            gameplay.emplace(
                take(readPlayerGameplayContent(*options.gameplayPacked, profile.configuration)));
        take(host->load(take(openConfiguredPlaybackSource(options)), std::move(gameplay)));
        captureClock.emplace(take(
            RealtimeClock::create(scheduling, mapping, "steady.us", host->context().clockSegment)));
        captureSegment = host->context().clockSegment;
        anchorUs = nowUs();
        anchorPositionUs = app.profile.outputCorrectionUs;
        pausedPositionUs.reset();
        logicalSegment = 0;
        seekHistory.reset();
        previousPresentationTick.reset();
        scoreCursor.reset();
        // Commit the first Runtime frame before controls can freeze a freshly loaded session.
        // Otherwise an immediate Pause (including paused R restart) has no frame to retain.
        const auto firstFrameDeadline = nowUs() + 5000000;
        for (;;) {
            const auto clock = take(observation(host->context()));
            const auto decision = take(host->advance(clock));
            if (decision.disposition == GameplayAdvanceDisposition::Executed) {
                const auto ticks = take(host->inputTicks(clock));
                const auto frame =
                    take(host->sample(ticks.presentationTick,
                                      {static_cast<double>(ticks.presentationTick) / 1000.0, 0.0,
                                       host->context().clockSegment},
                                      false));
                const auto receipt = take(host->present(frame));
                if (receipt.status != PresentStatus::succeeded)
                    throw Failure{error("Initial present was not confirmed")};
                previousPresentationTick = ticks.presentationTick;
                previousPresentationSegment = host->context().clockSegment;
                break;
            }
            if (decision.disposition == GameplayAdvanceDisposition::Failed ||
                decision.disposition == GameplayAdvanceDisposition::ClockInvalidNotExecuted ||
                nowUs() >= firstFrameDeadline)
                throw Failure{decision.error.value_or(error("Initial frame did not execute"))};
            take(pumpMain());
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    };
    load();
    const auto pause = [&] {
        take(host->pause(observation));
        logger.info("player.execution.control",
                    "Paused at cut=" + std::to_string(host->pausedCut().value_or(0)));
        if (!host->hasAudio()) {
            const auto cut = host->pausedCut();
            if (!cut)
                throw Failure{error("Pause did not publish its actual cut")};
            if ((*cut > 0 && app.profile.outputCorrectionUs > INT64_MAX - *cut) ||
                (*cut < 0 && app.profile.outputCorrectionUs < INT64_MIN - *cut))
                throw Failure{error("Paused physical position overflow")};
            pausedPositionUs = *cut + app.profile.outputCorrectionUs;
        }
    };
    while (!quitting && !faulted && !overflow) {
        take(pumpMain());
        if (faulted || overflow || quitting)
            break;
        const auto currentDrawable = take(surface->drawableSize());
        if (currentDrawable.width > 0 && currentDrawable.height > 0 &&
            (currentDrawable.width != drawable.width ||
             currentDrawable.height != drawable.height)) {
            take(host->resize({static_cast<std::uint32_t>(currentDrawable.width),
                               static_cast<std::uint32_t>(currentDrawable.height)}));
            drawable = currentDrawable;
        }
        auto clock = host->tail() ? host->tail()->eof : take(observation(host->context()));
        if (clock.playback == ClockPlaybackState::EndOfStream && !host->tail()) {
            if (!options.executionTailDeadline)
                throw Failure{core::Error{"host.rpa.profile_incomplete",
                                          "EOF requires explicit --execution-tail-input-deadline"}};
            std::int64_t deadline{};
            const auto& text = *options.executionTailDeadline;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), deadline);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
                throw Failure{error("Invalid tail input deadline")};
            take(host->beginTail(clock,
                                 {{1, 1}, deadline, 20000, "B5 explicit test-only tail", true}));
            captureSegment = host->context().clockSegment;
        }
        if (focusPauseRequested) {
            focusPauseRequested = false;
            if (host->state() == HostState::running) {
                pause();
            }
            continue;
        }
        if (!actions.empty()) {
            const auto action = actions.front();
            actions.pop_front();
            if (action == PlayerInputAction::PlayPause) {
                if (host->state() == HostState::running) {
                    pause();
                } else if (host->state() == HostState::paused) {
                    if (pausedPositionUs) {
                        anchorPositionUs = *pausedPositionUs;
                        anchorUs = nowUs();
                        pausedPositionUs.reset();
                    }
                    take(host->resume(observation));
                    captureClock.emplace(take(RealtimeClock::create(
                        scheduling, mapping, "steady.us", host->context().clockSegment)));
                    captureSegment = host->context().clockSegment;
                    logger.info("player.execution.control",
                                "Resumed segment=" + std::to_string(captureSegment));
                }
            } else if (action == PlayerInputAction::Reload) {
                const bool keepPaused = host->state() == HostState::paused;
                take(host->close());
                host.reset();
                backend.reset();
                backend.emplace(take(createPlayerBackend(runtime, window, app.app.requested.vsync,
                                                         options.shaderCacheDirectory, logger)));
                load();
                if (keepPaused)
                    pause();
                logger.info("player.execution.reload",
                            std::string{"RestartAtZero completed; state="} +
                                (host->state() == HostState::paused ? "paused" : "running"));
            } else if (action == PlayerInputAction::Stop) {
                break;
            } else if (action == PlayerInputAction::SeekBackward ||
                       action == PlayerInputAction::SeekForward) {
                auto ticks = clock.playback == ClockPlaybackState::Paused
                                 ? take(captureClock->observePaused(clock, nowUs()))
                                 : take(captureClock->sample(clock, nowUs()));
                if (host->state() == HostState::running)
                    pause();
                if (!seekHistory || ticks.horizon > seekHistoryHorizon) {
                    seekHistory = take(host->archive());
                    seekHistoryHorizon = ticks.horizon;
                }
                auto replay = *seekHistory;
                // Historical forward seeks cannot invent uncaptured future evidence.
                const auto horizon = action == PlayerInputAction::SeekBackward
                                         ? std::max<std::int64_t>(0, ticks.horizon - 5000000)
                                         : std::min(seekHistoryHorizon, ticks.horizon + 5000000);
                auto cut = take(host->replayCut(replay, {horizon}));
                anchorPositionUs = horizon + app.profile.outputCorrectionUs;
                anchorUs = nowUs();
                pausedPositionUs.reset();
                take(host->seek(replay, cut, {horizon}, observation));
                captureClock.emplace(take(RealtimeClock::create(scheduling, mapping, "steady.us",
                                                                host->context().clockSegment)));
                captureSegment = host->context().clockSegment;
            } else {
                logger.info("player.execution.control",
                            "Seek/Rebuild require explicit archive cut or device recovery; "
                            "ordinary playback stopped");
                faulted = true;
                fault = error("This control requires an explicit replay cut");
            }
            continue;
        }
        auto ticks = take(host->inputTicks(clock));
        if (host->state() == HostState::running) {
            auto result = host->advance(clock);
            if (!result && result.error().code() == "host.rpa.control_pending" &&
                host->state() == HostState::running)
                continue;
            auto decision = take(std::move(result));
            if (host->tail() && decision.disposition == GameplayAdvanceDisposition::Executed &&
                decision.request.horizon.value >= host->tail()->terminalHorizon) {
                logger.info("player.execution.tail.complete",
                            "Declared terminal horizon actually executed");
                break;
            }
            if (decision.disposition == GameplayAdvanceDisposition::Failed ||
                decision.disposition == GameplayAdvanceDisposition::ClockInvalidNotExecuted)
                throw Failure{decision.error.value_or(error("Execution advance failed"))};
        }
        const auto delta =
            previousPresentationTick && previousPresentationSegment == host->context().clockSegment
                ? static_cast<double>(ticks.presentationTick - *previousPresentationTick) / 1000.0
                : 0.0;
        auto sampled = host->sample(ticks.presentationTick,
                                    {static_cast<double>(ticks.presentationTick) / 1000.0, delta,
                                     host->context().clockSegment},
                                    host->state() == HostState::paused);
        if (!sampled && sampled.error().code() == "host.rpa.control_pending" &&
            host->state() == HostState::running)
            continue;
        auto frame = take(std::move(sampled));
        previousPresentationTick = ticks.presentationTick;
        previousPresentationSegment = host->context().clockSegment;
        if (!scoreCursor || *scoreCursor != frame.foldedFactCursor) {
            const auto result = take(host->queryGameplay());
            const auto score = take(result.score());
            logger.info("player.execution.judgement",
                        "score=" + std::to_string(score.score) + " combo=" +
                            std::to_string(score.combo) + " hits=" + std::to_string(score.hits) +
                            " misses=" + std::to_string(score.misses) +
                            " factCursor=" + std::to_string(frame.foldedFactCursor));
            scoreCursor = frame.foldedFactCursor;
        }
        auto presented = take(host->present(frame));
        if (presented.status != PresentStatus::succeeded)
            throw Failure{error("Present was not confirmed")};
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    take(host->close());
    if (overflow)
        return core::unexpected(
            error("Deferred event capacity exceeded; ordinary capture stopped"));
    if (faulted)
        return core::unexpected(fault.value_or(error("Execution is suspended")));
    return {};
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("Execution Player allocation failed"));
}
#endif
} // namespace cuexis::player
