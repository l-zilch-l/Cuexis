#pragma once

#include <cuexis/playback/playback_session.hpp>
#include <cuexis/runtime/presentation_read.hpp>

namespace cuexis::playback::detail {
// One current descriptor. Projection owns membership; this only names its revision.
// Source metadata is never relabelled by a delivery/control/query operation.
struct CommittedPresentationState final {
    std::uint64_t sessionScope{}, resultVersion{}, contentRevision{}, layoutRevision{},
        worldRevision{}, updateOrdinal{}, activationRevision{};
    runtime::detail::PresentationStamp runtime;
    internal::presentation::Source source{internal::presentation::Source::Prepared};
    std::optional<RuntimeFrame> frame;
    std::optional<std::int64_t> presentationTick, workTick;
    std::optional<std::uint64_t> factCursor, controlCursor;
    bool hasFold{};
};
struct PresentationAccess final {
    static auto state(const PlaybackSession&) -> core::Result<CommittedPresentationState>;
    static auto materialize(const PlaybackSession&, std::optional<std::uint64_t> exactVersion,
                            const FrameViewport&, FrameSnapshot&) -> core::Result<void>;
};
} // namespace cuexis::playback::detail
