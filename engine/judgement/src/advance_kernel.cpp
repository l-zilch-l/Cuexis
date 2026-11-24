#include <cuexis/judgement/advance_kernel.hpp>

#include <algorithm>
#include <utility>

namespace cuexis::judgement::runtime {
namespace {

auto kernelError(std::string code, std::string reason, std::string path, std::string message)
    -> core::Error {
    return core::Error{std::move(code), std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason))
        .withContext("fieldPath", std::move(path));
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

} // namespace

AdvanceKernel::AdvanceKernel(PreparedGameplay prepared, RunKey run, LatePolicyParameters late)
    : prepared_(std::move(prepared)), run_(run), late_(std::move(late)) {
    const auto requirements = prepared_.requirements();
    runs_.resize(requirements.size());
    phaseOffsets_.reserve(requirements.size());
    std::size_t offset = 0;
    for (const auto& requirement : requirements) {
        phaseOffsets_.push_back(offset);
        offset += requirement.phases.size();
    }
    const auto* score = prepared_.records().normalizedScore;
    if (score != nullptr) {
        units_.reserve(score->units.size());
        for (const auto& unit : score->units) {
            units_.push_back(UnitRun{unit.phaseIndices, unit.phaseIndices.size(), false});
        }
    }
}

auto AdvanceKernel::prepare(PreparedGameplay prepared, RunKey run) -> core::Result<AdvanceKernel> {
    auto valid = validateLate(prepared.records().latePolicy);
    if (!valid) {
        return core::unexpected(valid.error());
    }
    auto late = prepared.records().latePolicy;
    return AdvanceKernel{std::move(prepared), run, std::move(late)};
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
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.stop_cut.closed", "receive_closed",
                                            "/receive", "the receive boundary is closed"));
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

auto AdvanceKernel::processThrough(Tick horizon) -> std::size_t {
    std::vector<std::size_t> ordered;
    ordered.reserve(pending_.size());
    for (const auto index : pending_) {
        if (archive_[index].dispatchTick <= horizon) {
            ordered.push_back(index);
        }
    }
    std::sort(ordered.begin(), ordered.end(), [this](auto left, auto right) {
        if (archive_[left].dispatchTick != archive_[right].dispatchTick) {
            return archive_[left].dispatchTick < archive_[right].dispatchTick;
        }
        return archive_[left].sequence < archive_[right].sequence;
    });
    std::size_t processed = 0;
    for (const auto index : ordered) {
        const auto dispatch = archive_[index].dispatchTick;
        settleTimers(dispatch);
        if (const auto requirement = requirementIndexFor(records_[index])) {
            settleRecord(*requirement, records_[index], dispatch);
        }
        pending_.erase(std::find(pending_.begin(), pending_.end(), index));
        ++processed;
    }
    settleTimers(horizon);
    return processed;
}

auto AdvanceKernel::advanceInternal(Tick horizon, std::span<SettlementEvent> slots,
                                    bool enforceSlots) -> core::Result<AdvanceSummary> {
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.stop_cut.closed", "receive_closed",
                                            "/advance", "the receive boundary is closed"));
    }
    if (frontier_ && horizon < *frontier_) {
        return core::unexpected(kernelError("judgement.execution.horizon_reversed",
                                            "horizon_reversed", "/horizon",
                                            "the advance horizon moved backwards"));
    }
    auto runs = runs_;
    auto units = units_;
    auto pending = pending_;
    auto j = j_;
    auto c = c_;
    auto score = score_;
    auto combo = combo_;
    auto maxCombo = maxCombo_;
    std::vector<SettlementEvent> staged;
    staged_.clear();
    const auto processed = processThrough(horizon);
    if (enforceSlots && staged_.size() > slots.size()) {
        runs_ = std::move(runs);
        units_ = std::move(units);
        pending_ = std::move(pending);
        j_ = j;
        c_ = c;
        score_ = score;
        combo_ = combo;
        maxCombo_ = maxCombo;
        staged_.clear();
        return core::unexpected(kernelError("capability.budget_insufficient", "budget", "/slots",
                                            "the settlement slots are too small"));
    }
    for (std::size_t index = 0; index < staged_.size(); ++index) {
        if (index < slots.size()) {
            slots[index] = staged_[index];
        }
    }
    frontier_ = horizon;
    score_ = computeScore();
    return AdvanceSummary{horizon, processed, staged_.size(), j_, c_, score_};
}

auto AdvanceKernel::advance(Tick horizon) -> core::Result<AdvanceSummary> {
    return advanceInternal(horizon, std::span<SettlementEvent>{}, false);
}

auto AdvanceKernel::advance(Tick horizon, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceSummary> {
    return advanceInternal(horizon, slots, true);
}

auto AdvanceKernel::advanceStopCut(const StopCut& cut, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceSummary> {
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.stop_cut.closed", "receive_closed",
                                            "/stopCut", "a StopCut was already accepted"));
    }
    if (cut.run != run_) {
        return core::unexpected(kernelError("judgement.stop_cut.run_mismatch", "run_mismatch",
                                            "/run", "the StopCut names another run"));
    }
    if (cut.acceptedThrough != acceptedThrough_) {
        return core::unexpected(kernelError("judgement.stop_cut.bound_mismatch", "bound_mismatch",
                                            "/acceptedThrough",
                                            "the StopCut upper bound is not the kernel cursor"));
    }
    if (frontier_ && cut.cutoff < *frontier_) {
        return core::unexpected(kernelError("judgement.stop_cut.cutoff_reversed", "cutoff_reversed",
                                            "/cutoff",
                                            "the Stop cutoff precedes the sealed frontier"));
    }
    if (slots.size() < prepared_.settlementCapacity()) {
        return core::unexpected(kernelError("capability.budget_insufficient", "budget", "/slots",
                                            "the settlement slot count is too small"));
    }
    auto runs = runs_;
    auto units = units_;
    auto pending = pending_;
    auto j = j_;
    auto c = c_;
    auto score = score_;
    auto combo = combo_;
    auto maxCombo = maxCombo_;
    std::vector<SettlementEvent> staged;
    receiveClosed_ = true;
    staged_.clear();
    const auto processed = processThrough(cut.cutoff);
    if (staged_.size() > slots.size()) {
        receiveClosed_ = false;
        runs_ = std::move(runs);
        units_ = std::move(units);
        pending_ = std::move(pending);
        j_ = j;
        c_ = c;
        score_ = score;
        combo_ = combo;
        maxCombo_ = maxCombo;
        staged_.clear();
        return core::unexpected(kernelError("capability.budget_insufficient", "budget", "/slots",
                                            "the settlement slots are too small"));
    }
    for (std::size_t index = 0; index < staged_.size(); ++index) {
        slots[index] = staged_[index];
    }
    frontier_ = cut.cutoff;
    score_ = computeScore();
    return AdvanceSummary{cut.cutoff, processed, staged_.size(), j_, c_, score_};
}

auto AdvanceKernel::requirementIndexFor(const TickRecord& record) const
    -> std::optional<std::size_t> {
    const auto requirements = prepared_.requirements();
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
    std::optional<Tick> end;
    for (const auto& window : requirement.timing.successWindows) {
        if (window.phase.kind == PhaseKind::tail && (!end || window.end > *end)) {
            end = window.end;
        }
    }
    return end.value_or(requirement.deadline);
}

auto AdvanceKernel::gradeIndex(std::size_t requirement, PhaseKind phase, std::int64_t error) const
    -> std::optional<std::uint32_t> {
    const auto& preparedRequirement = prepared_.requirements()[requirement];
    for (const auto& component : preparedRequirement.measure) {
        if (component.phase != phase || !component.gradeTableIndex) {
            continue;
        }
        const auto* score = prepared_.records().normalizedScore;
        if (score == nullptr || *component.gradeTableIndex >= score->gradeTables.size()) {
            return std::nullopt;
        }
        const auto* grade = preparedGradeAt(score->gradeTables[*component.gradeTableIndex], error);
        if (grade == nullptr) {
            return std::nullopt;
        }
        const auto& grades = prepared_.settlementDictionary().grades;
        const auto found = std::find(grades.begin(), grades.end(), *grade);
        if (found == grades.end()) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(found - grades.begin());
    }
    return std::nullopt;
}

auto AdvanceKernel::emit(std::size_t requirement, PhaseKind phase, Tick tick, Outcome outcome,
                         std::optional<std::int64_t> error,
                         std::optional<TimingDirection> direction) -> SettlementEvent {
    SettlementEvent event{};
    event.gradeIndex = std::numeric_limits<std::uint32_t>::max();
    event.tick = tick;
    const auto& phases = prepared_.requirements()[requirement].phases;
    for (std::size_t index = 0; index < phases.size(); ++index) {
        if (phases[index].kind == phase) {
            event.phaseIndex = static_cast<std::uint32_t>(phaseOffset(requirement) + index);
            break;
        }
    }
    event.outcome = outcome;
    event.error = error ? std::optional<TickDelta>{TickDelta{*error}} : std::nullopt;
    event.direction = direction;
    if (error) {
        event.gradeIndex = gradeIndex(requirement, phase, *error)
                               .value_or(std::numeric_limits<std::uint32_t>::max());
    }
    applyFold(event);
    staged_.push_back(event);
    return event;
}

auto AdvanceKernel::computeScore() const -> std::uint32_t {
    const auto* score = prepared_.records().normalizedScore;
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
    const auto* score = prepared_.records().normalizedScore;
    if (score == nullptr) {
        return;
    }
    const auto& phases = prepared_.settlementDictionary().phases;
    if (event.phaseIndex >= phases.size()) {
        return;
    }
    const auto phase = phases[event.phaseIndex].phase;
    std::optional<std::string> grade;
    if (event.gradeIndex != std::numeric_limits<std::uint32_t>::max()) {
        const auto& grades = prepared_.settlementDictionary().grades;
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
    auto& preparedRequirement = prepared_.requirements()[requirement];
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
                emit(requirement, PhaseKind::tap, dispatch, Outcome::hit, error,
                     error < 0    ? std::optional<TimingDirection>{TimingDirection::early}
                     : error == 0 ? std::optional<TimingDirection>{TimingDirection::exact}
                                  : std::optional<TimingDirection>{TimingDirection::late});
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
        emit(requirement, PhaseKind::head, dispatch, Outcome::hit, std::nullopt, std::nullopt);
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
            emit(requirement, PhaseKind::body, dispatch, Outcome::miss, error,
                 error < 0    ? std::optional<TimingDirection>{TimingDirection::early}
                 : error == 0 ? std::optional<TimingDirection>{TimingDirection::exact}
                              : std::optional<TimingDirection>{TimingDirection::late});
            bodyMissed = true;
        } else {
            emit(requirement, PhaseKind::body, dispatch, Outcome::hit, std::nullopt, std::nullopt);
        }
        run.stage = RequirementRun::Stage::tail;
    }
    if (bodyMissed) {
        emit(requirement, PhaseKind::tail, dispatch, Outcome::miss, std::nullopt, std::nullopt);
        run.stage = RequirementRun::Stage::done;
        return;
    }
    const auto tailTarget = targetOf(preparedRequirement, PhaseKind::tail);
    const auto tailEnd = lastTailEnd(preparedRequirement);
    if (inWindow(preparedRequirement, PhaseKind::tail, observation)) {
        const auto error = observation.value() - tailTarget.value();
        emit(requirement, PhaseKind::tail, dispatch, Outcome::hit, error,
             error < 0    ? std::optional<TimingDirection>{TimingDirection::early}
             : error == 0 ? std::optional<TimingDirection>{TimingDirection::exact}
                          : std::optional<TimingDirection>{TimingDirection::late});
    } else if (observation < tailEnd) {
        const auto error = observation.value() - tailTarget.value();
        emit(requirement, PhaseKind::tail, dispatch, Outcome::miss, error,
             error < 0    ? std::optional<TimingDirection>{TimingDirection::early}
             : error == 0 ? std::optional<TimingDirection>{TimingDirection::exact}
                          : std::optional<TimingDirection>{TimingDirection::late});
    } else {
        emit(requirement, PhaseKind::tail, tailEnd, Outcome::miss, std::nullopt, std::nullopt);
    }
    run.stage = RequirementRun::Stage::done;
}

void AdvanceKernel::settleTimers(Tick horizon) {
    const auto requirements = prepared_.requirements();
    for (std::size_t index = 0; index < runs_.size(); ++index) {
        auto& run = runs_[index];
        const auto& requirement = requirements[index];
        if (run.stage == RequirementRun::Stage::body && requirement.timing.body &&
            horizon >= requirement.timing.body->end) {
            emit(index, PhaseKind::body, requirement.timing.body->end, Outcome::hit, std::nullopt,
                 std::nullopt);
            run.stage = RequirementRun::Stage::tail;
        }
        if (run.stage == RequirementRun::Stage::tail && horizon >= lastTailEnd(requirement)) {
            emit(index, PhaseKind::tail, lastTailEnd(requirement), Outcome::miss, std::nullopt,
                 std::nullopt);
            run.stage = RequirementRun::Stage::done;
        }
    }
}

} // namespace cuexis::judgement::runtime
