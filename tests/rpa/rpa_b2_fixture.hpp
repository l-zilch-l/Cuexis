#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::rpa::testing {

struct BatchAdmission final {
    std::uint64_t canonicalOrdinal{};
    bool accepted{};
    std::int64_t dispatchTick{};
    bool forwarded{};
    std::optional<std::int64_t> frontier;
    std::optional<std::int64_t> horizon;

    auto operator==(const BatchAdmission&) const -> bool = default;
};

struct KernelProjection final {
    std::int64_t score{};
    std::uint64_t combo{};
    std::vector<std::string> facts;
    std::vector<std::string> folds;

    auto operator==(const KernelProjection&) const -> bool = default;
};

struct ReplayRecord final {
    std::uint64_t id{};
    std::vector<std::uint64_t> canonicalOrdinals;
    std::vector<BatchAdmission> admissions;
    std::optional<std::int64_t> horizon;
    std::optional<KernelProjection> result;

    auto operator==(const ReplayRecord&) const -> bool = default;
};

struct FaultPrefix final {
    std::string stage;
    std::string code;
    std::string category;
    std::int32_t severity{};
    bool faulted{};
    std::vector<std::uint64_t> recordIds;

    auto operator==(const FaultPrefix&) const -> bool = default;
};

struct ReplayEvaluation final {
    std::string archiveIdentity;
    std::string profileIdentity;
    std::string factSemantic;
    std::string eventCodecId;
    std::uint32_t stateSchemaRevision{};
    std::uint64_t cut{};
    std::int64_t horizon{};
    std::uint64_t eventCount{};
    std::vector<ReplayRecord> records;
    KernelProjection projection;
    std::optional<FaultPrefix> fault;

    auto operator==(const ReplayEvaluation&) const -> bool = default;
};

struct EvaluationPayload final {
    KernelProjection projection;
    std::optional<FaultPrefix> fault;
};

struct ArchiveHandle final {
    std::uint64_t archiveNumber{};
    std::uint64_t sessionGeneration{};

    auto operator==(const ArchiveHandle&) const -> bool = default;
};

struct CheckpointKey final {
    std::uint64_t archiveNumber{};
    std::uint64_t cut{};
    std::int64_t horizon{};
    std::string profileIdentity;

    auto operator==(const CheckpointKey&) const -> bool = default;
};

enum class HandleReadStatus { accepted, closed, stale, missing };

struct ReadResult final {
    HandleReadStatus status{HandleReadStatus::missing};
    std::optional<ReplayEvaluation> evaluation;
};

struct FailureRecord final {
    std::string stage;
    std::uint64_t position{};
    bool retried{};
    bool rolledBack{};

    auto operator==(const FailureRecord&) const -> bool = default;
};

struct LifetimeMetrics final {
    std::uint64_t appendCount{};
    std::uint64_t publishCount{};
    std::uint64_t readCount{};
    std::uint64_t restoreCount{};
    std::uint64_t copiedRecordCount{};
    std::uint64_t peakActiveRecordCount{};
    std::uint64_t releaseCount{};
    std::uint64_t releaseTailOperations{};
};

enum class FailureInjection { none, reserve, allocation };

class OwningReplayResult final {
  public:
    OwningReplayResult() = default;
    auto evaluation() const -> const ReplayEvaluation&;
    auto valid() const -> bool;

  private:
    struct LifetimeToken;
    explicit OwningReplayResult(ReplayEvaluation evaluation,
                                std::shared_ptr<LifetimeToken> lifetime);

    ReplayEvaluation evaluation_;
    std::shared_ptr<LifetimeToken> lifetime_;

    friend class B2Store;
};

class B2Store final {
  public:
    B2Store(std::size_t chunkCapacity, std::string profileIdentity);

    auto append(ReplayRecord record) -> bool;
    auto publish(std::uint64_t cut, std::int64_t horizon, EvaluationPayload payload)
        -> std::optional<OwningReplayResult>;
    auto read(ArchiveHandle handle) const -> ReadResult;
    auto checkpoint(ArchiveHandle handle, std::uint64_t cut, std::int64_t horizon)
        -> std::optional<CheckpointKey>;
    auto restore(const CheckpointKey& key, const OwningReplayResult& archive) const
        -> std::optional<OwningReplayResult>;

    auto handleFor(const OwningReplayResult& archive) const -> ArchiveHandle;
    auto close() -> void;
    auto failNextReserve() -> void;
    auto failNextAllocation() -> void;
    auto isClosed() const -> bool;
    auto activeRecordCount() const -> std::size_t;
    auto metrics() const -> const LifetimeMetrics&;
    auto lastFailure() const -> const std::optional<FailureRecord>&;

  private:
    struct ArchiveEntry final {
        ArchiveHandle handle;
        ReplayEvaluation evaluation;
        std::weak_ptr<OwningReplayResult::LifetimeToken> lifetime;
    };

    auto reserve(std::uint64_t position, std::string_view stage) -> bool;
    auto validHandle(ArchiveHandle handle) const -> bool;

    std::size_t chunkCapacity_;
    std::string profileIdentity_;
    std::uint64_t sessionGeneration_{1};
    std::uint64_t nextArchiveNumber_{1};
    bool closed_{};
    FailureInjection failureInjection_{FailureInjection::none};
    std::vector<std::vector<ReplayRecord>> chunks_;
    std::size_t activeRecordCount_{};
    std::vector<ArchiveEntry> archives_;
    std::shared_ptr<LifetimeMetrics> metrics_;
    std::optional<FailureRecord> lastFailure_;
};

auto compareCompleteReplay(const ReplayEvaluation& expected, const ReplayEvaluation& actual)
    -> bool;
auto compareFaultPrefix(const std::optional<FaultPrefix>& expected,
                        const std::optional<FaultPrefix>& actual) -> bool;

} // namespace cuexis::rpa::testing
