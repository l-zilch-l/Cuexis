#pragma once

#include <cuexis/judgement/recovery.hpp>

namespace cuexis::judgement::detail {
// Immutable full chunks share their predecessors. Appending copies at most one active tail.
// Flattening belongs to explicit archive/snapshot operations, never the ordinary advance loop.
struct ExecutionLogChunk final {
    static constexpr std::size_t capacity = 32;
    std::shared_ptr<const ExecutionLogChunk> previous;
    std::vector<ReplayRecord> records;
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
    if (tail && tail->records.size() < ExecutionLogChunk::capacity) {
        candidate->previous = tail->previous;
        candidate->records = tail->records;
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
        size += current->records.size();
    }
    std::vector<ReplayRecord> records;
    records.reserve(size);
    for (auto current = chunks.rbegin(); current != chunks.rend(); ++current) {
        records.insert(records.end(), (*current)->records.begin(), (*current)->records.end());
    }
    return records;
}
} // namespace cuexis::judgement::detail
