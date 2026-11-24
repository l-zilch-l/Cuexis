#include <cuexis/judgement/advance_kernel.hpp>

#include "advance_testing.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace cuexis::judgement::runtime {
namespace {

//  Test injection points; the values match `detail::AdvanceFaultPoint` in `advance_testing.hpp`.
constexpr std::uint8_t kBeforeSeal = 1;
constexpr std::uint8_t kAfterSealFold = 2;
constexpr std::uint8_t kBeforeNumbering = 3;
constexpr std::uint8_t kReentrantSeal = 4;
constexpr std::uint8_t kAllocationBeforeSeal = 5;
constexpr std::uint8_t kArithmeticBeforeSeal = 6;

//  The production classification of one transaction exception: an allocation failure is a
//  resource fault, a checked-arithmetic failure is an arithmetic fault, everything else is an
//  invariant violation.
auto faultReasonFromCurrentException() noexcept -> AdvanceFaultReason {
    try {
        throw;
    } catch (const std::bad_alloc&) {
        return AdvanceFaultReason::resource;
    } catch (const std::overflow_error&) {
        return AdvanceFaultReason::arithmetic;
    } catch (...) {
        return AdvanceFaultReason::invariant;
    }
}

struct TransactionGuard final {
    bool& flag;
    explicit TransactionGuard(bool& value) noexcept : flag(value) {
        flag = true;
    }
    ~TransactionGuard() {
        flag = false;
    }
};

auto kernelError(std::string code, std::string reason, std::string path, std::string message)
    -> core::Error {
    return core::Error{std::move(code), std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason))
        .withContext("fieldPath", std::move(path));
}

auto capacityError(std::size_t required, std::size_t provided) -> core::Error {
    return core::Error{"judgement.advance.output_capacity_insufficient",
                       "the settlement slots are too small"}
        .withContext("category", "budget_exceeded")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", "budget")
        .withContext("fieldPath", "/slots")
        .withContext("required", std::to_string(required))
        .withContext("provided", std::to_string(provided));
}

auto lateError(std::string message) -> core::Error {
    return core::Error{"judgement.late.rejected", std::move(message)}
        .withContext("category", "late_policy_incomplete")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}

auto validateLate(const LatePolicyParameters& late) -> core::Result<void> {
    if (!late.policy || !late.windowOpenThreshold.isMeasured() ||
        !late.windowCloseThreshold.isMeasured() || !late.finalizationWatermark.isMeasured() ||
        !late.maxQueueHop.isMeasured()) {
        return core::unexpected(lateError("the late policy is incomplete"));
    }
    const auto open = late.windowOpenThreshold.measuredValue()->value();
    const auto close = late.windowCloseThreshold.measuredValue()->value();
    const auto finalization = late.finalizationWatermark.measuredValue()->value();
    const auto hop = late.maxQueueHop.measuredValue()->value();
    if (open < 0 || close < 0 || finalization <= close ||
        (*late.policy == LateEventPolicy::rejectLate && hop != 0) ||
        (*late.policy == LateEventPolicy::queueNextTick &&
         (hop < 1 || hop > finalization - close))) {
        return core::unexpected(lateError("the late policy has an invalid O/C/F/H relationship"));
    }
    return {};
}

auto sameTimebase(const PreparedTimebase& left, const PreparedTimebase& right) noexcept -> bool {
    return left.identity == right.identity &&
           left.microsecondsNumerator == right.microsecondsNumerator &&
           left.microsecondsDenominator == right.microsecondsDenominator &&
           left.origin == right.origin;
}

auto lastTailEndOf(const PreparedRequirement& requirement) -> Tick {
    std::optional<Tick> end;
    for (const auto& window : requirement.timing.successWindows) {
        if (window.phase.kind == PhaseKind::tail && (!end || window.end > *end)) {
            end = window.end;
        }
    }
    return end.value_or(requirement.deadline);
}

} // namespace

AdvanceKernel::AdvanceKernel(PreparedGameplay prepared, RunKey run, LatePolicyParameters late)
    : prepared_(std::move(prepared)), run_(run), late_(std::move(late)) {
    const auto records = prepared_->records();
    const auto requirements = records.requirements;
    runs_.resize(requirements.size());
    phaseOffsets_.reserve(requirements.size());
    std::size_t offset = 0;
    for (const auto& requirement : requirements) {
        phaseOffsets_.push_back(offset);
        offset += requirement.phases.size();
        terminalHorizon_ = std::max(terminalHorizon_, requirement.deadline);
        terminalHorizon_ = std::max(terminalHorizon_, lastTailEndOf(requirement));
        if (requirement.timing.body) {
            terminalHorizon_ = std::max(terminalHorizon_, requirement.timing.body->end);
        }
    }
    settlementCapacity_ = offset;
    const auto* score = prepared_->records().normalizedScore;
    if (score != nullptr) {
        units_.reserve(score->units.size());
        for (const auto& unit : score->units) {
            units_.push_back(UnitRun{unit.phaseIndices, unit.phaseIndices.size(), false});
        }
    }
    state_ = AdvanceSessionState::prepared;
}

AdvanceKernel::AdvanceKernel(AdvanceKernel&& other) noexcept
    : runs_(std::move(other.runs_)), units_(std::move(other.units_)),
      phaseOffsets_(std::move(other.phaseOffsets_)), staged_(std::move(other.staged_)),
      j_(other.j_), c_(other.c_), score_(other.score_), combo_(other.combo_),
      maxCombo_(other.maxCombo_), prepared_(std::move(other.prepared_)),
      configuration_(std::move(other.configuration_)), run_(other.run_),
      late_(std::move(other.late_)), state_(other.state_), fault_(other.fault_),
      inTransaction_(false), completed_(other.completed_),
      settlementCapacity_(other.settlementCapacity_), terminalHorizon_(other.terminalHorizon_),
      frontier_(other.frontier_), acceptedThrough_(other.acceptedThrough_),
      nextSequence_(other.nextSequence_), receiveClosed_(other.receiveClosed_),
      archive_(std::move(other.archive_)), records_(std::move(other.records_)),
      settled_(std::move(other.settled_)), pending_(std::move(other.pending_)),
      testFaultPoint_(other.testFaultPoint_), testSealOrdinal_(other.testSealOrdinal_),
      testAtSeal_(std::move(other.testAtSeal_)) {
    other.prepared_.reset();
    other.state_ = AdvanceSessionState::unprepared;
    other.inTransaction_ = false;
    other.staged_.clear();
}

auto AdvanceKernel::operator=(AdvanceKernel&& other) noexcept -> AdvanceKernel& {
    if (this == &other) {
        return *this;
    }
    runs_ = std::move(other.runs_);
    units_ = std::move(other.units_);
    phaseOffsets_ = std::move(other.phaseOffsets_);
    staged_ = std::move(other.staged_);
    j_ = other.j_;
    c_ = other.c_;
    score_ = other.score_;
    combo_ = other.combo_;
    maxCombo_ = other.maxCombo_;
    prepared_ = std::move(other.prepared_);
    configuration_ = std::move(other.configuration_);
    run_ = other.run_;
    late_ = std::move(other.late_);
    state_ = other.state_;
    fault_ = other.fault_;
    inTransaction_ = false;
    completed_ = other.completed_;
    settlementCapacity_ = other.settlementCapacity_;
    terminalHorizon_ = other.terminalHorizon_;
    frontier_ = other.frontier_;
    acceptedThrough_ = other.acceptedThrough_;
    nextSequence_ = other.nextSequence_;
    receiveClosed_ = other.receiveClosed_;
    archive_ = std::move(other.archive_);
    records_ = std::move(other.records_);
    settled_ = std::move(other.settled_);
    pending_ = std::move(other.pending_);
    testFaultPoint_ = other.testFaultPoint_;
    testSealOrdinal_ = other.testSealOrdinal_;
    testAtSeal_ = std::move(other.testAtSeal_);
    other.prepared_.reset();
    other.state_ = AdvanceSessionState::unprepared;
    other.inTransaction_ = false;
    other.staged_.clear();
    return *this;
}

auto AdvanceKernel::prepare(PreparedGameplay prepared, RunKey run) -> core::Result<AdvanceKernel> {
    if (run.sessionScope == 0 || run.runEpoch == 0) {
        return core::unexpected(kernelError("session.mutation_rejected", "run_zero", "/run",
                                            "the run key components must be non-zero"));
    }
    auto valid = validateLate(prepared.records().latePolicy);
    if (!valid) {
        return core::unexpected(valid.error());
    }
    auto late = prepared.records().latePolicy;
    return AdvanceKernel{std::move(prepared), run, std::move(late)};
}

auto AdvanceKernel::prepare(SessionConfiguration configuration, PreparedGameplay prepared,
                            RunKey run) -> core::Result<AdvanceKernel> {
    if (configuration.legacyRulesetToken.has_value()) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "legacy_ruleset_unsupported", "/session/ruleset",
                                            "a v3 runtime session refuses a legacy ruleset"));
    }
    const auto records = prepared.records();
    if (!sameTimebase(configuration.timebase, records.timebase)) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "timebase_mismatch", "/timebase",
                                            "the session timebase differs from the prepared run"));
    }
    if (configuration.identityDeclarations != prepared.identityDeclarations()) {
        return core::unexpected(
            core::Error{"playback.gameplay.invalid",
                        "the session identity declarations differ from the prepared run"}
                .withContext("category", "identity_closure_incomplete")
                .withContext("severity", "error")
                .withContext("faulted", "false")
                .withContext("fieldPath", "/identityDeclarations"));
    }
    if (configuration.latePolicy != records.latePolicy) {
        return core::unexpected(
            kernelError("judgement.execution.relation_invalid", "late_policy_mismatch",
                        "/latePolicy", "the session late policy differs from the prepared run"));
    }
    if (!configuration.executionProfileToken.empty() &&
        configuration.executionProfileToken != records.executionProfile) {
        return core::unexpected(kernelError(
            "judgement.execution.relation_invalid", "execution_profile_override",
            "/session/executionProfile", "the session overrides the prepared execution profile"));
    }
    if (!configuration.lateAlgorithmToken.empty() &&
        configuration.lateAlgorithmToken !=
            prepared.identityDeclarations().engine.lateAlgorithmToken.value_or("")) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "late_algorithm_override", "/session/lateAlgorithm",
                                            "the session overrides the prepared late algorithm"));
    }
    if (!configuration.factSemanticRevision.empty() &&
        configuration.factSemanticRevision !=
            prepared.identityDeclarations().engine.factSemanticRevision) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "fact_semantics_override", "/session/factSemantic",
                                            "the session overrides the prepared fact semantics"));
    }
    auto kernel = prepare(std::move(prepared), run);
    if (!kernel) {
        return kernel;
    }
    kernel->configuration_ = std::move(configuration);
    return kernel;
}

auto AdvanceKernel::rejectWrongThread() const -> core::Error {
    return kernelError("session.mutation_rejected", "wrong_thread", "/session/owner",
                       "the advance kernel is bound to another thread");
}

auto AdvanceKernel::rejectState(std::string_view reason, std::string_view message) const
    -> core::Error {
    return kernelError("session.mutation_rejected", std::string{reason}, "/session/state",
                       std::string{message});
}

auto AdvanceKernel::rejectRun(std::string_view reason, std::string_view message) const
    -> core::Error {
    return kernelError("session.mutation_rejected", std::string{reason}, "/run",
                       std::string{message});
}

void AdvanceKernel::markFaulted(AdvanceFaultStage stage, AdvanceFaultReason reason) {
    state_ = AdvanceSessionState::faulted;
    fault_ = FaultCode{stage, reason};
}

auto AdvanceKernel::dictionary() const noexcept -> const SettlementDictionary& {
    static const SettlementDictionary empty;
    return prepared_ ? prepared_->settlementDictionary() : empty;
}

auto AdvanceKernel::route(Tick observation, bool& forwarded) const -> std::optional<Tick> {
    forwarded = false;
    if (!frontier_) {
        return observation;
    }
    const auto close =
        offsetTicks(*frontier_, TickSpan{-late_.windowCloseThreshold.measuredValue()->value()});
    if (!close) {
        return std::nullopt;
    }
    if (observation >= *close) {
        return observation;
    }
    const auto finalization =
        offsetTicks(observation, *late_.finalizationWatermark.measuredValue());
    if (!finalization || observation >= *finalization ||
        *late_.policy == LateEventPolicy::rejectLate) {
        return std::nullopt;
    }
    auto queued = offsetTicks(*close, TickSpan{1});
    if (!queued) {
        return std::nullopt;
    }
    const auto hop = differenceTicks(*queued, observation);
    if (!hop || hop->value() < 1 || hop->value() > late_.maxQueueHop.measuredValue()->value()) {
        return std::nullopt;
    }
    forwarded = true;
    return *queued;
}

auto AdvanceKernel::receiveCanonicalBatch(std::span<const TickRecord> records)
    -> core::Result<std::vector<IngressReceipt>> {
    if (!owner_.isCurrent()) {
        return core::unexpected(rejectWrongThread());
    }
    if (inTransaction_) {
        return core::unexpected(kernelError("session.mutation_rejected", "reentrant",
                                            "/session/owner",
                                            "the advance kernel is already in a transaction"));
    }
    if (state_ != AdvanceSessionState::prepared) {
        if (state_ == AdvanceSessionState::faulted) {
            return core::unexpected(
                rejectState("faulted", "the advance kernel is already faulted"));
        }
        return core::unexpected(
            rejectState("not_prepared", "the advance kernel has no prepared run"));
    }
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "receive_closed", "/receive",
                                            "the receive boundary is closed"));
    }
    if (testFaultPoint_ == kBeforeNumbering) {
        return core::unexpected(core::Error{"host.rpa.allocation_failed",
                                            "the ingress reservation could not be allocated"}
                                    .withContext("category", "budget_exceeded")
                                    .withContext("severity", "error")
                                    .withContext("faulted", "false")
                                    .withContext("reason", "allocation_failed")
                                    .withContext("fieldPath", "/records"));
    }
    for (const auto& record : records) {
        if (record.domainToken.empty() || record.sourceClass.empty() ||
            record.channelToken.empty()) {
            return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                                "canonical_invalid", "/records",
                                                "a canonical record has an empty identity field"));
        }
    }
    if (!records.empty() &&
        records.size() > std::numeric_limits<std::uint64_t>::max() - nextSequence_.value()) {
        return core::unexpected(kernelError("host.rpa.capacity_exceeded", "capacity", "/records",
                                            "the sequence space is exhausted"));
    }
    TransactionGuard guard{inTransaction_};
    //  Number the whole batch before any late/admission decision, then commit records and cursor
    //  together. No operation after this point can fail.
    std::vector<IngressReceipt> receipts;
    receipts.reserve(records.size());
    auto sequence = nextSequence_.value();
    for (const auto& record : records) {
        bool forwarded = false;
        const auto dispatch = route(record.observationTick, forwarded);
        IngressReceipt receipt;
        receipt.sequence = IngressSequence{sequence};
        receipt.dispatchTick = dispatch.value_or(record.observationTick);
        receipt.accepted = dispatch.has_value();
        receipt.forwarded = forwarded;
        if (!dispatch) {
            receipt.rejectionCode = "judgement.late.rejected";
        }
        ++sequence;
        receipts.push_back(std::move(receipt));
    }
    for (std::size_t index = 0; index < receipts.size(); ++index) {
        const auto& receipt = receipts[index];
        archive_.push_back(receipt);
        records_.push_back(records[index]);
        if (receipt.accepted) {
            pending_.push_back(archive_.size() - 1);
        }
        acceptedThrough_ = receipt.sequence;
    }
    nextSequence_ = IngressSequence{sequence};
    return receipts;
}

auto AdvanceKernel::acceptedIngressThrough() const noexcept -> std::optional<IngressSequence> {
    return acceptedThrough_;
}

auto AdvanceKernel::replay() const -> ReplaySnapshot {
    ReplaySnapshot snapshot;
    snapshot.records.reserve(archive_.size());
    for (std::size_t index = 0; index < archive_.size(); ++index) {
        const auto& receipt = archive_[index];
        snapshot.records.push_back(ReplayRecord{receipt.sequence, receipt.dispatchTick,
                                                receipt.accepted, receipt.rejectionCode,
                                                records_[index]});
    }
    snapshot.events = settled_;
    snapshot.frontier = frontier_;
    snapshot.j = j_;
    snapshot.c = c_;
    snapshot.score = score_;
    return snapshot;
}

auto AdvanceKernel::summaryOf(std::optional<Tick> processedThrough, std::size_t processed,
                              std::size_t written) const -> AdvanceSummary {
    const auto* normalized = prepared_ ? prepared_->records().normalizedScore : nullptr;
    AdvanceSummary summary;
    summary.processedThrough = processedThrough;
    summary.processedRecords = processed;
    summary.settledEvents = written;
    summary.written = written;
    summary.j = j_;
    summary.c = c_;
    summary.score = score_;
    summary.jMax = normalized ? normalized->basis.jMax : 0;
    summary.cMax = normalized ? normalized->basis.cMax : 0;
    summary.mj = normalized ? normalized->mode.mj : 0;
    summary.mc = normalized ? normalized->mode.mc : 0;
    summary.combo = combo_;
    summary.maxCombo = maxCombo_;
    summary.status = completed_ ? AdvanceStatus::completed : AdvanceStatus::ongoing;
    summary.fault = fault_;
    return summary;
}

auto AdvanceKernel::runTransaction(Tick horizon, const RunKey* run,
                                   std::span<SettlementEvent> slots, bool enforceSlots,
                                   bool closeReceive) -> core::Result<AdvanceSummary> {
    if (!owner_.isCurrent()) {
        return core::unexpected(rejectWrongThread());
    }
    if (inTransaction_) {
        return core::unexpected(kernelError("session.mutation_rejected", "reentrant",
                                            "/session/owner",
                                            "the advance kernel is already in a transaction"));
    }
    if (state_ != AdvanceSessionState::prepared) {
        if (state_ == AdvanceSessionState::faulted) {
            return core::unexpected(
                rejectState("faulted", "the advance kernel is already faulted"));
        }
        return core::unexpected(
            rejectState("not_prepared", "the advance kernel has no prepared run"));
    }
    if (closeReceive && receiveClosed_) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "receive_closed", "/stopCut",
                                            "a StopCut was already accepted"));
    }
    if (run != nullptr && *run != run_) {
        return core::unexpected(rejectRun("run_mismatch", "the advance call names another run"));
    }
    if (frontier_ && horizon < *frontier_) {
        return core::unexpected(kernelError("judgement.timebase.time_reversal", "horizon_reversed",
                                            "/horizon", "the advance horizon moved backwards"));
    }
    if (enforceSlots && slots.size() < settlementCapacity_) {
        return core::unexpected(capacityError(settlementCapacity_, slots.size()));
    }
    TransactionGuard guard{inTransaction_};
    if (closeReceive) {
        receiveClosed_ = true;
    }

    //  Ordinary advance finalizes timers only through `horizon - windowCloseThreshold`; the StopCut
    //  already carries the stopped frontier and seals through its cutoff without the offset.
    auto sealLimit = horizon;
    if (!closeReceive) {
        auto limit =
            offsetTicks(horizon, TickSpan{-late_.windowCloseThreshold.measuredValue()->value()});
        if (!limit) {
            return core::unexpected(
                kernelError("judgement.timebase.tick_overflow", "tick_overflow", "/horizon",
                            "the close-threshold seal limit is not representable"));
        }
        sealLimit = *limit;
    }

    staged_.clear();
    auto makeSummary = [this, enforceSlots](std::optional<Tick> through, std::size_t processed,
                                            std::size_t written) {
        auto summary = summaryOf(through, processed, written);
        if (!enforceSlots) {
            summary.written = 0;
        }
        return summary;
    };

    std::size_t writtenTotal = 0;
    std::size_t processedSealed = 0;
    std::size_t sealOrdinal = 0;
    std::optional<Tick> sealedFrontier = frontier_;
    auto committedRuns = runs_;
    auto committedPending = pending_;
    AdvanceFaultStage stage = AdvanceFaultStage::kernel;
    auto faultSummary = [&](AdvanceFaultStage faultStage, AdvanceFaultReason reason) {
        runs_ = std::move(committedRuns);
        pending_ = std::move(committedPending);
        staged_.clear();
        markFaulted(faultStage, reason);
        auto summary = makeSummary(sealedFrontier, processedSealed, writtenTotal);
        summary.status = AdvanceStatus::faulted;
        summary.fault = fault_;
        summary.normalized.reset();
        return summary;
    };

    try {
        //  Candidate seals: every tick that can carry a dispatch or a due timer, plus the horizon.
        //  A record forwarded behind the sealed frontier (the late policy may queue it to
        //  `close+1 <= frontier`) is processed in the first seal without moving the frontier back.
        std::vector<Tick> ticks;
        ticks.reserve(pending_.size() + (4 * prepared_->requirements().size()) + 1);
        std::vector<std::size_t> behind;
        std::vector<std::size_t> dispatched;
        for (const auto index : pending_) {
            const auto dispatch = archive_[index].dispatchTick;
            if (dispatch > horizon) {
                continue;
            }
            if (frontier_ && dispatch < *frontier_) {
                behind.push_back(index);
            } else {
                dispatched.push_back(index);
                ticks.push_back(dispatch);
            }
        }
        for (const auto& requirement : prepared_->requirements()) {
            if (requirement.deadline <= sealLimit &&
                (!frontier_ || requirement.deadline >= *frontier_)) {
                ticks.push_back(requirement.deadline);
            }
            if (requirement.timing.body && requirement.timing.body->end <= sealLimit &&
                (!frontier_ || requirement.timing.body->end >= *frontier_)) {
                ticks.push_back(requirement.timing.body->end);
            }
            const auto tailEnd = lastTailEnd(requirement);
            if (tailEnd <= sealLimit && (!frontier_ || tailEnd >= *frontier_)) {
                ticks.push_back(tailEnd);
            }
        }
        ticks.push_back(horizon);
        std::sort(ticks.begin(), ticks.end());
        ticks.erase(std::unique(ticks.begin(), ticks.end()), ticks.end());
        if (frontier_) {
            ticks.erase(std::remove_if(ticks.begin(), ticks.end(),
                                       [this](Tick tick) { return tick < *frontier_; }),
                        ticks.end());
        }
        std::sort(behind.begin(), behind.end(), [this](auto left, auto right) {
            return archive_[left].sequence < archive_[right].sequence;
        });
        std::sort(dispatched.begin(), dispatched.end(), [this](auto left, auto right) {
            if (archive_[left].dispatchTick != archive_[right].dispatchTick) {
                return archive_[left].dispatchTick < archive_[right].dispatchTick;
            }
            return archive_[left].sequence < archive_[right].sequence;
        });

        bool behindDone = false;
        std::size_t dispatchedCursor = 0;
        for (const auto tick : ticks) {
            staged_.clear();
            settleTimers(tick, sealLimit);
            std::size_t processedThisTick = 0;
            if (!behindDone) {
                behindDone = true;
                for (const auto index : behind) {
                    if (const auto requirement = requirementIndexFor(records_[index])) {
                        settleRecord(*requirement, records_[index], archive_[index].dispatchTick);
                    }
                    pending_.erase(std::find(pending_.begin(), pending_.end(), index));
                    ++processedThisTick;
                }
            }
            while (dispatchedCursor < dispatched.size() &&
                   archive_[dispatched[dispatchedCursor]].dispatchTick <= tick) {
                const auto index = dispatched[dispatchedCursor++];
                if (const auto requirement = requirementIndexFor(records_[index])) {
                    settleRecord(*requirement, records_[index], archive_[index].dispatchTick);
                }
                pending_.erase(std::find(pending_.begin(), pending_.end(), index));
                ++processedThisTick;
            }
            //  Timers whose stage became complete on this tick's records settle in the same seal.
            settleTimers(tick, sealLimit);
            const bool eventBearing = !staged_.empty();
            if (eventBearing) {
                ++sealOrdinal;
            }
            if (eventBearing && sealOrdinal == testSealOrdinal_) {
                if (testFaultPoint_ == kBeforeSeal) {
                    throw std::runtime_error{"injected transaction failure"};
                }
                if (testFaultPoint_ == kAllocationBeforeSeal) {
                    throw std::bad_alloc{};
                }
                if (testFaultPoint_ == kArithmeticBeforeSeal) {
                    throw std::overflow_error{"injected checked arithmetic failure"};
                }
            }
            const auto count = staged_.size();
            //  The settled history insert may allocate; it runs before any slot or frontier is
            //  published, so a failure keeps the whole call unsealed.
            settled_.insert(settled_.end(), staged_.begin(), staged_.end());
            if (enforceSlots) {
                for (std::size_t index = 0; index < count; ++index) {
                    slots[writtenTotal + index] = staged_[index];
                }
            }
            frontier_ = tick;
            sealedFrontier = tick;
            processedSealed += processedThisTick;
            writtenTotal += count;
            committedRuns = runs_;
            committedPending = pending_;
            if (eventBearing) {
                stage = AdvanceFaultStage::fold;
                if (testFaultPoint_ == kAfterSealFold && sealOrdinal == testSealOrdinal_) {
                    throw std::runtime_error{"injected Fold failure"};
                }
                for (const auto& event : staged_) {
                    applyFold(event);
                }
                stage = AdvanceFaultStage::kernel;
            }
            if (eventBearing && testFaultPoint_ == kReentrantSeal &&
                sealOrdinal == testSealOrdinal_ && testAtSeal_) {
                auto callback = std::move(testAtSeal_);
                testAtSeal_ = {};
                callback();
            }
            staged_.clear();
        }
    } catch (...) {
        return faultSummary(stage, faultReasonFromCurrentException());
    }
    score_ = computeScore();
    const bool terminal = std::all_of(runs_.begin(), runs_.end(), [](const auto& run) {
        return run.stage == RequirementRun::Stage::done;
    });
    const bool finished =
        frontier_.has_value() && *frontier_ >= terminalHorizon_ && pending_.empty() && terminal;
    auto summary = makeSummary(horizon, processedSealed, writtenTotal);
    if (finished) {
        summary.status = AdvanceStatus::completed;
        if (!completed_) {
            const auto* normalized = prepared_ ? prepared_->records().normalizedScore : nullptr;
            summary.normalized = NormalizedScoreResult{static_cast<std::uint64_t>(j_),
                                                       normalized ? normalized->basis.jMax : 0,
                                                       static_cast<std::uint64_t>(c_),
                                                       normalized ? normalized->basis.cMax : 0,
                                                       normalized ? normalized->mode.mj : 0,
                                                       normalized ? normalized->mode.mc : 0,
                                                       score_,
                                                       combo_,
                                                       maxCombo_};
            completed_ = true;
        }
    }
    return summary;
}

auto AdvanceKernel::advanceInternal(Tick horizon, const RunKey* run,
                                    std::span<SettlementEvent> slots, bool enforceSlots)
    -> core::Result<AdvanceSummary> {
    return runTransaction(horizon, run, slots, enforceSlots, false);
}

auto AdvanceKernel::advance(Tick horizon) -> core::Result<AdvanceSummary> {
    return advanceInternal(horizon, nullptr, std::span<SettlementEvent>{}, false);
}

auto AdvanceKernel::advance(Tick horizon, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceSummary> {
    return advanceInternal(horizon, nullptr, slots, true);
}

auto AdvanceKernel::advance(Tick horizon, RunKey run, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceSummary> {
    return advanceInternal(horizon, &run, slots, true);
}

auto AdvanceKernel::stopCutInternal(const StopCut& cut, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceSummary> {
    if (!owner_.isCurrent()) {
        return core::unexpected(rejectWrongThread());
    }
    if (inTransaction_) {
        return core::unexpected(kernelError("session.mutation_rejected", "reentrant",
                                            "/session/owner",
                                            "the advance kernel is already in a transaction"));
    }
    if (state_ != AdvanceSessionState::prepared) {
        if (state_ == AdvanceSessionState::faulted) {
            return core::unexpected(
                rejectState("faulted", "the advance kernel is already faulted"));
        }
        return core::unexpected(
            rejectState("not_prepared", "the advance kernel has no prepared run"));
    }
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "receive_closed", "/stopCut",
                                            "a StopCut was already accepted"));
    }
    if (cut.run != run_) {
        return core::unexpected(rejectRun("run_mismatch", "the StopCut names another run"));
    }
    if (cut.acceptedThrough != acceptedThrough_) {
        return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                            "bound_mismatch", "/acceptedThrough",
                                            "the StopCut upper bound is not the kernel cursor"));
    }
    if (frontier_ && cut.cutoff < *frontier_) {
        return core::unexpected(kernelError("judgement.timebase.time_reversal", "cutoff_reversed",
                                            "/cutoff",
                                            "the Stop cutoff precedes the sealed frontier"));
    }
    if (slots.size() < settlementCapacity_) {
        return core::unexpected(capacityError(settlementCapacity_, slots.size()));
    }
    return runTransaction(cut.cutoff, &run_, slots, true, true);
}

auto AdvanceKernel::advanceStopCut(const StopCut& cut, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceSummary> {
    return stopCutInternal(cut, slots);
}

auto AdvanceKernel::requirementIndexFor(const TickRecord& record) const
    -> std::optional<std::size_t> {
    const auto requirements = prepared_->requirements();
    for (std::size_t index = 0; index < requirements.size(); ++index) {
        for (const auto& binding : requirements[index].atomBindings) {
            if (binding.domainToken != record.domainToken ||
                binding.sourceClass != record.sourceClass ||
                binding.channelToken != record.channelToken || binding.action != record.action) {
                continue;
            }
            if (binding.amountRange) {
                if (!record.amount || *record.amount < binding.amountRange->minimum ||
                    *record.amount > binding.amountRange->maximum) {
                    continue;
                }
            }
            return index;
        }
    }
    return std::nullopt;
}

auto AdvanceKernel::phaseOffset(std::size_t requirement) const -> std::size_t {
    return phaseOffsets_.at(requirement);
}

auto AdvanceKernel::inWindow(const PreparedRequirement& requirement, PhaseKind kind,
                             Tick tick) const -> bool {
    return std::any_of(requirement.timing.successWindows.begin(),
                       requirement.timing.successWindows.end(), [&](const auto& window) {
                           return window.phase.kind == kind && tick >= window.start &&
                                  tick < window.end;
                       });
}

auto AdvanceKernel::targetOf(const PreparedRequirement& requirement, PhaseKind kind) const -> Tick {
    for (const auto& target : requirement.timing.phaseTargets) {
        if (target.phase == kind) {
            return target.chartTick;
        }
    }
    return Tick{0};
}

auto AdvanceKernel::lastTailEnd(const PreparedRequirement& requirement) const -> Tick {
    return lastTailEndOf(requirement);
}

auto AdvanceKernel::gradeIndex(std::size_t requirement, PhaseKind phase, std::int64_t error) const
    -> std::optional<std::uint32_t> {
    const auto& preparedRequirement = prepared_->requirements()[requirement];
    for (const auto& component : preparedRequirement.measure) {
        if (component.phase != phase || !component.gradeTableIndex) {
            continue;
        }
        const auto* score = prepared_->records().normalizedScore;
        if (score == nullptr || *component.gradeTableIndex >= score->gradeTables.size()) {
            return std::nullopt;
        }
        const auto* grade = preparedGradeAt(score->gradeTables[*component.gradeTableIndex], error);
        if (grade == nullptr) {
            return std::nullopt;
        }
        const auto& grades = prepared_->settlementDictionary().grades;
        const auto found = std::find(grades.begin(), grades.end(), *grade);
        if (found == grades.end()) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(found - grades.begin());
    }
    return std::nullopt;
}

//  The direction is independent of the grade: it reads the selected table's own exact band, so an
//  error inside [exactStart, exactEnd) is Exact even when the grading interval around it differs.
//  A phase with no selected table has no direction evidence at all.
auto AdvanceKernel::directionOf(std::size_t requirement, PhaseKind phase, std::int64_t error) const
    -> std::optional<TimingDirection> {
    const auto& preparedRequirement = prepared_->requirements()[requirement];
    for (const auto& component : preparedRequirement.measure) {
        if (component.phase != phase || !component.gradeTableIndex) {
            continue;
        }
        const auto* score = prepared_->records().normalizedScore;
        if (score == nullptr || *component.gradeTableIndex >= score->gradeTables.size()) {
            return std::nullopt;
        }
        const auto& table = score->gradeTables[*component.gradeTableIndex];
        if (error < table.exactStart) {
            return TimingDirection::early;
        }
        if (error >= table.exactEnd) {
            return TimingDirection::late;
        }
        return TimingDirection::exact;
    }
    return std::nullopt;
}

auto AdvanceKernel::emit(std::size_t requirement, PhaseKind phase, Tick tick, Outcome outcome,
                         std::optional<std::int64_t> error) -> SettlementEvent {
    SettlementEvent event{};
    event.gradeIndex = std::numeric_limits<std::uint32_t>::max();
    event.tick = tick;
    const auto& phases = prepared_->requirements()[requirement].phases;
    for (std::size_t index = 0; index < phases.size(); ++index) {
        if (phases[index].kind == phase) {
            event.phaseIndex = static_cast<std::uint32_t>(phaseOffset(requirement) + index);
            break;
        }
    }
    event.outcome = outcome;
    event.error = error ? std::optional<TickDelta>{TickDelta{*error}} : std::nullopt;
    if (error) {
        event.gradeIndex = gradeIndex(requirement, phase, *error)
                               .value_or(std::numeric_limits<std::uint32_t>::max());
        event.direction = directionOf(requirement, phase, *error);
    }
    staged_.push_back(event);
    return event;
}

auto AdvanceKernel::computeScore() const -> std::uint32_t {
    const auto* score = prepared_ ? prepared_->records().normalizedScore : nullptr;
    if (score == nullptr) {
        return 0;
    }
    if (score->basis.jMax == 0 || score->basis.cMax == 0) {
        return 0;
    }
    const auto output = normalizedScoreOutput(score->mode.mj, score->mode.mc, score->mode.total,
                                              static_cast<std::uint64_t>(j_), score->basis.jMax,
                                              static_cast<std::uint64_t>(c_), score->basis.cMax);
    return output ? output.value() : 0;
}

void AdvanceKernel::applyFold(const SettlementEvent& event) {
    const auto* score = prepared_->records().normalizedScore;
    if (score == nullptr) {
        return;
    }
    const auto& phases = prepared_->settlementDictionary().phases;
    if (event.phaseIndex >= phases.size()) {
        return;
    }
    const auto phase = phases[event.phaseIndex].phase;
    std::optional<std::string> grade;
    if (event.gradeIndex != std::numeric_limits<std::uint32_t>::max()) {
        const auto& grades = prepared_->settlementDictionary().grades;
        if (event.gradeIndex < grades.size()) {
            grade = grades[event.gradeIndex];
        }
    }
    const auto* rule = static_cast<const ScoreRuleRow*>(nullptr);
    for (const auto& candidate : score->mode.rules) {
        if (candidate.phase == phase && candidate.outcome == event.outcome &&
            candidate.grade == grade) {
            rule = &candidate;
            break;
        }
    }
    auto unit = units_.end();
    for (auto candidate = units_.begin(); candidate != units_.end(); ++candidate) {
        if (std::find(candidate->phases.begin(), candidate->phases.end(), event.phaseIndex) !=
            candidate->phases.end()) {
            unit = candidate;
            break;
        }
    }
    if (unit == units_.end()) {
        return;
    }
    if (rule != nullptr && rule->qualifies && !unit->settled) {
        j_ += rule->judgePoints;
        switch (rule->combo) {
        case ComboAction::increment: {
            ++combo_;
            maxCombo_ = std::max(maxCombo_, combo_);
            const auto reward = comboReward(score->mode, combo_);
            if (reward) {
                c_ += static_cast<std::int64_t>(*reward);
            }
            break;
        }
        case ComboAction::reset:
            combo_ = 0;
            break;
        case ComboAction::keep:
            break;
        }
        unit->settled = true;
        unit->remaining = 0;
        return;
    }
    if (unit->settled) {
        return;
    }
    if (unit->remaining > 0) {
        --unit->remaining;
    }
    if (unit->remaining == 0) {
        if (score->mode.onExhausted == ComboAction::reset) {
            combo_ = 0;
        }
    }
}

void AdvanceKernel::settleRecord(std::size_t requirement, const TickRecord& record, Tick dispatch) {
    auto& run = runs_[requirement];
    auto& preparedRequirement = prepared_->requirements()[requirement];
    const bool hasTap =
        std::any_of(preparedRequirement.phases.begin(), preparedRequirement.phases.end(),
                    [](const auto& phase) { return phase.kind == PhaseKind::tap; });
    if (record.action == InputAction::press) {
        if (run.stage != RequirementRun::Stage::dormant) {
            return;
        }
        if (hasTap) {
            if (inWindow(preparedRequirement, PhaseKind::tap, record.observationTick)) {
                const auto target = targetOf(preparedRequirement, PhaseKind::tap);
                const auto error = record.observationTick.value() - target.value();
                emit(requirement, PhaseKind::tap, dispatch, Outcome::hit, error);
                run.tapDone = true;
                run.stage = RequirementRun::Stage::done;
            }
            return;
        }
        const bool headWindow =
            inWindow(preparedRequirement, PhaseKind::head, record.observationTick);
        if (!headWindow) {
            return;
        }
        emit(requirement, PhaseKind::head, dispatch, Outcome::hit, std::nullopt);
        run.pressObservation = record.observationTick;
        run.stage = RequirementRun::Stage::body;
        return;
    }
    if (record.action != InputAction::release) {
        return;
    }
    if (run.stage != RequirementRun::Stage::body && run.stage != RequirementRun::Stage::tail) {
        return;
    }
    const auto observation = record.observationTick;
    bool bodyMissed = false;
    if (run.stage == RequirementRun::Stage::body) {
        const bool bodyHit =
            preparedRequirement.timing.body && observation >= preparedRequirement.timing.body->end;
        if (!bodyHit) {
            const auto target = targetOf(preparedRequirement, PhaseKind::body);
            const auto error = observation.value() - target.value();
            emit(requirement, PhaseKind::body, dispatch, Outcome::miss, error);
            bodyMissed = true;
        } else {
            emit(requirement, PhaseKind::body, dispatch, Outcome::hit, std::nullopt);
        }
        run.stage = RequirementRun::Stage::tail;
    }
    if (bodyMissed) {
        emit(requirement, PhaseKind::tail, dispatch, Outcome::miss, std::nullopt);
        run.stage = RequirementRun::Stage::done;
        return;
    }
    const auto tailTarget = targetOf(preparedRequirement, PhaseKind::tail);
    const auto tailEnd = lastTailEnd(preparedRequirement);
    if (inWindow(preparedRequirement, PhaseKind::tail, observation)) {
        const auto error = observation.value() - tailTarget.value();
        emit(requirement, PhaseKind::tail, dispatch, Outcome::hit, error);
    } else if (observation < tailEnd) {
        const auto error = observation.value() - tailTarget.value();
        emit(requirement, PhaseKind::tail, dispatch, Outcome::miss, error);
    } else {
        emit(requirement, PhaseKind::tail, tailEnd, Outcome::miss, std::nullopt);
    }
    run.stage = RequirementRun::Stage::done;
}

void AdvanceKernel::settleTimers(Tick tick, Tick sealLimit) {
    const auto requirements = prepared_->requirements();
    for (std::size_t index = 0; index < runs_.size(); ++index) {
        auto& run = runs_[index];
        const auto& requirement = requirements[index];
        if (run.stage == RequirementRun::Stage::body && requirement.timing.body &&
            tick >= requirement.timing.body->end && sealLimit >= requirement.timing.body->end) {
            emit(index, PhaseKind::body, requirement.timing.body->end, Outcome::hit, std::nullopt);
            run.stage = RequirementRun::Stage::tail;
        }
        const auto tailEnd = lastTailEnd(requirement);
        if (run.stage == RequirementRun::Stage::tail && tick >= tailEnd && sealLimit >= tailEnd) {
            emit(index, PhaseKind::tail, tailEnd, Outcome::miss, std::nullopt);
            run.stage = RequirementRun::Stage::done;
        }
        const bool hasTap =
            std::any_of(requirement.phases.begin(), requirement.phases.end(),
                        [](const auto& phase) { return phase.kind == PhaseKind::tap; });
        if (run.stage == RequirementRun::Stage::dormant && hasTap && tick >= requirement.deadline &&
            sealLimit >= requirement.deadline) {
            emit(index, PhaseKind::tap, requirement.deadline, Outcome::miss, std::nullopt);
            run.stage = RequirementRun::Stage::done;
        }
    }
}

} // namespace cuexis::judgement::runtime
