#include "../../app/execution_host/frame_observation.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace cuexis;
using namespace cuexis::playback;
using namespace cuexis::playback::detail;

namespace {
auto bindingContext() -> HostContext {
    HostContext context;
    context.sessionScope = context.clockSegment = context.runEpoch = context.controlSequence =
        context.resourceGeneration = context.rendererGeneration = 1;
    return context;
}
} // namespace

TEST_CASE("PD3c control rebind preserves feedback identity and source time",
          "[pd][control-binding]") {
    auto context = bindingContext();
    auto ledger = FrameObservationLedger::create(context, 7, 8, 8);
    REQUIRE(ledger);
    const FeedbackId id{7, 1, 0, 0, "target", "binding"};
    REQUIRE(ledger->feedback(id, 10, 200));
    FrameEnvelope frame;
    frame.context = context;
    frame.projectionScope = 7;
    frame.frameId = 1;
    frame.presentationTick = 100;
    frame.membership = {id};
    REQUIRE(ledger->publish(frame));
    REQUIRE(ledger->takeLatest());
    auto paused = context;
    ++paused.clockSegment;
    ++paused.controlSequence;
    REQUIRE(ledger->rebind(paused));
    CHECK(ledger->query(id)->status == FeedbackStatus::created);
    // The old receipt updates its own obligation, without confirming the new control.
    REQUIRE(ledger->observe({context, 1, 7, PresentStatus::succeeded, 123, {}}));
    CHECK(ledger->query(id)->status == FeedbackStatus::confirmedPresent);
    CHECK(ledger->query(id)->created == 10);
    REQUIRE(ledger->query(id)->expiry);
    CHECK(ledger->query(id)->expiry->value == 200);
    REQUIRE(ledger->rebind(paused));
    CHECK_FALSE(ledger->publish(frame));
    frame.context = paused;
    frame.frameId = 2;
    frame.presentationTick = 99;
    CHECK_FALSE(ledger->publish(frame)); // controls cannot rewind the source frontier
    auto resumed = paused;
    ++resumed.clockSegment;
    ++resumed.runEpoch;
    ++resumed.controlSequence;
    REQUIRE(ledger->rebind(resumed));
    auto eof = resumed;
    ++eof.clockSegment;
    REQUIRE(ledger->rebind(eof));
    CHECK(ledger->query(id)->status == FeedbackStatus::confirmedPresent);
    frame.context = eof;
    frame.frameId = 3;
    frame.presentationTick = 110;
    REQUIRE(ledger->publish(frame));
    auto next = eof;
    ++next.clockSegment;
    ++next.controlSequence;
    REQUIRE(ledger->rebind(next));
    auto obsolete = ledger->takeLatest();
    REQUIRE(obsolete);
    CHECK_FALSE(*obsolete);
    frame.context = next;
    frame.frameId = 4;
    frame.presentationTick = 120;
    REQUIRE(ledger->publish(frame));
    auto latest = ledger->takeLatest();
    REQUIRE(latest);
    REQUIRE(*latest);
    CHECK((**latest).frameId == 4);
    CHECK((**latest).context == next);
    auto replaced = next;
    ++replaced.controlSequence;
    REQUIRE(ledger->activate(replaced, 8));
    CHECK(ledger->query(id)->status == FeedbackStatus::confirmedPresent);
}

TEST_CASE("PD3c control rebind rejects stale and changed resource bindings",
          "[pd][control-binding]") {
    const auto context = bindingContext();
    for (int field = 0; field < 7; ++field) {
        auto ledger = FrameObservationLedger::create(context, 7, 2, 2);
        REQUIRE(ledger);
        auto next = context;
        ++next.controlSequence;
        if (field == 0)
            ++next.sessionScope;
        if (field == 1)
            ++next.resourceGeneration;
        if (field == 2)
            ++next.rendererGeneration;
        if (field == 3)
            next.contentIdentity.sha256[0] = 1;
        if (field == 4)
            next.clockSegment = 0;
        if (field == 5)
            next.runEpoch = 0;
        if (field == 6)
            next.controlSequence = 0;
        CHECK_FALSE(ledger->rebind(next));
        FrameEnvelope frame;
        frame.context = context;
        frame.projectionScope = 7;
        frame.frameId = 1;
        frame.presentationTick = 100;
        REQUIRE(ledger->publish(std::move(frame))); // rejection kept the prior binding
    }
}

TEST_CASE("PD3c retained resize preserves all source values and view matrix",
          "[pd][control-binding]") {
    FrameSnapshot original;
    original.viewportWidth = 640;
    original.viewportHeight = 480;
    original.objects.resize(1);
    auto& object = original.objects[0];
    object.id = "older-successful-state";
    object.hasTransform = true;
    object.worldMatrix[12] = 42;
    object.visible = false;
    object.materialAssetId = "source-material";
    object.materialOpacity = 0.25;
    object.materialTint[0] = 0.125F;
    object.mesh = PresentationResourceRef{PresentationResourceType::Mesh, "mesh-source", {}};
    object.mesh->identity.sha256[0] = 37;
    object.material =
        PresentationResourceRef{PresentationResourceType::UnlitMaterial, "material-source", {}};
    object.material->identity.sha256[0] = 39;
    original.clearRed = 0.25F;
    original.clearAlpha = 0.5F;
    original.camera.nearPlane = 0.25;
    original.camera.farPlane = 800;
    original.camera.active = true;
    original.camera.fovY = 80;
    original.camera.viewMatrix[12] = 19;
    auto resized = rematerializeRetainedSnapshot(original, {1200, 400});
    REQUIRE(resized);
    CHECK(resized->viewportWidth == 1200);
    CHECK(resized->viewportHeight == 400);
    REQUIRE(resized->objects.size() == 1);
    const auto& result = resized->objects[0];
    CHECK(result.id == object.id);
    CHECK(result.mesh == object.mesh);
    CHECK(result.material == object.material);
    CHECK(resized->clearRed == original.clearRed);
    CHECK(resized->clearAlpha == original.clearAlpha);
    CHECK(resized->camera.nearPlane == original.camera.nearPlane);
    CHECK(resized->camera.farPlane == original.camera.farPlane);
    CHECK(result.hasTransform == object.hasTransform);
    CHECK(std::equal(std::begin(result.worldMatrix), std::end(result.worldMatrix),
                     std::begin(object.worldMatrix)));
    CHECK(result.visible == object.visible);
    CHECK(result.materialAssetId == object.materialAssetId);
    CHECK(result.materialOpacity == object.materialOpacity);
    CHECK(std::equal(std::begin(result.materialTint), std::end(result.materialTint),
                     std::begin(object.materialTint)));
    CHECK(std::equal(std::begin(resized->camera.viewMatrix), std::end(resized->camera.viewMatrix),
                     std::begin(original.camera.viewMatrix)));
    auto projection = core::makePerspective(80 * 3.14159265358979323846 / 180.0, 3, 0.25, 800);
    REQUIRE(projection);
    CHECK(std::equal(projection->values.begin(), projection->values.end(),
                     std::begin(resized->camera.projectionMatrix)));
    CHECK(original.viewportWidth == 640);
    CHECK(original.camera.viewMatrix[12] == 19);
    CHECK_FALSE(rematerializeRetainedSnapshot(original, {0, 480}));
}
