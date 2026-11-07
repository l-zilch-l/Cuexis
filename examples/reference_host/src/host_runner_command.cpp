// Reference host command mode: one live context dispatched command by command (R9 3.3, 3.4, 5).

#include "host_realtime.hpp"

#include "host_runner.hpp"
#include "host_runner_internal.hpp"

#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis_reference_host {

using cuexis::playback::FrameSnapshot;
using cuexis::playback::FrameViewport;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PreparedSemanticIdentity;
using cuexis::playback::ReloadPolicy;
using cuexis::playback::RuntimeFrame;
using cuexis::playback::SessionState;
// --- Command mode (R9 sections 3.3, 3.4 and 5) ------------------------------

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
[[nodiscard]] auto gameplayControl(HostContext& context, HostReport& report,
                                   cuexis::playback::GameplayControl control, std::string_view step)
    -> bool {
    if (!context.content.gameplay)
        return true;
    auto result = context.session.controlGameplay(control);
    if (result)
        return true;
    report.diagnostic(step, result.error().code(), "the Gameplay control was refused");
    return false;
}

[[nodiscard]] auto gameplaySample(HostContext& context, HostReport& report, bool seek,
                                  const RuntimeFrame& frame) -> bool {
    using namespace cuexis::playback;
    const auto fail = [&](std::string_view code) {
        report.diagnostic(seek ? "seek" : "tick", code, "the Gameplay step was refused");
        return false;
    };
    auto nextH = context.gameplayH;
    auto nextT = context.gameplayT;
    if (seek) {
        // The command parser admits only nonnegative exactly representable integers.
        nextH = static_cast<std::int64_t>(context.clock.chartTimeMs);
        nextT = nextH;
        auto archive = context.session.archiveGameplay();
        if (!archive)
            return fail(archive.error().code());
        auto cut = archive->cutAt({nextH});
        if (!cut)
            return fail(cut.error().code());
        auto restored = context.session.seekGameplay(*archive, {nextH}, *cut, {}, {nextT}, frame);
        if (!restored)
            return fail(restored.error().code());
        if (context.transport == Transport::Paused &&
            !gameplayControl(context, report, GameplayControl::Pause, "seek"))
            return false;
    } else {
        const auto& gameplay = *context.content.gameplay;
        if (nextH > std::numeric_limits<std::int64_t>::max() - gameplay.horizonStep ||
            nextT > std::numeric_limits<std::int64_t>::max() - gameplay.presentationStep)
            return fail(diagnostic::clockOverflow);
        nextH += gameplay.horizonStep;
        nextT += gameplay.presentationStep;
        if (!context.gameplayInputsSubmitted) {
            auto submitted = context.session.submitGameplay(gameplay.observations);
            if (!submitted)
                return fail(submitted.error().code());
            context.gameplayInputsSubmitted = true;
        }
        auto advanced = context.session.advanceGameplay({nextH}, {nextT}, frame);
        if (!advanced)
            return fail(advanced.error().code());
        auto receipt = context.session.gameplayAdvanceReceipt();
        if (!receipt)
            return fail(receipt.error().code());
        if (receipt->error)
            return fail(receipt->error->code());
    }
    context.gameplayH = nextH;
    context.gameplayT = nextT;
    auto result = context.session.queryGameplay();
    auto archive = context.session.archiveGameplay();
    if (!result)
        return fail(result.error().code());
    if (!archive)
        return fail(archive.error().code());
    auto evaluation = context.session.evaluateGameplayReplay(*archive);
    if (!evaluation)
        return fail(evaluation.error().code());
    auto same = result->sameResult(evaluation->result);
    if (!same)
        return fail(same.error().code());
    if (!evaluation->evidenceValid || !*same)
        return fail(diagnostic::stateMismatch);
    auto score = result->score();
    auto count = result->factCount();
    if (!score)
        return fail(score.error().code());
    if (!count)
        return fail(count.error().code());
    report.event("gameplay",
                 "H=" + std::to_string(nextH) + " T=" + std::to_string(nextT) +
                     " facts=" + std::to_string(*count) + " score=" + std::to_string(score->score) +
                     " combo=" + std::to_string(score->combo) +
                     " hits=" + std::to_string(score->hits) +
                     " misses=" + std::to_string(score->misses) + " completeReplay=same");
    return true;
}
#endif

[[nodiscard]] auto hostState(HostContext& context, HostReport& report, std::string_view step)
    -> std::optional<SessionState> {
    auto state = context.session.state();
    if (!state) {
        report.diagnostic(step, state.error().code(), "the session state is unavailable");
        return std::nullopt;
    }
    return *state;
}

void emitCommand(HostReport& report, std::uint64_t index, std::size_t line, Verb verb,
                 bool rejected, Transport before, Transport after) {
    report.event("command", std::string{"index="} + std::to_string(index) + " line=" +
                                std::to_string(line) + " verb=" + std::string{verbName(verb)} +
                                " outcome=" + (rejected ? "rejected" : "ok") +
                                " transport=" + std::string{transportName(before)} + "->" +
                                std::string{transportName(after)});
}

// A command that needs a live session. A missing session is not the same as an
// inconsistent one, and neither is the same as a Failed session, so each is
// reported as what it actually is.
[[nodiscard]] auto requireSession(HostContext& context, HostReport& report, std::string_view step,
                                  SessionState& stateOut) -> bool {
    const auto state = hostState(context, report, step);
    if (!state) {
        return false;
    }
    stateOut = *state;
    if (stateOut == SessionState::Failed) {
        report.diagnostic(step, diagnostic::stateMismatch,
                          "the session is Failed and no further command is dispatched");
        return false;
    }
    if (context.transport == Transport::Terminated) {
        report.diagnostic(step, diagnostic::stateMismatch, "the session is already terminated");
        return false;
    }
    if (context.transport == Transport::Empty) {
        if (stateOut != SessionState::Empty) {
            report.diagnostic(step, diagnostic::stateMismatch,
                              "the host is Empty while the session is not");
            return false;
        }
        report.diagnostic(step, diagnostic::notOpen, "no session is open");
        return false;
    }
    if (stateOut == SessionState::Empty) {
        report.diagnostic(step, diagnostic::stateMismatch,
                          "the host holds a session while the session reports Empty");
        return false;
    }
    return true;
}

// Loads and commits host project content. The SDK sequence is the same one the
// legacy self-check uses: section 4 forbids keeping two copies of it.
[[nodiscard]] auto openContent(HostContext& context, HostReport& report,
                               const std::filesystem::path& projectDirectory) -> bool {
    context.content.projectDirectory = projectDirectory;
    auto source = buildProjectSource(context.content, report, false);
    if (!source) {
        report.diagnostic("load", source.error().code(),
                          "the host could not build a project source");
        return false;
    }
    auto prepared = context.session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
    if (!prepared) {
        report.diagnostic("load", prepared.error().code(),
                          "the session refused the prepared source");
        return false;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    const auto* manifest = prepared->presentationManifest();
    if (!candidateIdentity || manifest == nullptr) {
        report.diagnostic("load", diagnostic::stateMismatch, "the load candidate is incomplete");
        return false;
    }
    report.event("load", std::string{"source=host-project identity="} +
                             identityText(*candidateIdentity) +
                             " presentation_entries=" + std::to_string(manifest->entries.size()));
    auto committed = context.session.commit(std::move(*prepared));
    if (!committed) {
        report.diagnostic("load", committed.error().code(), "the session refused the commit");
        return false;
    }
    auto active = context.session.semanticIdentity();
    if (!active) {
        report.diagnostic("load", active.error().code(), "the committed session has no identity");
        return false;
    }
    context.identity = identityText(*active);
    context.transport = Transport::Paused;
    context.clock = Clock{};
    context.lastSample.reset();
    context.hasSuccessfulUpdate = false;
    context.frameIndex = 0;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    context.gameplayH = context.gameplayT = 0;
    context.gameplayInputsSubmitted = false;
    if (!gameplayControl(context, report, cuexis::playback::GameplayControl::Pause, "open"))
        return false;
#endif
    report.event("commit", std::string{"state="} + std::string{stateName(SessionState::Ready)} +
                               " identity=" + *context.identity);
    return true;
}

// One real update, extract and digest. The counters are recorded at the call
// sites so a failure cannot be hidden by not counting it.
[[nodiscard]] auto sampleCommand(HostContext& context, HostReport& report, bool seek,
                                 std::uint64_t commandIndex) -> bool {
    const CommandStep command{.commandIndex = commandIndex,
                              .diagnosticStep =
                                  seek ? std::string_view{"seek"} : std::string_view{"tick"}};
    const ClockStep step{.chartTimeMs = static_cast<double>(context.clock.chartTimeMs),
                         .simulationDeltaTimeMs = seek ? 0.0 : static_cast<double>(tickStepMs),
                         .discontinuityId = context.clock.discontinuityId};
    ++context.counters.publicUpdateAttempts;
    bool alreadyUpdated = false;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (context.content.gameplay) {
        const RuntimeFrame frame{step.chartTimeMs, step.simulationDeltaTimeMs,
                                 step.discontinuityId};
        if (!gameplaySample(context, report, seek, frame))
            return false;
        alreadyUpdated = true;
    }
#endif
    auto observation = consumeStep(context.session, step, seek, report, context.frameIndex,
                                   &command, alreadyUpdated);
    if (!observation) {
        return false;
    }
    ++context.counters.publicUpdateSuccesses;
    context.lastSample = *observation;
    context.hasSuccessfulUpdate = true;
    ++context.frameIndex;
    ++context.counters.frameCount;
    if (!seek) {
        ++context.counters.emittedTickFrames;
    }
    return true;
}

// The pause check reads the session and re-digests the frame it already holds
// with the frame it already submitted. It must not update once to "verify" that
// nothing advanced: section 3.6 forbids manufacturing the evidence.
[[nodiscard]] auto pauseCheck(HostContext& context, HostReport& report) -> bool {
    const auto state = hostState(context, report, "pause");
    if (!state) {
        return false;
    }
    std::string objects{"0"};
    std::string digest{"none"};
    bool sampled = false;
    if (context.hasSuccessfulUpdate && context.lastSample) {
        auto snapshot = context.session.extractFrame(FrameViewport{.width = 1280, .height = 720});
        if (!snapshot) {
            report.diagnostic("pause", snapshot.error().code(),
                              "the session refused the pause check extraction");
            return false;
        }
        auto value = cuexis::playback::computeFrameDigest(context.lastSample->frame, *snapshot);
        if (!value) {
            report.diagnostic("pause", diagnostic::stateMismatch,
                              "the pause check could not compute a digest");
            return false;
        }
        objects = std::to_string(snapshot->objects.size());
        digest = std::to_string(value->value);
        sampled = true;
    }
    report.event("observation",
                 std::string{"reason=pause-check state="} + std::string{stateName(*state)} +
                     " chartTimeMs=" + std::to_string(context.clock.chartTimeMs) +
                     " discontinuityId=" + std::to_string(context.clock.discontinuityId) +
                     " sampled=" + (sampled ? "yes" : "no") + " objects=" + objects +
                     " digest=" + digest);
    return true;
}

void emitPlayPause(HostReport& report, std::string_view name, SessionState state,
                   const Clock& clock) {
    report.event(name, std::string{"state="} + std::string{stateName(state)} +
                           " chartTimeMs=" + std::to_string(clock.chartTimeMs) +
                           " discontinuityId=" + std::to_string(clock.discontinuityId));
}

// Dispatches one command against the live context. Returns false when the run
// must stop; the diagnostic has already been recorded by then.
[[nodiscard]] auto dispatchCommand(HostContext& context, HostReport& report,
                                   const HostOptions& options, const Command& command,
                                   std::uint64_t index) -> bool {
    const auto before = context.transport;
    const auto reject = [&](std::string_view code, std::string_view detail) {
        emitCommand(report, index, command.line, command.verb, true, before, context.transport);
        report.diagnostic(verbName(command.verb), code, detail);
        return false;
    };

    if (command.verb == Verb::Quit) {
        // Unloading is the one verb that is legal with no session at all.
        if (context.transport != Transport::Empty) {
            auto unloaded = context.session.unload();
            if (!unloaded) {
                return reject(unloaded.error().code(), "the session refused the unload");
            }
            const auto state = hostState(context, report, "quit");
            if (!state) {
                return false;
            }
            if (*state != SessionState::Empty) {
                return reject(diagnostic::stateMismatch,
                              "the session did not return to Empty on unload");
            }
        }
        context.transport = Transport::Terminated;
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        report.event("destroy", "state=Empty resources_released=yes");
        return true;
    }

    SessionState state = SessionState::Empty;
    if (command.verb != Verb::Open) {
        if (!requireSession(context, report, verbName(command.verb), state)) {
            emitCommand(report, index, command.line, command.verb, true, before, context.transport);
            return false;
        }
    }

    switch (command.verb) {
    case Verb::Open: {
        if (context.transport != Transport::Empty) {
            return reject(diagnostic::alreadyOpen,
                          "the host already has an active session; the first one is unchanged");
        }
        const auto root = command.openPath.empty() ? options.contentDirectory : command.openPath;
        if (root.empty()) {
            return reject(diagnostic::notOpen,
                          "open without a path needs --content to supply a default root");
        }
        if (!openContent(context, report, root)) {
            emitCommand(report, index, command.line, command.verb, true, before, context.transport);
            return false;
        }
        // --expect-identity is checked against the first successful open, and a
        // missing open is an expectation failure rather than a skipped check.
        if (options.expectedIdentity && context.identity != options.expectedIdentity) {
            return reject(diagnostic::expectationFailed,
                          "the opened content identity does not match --expect-identity");
        }
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        return true;
    }
    case Verb::Play: {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (!gameplayControl(context, report, cuexis::playback::GameplayControl::Resume, "play"))
            return false;
#endif
        context.transport = Transport::Playing;
        emitPlayPause(report, "play", state, context.clock);
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        return true;
    }
    case Verb::Pause: {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (!gameplayControl(context, report, cuexis::playback::GameplayControl::Pause, "pause"))
            return false;
#endif
        if (!pauseCheck(context, report)) {
            emitCommand(report, index, command.line, command.verb, true, before, context.transport);
            return false;
        }
        context.transport = Transport::Paused;
        emitPlayPause(report, "pause", state, context.clock);
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        return true;
    }
    case Verb::Tick: {
        // The budget counts what was asked for, so a request made while paused is
        // counted even though it advances nothing.
        context.counters.tickAttempts += command.number;
        if (context.transport == Transport::Paused) {
            context.counters.suppressedTicks += command.number;
            emitCommand(report, index, command.line, command.verb, false, before,
                        context.transport);
            return true;
        }
        for (std::uint64_t step = 0; step < command.number; ++step) {
            if (!context.clock.advanceOneTick()) {
                return reject(diagnostic::clockOverflow,
                              "the host clock would leave the exactly representable range");
            }
            if (!sampleCommand(context, report, false, index)) {
                emitCommand(report, index, command.line, command.verb, true, before,
                            context.transport);
                return false;
            }
        }
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        return true;
    }
    case Verb::Seek: {
        // A seek that lands on the current time still bumps the discontinuity id:
        // it is a discontinuity in the sampling, not a change of position.
        if (!context.clock.applySeek(command.number)) {
            return reject(diagnostic::clockOverflow, "the discontinuity id would wrap");
        }
        if (!sampleCommand(context, report, true, index)) {
            emitCommand(report, index, command.line, command.verb, true, before, context.transport);
            return false;
        }
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        return true;
    }
    case Verb::Reload: {
        if (!context.hasSuccessfulUpdate || !context.lastSample) {
            return reject(diagnostic::noSample,
                          "reload needs a frame from a successful update, not from a read-only "
                          "extraction");
        }
        // Section 5: the frame handed to prepareReload is the one actually
        // submitted, so the second of two consecutive paused reloads really does
        // pass a zero delta rather than relying on the SDK to clear it.
        const auto target = context.lastSample->frame;
        auto source = buildProjectSource(context.content, report, false);
        if (!source) {
            return reject(source.error().code(), "the host could not rebuild the project source");
        }
        auto prepared =
            context.session.prepareReload(std::move(*source), target, ReloadPolicy::KeepChartTime);
        if (!prepared) {
            return reject(prepared.error().code(), "the session refused the reload candidate");
        }
        auto committed = context.session.commit(std::move(*prepared));
        if (!committed) {
            return reject(committed.error().code(), "the session refused the reload commit");
        }
        auto active = context.session.semanticIdentity();
        if (!active || !context.identity || identityText(*active) != *context.identity) {
            return reject(diagnostic::stateMismatch,
                          "the active identity changed across a successful reload");
        }
        const RuntimeFrame normalized{.chartTimeMs = target.chartTimeMs,
                                      .simulationDeltaTimeMs = 0.0,
                                      .timeDiscontinuityId = target.timeDiscontinuityId};
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        context.gameplayH = 0;
        context.gameplayInputsSubmitted = false;
        if (!gameplayControl(context, report,
                             context.transport == Transport::Paused
                                 ? cuexis::playback::GameplayControl::Pause
                                 : cuexis::playback::GameplayControl::Resume,
                             "reload"))
            return false;
#endif
        // No new frame event: reload continues the same sampling position, and a
        // later tick must compute its own delta rather than inherit this zero.
        context.lastSample = FrameObservation{.frame = normalized,
                                              .digest = context.lastSample->digest,
                                              .objects = context.lastSample->objects,
                                              .seek = false};
        report.event("reload-sample",
                     std::string{"cmdIndex="} + std::to_string(index) + " targetChartTimeMs=" +
                         std::to_string(static_cast<std::int64_t>(target.chartTimeMs)) +
                         " targetSimulationDeltaTimeMs=" +
                         std::to_string(static_cast<std::int64_t>(target.simulationDeltaTimeMs)) +
                         " targetDiscontinuityId=" + std::to_string(target.timeDiscontinuityId) +
                         " normalizedChartTimeMs=" +
                         std::to_string(static_cast<std::int64_t>(normalized.chartTimeMs)) +
                         " normalizedSimulationDeltaTimeMs=0 normalizedDiscontinuityId=" +
                         std::to_string(normalized.timeDiscontinuityId));
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        return true;
    }
    case Verb::Quit:
        break;
    }
    return false;
}

[[nodiscard]] auto runCommandProgram(const HostOptions& options, HostReport& report,
                                     const Program& program) -> int {
    HostContext context;
    context.content.candidateEntry = options.candidateEntry;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    context.content.gameplay = options.gameplay;
#endif
    report.event("start", std::string{"sdk_api_baseline=0.7.1 content="} +
                              (options.contentDirectory.empty()
                                   ? std::string{"none"}
                                   : options.contentDirectory.filename().string()) +
                              " package=no mode=command");

    // Reported once, after the loop, so a failure mid-program still shows how far
    // the run actually got. It is emitted even when the loop never started,
    // because the program itself was accepted.
    const auto emitClock = [&report](const Counters& counters) {
        report.event(
            "clock",
            std::string{"tickAttempts="} + std::to_string(counters.tickAttempts) +
                " suppressedTicks=" + std::to_string(counters.suppressedTicks) +
                " emittedTickFrames=" + std::to_string(counters.emittedTickFrames) +
                " publicUpdateAttempts=" + std::to_string(counters.publicUpdateAttempts) +
                " publicUpdateSuccesses=" + std::to_string(counters.publicUpdateSuccesses) +
                " frameCount=" + std::to_string(counters.frameCount));
    };

    // A fresh host is Empty, and that is the expected starting point rather than
    // a missing session. Only an inconsistent fresh state is a failure.
    const auto startState = hostState(context, report, "open");
    if (!startState || *startState != SessionState::Empty) {
        emitClock(context.counters);
        report.diagnostic("open", diagnostic::stateMismatch, "a fresh session is not Empty");
        return 1;
    }

    bool ok = true;
    std::uint64_t index = 0;
    for (const auto& command : program.commands) {
        if (!dispatchCommand(context, report, options, command, index)) {
            ok = false;
            break;
        }
        ++index;
    }

    emitClock(context.counters);

    if (ok && options.expectedIdentity && !context.identity) {
        // The identity check never runs when nothing was opened, which is itself
        // a failure rather than a silently skipped assertion.
        report.diagnostic("quit", diagnostic::expectationFailed,
                          "--expect-identity was given but no open succeeded");
        ok = false;
    }

    if (ok && context.transport != Transport::Terminated) {
        // A program always ends in quit; reaching here without one means the
        // loop stopped early, and the cleanup below still runs exactly once.
        report.diagnostic("quit", diagnostic::stateMismatch,
                          "the program ended without terminating the session");
        ok = false;
    }
    return ok && report.ok() ? 0 : 1;
}
} // namespace cuexis_reference_host
