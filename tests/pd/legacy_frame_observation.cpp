#include "legacy_frame_observation.hpp"

#include <algorithm>

#include <cuexis_internal/presentation_metrics.hpp>

namespace cuexis::playback::detail {
namespace baseline {

namespace {

auto error(std::string_view code) -> core::Error {

    return {"host.rpa." + std::string{code}, "Frame observation protocol rejected"};
}

} // namespace

auto rematerializeRetainedSnapshot(const FrameSnapshot& retained, FrameViewport viewport)

    -> core::Result<FrameSnapshot> try {

    if (!viewport.width || !viewport.height)

        return core::unexpected(core::Error{"playback.viewport.invalid",

                                            "FrameViewport width and height must be positive"});

    FrameSnapshot result = retained;

    result.viewportWidth = viewport.width;

    result.viewportHeight = viewport.height;

    if (result.camera.active) {

        const auto projection =

            core::makePerspective(result.camera.fovY * 3.14159265358979323846 / 180.0,

                                  static_cast<double>(viewport.width) / viewport.height,

                                  result.camera.nearPlane, result.camera.farPlane);

        if (!projection)

            return core::unexpected(core::Error{"playback.snapshot.camera_invalid",

                                                "Active camera parameters are invalid"}

                                        .withCause(projection.error()));

        std::copy(projection->values.begin(), projection->values.end(),

                  result.camera.projectionMatrix);
    }

    return result;

} catch (...) {

    return core::unexpected(error("capacity_exceeded"));
}

FrameObservationLedger::FrameObservationLedger(

    HostContext c, std::uint64_t scope, std::vector<std::optional<FrameRecord>> frames,

    std::vector<std::optional<FeedbackObservation>> feedback, std::vector<std::uint64_t> scopes)

    : context_(c), scope_(scope), frames_(std::move(frames)), feedback_(std::move(feedback)),

      scopeHistory_(std::move(scopes)) {

    scopeHistory_[0] = scope;
}

void FrameObservationLedger::recordMissing() noexcept {

    if (fault_.reason == FaultSlot::Reason::none) {

        fault_ = {FaultSlot::Reason::capacity, {}};

        if (unknownCount_ != UINT64_MAX)

            ++unknownCount_;
    }
}

auto FrameObservationLedger::create(HostContext c, std::uint64_t scope, std::size_t fc,

                                    std::size_t bc) -> core::Result<FrameObservationLedger> try {

    if (!c.sessionScope || !scope || !fc || !bc || fc > SIZE_MAX - bc)

        return core::unexpected(error("profile_incomplete"));

    return FrameObservationLedger{c, scope, std::vector<std::optional<FrameRecord>>(fc),

                                  std::vector<std::optional<FeedbackObservation>>(bc),

                                  std::vector<std::uint64_t>(fc + bc)};

} catch (...) {

    return core::unexpected(error("capacity_exceeded"));
}

auto FrameObservationLedger::feedback(FeedbackId id, std::int64_t created, std::int64_t expiry)

    -> core::Result<void> try {

    if (!owner_.isCurrent() || id.projectionScope != scope_ || expiry < created)

        return core::unexpected(error("context_stale"));

    for (const auto& f : feedback_)

        if (f && f->id == id) {

            if (f->created != created || f->expiry != expiry)

                return core::unexpected(error("message_conflict"));

            return {};
        }

    if (fault_.reason != FaultSlot::Reason::none)

        return core::unexpected(error("capacity_exceeded"));

    auto slot = std::find_if(feedback_.begin(), feedback_.end(), [](const auto& f) { return !f; });

    if (slot == feedback_.end()) {

        recordMissing();

        return core::unexpected(error("capacity_exceeded"));
    }

    slot->emplace(

        FeedbackObservation{std::move(id), created, expiry, FeedbackStatus::created, false});

    return {};

} catch (...) {

    fault_ = {FaultSlot::Reason::allocation, {}};

    return core::unexpected(allocationFailure_);
}

auto FrameObservationLedger::publish(FrameEnvelope frame) -> core::Result<void> {

    return publishImpl(std::move(frame), false);
}

auto FrameObservationLedger::publishRetained(FrameEnvelope frame,

                                             const RetainedPresentation& retained)

    -> core::Result<void> {

    if (!owner_.isCurrent())

        return core::unexpected(error("owner_mismatch"));

    const auto& original = retained.frame();

    if (!original || original->projectionScope != scope_ ||

        original->context.sessionScope != context_.sessionScope ||

        original->context.contentIdentity != context_.contentIdentity ||

        original->context.resourceGeneration != context_.resourceGeneration ||

        original->context.rendererGeneration != context_.rendererGeneration ||

        frame.presentationTick != original->presentationTick ||

        frame.membership != original->membership)

        return core::unexpected(error("context_stale"));

    return publishImpl(std::move(frame), true);
}

auto FrameObservationLedger::publishImpl(FrameEnvelope frame, bool retained)

    -> core::Result<void> try {

    internal::pd::Timer measurement{internal::pd::Stage::ledgerPublish};

    if (!owner_.isCurrent() || frame.context != context_ || frame.projectionScope != scope_ ||

        !frame.frameId || frame.frameId <= lastFrame_ ||

        (!retained && lastTick_ && frame.presentationTick && *frame.presentationTick < *lastTick_))

        return core::unexpected(error("context_stale"));

    for (const auto& id : frame.membership) {

        auto f = query(id);

        if (!f || !frame.presentationTick || id.projectionScope != scope_ ||

            *frame.presentationTick < f->created || *frame.presentationTick >= f->expiry)

            return core::unexpected(error("context_stale"));
    }

    if (fault_.reason != FaultSlot::Reason::none)

        return core::unexpected(error("capacity_exceeded"));

    auto slot = std::find_if(frames_.begin(), frames_.end(), [](const auto& f) { return !f; });

    if (slot == frames_.end()) {

        recordMissing();

        return core::unexpected(error("capacity_exceeded"));
    }

    // Merged frames never count as confirmed present; their complete membership remains historical.

    if (latest_)

        frames_[*latest_]->receipt = PresentReceipt{frames_[*latest_]->frame.context,

                                                    frames_[*latest_]->frame.frameId,

                                                    frames_[*latest_]->frame.projectionScope,

                                                    PresentStatus::unknown,

                                                    0,

                                                    {}};

    lastFrame_ = frame.frameId;

    if (!retained && frame.presentationTick)

        lastTick_ = frame.presentationTick;

    slot->emplace(FrameRecord{std::move(frame), {}, false});

    latest_ = static_cast<std::size_t>(slot - frames_.begin());

    return {};

} catch (...) {

    fault_ = {FaultSlot::Reason::allocation, {}};

    return core::unexpected(allocationFailure_);
}

auto FrameObservationLedger::takeLatest() -> core::Result<std::optional<FrameEnvelope>> try {

    internal::pd::Timer measurement{internal::pd::Stage::ledgerTake};

    if (!owner_.isCurrent())

        return core::unexpected(error("owner_mismatch"));

    if (!latest_)

        return std::optional<FrameEnvelope>{};

    auto& record = *frames_[*latest_];

    // Copy first: allocation failure must leave the retained frame available for retry.

    std::optional<FrameEnvelope> result{record.frame};

    record.submitted = true;

    latest_.reset();

    return result;

} catch (...) {

    fault_ = {FaultSlot::Reason::allocation, {}};

    return core::unexpected(allocationFailure_);
}

auto FrameObservationLedger::observe(const PresentReceipt& r) -> core::Result<void> try {

    internal::pd::Timer measurement{internal::pd::Stage::receipt};

    if (!owner_.isCurrent())

        return core::unexpected(error("owner_mismatch"));

    auto record = std::find_if(frames_.begin(), frames_.end(), [&](const auto& f) {
        return f && f->frame.frameId == r.frameId && f->frame.context == r.context &&

               f->frame.projectionScope == r.projectionScope;
    });

    if (record == frames_.end())

        return core::unexpected(error("context_stale"));

    auto& f = **record;

    if (!f.submitted)

        return core::unexpected(error("message_conflict"));

    if (f.receipt) {

        if (f.receipt->status == r.status && f.receipt->callMonotonicUs == r.callMonotonicUs &&

            sameHostError(f.receipt->cause, r.cause))

            return {};

        if (f.receipt->status != PresentStatus::unknown || r.status == PresentStatus::unknown)

            return core::unexpected(error("message_conflict"));
    }

    auto recorded = r;

    f.receipt = std::move(recorded);

    for (const auto& id : f.frame.membership)

        for (auto& observation : feedback_)

            if (observation && observation->id == id) {

                if (r.status == PresentStatus::succeeded) {

                    if (observation->status == FeedbackStatus::created &&

                        id.projectionScope == scope_)

                        observation->status = FeedbackStatus::confirmedPresent;

                    else if (observation->status != FeedbackStatus::confirmedPresent)

                        observation->laterConfirmed = true;

                } else if (r.status == PresentStatus::unknown &&

                           observation->status == FeedbackStatus::created) {

                    observation->status = FeedbackStatus::unknownEvidence;

                    if (unknownCount_ != UINT64_MAX)

                        ++unknownCount_;
                }
            }

    return {};

} catch (...) {

    fault_ = {FaultSlot::Reason::allocation, {}};

    return core::unexpected(allocationFailure_);
}

auto FrameObservationLedger::expire(std::int64_t tick) -> core::Result<void> try {

    if (!owner_.isCurrent())

        return core::unexpected(error("owner_mismatch"));

    for (auto& f : feedback_)

        if (f && f->id.projectionScope == scope_ && f->expiry <= tick &&

            f->status == FeedbackStatus::created) {

            bool missing = false;

            for (const auto& record : frames_)

                if (record &&

                    std::find(record->frame.membership.begin(), record->frame.membership.end(),

                              f->id) != record->frame.membership.end() &&

                    record->submitted &&

                    (!record->receipt || record->receipt->status == PresentStatus::unknown))

                    missing = true;

            f->status =

                missing ? FeedbackStatus::unknownEvidence : FeedbackStatus::expiredUnconfirmed;

            if (missing && unknownCount_ != UINT64_MAX)

                ++unknownCount_;
        }

    return {};

} catch (...) {

    fault_ = {FaultSlot::Reason::allocation, {}};

    return core::unexpected(allocationFailure_);
}

auto FrameObservationLedger::rebind(HostContext context) -> core::Result<void> {

    if (!owner_.isCurrent())

        return core::unexpected(error("owner_mismatch"));

    if (context == context_)

        return {};

    auto clockOnly = context_;

    clockOnly.clockSegment = context.clockSegment;

    const bool forwardClock = context.clockSegment > context_.clockSegment && context == clockOnly;

    if (context.sessionScope != context_.sessionScope ||

        context.contentIdentity != context_.contentIdentity ||

        context.resourceGeneration != context_.resourceGeneration ||

        context.rendererGeneration != context_.rendererGeneration ||

        context.clockSegment < context_.clockSegment || context.runEpoch < context_.runEpoch ||

        (context.controlSequence <= context_.controlSequence && !forwardClock))

        return core::unexpected(error("context_stale"));

    // Old attempts remain immutable and late receipts still match their original context.

    // Dropping admission of an unclaimed old frame does not invent presentation evidence.

    latest_.reset();

    context_ = context;

    return {};
}

auto FrameObservationLedger::activate(HostContext context, std::uint64_t scope)

    -> core::Result<void> try {

    auto clockOnly = context_;

    clockOnly.clockSegment = context.clockSegment;

    const bool forwardClock = context.clockSegment > context_.clockSegment && context == clockOnly;

    if (!owner_.isCurrent() || !scope || scope == scope_ || context.sessionScope == 0 ||

        (context.controlSequence <= context_.controlSequence && !forwardClock))

        return core::unexpected(error("context_stale"));

    if (std::find(scopeHistory_.begin(), scopeHistory_.begin() + scopeCount_, scope) !=

        scopeHistory_.begin() + scopeCount_)

        return core::unexpected(error("context_stale"));

    if (scopeCount_ == scopeHistory_.size()) {

        recordMissing();

        return core::unexpected(error("capacity_exceeded"));
    }

    scopeHistory_[scopeCount_++] = scope;

    for (auto& f : feedback_)

        if (f && f->id.projectionScope == scope_ && f->status == FeedbackStatus::created)

            f->status = FeedbackStatus::canceledByControl;

    latest_.reset();

    context_ = context;

    scope_ = scope;

    lastTick_.reset();

    return {};

} catch (...) {

    fault_ = {FaultSlot::Reason::allocation, {}};

    return core::unexpected(allocationFailure_);
}

auto FrameObservationLedger::query(const FeedbackId& id) const

    -> core::Result<FeedbackObservation> try {

    if (!owner_.isCurrent())

        return core::unexpected(error("owner_mismatch"));

    for (const auto& f : feedback_)

        if (f && f->id == id)

            return *f;

    return core::unexpected(error("context_stale"));

} catch (...) {

    fault_ = {FaultSlot::Reason::allocation, {}};

    return core::unexpected(allocationFailure_);
}

} // namespace baseline
} // namespace cuexis::playback::detail
