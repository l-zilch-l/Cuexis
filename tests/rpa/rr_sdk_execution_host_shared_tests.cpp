// RR-4 shared Player evidence: the execution-host access bridge must resolve to the same
// experimental Playback shared library the real Player loads. The case exercises every entry of
// detail::ExecutionHostAccess against a real loaded session; the accompanying CTest driver checks
// the Player import set and the exported internal symbols.

#include <cuexis/playback/execution_host_access.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

using namespace cuexis;

TEST_CASE("rr_sdk_execution_host_shared_linkage", "[rpa][sdk][shared]") {
    using playback::detail::ExecutionHostAccess;

    playback::PlaybackSession session;
    auto source = playback::PlaybackSource::fromFilesystemProject(
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets/projects/stage3_project");
    REQUIRE(source);
    REQUIRE(session.load(std::move(*source), playback::PlaybackMode::ChartClock));

    // Real owner state comes from the loaded production session; no proof token exists yet.
    const auto owner = ExecutionHostAccess::executionOwnerState(session);
    REQUIRE(owner);
    CHECK(owner->sessionScope != 0);
    CHECK_FALSE(owner->fence.has_value());

    // Every other entry has to link against the shared library and keep the member's rejection or
    // value semantics; fabricating a fence or a proof must never authenticate one.
    CHECK_FALSE(
        ExecutionHostAccess::enterExecutionFence(session, playback::PresentationCandidateInput{}));
    CHECK_FALSE(ExecutionHostAccess::enterExecutionFence(session, playback::ControlReceipt{}));
    CHECK_FALSE(ExecutionHostAccess::commitExecutionCandidate(session, playback::PreparedPlayback{},
                                                              playback::ExecutionFence{},
                                                              playback::ControlReceipt{}));
    CHECK_FALSE(ExecutionHostAccess::releaseExecutionFence(session, playback::ExecutionFence{},
                                                           playback::ControlReceipt{}));
    CHECK_FALSE(ExecutionHostAccess::executionFence(session));
    CHECK_FALSE(ExecutionHostAccess::completeExecutionClose(session, playback::ExecutionFence{},
                                                            playback::ControlReceipt{}));
    CHECK_FALSE(
        ExecutionHostAccess::validateExecutionControlBinding(session, playback::StepRequest{}));
    CHECK_FALSE(ExecutionHostAccess::validateExecutionControlProof(session, playback::StepRequest{},
                                                                   playback::ControlReceipt{}));
    CHECK_FALSE(
        ExecutionHostAccess::validateExecutionFrameContext(session, playback::HostContext{}));
    CHECK_FALSE(ExecutionHostAccess::acceptExecutionControl(session, playback::ControlRequest{}));
    CHECK_FALSE(ExecutionHostAccess::retireExecutionStep(session, playback::StepRequest{},
                                                         playback::ControlReceipt{}));
    CHECK_FALSE(ExecutionHostAccess::pauseExecutionStep(session, playback::StepRequest{}, 0));
    CHECK_FALSE(ExecutionHostAccess::resumeExecutionStep(
        session, playback::StepRequest{}, playback::GameplayReconciliation{},
        playback::GameplayExecutionContext{}, playback::ClockObservation{}, 0, nullptr));
    CHECK_FALSE(ExecutionHostAccess::seekExecutionStep(
        session, playback::StepRequest{}, playback::ControlReceipt{}, playback::GameplayReplay{},
        playback::GameplayTick{0}, playback::GameplayReplayCut{}, {},
        playback::GameplayPresentationTick{0}, playback::RuntimeFrame{},
        playback::SchedulingProfile{}, playback::ClockMappingConfiguration{}, "rr.sdk.shared"));
    // The two table-named entries that the installed owner/step probes also consume.
    const auto effects = ExecutionHostAccess::executionStepEffects(session);
    CHECK_FALSE(effects);
    auto identityContext = playback::GameplayExecutionContext{};
    std::optional<std::uint64_t> lastAcceptedId;
    std::optional<std::uint64_t> reclaimedThrough;
    const auto retention = ExecutionHostAccess::executionAdvanceRetention(
        session, identityContext, lastAcceptedId, reclaimedThrough);
    CHECK_FALSE(retention);
}
