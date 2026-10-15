#include <cuexis/playback/playback_session.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>
#include <thread>

using namespace cuexis;
using namespace playback;

TEST_CASE("Public execution candidates reject foreign owners and opaque fences",
          "[rpa][boundary][public]") {
    PlaybackSession session;
    PlaybackSession foreign;
    presentation_renderer::TestPresentationRenderer renderer;
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    auto prepared = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
    REQUIRE(prepared);
    auto candidate =
        prepared->materializePresentation(renderer.capabilities(), {}, renderer.identity().instance,
                                          renderer.identity().generation, 1);
    REQUIRE(candidate);
    REQUIRE_FALSE(foreign.enterExecutionFence(*candidate));
    const auto before = session.executionOwnerState();
    REQUIRE(before);
    auto fence = session.enterExecutionFence(*candidate);
    REQUIRE(fence);
    REQUIRE_FALSE(session.update({}));
    auto proof = std::make_unique<ControlReceipt>();
    REQUIRE_FALSE(session.releaseExecutionFence(ExecutionFence{}, *proof));
    REQUIRE_FALSE(session.releaseExecutionFence(*fence, *proof));
    auto after = session.executionOwnerState();
    REQUIRE(after);
    CHECK(after->generation == before->generation);
    CHECK(after->fence == *fence);
    // Owner-bound SDK calls reject foreign threads without publishing a different state.
    bool ownerRejected{};
    std::thread worker{[&] { ownerRejected = !session.executionOwnerState(); }};
    worker.join();
    CHECK(ownerRejected);
    REQUIRE(renderer.close());
}

TEST_CASE("Public execution proof predicates reject out of range participants",
          "[rpa][boundary][public]") {
    auto proof = std::make_unique<ControlReceipt>();
    CHECK(confirmedExecutionStep(*proof, static_cast<Participant>(255), StepPhase::prepare) ==
          nullptr);
    CHECK(confirmedExecutionStep(*proof, Participant::software, static_cast<StepPhase>(255)) ==
          nullptr);
    PlaybackSession session;
    auto before = session.executionOwnerState();
    REQUIRE(before);
    REQUIRE_FALSE(session.pauseExecutionStep({}, 0));
    REQUIRE_FALSE(session.retireExecutionStep({}, *proof));
    auto after = session.executionOwnerState();
    REQUIRE(after);
    CHECK(after->generation == before->generation);
    CHECK_FALSE(after->acceptedControl);
    CHECK_FALSE(after->fence);
}
