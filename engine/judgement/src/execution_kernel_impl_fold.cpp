#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

void ExecutionKernel::Impl::validatePhaseLayout(const RuntimeState& d) const {
    std::set<std::pair<RequirementIdentity, PhaseKind>> phases;
    for (const auto& p : d.projection.phases) {
        if (!phases.emplace(p.requirement, p.phase).second) {
            throw std::logic_error{"duplicate phase draft"};
        }
    }
}

void ExecutionKernel::Impl::validateDraft(const RuntimeState& d, bool phaseLayout) const {
    if (phaseLayout)
        validatePhaseLayout(d);
    std::set<std::tuple<std::string, std::string, std::string>> channels;
    for (const auto& c : d.projection.contacts) {
        if (!channels.emplace(c.domainToken, c.sourceClass, c.channelToken).second) {
            throw std::logic_error{"duplicate physical contact"};
        }
    }
    for (std::size_t i = 0; i < d.projection.ownership.size(); ++i) {
        const auto& o = d.projection.ownership[i];
        if (std::find(d.projection.contacts.begin(), d.projection.contacts.end(), o.contact) ==
            d.projection.contacts.end()) {
            throw std::logic_error{"ownership without physical contact"};
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (d.projection.ownership[j].contact == o.contact ||
                d.projection.ownership[j].requirement == o.requirement) {
                throw std::logic_error{"duplicate ownership"};
            }
        }
        if (o.lease) {
            const auto r = std::find_if(
                d.projection.resources.begin(), d.projection.resources.end(),
                [&](const auto& r) { return r.resourceId == o.lease->slot.resourceId; });
            if (r == d.projection.resources.end()) {
                throw std::logic_error{"missing owned resource"};
            }
            const auto* held = std::get_if<Held>(&r->state);
            if (!held || held->lease != *o.lease || held->contact != o.contact) {
                throw std::logic_error{"lease state mismatch"};
            }
        }
    }
    for (const auto& r : d.projection.resources) {
        if (const auto* held = std::get_if<Held>(&r.state)) {
            if (std::count_if(d.projection.ownership.begin(), d.projection.ownership.end(),
                              [&](const auto& o) { return o.lease && *o.lease == held->lease; }) !=
                1) {
                throw std::logic_error{"unowned held resource"};
            }
        }
    }
}

auto ExecutionKernel::Impl::idleDeadlineEnd(const RuntimeState& d, Tick tick,
                                            std::size_t& end) const -> bool {
    if (!pending.empty() || !d.projection.contacts.empty() || !d.projection.ownership.empty() ||
        (configuration.ruleset && !configuration.ruleset->declaration().hooks.empty()))
        return false;
    if (testControls &&
        (testControls->forceReferenceAdvance || testControls->failedTick == tick ||
         testControls->duplicatePhaseTick == tick || testControls->invalidRegisterTick == tick ||
         std::find(testControls->signalTicks.begin(), testControls->signalTicks.end(), tick) !=
             testControls->signalTicks.end()))
        return false;
    const auto timers = prepared.timers();
    end = d.timerCursor;
    while (end < timers.size() && timers[end].tick == tick) {
        if (timers[end].kind != PreparedTimerKind::hardDeadline)
            return false;
        const auto i = preparation->timerRequirements[end];
        for (auto p = preparation->phaseOffsets[i]; p < preparation->phaseOffsets[i + 1]; ++p)
            if (d.projection.phases[p].state == KernelPhaseState::Pending)
                return false;
        ++end;
    }
    return end != d.timerCursor;
}

void ExecutionKernel::Impl::sealIdleDeadline(RuntimeState& d, Tick tick,
                                             std::size_t cursor) noexcept {
    d.timerCursor = cursor;
    d.projection.processedFrontier = tick;
    d.projection.kernelWorkTick = tick;
    d.projection.sealedFactCursor = static_cast<std::uint64_t>(d.projection.facts.size());
    if (d.projection.rpa) {
        d.projection.rpa->foldBaseTick = tick;
        if (d.projection.fold)
            d.projection.rpa->foldAtTickStart->workTick = d.projection.fold->workTick;
    }
    if (d.projection.fold)
        d.projection.fold->workTick = tick;
}

void ExecutionKernel::Impl::fold(RuntimeState& d, Tick tick) const {
    if (!configuration.ruleset) {
        return;
    }
    auto& v = *d.projection.fold;
    const auto& config = configuration.ruleset->declaration();
    while (v.hookConsumerCursor < v.produced.size() &&
           v.produced[static_cast<std::size_t>(v.hookConsumerCursor)].visibleTick <= tick) {
        v.bonus = v.produced[static_cast<std::size_t>(v.hookConsumerCursor++)].value;
    }
    const auto increment = [](std::uint64_t& x) {
        if (x == UINT64_MAX) {
            throw std::overflow_error{"Fold count overflow"};
        }
        ++x;
    };
    bool phaseFact = false;
    std::uint64_t combined = 0;
    std::vector<RegisterContribution> hookWrites;
    for (std::size_t i = static_cast<std::size_t>(v.factCursor); i < d.projection.facts.size();
         ++i) {
        const auto* f = std::get_if<PhaseOutcomeFact>(&d.projection.facts[i]);
        if (!f) {
            const auto& receipt = std::get<ReceiptFact>(d.projection.facts[i]);
            increment(receipt.kind == FactKind::stray ? v.strayCount : v.consumeEmptyCount);
            continue;
        }
        phaseFact = true;
        const auto rule =
            std::find_if(config.score.rules.begin(), config.score.rules.end(), [&](const auto& r) {
                return r.phase == f->phase && r.outcome == f->outcome && r.grade == f->grade;
            });
        if (rule == config.score.rules.end()) {
            throw std::logic_error{"score mapping missing"};
        }
        const auto count = std::find_if(v.counts.begin(), v.counts.end(), [&](const auto& row) {
            return row.phase == f->phase && row.outcome == f->outcome && row.grade == f->grade;
        });
        if (count == v.counts.end()) {
            throw std::logic_error{"statistics mapping missing"};
        }
        increment(count->count);
        auto score = checkedScore(v.score, rule->delta, config.score);
        if (!score) {
            throw std::overflow_error{"score checked failure"};
        }
        v.score = *score;
        if (f->outcome == Outcome::hit && v.bonus) {
            const auto available =
                static_cast<std::uint64_t>(INT64_MAX) - static_cast<std::uint64_t>(v.score);
            if (v.bonus > available) {
                if (config.score.arithmetic != ArithmeticPolicy::clamp) {
                    throw std::overflow_error{"bonus score overflow"};
                }
                v.score = config.score.maximum;
            } else {
                const auto sum =
                    std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(v.score) + v.bonus);
                score = checkedScore(sum, 0, config.score);
                if (!score) {
                    throw std::overflow_error{"bonus checked failure"};
                }
                v.score = *score;
            }
        }
        if (rule->incrementsCombo) {
            increment(v.combo);
        } else {
            v.combo = 0;
        }
        v.maxCombo = std::max(v.maxCombo, v.combo);
        if (f->outcome == Outcome::hit) {
            increment(v.hits);
        } else {
            increment(v.misses);
        }
        for (const auto& h : config.hooks) {
            hookWrites.push_back({h.target, "hook", f->factId.commitId, f->factId.localOrdinal, 0,
                                  RegisterValue{h.value}});
        }
    }
    const auto fault = testControls && testControls->invalidRegisterTick == tick
                           ? testControls->registerFault
                           : RegisterFault::none;
    std::vector<RegisterContribution> scoreWrites{
        {"score", "score", 0, 0, 0, RegisterValue{v.score}}};
    if (fault == RegisterFault::secondExclusive) {
        scoreWrites.push_back(scoreWrites.front());
    }
    const RegisterDeclaration scoreRegister{
        "score", RegisterKind::exclusive, RegisterValueTag::signed64, "score",
        {},      CombineOperator::maximum};
    auto scoreCommit = commitRegister(scoreRegister, scoreWrites, RegisterValue{v.score});
    if (!scoreCommit) {
        throw std::logic_error{"exclusive score write invalid"};
    }
    if (!config.hooks.empty()) {
        if (fault == RegisterFault::duplicateContribution && !hookWrites.empty()) {
            hookWrites.push_back(hookWrites.front());
        }
        const auto& h = config.hooks.front();
        const RegisterDeclaration hookRegister{h.target,
                                               RegisterKind::commutativeMonoid,
                                               RegisterValueTag::unsigned64,
                                               "",
                                               h.contributors,
                                               h.combine};
        auto hookCommit = commitRegister(hookRegister, hookWrites, RegisterValue{std::uint64_t{0}});
        if (!hookCommit) {
            throw std::logic_error{"monoid Hook contribution invalid"};
        }
        combined = std::get<std::uint64_t>(*hookCommit);
    }
    const RegisterDeclaration ledgerRegister{
        "fact.count", RegisterKind::ledgerDerived, RegisterValueTag::unsigned64, "",
        {},           CombineOperator::maximum};
    std::vector<RegisterContribution> direct;
    if (fault == RegisterFault::directDerived) {
        direct.push_back({"fact.count", "statistics", 0, 0, 0, RegisterValue{std::uint64_t{0}}});
    }
    auto derived =
        commitRegister(ledgerRegister, direct,
                       RegisterValue{static_cast<std::uint64_t>(d.projection.facts.size())});
    if (!derived) {
        throw std::logic_error{"ledger-derived direct write invalid"};
    }
    v.factCursor = static_cast<std::uint64_t>(d.projection.facts.size());
    v.ledgerDerivedCount = v.factCursor;
    v.workTick = tick;
    if (phaseFact && !config.hooks.empty()) {
        const auto visible = offsetTicks(tick, TickSpan{1});
        if (!visible) {
            throw std::overflow_error{"Hook visibility overflow"};
        }
        v.monoidValue = config.hooks.front().combine == CombineOperator::maximum
                            ? std::max(v.monoidValue, combined)
                            : v.monoidValue | combined;
        v.produced.push_back({config.hooks.front().target, *visible, combined});
    }
}

void ExecutionKernel::Impl::ledger(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                   Tick tick) const {
    const auto rank = [](const auto& v) {
        return v.phase ? static_cast<unsigned>(*v.phase) + 1 : 0U;
    };
    std::sort(facts.begin(), facts.end(), [&](const auto& a, const auto& b) {
        const auto aRank = rank(a);
        const auto bRank = rank(b);
        return std::tie(a.origin, aRank, a.kind, a.requirement) <
               std::tie(b.origin, bRank, b.kind, b.requirement);
    });
    std::optional<OriginId> previous;
    std::uint64_t commit = 0, local = 0;
    for (const auto& f : facts) {
        if (!previous || *previous != f.origin) {
            if (d.idsExhausted) {
                throw std::overflow_error{"commit ID exhausted"};
            }
            commit = d.nextCommitId;
            if (commit == UINT64_MAX) {
                d.idsExhausted = true;
            } else {
                ++d.nextCommitId;
            }
            previous = f.origin;
            local = 0;
        }
        LogicalCanonicalOrdinal ordinal{f.origin, 0, local, static_cast<std::uint8_t>(rank(f)),
                                        f.kind};
        if (f.phase) {
            std::optional<std::string> grade;
            if (f.error) {
                const auto r =
                    std::find_if(graph().requirements.begin(), graph().requirements.end(),
                                 [&](const auto& r) { return r.identity == *f.requirement; });
                const auto c =
                    std::find_if(r->measure.components.begin(), r->measure.components.end(),
                                 [&](const auto& c) { return c.phase == *f.phase; });
                if (c != r->measure.components.end() && c->gradeTable) {
                    const auto row = std::find_if(c->gradeTable->begin(), c->gradeTable->end(),
                                                  [&](const auto& row) {
                                                      return f.error->value() >= row.minimum &&
                                                             f.error->value() <= row.maximum;
                                                  });
                    if (row == c->gradeTable->end()) {
                        throw std::logic_error{"grade outside compiled table"};
                    }
                    grade = row->grade;
                }
            }
            d.projection.facts.emplace_back(PhaseOutcomeFact{*f.requirement,
                                                             *f.phase,
                                                             f.outcome,
                                                             factCategoryOfPhase(*f.phase),
                                                             f.error,
                                                             f.evidence,
                                                             f.origin,
                                                             commit,
                                                             {commit, local},
                                                             tick,
                                                             ordinal,
                                                             std::move(grade)});
        } else {
            d.projection.facts.emplace_back(
                ReceiptFact{f.kind, *f.evidence, f.origin, commit, {commit, local}, tick, ordinal});
        }
        if (local == UINT64_MAX) {
            throw std::overflow_error{"local ordinal exhausted"};
        }
        ++local;
    }
}

} // namespace cuexis::judgement::detail
