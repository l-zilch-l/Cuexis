#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

auto ExecutionKernel::query() const noexcept -> std::shared_ptr<const ProjectionStorage> {
    return impl_->active;
}
auto ExecutionKernel::initialProjection(const ReplayArchive& archive)
    -> core::Result<std::shared_ptr<const ProjectionStorage>> {
    const auto& plan = archive.preparation_;
    if (archive.internallySealed_ && plan && plan->inputs == archive.inputs_)
        return std::static_pointer_cast<const ProjectionStorage>(plan->initial);
    auto kernel = prepare(archive.dependencies().configuration, archive.dependencies().prepared);
    if (!kernel)
        return core::unexpected(std::move(kernel.error()));
    return (*kernel)->query();
}

auto ExecutionKernel::inject(KernelTestControls controls) -> core::Result<void> try {
    auto draft = std::make_shared<RuntimeState>(*impl_->active);
    if (controls.nextCommitId) {
        draft->nextCommitId = *controls.nextCommitId;
        draft->idsExhausted = false;
    }
    impl_->testControls = std::move(controls);
    impl_->active = std::move(draft);
    return {};
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "checking fixture allocation failed"));
}
auto ExecutionKernel::visibleSignalCount(Tick tick) const noexcept -> std::size_t {
    return static_cast<std::size_t>(std::count_if(
        impl_->active->signalVisibility.begin(), impl_->active->signalVisibility.end(),
        [tick](Tick visible) { return visible <= tick; }));
}

auto ExecutionKernel::certifiedPrefixRecords() const noexcept -> std::uint64_t {
    for (auto chunk = impl_->logTail.get(); chunk; chunk = chunk->previous.get())
        if (chunk->baseState)
            return static_cast<std::uint64_t>(chunk->baseRecords);
    return 0;
}
auto ExecutionKernel::nativePrefixRecords() const noexcept -> std::uint64_t {
    return impl_->nativeBaseRecords;
}

auto ExecutionKernel::checkpointCertificate(const ReplayArchive& archive, ReplayCut cut,
                                            Tick horizon,
                                            std::shared_ptr<const SnapshotDTO> state) const
    -> std::shared_ptr<const CheckpointCertificate> {
    auto cert = std::make_shared<CheckpointCertificate>(
        CheckpointCertificate{archive.anchor(), cut, horizon, std::move(state)});
    // Ordinary legacy certificates retain their existing behavior. Partial checkpoints and
    // external mutable owners continue through the original validation path.
    if (!archive.internallySealed_ || !cert->state->kernel.rpa || cut.partialHorizon ||
        !cut.completeRecords || cut.completeRecords > archive.data().records.size() ||
        archive.data().records[static_cast<std::size_t>(cut.completeRecords - 1)].horizon !=
            horizon ||
        impl_->testControls || !impl_->active->signalVisibility.empty() ||
        impl_->ingress.lastObservedTick_ || impl_->ingress.nextObservationId_ ||
        impl_->ingress.observationIdsExhausted_ || !impl_->ingress.admittedSubjects_.empty() ||
        !impl_->ingress.ownedSubjects_.empty() || !impl_->ingress.admittedSequences_.empty() ||
        cert->state->rpaHistory.size() != cut.completeRecords)
        return cert;
    try {
        auto identity = archive.canonicalIdentity();
        if (!identity)
            return cert; // Optional certification failure is a miss, not a kernel fault.
        cert->canonicalIdentity = *identity;
        cert->prefixEventCount = impl_->recording->eventCount;
        cert->rpaStateVersion = 1;
    } catch (const std::exception&) {
        // The required owning checkpoint still succeeds when the optional certificate cannot.
    }
    return cert;
}

auto ExecutionKernel::restoreCertifiedRpa(const CheckpointCertificate& cert,
                                          const KernelTestControls* controls) noexcept -> bool try {
    const auto& dto = *cert.state;
    SharedSequence<MatcherState> matchers;
    matchers.reserve(dto.matchers.size());
    for (const auto& saved : dto.matchers) {
        MatcherState matcher{{}, saved.epsilonAttempted};
        matcher.states.reserve(saved.states.size());
        for (const auto state : saved.states)
            matcher.states.push_back(static_cast<std::size_t>(state));
        matchers.push_back(std::move(matcher));
    }
    auto state = std::make_shared<RuntimeState>(dto.kernel, std::move(matchers));
    state->projection.runScope = impl_->active->projection.runScope;
    state->coverageHistory = dto.coverageHistory;
    state->activeRequirements.reserve(dto.activeRequirements.size());
    for (const auto index : dto.activeRequirements)
        state->activeRequirements.push_back(static_cast<std::size_t>(index));
    state->timerCursor = static_cast<std::size_t>(dto.timerCursor);
    state->nextCommitId = dto.nextCommitId;
    state->idsExhausted = dto.commitIdsExhausted;
    auto pending = dto.pending;
    if (controls && controls->failCertifiedRestore)
        throw std::bad_alloc{};
    auto reserve = impl_->reserveFault(*state);
    auto base = std::make_shared<ExecutionLogChunk>();
    base->baseState = cert.state;
    base->baseRecords = static_cast<std::size_t>(cert.cut.completeRecords);
    auto recording = std::make_shared<const ReplayData>(
        ReplayData{{}, cert.prefixEventCount, dto.kernel.judgementIdentity});
    // Every fallible copy/reserve precedes publication. Legacy ingress remains at its
    // prepared defaults: ordinary RPA admission stores its accepted keys in the projection.
    impl_->pending = std::move(pending);
    impl_->faultReserve = std::move(reserve);
    impl_->active = std::move(state);
    impl_->recording = std::move(recording);
    impl_->logTail = std::move(base);
    impl_->appliedRecords = cert.cut.completeRecords;
    // This journal borrows a checkpoint DTO. Exported native anchors must not extend its
    // lifetime after the run retires; ordinary recovery and the explicit hint remain valid.
    impl_->nativeAnchorEligible = false;
    return true;
} catch (const std::exception&) {
    return false;
}

} // namespace cuexis::judgement::detail
