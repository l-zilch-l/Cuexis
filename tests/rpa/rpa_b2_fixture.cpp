#include "rpa_b2_fixture.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace cuexis::rpa::testing {

struct OwningReplayResult::LifetimeToken final {
    explicit LifetimeToken(std::shared_ptr<LifetimeMetrics> metrics, std::uint64_t operations)
        : metrics_(std::move(metrics)), operations_(operations) {}

    ~LifetimeToken() {
        if (metrics_) {
            ++metrics_->releaseCount;
            metrics_->releaseTailOperations += operations_;
        }
    }

    std::shared_ptr<LifetimeMetrics> metrics_;
    std::uint64_t operations_;
};

OwningReplayResult::OwningReplayResult(ReplayEvaluation evaluation,
                                       std::shared_ptr<LifetimeToken> lifetime)
    : evaluation_(std::move(evaluation)), lifetime_(std::move(lifetime)) {}

auto OwningReplayResult::evaluation() const -> const ReplayEvaluation& {
    return evaluation_;
}

auto OwningReplayResult::valid() const -> bool {
    return lifetime_ != nullptr;
}

B2Store::B2Store(std::size_t chunkCapacity, std::string profileIdentity)
    : chunkCapacity_(std::max<std::size_t>(1, chunkCapacity)),
      profileIdentity_(std::move(profileIdentity)), metrics_(std::make_unique<LifetimeMetrics>()) {}

auto B2Store::reserve(std::uint64_t position, std::string_view stage) -> bool {
    if (failureInjection_ == FailureInjection::none) {
        return true;
    }
    const auto injection = failureInjection_;
    failureInjection_ = FailureInjection::none;
    lastFailure_ = FailureRecord{
        .stage = injection == FailureInjection::allocation ? "allocation" : std::string(stage),
        .position = position,
        .retried = false,
        .rolledBack = true};
    return false;
}

auto B2Store::append(ReplayRecord record) -> bool {
    if (closed_ || !reserve(activeRecordCount_, "append")) {
        return false;
    }
    if (chunks_.empty() || chunks_.back().size() == chunkCapacity_) {
        chunks_.emplace_back();
        chunks_.back().reserve(chunkCapacity_);
    }
    chunks_.back().push_back(std::move(record));
    ++activeRecordCount_;
    if (lastFailure_ && lastFailure_->stage == "append" &&
        lastFailure_->position + 1 == activeRecordCount_) {
        lastFailure_->retried = true;
    }
    ++metrics_->appendCount;
    metrics_->peakActiveRecordCount =
        std::max<std::uint64_t>(metrics_->peakActiveRecordCount, activeRecordCount_);
    return true;
}

auto B2Store::publish(std::uint64_t cut, std::int64_t horizon, EvaluationPayload payload)
    -> std::optional<OwningReplayResult> {
    if (closed_ || !reserve(activeRecordCount_, "publish")) {
        return {};
    }

    const auto recordCount = std::min<std::size_t>(cut, activeRecordCount_);
    std::vector<ReplayRecord> records;
    records.reserve(recordCount);
    std::size_t remaining = recordCount;
    for (const auto& chunk : chunks_) {
        const auto take = std::min(remaining, chunk.size());
        records.insert(records.end(), chunk.begin(), chunk.begin() + take);
        remaining -= take;
        if (remaining == 0) {
            break;
        }
    }

    const auto archiveNumber = nextArchiveNumber_++;
    const ArchiveHandle handle{.archiveNumber = archiveNumber,
                               .sessionGeneration = sessionGeneration_};
    ReplayEvaluation evaluation{.archiveIdentity = "archive:" + std::to_string(archiveNumber),
                                .profileIdentity = profileIdentity_,
                                .factSemantic = "fact.semantic.rpa.v1",
                                .eventCodecId = "canonical.discrete.rpa.le.v1",
                                .stateSchemaRevision = 2,
                                .cut = cut,
                                .horizon = horizon,
                                .eventCount = records.size(),
                                .records = std::move(records),
                                .projection = std::move(payload.projection),
                                .fault = std::move(payload.fault)};
    const auto lifetime = std::make_shared<OwningReplayResult::LifetimeToken>(
        metrics_, static_cast<std::uint64_t>(evaluation.records.size() + chunks_.size()));
    archives_.push_back(
        ArchiveEntry{.handle = handle, .evaluation = evaluation, .lifetime = lifetime});
    metrics_->copiedRecordCount += evaluation.records.size();
    ++metrics_->publishCount;
    return OwningReplayResult(std::move(evaluation), lifetime);
}

auto B2Store::validHandle(ArchiveHandle handle) const -> bool {
    return !closed_ && handle.sessionGeneration == sessionGeneration_ &&
           std::any_of(archives_.begin(), archives_.end(),
                       [handle](const ArchiveEntry& entry) { return entry.handle == handle; });
}

auto B2Store::read(ArchiveHandle handle) const -> ReadResult {
    if (closed_) {
        return ReadResult{.status = HandleReadStatus::closed};
    }
    if (handle.sessionGeneration != sessionGeneration_) {
        return ReadResult{.status = HandleReadStatus::stale};
    }
    const auto found = std::find_if(archives_.begin(), archives_.end(),
                                    [handle](const auto& entry) { return entry.handle == handle; });
    if (found == archives_.end()) {
        return ReadResult{.status = HandleReadStatus::missing};
    }
    ++metrics_->readCount;
    return ReadResult{.status = HandleReadStatus::accepted, .evaluation = found->evaluation};
}

auto B2Store::checkpoint(ArchiveHandle handle, std::uint64_t cut, std::int64_t horizon)
    -> std::optional<CheckpointKey> {
    if (!validHandle(handle)) {
        return {};
    }
    return CheckpointKey{.archiveNumber = handle.archiveNumber,
                         .cut = cut,
                         .horizon = horizon,
                         .profileIdentity = profileIdentity_};
}

auto B2Store::restore(const CheckpointKey& key, const OwningReplayResult& archive) const
    -> std::optional<OwningReplayResult> {
    if (!archive.valid() || key.profileIdentity != profileIdentity_ ||
        archive.evaluation().archiveIdentity != "archive:" + std::to_string(key.archiveNumber) ||
        archive.evaluation().cut != key.cut || archive.evaluation().horizon != key.horizon) {
        return {};
    }
    ++metrics_->restoreCount;
    return archive;
}

auto B2Store::handleFor(const OwningReplayResult& archive) const -> ArchiveHandle {
    const auto prefix = std::string_view(archive.evaluation().archiveIdentity).substr(8);
    return ArchiveHandle{.archiveNumber =
                             static_cast<std::uint64_t>(std::stoull(std::string(prefix))),
                         .sessionGeneration = sessionGeneration_};
}

auto B2Store::close() -> void {
    if (!closed_) {
        closed_ = true;
        ++sessionGeneration_;
    }
}

auto B2Store::failNextReserve() -> void {
    failureInjection_ = FailureInjection::reserve;
}

auto B2Store::failNextAllocation() -> void {
    failureInjection_ = FailureInjection::allocation;
}

auto B2Store::isClosed() const -> bool {
    return closed_;
}

auto B2Store::activeRecordCount() const -> std::size_t {
    return activeRecordCount_;
}

auto B2Store::metrics() const -> const LifetimeMetrics& {
    return *metrics_;
}

auto B2Store::lastFailure() const -> const std::optional<FailureRecord>& {
    return lastFailure_;
}

auto compareCompleteReplay(const ReplayEvaluation& expected, const ReplayEvaluation& actual)
    -> bool {
    return expected == actual;
}

auto compareFaultPrefix(const std::optional<FaultPrefix>& expected,
                        const std::optional<FaultPrefix>& actual) -> bool {
    return expected == actual;
}

} // namespace cuexis::rpa::testing
