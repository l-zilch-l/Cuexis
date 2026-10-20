#pragma once
// Included by playback_allocation_tests.cpp to reuse its actual allocator boundary.
#include <cuexis/playback/frame_digest.hpp>

TEST_CASE("PD3b materializer allocation failure retains the public extraction error boundary",
          "[pd][presentation-state][allocation]") {
#if defined(CUEXIS_PLAYBACK_ALLOCATION_TEST_SHARED)
    SKIP("Executable allocator injection does not intercept the shared Playback DLL.");
#else
    auto session = makeAllocationBoundarySession();
    REQUIRE(session);
    auto initial = session->extractFrame({640, 480});
    REQUIRE(initial);
    REQUIRE(initial->objects.size() == 1);
    // Construct the Debug STL containers before injection; fail an actual string payload.
    cuexis::playback::FrameSnapshot destination;
    destination.objects.resize(1);
    beginAllocationFailure(1);
    auto failed = session->extractFrame({640, 480}, destination);
    endAllocationFailure();
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code() == "playback.prepare.budget_exceeded");
    bool hasOperation{};
    for (const auto& context : failed.error().context())
        if (context.key == "operation" && context.value == "extract_frame")
            hasOperation = true;
    CHECK(hasOperation);
    REQUIRE(session->extractFrame({640, 480}, destination));
    CHECK(destination.objects[0].id == initial->objects[0].id);
#endif
}

TEST_CASE("PD3a extraction failure after install preserves committed state and override lifetime",
          "[pd][transaction][allocation][delivery]") {
#if defined(CUEXIS_PLAYBACK_ALLOCATION_TEST_SHARED)
    SKIP("Executable allocator injection does not intercept the shared Playback DLL.");
#else
    using namespace cuexis::playback;
    auto session = makeAllocationBoundarySession();
    REQUIRE(session);
    auto initial = session->extractFrame({640, 480});
    REQUIRE(initial);
    REQUIRE(initial->objects.size() == 1);
    const std::array actualWrites{
        HostOverrideWrite{initial->objects[0].id, HostPropertyId::TransformPositionX, 7.0}};
    REQUIRE(session->acquireHostOverride(
        "pd3a.delivery", 0, hostPropertyBit(HostPropertyId::TransformPositionX),
        {.kind = HostOverrideLifetimeKind::RemainingFrames, .remainingFrames = 2}, actualWrites));
    const RuntimeFrame frame{10, 10, 0};
    REQUIRE(session->update(frame));
    auto committed = session->extractFrame({640, 480});
    REQUIRE(committed);
    const auto expected = computeFrameDigest(frame, *committed);
    REQUIRE(expected);
    CHECK(committed->objects[0].worldMatrix[12] == 7);
    CHECK_FALSE(session->extractFrame({0, 480}));
#if !defined(_MSC_VER) || !defined(_DEBUG)
    // Release has no MSVC iterator proxies in noexcept default constructors. Sweep every
    // allocation in the reusable extraction, with the destination constructed beforehand.
    FrameSnapshot probe;
    beginAllocationTracking();
    auto extracted = session->extractFrame({640, 480}, probe);
    const auto count = endAllocationTracking();
    REQUIRE(extracted);
    REQUIRE(count > 0);
    for (std::size_t index = 1; index <= count; ++index) {
        CAPTURE(index, count);
        FrameSnapshot destination;
        const auto outcome = invokeWithAllocationFailure(
            index, [&] { return session->extractFrame({640, 480}, destination); });
        CHECK_FALSE(outcome.returnedValue);
        CHECK_FALSE(outcome.badAllocEscaped);
        CHECK_FALSE(outcome.lengthErrorEscaped);
        auto retried = session->extractFrame({640, 480});
        REQUIRE(retried);
        auto actual = computeFrameDigest(frame, *retried);
        REQUIRE(actual);
        CHECK(actual->value == expected->value);
    }
#endif
    auto retried = session->extractFrame({640, 480});
    REQUIRE(retried);
    CHECK(computeFrameDigest(frame, *retried)->value == expected->value);
    REQUIRE(session->update({10, 0, 0}));
    CHECK(session->extractFrame({640, 480})->objects[0].worldMatrix[12] == 7);
    REQUIRE(session->update({10, 0, 0}));
    CHECK(session->extractFrame({640, 480})->objects[0].worldMatrix[12] == 0);
#endif
}
