#include "../../app/execution_host/audio_worker.hpp"
#include "../../app/execution_host/renderer_owner.hpp"
#include "execution_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <future>

using namespace cuexis;
using namespace cuexis::playback::detail;

TEST_CASE("B5 retained renderer readiness rejects stale identity before fencing", "[rpa][b5]") {
    playback::PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto prepared =
        session.prepareLoad(test_support::presentation(), playback::PlaybackMode::ChartClock);
    REQUIRE(prepared);
    auto input = HostCoordinationAccess::materialize(*prepared, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    auto owner = RendererOwner<presentation_renderer::TestPresentationRenderer>::create(
        input->context.sessionScope, renderer, 32, 8);
    REQUIRE(owner);
    StepRequest request;
    request.key = {input->context.sessionScope, 1, Participant::renderer, StepPhase::prepare, 0};
    request.target.context = input->context;
    request.target.candidateId = input->candidateId;
    request.target.participantGenerations[3] = renderer.identity().generation;
    REQUIRE(owner->prepare(request, *input)->status == StepStatus::applied);
    request.key.phase = StepPhase::fence;
    REQUIRE(owner->fence(request)->status == StepStatus::applied);
    request.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(request)->status == StepStatus::applied);
    const auto identity = renderer.identity();
    request.key.operationId = 2;
    request.key.phase = StepPhase::prepare;
    request.target.context.controlSequence = 2;
    ++*request.target.participantGenerations[3];
    REQUIRE(owner->prepareActive(request)->status == StepStatus::rejectedNotApplied);
    request.key.operationId = 3;
    request.target.context.controlSequence = 3;
    request.target.participantGenerations[3] = identity.generation;
    ++request.target.context.resourceGeneration;
    REQUIRE(owner->prepareActive(request)->status == StepStatus::rejectedNotApplied);
    request.key.operationId = 4;
    request.target.context.controlSequence = 4;
    request.target.context.resourceGeneration = input->context.resourceGeneration;
    REQUIRE(owner->prepareActive(request)->status == StepStatus::applied);
    REQUIRE(owner->prepareActive(request)->status == StepStatus::applied);
    request.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(request)->status == StepStatus::rejectedNotApplied);
    CHECK(renderer.hasActivePresentation());
    CHECK(renderer.identity().generation == identity.generation);
}

TEST_CASE("B5 failed audio creation retains accepted replies through shutdown", "[rpa][b5]") {
    AudioWorkerOptions options;
    options.testOnly = true;
    options.sessionScope = 1;
    options.timeDomain = 1;
    options.capacity = 3;
    options.controlReserve = 1;
    std::promise<void> entered, release;
    auto released = release.get_future().share();
    auto worker = AudioWorker::start(
        options,
        [&](audio::AudioClipStore&) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
            entered.set_value();
            released.wait();
            return core::unexpected(
                core::Error{"probe.device_unavailable", "Injected creation failure"});
        },
        [](std::span<const std::byte>) -> core::Result<audio::AudioClip> {
            return core::unexpected(core::Error{"probe.decode_unexpected", "Decode must not run"});
        });
    REQUIRE(worker);
    const auto started = entered.get_future().wait_for(std::chrono::seconds{3});
    if (started != std::future_status::ready)
        release.set_value();
    REQUIRE(started == std::future_status::ready);
    auto first = (*worker)->post({}, false);
    auto second = (*worker)->post({}, false);
    auto full = (*worker)->post({}, false);
    auto reserved = (*worker)->post({}, true);
    auto prematureJoin = (*worker)->joinExited();
    (*worker)->requestShutdown();
    auto sealed = (*worker)->post({}, true);
    // Always release the fixture before assertions, including failure paths.
    release.set_value();
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE_FALSE(full);
    REQUIRE(reserved);
    REQUIRE_FALSE(prematureJoin);
    REQUIRE_FALSE(sealed);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (!(*worker)->exited() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    REQUIRE((*worker)->exited());
    REQUIRE((*worker)->joinExited());
    auto publication = (*worker)->publication();
    REQUIRE(publication);
    REQUIRE(publication->fault == AudioPublication::Fault::device);
    REQUIRE(publication->serviceError);
    REQUIRE(publication->serviceError->code() == "probe.device_unavailable");
    for (const auto id : {*first, *second, *reserved}) {
        auto reply = (*worker)->poll(id);
        REQUIRE(reply);
        REQUIRE(*reply);
        REQUIRE_FALSE(**reply);
        REQUIRE((**reply).error().code() == "host.rpa.owner_closed");
        REQUIRE((*worker)->acknowledge(id));
        REQUIRE_FALSE((*worker)->poll(id));
    }
}
