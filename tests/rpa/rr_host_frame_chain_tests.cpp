// T-06 host frame chain: the owning frame payload variant, the laid-out request target, the
// bindings that submit it, and the non-fault deadline/target drops. The renderer is a neutral
// wrapper around the existing test presentation renderer; no window, GL context, or audio device
// is created.

#include "execution_fixture.hpp"

#include "../../app/player/src/execution_host.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

using namespace cuexis;
using namespace cuexis::playback;
using namespace cuexis::playback::detail;

namespace {

class LayoutProbeRenderer final : public presentation_renderer::IPresentationRenderer {
  public:
    [[nodiscard]] auto capabilities() const noexcept -> const PresentationCapabilities& override {
        return inner_.capabilities();
    }
    [[nodiscard]] auto identity() const noexcept
        -> presentation_renderer::RendererIdentity override {
        return inner_.identity();
    }
    [[nodiscard]] auto prepare(PreparedPlayback& prepared, const PresentationRequest& request = {})
        -> core::Result<presentation_renderer::PreparedPresentation> override {
        return inner_.prepare(prepared, request);
    }
    [[nodiscard]] auto prepareOwned(const PresentationCandidateInput& input)
        -> core::Result<presentation_renderer::PreparedPresentation> override {
        return inner_.prepareOwned(input);
    }
    [[nodiscard]] auto accepts(const presentation_renderer::PreparedPresentation& candidate) const
        -> core::Result<void> override {
        return inner_.accepts(candidate);
    }
    void activate(presentation_renderer::PreparedPresentation&& candidate) noexcept override {
        inner_.activate(std::move(candidate));
    }
    void discard(presentation_renderer::PreparedPresentation&& candidate) noexcept override {
        inner_.discard(std::move(candidate));
    }
    [[nodiscard]] auto hasActivePresentation() const noexcept -> bool override {
        return inner_.hasActivePresentation();
    }
    [[nodiscard]] auto submit(const FrameSnapshot& snapshot,
                              const render::RenderScene* debugScene = nullptr)
        -> core::Result<presentation_renderer::DrawSummary> override {
        ++plainSubmits;
        return inner_.submit(snapshot, debugScene);
    }
    [[nodiscard]] auto present() -> core::Result<void> override {
        ++presents;
        return inner_.present();
    }
    [[nodiscard]] auto resize(std::uint32_t width, std::uint32_t height)
        -> core::Result<void> override {
        return inner_.resize(width, height);
    }
    [[nodiscard]] auto rebuild() -> core::Result<void> override {
        return inner_.rebuild();
    }
    [[nodiscard]] auto close() -> core::Result<void> override {
        return inner_.close();
    }
    [[nodiscard]] auto submitLaidOut(const experimental::LaidOutFrame& frame,
                                     const experimental::FrameTarget& currentTarget,
                                     const render::RenderScene* debugScene = nullptr)
        -> core::Result<presentation_renderer::DrawSummary> {
        ++layoutSubmits;
        lastTarget = currentTarget;
        lastSubviewWidth = frame.layout().content.width;
        return inner_.submit(frame.snapshot(), debugScene);
    }
    [[nodiscard]] auto binding() -> FrameLayoutRendererBinding {
        return FrameLayoutRendererBinding{this, this, &prepareOwnedThunk, &submitThunk};
    }
    std::uint64_t plainSubmits{}, layoutSubmits{}, presents{};
    experimental::FrameTarget lastTarget{};
    std::uint32_t lastSubviewWidth{};

  private:
    static auto prepareOwnedThunk(void* object, const PresentationCandidateInput& input,
                                  std::span<const experimental::FrameFraming> framings)
        -> core::Result<presentation_renderer::PreparedPresentation> {
        // The legacy fixture carries no author framing table; the empty table is the contract's
        // legacy path and the renderer prepare stays the same one the plain path uses.
        REQUIRE(framings.empty());
        return static_cast<LayoutProbeRenderer*>(object)->prepareOwned(input);
    }
    static auto submitThunk(void* object, const experimental::LaidOutFrame& frame,
                            const experimental::FrameTarget& currentTarget,
                            const render::RenderScene* debugScene)
        -> core::Result<presentation_renderer::DrawSummary> {
        return static_cast<LayoutProbeRenderer*>(object)->submitLaidOut(frame, currentTarget,
                                                                        debugScene);
    }
    void detachCandidate(std::uint64_t, std::uint64_t,
                         const PresentationCandidateToken&) noexcept override {}
    presentation_renderer::TestPresentationRenderer inner_;
};

auto options(std::function<std::int64_t()> clock, std::chrono::milliseconds replyDeadline)
    -> player::ExecutionHostOptions {
    player::ExecutionHostOptions result;
    result.session = {64, 16, 128, 32, 32, 64, true, std::move(clock)};
    result.audio.testOnly = true;
    result.hostCapacity = 16;
    result.captureCapacity = 64;
    result.captureReserve = 16;
    result.replyDeadline = replyDeadline;
    result.viewport = {640, 480};
    return result;
}

[[nodiscard]] auto hasReason(const core::Error& error, std::string_view reason) -> bool {
    for (const auto& context : error.context())
        if (context.key == "reason" && context.value == reason)
            return true;
    return false;
}

} // namespace

TEST_CASE("T-06 host samples and presents one laid-out owning frame", "[rpa][t06][frame-chain]") {
    LayoutProbeRenderer renderer;
    std::int64_t clock = 1000;
    auto settings = options([&] { return clock; }, std::chrono::seconds{5});
    settings.frameLayout = renderer.binding();
    auto made = player::ExecutionHost::create(renderer, settings, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation()));

    auto sampled = host.sample(0, {0, 0, host.context().clockSegment}, false);
    REQUIRE(sampled);
    REQUIRE(sampled->payload);
    REQUIRE(sampled->laidOutView());
    REQUIRE(sampled->layoutView());
    // The frame, its layout, its source and its token come from the same owning extraction.
    CHECK(sampled->snapshotView().viewportWidth == 640);
    CHECK(sampled->snapshotView().viewportHeight == 480);
    CHECK(sampled->layoutView()->target.extent.width == 640);
    CHECK(sampled->layoutView()->target.extent.height == 480);
    CHECK(sampled->layoutView()->content.width == 640);
    CHECK(sampled->layoutView()->content.height == 480);
    CHECK_FALSE(sampled->laidOutView()->framing().has_value());
    CHECK(sampled->source.has_value());

    auto receipt = host.present(*sampled);
    REQUIRE(receipt);
    CHECK(receipt->status == PresentStatus::succeeded);
    CHECK(renderer.layoutSubmits == 1);
    CHECK(renderer.plainSubmits == 0);
    CHECK(renderer.presents == 1);
    CHECK(renderer.lastTarget.extent.width == 640);
    CHECK(renderer.lastTarget.extent.height == 480);

    REQUIRE(host.close());
}

TEST_CASE("T-06 host drops a deadline-expired attempt before any device call",
          "[rpa][t06][frame-chain]") {
    LayoutProbeRenderer renderer;
    std::int64_t clock = 1000;
    auto settings = options([&] { return clock; }, std::chrono::milliseconds{1000});
    settings.frameLayout = renderer.binding();
    auto made = player::ExecutionHost::create(renderer, settings, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation()));

    // Inside the deadline the same injected clock still submits and presents once.
    auto inside = host.sample(0, {0, 0, host.context().clockSegment}, false);
    REQUIRE(inside);
    clock = 1'000'999;
    auto accepted = host.present(*inside);
    REQUIRE(accepted);
    CHECK(accepted->status == PresentStatus::succeeded);
    CHECK(renderer.layoutSubmits == 1);
    CHECK(renderer.presents == 1);

    // At the exact deadline the attempt ends as a non-fault stale drop: zero submit, zero
    // present, no renderer failure, and the previous retained presentation stays valid.
    clock = 2'000'000;
    auto expired = host.sample(1, {0, 0, host.context().clockSegment}, false);
    REQUIRE(expired);
    clock = 3'000'000;
    auto dropped = host.present(*expired);
    REQUIRE(dropped);
    CHECK(dropped->status == PresentStatus::dropped);
    REQUIRE(dropped->cause);
    CHECK(dropped->cause->code() == "host.rpa.context_stale");
    CHECK(hasReason(*dropped->cause, "frame_deadline"));
    CHECK(renderer.layoutSubmits == 1);
    CHECK(renderer.plainSubmits == 0);
    CHECK(renderer.presents == 1);
    CHECK(host.state() == HostState::running);
    CHECK(host.deliveryStatus() == DeliveryStatus::expiredRequest);

    REQUIRE(host.close());
}

TEST_CASE("T-06 host drops a changed target and keeps the old retained frame",
          "[rpa][t06][frame-chain]") {
    LayoutProbeRenderer renderer;
    std::int64_t clock = 5000;
    auto settings = options([&] { return clock; }, std::chrono::seconds{5});
    settings.frameLayout = renderer.binding();
    auto made = player::ExecutionHost::create(renderer, settings, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation()));

    auto first = host.sample(0, {0, 0, host.context().clockSegment}, false);
    REQUIRE(first);
    REQUIRE(host.present(*first));

    auto changed = host.sample(1, {0.001, 0.001, host.context().clockSegment}, false);
    REQUIRE(changed);
    REQUIRE(host.setFrameTarget(experimental::FrameTarget{
        {1600, 1000}, {200, 50, 1280, 721}, experimental::FrameRotation::Deg0}));
    auto dropped = host.present(*changed);
    REQUIRE(dropped);
    CHECK(dropped->status == PresentStatus::dropped);
    REQUIRE(dropped->cause);
    CHECK(dropped->cause->code() == "host.rpa.context_stale");
    CHECK(hasReason(*dropped->cause, "frame_target_changed"));
    CHECK(renderer.layoutSubmits == 1);
    CHECK(renderer.presents == 1);
    CHECK(host.state() == HostState::running);
    // A caller that drops the stale attempt releases its backing before the next request.
    changed->payload.reset();

    // The old retained owning frame survives the drop; a frozen request relayouts it to the new
    // target from its own sampled projection and still reports the original source.
    auto retainedResult = host.trySample(2, {}, true);
    REQUIRE(retainedResult);
    INFO("delivery status " << static_cast<unsigned>(host.deliveryStatus()));
    auto retained = std::move(*retainedResult);
    REQUIRE(retained);
    REQUIRE(retained->laidOutView());
    REQUIRE(retained->source);
    CHECK(retained->source->resultVersion == first->source->resultVersion);
    CHECK(retained->layoutView()->target.extent.width == 1600);
    CHECK(retained->layoutView()->target.extent.height == 1000);
    CHECK(host.present(*retained));
    CHECK(renderer.layoutSubmits == 2);
    retained->payload.reset();
    first->payload.reset();

    // The next ordinary request uses the new target and presents normally.
    auto fresh = host.sample(3, {0.003, 0.001, host.context().clockSegment}, false);
    if (!fresh)
        UNSCOPED_INFO("fresh error: " << fresh.error().code());
    REQUIRE(fresh);
    CHECK(fresh->layoutView()->target.extent.width == 1600);
    CHECK(fresh->layoutView()->target.extent.height == 1000);
    REQUIRE(host.present(*fresh));
    CHECK(renderer.layoutSubmits == 3);
    CHECK(renderer.lastTarget.host.x == 200);
    CHECK(renderer.lastTarget.host.width == 1280);

    REQUIRE(host.close());
}

TEST_CASE("T-06 host finishes an empty laid-out target as skipped empty layout",
          "[rpa][t06][frame-chain]") {
    LayoutProbeRenderer renderer;
    std::int64_t clock = 9000;
    auto settings = options([&] { return clock; }, std::chrono::seconds{5});
    settings.frameLayout = renderer.binding();
    auto made = player::ExecutionHost::create(renderer, settings, {});
    REQUIRE(made);
    auto& host = **made;
    REQUIRE(host.load(test_support::presentation()));

    REQUIRE(host.setFrameTarget(
        experimental::FrameTarget{{0, 0}, {0, 0, 0, 0}, experimental::FrameRotation::Deg0}));
    auto empty = host.trySample(0, {}, false);
    REQUIRE(empty);
    CHECK_FALSE(*empty);
    CHECK(host.deliveryStatus() == DeliveryStatus::skippedEmptyLayout);
    CHECK(renderer.layoutSubmits == 0);
    CHECK(renderer.presents == 0);
    CHECK(host.state() == HostState::running);

    REQUIRE(host.close());
}
