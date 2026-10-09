#pragma once

#include <cuexis/judgement/kernel_types.hpp>

namespace cuexis::judgement {
struct CanonicalInput final {
    CanonicalObservationKey key;
    IngressSequence sequence;
    friend auto operator==(const CanonicalInput&, const CanonicalInput&) -> bool = default;
};
struct MatcherSnapshot final {
    std::vector<std::uint64_t> states;
    bool epsilonAttempted;
    friend auto operator==(const MatcherSnapshot&, const MatcherSnapshot&) -> bool = default;
};
struct IngressSnapshot final {
    std::vector<CanonicalInput> accepted;
    std::optional<ObservationTick> lastObservedTick;
    std::uint64_t nextId;
    bool exhausted;
    friend auto operator==(const IngressSnapshot&, const IngressSnapshot&) -> bool = default;
};
struct SnapshotDTO final {
    KernelProjection kernel;
    std::vector<MatcherSnapshot> matchers;
    std::vector<HeadContactOwnership> coverageHistory;
    std::vector<std::uint64_t> activeRequirements;
    std::uint64_t timerCursor, nextCommitId;
    bool commitIdsExhausted;
    IngressSnapshot ingress;
    std::vector<AdmittedObservation> pending;
};
struct RecoveryInputs final {
    SessionConfiguration configuration;
    PreparedGameplay prepared;
};
struct ReplayRecord final {
    std::vector<CanonicalInput> batch;
    std::vector<InputReceiptPending> admission;
    std::optional<Tick> horizon;
    std::shared_ptr<const KernelProjection> result;
};
struct ReplayData final {
    std::vector<ReplayRecord> records;
    std::uint64_t eventCount;
    PreparedIdentity identity;
};
class ReplayArchive final {
  public:
    [[nodiscard]] auto data() const noexcept -> const ReplayData& {
        return *data_;
    }
    [[nodiscard]] auto anchor() const noexcept -> std::shared_ptr<const ReplayData> {
        return data_;
    }
    [[nodiscard]] auto dependencies() const noexcept -> const RecoveryInputs& {
        return *inputs_;
    }
    ReplayArchive(std::shared_ptr<const ReplayData> data,
                  std::shared_ptr<const RecoveryInputs> inputs)
        : data_(std::move(data)), inputs_(std::move(inputs)) {}

  private:
    std::shared_ptr<const ReplayData> data_;
    std::shared_ptr<const RecoveryInputs> inputs_;
};
struct ReplayCut final {
    std::uint64_t completeRecords;
    std::optional<Tick> partialHorizon;
    friend auto operator==(const ReplayCut&, const ReplayCut&) -> bool = default;
};
namespace detail {
struct CheckpointCertificate;
}
class ReplayCheckpoint final {
  public:
    ReplayCheckpoint(ReplayArchive a, ReplayCut c, Tick h, std::shared_ptr<const SnapshotDTO> s)
        : archive(std::move(a)), cut(c), horizon(h), state(std::move(s)) {}

    ReplayArchive archive;
    ReplayCut cut;
    Tick horizon;
    std::shared_ptr<const SnapshotDTO> state;

  private:
    friend class JudgementSession;
    friend class detail::ExecutionKernel;
    std::shared_ptr<const detail::CheckpointCertificate> certificate_;
};
[[nodiscard]] auto replayCutAt(const ReplayArchive&, Tick horizon) -> ReplayCut;
[[nodiscard]] auto sameReplayHistory(const ReplayArchive&, const ReplayArchive&) -> bool;
struct ReplayEvaluation final {
    bool evidenceValid;
    std::shared_ptr<const KernelProjection> result;
};
struct CodecBudget final {
    std::uint64_t maxBytes, maxRecords, maxElements;
    // Numerical budgets are accepted only in explicitly test-only decoding fixtures.
    bool testOnly;
};
[[nodiscard]] auto sameKernelResult(const KernelProjection&, const KernelProjection&) -> bool;
[[nodiscard]] auto sameSnapshot(const SnapshotDTO&, const SnapshotDTO&) -> bool;
} // namespace cuexis::judgement
