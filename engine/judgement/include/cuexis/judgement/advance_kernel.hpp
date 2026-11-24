#pragma once

//  Judgement runtime - the minimal tick-record advance kernel.
//
//  This kernel consumes `runtime::PreparedGameplay` only. It owns the same-run ingress cursor, the
//  sequence allocator and the receive-closed flag; the phase/Fold settlement is deliberately not
//  part of this first minimal kernel (the report records that gap). No author header, graph or
//  legacy timebase type is reachable from here.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/advance.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement::runtime {

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

//  The result of one advance or Stop transaction.
struct AdvanceSummary final {
    std::optional<Tick> processedThrough;
    std::size_t processedRecords{0};
    std::size_t settledEvents{0};
};

class AdvanceKernel final {
  public:
    //  Publishes one owning prepared handle. The handle must carry a complete late policy; no
    //  phase or score table is required by this minimal kernel.
    [[nodiscard]] static auto prepare(PreparedGameplay prepared, RunKey run = {})
        -> core::Result<AdvanceKernel>;

    //  Assigns contiguous sequences to the whole batch before any late/admission decision, then
    //  records accept/reject decisions and commits the cursor with the records. A canonical or
    //  capacity failure happens before numbering and leaves the cursor untouched.
    [[nodiscard]] auto receiveCanonicalBatch(std::span<const TickRecord> records)
        -> core::Result<std::vector<IngressReceipt>>;

    //  The inclusive upper bound of the received prefix; `nullopt` is a legal empty prefix.
    [[nodiscard]] auto acceptedIngressThrough() const noexcept -> std::optional<IngressSequence>;

    //  Advances the frontier and processes accepted records whose dispatch is at or before the
    //  horizon. It never moves backwards and never allocates a sequence.
    [[nodiscard]] auto advance(Tick horizon) -> core::Result<AdvanceSummary>;

    //  Validates the caller's run, bound and cutoff, closes receive, and drains accepted records up
    //  to the cutoff. The upper bound must equal the current cursor exactly.
    [[nodiscard]] auto advanceStopCut(const StopCut& cut, std::span<SettlementEvent> slots)
        -> core::Result<AdvanceSummary>;

    [[nodiscard]] auto run() const noexcept -> RunKey {
        return run_;
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

  private:
    AdvanceKernel(PreparedGameplay prepared, RunKey run, LatePolicyParameters late);

    auto route(Tick observation, bool& forwarded) const -> std::optional<Tick>;
    auto processThrough(Tick horizon) -> std::size_t;

    PreparedGameplay prepared_;
    RunKey run_{};
    LatePolicyParameters late_;
    std::optional<Tick> frontier_;
    std::optional<IngressSequence> acceptedThrough_;
    IngressSequence nextSequence_{};
    bool receiveClosed_{false};
    std::vector<IngressReceipt> archive_;
    std::vector<std::size_t> pending_;
};

} // namespace cuexis::judgement::runtime
