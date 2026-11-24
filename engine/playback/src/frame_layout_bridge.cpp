// Committed laid-out frame construction and retained relayout for the experimental frame bridge.
// The unit reads the same committed descriptor and Runtime window as extractFrame, writes the one
// final projection into the snapshot, and never re-reads Runtime for a retained relayout.

#include "frame_layout_internal.hpp"
#include "playback_session_internal.hpp"
#include "presentation_materializer.hpp"
#include "presentation_state.hpp"
#include <cuexis/playback/frame_layout_access.hpp>

#include <cuexis/runtime/presentation_read.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::playback::experimental {

LaidOutFrame::~LaidOutFrame() = default;
LaidOutFrame::LaidOutFrame(LaidOutFrame&&) noexcept = default;
auto LaidOutFrame::operator=(LaidOutFrame&&) noexcept -> LaidOutFrame& = default;

auto LaidOutFrame::snapshot() const noexcept -> const FrameSnapshot& {
    return snapshot_;
}
auto LaidOutFrame::layout() const noexcept -> const FrameLayout& {
    return layout_;
}
auto LaidOutFrame::source() const noexcept -> const PresentationSourceInfo& {
    return source_;
}
auto LaidOutFrame::resourceToken() const noexcept -> const PresentationCandidateToken& {
    return token_;
}
auto LaidOutFrame::framing() const noexcept -> std::optional<FrameFraming> {
    return framing_;
}

auto preparedFramings(const PreparedPlayback& prepared) -> core::Result<std::vector<FrameFraming>> {
    return detail::FrameLayoutAccess::framings(prepared);
}

auto extractLaidOutFrame(const PlaybackSession& session, FrameTarget target,
                         std::optional<std::uint64_t> exactResultVersion, FramingRequest request)
    -> core::Result<std::optional<LaidOutFrame>> {
    return detail::FrameLayoutAccess::extract(session, target, exactResultVersion, request);
}

auto relayoutRetainedFrame(const LaidOutFrame& frame, FrameTarget target)
    -> core::Result<std::optional<LaidOutFrame>> {
    return detail::FrameLayoutAccess::relayout(frame, target);
}

} // namespace cuexis::playback::experimental

namespace cuexis::playback::detail {
namespace {

using experimental::AspectRatio;
using experimental::FrameFraming;
using experimental::FrameRotation;
using experimental::FramingRequest;
using experimental::LaidOutFrame;
using experimental::SelectedFrameLayout;

constexpr std::uint32_t maxAspectComponent = 2147483647U;
constexpr std::size_t maxFramingCount = 16;

[[nodiscard]] auto invalidFraming(std::size_t index, std::string_view reason) -> core::Error {
    return core::Error{"playback.presentation.layout.invalid_framing",
                       "A prepared framing entry is outside its accepted domain"}
        .withContext("index", std::to_string(index))
        .withContext("reason", std::string{reason});
}

[[nodiscard]] auto reducedAspect(AspectRatio aspect) noexcept -> AspectRatio {
    const auto divisor = std::gcd(aspect.width, aspect.height);
    return AspectRatio{aspect.width / divisor, aspect.height / divisor};
}

[[nodiscard]] auto makeSourceInfo(const CommittedPresentationState& state)
    -> PresentationSourceInfo {
    PresentationSourceInfo source{};
    switch (state.source) {
    case internal::presentation::Source::Prepared:
        source.source = PresentationSource::Prepared;
        break;
    case internal::presentation::Source::RuntimeUpdate:
        source.source = PresentationSource::RuntimeUpdate;
        break;
    case internal::presentation::Source::Restore:
        source.source = PresentationSource::Restore;
        break;
    case internal::presentation::Source::Mutation:
        source.source = PresentationSource::Mutation;
        break;
    }
    source.sessionScope = state.sessionScope;
    source.resultVersion = state.resultVersion;
    source.contentRevision = state.contentRevision;
    source.layoutRevision = state.layoutRevision;
    source.worldRevision = state.worldRevision;
    source.updateOrdinal = state.updateOrdinal;
    source.frame = state.frame;
    source.presentationTick = state.presentationTick;
    source.workTick = state.workTick;
    source.factCursor = state.factCursor;
    source.controlCursor = state.controlCursor;
    if (state.activationRevision) {
        source.activationRevision = state.activationRevision;
    }
    source.hasFold = state.hasFold;
    return source;
}

[[nodiscard]] auto effectiveFovY(double baseFovYDegrees, const std::optional<FrameFraming>& framing)
    -> double {
    if (!framing) {
        return baseFovYDegrees;
    }
    const auto halfAngle = baseFovYDegrees * 3.14159265358979323846 / 360.0;
    return 2.0 * std::atan(framing->verticalScale * std::tan(halfAngle)) * 180.0 /
           3.14159265358979323846;
}

// Inverts the effective vertical angle back to the unscaled camera sample stored in the frame.
[[nodiscard]] auto baseFovY(double effectiveFovYDegrees, const std::optional<FrameFraming>& framing)
    -> double {
    if (!framing) {
        return effectiveFovYDegrees;
    }
    const auto halfAngle = effectiveFovYDegrees * 3.14159265358979323846 / 360.0;
    return 2.0 * std::atan(std::tan(halfAngle) / framing->verticalScale) * 180.0 /
           3.14159265358979323846;
}

[[nodiscard]] auto buildBaseProjection(double fovYDegrees, double nearPlane, double farPlane,
                                       const SelectedFrameLayout& selected)
    -> core::Result<core::Mat4> {
    const auto width = static_cast<double>(selected.layout.content.width);
    const auto height = static_cast<double>(selected.layout.content.height);
    if (!(width > 0.0) || !(height > 0.0)) {
        return core::unexpected(frameProjectionError("content_rect"));
    }
    double aspect = 0.0;
    if (selected.framing) {
        aspect = static_cast<double>(selected.framing->aspect.width) /
                 static_cast<double>(selected.framing->aspect.height);
    } else if (selected.layout.target.rotation == FrameRotation::Deg90 ||
               selected.layout.target.rotation == FrameRotation::Deg270) {
        // Legacy quarter turn: the base projection uses the reciprocal display aspect and the
        // rotation is applied once by composeFrameProjection.
        aspect = height / width;
    } else {
        aspect = width / height;
    }
    auto projection = core::makePerspective(fovYDegrees * 3.14159265358979323846 / 180.0, aspect,
                                            nearPlane, farPlane);
    if (!projection) {
        return core::unexpected(
            core::Error{"playback.snapshot.camera_invalid", "Active camera parameters are invalid"}
                .withCause(projection.error()));
    }
    return *projection;
}

[[nodiscard]] auto normalizedRequest(FramingRequest request) -> FramingRequest {
    if (request.exact) {
        request.exact = reducedAspect(*request.exact);
    }
    return request;
}

} // namespace

auto FrameLayoutAccess::framings(const PreparedPlayback& prepared)
    -> core::Result<std::vector<FrameFraming>> try {
    if (!prepared.state_ || !prepared.state_->ownerThread.isCurrent() || !prepared.valid()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    return prepared.state_->framings;
} catch (...) {
    return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                        "Prepared framing table copy could not be allocated"}
                                .withContext("reason", "allocation"));
}

auto FrameLayoutAccess::attachFramings(PreparedPlayback& prepared,
                                       std::span<const FrameFraming> table)
    -> core::Result<void> try {
    if (!prepared.state_ || !prepared.state_->ownerThread.isCurrent() || !prepared.valid()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    if (table.size() > maxFramingCount) {
        return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                            "Prepared framing table exceeds its capacity"}
                                    .withContext("count", std::to_string(table.size())));
    }
    std::vector<AspectRatio> seen;
    seen.reserve(table.size());
    for (std::size_t index = 0; index < table.size(); ++index) {
        const auto& framing = table[index];
        if (framing.aspect.width == 0 || framing.aspect.height == 0 ||
            framing.aspect.width > maxAspectComponent ||
            framing.aspect.height > maxAspectComponent) {
            return core::unexpected(
                core::Error{"playback.presentation.layout.invalid_aspect",
                            "Frame aspect must use finite positive components within the accepted "
                            "range"}
                    .withContext("width", std::to_string(framing.aspect.width))
                    .withContext("height", std::to_string(framing.aspect.height))
                    .withContext("index", std::to_string(index)));
        }
        if (!std::isfinite(framing.verticalScale) || framing.verticalScale < 0.125 ||
            framing.verticalScale > 8.0) {
            return core::unexpected(invalidFraming(index, "vertical_scale"));
        }
        const auto reduced = reducedAspect(framing.aspect);
        if (reduced.width != framing.aspect.width || reduced.height != framing.aspect.height) {
            return core::unexpected(invalidFraming(index, "unreduced_aspect"));
        }
        const auto duplicate = std::find_if(seen.begin(), seen.end(), [&](AspectRatio candidate) {
            return candidate.width == reduced.width && candidate.height == reduced.height;
        });
        if (duplicate != seen.end()) {
            auto error = invalidFraming(index, "duplicate_aspect");
            error.withContext("duplicate_index",
                              std::to_string(static_cast<std::size_t>(duplicate - seen.begin())));
            return core::unexpected(std::move(error));
        }
        seen.push_back(reduced);
    }
    std::vector<FrameFraming> copy;
    copy.assign(table.begin(), table.end());
    prepared.state_->framings = std::move(copy);
    return {};
} catch (const std::bad_alloc&) {
    return core::unexpected(invalidFraming(0, "allocation"));
} catch (...) {
    return core::unexpected(invalidFraming(0, "allocation"));
}

auto FrameLayoutAccess::extract(const PlaybackSession& session, experimental::FrameTarget target,
                                std::optional<std::uint64_t> exactResultVersion,
                                experimental::FramingRequest request)
    -> core::Result<std::optional<LaidOutFrame>> try {
    auto committed = PresentationAccess::state(session);
    if (!committed) {
        return core::unexpected(std::move(committed.error()));
    }
    if (exactResultVersion && *exactResultVersion != committed->resultVersion) {
        return core::unexpected(core::Error{"playback.presentation.version_unavailable",
                                            "Requested presentation version is unavailable"});
    }
    auto& sessionState = *session.state_;
    SessionOperation operation{sessionState.operationActive};
    const auto& table = sessionState.framings;
    auto selected = experimental::computeFrameLayout(table, target, request);
    if (!selected) {
        return core::unexpected(std::move(selected.error()));
    }
    if (!selected->has_value()) {
        return std::optional<LaidOutFrame>{};
    }
    const auto& layout = (*selected)->layout;
    const FrameViewport viewport{layout.content.width, layout.content.height};
    FrameSnapshot snapshot;
    auto extracted = runtime::detail::PresentationAccess::read(
        std::as_const(*sessionState.runtimeSession), committed->runtime,
        [&](const runtime::detail::PresentationRead& view) {
            return materializeSnapshot(view, sessionState.snapshotLayout, viewport, snapshot,
                                       **selected);
        });
    if (!extracted) {
        return core::unexpected(std::move(extracted.error()));
    }

    LaidOutFrame result;
    result.snapshot_ = std::move(snapshot);
    result.layout_ = layout;
    result.source_ = makeSourceInfo(*committed);
    result.token_ = sessionState.committedPresentationToken.value_or(PresentationCandidateToken{});
    result.framing_ = (*selected)->framing;
    result.framingCount_ = static_cast<std::uint8_t>(table.size());
    std::copy(table.begin(), table.end(), result.framings_.begin());
    result.request_ = normalizedRequest(request);
    result.baseFovY_ = baseFovY(result.snapshot_.camera.fovY, result.framing_);
    result.nearPlane_ = result.snapshot_.camera.nearPlane;
    result.farPlane_ = result.snapshot_.camera.farPlane;
    return std::optional<LaidOutFrame>{std::move(result)};
} catch (const std::bad_alloc&) {
    return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                        "Laid-out frame extraction could not allocate memory"}
                                .withContext("reason", "allocation"));
} catch (...) {
    return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                        "Laid-out frame extraction failed"});
}

auto FrameLayoutAccess::relayout(const LaidOutFrame& frame, experimental::FrameTarget target)
    -> core::Result<std::optional<LaidOutFrame>> try {
    const std::span<const FrameFraming> table{frame.framings_.data(), frame.framingCount_};
    auto selected = experimental::computeFrameLayout(table, target, frame.request_);
    if (!selected) {
        return core::unexpected(std::move(selected.error()));
    }
    if (!selected->has_value()) {
        return std::optional<LaidOutFrame>{};
    }
    LaidOutFrame result;
    result.snapshot_ = frame.snapshot_;
    result.layout_ = (*selected)->layout;
    result.source_ = frame.source_;
    result.token_ = frame.token_;
    result.framing_ = (*selected)->framing;
    result.framings_ = frame.framings_;
    result.framingCount_ = frame.framingCount_;
    result.request_ = frame.request_;
    result.baseFovY_ = frame.baseFovY_;
    result.nearPlane_ = frame.nearPlane_;
    result.farPlane_ = frame.farPlane_;
    if (result.snapshot_.camera.active) {
        auto base =
            buildBaseProjection(frame.baseFovY_, frame.nearPlane_, frame.farPlane_, **selected);
        if (!base) {
            return core::unexpected(std::move(base.error()));
        }
        auto composed = composeFrameProjection(*base, **selected);
        if (!composed) {
            return core::unexpected(std::move(composed.error()));
        }
        std::copy(composed->values.begin(), composed->values.end(),
                  result.snapshot_.camera.projectionMatrix);
        result.snapshot_.camera.fovY = effectiveFovY(frame.baseFovY_, result.framing_);
    }
    result.snapshot_.viewportWidth = result.layout_.content.width;
    result.snapshot_.viewportHeight = result.layout_.content.height;
    return std::optional<LaidOutFrame>{std::move(result)};
} catch (const std::bad_alloc&) {
    return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                        "Retained frame relayout could not allocate memory"}
                                .withContext("reason", "allocation"));
} catch (...) {
    return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                        "Retained frame relayout failed"});
}

} // namespace cuexis::playback::detail
