#include "frame_observation.hpp"
#include <algorithm>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/presentation_metrics.hpp>

namespace cuexis::playback::detail {
namespace {
auto error(std::string_view code) -> core::Error {
    return {"host.rpa." + std::string{code}, "Frame observation protocol rejected"};
}
auto idBytes(const FeedbackId& id) noexcept -> std::size_t {
    return sizeof(FeedbackId) + id.targetId.capacity() + id.bindingId.capacity() + 2;
}
auto feedbackBytes(const FeedbackObservation& f) noexcept -> std::size_t {
    return sizeof(FeedbackObservation) + idBytes(f.id);
}
auto receiptBytes(const PresentReceipt& r) noexcept -> std::size_t {
    std::size_t bytes = 0, depth = 0;
    for (auto* e = r.cause ? &*r.cause : nullptr; e; e = e->cause()) {
        if (++depth > 16)
            return SIZE_MAX;
        const auto add = [&](std::size_t n) {
            if (n > 4096 || bytes > 8192 - n)
                return false;
            bytes += n;
            return true;
        };
        if (!add(sizeof(core::Error) + 64) ||
            !add(e->context().capacity() * sizeof(core::ErrorContext)) ||
            !add(e->code().size() + 32) || !add(e->message().size() + 32))
            return SIZE_MAX;
        for (const auto& c : e->context())
            if (!add(c.key.capacity() + 1) || !add(c.value.capacity() + 1))
                return SIZE_MAX;
    }
    return bytes;
}
} // namespace
auto copyPresentReceipt(const PresentReceipt& input) -> core::Result<PresentReceipt> try {
    std::array<const core::Error*, 16> chain{};
    std::size_t count = 0, bytes = 0;
    for (auto* e = input.cause ? &*input.cause : nullptr; e; e = e->cause()) {
        if (count == chain.size() || e->context().size() > 64)
            return core::unexpected(error("context_stale"));
        const auto add = [&](std::size_t n) {
            if (n > 4096 || bytes > 4096 - n)
                return false;
            bytes += n;
            return true;
        };
        if (!add(e->code().size()) || !add(e->message().size()))
            return core::unexpected(error("context_stale"));
        for (const auto& c : e->context())
            if (!add(sizeof(core::ErrorContext)) || !add(c.key.size()) || !add(c.value.size()))
                return core::unexpected(error("context_stale"));
        chain[count++] = e;
    }
    std::optional<core::Error> cause;
    while (count) {
        const auto& e = *chain[--count];
        core::Error copy{std::string{e.code()}, std::string{e.message()}};
        for (const auto& c : e.context())
            copy.withContext(std::string{c.key.data(), c.key.size()},
                             std::string{c.value.data(), c.value.size()});
        if (cause)
            copy.withCause(std::move(*cause));
        cause = std::move(copy);
    }
    auto result = input;
    result.cause = std::move(cause);
    return result;
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
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
    std::vector<std::optional<FeedbackObservation>> feedback)
    : context_(c), scope_(scope), frames_(std::move(frames)), feedback_(std::move(feedback)) {}
auto FrameObservationLedger::configure(Limits limits) -> core::Result<void> try {
    if (!owner_.isCurrent() || !limits.bytes || !limits.tombstones || !limits.coverageIntervals ||
        !limits.members || limits.retentionUs <= 0 || limits.receiptDeadlineUs <= 0 || lastFrame_)
        return core::unexpected(error("profile_incomplete"));
    std::vector<std::optional<FeedbackObservation>>(limits.tombstones).swap(tombstones_);
    feedbackTimes_.resize(feedback_.size());
    tombstoneTimes_.resize(limits.tombstones);
    std::vector<Coverage> coverage;
    coverage.reserve(limits.coverageIntervals);
    coverage_.swap(coverage);
    limits_ = limits;
    journalBytes_ =
        frames_.capacity() * sizeof(frames_[0]) + feedback_.capacity() * sizeof(feedback_[0]) +
        tombstones_.capacity() * sizeof(tombstones_[0]) +
        (feedbackTimes_.capacity() + tombstoneTimes_.capacity()) * sizeof(std::int64_t) +
        coverage_.capacity() * sizeof(Coverage);
    if (journalBytes_ > limits.bytes)
        return core::unexpected(error("profile_incomplete"));
    peakJournalBytes_ = journalBytes_;
    return {};
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
void FrameObservationLedger::rememberLoss(std::uint64_t first, std::uint64_t last) noexcept {
    historyLost_ = true;
    if (!first)
        return;
    if (!coverage_.empty() &&
        first <= coverage_.back().last + (coverage_.back().last != UINT64_MAX)) {
        coverage_.back().last = std::max(last, coverage_.back().last);
    } else if (coverage_.size() < limits_.coverageIntervals) {
        coverage_.push_back({first, last});
    } else {
        // Collapse holes into a conservative incomplete interval, never an unbounded gap list.
        coverage_.front().first = std::min(first, coverage_.front().first);
        coverage_.front().last = std::max(last, coverage_.back().last);
        coverage_.resize(1);
    }
}
void FrameObservationLedger::dropAttempt(std::size_t index) noexcept {
    auto& slot = frames_[index];
    if (!slot)
        return;
    attemptsEvicted_ = true;
    rememberLoss(slot->frameId, slot->frameId);
    if ((slot->submitted ||
         (slot->dispatch && slot->dispatch->state.load() != DispatchState::notSubmitted)) &&
        (!slot->receipt || slot->receipt->status == PresentStatus::unknown)) {
        for (const auto& id : slot->membership)
            for (auto& f : feedback_)
                if (f && f->id == id && f->status == FeedbackStatus::created)
                    f->status = FeedbackStatus::unknownEvidence;
        recordMissing();
    }
    journalBytes_ -= slot->bytes;
    slot.reset();
}
void FrameObservationLedger::dropFeedback(std::size_t index) noexcept {
    auto& slot = feedback_[index];
    if (!slot)
        return;
    feedbackEvicted_ = true;
    recordMissing();
    if (slot->id.projectionScope == scope_)
        feedbackLossCommitThrough_ = std::max(feedbackLossCommitThrough_, slot->id.commitId);
    if (slot->id.projectionScope == scope_)
        feedbackLossThrough_ = std::max(feedbackLossThrough_.value_or(INT64_MIN), slot->created);
    if (slot->status == FeedbackStatus::created)
        slot->status = FeedbackStatus::unknownEvidence;
    auto& old = tombstones_[tombstoneNext_];
    if (old)
        journalBytes_ -= feedbackBytes(*old);
    old = std::move(slot);
    tombstoneTimes_[tombstoneNext_] = feedbackTimes_[index];
    slot.reset();
    tombstoneNext_ = (tombstoneNext_ + 1) % tombstones_.size();
}
void FrameObservationLedger::trim(std::size_t needed) noexcept {
    for (std::size_t n = 0;
         journalBytes_ > limits_.bytes - std::min(needed, limits_.bytes) && n < frames_.size(); ++n)
        dropAttempt((nextFrame_ + n) % frames_.size());
    for (std::size_t n = 0;
         journalBytes_ > limits_.bytes - std::min(needed, limits_.bytes) && n < tombstones_.size();
         ++n) {
        auto& old = tombstones_[(tombstoneNext_ + n) % tombstones_.size()];
        if (old) {
            journalBytes_ -= feedbackBytes(*old);
            old.reset();
            recordMissing();
        }
    }
}
void FrameObservationLedger::maintain(std::int64_t now) noexcept {
    if (!owner_.isCurrent())
        return;
    now_ = std::max(now_, now);
    const auto elapsed = [&](std::int64_t at, std::int64_t duration) {
        return now_ >= at && static_cast<std::uint64_t>(now_) - static_cast<std::uint64_t>(at) >=
                                 static_cast<std::uint64_t>(duration);
    };
    for (std::size_t i = 0; i < frames_.size(); ++i)
        if (auto& f = frames_[i]; f) {
            if (elapsed(f->insertedAt, limits_.retentionUs)) {
                dropAttempt(i);
                continue;
            }
            if (elapsed(f->insertedAt, limits_.receiptDeadlineUs) && !f->receipt &&
                (f->submitted ||
                 (f->dispatch && f->dispatch->state.load() == DispatchState::transferred))) {
                // Preserve the unknown receipt so an eventual success is only late evidence.
                f->receipt = PresentReceipt{
                    f->context, f->frameId, f->projectionScope, PresentStatus::unknown, 0, {}};
                for (const auto& id : f->membership)
                    for (auto& b : feedback_)
                        if (b && b->id == id && b->status == FeedbackStatus::created)
                            b->status = FeedbackStatus::unknownEvidence;
                recordMissing();
            }
        }
    for (std::size_t i = 0; i < feedback_.size(); ++i)
        if (feedback_[i] && elapsed(feedbackTimes_[i], limits_.retentionUs))
            dropFeedback(i);
    for (std::size_t i = 0; i < tombstones_.size(); ++i)
        if (tombstones_[i] && elapsed(tombstoneTimes_[i], limits_.retentionUs)) {
            journalBytes_ -= feedbackBytes(*tombstones_[i]);
            tombstones_[i].reset();
            recordMissing();
        }
}
auto FrameObservationLedger::coverage() const noexcept -> CoverageView {
    std::uint64_t first = UINT64_MAX, last = 0;
    for (const auto& f : frames_)
        if (f) {
            first = std::min(first, f->frameId);
            last = std::max(last, f->frameId);
        }
    return {coverage_,        last ? first : 0, last,         unknownCount_, false,
            attemptsEvicted_, feedbackEvicted_, receiptLoss_, historyMisses_};
}
void FrameObservationLedger::recordMissing() noexcept {
    historyLost_ = true;
    if (unknownCount_ != UINT64_MAX)
        ++unknownCount_;
}
void FrameObservationLedger::coverageLost() noexcept {
    recordMissing();
    for (auto& f : feedback_)
        if (f && f->status == FeedbackStatus::created)
            f->status = FeedbackStatus::unknownEvidence;
}
void FrameObservationLedger::receiptLost(std::uint64_t sessionFirst, std::uint64_t sessionLast,
                                         std::uint64_t scopeFirst, std::uint64_t scopeLast,
                                         std::uint64_t frameFirst,
                                         std::uint64_t frameLast) noexcept {
    receiptLoss_ = true;
    recordMissing();
    for (const auto& f : frames_)
        if (f && f->context.sessionScope >= sessionFirst &&
            f->context.sessionScope <= sessionLast && f->projectionScope >= scopeFirst &&
            f->projectionScope <= scopeLast && f->frameId >= frameFirst &&
            f->frameId <= frameLast &&
            (!f->receipt || f->receipt->status == PresentStatus::unknown)) {
            for (const auto& id : f->membership)
                for (auto* table : {&feedback_, &tombstones_})
                    for (auto& b : *table)
                        if (b && b->id == id && b->status == FeedbackStatus::created)
                            b->status = FeedbackStatus::unknownEvidence;
        }
}
auto FrameObservationLedger::retainedAttempts() const noexcept -> std::size_t {
    return std::count_if(frames_.begin(), frames_.end(), [](const auto& f) { return bool(f); });
}
auto FrameObservationLedger::create(HostContext c, std::uint64_t scope, std::size_t fc,
                                    std::size_t bc) -> core::Result<FrameObservationLedger> try {
    const internal::pd::memory::ExecutionTagScope memoryScope{
        internal::pd::memory::ownerPlaybackWorker, internal::pd::memory::domainJournal};
    if (!c.sessionScope || !scope || !fc || !bc || fc > SIZE_MAX - bc)
        return core::unexpected(error("profile_incomplete"));
    FrameObservationLedger result{c, scope, std::vector<std::optional<FrameRecord>>(fc),
                                  std::vector<std::optional<FeedbackObservation>>(bc)};
    auto configured = result.configure(Limits{});
    if (!configured)
        return core::unexpected(configured.error());
    return result;
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
auto FrameObservationLedger::feedback(FeedbackId id, std::int64_t created,
                                      std::optional<GameplayPresentationTick> expiry)
    -> core::Result<void> try {
    if (!owner_.isCurrent() || id.projectionScope != scope_ || (expiry && expiry->value <= created))
        return core::unexpected(error("context_stale"));
    for (const auto& f : feedback_)
        if (f && f->id == id) {
            if (f->created != created ||
                (bool(f->expiry) != bool(expiry) || (expiry && f->expiry->value != expiry->value)))
                return core::unexpected(error("message_conflict"));
            return {};
        }
    for (const auto& old : tombstones_)
        if (old && old->id == id) {
            if (old->created != created || (bool(old->expiry) != bool(expiry) ||
                                            (expiry && old->expiry->value != expiry->value)))
                return core::unexpected(error("message_conflict"));
            return {};
        }
    if (id.targetId.size() > 256 || id.bindingId.size() > 256) {
        coverageLost();
        return {};
    }
    const auto initial = feedbackLossThrough_ && created <= *feedbackLossThrough_ &&
                                 id.commitId <= feedbackLossCommitThrough_
                             ? FeedbackStatus::unknownEvidence
                             : FeedbackStatus::created;
    FeedbackObservation incoming{std::move(id), created, expiry, initial, false};
    const auto bytes = feedbackBytes(incoming);
    dropFeedback(nextFeedback_);
    trim(bytes);
    if (bytes > limits_.bytes || journalBytes_ > limits_.bytes - bytes) {
        feedbackLossThrough_ = std::max(feedbackLossThrough_.value_or(INT64_MIN), created);
        feedbackLossCommitThrough_ = std::max(feedbackLossCommitThrough_, incoming.id.commitId);
        coverageLost();
        return {};
    }
    auto& slot = feedback_[nextFeedback_];
    slot.emplace(std::move(incoming));
    journalBytes_ += bytes;
    peakJournalBytes_ = std::max(peakJournalBytes_, journalBytes_);
    feedbackTimes_[nextFeedback_] = now_;
    nextFeedback_ = (nextFeedback_ + 1) % feedback_.size();
    return {};
} catch (...) {
    coverageLost();
    return {};
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
        frame.membershipView() != original->membershipView())
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
    for (const auto& id : frame.membershipView()) {
        auto f = query(id);
        if (id.projectionScope != scope_ || !frame.presentationTick ||
            (f && (*frame.presentationTick < f->created ||
                   (f->expiry && *frame.presentationTick >= f->expiry->value))) ||
            (!f && !historyLost_))
            return core::unexpected(error("context_stale"));
    }
    if (latest_) {
        // This owning response has never been transferred: dropping it is not an unknown call.
        latestFrame_.reset();
        latest_.reset();
    }
    lastFrame_ = frame.frameId;
    if (!retained && frame.presentationTick)
        lastTick_ = frame.presentationTick;
    const auto& members = frame.membershipView();
    if (members.size() > limits_.members) {
        coverageLost();
        return {};
    }
    std::size_t bytes = sizeof(FrameRecord);
    for (const auto& id : members) {
        if (id.targetId.size() > 256 || id.bindingId.size() > 256) {
            coverageLost();
            return {};
        }
        bytes += sizeof(FeedbackId) + id.targetId.size() + id.bindingId.size() + 64;
    }
    dropAttempt(nextFrame_);
    trim(bytes);
    if (bytes > limits_.bytes || journalBytes_ > limits_.bytes - bytes) {
        coverageLost();
        return {};
    }
    auto& slot = frames_[nextFrame_];
    try {
        slot.emplace(FrameRecord{frame.context,
                                 frame.frameId,
                                 frame.projectionScope,
                                 members,
                                 {},
                                 false,
                                 frame.dispatch,
                                 frame.consumerId,
                                 frame.requestId,
                                 frame.source ? frame.source->resultVersion : 0,
                                 frame.presentationTick,
                                 0,
                                 now_,
                                 bytes});
        journalBytes_ += bytes;
        peakJournalBytes_ = std::max(peakJournalBytes_, journalBytes_);
        latest_ = nextFrame_;
        nextFrame_ = (nextFrame_ + 1) % frames_.size();
    } catch (...) {
        coverageLost();
        slot.reset();
    }
    latestFrame_.emplace(std::move(frame));
    return {};
} catch (...) {
    coverageLost();
    return {};
}
auto FrameObservationLedger::recordAttempt(const FrameEnvelope& frame) -> core::Result<void> try {
    // The compatibility publication wrapper has a single transfer slot, never journal payload
    // history. PD4 already has an owning synchronized Ready slot and only records metadata here.
    FrameEnvelope metadata;
    metadata.context = frame.context;
    metadata.frameId = frame.frameId;
    metadata.projectionScope = frame.projectionScope;
    metadata.presentationTick = frame.presentationTick;
    metadata.membership = frame.membershipView();
    metadata.dispatch = frame.dispatch;
    metadata.consumerId = frame.consumerId;
    metadata.requestId = frame.requestId;
    metadata.source = frame.source;
    auto published = publishImpl(std::move(metadata), frame.retainedSerial != 0);
    latestFrame_.reset();
    latest_.reset();
    return published;
} catch (...) {
    coverageLost();
    return {};
}
auto FrameObservationLedger::takeLatest() -> core::Result<std::optional<FrameEnvelope>> {
    internal::pd::Timer measurement{internal::pd::Stage::ledgerTake};
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    if (!latestFrame_)
        return std::optional<FrameEnvelope>{};
    if (latest_ && frames_[*latest_])
        frames_[*latest_]->submitted = true;
    latest_.reset();
    auto result = std::move(latestFrame_);
    latestFrame_.reset();
    return result;
}
auto FrameObservationLedger::observe(const PresentReceipt& r) -> core::Result<void> try {
    internal::pd::Timer measurement{internal::pd::Stage::receipt};
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    auto record = std::find_if(frames_.begin(), frames_.end(), [&](const auto& f) {
        return f && f->frameId == r.frameId && f->context == r.context &&
               f->projectionScope == r.projectionScope;
    });
    if (record == frames_.end())
        return core::unexpected(error("context_stale").withContext("history", "unavailable"));
    auto& f = **record;
    if (!f.submitted && (!f.dispatch || f.dispatch->state.load() != DispatchState::transferred))
        return core::unexpected(error("message_conflict"));
    if (f.receipt) {
        if (f.receipt->status != PresentStatus::unknown && r.status == PresentStatus::unknown)
            return {};
        if (f.receipt->status == r.status && f.receipt->callMonotonicUs == r.callMonotonicUs &&
            sameHostError(f.receipt->cause, r.cause))
            return {};
        if (f.receipt->status != PresentStatus::unknown || r.status == PresentStatus::unknown)
            return core::unexpected(error("message_conflict"));
    }
    auto recorded = copyPresentReceipt(r);
    if (!recorded) {
        receiptLost(r.context.sessionScope, r.context.sessionScope, r.projectionScope,
                    r.projectionScope, r.frameId, r.frameId);
        return {};
    }
    const auto bytes = receiptBytes(*recorded);
    const auto previous = f.receiptStorageBytes;
    const auto extra = bytes > previous ? bytes - previous : 0;
    if (bytes == SIZE_MAX || extra > limits_.bytes || journalBytes_ > limits_.bytes - extra) {
        receiptLost(r.context.sessionScope, r.context.sessionScope, r.projectionScope,
                    r.projectionScope, r.frameId, r.frameId);
        return {};
    }
    f.receipt = std::move(*recorded);
    f.receivedAt = now_;
    f.bytes = f.bytes - previous + bytes;
    journalBytes_ = journalBytes_ - previous + bytes;
    f.receiptStorageBytes = bytes;
    peakJournalBytes_ = std::max(peakJournalBytes_, journalBytes_);
    for (const auto& id : f.membership)
        for (auto* table : {&feedback_, &tombstones_})
            for (auto& observation : *table)
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
    receiptLost(r.context.sessionScope, r.context.sessionScope, r.projectionScope,
                r.projectionScope, r.frameId, r.frameId);
    return core::unexpected(allocationFailure_);
}
auto FrameObservationLedger::expire(std::int64_t tick) -> core::Result<void> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    for (auto& f : feedback_)
        if (f && f->id.projectionScope == scope_ && f->expiry && f->expiry->value <= tick &&
            f->status == FeedbackStatus::created) {
            bool missing = false;
            for (const auto& record : frames_)
                if (record &&
                    std::find(record->membership.begin(), record->membership.end(), f->id) !=
                        record->membership.end() &&
                    (record->submitted || (record->dispatch && record->dispatch->state.load() ==
                                                                   DispatchState::transferred)) &&
                    (!record->receipt || record->receipt->status == PresentStatus::unknown))
                    missing = true;
            f->status =
                missing ? FeedbackStatus::unknownEvidence : FeedbackStatus::expiredUnconfirmed;
            if (missing && unknownCount_ != UINT64_MAX)
                ++unknownCount_;
        }
    return {};
} catch (...) {
    coverageLost();
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
    latestFrame_.reset();
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
    if (scope <= scope_)
        return core::unexpected(error("context_stale"));
    for (auto& f : feedback_)
        if (f && f->id.projectionScope == scope_ && f->status == FeedbackStatus::created)
            f->status = FeedbackStatus::canceledByControl;
    latest_.reset();
    latestFrame_.reset();
    context_ = context;
    scope_ = scope;
    feedbackLossThrough_.reset();
    feedbackLossCommitThrough_ = 0;
    lastTick_.reset();
    return {};
} catch (...) {
    coverageLost();
    return core::unexpected(allocationFailure_);
}
auto FrameObservationLedger::query(const FeedbackId& id) const
    -> core::Result<FeedbackObservation> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    for (const auto& f : feedback_)
        if (f && f->id == id)
            return *f;
    for (const auto& f : tombstones_)
        if (f && f->id == id)
            return *f;
    return core::unexpected(error("context_stale").withContext("history", "unavailable"));
} catch (...) {
    return core::unexpected(allocationFailure_);
}
} // namespace cuexis::playback::detail
