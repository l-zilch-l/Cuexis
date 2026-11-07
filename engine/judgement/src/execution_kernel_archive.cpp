#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

ExecutionKernel::ExecutionKernel(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ExecutionKernel::~ExecutionKernel() = default;

auto ExecutionKernel::archive() const -> ReplayArchive {
    if (impl_->active->projection.rpa && !impl_->seekSource) {
        auto data = *impl_->recording;
        data.records = flattenLog(impl_->logTail);
        ReplayArchive archive{std::make_shared<const ReplayData>(std::move(data)),
                              impl_->recoveryInputs};
        archive.internallySealed_ = true;
        archive.preparation_ = impl_->preparation;
        auto index = std::make_shared<NativeRecoveryIndex>();
        index->points = impl_->nativePoints;
        // The index is rebound only while sealing this exact append-only journal. Every
        // point retains the immutable prefix from this branch; no address/hash matching.
        archive.nativeRecovery_ = std::move(index);
        archive.canonicalCache_ = makeCanonicalReplayIdentityCache();
        return archive;
    }
    ReplayArchive archive{impl_->seekSource ? impl_->seekSource : impl_->recording,
                          impl_->recoveryInputs};
    // Returning an untrusted Seek source cannot launder it into an internal source.
    archive.internallySealed_ = !impl_->seekSource || impl_->seekSourceSealed;
    if (archive.internallySealed_)
        archive.preparation_ = impl_->preparation;
    if (impl_->seekSource)
        archive.nativeRecovery_ = impl_->seekNativeSource;
    if (impl_->seekSource)
        archive.canonicalCache_ = impl_->seekCanonicalCache;
    return archive;
}
auto ExecutionKernel::canArchive() const noexcept -> bool {
    return impl_->historyAvailable;
}
auto ExecutionKernel::evaluate(const ReplayArchive& archive, const KernelTestControls* controls)
    -> core::Result<ReplayEvaluation> try {
    auto k = prepare(archive.dependencies().configuration, archive.dependencies().prepared);
    if (!k) {
        return core::unexpected(k.error());
    }
    if ((*k)->query()->projection.judgementIdentity != archive.data().identity) {
        return core::unexpected(error("execution.relation_invalid", "Replay identity mismatch"));
    }
    for (const auto& record : archive.data().records)
        if (record.result && record.result->faultDiagnostic &&
            containsLegacyDiagnostic(*record.result->faultDiagnostic))
            return core::unexpected(
                error("execution.relation_invalid", "Replay diagnostic dialect invalid"));
    const bool controlInjection =
        controls && std::any_of(archive.data().records.begin(), archive.data().records.end(),
                                [](const auto& r) {
                                    return r.control && r.result &&
                                           r.result->state == KernelSessionState::Faulted;
                                });
    if (controls && !controlInjection) {
        auto applied = (*k)->inject(*controls);
        if (!applied) {
            return core::unexpected(applied.error());
        }
    }
    std::uint64_t count = 0;
    for (const auto& record : archive.data().records) {
        if (record.control) {
            if (controlInjection && record.result &&
                record.result->state == KernelSessionState::Faulted) {
                auto injected = (*k)->inject(*controls);
                if (!injected)
                    return core::unexpected(injected.error());
            }
            if (record.rejection || record.horizon || !record.batch.empty() ||
                !record.admission.empty() || !record.result)
                return core::unexpected(
                    error("execution.relation_invalid", "Replay RPA structure invalid"));
            auto result = (*k)->controlRpa(*record.control);
            if ((!result && (*k)->query()->projection.state != KernelSessionState::Faulted) ||
                !sameKernelResult((*k)->query()->projection, *record.result))
                return core::unexpected(
                    error("execution.relation_invalid", "Replay RPA full result mismatch"));
        } else if (record.horizon) {
            if (!record.batch.empty() || !record.admission.empty() || !record.result) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay control structure invalid"));
            }
            auto result = (*k)->advance(*record.horizon, record.controlDrain);
            const auto& current = (*k)->query()->projection;
            if ((!result && current.state != KernelSessionState::Faulted) ||
                !sameKernelResult(current, *record.result)) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay full result mismatch"));
            }
        } else {
            if (record.result || record.batch.empty() || record.batch.size() > UINT64_MAX - count) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay accepted batch structure invalid"));
            }
            const auto sorted =
                std::is_sorted(record.batch.begin(), record.batch.end(),
                               [](const auto& a, const auto& b) { return a.key < b.key; });
            if (!sorted) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay canonical records unordered"));
            }
            auto accepted = (*k)->submitCanonical(record.batch);
            if (record.rejection
                    ? accepted.has_value() || !sameRecoveryError(accepted.error(), record.rejection)
                    : !accepted || *accepted != record.admission) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay admission mismatch"));
            }
            count += static_cast<std::uint64_t>(record.batch.size());
        }
    }
    if (count != archive.data().eventCount) {
        return core::unexpected(error("execution.relation_invalid", "Replay event count mismatch"));
    }
    return ReplayEvaluation{true, std::shared_ptr<const KernelProjection>(
                                      (*k)->impl_->active, &(*k)->impl_->active->projection)};
} catch (const std::exception&) {
    return core::unexpected(error("execution.relation_invalid", "Replay evaluation failed"));
}
auto ExecutionKernel::seekCandidate(const ReplayArchive& archive, Tick horizon,
                                    const KernelTestControls* controls,
                                    const ReplayCut* requestedCut,
                                    std::span<const ReplayCheckpoint> checkpoints)
    -> core::Result<std::unique_ptr<ExecutionKernel>> try {
    const auto cut = requestedCut ? *requestedCut : replayCutAt(archive, horizon);
    if (cut.completeRecords > archive.data().records.size() ||
        (cut.partialHorizon && *cut.partialHorizon != horizon)) {
        return core::unexpected(error("execution.relation_invalid", "Seek cut invalid"));
    }
    std::uint64_t eventCount = 0;
    for (const auto& r : archive.data().records) {
        if (r.control) {
            if (!r.result || r.horizon || !r.batch.empty() || !r.admission.empty())
                return core::unexpected(
                    error("execution.relation_invalid", "Replay RPA shape invalid"));
        } else if (r.horizon) {
            if (!r.result || !r.batch.empty() || !r.admission.empty()) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay control shape invalid"));
            }
        } else {
            if (r.batch.empty() || r.result ||
                (r.rejection ? !r.admission.empty() : r.batch.size() != r.admission.size()) ||
                r.batch.size() > UINT64_MAX - eventCount ||
                !std::is_sorted(r.batch.begin(), r.batch.end(),
                                [](const auto& a, const auto& b) { return a.key < b.key; })) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay batch shape invalid"));
            }
            eventCount += static_cast<std::uint64_t>(r.batch.size());
        }
    }
    if (eventCount != archive.data().eventCount) {
        return core::unexpected(error("execution.relation_invalid", "Replay event count invalid"));
    }
    if (!cut.partialHorizon) {
        std::optional<Tick> recordedHorizon;
        for (std::uint64_t i = 0; i < cut.completeRecords; ++i) {
            if (archive.data().records[static_cast<std::size_t>(i)].horizon) {
                recordedHorizon = archive.data().records[static_cast<std::size_t>(i)].horizon;
            }
        }
        if (recordedHorizon != horizon) {
            return core::unexpected(
                error("execution.relation_invalid", "Seek full cut horizon mismatch"));
        }
    }
    auto candidate = [&]() -> core::Result<std::unique_ptr<ExecutionKernel>> {
        const auto& plan = archive.preparation_;
        if (!archive.internallySealed_ || !plan || plan->inputs != archive.inputs_)
            return prepare(archive.dependencies().configuration, archive.dependencies().prepared);
        // This exact immutable owner already passed full preparation. A run owns its
        // scope, matchers and fault reserve independently; only immutable data is reused.
        auto initial = std::make_shared<RuntimeState>(*plan->initial);
        initial->projection.runScope = std::make_shared<const std::uint8_t>(0);
        auto impl = std::make_unique<Impl>(plan->inputs->configuration, plan->inputs->prepared,
                                           std::move(initial), plan);
        return std::unique_ptr<ExecutionKernel>{new ExecutionKernel{std::move(impl)}};
    }();
    if (!candidate) {
        return core::unexpected(candidate.error());
    }
    if ((*candidate)->query()->projection.judgementIdentity != archive.data().identity) {
        return core::unexpected(
            error("execution.relation_invalid", "Seek archive identity mismatch"));
    }
    for (const auto& record : archive.data().records)
        if (record.result && record.result->faultDiagnostic &&
            containsLegacyDiagnostic(*record.result->faultDiagnostic))
            return core::unexpected(
                error("execution.relation_invalid", "Replay diagnostic dialect invalid"));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    std::uint64_t rpaBatchInputs = 0;
    std::uint64_t rpaControlHorizons = 0;
    std::uint64_t rpaAdvanceCalls = 0;
    std::uint64_t rpaIdleAdvances = 0;
    std::uint64_t rpaMatcherSteps = 0;
    std::uint64_t rpaMaxBatch = 0;
    std::uint64_t rpaMaxRecordElements = 0;
    const auto rpaElements = [](const KernelProjection& p) {
        return static_cast<std::uint64_t>(p.facts.size() + p.contacts.size() + p.observers.size() +
                                          p.receipts.size() + p.ownership.size() + p.phases.size() +
                                          p.resources.size());
    };
    std::uint64_t rpaRestores = 0;
    std::uint64_t rpaReplayPrefixCopies = 0;
#endif
    std::uint64_t begin = 0;
    const ReplayCheckpoint* chosen = nullptr;
    const bool rpa = (*candidate)->query()->projection.rpa.has_value();
    if (rpa && checkpoints.empty() && !controls && archive.internallySealed_ &&
        archive.nativeRecovery_) {
        const NativeRecoveryPoint* chosenNative = nullptr;
        for (const auto& point : archive.nativeRecovery_->points) {
            const auto& p = point->state->projection;
            if (point->completeRecords <= cut.completeRecords && p.lastAdvanceHorizon &&
                *p.lastAdvanceHorizon <= horizon &&
                (!chosenNative || point->completeRecords > chosenNative->completeRecords))
                chosenNative = point.get();
        }
        if (chosenNative) {
            try {
                auto& run = *(*candidate)->impl_;
                auto state = std::make_shared<RuntimeState>(*chosenNative->state);
                state->projection.runScope = run.active->projection.runScope;
                auto reserve = run.reserveFault(*state);
                run.active = std::move(state);
                run.faultReserve = std::move(reserve);
                run.logTail = chosenNative->prefix;
                run.recording = chosenNative->recording;
                run.appliedRecords = chosenNative->completeRecords;
                run.nativeBaseRecords = chosenNative->completeRecords;
                begin = chosenNative->completeRecords;
            } catch (const std::exception&) {
                // All fallible work precedes publication. Optional cache miss uses origin.
            }
        }
    }
    if (rpa && checkpoints.size() == 1) {
        const auto& checkpoint = checkpoints.front();
        const auto& cert = checkpoint.certificate_;
        // Only one already-certified hint is inspected. Neither an external mutable owner
        // nor a substituted public field can acquire a private state certificate.
        if ((!controls || controls->failCertifiedRestore) && archive.internallySealed_ &&
            checkpoint.archive.internallySealed_ && checkpoint.state && cert &&
            cert->rpaStateVersion == 1 && cert->archive == archive.anchor() &&
            checkpoint.archive.anchor() == cert->archive && checkpoint.state == cert->state &&
            checkpoint.cut == cert->cut && checkpoint.horizon == cert->horizon &&
            !checkpoint.cut.partialHorizon && checkpoint.horizon <= horizon &&
            checkpoint.cut.completeRecords <= cut.completeRecords &&
            cert->state->kernel.judgementIdentity == archive.data().identity &&
            (*candidate)->restoreCertifiedRpa(*cert, controls)) {
            begin = checkpoint.cut.completeRecords;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            ++rpaRestores;
#endif
        }
    }
    if (!rpa) {
        for (const auto& checkpoint : checkpoints) {
            if (checkpoint.state && checkpoint.horizon <= horizon &&
                checkpoint.cut.completeRecords <= cut.completeRecords &&
                (checkpoint.cut.completeRecords != cut.completeRecords ||
                 !checkpoint.cut.partialHorizon ||
                 (cut.partialHorizon && *checkpoint.cut.partialHorizon <= *cut.partialHorizon)) &&
                (!chosen || checkpoint.horizon > chosen->horizon ||
                 (checkpoint.horizon == chosen->horizon &&
                  checkpoint.cut.completeRecords > chosen->cut.completeRecords)) &&
                sameReplayHistory(checkpoint.archive, archive)) {
                chosen = &checkpoint;
            }
        }
    }
    if (chosen && !(*candidate)->query()->projection.rpa) {
        // A same-identity, structurally valid state is insufficient. Bind the full archive,
        // exact cut/H and compare every private member from the origin before trusting it.
        const auto& cert = chosen->certificate_;
        bool verified = archive.internallySealed_ && cert && cert->archive == archive.anchor() &&
                        cert->cut == chosen->cut && cert->horizon == chosen->horizon &&
                        cert->state == chosen->state && !controls;
        if (!verified) {
            auto origin = seekCandidate(archive, chosen->horizon, controls, &chosen->cut);
            if (origin) {
                auto full = (*origin)->snapshot();
                verified = full && sameSnapshot((*full)->dto, *chosen->state);
            }
        }
        if (verified) {
            SnapshotStorage state{*chosen->state,
                                  std::make_shared<const RecoveryInputs>(archive.dependencies())};
            auto restored = restore(state, archive.dependencies());
            if (restored) {
                candidate = std::move(restored);
                begin = chosen->cut.completeRecords;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                ++rpaRestores;
#endif
                auto prefix = std::make_shared<ReplayData>(archive.data());
                prefix->records.resize(static_cast<std::size_t>(begin));
                prefix->eventCount = 0;
                for (const auto& r : prefix->records) {
                    prefix->eventCount += static_cast<std::uint64_t>(r.batch.size());
                }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                ++rpaReplayPrefixCopies;
#endif
                (*candidate)->impl_->recording = std::move(prefix);
                (*candidate)->impl_->historyAvailable = true;
            }
        }
    }
    auto& k = **candidate;
    if (controls) {
        auto applied = k.inject(*controls);
        if (!applied) {
            return core::unexpected(applied.error());
        }
    }
    const auto shareValidatedPages = [&](const KernelProjection& expected) {
        if (!rpa || !archive.internallySealed_ || controls)
            return;
        // Called only AFTER full exact comparison. Rebind equal immutable pages to the
        // source owner, so later unchanged records compare in O(page directory) or O(1).
        // Do not mutate an already published state or any source/checkpoint owner.
        auto state = std::make_shared<RuntimeState>(*k.impl_->active);
        state->projection.phases = expected.phases;
        state->projection.facts = expected.facts;
        auto reserve = k.impl_->reserveFault(*state);
        k.impl_->active = std::move(state);
        k.impl_->faultReserve = std::move(reserve);
    };
    for (std::uint64_t i = begin; i < cut.completeRecords; ++i) {
        const auto& record = archive.data().records[static_cast<std::size_t>(i)];
        if (record.control) {
            if (record.control->cutTick > horizon || !record.result || record.horizon ||
                !record.batch.empty())
                return core::unexpected(
                    error("execution.relation_invalid", "Seek RPA control invalid"));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            ++rpaControlHorizons;
#endif
            auto result = k.controlRpa(*record.control);
            if (!result || !sameKernelResult(k.query()->projection, *record.result))
                return core::unexpected(
                    error("execution.relation_invalid", "Seek RPA result mismatch"));
            shareValidatedPages(*record.result);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpaMaxRecordElements = std::max(rpaMaxRecordElements, rpaElements(*record.result));
#endif
        } else if (record.horizon) {
            if (!record.result || !record.batch.empty() || *record.horizon > horizon) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek complete control invalid"));
            }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            ++rpaControlHorizons;
            ++rpaAdvanceCalls;
            if (record.controlDrain)
                ++rpaIdleAdvances;
#endif
            auto result = k.advance(*record.horizon, record.controlDrain);
            if (!result) {
                return core::unexpected(result.error());
            }
            if (!sameKernelResult(k.query()->projection, *record.result)) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek complete control mismatch"));
            }
            shareValidatedPages(*record.result);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpaMaxRecordElements = std::max(rpaMaxRecordElements, rpaElements(*record.result));
#endif
        } else {
            if (record.result || record.batch.empty()) {
                return core::unexpected(error("execution.relation_invalid", "Seek batch invalid"));
            }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpaBatchInputs += static_cast<std::uint64_t>(record.batch.size());
            rpaMatcherSteps += static_cast<std::uint64_t>(record.batch.size());
            rpaMaxBatch = std::max(rpaMaxBatch, static_cast<std::uint64_t>(record.batch.size()));
#endif
            auto accepted = k.submitCanonical(record.batch);
            if (record.rejection
                    ? accepted.has_value() || !sameRecoveryError(accepted.error(), record.rejection)
                    : !accepted || *accepted != record.admission) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek admission mismatch"));
            }
        }
    }
    if (cut.partialHorizon) {
        if (cut.completeRecords < archive.data().records.size()) {
            const auto& next =
                archive.data().records[static_cast<std::size_t>(cut.completeRecords)];
            if (!next.horizon || *next.horizon < horizon) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek partial control invalid"));
            }
        }
        auto result = k.advance(horizon);
        if (!result) {
            return core::unexpected(result.error());
        }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        ++rpaAdvanceCalls;
#endif
    } else if (k.query()->projection.lastAdvanceHorizon != horizon) {
        return core::unexpected(
            error("execution.relation_invalid", "Seek full cut horizon mismatch"));
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    {
        using Phase = internal::execution_diagnostics::rpa::SeekPhase;
        using Work = internal::execution_diagnostics::rpa::SeekWork;
        const auto set = [](Phase phase, std::uint64_t Work::* member, std::uint64_t value) {
            internal::execution_diagnostics::rpa::seekSet(phase, member, value);
        };
        const auto& projection = k.query()->projection;
        const auto facts = static_cast<std::uint64_t>(projection.facts.size());
        const auto elements = facts + static_cast<std::uint64_t>(projection.contacts.size()) +
                              static_cast<std::uint64_t>(projection.observers.size()) +
                              static_cast<std::uint64_t>(projection.receipts.size()) +
                              static_cast<std::uint64_t>(projection.ownership.size()) +
                              static_cast<std::uint64_t>(projection.phases.size()) +
                              static_cast<std::uint64_t>(projection.resources.size());
        const auto bytes = static_cast<std::uint64_t>(
            projection.facts.capacity() * sizeof(KernelFact) +
            projection.contacts.capacity() * sizeof(ContactHandle) +
            projection.observers.capacity() * sizeof(ObservationRecord) +
            projection.receipts.capacity() * sizeof(InputReceipt) +
            projection.ownership.capacity() * sizeof(HeadContactOwnership) +
            projection.phases.capacity() * sizeof(PhaseProjection) +
            projection.resources.capacity() * sizeof(ResourceProjection));
        const auto peakElements = static_cast<std::uint64_t>(
            projection.facts.capacity() + projection.contacts.capacity() +
            projection.observers.capacity() + projection.receipts.capacity() +
            projection.ownership.capacity() + projection.phases.capacity() +
            projection.resources.capacity());
        set(Phase::s3Validation, &Work::actualRecords, cut.completeRecords - begin);
        set(Phase::s3Validation, &Work::batchInputs, rpaBatchInputs);
        set(Phase::s3Validation, &Work::controlHorizons, rpaControlHorizons);
        set(Phase::s3Validation, &Work::nestedResultElements, elements);
        set(Phase::s3Validation, &Work::nestedResultBytes, bytes);
        set(Phase::s4InternalExecution, &Work::fullTicks, rpaAdvanceCalls);
        set(Phase::s4InternalExecution, &Work::idleTicks, rpaIdleAdvances);
        set(Phase::s4InternalExecution, &Work::timers,
            static_cast<std::uint64_t>(archive.dependencies().prepared.timers().size()));
        set(Phase::s4InternalExecution, &Work::matcherSteps, rpaMatcherSteps);
        set(Phase::s4InternalExecution, &Work::folds, rpaAdvanceCalls);
        set(Phase::s4InternalExecution, &Work::facts, facts);
        set(Phase::s4InternalExecution, &Work::runtimeCopies,
            1 + rpaRestores + rpaReplayPrefixCopies);
        set(Phase::s4InternalExecution, &Work::runtimeElements, elements);
        set(Phase::s4InternalExecution, &Work::runtimePayloadBytes, bytes);
        set(Phase::s4InternalExecution, &Work::runtimePeakElements, peakElements);
        set(Phase::s4InternalExecution, &Work::recordPeakElements,
            std::max({rpaMaxRecordElements, rpaMaxBatch, elements}));
    }
#endif
    k.impl_->seekSource = archive.anchor();
    k.impl_->seekSourceSealed = archive.internallySealed_;
    k.impl_->seekNativeSource = archive.nativeRecovery_;
    k.impl_->seekCanonicalCache = archive.canonicalCache_;
    return std::move(*candidate);
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "Seek candidate allocation failed"));
}

} // namespace cuexis::judgement::detail
