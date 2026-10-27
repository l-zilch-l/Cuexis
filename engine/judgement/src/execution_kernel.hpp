#pragma once

#include "ingress_transaction.hpp"
#include <array>
#include <cuexis/judgement/recovery.hpp>

namespace cuexis::judgement::detail {
struct KernelTestControls;
auto makeCanonicalReplayIdentityCache() -> std::shared_ptr<CanonicalReplayIdentityCache>;
class ProjectionStorage {
  public:
    explicit ProjectionStorage(KernelProjection value) : projection(std::move(value)) {}
    KernelProjection projection;
};
struct CheckpointCertificate final {
    std::shared_ptr<const ReplayData> archive;
    ReplayCut cut;
    Tick horizon;
    std::shared_ptr<const SnapshotDTO> state;
    // Optional normal-RPA closure, minted only from a complete internal factory replay.
    std::uint32_t rpaStateVersion{};
    // Exact generation evidence attached to the private immutable owner. Hot Seek binds that
    // owner; it does not claim to re-encode or re-hash the archive on every request.
    std::array<std::uint8_t, 32> canonicalIdentity{};
    std::uint64_t prefixEventCount{};
};
class SnapshotStorage final {
  public:
    SnapshotStorage(SnapshotDTO value, std::shared_ptr<const RecoveryInputs> dependencies)
        : dto(std::move(value)), inputs(std::move(dependencies)) {}
    SnapshotDTO dto;
    std::shared_ptr<const RecoveryInputs> inputs;
};
auto validateSessionConfiguration(const SessionConfiguration&) -> core::Result<void>;

class ExecutionKernel final {
  public:
    static auto prepare(const SessionConfiguration& configuration, const PreparedGameplay& prepared)
        -> core::Result<std::unique_ptr<ExecutionKernel>>;
    static auto initialProjection(const ReplayArchive&)
        -> core::Result<std::shared_ptr<const ProjectionStorage>>;
    auto submit(std::vector<ClockedIngress> batch)
        -> core::Result<std::vector<InputReceiptPending>>;
    auto submitCanonical(std::vector<CanonicalInput> batch)
        -> core::Result<std::vector<InputReceiptPending>>;
    auto controlRpa(const RpaControlRecord&) -> core::Result<void>;
    auto snapshot() const -> core::Result<std::shared_ptr<const SnapshotStorage>>;
    static auto restore(const SnapshotStorage&, const RecoveryInputs&)
        -> core::Result<std::unique_ptr<ExecutionKernel>>;
    auto canArchive() const noexcept -> bool;
    auto archive() const -> ReplayArchive;
    static auto evaluate(const ReplayArchive&, const KernelTestControls* = nullptr)
        -> core::Result<ReplayEvaluation>;
    static auto seekCandidate(const ReplayArchive&, Tick horizon,
                              const KernelTestControls* = nullptr, const ReplayCut* = nullptr,
                              std::span<const ReplayCheckpoint> = {})
        -> core::Result<std::unique_ptr<ExecutionKernel>>;
    auto advance(Tick horizon, bool controlDrain = false) -> core::Result<void>;
    [[nodiscard]] auto query() const noexcept -> std::shared_ptr<const ProjectionStorage>;
    auto inject(KernelTestControls) -> core::Result<void>;
    auto visibleSignalCount(Tick) const noexcept -> std::size_t;
    auto certifiedPrefixRecords() const noexcept -> std::uint64_t;
    auto nativePrefixRecords() const noexcept -> std::uint64_t;
    ~ExecutionKernel();

  private:
    friend class KernelTestAccess;
    friend class ::cuexis::judgement::JudgementSession;
    auto checkpointCertificate(const ReplayArchive&, ReplayCut, Tick,
                               std::shared_ptr<const SnapshotDTO>) const
        -> std::shared_ptr<const CheckpointCertificate>;
    auto captureState() const -> core::Result<std::shared_ptr<const SnapshotStorage>>;
    auto submitBatch(std::span<const ClockedIngress>,
                     std::span<const std::optional<std::int64_t>> = {})
        -> core::Result<std::vector<InputReceiptPending>>;
    auto advanceRecorded(Tick horizon) -> core::Result<void>;
    auto submitRpa(std::vector<CanonicalInput>, bool& transientAllocationFailure)
        -> core::Result<std::vector<InputReceiptPending>>;
    auto restoreCertifiedRpa(const CheckpointCertificate&, const KernelTestControls*) noexcept
        -> bool;
    struct Impl;
    explicit ExecutionKernel(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace cuexis::judgement::detail
