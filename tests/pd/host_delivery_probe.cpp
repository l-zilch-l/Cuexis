#include "../../app/player/src/execution_host.hpp"
#include "../support/environment.hpp"
#include "measurement_environment.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis_internal/presentation_metrics.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {
using namespace cuexis;
using namespace playback;
using Clock = std::chrono::steady_clock;
constexpr unsigned warmup = 32;

template <class T> auto take(core::Result<T> value) -> T {
    if (!value)
        throw std::runtime_error(std::string{value.error().code()} + ": " +
                                 std::string{value.error().message()});
    if constexpr (!std::is_void_v<T>)
        return std::move(*value);
}
auto microseconds(Clock::duration duration) -> double {
    return std::chrono::duration<double, std::micro>(duration).count();
}
auto source() -> PlaybackSource {
    return take(PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                      "assets/projects/stage3_project"));
}
} // namespace

TEST_CASE("PD host delivery and receipt acknowledgement baseline", "[.][pd][host-delivery]") {
    using namespace cuexis;
    using namespace playback;
    using namespace playback::detail;
    // The existing formal harness remains 512 frames. The finite diagnostic opts in before launch.
    const bool extended =
        test_support::environmentVariable("CUEXIS_PD_HOST_DIAGNOSTIC_1024") == "1";
    const unsigned measured = extended ? 1024U : 512U;
    const unsigned frameCapacity = extended ? 2048U : 1024U;
    // Recording on main is only enabled around host calls; reference work is excluded.
    // A worker inherits the process-level observation mode when its metrics are initialized.
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    const bool recording = internal::pd::threadMetrics().path != nullptr;
#else
    const bool recording = false;
#endif
    internal::pd::setRecording(false);
    player::ExecutionHostOptions options;
    options.session = {64, 16, 128, 32, frameCapacity, 16, true, {}};
    options.audio.testOnly = true;
    options.hostCapacity = 16;
    options.captureCapacity = 64;
    options.captureReserve = 16;
    options.replyDeadline = std::chrono::seconds{5};
    options.viewport = {1280, 720};
    presentation_renderer::TestPresentationRenderer renderer;
    auto host = take(player::ExecutionHost::create(renderer, options, {}));
    take(host->load(source()));
    REQUIRE_FALSE(host->hasAudio());
    REQUIRE(host->state() == HostState::running);
    const auto context = host->context();
    PlaybackSession reference;
    take(reference.commit(take(reference.prepareLoad(source(), PlaybackMode::ChartClock))));

    std::uint64_t digest = 14695981039346656037ULL;
    std::cout << std::setprecision(17);
    const auto pumpsBefore = host->pumpCount();
#if defined(_WIN32)
    pd_measure::environmentJson("begin");
#endif
    for (unsigned i = 0; i < warmup + measured; ++i) {
        const auto tick = static_cast<std::int64_t>(i + 1) * 10;
        const RuntimeFrame requested{static_cast<double>(tick), 10, context.clockSegment};
        internal::pd::setRecording(recording && i >= warmup);
        const auto requestAt = Clock::now();
        auto frame = take(host->sample(tick, requested, false));
        const auto readyAt = Clock::now();
        const auto receipt = take(host->present(frame));
        const auto presentReturnedAt = Clock::now();
        internal::pd::setRecording(false);
        REQUIRE(frame.context == context);
        REQUIRE(frame.frameId == i + 1);
        REQUIRE(frame.snapshotView().objects.size() == 2);
        REQUIRE(frame.membershipView().empty());
        REQUIRE_FALSE(frame.foldedFactCursor);
        REQUIRE(receipt.context == frame.context);
        REQUIRE(receipt.frameId == frame.frameId);
        REQUIRE(receipt.projectionScope == frame.projectionScope);
        REQUIRE(receipt.status == PresentStatus::succeeded);
        const auto returnedAtUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                      presentReturnedAt.time_since_epoch())
                                      .count();
        REQUIRE(receipt.callMonotonicUs <= returnedAtUs);
        // The reference uses the actual frame identity, independent of the worker/ledger route.
        REQUIRE(frame.runtimeFrame);
        take(reference.update(*frame.runtimeFrame));
        const auto expected = take(reference.extractFrame(options.viewport));
        const auto actualDigest =
            take(computeFrameDigest(*frame.runtimeFrame, frame.snapshotView()));
        REQUIRE(actualDigest.value ==
                take(computeFrameDigest(*frame.runtimeFrame, expected)).value);
        if (i < warmup)
            continue;
        digest = (digest ^ actualDigest.value) * 1099511628211ULL;
        std::cout << "PD_HOST_SAMPLE {\"ordinal\":" << i - warmup
                  << ",\"request_to_ready_us\":" << microseconds(readyAt - requestAt)
                  << ",\"ready_to_present_return_us\":" << microseconds(presentReturnedAt - readyAt)
                  << ",\"request_to_present_return_us\":"
                  << microseconds(presentReturnedAt - requestAt)
                  << ",\"present_call_to_return_us\":" << returnedAtUs - receipt.callMonotonicUs
                  << ",\"projection_scope\":" << frame.projectionScope << "}\n";
    }
    const auto pumps = host->pumpCount() - pumpsBefore;
#if defined(_WIN32)
    pd_measure::environmentJson("end");
#endif
    take(host->close());
    REQUIRE(host->state() == HostState::closed);
    std::cout << "PD_HOST_ID "
                 "{\"receipt_mode\":\"asynchronous\",\"measurement_schema\":\"pd4.present-return."
                 "v1\",\"digest\":"
              << digest << ",\"warmup\":" << warmup << ",\"measured\":" << measured
              << ",\"frame_capacity\":" << frameCapacity
              << ",\"frames_published\":" << warmup + measured
              << ",\"successful_receipts\":" << warmup + measured
              << ",\"source\":\"assets/projects/stage3_project\",\"viewport\":[1280,720],"
                 "\"objects\":2,\"audio\":false,\"gpu\":false,\"pump_count\":"
              << pumps << ",\"recording\":" << (recording ? "true" : "false")
              << ",\"closed\":true}\n";
}
