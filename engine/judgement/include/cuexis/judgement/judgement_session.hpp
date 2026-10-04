#pragma once

//  Judgement typed kernel - S7A-1 session lifecycle skeleton.
//
//  Frozen by this batch (plan S7A-1, rulings S1-03 / S1-04 / S1-05):
//
//    * the module and its installation boundary: an internal STATIC target cuexis_judgement that
//      links cuexis::core only, installs no header and adds no public component and no Playback
//      method;
//    * the nine lifecycle verbs: create / configure / prepare / submit / advance / query /
//      snapshot / seek / reset;
//    * the ownership and exception commitments of the ABI section "ownership and lifetime": a
//      single owner thread with a unique owner of the mutable state, atomic prepare, prepared
//      values read-only after prepare, non-owning prepared views, the Result error channel, no
//      exception across the module boundary, and no throw from a destructor or a real-time path.
//
//  Deliberately not frozen by this batch, and therefore not representable here:
//
//    * every payload role. No lifecycle verb takes an argument. A verb that accepted a guessed
//      payload type, integer width, enumeration value, default value or serialization encoding
//      would freeze exactly what ruling S1-05 moves out of this batch, so those by-value fields
//      and operations are out of scope and the verb only reports its stable rejection;
//    * every semantic result. No verb has a success path: configure refuses because no
//      configuration role is representable, and prepare, submit, advance, query, snapshot, seek
//      and reset refuse because no prepared session can exist. create is the only verb that can
//      succeed, and it returns an empty session that owns no judgement state;
//    * the prepared graph, prepared grace, arbitration policy and interface projection: they have
//      no setter, no non-const accessor and no verb parameter, so no runtime mutation of them is
//      expressible at all, not merely forbidden by a comment;
//    * the projection and snapshot field tables, any serialization layout, and the diagnostic
//      code table.
//
//  Runtime immutability is structural: the reading verbs are const-qualified, the session is
//  move-only so no second alias can mutate it, and there is no code path that could modify
//  prepared state because there is no code path that could create it.
//
//  A moved-from session is only valid for destruction or for move assignment. Using it is a
//  programming error that debug builds report through an assertion.

#include <cuexis/core/result.hpp>

#include <memory>

namespace cuexis::judgement {

namespace detail {

//  Storage anchors of the self-owning output carriers. Declared and never defined: the field
//  table of judged output and of a snapshot is unfrozen (unresolved item #3), so this boundary
//  promises no member, no width and no byte encoding. Only shared ownership of the opaque anchor
//  crosses the boundary.
class ProjectionStorage;
class SnapshotStorage;

} // namespace detail

//  Self-owning, read-only projection of judged output. The frozen ownership rule for a judgement
//  result is "value type projection, returned by value, never referencing session storage"; that
//  rule is expressed by the shared, self-owned anchor below. No accessor and no field is exposed,
//  because an accessor would freeze one entry of an unfrozen field table.
class JudgementProjection final {
  public:
    JudgementProjection(const JudgementProjection&) noexcept = default;
    JudgementProjection(JudgementProjection&&) noexcept = default;
    auto operator=(const JudgementProjection&) noexcept -> JudgementProjection& = default;
    auto operator=(JudgementProjection&&) noexcept -> JudgementProjection& = default;
    ~JudgementProjection() noexcept = default;

  private:
    friend class JudgementSession;

    //  The session is the only producer of a projection. S7A-1 never reaches a prepared session,
    //  so this seam is never called; it exists so that the ownership contract stays expressible
    //  without inventing a field.
    explicit JudgementProjection(std::shared_ptr<const detail::ProjectionStorage> storage) noexcept;

    std::shared_ptr<const detail::ProjectionStorage> storage_;
};

//  Self-owning snapshot payload. The frozen ownership rule is "self-owned, independently
//  verifiable, restoration does not depend on the original session"; the shared anchor expresses
//  it. As above, no field and no accessor is exposed in this batch.
class SnapshotPayload final {
  public:
    SnapshotPayload(const SnapshotPayload&) noexcept = default;
    SnapshotPayload(SnapshotPayload&&) noexcept = default;
    auto operator=(const SnapshotPayload&) noexcept -> SnapshotPayload& = default;
    auto operator=(SnapshotPayload&&) noexcept -> SnapshotPayload& = default;
    ~SnapshotPayload() noexcept = default;

  private:
    friend class JudgementSession;

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

    //  prepare - freezes chart, ruleset, loadout, input mapping, timing and judgement config. It
    //  is atomic: a refused prepare publishes nothing and leaves no half-prepared session.
    //  S7A-1 has no prepared-graph, prepared-grace or arbitration-policy representation, so
    //  prepare cannot succeed. An unconfigured session refuses because the lifecycle order
    //  requires a configuration first; a configured session would still refuse rather than
    //  publish a guessed representation.
    [[nodiscard]] auto prepare() -> core::Result<void>;

    //  submit / advance - the single live and replay path of the ABI. Both require a prepared
    //  session plus a representable observation, event sequence and timebase; none exists in
    //  this batch, so both refuse.
    [[nodiscard]] auto submit() -> core::Result<void>;
    [[nodiscard]] auto advance() -> core::Result<void>;

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
