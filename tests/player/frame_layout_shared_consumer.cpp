// In-repo shared-library consumer for the experimental laid-out frame bridge. The program is run
// by the rr_frame_layout_shared CTest after the shared Player is built, so it links the same
// Cuexis::Playback binary the Player does and proves construction, move, relayout and destruction
// against the installed/source contract values rather than a header-only reimplementation.

#include <cuexis/playback/experimental/frame_layout.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <utility>

namespace {

using namespace cuexis;
using namespace cuexis::playback;

auto fail(std::string_view message) -> int {
    std::fprintf(stderr, "frame_layout_shared: %.*s\n", static_cast<int>(message.size()),
                 message.data());
    return 1;
}

} // namespace

int main() {
    const std::filesystem::path project =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets/projects/stage3_project";
    PlaybackSession session;
    auto source = PlaybackSource::fromFilesystemProject(project);
    if (!source)
        return fail("stage3 project source did not load");
    auto prepared = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
    if (!prepared)
        return fail("stage3 project did not prepare");
    if (auto committed = session.commit(std::move(*prepared)); !committed)
        return fail("stage3 project did not commit");
    if (auto updated = session.update({0, 0, 0}); !updated)
        return fail("stage3 project did not update");

    const experimental::FrameTarget target{
        {1280, 720}, {0, 0, 1280, 720}, experimental::FrameRotation::Deg0};
    auto extracted = experimental::extractLaidOutFrame(session, target);
    if (!extracted)
        return fail("laid-out extraction reported an error");
    if (!*extracted)
        return fail("laid-out extraction returned an empty frame");

    // Construct, move construct, and read all five accessors while the first frame is alive.
    auto first = std::move(**extracted);
    const auto& firstSnapshot = first.snapshot();
    const auto& firstLayout = first.layout();
    const auto& firstSource = first.source();
    const auto& firstToken = first.resourceToken();
    const auto firstFraming = first.framing();
    if (firstLayout.target.extent.width != target.extent.width ||
        firstLayout.target.extent.height != target.extent.height)
        return fail("the committed layout does not carry the requested target");
    if (firstSnapshot.viewportWidth != firstLayout.content.width ||
        firstSnapshot.viewportHeight != firstLayout.content.height)
        return fail("the snapshot viewport is not the laid-out content size");
    if (firstFraming.has_value())
        return fail("the legacy fixture unexpectedly selected an author framing");

    char digestText[32]{};
    if (firstSource.frame) {
        auto digest = computeFrameDigest(*firstSource.frame, firstSnapshot);
        if (!digest)
            return fail("frame digest failed");
        std::snprintf(digestText, sizeof(digestText), "%016llx",
                      static_cast<unsigned long long>(digest->value));
    }
    const auto firstContentWidth = firstLayout.content.width;
    const auto firstContentHeight = firstLayout.content.height;

    auto moved = std::move(first);
    const bool tokenStable = moved.resourceToken() == firstToken;
    if (!tokenStable)
        return fail("move construction changed the committed resource token");

    const experimental::FrameTarget resizedTarget{
        {1600, 1000}, {200, 50, 1280, 721}, experimental::FrameRotation::Deg0};
    auto relaid = experimental::relayoutRetainedFrame(moved, resizedTarget);
    if (!relaid)
        return fail("retained relayout reported an error");
    if (!*relaid)
        return fail("retained relayout returned an empty frame");
    if ((*relaid)->layout().target.extent.width != 1600 ||
        (*relaid)->source().resultVersion != moved.source().resultVersion)
        return fail("retained relayout changed the target or re-read the source version");

    experimental::LaidOutFrame assigned = std::move(**relaid);
    assigned = std::move(moved);
    if (assigned.layout().target.extent.width != 1280)
        return fail("move assignment did not transfer the owning frame");

    std::printf("frame_layout_shared ok digest=%s viewport=%ux%u relayout=%ux%u token_stable=%d\n",
                digestText[0] != '\0' ? digestText : "none", firstContentWidth, firstContentHeight,
                assigned.layout().target.extent.width, assigned.layout().target.extent.height,
                tokenStable ? 1 : 0);
    return 0;
}
