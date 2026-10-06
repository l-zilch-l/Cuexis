#include "host_runner.hpp"

#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis_reference_host {
namespace {

using cuexis::playback::FrameSnapshot;
using cuexis::playback::FrameViewport;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PreparedSemanticIdentity;
using cuexis::playback::ReloadPolicy;
using cuexis::playback::RuntimeFrame;
using cuexis::playback::SessionState;

// The host clock script. The host owns chart time, so it advances or jumps the
// clock itself and bumps the discontinuity id on every jump.
struct ClockStep final {
    double chartTimeMs{};
    double simulationDeltaTimeMs{};
    std::uint64_t discontinuityId{};
};

constexpr std::array<ClockStep, 4> scriptedSteps{
    ClockStep{.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 0},
    ClockStep{.chartTimeMs = 625.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 1},
    ClockStep{.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 2},
    ClockStep{.chartTimeMs = 1250.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 3},
};

[[nodiscard]] auto stateName(SessionState state) -> std::string_view {
    switch (state) {
    case SessionState::Empty:
        return "Empty";
    case SessionState::Ready:
        return "Ready";
    case SessionState::Running:
        return "Running";
    case SessionState::Failed:
        return "Failed";
    }
    return "Unknown";
}

[[nodiscard]] auto identityText(const PreparedSemanticIdentity& identity) -> std::string {
    return hexIdentity(identity.sha256.data(), identity.sha256.size());
}

struct FrameObservation final {
    RuntimeFrame frame;
    std::uint64_t digest{};
    std::size_t objects{};
    bool seek{false};
};

// Command-mode extras for one step. Legacy passes nothing, so its frame lines and
// its failure lines stay exactly what they were; section 3.6 appends the two new
// fields only after algorithm=3 and only for a user program.
struct CommandStep final {
    std::uint64_t commandIndex{0};
    // The diagnostic step for a refused step. "advance" is not one of the frozen
    // step names, so a command-mode failure names the verb that asked for it.
    std::string_view diagnosticStep;
};

void reportStepFailure(HostReport& report, std::string_view step, std::string_view code,
                       const CommandStep* command) {
    if (command == nullptr) {
        report.failure(step, code);
        return;
    }
    report.diagnostic(command->diagnosticStep, code, "the session refused the step");
}

// Consumes one host clock step: updates the SDK, extracts the frame and records
// the digest. The host decides when a step is an advance and when it is a seek.
[[nodiscard]] auto consumeStep(PlaybackSession& session, const ClockStep& step, bool seek,
                               HostReport& report, std::size_t index,
                               const CommandStep* command = nullptr)
    -> std::optional<FrameObservation> {
    const RuntimeFrame frame{.chartTimeMs = step.chartTimeMs,
                             .simulationDeltaTimeMs = step.simulationDeltaTimeMs,
                             .timeDiscontinuityId = step.discontinuityId};
    const auto stepName = seek ? "seek" : "advance";
    auto updated = session.update(frame);
    if (!updated) {
        reportStepFailure(report, stepName, updated.error().code(), command);
        return std::nullopt;
    }
    auto snapshot = session.extractFrame(FrameViewport{.width = 1280, .height = 720});
    if (!snapshot) {
        reportStepFailure(report, stepName, snapshot.error().code(), command);
        return std::nullopt;
    }
    auto digest = cuexis::playback::computeFrameDigest(frame, *snapshot);
    if (!digest || digest->algorithmVersion != 3U) {
        reportStepFailure(report, stepName, "frame digest unavailable", command);
        return std::nullopt;
    }
    std::string fields =
        std::string{"index="} + std::to_string(index) + " mode=" + std::string{stepName} +
        " chartTimeMs=" + std::to_string(static_cast<std::int64_t>(step.chartTimeMs)) +
        " discontinuityId=" + std::to_string(step.discontinuityId) +
        " objects=" + std::to_string(snapshot->objects.size()) +
        " digest=" + std::to_string(digest->value) +
        " algorithm=" + std::to_string(digest->algorithmVersion);
    if (command != nullptr) {
        fields += " cmdIndex=" + std::to_string(command->commandIndex) + " simulationDeltaTimeMs=" +
                  std::to_string(static_cast<std::int64_t>(step.simulationDeltaTimeMs));
    }
    report.event("frame", fields);
    return FrameObservation{
        .frame = frame, .digest = digest->value, .objects = snapshot->objects.size(), .seek = seek};
}

[[nodiscard]] auto activeIdentity(PlaybackSession& session, HostReport& report,
                                  std::string_view step) -> std::optional<std::string> {
    auto identity = session.semanticIdentity();
    if (!identity) {
        report.failure(step, identity.error().code());
        return std::nullopt;
    }
    return identityText(*identity);
}

[[nodiscard]] auto currentState(PlaybackSession& session, HostReport& report, std::string_view step)
    -> std::optional<SessionState> {
    auto state = session.state();
    if (!state) {
        report.failure(step, state.error().code());
        return std::nullopt;
    }
    return *state;
}

// Runs the whole frame script once and returns the observations in order.
[[nodiscard]] auto runFrameScript(PlaybackSession& session, const HostOptions& options,
                                  HostReport& report, std::string_view phase)
    -> std::optional<std::vector<FrameObservation>> {
    std::vector<FrameObservation> observations;
    observations.reserve(scriptedSteps.size() + options.advanceFrames);
    const std::uint64_t discontinuityId = scriptedSteps.back().discontinuityId;
    double chartTimeMs = scriptedSteps.back().chartTimeMs;
    std::size_t index = 0;
    for (const auto& step : scriptedSteps) {
        const bool seek =
            index > 0U && step.discontinuityId != scriptedSteps[index - 1U].discontinuityId;
        auto observation = consumeStep(session, step, seek, report, index);
        if (!observation) {
            report.failure(phase, "frame script failed");
            return std::nullopt;
        }
        observations.push_back(*observation);
        ++index;
    }
    // Host-owned advance loop: the host keeps its own monotonic clock, stays
    // inside the current discontinuity and feeds a real simulation delta.
    for (std::size_t extra = 0; extra < options.advanceFrames; ++extra) {
        chartTimeMs += 250.0;
        const ClockStep step{.chartTimeMs = chartTimeMs,
                             .simulationDeltaTimeMs = 250.0,
                             .discontinuityId = discontinuityId};
        auto observation = consumeStep(session, step, false, report, index);
        if (!observation) {
            report.failure(phase, "advance loop failed");
            return std::nullopt;
        }
        observations.push_back(*observation);
        ++index;
    }
    return observations;
}

[[nodiscard]] auto sameDigests(const std::vector<FrameObservation>& left,
                               const std::vector<FrameObservation>& right) -> bool {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].digest != right[index].digest) {
            return false;
        }
    }
    return true;
}

// --- Command mode (R9 sections 3.3, 3.4 and 5) ------------------------------

// The host's own context. It exists so that a user program's pause really changes
// what the next tick does: the loop dispatches command by command against one
// live context rather than re-entering the legacy sequence per command.
struct HostContext final {
    PlaybackSession session;
    HostContent content;
    Transport transport{Transport::Empty};
    Clock clock;
    Counters counters;
    // The frame most recently submitted through a real update. reload() hands
    // exactly this frame to prepareReload, which is what makes H4.1 measurable;
    // it is empty until the first successful update.
    std::optional<FrameObservation> lastSample;
    std::optional<std::string> identity;
    std::size_t frameIndex{0};
    bool hasSuccessfulUpdate{false};
};

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
    auto observation =
        consumeStep(context.session, step, seek, report, context.frameIndex, &command);
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
        context.transport = Transport::Playing;
        emitPlayPause(report, "play", state, context.clock);
        emitCommand(report, index, command.line, command.verb, false, before, context.transport);
        return true;
    }
    case Verb::Pause: {
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

} // namespace

auto runHost(const HostOptions& options, HostReport& report) -> int {
    report.event("build", std::string{"flavor="} + CUEXIS_HOST_BUILD_FLAVOR);
    if (options.commandProgram) {
        return runCommandProgram(options, report, *options.commandProgram);
    }
    if (options.contentDirectory.empty()) {
        report.failure("start", "content directory is required");
        return 1;
    }
    HostContent content{.projectDirectory = options.contentDirectory,
                        .candidateEntry = options.candidateEntry};

    report.event("start", std::string{"sdk_api_baseline=0.7.1 content="} +
                              options.contentDirectory.filename().string() +
                              (options.packageFile ? " package=yes" : " package=no"));

    PlaybackSession session;
    auto state = currentState(session, report, "start");
    if (!state || *state != SessionState::Empty) {
        report.failure("start", "fresh session is not Empty");
        return 1;
    }

    // 1. Load the host project content through the host content provider.
    auto source = buildProjectSource(content, report, false);
    if (!source) {
        report.failure("load", source.error().code());
        return 1;
    }
    auto prepared = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
    if (!prepared) {
        report.failure("load", prepared.error().code());
        return 1;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    const auto* manifest = prepared->presentationManifest();
    if (!candidateIdentity || manifest == nullptr) {
        report.failure("load", "candidate is incomplete");
        return 1;
    }
    report.event("load", std::string{"source=host-project identity="} +
                             identityText(*candidateIdentity) +
                             " presentation_entries=" + std::to_string(manifest->entries.size()));
    auto committed = session.commit(std::move(*prepared));
    if (!committed) {
        report.failure("commit", committed.error().code());
        return 1;
    }
    auto active = activeIdentity(session, report, "commit");
    if (!active) {
        return 1;
    }
    const auto sessionIdentity = *active;
    auto commitState = currentState(session, report, "commit");
    if (!commitState) {
        return 1;
    }
    report.event("commit", std::string{"state="} + std::string{stateName(*commitState)} +
                               " identity=" + sessionIdentity);

    // 2. Frames, seeks and digests on the host clock.
    auto firstObservations = runFrameScript(session, options, report, "frames");
    if (!firstObservations) {
        return 1;
    }
    for (const auto& [index, expected] : options.expectedDigests) {
        if (index >= firstObservations->size()) {
            report.failure("frames", "expected digest index is out of range");
            return 1;
        }
        if ((*firstObservations)[index].digest != expected) {
            report.failure("frames", std::string{"digest mismatch at index "} +
                                         std::to_string(index) + ": expected " +
                                         std::to_string(expected) + " observed " +
                                         std::to_string((*firstObservations)[index].digest));
            return 1;
        }
        report.event("digest", std::string{"index="} + std::to_string(index) +
                                   " expected=" + std::to_string(expected) + " matched=yes");
    }

    // 3. Reload the same content. Chart time must be preserved and the observed
    //    frames must be identical.
    auto reloadSource = buildProjectSource(content, report, false);
    if (!reloadSource) {
        report.failure("reload", reloadSource.error().code());
        return 1;
    }
    auto reloaded = session.prepareReload(std::move(*reloadSource), firstObservations->back().frame,
                                          ReloadPolicy::KeepChartTime);
    if (!reloaded) {
        report.failure("reload", reloaded.error().code());
        return 1;
    }
    auto reloadCommitted = session.commit(std::move(*reloaded));
    if (!reloadCommitted) {
        report.failure("reload", reloadCommitted.error().code());
        return 1;
    }
    auto reloadIdentity = activeIdentity(session, report, "reload");
    if (!reloadIdentity || *reloadIdentity != sessionIdentity) {
        report.failure("reload", "active identity changed across a successful reload");
        return 1;
    }
    auto secondObservations = runFrameScript(session, options, report, "reload-frames");
    if (!secondObservations || !sameDigests(*firstObservations, *secondObservations)) {
        report.failure("reload", "reload changed the observed frames");
        return 1;
    }
    auto reloadState = currentState(session, report, "reload");
    if (!reloadState) {
        return 1;
    }
    report.event("reload", std::string{"outcome=ok identity="} + sessionIdentity + " state=" +
                               std::string{stateName(*reloadState)} + " frames_identical=yes");

    // 4. A rejected reload must not disturb active content.
    const auto identityBeforeFailure = sessionIdentity;
    const auto digestBeforeFailure = secondObservations->back().digest;
    auto faultySource = buildProjectSource(content, report, true);
    if (!faultySource) {
        report.failure("reload-failed", faultySource.error().code());
        return 1;
    }
    auto rejected = session.prepareReload(
        std::move(*faultySource), secondObservations->back().frame, ReloadPolicy::KeepChartTime);
    if (rejected) {
        report.failure("reload-failed", "a host provider failure was accepted as a candidate");
        return 1;
    }
    report.rejection("reload-failed", rejected.error().code());
    auto identityAfterFailure = activeIdentity(session, report, "reload-failed");
    auto stateAfterFailure = currentState(session, report, "reload-failed");
    if (!identityAfterFailure || !stateAfterFailure ||
        *identityAfterFailure != identityBeforeFailure ||
        *stateAfterFailure == SessionState::Failed) {
        report.failure("reload-failed", "active content was disturbed by a rejected reload");
        return 1;
    }
    auto frameAfterFailure = session.extractFrame(FrameViewport{.width = 1280, .height = 720});
    if (!frameAfterFailure) {
        report.failure("reload-failed", frameAfterFailure.error().code());
        return 1;
    }
    auto digestAfterFailure =
        cuexis::playback::computeFrameDigest(secondObservations->back().frame, *frameAfterFailure);
    if (!digestAfterFailure || digestAfterFailure->value != digestBeforeFailure) {
        report.failure("reload-failed", "active frame changed after a rejected reload");
        return 1;
    }
    report.event("active", std::string{"identity="} + *identityAfterFailure +
                               " state=" + std::string{stateName(*stateAfterFailure)} + " digest=" +
                               std::to_string(digestAfterFailure->value) + " preserved=yes");

    // 5. Load the published package. A production host must observe the same
    //    identity and the same frames as it does for host-provided content.
    if (options.packageFile) {
        // Switching content is an explicit host decision: the host unloads and
        // then loads the published artifact.
        auto unloadedForPackage = session.unload();
        if (!unloadedForPackage) {
            report.failure("package", unloadedForPackage.error().code());
            return 1;
        }
        auto packageSource =
            options.candidateEntry
                ? PlaybackSource::fromCxcFileEntry(*options.packageFile, *options.candidateEntry)
                : PlaybackSource::fromCxcFile(*options.packageFile);
        if (!packageSource) {
            report.failure("package", packageSource.error().code());
            return 1;
        }
        auto packagePrepared =
            session.prepareLoad(std::move(*packageSource), PlaybackMode::ChartClock);
        if (!packagePrepared) {
            report.failure("package", packagePrepared.error().code());
            return 1;
        }
        const auto packageIdentity = packagePrepared->semanticIdentity();
        if (!packageIdentity) {
            report.failure("package", "published package candidate is incomplete");
            return 1;
        }
        if (identityText(*packageIdentity) != sessionIdentity) {
            report.failure("package", "published package identity differs from host content");
            return 1;
        }
        auto packageCommitted = session.commit(std::move(*packagePrepared));
        if (!packageCommitted) {
            report.failure("package", packageCommitted.error().code());
            return 1;
        }
        auto packageObservations = runFrameScript(session, options, report, "package-frames");
        if (!packageObservations || !sameDigests(*firstObservations, *packageObservations)) {
            report.failure("package", "published package changed the observed frames");
            return 1;
        }
        report.event("package", std::string{"outcome=ok identity="} + sessionIdentity +
                                    " identity_matches_host_content=yes frames_identical=yes");
    }

    // 6. Destroy: the host unloads and releases the session.
    auto unloaded = session.unload();
    if (!unloaded) {
        report.failure("destroy", unloaded.error().code());
        return 1;
    }
    auto finalState = currentState(session, report, "destroy");
    if (!finalState || *finalState != SessionState::Empty) {
        report.failure("destroy", "session did not return to Empty");
        return 1;
    }
    report.event("destroy", "state=Empty resources_released=yes");

    if (options.expectedIdentity && *options.expectedIdentity != sessionIdentity) {
        report.failure("verify", std::string{"identity mismatch: expected "} +
                                     *options.expectedIdentity + " observed " + sessionIdentity);
    } else if (options.expectedIdentity) {
        report.event("verify", std::string{"expected_identity="} + *options.expectedIdentity +
                                   " matched=yes");
    }
    return report.ok() ? 0 : 1;
}

} // namespace cuexis_reference_host
