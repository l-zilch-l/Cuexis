#include "presentation_fixture.hpp"

#include "frame_layout_access.hpp"
#include "frame_layout_internal.hpp"

#include <cuexis/core/math.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using cuexis::playback::experimental::AspectRatio;
using cuexis::playback::experimental::FrameFraming;
using cuexis::playback::experimental::FrameRotation;
using cuexis::playback::experimental::FrameTarget;
using cuexis::playback::experimental::FramingRequest;
using cuexis::playback::experimental::FramingSelection;
using cuexis::playback::experimental::PixelRect;

constexpr double pi = 3.14159265358979323846;

[[nodiscard]] auto expectedEffectiveFov(double baseFovY, double verticalScale) -> double {
    return 2.0 * std::atan(verticalScale * std::tan(baseFovY * pi / 360.0)) * 180.0 / pi;
}

[[nodiscard]] auto baseProjection(double fovY, double aspect, double nearPlane, double farPlane)
    -> cuexis::core::Mat4 {
    auto projection = cuexis::core::makePerspective(fovY * pi / 180.0, aspect, nearPlane, farPlane);
    REQUIRE(projection.has_value());
    return *projection;
}

[[nodiscard]] auto loadedSession(cuexis::playback::PlaybackSession& session)
    -> cuexis::playback::PreparedPlayback {
    const auto fixture = presentation_fixture::loadFixture();
    auto source =
        presentation_fixture::typedSource(presentation_fixture::ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    return std::move(*prepared);
}

[[nodiscard]] auto hasDiagnostic(const cuexis::core::Diagnostics& diagnostics,
                                 std::string_view code) -> bool {
    for (const auto& item : diagnostics.items()) {
        if (item.code() == code) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("rr_laid_out_frame", "[playback][presentation][frame-layout][bridge]") {
    // Fixed overhead of the frame bridge lives in the frame object itself; FramePayload sizing is
    // wired by the execution-host task and not asserted here.
    STATIC_REQUIRE(sizeof(cuexis::playback::experimental::LaidOutFrame) >=
                   16 * sizeof(FrameFraming));

    SECTION("prepared framings start empty and attach validates the reduced domain") {
        cuexis::playback::PlaybackSession session;
        auto prepared = loadedSession(session);
        auto empty = cuexis::playback::experimental::preparedFramings(prepared);
        REQUIRE(empty.has_value());
        CHECK(empty->empty());

        const std::array<FrameFraming, 2> table{FrameFraming{AspectRatio{7, 3}, 1.0},
                                                FrameFraming{AspectRatio{9, 16}, 2.0}};
        REQUIRE(cuexis::playback::detail::FrameLayoutAccess::attachFramings(prepared, table)
                    .has_value());

        // An adapter that only runs the legacy submit cannot prepare a non-empty framing table.
        cuexis::playback::PresentationCapabilities portable;
        portable.version = 2;
        portable.portableProfileVersion = 1;
        portable.opaquePass = true;
        portable.transparentPass = true;
        portable.linearTexture = true;
        portable.srgbTexture = true;
        portable.straightAlphaBlend = true;
        portable.backFaceCulling = true;
        portable.doubleSided = true;
        portable.debugPass = true;
        portable.maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
        portable.maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL;
        portable.maxTextureDimension = 8192;
        portable.maxMeshVertices = 1'048'576;
        portable.maxMeshIndices = 3'145'728;
        auto validation = prepared.validatePresentation(portable, {});
        CHECK_FALSE(validation.hasValue());
        CHECK(hasDiagnostic(validation.diagnostics, "playback.presentation.layout.unsupported"));
        auto rejectedCandidate = prepared.materializePresentation(portable, {}, 1, 1, 1);
        REQUIRE_FALSE(rejectedCandidate.has_value());
        CHECK(rejectedCandidate.error().code() == "playback.presentation.layout.unsupported");

        auto copied = cuexis::playback::experimental::preparedFramings(prepared);
        REQUIRE(copied.has_value());
        REQUIRE(copied->size() == 2);
        CHECK(copied->at(0).aspect.width == 7);
        CHECK(copied->at(1).verticalScale == 2.0);

        const auto attachCode = [&](std::span<const FrameFraming> bad) {
            auto rejected =
                cuexis::playback::detail::FrameLayoutAccess::attachFramings(prepared, bad);
            REQUIRE_FALSE(rejected.has_value());
            return std::string{rejected.error().code()};
        };
        const std::array<FrameFraming, 1> unreduced{FrameFraming{AspectRatio{42, 18}, 1.0}};
        CHECK(attachCode(unreduced) == "playback.presentation.layout.invalid_framing");
        const std::array<FrameFraming, 1> zeroWidth{FrameFraming{AspectRatio{0, 3}, 1.0}};
        CHECK(attachCode(zeroWidth) == "playback.presentation.layout.invalid_aspect");
        const std::array<FrameFraming, 1> badScale{FrameFraming{AspectRatio{7, 3}, 0.124}};
        CHECK(attachCode(badScale) == "playback.presentation.layout.invalid_framing");
        const std::array<FrameFraming, 2> duplicate{FrameFraming{AspectRatio{21, 9}, 1.0},
                                                    FrameFraming{AspectRatio{7, 3}, 2.0}};
        CHECK(attachCode(duplicate) == "playback.presentation.layout.invalid_framing");
        // Rejected tables never replace the previously attached value.
        auto untouched = cuexis::playback::experimental::preparedFramings(prepared);
        REQUIRE(untouched.has_value());
        CHECK(untouched->size() == 2);
        REQUIRE(session.commit(std::move(prepared)).has_value());
    }

    SECTION("extraction selects, projects and carries one committed identity") {
        cuexis::playback::PlaybackSession session;
        auto prepared = loadedSession(session);
        const std::array<FrameFraming, 2> table{FrameFraming{AspectRatio{7, 3}, 1.0},
                                                FrameFraming{AspectRatio{9, 16}, 2.0}};
        REQUIRE(cuexis::playback::detail::FrameLayoutAccess::attachFramings(prepared, table)
                    .has_value());
        auto token = prepared.presentationCandidateToken();
        REQUIRE(token.has_value());
        REQUIRE(session.commit(std::move(prepared)).has_value());

        const FrameTarget target{.extent = {1600, 1000},
                                 .host = PixelRect{100, 50, 1280, 720},
                                 .rotation = FrameRotation::Deg0};
        auto extracted = cuexis::playback::experimental::extractLaidOutFrame(session, target);
        REQUIRE(extracted.has_value());
        REQUIRE(extracted->has_value());
        const auto& frame = **extracted;
        CHECK(frame.layout().content.width == 1280);
        CHECK(frame.layout().content.height == 548);
        CHECK(frame.layout().content.x == 0);
        CHECK(frame.layout().content.y == 86);
        CHECK(frame.snapshot().viewportWidth == 1280);
        CHECK(frame.snapshot().viewportHeight == 548);
        REQUIRE(frame.framing().has_value());
        CHECK(frame.framing()->aspect.width == 7);
        CHECK(frame.framing()->aspect.height == 3);
        CHECK(frame.resourceToken() == *token);
        CHECK(frame.source().resultVersion == session.presentationSource()->resultVersion);

        REQUIRE(frame.snapshot().camera.active);
        CHECK(frame.snapshot().camera.fovY ==
              Catch::Approx(expectedEffectiveFov(60.0, 1.0)).epsilon(1e-9));
        const auto base = baseProjection(60.0, 7.0 / 3.0, frame.snapshot().camera.nearPlane,
                                         frame.snapshot().camera.farPlane);
        CHECK(frame.snapshot().camera.projectionMatrix[0] ==
              Catch::Approx(base.values[0]).epsilon(1e-6));
        const double expectedQyy = (1280.0 * 3.0 / 7.0) / 548.0;
        CHECK(frame.snapshot().camera.projectionMatrix[5] ==
              Catch::Approx(base.values[5] * expectedQyy).epsilon(1e-6));

        // Exact selection resolves the reduced author pair and rejects the undeclared one.
        FramingRequest exact;
        exact.selection = FramingSelection::Exact;
        exact.exact = AspectRatio{14, 6};
        auto exactFrame =
            cuexis::playback::experimental::extractLaidOutFrame(session, target, {}, exact);
        REQUIRE(exactFrame.has_value());
        REQUIRE(exactFrame->has_value());
        CHECK((*exactFrame)->framing()->aspect.width == 7);
        exact.exact = AspectRatio{3, 2};
        auto missing =
            cuexis::playback::experimental::extractLaidOutFrame(session, target, {}, exact);
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.error().code() == "playback.presentation.layout.aspect_unavailable");

        // Exact version 0 check and an empty host both use the success-empty terminal.
        const auto version = session.presentationSource()->resultVersion;
        auto stale =
            cuexis::playback::experimental::extractLaidOutFrame(session, target, version + 1);
        REQUIRE_FALSE(stale.has_value());
        CHECK(stale.error().code() == "playback.presentation.version_unavailable");
        auto emptyTarget = cuexis::playback::experimental::extractLaidOutFrame(
            session, FrameTarget{.extent = {10, 10}, .host = PixelRect{10, 0, 0, 10}});
        REQUIRE(emptyTarget.has_value());
        CHECK_FALSE(emptyTarget->has_value());
    }

    SECTION("scale changes only the effective vertical angle") {
        cuexis::playback::PlaybackSession session;
        auto prepared = loadedSession(session);
        const std::array<FrameFraming, 2> table{FrameFraming{AspectRatio{7, 3}, 1.0},
                                                FrameFraming{AspectRatio{9, 16}, 2.0}};
        REQUIRE(cuexis::playback::detail::FrameLayoutAccess::attachFramings(prepared, table)
                    .has_value());
        REQUIRE(session.commit(std::move(prepared)).has_value());
        FramingRequest exact;
        exact.selection = FramingSelection::Exact;
        exact.exact = AspectRatio{9, 16};
        const FrameTarget target{.extent = {720, 1280},
                                 .host = PixelRect{0, 0, 720, 1280},
                                 .rotation = FrameRotation::Deg0};
        auto extracted =
            cuexis::playback::experimental::extractLaidOutFrame(session, target, {}, exact);
        REQUIRE(extracted.has_value());
        REQUIRE(extracted->has_value());
        CHECK((*extracted)->snapshot().camera.fovY ==
              Catch::Approx(expectedEffectiveFov(60.0, 2.0)).epsilon(1e-9));
    }

    SECTION("retained relayout reuses its own values after the session is gone") {
        cuexis::playback::PlaybackSession session;
        auto prepared = loadedSession(session);
        const std::array<FrameFraming, 2> table{FrameFraming{AspectRatio{7, 3}, 1.0},
                                                FrameFraming{AspectRatio{9, 16}, 2.0}};
        REQUIRE(cuexis::playback::detail::FrameLayoutAccess::attachFramings(prepared, table)
                    .has_value());
        REQUIRE(session.commit(std::move(prepared)).has_value());
        const FrameTarget target{.extent = {1600, 1000},
                                 .host = PixelRect{100, 50, 1280, 720},
                                 .rotation = FrameRotation::Deg0};
        auto extracted = cuexis::playback::experimental::extractLaidOutFrame(session, target);
        REQUIRE(extracted.has_value());
        REQUIRE(extracted->has_value());
        const auto retained = std::move(**extracted);
        const auto sourceVersion = retained.source().resultVersion;
        REQUIRE(session.unload().has_value());

        const FrameTarget resized{.extent = {1600, 1000},
                                  .host = PixelRect{160, 50, 1280, 721},
                                  .rotation = FrameRotation::Deg0};
        auto relaid = cuexis::playback::experimental::relayoutRetainedFrame(retained, resized);
        REQUIRE(relaid.has_value());
        REQUIRE(relaid->has_value());
        CHECK((*relaid)->layout().target.host.x == 160);
        CHECK((*relaid)->layout().content.y == 86);
        CHECK((*relaid)->source().resultVersion == sourceVersion);
        CHECK((*relaid)->resourceToken() == retained.resourceToken());
        const auto base = baseProjection(60.0, 7.0 / 3.0, 0.1, 1000.0);
        CHECK((*relaid)->snapshot().camera.projectionMatrix[0] ==
              Catch::Approx(base.values[0]).epsilon(1e-6));

        // Shrinking the host keeps the same values; BestFit re-selection is covered by the pure
        // geometry tests, the retained path only re-projects against the stored table.
        const FrameTarget square{.extent = {1000, 1000},
                                 .host = PixelRect{0, 0, 1000, 1000},
                                 .rotation = FrameRotation::Deg0};
        auto requad = cuexis::playback::experimental::relayoutRetainedFrame(retained, square);
        REQUIRE(requad.has_value());
        REQUIRE(requad->has_value());
        CHECK((*requad)->layout().content.width == 1000);
    }

    SECTION("legacy extraction keeps the zero-degree digest and applies the quarter turn once") {
        cuexis::playback::PlaybackSession session;
        auto prepared = loadedSession(session);
        REQUIRE(session.commit(std::move(prepared)).has_value());
        const FrameTarget zero{.extent = {100, 200},
                               .host = PixelRect{0, 0, 100, 200},
                               .rotation = FrameRotation::Deg0};
        auto legacyZero = cuexis::playback::experimental::extractLaidOutFrame(session, zero);
        REQUIRE(legacyZero.has_value());
        REQUIRE(legacyZero->has_value());
        CHECK_FALSE((*legacyZero)->framing().has_value());
        CHECK((*legacyZero)->snapshot().camera.fovY == Catch::Approx(60.0));
        auto plain = session.extractFrame({.width = 100, .height = 200});
        REQUIRE(plain.has_value());
        CHECK(std::equal(plain->camera.projectionMatrix,
                         std::next(plain->camera.projectionMatrix, 16),
                         (*legacyZero)->snapshot().camera.projectionMatrix));

        const FrameTarget quarter{.extent = {100, 200},
                                  .host = PixelRect{0, 0, 100, 200},
                                  .rotation = FrameRotation::Deg90};
        auto legacyQuarter = cuexis::playback::experimental::extractLaidOutFrame(session, quarter);
        REQUIRE(legacyQuarter.has_value());
        REQUIRE(legacyQuarter->has_value());
        const auto base = baseProjection(60.0, 200.0 / 100.0, 0.1, 1000.0);
        // R90 * P: row 0 becomes P row 1, row 1 becomes -P row 0.
        CHECK((*legacyQuarter)->snapshot().camera.projectionMatrix[0] ==
              Catch::Approx(base.values[1]).epsilon(1e-6));
        CHECK((*legacyQuarter)->snapshot().camera.projectionMatrix[5] ==
              Catch::Approx(-base.values[4]).epsilon(1e-6));
        CHECK((*legacyQuarter)->snapshot().camera.fovY == Catch::Approx(60.0));
        CHECK((*legacyQuarter)->snapshot().viewportWidth == 100);
        CHECK((*legacyQuarter)->snapshot().viewportHeight == 200);
    }
}
