#pragma once

//  Judgement runtime - the advance value contract.
//
//  This header carries the value types of the v3 advance contract: the run key, the advance
//  status/fault vocabulary, the settlement event/dictionary values and the per-call outcome. The
//  transactional entry itself (`JudgementSession::advance` and `submitCanonical`) is not declared
//  in this round, so no caller can bind to a symbol the tree does not define. Events and results
//  are plain copyable values; they are not a wire format and no type-punning into an API type is
//  permitted.

#include <cuexis/judgement/phase_types.hpp>
#include <cuexis/judgement/timebase_tick.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cuexis::judgement {

//  The identity of one run: the execution scope plus the retry epoch. Both are non-zero in a bound
//  session; neither is part of the clock segment.
struct RunKey final {
    std::uint64_t sessionScope, runEpoch;
    friend auto operator==(RunKey, RunKey) noexcept -> bool = default;
};

//  The state one completed `advance` reports.
enum class AdvanceStatus : std::uint8_t { ongoing, completed, faulted };

//  Where a fault was observed: in the kernel transaction, in the independent Fold, or in the
//  surrounding control step.
enum class AdvanceFaultStage : std::uint8_t { none, kernel, fold, control };

//  What kind of fault it was: a resource failure, a checked-arithmetic failure or an invariant
//  violation. The detailed diagnostic is observed after the call returns.
enum class AdvanceFaultReason : std::uint8_t { none, resource, arithmetic, invariant };

struct FaultCode final {
    AdvanceFaultStage stage;
    AdvanceFaultReason reason;
};

//  The direction of one observed error. Absent when the phase has no evidence.
enum class TimingDirection : std::uint8_t { early, exact, late };

//  The original integer counters and denominators of one completed run. The four ratios can be
//  reconstructed exactly; this is not a floating-point score.
struct NormalizedScoreResult final {
    std::uint64_t j, jMax, c, cMax;
    std::uint32_t mj, mc, score, combo, maxCombo;
};

//  One sealed settlement fact, by value. `tick` is the settlement instant, `error` is the checked
//  `observationTick - phaseTarget`, and direction/grade are present only when evidence exists and
//  the phase selected a table.
struct SettlementEvent final {
    Tick tick;
    std::uint32_t phaseIndex;
    //  UINT32_MAX: the phase has no selected grade table.
    std::uint32_t gradeIndex;
    Outcome outcome;
    std::optional<TickDelta> error;
    std::optional<TimingDirection> direction;
};

//  One dictionary entry: the phase instance and its declaration kind.
struct SettlementPhase final {
    RequirementIdentity requirement;
    PhaseKind phase;
};

//  The owning explanation of a settlement buffer. The host copies it before the run starts, so the
//  values stay usable after the session is unloaded.
struct SettlementDictionary final {
    std::vector<SettlementPhase> phases;
    std::vector<std::string> grades;
};

//  The result of one advance call. `written` counts the slots actually filled; the slots outside
//  `written` keep their previous value. `score` is present only on the first normal completion of a
//  v3 run; ongoing, faulted, repeated-completion and legacy calls leave it empty.
struct AdvanceOutcome final {
    RunKey run;
    std::size_t written;
    std::optional<Tick> processedThrough;
    AdvanceStatus status;
    FaultCode fault;
    std::optional<NormalizedScoreResult> score;
};

} // namespace cuexis::judgement
