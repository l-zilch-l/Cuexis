// RR-F508 session lifecycle: content unload keeps the accepted host control identity, the Close
// terminal clears it, a replaced content rejects the old proof, and a no-fence Close terminal is
// acknowledged only from the accepted identity plus the applied host step record.

#include "playback_candidate_internal.hpp"
#include "playback_gameplay_fixture.hpp"

#include <cuexis/playback/execution_host_access.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>
#include <cuexis/playback/frame_layout_access.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace {

using cuexis::playback::ClockFramePosition;
using cuexis::playback::ClockMappingConfiguration;
using cuexis::playback::ClockObservation;
using cuexis::playback::ClockPlaybackState;
using cuexis::playback::ClockQuality;
using cuexis::playback::controlFullyApplied;
using cuexis::playback::ControlKind;
using cuexis::playback::ControlReceipt;
using cuexis::playback::ControlRequest;
using cuexis::playback::ControlStatus;
using cuexis::playback::ExecutionFence;
using cuexis::playback::GameplayExecutionContext;
using cuexis::playback::HostContext;
using cuexis::playback::Participant;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PreparedSemanticIdentity;
using cuexis::playback::SessionState;
using cuexis::playback::StepPhase;
using cuexis::playback::StepReceipt;
using cuexis::playback::StepRequest;
using cuexis::playback::StepStatus;
using cuexis::playback::detail::CandidateMetadataAccess;
using cuexis::playback::detail::ExecutionHostAccess;
using cuexis::playback::detail::OwnerStepLedger;
using cuexis::playback::experimental::AspectRatio;
using cuexis::playback::experimental::FrameFraming;

namespace fixture = playback_gameplay_fixture;

constexpr auto softwareIndex = static_cast<std::size_t>(Participant::software);
constexpr auto retirePhase = static_cast<std::size_t>(StepPhase::retire);
constexpr auto closePhase = static_cast<std::size_t>(StepPhase::close);

[[nodiscard]] auto clockMapping() -> ClockMappingConfiguration {
    return ClockMappingConfiguration{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
}

// A second, independently identified chart for the content-replacement guard.
constexpr std::string_view replacementChart = R"json({
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000301","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
            "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
  "templates":[],"behaviors":[],
  "objects":[{"id":"019b0000-0000-7abc-8def-000000000310","parent":null,
    "components":{"cuexis.transform":{"version":1,"position":[0,0,0],
                  "rotation":[0,0,0,1],"scale":[1,1,1]}},"extensions":{}}],
  "requiredExtensions":[],"extensions":{}
})json";

#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
[[nodiscard]] auto framedTable() -> std::array<FrameFraming, 2> {
    return {FrameFraming{AspectRatio{16, 9}, 1.0}, FrameFraming{AspectRatio{9, 16}, 1.0}};
}
#endif

[[nodiscard]] auto retireStep(const ControlRequest& request) -> StepRequest {
    return StepRequest{{request.context.sessionScope, request.operationId, Participant::software,
                        StepPhase::retire, 0},
                       request.target};
}

[[nodiscard]] auto preparingProof(const ControlRequest& request, std::uint64_t decision)
    -> ControlReceipt {
    ControlReceipt proof;
    proof.request = request;
    proof.decisionOrdinal = decision;
    proof.status = ControlStatus::preparing;
    return proof;
}

[[nodiscard]] auto appliedStep(const ControlRequest& request, StepPhase phase) -> StepReceipt {
    StepReceipt receipt;
    receipt.request = StepRequest{
        {request.context.sessionScope, request.operationId, Participant::software, phase, 0},
        request.target};
    receipt.status = StepStatus::applied;
    return receipt;
}

[[nodiscard]] auto completedCloseProof(const ControlRequest& request, std::uint64_t decision)
    -> ControlReceipt {
    ControlReceipt proof = preparingProof(request, decision);
    proof.status = ControlStatus::complete;
    proof.cleanupComplete = true;
    proof.softwareApplied = true;
    proof.steps[softwareIndex][retirePhase] = appliedStep(request, StepPhase::retire);
    proof.steps[softwareIndex][retirePhase]->effects.referencesReleased = true;
    proof.steps[softwareIndex][closePhase] = appliedStep(request, StepPhase::close);
    return proof;
}

void loadPresentation(PlaybackSession& session, bool withFramings) {
    auto prepared = session.prepareLoad(fixture::presentation(), PlaybackMode::ChartClock);
    REQUIRE(prepared);
#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
    // The committed framing table is seeded through the private library test seam, which shared
    // Playback builds do not export. The projection-clearing assertions below stay conditional for
    // the same reason.
    if (withFramings) {
        const auto table = framedTable();
        REQUIRE(cuexis::playback::detail::FrameLayoutAccess::attachFramings(*prepared, table));
    }
#else
    static_cast<void>(withFramings);
#endif
    REQUIRE(session.commit(std::move(*prepared)));
}

// The RPA profile and the exact calibration identity the clock mapping needs, built from the same
// assembled fixture the execution tests use.
void configureRealtimeContent(fixture::TestContent& content,
                              const ClockMappingConfiguration& clock) {
    auto& assembly = content.request.assembly;
    assembly.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
    assembly.identityDeclarations.engine.executionProfileToken = assembly.executionProfile;
    assembly.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
    assembly.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    assembly.capabilityContext = {{"cuexis.gameplay.rpa.v1"}, {"cuexis.gameplay.rpa.v1"}};
    assembly.declaredCapabilities = {{{"cuexis.gameplay.rpa.v1", "1"}}};
    assembly.closureContributions.capabilities = {{"cuexis.gameplay.rpa.v1", "1"}};
    assembly.sources[0].document.requirements[0].required.capabilities = {
        {"cuexis.gameplay.rpa.v1", "1"}};
    content.config.engine[1] = "fact.semantic.rpa.v1";
    content.config.engine[4] = assembly.executionProfile;
    content.config.engine[5] = "late.window.batch.v1";
    content.config.enabledCapabilities = {"cuexis.gameplay.rpa.v1"};
    const auto calibration = cuexis::playback::RealtimeClock::calibrationIdentity(clock);
    REQUIRE(calibration);
    content.config.calibration = *calibration;
}

void loadRealtimeContent(PlaybackSession& session, const ClockMappingConfiguration& clock) {
    fixture::TestContent content;
    configureRealtimeContent(content, clock);
    auto prepared = session.prepareGameplayLoad(fixture::presentation(), PlaybackMode::ChartClock,
                                                content.content());
    REQUIRE(prepared);
    REQUIRE(session.commit(std::move(*prepared)));
}

[[nodiscard]] auto endOfStream(std::string domain, std::uint64_t segment, std::int64_t now)
    -> ClockObservation {
    ClockObservation observation{};
    observation.monotonicDomain = std::move(domain);
    observation.segment = segment;
    observation.publicationSequence = 1;
    observation.sampleMonotonicUs = now;
    observation.validUntilMonotonicUs = now + 1'000'000;
    observation.source = ClockFramePosition{0, 1'000'000};
    observation.presented = ClockFramePosition{0, 1'000'000};
    observation.playback = ClockPlaybackState::EndOfStream;
    observation.quality = ClockQuality::Exact;
    return observation;
}

// A real directed Pause control that reaches a confirmed full context: it is admitted, enters and
// releases its software fence, and leaves confirmedHostContext behind. The producer must exist
// before the content is unloaded so the retention assertion observes a real value.
[[nodiscard]] auto seedConfirmedContext(PlaybackSession& session,
                                        const PreparedSemanticIdentity& identity) -> HostContext {
    const auto owner = session.executionOwnerState();
    REQUIRE(owner);
    ControlRequest pause;
    pause.context = HostContext{owner->sessionScope, 1, 1, 1, 1, 1, identity};
    pause.operationId = 1;
    pause.commandOrdinal = 1;
    pause.kind = ControlKind::pause;
    pause.participants = {false, false, false, false};
    pause.target.context = pause.context;
    pause.target.commandOrdinal = 1;
    const auto decision = session.acceptExecutionControl(pause);
    REQUIRE(decision);
    auto fence = session.enterExecutionFence(preparingProof(pause, *decision));
    REQUIRE(fence);
    auto completed = preparingProof(pause, *decision);
    completed.status = ControlStatus::complete;
    completed.cleanupComplete = true;
    completed.softwareApplied = true;
    REQUIRE(session.releaseExecutionFence(*fence, completed));
    const auto state = CandidateMetadataAccess::candidateState(session);
    CHECK(state.confirmedHostContext);
    CHECK_FALSE(state.hostFence);
    return pause.context;
}

struct AcceptedClose final {
    ControlRequest request;
    std::uint64_t decision{};
    StepRequest retire;
    std::optional<ExecutionFence> fence;
};

[[nodiscard]] auto acceptClose(PlaybackSession& session, const HostContext& context,
                               std::uint64_t operationId, std::uint64_t commandOrdinal,
                               bool enterFence) -> AcceptedClose {
    ControlRequest close;
    close.context = context;
    close.operationId = operationId;
    close.commandOrdinal = commandOrdinal;
    close.kind = ControlKind::close;
    close.participants = {false, true, false, false};
    close.target.context = close.context;
    close.target.commandOrdinal = commandOrdinal;
    const auto owner = session.executionOwnerState();
    REQUIRE(owner);
    close.target.participantGenerations[1] = owner->generation;
    auto decision = session.acceptExecutionControl(close);
    INFO((decision ? ""
                   : std::string{decision.error().code()} + ": " +
                         std::string{decision.error().message()}));
    REQUIRE(decision);
    AcceptedClose accepted{close, *decision, retireStep(close), std::nullopt};
    if (enterFence) {
        auto fence = session.enterExecutionFence(preparingProof(close, *decision));
        REQUIRE(fence);
        accepted.fence = *fence;
    }
    return accepted;
}

} // namespace

TEST_CASE("rr_session_unload_lifecycle", "[playback][rpa][lifecycle][f508]") {
    SECTION(
        "content unload keeps the accepted control identity and clears the framing projection") {
        PlaybackSession session;
        loadPresentation(session, true);
        const auto identity = session.semanticIdentity();
        REQUIRE(identity);
        const auto confirmed = seedConfirmedContext(session, *identity);
        auto context = confirmed;
        ++context.controlSequence;
        auto accepted = acceptClose(session, context, 2, 2, true);

        const auto before = CandidateMetadataAccess::candidateState(session);
#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
        CHECK(before.framings == 2);
        CHECK(before.committedPresentationToken);
#endif
        CHECK(before.confirmedHostContext);

        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));
        REQUIRE(*session.state() == SessionState::Empty);
        const auto retained = session.executionOwnerState();
        REQUIRE(retained);
        REQUIRE(retained->fence);
        CHECK(*retained->fence == *accepted.fence);
        REQUIRE(retained->acceptedControl);
        CHECK(*retained->acceptedControl == accepted.request);
        REQUIRE(session.executionFence());
        CHECK(*session.executionFence() == *accepted.fence);
        const auto state = CandidateMetadataAccess::candidateState(session);
        CHECK(state.hostFence);
        CHECK(state.acceptedHostControl);
        CHECK(state.confirmedHostContext);
        CHECK_FALSE(state.executionMapping);
        CHECK_FALSE(state.tailTransition);
#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
        CHECK(state.framings == 0);
        CHECK_FALSE(state.committedPresentationToken);
#endif

        // A repeated retire still matches the retained accepted proof and changes nothing.
        const auto generation = retained->generation;
        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));
        CHECK(session.executionOwnerState()->generation == generation);
        auto closeStep = accepted.retire;
        closeStep.key.phase = StepPhase::close;
        REQUIRE(ExecutionHostAccess::validateExecutionControlProof(
            session, closeStep, preparingProof(accepted.request, accepted.decision)));
    }

    SECTION("realtime mapping and tail transition are content, not retained control identity") {
        PlaybackSession session;
        const auto clock = clockMapping();
        loadRealtimeContent(session, clock);
        const auto identity = session.semanticIdentity();
        REQUIRE(identity);
        const auto confirmed = seedConfirmedContext(session, *identity);
        const auto scope = session.executionOwnerState()->sessionScope;
        const GameplayExecutionContext execution{scope, 1, 1};
        REQUIRE(session.beginRealtimeGameplay({1'000'000, 10, 64, "", true}, clock, execution,
                                              "lifecycle.clock"));
        const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
        const auto eof = endOfStream("lifecycle.clock", execution.clockSegment, now);
        auto tail =
            session.beginRealtimeTail(eof, execution.clockSegment + 1,
                                      {{1, 1}, 100'000, 0, "explicit lifecycle tail", true}, now);
        INFO((tail
                  ? ""
                  : std::string{tail.error().code()} + ": " + std::string{tail.error().message()}));
        REQUIRE(tail);
        auto context = confirmed;
        ++context.controlSequence;
        context.clockSegment = execution.clockSegment + 1;
        auto accepted = acceptClose(session, context, 2, 2, true);

        const auto seeded = CandidateMetadataAccess::candidateState(session);
        CHECK(seeded.executionMapping);
        CHECK(seeded.tailTransition);
        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));
        const auto state = CandidateMetadataAccess::candidateState(session);
        CHECK(state.hostFence);
        CHECK(state.acceptedHostControl);
        CHECK(state.confirmedHostContext);
        CHECK_FALSE(state.executionMapping);
        CHECK_FALSE(state.tailTransition);
    }

    SECTION("the Close terminal clears the retained control identity") {
        PlaybackSession session;
        loadPresentation(session, false);
        const auto identity = session.semanticIdentity();
        REQUIRE(identity);
        const auto confirmed = seedConfirmedContext(session, *identity);
        auto context = confirmed;
        ++context.controlSequence;
        auto accepted = acceptClose(session, context, 2, 2, true);
        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));

        const auto completed = completedCloseProof(accepted.request, accepted.decision);
        REQUIRE(session.releaseExecutionFence(*accepted.fence, completed));
        const auto terminal = session.executionOwnerState();
        REQUIRE(terminal);
        CHECK_FALSE(terminal->fence);
        CHECK_FALSE(terminal->acceptedControl);
        CHECK_FALSE(session.executionFence());
        const auto state = CandidateMetadataAccess::candidateState(session);
        CHECK_FALSE(state.hostFence);
        CHECK_FALSE(state.acceptedHostControl);
        CHECK_FALSE(state.confirmedHostContext);
        auto stale = session.retireExecutionStep(
            accepted.retire, preparingProof(accepted.request, accepted.decision));
        REQUIRE_FALSE(stale);
        CHECK(stale.error().code() == "host.rpa.context_stale");
    }

    SECTION("completeExecutionClose consumes an older fence and clears the identity") {
        PlaybackSession session;
        loadPresentation(session, false);
        const auto identity = session.semanticIdentity();
        REQUIRE(identity);
        const auto confirmed = seedConfirmedContext(session, *identity);

        // A second directed control keeps its software fence alive across the Close retire, so the
        // terminal is completed against the older fence sequence.
        auto pause = confirmed;
        ++pause.controlSequence;
        ControlRequest directed;
        directed.context = pause;
        directed.operationId = 2;
        directed.commandOrdinal = 2;
        directed.kind = ControlKind::pause;
        directed.participants = {false, false, false, false};
        directed.target.context = directed.context;
        directed.target.commandOrdinal = 2;
        const auto directedDecision = session.acceptExecutionControl(directed);
        REQUIRE(directedDecision);
        auto fence = session.enterExecutionFence(preparingProof(directed, *directedDecision));
        REQUIRE(fence);

        auto closeContext = pause;
        ++closeContext.controlSequence;
        auto accepted = acceptClose(session, closeContext, 3, 3, false);
        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));
        REQUIRE(*session.state() == SessionState::Empty);
        REQUIRE(session.executionFence());

        REQUIRE(session.completeExecutionClose(
            *fence, completedCloseProof(accepted.request, accepted.decision)));
        const auto terminal = session.executionOwnerState();
        REQUIRE(terminal);
        CHECK_FALSE(terminal->fence);
        CHECK_FALSE(terminal->acceptedControl);
        const auto state = CandidateMetadataAccess::candidateState(session);
        CHECK_FALSE(state.hostFence);
        CHECK_FALSE(state.acceptedHostControl);
        CHECK_FALSE(state.confirmedHostContext);
        // A second acknowledgement has no fence left and is not admitted as another terminal.
        CHECK_FALSE(session.completeExecutionClose(
            *fence, completedCloseProof(accepted.request, accepted.decision)));
    }

    SECTION("a new content commit rejects the old proof") {
        PlaybackSession session;
        loadPresentation(session, false);
        const auto identity = session.semanticIdentity();
        REQUIRE(identity);
        const auto owner = session.executionOwnerState();
        REQUIRE(owner);
        auto context = HostContext{owner->sessionScope, 1, 1, 1, 1, 1, *identity};
        auto accepted = acceptClose(session, context, 1, 1, false);
        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));
        REQUIRE(*session.state() == SessionState::Empty);
        REQUIRE(session.executionOwnerState()->acceptedControl);

        REQUIRE(session.loadChart(replacementChart));
        const auto replacement = session.semanticIdentity();
        REQUIRE(replacement);
        CHECK_FALSE(*replacement == *identity);
        const auto generation = session.executionOwnerState()->generation;

        auto stale = session.retireExecutionStep(
            accepted.retire, preparingProof(accepted.request, accepted.decision));
        REQUIRE_FALSE(stale);
        CHECK(stale.error().code() == "host.rpa.context_stale");
        CHECK(stale.error().message() == "Accepted control belongs to replaced content");
        CHECK(session.executionOwnerState()->generation == generation);
        CHECK(*session.state() == SessionState::Ready);

        auto closeStep = accepted.retire;
        closeStep.key.phase = StepPhase::close;
        auto closeStale = ExecutionHostAccess::validateExecutionControlProof(
            session, closeStep, preparingProof(accepted.request, accepted.decision));
        REQUIRE_FALSE(closeStale);
        CHECK(closeStale.error().code() == "host.rpa.context_stale");
    }

    SECTION("no-fence Close terminal needs the accepted identity and the applied host record") {
        PlaybackSession session;
        loadPresentation(session, false);
        const auto identity = session.semanticIdentity();
        REQUIRE(identity);
        const auto owner = session.executionOwnerState();
        REQUIRE(owner);
        auto context = HostContext{owner->sessionScope, 1, 1, 1, 1, 1, *identity};
        auto accepted = acceptClose(session, context, 1, 1, false);
        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));
        REQUIRE(*session.state() == SessionState::Empty);
        REQUIRE_FALSE(session.executionFence());

        auto closeStep = accepted.retire;
        closeStep.key.phase = StepPhase::close;
        const auto proof = completedCloseProof(accepted.request, accepted.decision);
        REQUIRE(controlFullyApplied(proof));

        // The host step ledger proves the software participant applied this exact close step.
        auto ledger = OwnerStepLedger::create(context.sessionScope, Participant::software, 16, 4);
        REQUIRE(ledger);
        auto started = ledger->start(closeStep);
        REQUIRE(started);
        REQUIRE(*started);
        REQUIRE(ledger->complete(StepReceipt{closeStep, StepStatus::applied, {}, {}}));

        REQUIRE(session.executionOwnerState()->acceptedControl);
        REQUIRE(ExecutionHostAccess::validateExecutionControlBinding(session, closeStep));
        const auto actual = ledger->query(closeStep.key);
        REQUIRE(actual);
        CHECK(actual->status == StepStatus::applied);
        CHECK(actual->request == closeStep);
        CHECK(actual->effects == proof.steps[softwareIndex][closePhase]->effects);

        // The explicit no-fence terminal acknowledges the accepted identity and drops it.
        REQUIRE(session.unload());
        CHECK_FALSE(session.executionOwnerState()->acceptedControl);
        CHECK_FALSE(CandidateMetadataAccess::candidateState(session).acceptedHostControl);
        // A repeated confirmation finds no session identity and relies on the applied record.
        CHECK_FALSE(session.executionOwnerState()->acceptedControl);
        CHECK(ledger->query(closeStep.key)->status == StepStatus::applied);

        // An unaccepted Close has neither a matching identity nor a ledger record: it stays stale.
        PlaybackSession unaccepted;
        auto unacceptedBinding =
            ExecutionHostAccess::validateExecutionControlBinding(unaccepted, closeStep);
        REQUIRE_FALSE(unacceptedBinding);
        CHECK(unacceptedBinding.error().code() == "host.rpa.context_stale");
        auto unacceptedProof =
            ExecutionHostAccess::validateExecutionControlProof(unaccepted, closeStep, proof);
        REQUIRE_FALSE(unacceptedProof);
        CHECK(unacceptedProof.error().code() == "host.rpa.context_stale");
        auto absentLedger =
            OwnerStepLedger::create(context.sessionScope, Participant::software, 16, 4);
        REQUIRE(absentLedger);
        CHECK_FALSE(absentLedger->query(closeStep.key));
    }

    SECTION("suspended or unknown Close effects keep the cleanup state") {
        PlaybackSession session;
        loadPresentation(session, false);
        const auto identity = session.semanticIdentity();
        REQUIRE(identity);
        const auto owner = session.executionOwnerState();
        REQUIRE(owner);
        auto context = HostContext{owner->sessionScope, 1, 1, 1, 1, 1, *identity};
        auto accepted = acceptClose(session, context, 1, 1, false);
        REQUIRE(session.retireExecutionStep(accepted.retire,
                                            preparingProof(accepted.request, accepted.decision)));

        auto suspended = completedCloseProof(accepted.request, accepted.decision);
        suspended.status = ControlStatus::suspended;
        suspended.cleanupComplete = false;
        CHECK_FALSE(controlFullyApplied(suspended));
        // The accepted identity still matches, so the terminal is pending rather than stale and the
        // retained cleanup state survives for a later Close acknowledgement.
        REQUIRE(ExecutionHostAccess::validateExecutionControlBinding(session, accepted.retire));
        const auto retained = session.executionOwnerState();
        REQUIRE(retained);
        REQUIRE(retained->acceptedControl);
        CHECK(*retained->acceptedControl == accepted.request);
        CHECK(CandidateMetadataAccess::candidateState(session).acceptedHostControl);
    }
}
