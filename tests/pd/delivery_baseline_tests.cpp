#include "legacy_frame_observation.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace cuexis;
using namespace playback;
using namespace playback::detail;
using FrameObservationLedger = playback::detail::baseline::FrameObservationLedger;
using FrameEnvelope = playback::detail::baseline::FrameEnvelope;
using PresentStatus = playback::detail::baseline::PresentStatus;
template <class T> auto take(core::Result<T> value) -> T {
    if (!value)
        throw std::runtime_error(std::string{value.error().code()});
    if constexpr (!std::is_void_v<T>)
        return std::move(*value);
}
auto hostContext() -> HostContext {
    HostContext c;
    c.sessionScope = c.clockSegment = c.runEpoch = c.controlSequence = c.resourceGeneration =
        c.rendererGeneration = 1;
    return c;
}
} // namespace

TEST_CASE("PD old ledger retains confirmed frames until capacity fault", "[pd][baseline][ledger]") {
    internal::pd::setRecording(true);
    const auto context = hostContext();
    for (const auto capacity : {4U, 4096U}) {
        for (const bool frozen : {false, true}) {
            auto ledger = take(FrameObservationLedger::create(context, 1, capacity, 4));
            for (std::uint64_t i = 1; i <= capacity; ++i) {
                FrameEnvelope frame;
                frame.context = context;
                frame.projectionScope = 1;
                frame.frameId = i;
                frame.presentationTick = frozen ? 0 : static_cast<std::int64_t>(i);
                frame.snapshot.objects.resize(1);
                frame.snapshot.objects[0].id = "pd-retained-object-with-owned-string-storage";
                take(ledger.publish(std::move(frame)));
                const auto submitted = take(ledger.takeLatest());
                REQUIRE(submitted.has_value());
                REQUIRE(submitted->frameId == i);
                take(ledger.observe(
                    {context, i, 1, PresentStatus::succeeded, static_cast<std::int64_t>(i), {}}));
                take(ledger.expire(static_cast<std::int64_t>(i + 1000000)));
            }
            FrameEnvelope overflow;
            overflow.context = context;
            overflow.projectionScope = 1;
            overflow.frameId = capacity + 1;
            overflow.presentationTick = frozen ? 0 : capacity + 1;
            const auto rejected = ledger.publish(std::move(overflow));
            REQUIRE_FALSE(rejected.has_value());
            REQUIRE(rejected.error().code() == "host.rpa.capacity_exceeded");
            REQUIRE(ledger.unknownCount() == 1);
            std::cout << "PD_LEDGER {\"capacity\":" << capacity
                      << ",\"frozen_tick\":" << (frozen ? "true" : "false")
                      << ",\"successful_receipts\":" << capacity
                      << ",\"first_rejected_publish\":" << capacity + 1
                      << ",\"unknown\":1,\"reclaim\":false,\"scope\":1,\"resource_generation\":1}"
                         "\n";
        }
    }
}

TEST_CASE("PD portable renderer submit and present baseline", "[pd][baseline][renderer]") {
    internal::pd::setRecording(true);
    PlaybackSession session;
    auto source = take(PlaybackSource::fromFilesystemProject(
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets/projects/stage3_project"));
    auto prepared = take(session.prepareLoad(std::move(source), PlaybackMode::ChartClock));
    presentation_renderer::TestPresentationRenderer renderer;
    auto candidate = take(renderer.prepare(prepared));
    take(renderer.accepts(candidate));
    take(session.commit(std::move(prepared)));
    renderer.activate(std::move(candidate));
    take(renderer.resize(1280, 720));
    take(session.update({0, 0, 0}));
    const auto snapshot = take(session.extractFrame({1280, 720}));
    std::size_t logicalBytes =
        sizeof(FrameSnapshot) + snapshot.objects.size() * sizeof(FrameSnapshot::ObjectSnapshot);
    for (const auto& o : snapshot.objects) {
        logicalBytes += o.id.size() + o.materialAssetId.size();
        if (o.mesh)
            logicalBytes += o.mesh->assetId.size();
        if (o.material)
            logicalBytes += o.material->assetId.size();
    }
    for (unsigned i = 0; i < 4096; ++i) {
        {
            internal::pd::Timer measurement{internal::pd::Stage::renderer};
            take(renderer.submit(snapshot));
            measurement.items(1);
        }
        {
            internal::pd::Timer measurement{internal::pd::Stage::rendererPresent};
            take(renderer.present());
            measurement.items(1);
        }
    }
    take(renderer.resize(0, 0));
    const auto suspended = renderer.submit(snapshot);
    REQUIRE_FALSE(suspended.has_value());
    REQUIRE(suspended.error().code() == "presentation.renderer.surface.zero_size");
    std::cout << "PD_RENDERER {\"kind\":\"CPU TestPresentationRenderer\",\"pairs\":4096,"
                 "\"viewport\":[1280,720],\"objects\":"
              << snapshot.objects.size() << ",\"logical_snapshot_bytes\":" << logicalBytes
              << ",\"renderer_generation\":" << renderer.identity().generation
              << ",\"zero_surface_rejected\":true,\"gpu_time\":null}\n";
    take(renderer.close());
}
