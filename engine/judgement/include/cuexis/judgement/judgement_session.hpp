#pragma once

// Internal owning T4/K4 session. Typed overloads execute the selected S7A-4 profile;
// legacy argument-free verbs and snapshot/seek retain their stable rejection paths.
// Prepared state and projections own their storage. Query returns immutable shared
// snapshots; reset cannot invalidate earlier projections. No header is installed.
// A session has one owner thread and is move-only. A moved-from value is usable
// only for destruction or move assignment.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/recovery.hpp>

#include <memory>

namespace cuexis::judgement {

namespace detail {

class ProjectionStorage;
class SnapshotStorage;
class KernelTestAccess;

} // namespace detail

class JudgementProjection final {
  public:
    [[nodiscard]] auto kernelView() const noexcept -> const KernelProjection&;
    JudgementProjection(const JudgementProjection&) noexcept = default;
    JudgementProjection(JudgementProjection&&) noexcept = default;
    auto operator=(const JudgementProjection&) noexcept -> JudgementProjection& = default;
    auto operator=(JudgementProjection&&) noexcept -> JudgementProjection& = default;
    ~JudgementProjection() noexcept = default;

  private:
    friend class JudgementSession;

    explicit JudgementProjection(std::shared_ptr<const detail::ProjectionStorage> storage) noexcept;

    std::shared_ptr<const detail::ProjectionStorage> storage_;
};

//  Self-owning snapshot payload. The frozen ownership rule is "self-owned, independently
//  verifiable, restoration does not depend on the original session"; the shared anchor expresses
//  it. As above, no field and no accessor is exposed in this batch.
class SnapshotPayload final {
  public:
    [[nodiscard]] auto state() const noexcept -> const SnapshotDTO&;
    SnapshotPayload(const SnapshotPayload&) noexcept = default;
    SnapshotPayload(SnapshotPayload&&) noexcept = default;
    auto operator=(const SnapshotPayload&) noexcept -> SnapshotPayload& = default;
    auto operator=(SnapshotPayload&&) noexcept -> SnapshotPayload& = default;
    ~SnapshotPayload() noexcept = default;

  private:
    friend class JudgementSession;
    friend auto encodeSnapshot(const SnapshotPayload&) -> core::Result<std::vector<std::byte>>;
    friend auto decodeSnapshot(std::span<const std::byte>, const RecoveryInputs&,
                               std::optional<CodecBudget>) -> core::Result<SnapshotPayload>;

    explicit SnapshotPayload(std::shared_ptr<const detail::SnapshotStorage> storage) noexcept;

    std::shared_ptr<const detail::SnapshotStorage> storage_;
};

//  Judgement session lifecycle skeleton. Every verb below is an entry point of the typed boundary;
//  see the file header for what is frozen and what is deliberately absent.
class JudgementSession final {
  public:
    //  create - the only verb with a success path in this batch. The returned session owns no
    //  judgement state, opens no device and needs no GPU, window, audio or render backend, so it
    //  is creatable and destructible headlessly.
    [[nodiscard]] static auto create() -> core::Result<JudgementSession>;

    ~JudgementSession() noexcept;
    JudgementSession(JudgementSession&& other) noexcept;
    auto operator=(JudgementSession&& other) noexcept -> JudgementSession&;

    //  A copy would be a second owner of the same session state, which the owner-thread rule
    //  forbids.
    JudgementSession(const JudgementSession&) = delete;
    auto operator=(const JudgementSession&) -> JudgementSession& = delete;

    //  configure - enters the configured phase. S7A-1 refuses: every configuration role (chart,
    //  ruleset, loadout, input mapping, timebase, judgement config) is unrepresentable here, and
    //  accepting nothing while reporting success would be a pseudo-success. A refused configure
    //  publishes nothing, so no half-configured session can exist.
    [[nodiscard]] auto configure() -> core::Result<void>;
    [[nodiscard]] auto configure(const SessionConfiguration& configuration) -> core::Result<void>;

    //  prepare - freezes chart, ruleset, loadout, input mapping, timing and judgement config. It
    //  is atomic: a refused prepare publishes nothing and leaves no half-prepared session.
    //  S7A-1 has no prepared-graph, prepared-grace or arbitration-policy representation, so
    //  prepare cannot succeed. An unconfigured session refuses because the lifecycle order
    //  requires a configuration first; a configured session would still refuse rather than
    //  publish a guessed representation.
    [[nodiscard]] auto prepare() -> core::Result<void>;
    [[nodiscard]] auto prepare(const PreparedGameplay& prepared) -> core::Result<void>;

    //  submit / advance - the single live and replay path of the ABI. Both require a prepared
    //  session plus a representable observation, event sequence and timebase; none exists in
    //  this batch, so both refuse.
    [[nodiscard]] auto submit() -> core::Result<void>;
    [[nodiscard]] auto advance() -> core::Result<void>;
    [[nodiscard]] auto submit(std::vector<ClockedIngress> batch)
        -> core::Result<std::vector<InputReceiptPending>>;
    [[nodiscard]] auto submitCanonical(std::vector<CanonicalInput> batch)
        -> core::Result<std::vector<InputReceiptPending>>;
    [[nodiscard]] auto archive() const -> core::Result<ReplayArchive>;
    [[nodiscard]] static auto evaluateReplay(const ReplayArchive&)
        -> core::Result<ReplayEvaluation>;
    [[nodiscard]] static auto recover(const SnapshotPayload&, const RecoveryInputs&)
        -> core::Result<JudgementSession>;
    [[nodiscard]] auto restore(const SnapshotPayload&, const RecoveryInputs&) -> core::Result<void>;
    [[nodiscard]] static auto checkpoint(const ReplayArchive&, Tick horizon)
        -> core::Result<ReplayCheckpoint>;
    [[nodiscard]] auto seek(const ReplayArchive&, Tick horizon) -> core::Result<void>;
    [[nodiscard]] auto seek(const ReplayArchive&, Tick horizon, const ReplayCut&,
                            std::span<const ReplayCheckpoint> = {}) -> core::Result<void>;
    [[nodiscard]] auto advance(Tick horizon) -> core::Result<JudgementProjection>;

    //  query / snapshot - reads. Const-qualified, so no read path can modify the session. Both
    //  require a prepared session; the returned carriers are self-owning value types that never
    //  reference session storage.
    [[nodiscard]] auto query() const -> core::Result<JudgementProjection>;
    [[nodiscard]] auto snapshot() const -> core::Result<SnapshotPayload>;

    //  seek / reset - explicit transaction replacement, never an implicit rewind. Both require a
    //  prepared session, so both refuse here. On success a reset would return every mutable
    //  member to its created-phase value; a refused reset touches no state at all.
    [[nodiscard]] auto seek() -> core::Result<void>;
    [[nodiscard]] auto reset() -> core::Result<void>;

    //  Lifecycle read-back. Owner thread only, does not modify any state, and is never part of a
    //  projection or a snapshot. S7A-1 has no path to a prepared session, so this is false in
    //  every reachable state: it is the observable form of "a refused prepare leaves no
    //  half-prepared session".
    [[nodiscard]] auto hasPreparedState() const noexcept -> bool;

  private:
    struct Impl;
    friend class detail::KernelTestAccess;

    explicit JudgementSession(std::unique_ptr<Impl> impl) noexcept;

    //  Entry invariant of every lifecycle verb: the session still owns its state and the caller
    //  runs on the owner thread. Debug-only; a moved-from session must only be destroyed or
    //  reassigned.
    void assertUsable() const noexcept;

    //  Mutable state of a session. Owner thread: the thread that called create(); unique owner, so
    //  it is never shared with another thread. Read time point: the entry of every lifecycle verb,
    //  as the ownership invariant above. Snapshot ownership: never part of a projection or a
    //  snapshot, which are self-owning. Reset behavior: not affected by a reset; ownership is
    //  never transferred by any verb. All other mutable members live in Impl, in the .cpp file.
    std::unique_ptr<Impl> impl_;
};

} // namespace cuexis::judgement
