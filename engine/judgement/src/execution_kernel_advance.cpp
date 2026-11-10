#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

auto ExecutionKernel::controlRpa(const RpaControlRecord& control) -> core::Result<void> try {
    const internal::pd::memory::ExecutionDomainScope memoryScope{
        internal::pd::memory::domainCurrent};
    const auto& p = impl_->active->projection;
    const auto invalid = [] {
        return core::unexpected(
            error("control.reconciliation_invalid", "invalid RPA control declaration"));
    };
    if (!p.rpa || p.state != KernelSessionState::Prepared)
        return invalid();
    for (const auto& old : p.rpa->controls) {
        if (old.kind == control.kind && old.controlOrdinal == control.controlOrdinal) {
            if (old == control)
                return {};
            return invalid();
        }
    }
    if (control.controlOrdinal != p.rpa->nextControlOrdinal ||
        control.controlOrdinal == UINT64_MAX || p.processedFrontier != control.cutTick ||
        (!control.trace.empty() && control.kind == RpaControlKind::pause))
        return invalid();
    if (control.kind == RpaControlKind::pause &&
        (p.rpa->paused || control.pauseEpoch <= p.rpa->pauseEpoch))
        return invalid();
    if (control.kind == RpaControlKind::resume &&
        (!p.rpa->paused || p.rpa->cutTick != control.cutTick ||
         p.rpa->pauseEpoch != control.pauseEpoch || control.trace.size() != control.finalTraceCut))
        return invalid();
    if (control.kind != RpaControlKind::pause && control.kind != RpaControlKind::resume)
        return invalid();
    if (control.kind == RpaControlKind::pause && control.finalTraceCut)
        return invalid();
    const auto mapping = impl_->configuration.inputMapping.view();
    for (std::size_t i = 0; i < control.trace.size(); ++i) {
        const auto& key = control.trace[i].observation;
        const auto domain =
            std::find_if(mapping.domains.begin(), mapping.domains.end(),
                         [&](const auto& d) { return d.domainToken == key.domainToken; });
        if (control.trace[i].sequence != i || !key.sourceLocalPosition ||
            key.observationTick.tick() < control.cutTick || key.channelToken.empty() ||
            !validUtf8(key.channelToken) || key.sourceClass != mapping.sourceClass.token() ||
            domain == mapping.domains.end() ||
            (key.action != InputAction::press && key.action != InputAction::release &&
             key.action != InputAction::update) ||
            (i && key.observationTick < control.trace[i - 1].observation.observationTick))
            return invalid();
        if (key.amount &&
            (*key.amount < domain->amount.minimum || *key.amount > domain->amount.maximum ||
             (domain->amount.boundaryPolicy == AmountBoundaryPolicy::exclusive &&
              (*key.amount == domain->amount.minimum || *key.amount == domain->amount.maximum))))
            return invalid();
        for (std::size_t j = 0; j < i; ++j) {
            const auto& prior = control.trace[j].observation;
            if (std::tie(prior.observationTick, prior.domainToken, prior.sourceClass,
                         prior.channelToken) == std::tie(key.observationTick, key.domainToken,
                                                         key.sourceClass, key.channelToken) &&
                prior.sourceLocalPosition >= key.sourceLocalPosition)
                return invalid();
        }
    }
    auto recording = impl_->recordingCandidate();
    recording->records.push_back({{}, {}, {}, {}, control});
    auto draft = std::make_shared<RuntimeState>(*impl_->active);
    auto& logical = *draft->projection.rpa;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaObservation::seekAdd(SeekPhase::s4InternalExecution, &SeekWork::draftCopies);
#endif
    std::vector<FactDescriptor> facts;
    if (control.kind == RpaControlKind::pause) {
        logical.paused = true;
        logical.cutTick = control.cutTick;
        logical.pauseEpoch = control.pauseEpoch;
    } else {
        // Derive the physical contact at the cut from the complete admitted source history,
        // including future dispatches. A release binds only the currently active press.
        std::vector<CanonicalObservationKey> physical;
        auto accepted = logical.accepted;
        for (const auto& old : logical.controls)
            if (old.kind == RpaControlKind::resume)
                for (const auto& transition : old.trace)
                    accepted.push_back(transition.observation);
        std::sort(accepted.begin(), accepted.end());
        for (const auto& key : accepted) {
            if (key.observationTick.tick() > control.cutTick)
                continue;
            auto active = std::find_if(physical.begin(), physical.end(), [&](const auto& v) {
                return std::tie(v.domainToken, v.sourceClass, v.channelToken) ==
                       std::tie(key.domainToken, key.sourceClass, key.channelToken);
            });
            if (key.action == InputAction::release && active != physical.end())
                physical.erase(active);
            else if (key.action == InputAction::press && active == physical.end())
                physical.push_back(key);
        }
        for (const auto& transition : control.trace) {
            const auto& key = transition.observation;
            auto active = std::find_if(physical.begin(), physical.end(), [&](const auto& v) {
                return std::tie(v.domainToken, v.sourceClass, v.channelToken) ==
                       std::tie(key.domainToken, key.sourceClass, key.channelToken);
            });
            if (key.action == InputAction::release && active != physical.end()) {
                const auto start = *active;
                physical.erase(active);
                const bool future =
                    std::any_of(impl_->pending.begin(), impl_->pending.end(),
                                [&](const auto& v) { return v.observationKey == start; });
                if (future)
                    logical.deferred.push_back({start, control.controlOrdinal, transition});
                else
                    impl_->interruptRpa(*draft, facts, start,
                                        ReconciliationOrigin{control.cutTick,
                                                             control.controlOrdinal,
                                                             {},
                                                             control.finalTraceCut,
                                                             {}});
            } else if (key.action == InputAction::press && active == physical.end())
                physical.push_back(key);
        }
        logical.quarantine.clear();
        for (const auto& held : physical) {
            const bool contact =
                std::any_of(draft->projection.contacts.begin(), draft->projection.contacts.end(),
                            [&](const auto& v) { return v.startPressKey == held; });
            const bool future =
                std::any_of(impl_->pending.begin(), impl_->pending.end(),
                            [&](const auto& v) { return v.observationKey == held; });
            if (!contact && !future)
                logical.quarantine.push_back(held);
        }
        logical.paused = false;
        ++logical.nextControlOrdinal;
    }
    logical.controls.push_back(control);
    impl_->validateDraft(*draft);
    impl_->failIfRequested(control.cutTick, KernelFailurePoint::beforeSeal);
    impl_->ledger(*draft, facts, control.cutTick);
    draft->projection.sealedFactCursor = static_cast<std::uint64_t>(draft->projection.facts.size());
    auto reserve = impl_->reserveFault(*draft);
    reserve->projection.faultDiagnostic = impl_->foldFailureResult;
    // All fallible pre-seal work is complete. Fold is an independent failure boundary.
    impl_->active = std::move(draft);
    impl_->faultReserve = std::move(reserve);
    try {
        auto folded = std::make_shared<RuntimeState>(*impl_->active);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaObservation::seekAdd(SeekPhase::s4InternalExecution, &SeekWork::foldCopies);
#endif
        if (control.kind == RpaControlKind::resume && folded->projection.fold) {
            auto& rpa = *folded->projection.rpa;
            if (rpa.foldBaseTick == control.cutTick)
                folded->projection.fold = rpa.foldAtTickStart;
            else {
                rpa.foldAtTickStart = folded->projection.fold;
                rpa.foldBaseTick = control.cutTick;
            }
            impl_->failIfRequested(control.cutTick, KernelFailurePoint::beforeFold);
            impl_->fold(*folded, control.cutTick);
        }
        folded->projection.rpa->foldedControlCount =
            static_cast<std::uint64_t>(folded->projection.rpa->controls.size());
        auto foldedReserve = impl_->reserveFault(*folded);
        impl_->active = std::move(folded);
        impl_->faultReserve = std::move(foldedReserve);
    } catch (const std::exception&) {
        impl_->faultReserve->projection.failedTick = control.cutTick;
        impl_->faultReserve->projection.faultStage = FaultStage::fold;
        impl_->active = std::move(impl_->faultReserve);
        recording->records.back().result =
            std::shared_ptr<const KernelProjection>(impl_->active, &impl_->active->projection);
        impl_->publishRecording(std::move(recording));
        impl_->releaseSeekSource();
        return core::unexpected(std::move(impl_->foldFailureResult));
    }
    recording->records.back().result =
        std::shared_ptr<const KernelProjection>(impl_->active, &impl_->active->projection);
    impl_->publishRecording(std::move(recording));
    impl_->releaseSeekSource();
    return {};
} catch (const std::exception&) {
    return core::unexpected(
        error("control.reconciliation_invalid", "RPA pre-seal reservation failed"));
}

auto ExecutionKernel::advance(Tick horizon, bool controlDrain) -> core::Result<void> try {
    const internal::pd::memory::ExecutionDomainScope memoryScope{
        internal::pd::memory::domainCurrent};
    if (impl_->testControls && impl_->testControls->failRecordReservationHorizon == horizon) {
        throw std::bad_alloc{};
    }
    auto record = impl_->recordingCandidate();
    record->records.push_back({{}, {}, horizon, {}, {}, {}, controlDrain});
    const auto before = impl_->active;
    auto outcome = advanceRecorded(horizon);
    if (outcome || before != impl_->active) {
        record->records.back().result =
            std::shared_ptr<const KernelProjection>(impl_->active, &impl_->active->projection);
        impl_->publishRecording(std::move(record));
        impl_->releaseSeekSource();
    }
    return outcome;
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "Replay control reservation failed"));
}

auto ExecutionKernel::advanceRecorded(Tick horizon) -> core::Result<void> {
    internal::execution_diagnostics::Trace diagnostic{"advanceRecorded", horizon.value(),
                                                      impl_->prepared.timers().size()};
    if ((impl_->active->projection.rpa && impl_->active->projection.rpa->paused) ||
        impl_->active->projection.state != KernelSessionState::Prepared ||
        (impl_->active->projection.lastAdvanceHorizon &&
         horizon < *impl_->active->projection.lastAdvanceHorizon)) {
        return core::unexpected(
            error("session.lifecycle_order", "advance requires a monotonic healthy session"));
    }
    auto frontier = offsetTicks(
        horizon,
        TickSpan{-impl_->configuration.latePolicy.windowCloseThreshold.measuredValue()->value()});
    if (!frontier) {
        return core::unexpected(frontier.error());
    }
    if (impl_->active->projection.lastAdvanceHorizon == horizon) {
        return {};
    }
    std::optional<Tick> failedTick;
    bool foldFailed = false;
    try {
        const auto timers = impl_->prepared.timers();
        std::shared_ptr<RuntimeState> writable;
        bool idleFoldReady = false;
        bool executedTick = false;
        while (true) {
            std::optional<Tick> tick;
            auto cursor = impl_->active->timerCursor;
            if (cursor < timers.size() && timers[cursor].tick <= *frontier) {
                tick = timers[cursor].tick;
            }
            for (const auto& input : impl_->pending) {
                if (input.dispatchTick <= *frontier &&
                    (!impl_->active->projection.processedFrontier ||
                     input.dispatchTick > *impl_->active->projection.processedFrontier) &&
                    (!tick || input.dispatchTick < *tick)) {
                    tick = input.dispatchTick;
                }
            }
            if (impl_->active->projection.fold) {
                const auto& fold = *impl_->active->projection.fold;
                if (fold.hookConsumerCursor < fold.produced.size()) {
                    const auto due =
                        fold.produced[static_cast<std::size_t>(fold.hookConsumerCursor)]
                            .visibleTick;
                    if (due <= *frontier && (!tick || due < *tick)) {
                        tick = due;
                    }
                }
            }
            if (impl_->testControls) {
                for (const auto signal : impl_->testControls->signalTicks) {
                    if (signal <= *frontier &&
                        (!impl_->active->projection.processedFrontier ||
                         signal > *impl_->active->projection.processedFrontier) &&
                        (!tick || signal < *tick)) {
                        tick = signal;
                    }
                }
            }
            if (!tick) {
                break;
            }
            executedTick = true;
            failedTick = *tick;
            diagnostic.progress(tick->value(), impl_->active->timerCursor);
            std::size_t idleEnd{};
            if (idleFoldReady && writable && writable.get() == impl_->active.get() &&
                writable.use_count() == 2 && impl_->idleDeadlineEnd(*writable, *tick, idleEnd)) {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::idleDeadline};
                if (impl_->testControls && impl_->testControls->work) {
                    ++impl_->testControls->work->idleDeadlineTicks;
                    impl_->testControls->work->timers += idleEnd - writable->timerCursor;
                }
                Impl::sealIdleDeadline(*writable, *tick, idleEnd);
                Impl::sealIdleDeadline(*impl_->faultReserve, *tick, idleEnd);
                diagnostic.sealed(tick->value(), idleEnd);
                failedTick.reset();
                continue;
            }
            if (impl_->testControls && impl_->testControls->work)
                ++impl_->testControls->work->fullTickTransactions;
            auto draft = [&] {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::copy};
                auto copy = std::make_shared<RuntimeState>(*impl_->active);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                rpaObservation::seekAdd(SeekPhase::s4InternalExecution, &SeekWork::draftCopies);
#endif
                return copy;
            }();
            std::vector<AdmittedObservation> inputs;
            for (const auto& input : impl_->pending) {
                if (input.dispatchTick == *tick) {
                    inputs.push_back(input);
                }
            }
            std::vector<FactDescriptor> facts;
            const auto activeCount = draft->activeRequirements.size();
            while (draft->timerCursor < timers.size() && timers[draft->timerCursor].tick == *tick) {
                if (impl_->testControls && impl_->testControls->work)
                    ++impl_->testControls->work->timers;
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::timers};
                const auto timerIndex = draft->timerCursor++;
                impl_->timer(*draft, facts, timers[timerIndex],
                             impl_->preparation->timerRequirements[timerIndex], inputs);
            }
            if (draft->activeRequirements.size() != activeCount)
                std::sort(draft->activeRequirements.begin(), draft->activeRequirements.end());
            impl_->failIfRequested(*tick, KernelFailurePoint::afterTimers);
            for (const auto& input : inputs) {
                impl_->input(*draft, facts, input);
                impl_->deferredRpa(*draft, facts, input);
            }
            impl_->failIfRequested(*tick, KernelFailurePoint::afterInput);
            if (impl_->testControls && impl_->testControls->duplicatePhaseTick == *tick &&
                !draft->projection.phases.empty()) {
                draft->projection.phases.push_back(draft->projection.phases[0]);
            }
            {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::validate};
                // Normal execution changes phase states, never their identity/layout.
                impl_->validateDraft(*draft, impl_->testControls &&
                                                 impl_->testControls->duplicatePhaseTick == *tick);
            }
            impl_->failIfRequested(*tick, KernelFailurePoint::beforeLedger);
            {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::ledger};
                impl_->ledger(*draft, facts, *tick);
            }
            if (impl_->testControls && std::find(impl_->testControls->signalTicks.begin(),
                                                 impl_->testControls->signalTicks.end(),
                                                 *tick) != impl_->testControls->signalTicks.end()) {
                auto visible = offsetTicks(*tick, TickSpan{1});
                if (!visible) {
                    throw std::overflow_error{"signal visibility overflow"};
                }
                draft->signalVisibility.push_back(*visible);
            }
            impl_->failIfRequested(*tick, KernelFailurePoint::beforeSeal);
            draft->projection.processedFrontier = *tick;
            draft->projection.kernelWorkTick = *tick;
            draft->projection.sealedFactCursor =
                static_cast<std::uint64_t>(draft->projection.facts.size());
            auto reserve = [&] {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::reserve};
                return impl_->reserveFault(*draft);
            }();
            reserve->projection.faultDiagnostic = impl_->foldFailureResult;
            impl_->active = std::move(draft);
            impl_->faultReserve = std::move(reserve);
            impl_->pending.erase(
                std::remove_if(impl_->pending.begin(), impl_->pending.end(),
                               [&](const auto& v) { return v.dispatchTick <= *tick; }),
                impl_->pending.end());
            foldFailed = true;
            auto folded = [&] {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::foldCopy};
                auto copy = std::make_shared<RuntimeState>(*impl_->active);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                rpaObservation::seekAdd(SeekPhase::s4InternalExecution, &SeekWork::foldCopies);
#endif
                return copy;
            }();
            impl_->failIfRequested(*tick, KernelFailurePoint::beforeFold);
            if (folded->projection.rpa) {
                folded->projection.rpa->foldAtTickStart = folded->projection.fold;
                folded->projection.rpa->foldBaseTick = *tick;
            }
            {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::fold};
                impl_->fold(*folded, *tick);
            }
            auto foldedReserve = [&] {
                internal::execution_diagnostics::PartScope part{
                    internal::execution_diagnostics::Part::reserve};
                return impl_->reserveFault(*folded);
            }();
            impl_->active = folded;
            writable = std::move(folded);
            impl_->faultReserve = std::move(foldedReserve);
            idleFoldReady =
                facts.empty() &&
                (!writable->projection.fold ||
                 (writable->projection.fold->produced.empty() &&
                  writable->projection.fold->factCursor == writable->projection.facts.size())) &&
                (!writable->projection.rpa ||
                 writable->projection.rpa->foldAtTickStart.has_value() ==
                     writable->projection.fold.has_value());
            foldFailed = false;
            diagnostic.sealed(tick->value(), impl_->active->timerCursor);
            failedTick.reset();
        }
        if (impl_->testControls && impl_->testControls->failAdvanceFinalization)
            throw std::bad_alloc{};
        auto final = std::make_shared<RuntimeState>(*impl_->active);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaObservation::seekAdd(SeekPhase::s4InternalExecution, &SeekWork::finalizationCopies);
#endif
        final->projection.processedFrontier = *frontier;
        final->projection.lastAdvanceHorizon = horizon;
        // With no Tick work, the private reserve already mirrors every payload member.
        // Test injection can change active alone, so injected runs keep the reference path.
        const bool reuseReserve = !executedTick && final->projection.rpa && !impl_->testControls &&
                                  impl_->faultReserve.use_count() == 1;
        auto reserve = reuseReserve ? impl_->faultReserve : impl_->reserveFault(*final);
        // All fallible work is complete. Only these nonthrowing scalars changed on this path.
        if (reuseReserve) {
            reserve->projection.processedFrontier = *frontier;
            reserve->projection.lastAdvanceHorizon = horizon;
        }
        impl_->active = std::move(final);
        impl_->faultReserve = std::move(reserve);
        return {};
    } catch (const std::exception&) {
        // The reserve mirrors the last sealed prefix and has never been published.
        // This path performs no allocation, including the Result error construction.
        auto& projection = impl_->faultReserve->projection;
        projection.failedTick = failedTick;
        projection.requestedHorizon = horizon;
        projection.faultStage =
            foldFailed ? FaultStage::fold : (failedTick ? FaultStage::kernel : FaultStage::control);
        if (failedTick && !foldFailed) {
            projection.processedFrontier = failedTick->value() == INT64_MIN
                                               ? std::nullopt
                                               : std::optional{Tick{failedTick->value() - 1}};
        }
        impl_->active = std::move(impl_->faultReserve);
        return core::unexpected(foldFailed ? std::move(impl_->foldFailureResult)
                                           : std::move(impl_->failureResult));
    }
}

} // namespace cuexis::judgement::detail
