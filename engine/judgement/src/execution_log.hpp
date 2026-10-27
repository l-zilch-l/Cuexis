#pragma once

#include <cuexis/judgement/recovery.hpp>
#include <cuexis_internal/execution_diagnostics.hpp>
#include <stdexcept>

namespace cuexis::judgement::detail {
// Immutable full chunks share their predecessors. Appending copies at most one active tail.
// Flattening belongs to explicit archive/snapshot operations, never the ordinary advance loop.
struct ExecutionLogChunk final {
    static constexpr std::size_t capacity = 32;
    std::shared_ptr<const ExecutionLogChunk> previous;
    std::vector<ReplayRecord> records;
    // A certified applied prefix borrows only its owning immutable Snapshot, never source suffix.
    std::shared_ptr<const SnapshotDTO> baseState;
    std::size_t baseRecords{};
    ~ExecutionLogChunk() {
        auto cursor = std::move(previous);
        while (cursor && cursor.use_count() == 1) {
            // The last owner may detach a predecessor during destruction only.
            auto next = std::move(const_cast<ExecutionLogChunk*>(cursor.get())->previous);
            cursor.reset();
            cursor = std::move(next);
        }
    }
};
inline auto reserveLogAppend(const std::shared_ptr<const ExecutionLogChunk>& tail)
    -> std::shared_ptr<ExecutionLogChunk> {
    auto candidate = std::make_shared<ExecutionLogChunk>();
    if (tail && !tail->baseState && tail->records.size() < ExecutionLogChunk::capacity) {
        candidate->previous = tail->previous;
        candidate->records = tail->records;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        namespace rpa = cuexis::internal::execution_diagnostics::rpa;
        rpa::seekAdd(rpa::SeekPhase::s4InternalExecution, &rpa::SeekWork::tailRecordCopies,
                     tail->records.size());
        // The tail is bounded by capacity. Record nested element counts, not invented bytes.
        if (rpa::seekSink)
            for (const auto& record : tail->records) {
                rpa::seekAdd(rpa::SeekPhase::s4InternalExecution, &rpa::SeekWork::tailBatchElements,
                             record.batch.size());
                rpa::seekAdd(rpa::SeekPhase::s4InternalExecution,
                             &rpa::SeekWork::tailAdmissionElements, record.admission.size());
                rpa::seekAdd(rpa::SeekPhase::s4InternalExecution,
                             &rpa::SeekWork::tailControlElements,
                             record.control ? record.control->trace.size() : 0);
            }
#endif
    } else {
        candidate->previous = tail;
    }
    candidate->records.reserve(ExecutionLogChunk::capacity);
    return candidate;
}
inline auto flattenLog(const std::shared_ptr<const ExecutionLogChunk>& tail)
    -> std::vector<ReplayRecord> {
    std::vector<const ExecutionLogChunk*> chunks;
    std::size_t size = 0;
    for (auto current = tail.get(); current; current = current->previous.get()) {
        chunks.push_back(current);
        if (current->baseState) {
            if (current->baseRecords > current->baseState->rpaHistory.size())
                throw std::logic_error{"certified log prefix exceeds its owner"};
            size += current->baseRecords;
        }
        size += current->records.size();
    }
    std::vector<ReplayRecord> records;
    records.reserve(size);
    for (auto current = chunks.rbegin(); current != chunks.rend(); ++current) {
        if ((*current)->baseState) {
            const auto& prefix = (*current)->baseState->rpaHistory;
            records.insert(records.end(), prefix.begin(),
                           prefix.begin() + static_cast<std::ptrdiff_t>((*current)->baseRecords));
        }
        records.insert(records.end(), (*current)->records.begin(), (*current)->records.end());
    }
    return records;
}
} // namespace cuexis::judgement::detail
