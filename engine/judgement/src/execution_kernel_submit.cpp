#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

auto ExecutionKernel::submit(std::vector<ClockedIngress> batch)
    -> core::Result<std::vector<InputReceiptPending>> {
    if (impl_->active->projection.rpa) {
        return core::unexpected(
            error("execution.profile_unsupported", "RPA requires closed occurrence batches"));
    }
    return submitBatch(batch);
}

auto ExecutionKernel::submitCanonical(std::vector<CanonicalInput> batch)
    -> core::Result<std::vector<InputReceiptPending>> try {
    if (impl_->active->projection.rpa) {
        bool transientAllocationFailure{false};
        auto result = submitRpa(batch, transientAllocationFailure);
        if (!result && !transientAllocationFailure && !batch.empty()) {
            auto record = impl_->recordingCandidate();
            std::sort(batch.begin(), batch.end(),
                      [](const auto& a, const auto& b) { return a.key < b.key; });
            if (batch.size() > UINT64_MAX - record->eventCount)
                throw std::overflow_error{"rejection event count"};
            record->eventCount += static_cast<std::uint64_t>(batch.size());
            record->records.push_back({std::move(batch), {}, {}, {}, {}, result.error()});
            impl_->publishRecording(std::move(record));
            impl_->releaseSeekSource();
        }
        return result;
    }
    if (std::any_of(batch.begin(), batch.end(),
                    [](const auto& v) { return v.key.sourceLocalPosition.has_value(); })) {
        return core::unexpected(
            error("execution.profile_unsupported", "occurrence input requires RPA"));
    }
    std::vector<ClockedIngress> inputs;
    std::vector<std::optional<std::int64_t>> amounts;
    const auto mapping = impl_->configuration.inputMapping.view();
    for (const auto& c : batch) {
        if (c.key.sourceClass != mapping.sourceClass.token()) {
            return core::unexpected(
                error("execution.relation_invalid", "canonical source class mismatch"));
        }
        const auto channel = ChannelRef::fromToken(c.key.channelToken);
        if (!channel) {
            return core::unexpected(channel.error());
        }
        inputs.push_back({c.key.observationTick,
                          {c.sequence,
                           c.key.action,
                           *channel,
                           c.key.domainToken,
                           {},
                           {false, false, false},
                           ContinuityKind::discrete,
                           {}}});
        amounts.push_back(c.key.amount);
    }
    return submitBatch(inputs, amounts);
} catch (const std::exception&) {
    impl_->reservedLog.reset();
    return core::unexpected(
        error("execution.relation_invalid", "canonical submission allocation failed"));
}
auto ExecutionKernel::submitBatch(std::span<const ClockedIngress> batch,
                                  std::span<const std::optional<std::int64_t>> amounts)
    -> core::Result<std::vector<InputReceiptPending>> try {
    if (impl_->active->projection.state != KernelSessionState::Prepared) {
        return core::unexpected(
            error("session.lifecycle_order", "submission requires a healthy prepared session"));
    }
    if (batch.empty()) {
        return std::vector<InputReceiptPending>{};
    }
    auto mapping = impl_->configuration.inputMapping.view();
    if (impl_->configuration.ruleset) {
        if (!validUtf8(mapping.profileId) || !validUtf8(mapping.profileVersion) ||
            !validUtf8(mapping.sourceClass.token())) {
            return core::unexpected(error("execution.relation_invalid", "invalid UTF-8 mapping"));
        }
        for (const auto& item : batch) {
            if (!validUtf8(item.declaration.domainToken) ||
                !validUtf8(item.declaration.channel.token())) {
                return core::unexpected(error("execution.relation_invalid", "invalid UTF-8 input"));
            }
        }
    }
    auto journal = prepareIngressBatch(impl_->ingress, mapping, batch, true, amounts);
    if (!journal) {
        return core::unexpected(journal.error());
    }
    std::vector<AdmittedObservation> entries;
    std::vector<InputReceiptPending> receipts;
    const auto& projection = impl_->active->projection;
    for (const auto& entry : journal->entries) {
        auto route = routeExecutionObservation(impl_->configuration.latePolicy,
                                               entry.observation.observationTick.tick(),
                                               projection.lastAdvanceHorizon);
        if (!route) {
            return core::unexpected(route.error());
        }
        auto key = keyOf(entry.observation);
        if (key.action == InputAction::release) {
            for (const auto& ownership : impl_->active->coverageHistory) {
                const auto requirement = std::find_if(
                    impl_->graph().requirements.begin(), impl_->graph().requirements.end(),
                    [&](const auto& r) { return r.identity == ownership.requirement; });
                const auto body = std::find_if(
                    projection.phases.begin(), projection.phases.end(), [&](const auto& p) {
                        return p.requirement == ownership.requirement && p.phase == PhaseKind::body;
                    });
                if (body != projection.phases.end() && body->state == KernelPhaseState::Hit &&
                    sameChannel(ownership.contact, key) &&
                    key.observationTick >= ownership.contact.startPressKey.observationTick &&
                    key.observationTick.tick() < requirement->timing->body->end) {
                    return core::unexpected(core::Error{"judgement.late.rejected",
                                                        "release would rewrite sealed coverage"}
                                                .withContext("category", "late_policy_incomplete")
                                                .withContext("severity", "error")
                                                .withContext("faulted", "false"));
                }
            }
        }
        for (const auto& existing : impl_->pending) {
            if (existing.dispatchTick == route->dispatchTick) {
                return core::unexpected(
                    error("late.dispatch_collision", "two observations share a dispatch Tick"));
            }
        }
        for (const auto& existing : entries) {
            if (existing.dispatchTick == route->dispatchTick) {
                return core::unexpected(
                    error("late.dispatch_collision", "batch observations share a dispatch Tick"));
            }
        }
        entries.push_back({key, route->dispatchTick, route->wasForwarded,
                           projection.processedFrontier, projection.lastAdvanceHorizon});
        receipts.push_back({std::move(key), route->dispatchTick, route->wasForwarded,
                            projection.processedFrontier, projection.lastAdvanceHorizon});
    }
    if (entries.size() > impl_->pending.max_size() - impl_->pending.size()) {
        return core::unexpected(
            error("execution.relation_invalid", "submission storage is not representable"));
    }
    static_assert(std::is_nothrow_move_constructible_v<AdmittedObservation>);
    static_assert(std::is_nothrow_move_assignable_v<AdmittedObservation>);
    impl_->pending.reserve(impl_->pending.size() + entries.size());
    if (auto reserved = reserveIngressBatch(impl_->ingress, *journal); !reserved) {
        return core::unexpected(reserved.error());
    }
    if (impl_->testControls && impl_->testControls->failSubmitRecordReservation) {
        throw std::bad_alloc{};
    }
    auto recording = impl_->recordingCandidate();
    if (journal->entries.size() > UINT64_MAX - recording->eventCount) {
        return core::unexpected(error("execution.relation_invalid", "Replay event count overflow"));
    }
    ReplayRecord record{{}, receipts, {}, {}};
    record.batch.reserve(journal->entries.size());
    for (const auto& entry : journal->entries) {
        record.batch.push_back({keyOf(entry.observation), entry.observation.ingressSequence});
    }
    recording->eventCount += static_cast<std::uint64_t>(record.batch.size());
    recording->records.push_back(std::move(record));
    commitIngressBatch(impl_->ingress, std::move(*journal));
    for (auto& entry : entries) {
        impl_->pending.push_back(std::move(entry));
    }
    std::sort(impl_->pending.begin(), impl_->pending.end(),
              [](const auto& a, const auto& b) { return a.dispatchTick < b.dispatchTick; });
    impl_->publishRecording(std::move(recording));
    impl_->releaseSeekSource();
    return receipts;
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "submission storage allocation failed"));
}

auto ExecutionKernel::submitRpa(std::vector<CanonicalInput> batch, bool& transientAllocationFailure)
    -> core::Result<std::vector<InputReceiptPending>> try {
    transientAllocationFailure = false;
    const auto& p = impl_->active->projection;
    if (p.state != KernelSessionState::Prepared || p.rpa->paused) {
        return core::unexpected(
            error("session.lifecycle_order", "RPA admission requires running state"));
    }
    auto draft = std::make_shared<RuntimeState>(*impl_->active);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaObservation::seekAdd(SeekPhase::s4InternalExecution, &SeekWork::draftCopies);
#endif
    auto pending = impl_->pending;
    auto recording = impl_->recordingCandidate();
    std::vector<InputReceiptPending> receipts;
    std::sort(batch.begin(), batch.end(),
              [](const auto& a, const auto& b) { return a.key < b.key; });
    const auto mapping = impl_->configuration.inputMapping.view();
    for (const auto& c : batch) {
        const auto& key = c.key;
        const auto domain =
            std::find_if(mapping.domains.begin(), mapping.domains.end(),
                         [&](const auto& v) { return v.domainToken == key.domainToken; });
        if (!key.sourceLocalPosition || key.channelToken.empty() ||
            key.sourceClass != mapping.sourceClass.token() || domain == mapping.domains.end() ||
            !validUtf8(key.domainToken) || !validUtf8(key.channelToken) ||
            (key.action != InputAction::press && key.action != InputAction::release &&
             key.action != InputAction::update)) {
            return core::unexpected(
                error("execution.relation_invalid", "invalid occurrence content"));
        }
        if (key.amount &&
            (*key.amount < domain->amount.minimum || *key.amount > domain->amount.maximum ||
             (domain->amount.boundaryPolicy == AmountBoundaryPolicy::exclusive &&
              (*key.amount == domain->amount.minimum || *key.amount == domain->amount.maximum)))) {
            return core::unexpected(
                error("execution.relation_invalid", "occurrence amount outside domain"));
        }
        for (const auto& existing : draft->projection.rpa->accepted) {
            if (std::tie(existing.observationTick, existing.domainToken, existing.sourceClass,
                         existing.channelToken, existing.sourceLocalPosition) ==
                std::tie(key.observationTick, key.domainToken, key.sourceClass, key.channelToken,
                         key.sourceLocalPosition)) {
                if (existing != key)
                    return core::unexpected(error("input.occurrence_conflict",
                                                  "same occurrence slot has different content"));
                return core::unexpected(core::Error{"judgement.late.duplicate_queue_entry",
                                                    "occurrence already admitted"}
                                            .withContext("category", "late_policy_incomplete")
                                            .withContext("severity", "error")
                                            .withContext("faulted", "false"));
            }
        }
        auto route = routeExecutionObservation(impl_->configuration.latePolicy,
                                               key.observationTick.tick(), p.lastAdvanceHorizon);
        if (!route)
            return core::unexpected(route.error());
        if (key.action == InputAction::release) {
            for (const auto& owned : impl_->active->coverageHistory) {
                const auto r = std::find_if(
                    impl_->graph().requirements.begin(), impl_->graph().requirements.end(),
                    [&](const auto& v) { return v.identity == owned.requirement; });
                const auto body =
                    std::find_if(p.phases.begin(), p.phases.end(), [&](const auto& v) {
                        return v.requirement == owned.requirement && v.phase == PhaseKind::body;
                    });
                if (body != p.phases.end() && body->state == KernelPhaseState::Hit &&
                    sameChannel(owned.contact, key) &&
                    key.observationTick >= owned.contact.startPressKey.observationTick &&
                    key.observationTick.tick() < r->timing->body->end) {
                    return core::unexpected(core::Error{"judgement.late.rejected",
                                                        "release would rewrite sealed coverage"}
                                                .withContext("category", "late_policy_incomplete")
                                                .withContext("severity", "error")
                                                .withContext("faulted", "false"));
                }
            }
        }
        pending.push_back({key, route->dispatchTick, route->wasForwarded, p.processedFrontier,
                           p.lastAdvanceHorizon});
        receipts.push_back({key, route->dispatchTick, route->wasForwarded, p.processedFrontier,
                            p.lastAdvanceHorizon});
        draft->projection.rpa->accepted.push_back(key);
    }
    std::sort(pending.begin(), pending.end(), [](const auto& a, const auto& b) {
        return std::tie(a.dispatchTick, a.observationKey) <
               std::tie(b.dispatchTick, b.observationKey);
    });
    if (batch.size() > UINT64_MAX - recording->eventCount)
        throw std::overflow_error{"event count"};
    if (impl_->testControls && impl_->testControls->failSubmitRecordReservation)
        throw std::bad_alloc{};
    recording->eventCount += static_cast<std::uint64_t>(batch.size());
    recording->records.push_back({std::move(batch), receipts, {}, {}, {}});
    auto reserve = impl_->reserveFault(*draft);
    impl_->active = std::move(draft);
    impl_->faultReserve = std::move(reserve);
    impl_->pending = std::move(pending);
    impl_->publishRecording(std::move(recording));
    impl_->releaseSeekSource();
    return receipts;
} catch (const std::bad_alloc&) {
    // No admission completed. A transient resource failure is not a deterministic Replay
    // rejection; preserve both the applied journal and an existing complete Seek source.
    transientAllocationFailure = true;
    impl_->reservedLog.reset();
    return core::unexpected(error("execution.relation_invalid", "occurrence reservation failed"));
} catch (const std::exception&) {
    return core::unexpected(error("execution.relation_invalid", "occurrence reservation failed"));
}

} // namespace cuexis::judgement::detail
