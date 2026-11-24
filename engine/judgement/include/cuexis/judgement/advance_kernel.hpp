#pragma once

//  Judgement runtime - the tick-record advance kernel.
//
//  The kernel consumes `runtime::PreparedGameplay` only. It owns the same-run ingress cursor, the
//  sequence allocator, the receive-closed flag, the owner-thread guard and the per-call fault
//  state. Settlement is staged first and folded second: a successful seal publishes the slots and
//  the frontier, and a Fold failure keeps that sealed prefix while reporting `faulted` with no
//  score payload. No author header, graph or legacy timebase type is reachable from here.

#include <cuexis/core/result.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/judgement/advance.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>

#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement::runtime {

namespace detail {
class AdvanceTestAccess;
}

//  One same-run ingress sequence. 0 is the first sequence; the allocator never wraps.
class IngressSequence final {
  public:
    constexpr IngressSequence() noexcept = default;
    explicit constexpr IngressSequence(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint64_t {
        return value_;
    }
    friend constexpr auto operator==(IngressSequence, IngressSequence) noexcept -> bool = default;
    friend constexpr auto operator<=>(IngressSequence, IngressSequence) noexcept
        -> std::strong_ordering = default;

  private:
    std::uint64_t value_{0};
};

//  One already-canonical tick record. The kernel does not normalize or re-time it; the caller
//  supplies the canonical observation fields and the original tick.
struct TickRecord final {
    Tick observationTick;
    std::string domainToken;
    std::string sourceClass;
    std::string channelToken;
    InputAction action{};
    std::optional<std::int64_t> amount;
};

//  One archived ingress row: the canonical record, its true sequence and dispatch decision.
struct ReplayRecord final {
    IngressSequence sequence;
    Tick dispatchTick;
    bool accepted;
    std::string rejectionCode;
    TickRecord record;
};

//  The in-memory replay of one open or stopped run. `events` are the settled events in emission
//  order; `records` retain every numbered row, including rejected rows.
struct ReplaySnapshot final {
    std::vector<ReplayRecord> records;
    std::vector<SettlementEvent> events;
    std::optional<Tick> frontier;
    std::int64_t j{0};
    std::int64_t c{0};
    std::uint32_t score{0};
};

//  The accepted or rejected outcome of one record. `dispatchTick` is the original tick when the
//  record is on time and the queued tick when the late policy forwarded it; a rejected record keeps
//  its real sequence and the dispatch it would have had.
struct IngressReceipt final {
    IngressSequence sequence;
    Tick dispatchTick;
    bool accepted{false};
    bool forwarded{false};
    std::string rejectionCode;
};

//  The same-run Stop boundary. `acceptedThrough` is the inclusive upper bound read from this
//  kernel, never a delivery counter.
struct StopCut final {
    RunKey run{};
    std::optional<IngressSequence> acceptedThrough;
    Tick cutoff{};
};

//  The runtime-side session configuration. The timebase is the verified `PreparedTimebase` of the
//  run, not the legacy ownable profile; identity declarations stay by value. The legacy-only
//  carriers are represented as a refusal: the runtime shape records that a legacy ruleset was
//  requested so `prepare` can reject it instead of silently downgrading.
struct SessionConfiguration final {
    PreparedTimebase timebase;
    LatePolicyParameters latePolicy;
    PreparedIdentityDeclarations identityDeclarations;
    std::string calibrationIdentityToken;
    std::string executionProfileToken;
    std::string lateAlgorithmToken;
    std::string factSemanticRevision;
    //  Any value means "this call carries a legacy ruleset"; the v3 runtime boundary refuses it.
    std::optional<std::string> legacyRulesetToken;
};

//  The observable lifecycle of one kernel. A default-constructed kernel is `unprepared`; a faulted
//  kernel freezes and refuses every later mutating call.
enum class AdvanceSessionState : std::uint8_t { unprepared, prepared, faulted };

//  The result of one advance or Stop transaction. `normalized` is present only on the first normal
//  completion of the run; a faulted call keeps the sealed prefix (`settledEvents`, slots) and
//  leaves it empty.
struct AdvanceSummary final {
    std::optional<Tick> processedThrough;
    std::size_t processedRecords{0};
    std::size_t settledEvents{0};
    //  The number of settlement slots this call filled; equal to `settledEvents` when the call got
    //  a slot span, zero for the counting-only overload. A faulted call reports the sealed prefix.
    std::size_t written{0};
    std::int64_t j{0};
    std::int64_t c{0};
    std::uint32_t score{0};
    std::uint64_t jMax{0};
    std::uint64_t cMax{0};
    std::uint32_t mj{0};
    std::uint32_t mc{0};
    std::uint32_t combo{0};
    std::uint32_t maxCombo{0};
    AdvanceStatus status{AdvanceStatus::ongoing};
    FaultCode fault{AdvanceFaultStage::none, AdvanceFaultReason::none};
    std::optional<NormalizedScoreResult> normalized;
};

class AdvanceKernel final {
  public:
    //  An unprepared kernel carries no prepared handle and refuses every mutating call with
    //  `session.mutation_rejected` at `/session/state`.
    AdvanceKernel() noexcept = default;
    AdvanceKernel(const AdvanceKernel&) = delete;
    AdvanceKernel(AdvanceKernel&& other) noexcept;
    auto operator=(AdvanceKernel&& other) noexcept -> AdvanceKernel&;
    auto operator=(const AdvanceKernel&) -> AdvanceKernel& = delete;
    ~AdvanceKernel() noexcept = default;

    //  Publishes one owning prepared handle. Both run-key components must be non-zero; the first
    //  call is the thread that owns the kernel.
    [[nodiscard]] static auto prepare(PreparedGameplay prepared, RunKey run)
        -> core::Result<AdvanceKernel>;

    //  The runtime session form: validates the timebase, late policy, identity declarations and the
    //  absence of legacy overrides against the prepared run before publishing the handle.
    [[nodiscard]] static auto prepare(SessionConfiguration configuration, PreparedGameplay prepared,
                                      RunKey run) -> core::Result<AdvanceKernel>;

    //  Assigns contiguous sequences to the whole batch before any late/admission decision, then
    //  records accept/reject decisions and commits the cursor with the records. A canonical or
    //  capacity failure happens before numbering and leaves the cursor untouched.
    [[nodiscard]] auto receiveCanonicalBatch(std::span<const TickRecord> records)
        -> core::Result<std::vector<IngressReceipt>>;

    //  The inclusive upper bound of the received prefix; `nullopt` is a legal empty prefix.
    [[nodiscard]] auto acceptedIngressThrough() const noexcept -> std::optional<IngressSequence>;

    //  Advances the frontier and processes accepted records whose dispatch is at or before the
    //  horizon. It never moves backwards and never allocates a sequence. The overload without
    //  slots settles for counting only; every overload with slots requires at least P slots before
    //  the transaction starts. The run overload applies the contract pre-check order.
    [[nodiscard]] auto advance(Tick horizon) -> core::Result<AdvanceSummary>;
    [[nodiscard]] auto advance(Tick horizon, std::span<SettlementEvent> slots)
        -> core::Result<AdvanceSummary>;
    [[nodiscard]] auto advance(Tick horizon, RunKey run, std::span<SettlementEvent> slots)
        -> core::Result<AdvanceSummary>;

    //  Validates the caller's run, bound and cutoff, closes receive, seals the events up to the
    //  cutoff and folds them. The upper bound must equal the current cursor exactly.
    [[nodiscard]] auto advanceStopCut(const StopCut& cut, std::span<SettlementEvent> slots)
        -> core::Result<AdvanceSummary>;

    [[nodiscard]] auto run() const noexcept -> RunKey {
        return run_;
    }
    [[nodiscard]] auto state() const noexcept -> AdvanceSessionState {
        return state_;
    }
    [[nodiscard]] auto fault() const noexcept -> FaultCode {
        return fault_;
    }
    [[nodiscard]] auto receiveClosed() const noexcept -> bool {
        return receiveClosed_;
    }
    [[nodiscard]] auto frontier() const noexcept -> std::optional<Tick> {
        return frontier_;
    }
    [[nodiscard]] auto nextSequence() const noexcept -> IngressSequence {
        return nextSequence_;
    }
    [[nodiscard]] auto archivedRecords() const noexcept -> std::span<const IngressReceipt> {
        return archive_;
    }
    //  P: the exact number of phase instances in this run, which is the settlement slot count.
    [[nodiscard]] auto settlementCapacity() const noexcept -> std::size_t {
        return settlementCapacity_;
    }
    //  The owning dictionary of this prepared run. The host copies it before the run starts.
    [[nodiscard]] auto dictionary() const noexcept -> const SettlementDictionary&;
    //  Read-only, owning replay: no renumbering, no re-emission, no allocation of sequence.
    [[nodiscard]] auto replay() const -> ReplaySnapshot;

  private:
    friend class detail::AdvanceTestAccess;

    AdvanceKernel(PreparedGameplay prepared, RunKey run, LatePolicyParameters late);

    struct RequirementRun final {
        enum class Stage : std::uint8_t { dormant, body, tail, done };
        Stage stage{Stage::dormant};
        Tick pressObservation{};
        bool tapDone{false};
    };

    struct UnitRun final {
        std::vector<std::uint32_t> phases;
        std::size_t remaining{0};
        bool settled{false};
    };

    auto advanceInternal(Tick horizon, const RunKey* run, std::span<SettlementEvent> slots,
                         bool enforceSlots) -> core::Result<AdvanceSummary>;
    auto stopCutInternal(const StopCut& cut, std::span<SettlementEvent> slots)
        -> core::Result<AdvanceSummary>;
    auto runTransaction(Tick horizon, const RunKey* run, std::span<SettlementEvent> slots,
                        bool enforceSlots, bool closeReceive) -> core::Result<AdvanceSummary>;
    auto summaryOf(std::optional<Tick> processedThrough, std::size_t processed,
                   std::size_t written) const -> AdvanceSummary;
    auto rejectWrongThread() const -> core::Error;
    auto rejectState(std::string_view reason, std::string_view message) const -> core::Error;
    auto rejectRun(std::string_view reason, std::string_view message) const -> core::Error;
    void markFaulted(AdvanceFaultStage stage, AdvanceFaultReason reason);
    auto route(Tick observation, bool& forwarded) const -> std::optional<Tick>;
    auto requirementIndexFor(const TickRecord& record) const -> std::optional<std::size_t>;
    auto phaseOffset(std::size_t requirement) const -> std::size_t;
    auto inWindow(const PreparedRequirement& requirement, PhaseKind kind, Tick tick) const -> bool;
    auto targetOf(const PreparedRequirement& requirement, PhaseKind kind) const -> Tick;
    auto lastTailEnd(const PreparedRequirement& requirement) const -> Tick;
    auto gradeIndex(std::size_t requirement, PhaseKind phase, std::int64_t error) const
        -> std::optional<std::uint32_t>;
    auto directionOf(std::size_t requirement, PhaseKind phase, std::int64_t error) const
        -> std::optional<TimingDirection>;
    auto emit(std::size_t requirement, PhaseKind phase, Tick tick, Outcome outcome,
              std::optional<std::int64_t> error) -> SettlementEvent;
    void settleRecord(std::size_t requirement, const TickRecord& record, Tick dispatch);
    void settleTimers(Tick tick, Tick sealLimit);
    void applyFold(const SettlementEvent& event);
    auto computeScore() const -> std::uint32_t;
    auto processThrough(Tick horizon) -> std::size_t;

    std::vector<RequirementRun> runs_;
    std::vector<UnitRun> units_;
    std::vector<std::size_t> phaseOffsets_;
    std::vector<SettlementEvent> staged_;
    std::int64_t j_{0};
    std::int64_t c_{0};
    std::uint32_t score_{0};
    std::uint32_t combo_{0};
    std::uint32_t maxCombo_{0};

    std::optional<PreparedGameplay> prepared_;
    std::optional<SessionConfiguration> configuration_;
    RunKey run_{};
    LatePolicyParameters late_;
    core::ThreadChecker owner_;
    AdvanceSessionState state_{AdvanceSessionState::unprepared};
    FaultCode fault_{AdvanceFaultStage::none, AdvanceFaultReason::none};
    bool inTransaction_{false};
    bool completed_{false};
    std::size_t settlementCapacity_{0};
    Tick terminalHorizon_{};
    std::optional<Tick> frontier_;
    std::optional<IngressSequence> acceptedThrough_;
    IngressSequence nextSequence_{};
    bool receiveClosed_{false};
    std::vector<IngressReceipt> archive_;
    std::vector<TickRecord> records_;
    std::vector<SettlementEvent> settled_;
    std::vector<std::size_t> pending_;

    //  Test-only seam; empty in production. `advance_testing.hpp` maps its controls onto these.
    std::optional<std::uint8_t> testFaultPoint_;
    std::size_t testSealOrdinal_{1};
    std::function<void()> testAtSeal_;
};

} // namespace cuexis::judgement::runtime
