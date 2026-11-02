#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

auto ExecutionKernel::snapshot() const -> core::Result<std::shared_ptr<const SnapshotStorage>> {
    if (impl_->active->projection.state != KernelSessionState::Prepared) {
        return core::unexpected(
            error("session.lifecycle_order", "faulted session cannot create Snapshot"));
    }
    return captureState();
}
auto ExecutionKernel::captureState() const
    -> core::Result<std::shared_ptr<const SnapshotStorage>> try {
    SnapshotDTO dto{impl_->active->projection,
                    {},
                    impl_->active->coverageHistory,
                    {},
                    static_cast<std::uint64_t>(impl_->active->timerCursor),
                    impl_->active->nextCommitId,
                    impl_->active->idsExhausted,
                    {{},
                     impl_->ingress.lastObservedTick_,
                     impl_->ingress.nextObservationId_,
                     impl_->ingress.observationIdsExhausted_},
                    impl_->pending};
    if (dto.kernel.rpa) {
        // Recovery replays the applied journal, including a synthetic partial advance.
        // archive() may still expose the complete Seek source with an unapplied suffix.
        dto.rpaHistory = flattenLog(impl_->logTail);
    }
    dto.kernel.runScope.reset();
    for (const auto& m : impl_->active->matchers) {
        MatcherSnapshot v{{}, m.epsilonAttempted};
        for (const auto state : m.states) {
            v.states.push_back(static_cast<std::uint64_t>(state));
        }
        dto.matchers.push_back(std::move(v));
    }
    for (const auto i : impl_->active->activeRequirements) {
        dto.activeRequirements.push_back(static_cast<std::uint64_t>(i));
    }
    if (dto.kernel.rpa) {
        for (std::size_t i = 0; i < dto.kernel.rpa->accepted.size(); ++i)
            dto.ingress.accepted.push_back(
                {dto.kernel.rpa->accepted[i], IngressSequence{static_cast<std::uint64_t>(i)}});
    }
    for (std::size_t i = 0; i < impl_->ingress.admittedSubjects_.size(); ++i) {
        const auto& v = impl_->ingress.admittedSubjects_[i];
        dto.ingress.accepted.push_back(
            {{v.observationTick, std::string{v.domainToken}, std::string{v.sourceClass.token()},
              std::string{v.channel.token()}, v.action,
              v.hasAmount ? std::optional{v.amountCanonicalInteger} : std::nullopt},
             impl_->ingress.admittedSequences_[i]});
    }
    return std::make_shared<const SnapshotStorage>(std::move(dto), impl_->recoveryInputs);
} catch (const std::exception&) {
    return core::unexpected(error("execution.relation_invalid", "Snapshot allocation failed"));
}
auto ExecutionKernel::restore(const SnapshotStorage& saved, const RecoveryInputs& inputs)
    -> core::Result<std::unique_ptr<ExecutionKernel>> try {
    auto initialCandidate = prepare(inputs.configuration, inputs.prepared);
    if (!initialCandidate) {
        return core::unexpected(initialCandidate.error());
    }
    auto& k = **initialCandidate;
    const auto& dto = saved.dto;
    if (dto.kernel.rpa) {
        // The initial candidate already owns fresh matchers, indices and fault reserve.
        // Reuse it for validation replay; no state has been applied to it yet.
        auto& candidate = initialCandidate;
        if (!(*candidate)->query()->projection.rpa)
            return core::unexpected(
                error("execution.relation_invalid", "RPA snapshot profile mismatch"));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        // Includes replay, full DTO materialization and full comparison, excludes prepare.
        rpaObservation::SeekWorkTimer timer{SeekPhase::s3Validation,
                                            &SeekWork::rpaRestoreValidationUs};
#endif
        for (const auto& record : dto.rpaHistory) {
            if (record.result && record.result->state == KernelSessionState::Faulted) {
                KernelTestControls controls{};
                controls.failedTick = record.result->failedTick;
                controls.failurePoint = record.result->faultStage == FaultStage::fold
                                            ? KernelFailurePoint::beforeFold
                                            : KernelFailurePoint::beforeSeal;
                // A finalization fault has no failed Tick: replay its last sealed prefix and
                // fail the horizon commit, then compare every diagnostic/progress field below.
                controls.failAdvanceFinalization =
                    record.horizon && record.result->faultStage == FaultStage::control &&
                    !record.result->failedTick;
                auto injected = (*candidate)->inject(controls);
                if (!injected)
                    return core::unexpected(injected.error());
            }
            if (record.control) {
                auto result = (*candidate)->controlRpa(*record.control);
                if (!result &&
                    (*candidate)->query()->projection.state != KernelSessionState::Faulted)
                    return core::unexpected(result.error());
            } else if (record.horizon) {
                auto result = (*candidate)->advance(*record.horizon, record.controlDrain);
                if (!result &&
                    (*candidate)->query()->projection.state != KernelSessionState::Faulted)
                    return core::unexpected(result.error());
            } else {
                auto result = (*candidate)->submitCanonical(record.batch);
                if (record.rejection
                        ? result.has_value() || !sameRecoveryError(result.error(), record.rejection)
                        : !result || *result != record.admission)
                    return core::unexpected(
                        error("execution.relation_invalid", "RPA snapshot admission mismatch"));
            }
            if (record.result &&
                !sameKernelResult((*candidate)->query()->projection, *record.result))
                return core::unexpected(
                    error("execution.relation_invalid", "RPA snapshot result mismatch"));
        }
        auto actual = (*candidate)->captureState();
        if (!actual || !sameSnapshot((*actual)->dto, dto))
            return core::unexpected(
                error("execution.relation_invalid", "RPA snapshot state unreachable"));
        return std::move(candidate);
    }

    const auto& p = dto.kernel;
    const auto& initial = k.impl_->active->projection;
    if (p.judgementIdentity != initial.judgementIdentity ||
        saved.inputs->prepared.assembled().prepared != inputs.prepared.assembled().prepared) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot dependency identity mismatch"));
    }
    if ((p.state != KernelSessionState::Prepared && p.state != KernelSessionState::Faulted) ||
        (p.state == KernelSessionState::Faulted) != p.faultDiagnostic.has_value() ||
        p.phases.size() != initial.phases.size() ||
        p.resources.size() != initial.resources.size() ||
        dto.matchers.size() != inputs.prepared.requirements().size() ||
        dto.timerCursor > inputs.prepared.timers().size() || p.sealedFactCursor != p.facts.size() ||
        p.fold.has_value() != inputs.configuration.ruleset.has_value()) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot state closure invalid"));
    }
    if (p.faultDiagnostic && containsLegacyDiagnostic(*p.faultDiagnostic))
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot diagnostic dialect invalid"));
    if (p.state == KernelSessionState::Prepared) {
        if (p.faultStage || p.failedTick || p.requestedHorizon) {
            return core::unexpected(
                error("execution.relation_invalid", "healthy Snapshot has fault progress"));
        }
    } else {
        if (!p.faultStage || !p.requestedHorizon || p.faultDiagnostic->message().empty()) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot fault schema incomplete"));
        }
        const auto expected = *p.faultStage == FaultStage::fold
                                  ? "ruleset.transaction_failed"
                                  : "judgement.kernel.transaction_failed";
        if (p.faultDiagnostic->code() != expected || p.faultDiagnostic->cause() ||
            p.faultDiagnostic->context().size() != 3) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot fault diagnostic invalid"));
        }
        const auto& context = p.faultDiagnostic->context();
        if (context[0].key != "category" || context[0].value != "invalid_relation" ||
            context[1].key != "severity" || context[1].value != "error" ||
            context[2].key != "faulted" || context[2].value != "true" ||
            (*p.faultStage == FaultStage::control ? p.failedTick.has_value()
                                                  : !p.failedTick.has_value())) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot fault context/progress invalid"));
        }
        if (*p.faultStage == FaultStage::fold &&
            (!p.fold || p.kernelWorkTick != p.failedTick || p.processedFrontier != p.failedTick)) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot Fold fault boundary invalid"));
        }
        if (*p.faultStage == FaultStage::kernel) {
            const auto frontier = p.failedTick->value() == INT64_MIN
                                      ? std::nullopt
                                      : std::optional{Tick{p.failedTick->value() - 1}};
            if (p.processedFrontier != frontier ||
                (p.kernelWorkTick && *p.kernelWorkTick >= *p.failedTick)) {
                return core::unexpected(
                    error("execution.relation_invalid", "Snapshot kernel fault boundary invalid"));
            }
        }
    }
    for (std::size_t i = 0; i < p.phases.size(); ++i) {
        if (p.phases[i].requirement != initial.phases[i].requirement ||
            p.phases[i].phase != initial.phases[i].phase ||
            p.phases[i].state > KernelPhaseState::Expired) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot phase reference invalid"));
        }
    }
    for (std::size_t i = 0; i < p.resources.size(); ++i) {
        if (p.resources[i].resourceId != initial.resources[i].resourceId) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot resource reference invalid"));
        }
    }
    if (p.fold &&
        (p.fold->factCursor > p.facts.size() ||
         p.fold->hookConsumerCursor > p.fold->produced.size() ||
         p.fold->ledgerDerivedCount != p.fold->factCursor || p.fold->maxCombo < p.fold->combo ||
         (p.state == KernelSessionState::Prepared && p.fold->factCursor != p.facts.size()))) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot Fold prefix invalid"));
    }
    if (p.effectiveFactSemanticRevision != initial.effectiveFactSemanticRevision ||
        (p.kernelWorkTick && (!p.processedFrontier || *p.kernelWorkTick > *p.processedFrontier))) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot revision or work frontier invalid"));
    }
    const auto timers = inputs.prepared.timers();
    for (std::size_t i = 0; i < timers.size(); ++i) {
        if ((i < dto.timerCursor) !=
            (p.processedFrontier && timers[i].tick <= *p.processedFrontier)) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot timer frontier invalid"));
        }
    }
    std::optional<std::tuple<Tick, OriginId, std::uint8_t, FactKind, RequirementIdentity>> lastFact;
    std::optional<FactId> lastId;
    for (const auto& fact : p.facts) {
        bool valid = true;
        std::visit(
            [&](const auto& f) {
                RequirementIdentity requirement;
                if constexpr (std::is_same_v<std::decay_t<decltype(f)>, PhaseOutcomeFact>) {
                    requirement = f.requirement;
                    const auto r =
                        std::find_if(inputs.prepared.assembled().graph.requirements.begin(),
                                     inputs.prepared.assembled().graph.requirements.end(),
                                     [&](const auto& r) { return r.identity == f.requirement; });
                    valid = f.outcome <= Outcome::miss &&
                            r != inputs.prepared.assembled().graph.requirements.end() &&
                            f.category == factCategoryOfPhase(f.phase) &&
                            f.canonicalOrdinal.factKind == FactKind::phaseOutcome &&
                            f.canonicalOrdinal.phaseRank == static_cast<unsigned>(f.phase) + 1 &&
                            std::any_of(p.phases.begin(), p.phases.end(), [&](const auto& row) {
                                return row.requirement == f.requirement && row.phase == f.phase &&
                                       row.state == (f.outcome == Outcome::hit
                                                         ? KernelPhaseState::Hit
                                                         : KernelPhaseState::Miss);
                            });
                    std::optional<std::string> grade;
                    if (valid && f.error) {
                        const auto target = std::find_if(
                            r->timing->phaseTargets.begin(), r->timing->phaseTargets.end(),
                            [&](const auto& t) { return t.phase == f.phase; });
                        if (!f.evidence || target == r->timing->phaseTargets.end()) {
                            valid = false;
                        } else {
                            auto delta = differenceTicks(f.evidence->observationTick.tick(),
                                                         target->chartTick);
                            valid = delta && *delta == *f.error;
                        }
                        const auto component =
                            std::find_if(r->measure.components.begin(), r->measure.components.end(),
                                         [&](const auto& c) { return c.phase == f.phase; });
                        if (component != r->measure.components.end() && component->gradeTable) {
                            const auto row =
                                std::find_if(component->gradeTable->begin(),
                                             component->gradeTable->end(), [&](const auto& row) {
                                                 return f.error->value() >= row.minimum &&
                                                        f.error->value() <= row.maximum;
                                             });
                            if (row == component->gradeTable->end()) {
                                valid = false;
                            } else {
                                grade = row->grade;
                            }
                        }
                    }
                    valid = valid && f.grade == grade;
                    if (const auto* origin = std::get_if<ObservationOrigin>(&f.originId)) {
                        valid = valid && origin->dispatchTick == f.commitTick &&
                                (!f.evidence || f.evidence == origin->observationKey);
                    } else if (const auto* timerOrigin = std::get_if<TimerOrigin>(&f.originId)) {
                        valid = valid && timerOrigin->tick == f.commitTick &&
                                timerOrigin->requirement == f.requirement && !f.evidence &&
                                !f.error;
                    } else {
                        valid = false;
                    }
                } else {
                    const auto* origin = std::get_if<CoordinationOrigin>(&f.originId);
                    valid = origin && origin->dispatchTick == f.commitTick &&
                            origin->observationKey == f.observationKey &&
                            f.kind != FactKind::phaseOutcome &&
                            f.canonicalOrdinal.factKind == f.kind &&
                            f.canonicalOrdinal.phaseRank == 0;
                }
                valid = valid && f.canonicalOrdinal.originOrdinal == 0;
                const auto key = std::tuple{f.commitTick, f.originId, f.canonicalOrdinal.phaseRank,
                                            f.canonicalOrdinal.factKind, requirement};
                valid = valid && (!lastFact || *lastFact < key) &&
                        f.factId.commitId == f.commitId &&
                        f.canonicalOrdinal.originScope == f.originId &&
                        f.canonicalOrdinal.localOrdinal == f.factId.localOrdinal &&
                        (!lastId || f.factId.commitId > lastId->commitId ||
                         (f.factId.commitId == lastId->commitId &&
                          f.factId.localOrdinal > lastId->localOrdinal)) &&
                        (dto.commitIdsExhausted || f.commitId < dto.nextCommitId) &&
                        p.processedFrontier && f.commitTick <= *p.processedFrontier;
                lastFact = key;
                lastId = f.factId;
            },
            fact);
        if (!valid) {
            return core::unexpected(error("execution.relation_invalid",
                                          "Snapshot Fact identity/order/reference invalid"));
        }
    }
    if (p.fold) {
        const auto& config = inputs.configuration.ruleset->declaration().score;
        if (p.fold->score < config.minimum || p.fold->score > config.maximum) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot Score outside declared range"));
        }
        RuntimeState rebuilt{initial, {}};
        auto rebuiltInitial = inputs.configuration.ruleset->initialState();
        if (!rebuiltInitial) {
            return core::unexpected(rebuiltInitial.error());
        }
        rebuilt.projection.fold = std::move(*rebuiltInitial);
        std::set<Tick> work;
        if (p.fold->workTick) {
            for (std::size_t i = 0; i < dto.timerCursor; ++i) {
                if (timers[i].tick <= *p.fold->workTick) {
                    work.insert(timers[i].tick);
                }
            }
            for (const auto& r : p.receipts) {
                if (r.dispatchTick <= *p.fold->workTick) {
                    work.insert(r.dispatchTick);
                }
            }
            for (std::uint64_t i = 0; i < p.fold->factCursor; ++i) {
                std::visit([&](const auto& f) { work.insert(f.commitTick); },
                           p.facts[static_cast<std::size_t>(i)]);
            }
        }
        std::size_t cursor = 0;
        while (!work.empty()) {
            const auto tick = *work.begin();
            work.erase(work.begin());
            if (!p.fold->workTick || tick > *p.fold->workTick) {
                break;
            }
            while (
                cursor < p.fold->factCursor &&
                std::visit([&](const auto& f) { return f.commitTick <= tick; }, p.facts[cursor])) {
                rebuilt.projection.facts.push_back(p.facts[cursor++]);
            }
            k.impl_->fold(rebuilt, tick);
            const auto& fold = *rebuilt.projection.fold;
            if (fold.hookConsumerCursor < fold.produced.size()) {
                work.insert(
                    fold.produced[static_cast<std::size_t>(fold.hookConsumerCursor)].visibleTick);
            }
        }
        if (*rebuilt.projection.fold != *p.fold) {
            return core::unexpected(
                error("execution.relation_invalid",
                      "Snapshot Fold state does not match successful Fact prefix"));
        }
    }
    std::vector<HeadContactOwnership> expectedCoverage;
    for (const auto& fact : p.facts) {
        const auto* head = std::get_if<PhaseOutcomeFact>(&fact);
        if (!head || head->phase != PhaseKind::head || head->outcome != Outcome::hit) {
            continue;
        }
        if (!head->evidence || head->evidence->action != InputAction::press) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot head evidence invalid"));
        }
        const auto& graph = inputs.prepared.assembled().graph;
        const auto requirement =
            std::find_if(graph.requirements.begin(), graph.requirements.end(),
                         [&](const auto& r) { return r.identity == head->requirement; });
        std::optional<LeaseIdentity> lease;
        if (!requirement->resourceClaims.empty()) {
            const auto& claim = requirement->resourceClaims.front();
            const auto resource =
                std::find_if(graph.resources.begin(), graph.resources.end(),
                             [&](const auto& r) { return r.ref == claim.resourceRef; });
            lease = LeaseIdentity{{claim.resourceRef.resourceId, resource->slotToken, 0},
                                  head->requirement,
                                  {head->requirement, PhaseKind::head, head->originId,
                                   claim.resourceRef.resourceId, claim.intent}};
        }
        const auto& key = *head->evidence;
        std::optional<CanonicalObservationKey> startPress;
        for (const auto& receipt : p.receipts) {
            const auto& observed = receipt.observationKey;
            if (observed.domainToken != key.domainToken ||
                observed.sourceClass != key.sourceClass ||
                observed.channelToken != key.channelToken) {
                continue;
            }
            if (observed.action == InputAction::press && !startPress) {
                startPress = observed;
            }
            if (observed == key) {
                break;
            }
            if (observed.action == InputAction::release) {
                startPress.reset();
            }
        }
        if (!startPress) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot contact history invalid"));
        }
        expectedCoverage.push_back(
            {head->requirement,
             {key.domainToken, key.sourceClass, key.channelToken, *startPress},
             head->commitTick,
             lease});
    }
    if (expectedCoverage != dto.coverageHistory) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot coverage closure invalid"));
    }
    std::vector<std::uint64_t> expectedActive;
    for (std::size_t i = 0; i < inputs.prepared.assembled().graph.requirements.size(); ++i) {
        const auto& identity = inputs.prepared.assembled().graph.requirements[i].identity;
        if (std::any_of(timers.begin(),
                        timers.begin() + static_cast<std::ptrdiff_t>(dto.timerCursor),
                        [&](const auto& timer) {
                            return timer.kind == PreparedTimerKind::activation &&
                                   timer.requirement == identity;
                        })) {
            expectedActive.push_back(static_cast<std::uint64_t>(i));
        }
    }
    if (dto.activeRequirements != expectedActive) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot activation closure invalid"));
    }
    SharedSequence<MatcherState> matchers;
    for (std::size_t i = 0; i < dto.matchers.size(); ++i) {
        const auto& program = *inputs.prepared.requirements()[i].executionProgram;
        if (dto.matchers[i].states.empty() ||
            !std::is_sorted(dto.matchers[i].states.begin(), dto.matchers[i].states.end()) ||
            std::adjacent_find(dto.matchers[i].states.begin(), dto.matchers[i].states.end()) !=
                dto.matchers[i].states.end() ||
            dto.matchers[i].epsilonAttempted !=
                (program.accepting[program.start] != 0 &&
                 std::find(expectedActive.begin(), expectedActive.end(), i) !=
                     expectedActive.end())) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot matcher closure invalid"));
        }
        MatcherState m{{}, dto.matchers[i].epsilonAttempted};
        for (const auto v : dto.matchers[i].states) {
            if (v >= program.accepting.size()) {
                return core::unexpected(
                    error("execution.relation_invalid", "Snapshot matcher state invalid"));
            }
            m.states.push_back(static_cast<std::size_t>(v));
        }
        matchers.push_back(std::move(m));
    }
    auto state = std::make_shared<RuntimeState>(p, std::move(matchers));
    state->projection.runScope = std::make_shared<const std::uint8_t>(0);
    state->coverageHistory = dto.coverageHistory;
    std::set<std::uint64_t> active;
    for (const auto i : dto.activeRequirements) {
        if (i >= inputs.prepared.requirements().size() || !active.insert(i).second) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot active instance invalid"));
        }
        state->activeRequirements.push_back(static_cast<std::size_t>(i));
    }
    state->timerCursor = static_cast<std::size_t>(dto.timerCursor);
    state->nextCommitId = dto.nextCommitId;
    state->idsExhausted = dto.commitIdsExhausted;
    k.impl_->validateDraft(*state);
    std::vector<ClockedIngress> raw;
    std::vector<std::optional<std::int64_t>> amounts;
    const auto mapping = inputs.configuration.inputMapping.view();
    std::optional<ObservationTick> last;
    for (const auto& v : dto.ingress.accepted) {
        if (v.key.sourceClass != mapping.sourceClass.token()) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot source mismatch"));
        }
        const auto channel = ChannelRef::fromToken(v.key.channelToken);
        if (!channel) {
            return core::unexpected(channel.error());
        }
        raw.push_back({v.key.observationTick,
                       {v.sequence,
                        v.key.action,
                        *channel,
                        v.key.domainToken,
                        {},
                        {false, false, false},
                        ContinuityKind::discrete,
                        {}}});
        amounts.push_back(v.key.amount);
        if (!last || v.key.observationTick > *last) {
            last = v.key.observationTick;
        }
    }
    if (last != dto.ingress.lastObservedTick || dto.ingress.exhausted ||
        dto.ingress.nextId != dto.ingress.accepted.size()) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot ingress counters invalid"));
    }
    auto journal = prepareIngressBatch(k.impl_->ingress, mapping, raw, true, amounts);
    if (!journal) {
        return core::unexpected(journal.error());
    }
    if (auto reserve = reserveIngressBatch(k.impl_->ingress, *journal); !reserve) {
        return core::unexpected(reserve.error());
    }
    commitIngressBatch(k.impl_->ingress, std::move(*journal));
    k.impl_->ingress.nextObservationId_ = dto.ingress.nextId;
    k.impl_->ingress.observationIdsExhausted_ = dto.ingress.exhausted;
    std::set<Tick> dispatches;
    for (const auto& v : dto.pending) {
        auto route =
            routeExecutionObservation(inputs.configuration.latePolicy,
                                      v.observationKey.observationTick.tick(), v.admissionHorizon);
        std::optional<Tick> admissionWatermark;
        if (v.admissionHorizon) {
            auto frontier = offsetTicks(
                *v.admissionHorizon,
                TickSpan{-inputs.configuration.latePolicy.windowCloseThreshold.measuredValue()
                              ->value()});
            if (!frontier) {
                return core::unexpected(frontier.error());
            }
            admissionWatermark = *frontier;
        }
        if (v.admissionFrontier != admissionWatermark || !route ||
            route->dispatchTick != v.dispatchTick || route->wasForwarded != v.wasForwarded ||
            !dispatches.insert(v.dispatchTick).second ||
            (p.processedFrontier && v.dispatchTick <= *p.processedFrontier)) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot pending route invalid"));
        }
        if (std::none_of(dto.ingress.accepted.begin(), dto.ingress.accepted.end(),
                         [&](const auto& input) { return input.key == v.observationKey; })) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot pending input not accepted"));
        }
    }
    for (const auto& accepted : dto.ingress.accepted) {
        const auto pending =
            std::count_if(dto.pending.begin(), dto.pending.end(),
                          [&](const auto& v) { return v.observationKey == accepted.key; });
        const auto processed =
            std::count_if(p.receipts.begin(), p.receipts.end(),
                          [&](const auto& v) { return v.observationKey == accepted.key; });
        if (pending + processed != 1) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot accepted input closure incomplete"));
        }
    }
    for (const auto& receipt : p.receipts) {
        const auto hop =
            differenceTicks(receipt.dispatchTick, receipt.observationKey.observationTick.tick());
        if (!p.processedFrontier || receipt.dispatchTick > *p.processedFrontier || !hop ||
            hop->value() < 0 ||
            hop->value() > inputs.configuration.latePolicy.maxQueueHop.measuredValue()->value() ||
            std::count_if(dto.ingress.accepted.begin(), dto.ingress.accepted.end(),
                          [&](const auto& v) { return v.key == receipt.observationKey; }) != 1) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot processed receipt closure invalid"));
        }
    }
    // Re-execute the sealed kernel prefix from prepared state and recorded dispatches.
    // Structural index checks alone cannot prove matcher/contact/phase reachability.
    auto prefix = prepare(inputs.configuration, inputs.prepared);
    if (!prefix) {
        return core::unexpected(prefix.error());
    }
    (*prefix)->impl_->configuration.ruleset.reset();
    for (const auto& receipt : p.receipts) {
        (*prefix)->impl_->pending.push_back(
            {receipt.observationKey,
             receipt.dispatchTick,
             receipt.dispatchTick != receipt.observationKey.observationTick.tick(),
             {},
             {}});
    }
    (*prefix)->impl_->pending.insert((*prefix)->impl_->pending.end(), dto.pending.begin(),
                                     dto.pending.end());
    if (p.processedFrontier) {
        const auto horizon =
            offsetTicks(*p.processedFrontier,
                        *inputs.configuration.latePolicy.windowCloseThreshold.measuredValue());
        if (!horizon) {
            return core::unexpected(horizon.error());
        }
        const auto advanced = (*prefix)->advanceRecorded(*horizon);
        if (!advanced) {
            return core::unexpected(advanced.error());
        }
    }
    const auto& rebuilt = *(*prefix)->impl_->active;
    const auto& result = rebuilt.projection;
    if (result.phases != p.phases || result.resources != p.resources ||
        result.contacts != p.contacts || result.ownership != p.ownership ||
        result.observers != p.observers || result.receipts != p.receipts ||
        result.facts != p.facts || rebuilt.matchers.size() != state->matchers.size()) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot kernel prefix unreachable"));
    }
    for (std::size_t i = 0; i < rebuilt.matchers.size(); ++i) {
        if (rebuilt.matchers[i].states != state->matchers[i].states ||
            rebuilt.matchers[i].epsilonAttempted != state->matchers[i].epsilonAttempted) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot matcher prefix unreachable"));
        }
    }
    k.impl_->pending = dto.pending;
    k.impl_->faultReserve = k.impl_->reserveFault(*state);
    k.impl_->active = std::move(state);
    k.impl_->historyAvailable = false;
    return std::move(*initialCandidate);
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "Snapshot restore validation failed"));
}

} // namespace cuexis::judgement::detail
