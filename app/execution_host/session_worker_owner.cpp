#include "session_worker_internal.hpp"

// Frame binding, presentation membership, coherent advance retention and the software ledger.

namespace cuexis::playback::detail {

using namespace worker_detail;

SessionOwner::SessionOwner(SessionWorkerConfiguration& config,
                           std::shared_ptr<FrameDelivery> frameDelivery)
    : configuration(config), delivery(std::move(frameDelivery)) {}

[[nodiscard]] auto SessionOwner::bindFrames(const HostContext& context, std::uint64_t scope)
    -> core::Result<void> {
    auto valid = HostCoordinationAccess::validateFrameContext(session, context);
    if (!valid)
        return valid;
    if (!scope)
        return core::unexpected(workerError("context_stale", "Projection scope absent"));
    if (!frames) {
        auto created = FrameObservationLedger::create(context, scope, configuration.frameCapacity,
                                                      configuration.feedbackCapacity);
        if (!created)
            return core::unexpected(created.error());
        auto configured = created->configure(configuration.observation);
        if (!configured)
            return configured;
        frames.emplace(std::move(*created));
    } else if (!frameContext || *frameContext != context || projectionScope != scope) {
        auto bound =
            projectionScope == scope ? frames->rebind(context) : frames->activate(context, scope);
        if (!bound)
            return bound;
        if (projectionScope != scope)
            lastPresentationTick.reset();
    }
    frameContext = context;
    projectionScope = scope;
    frames->maintain(now());
    return {};
}

[[nodiscard]] auto SessionOwner::updatePresentation(const SessionUpdatePresentation& job)
    -> core::Result<void> {
    auto bound = bindFrames(job.context, job.projectionScope);
    if (!bound)
        return bound;
    if (lastPresentationTick && job.presentationTick < *lastPresentationTick)
        return core::unexpected(workerError("context_stale", "Source time stale"));
    auto updated = job.realtime ? HostCoordinationAccess::samplePresentation(
                                      session, {job.presentationTick}, job.runtimeFrame)
                                : session.update(job.runtimeFrame);
    if (!updated)
        return updated;
    auto source = session.presentationSource();
    if (!source)
        return core::unexpected(source.error());
    lastPresentationTick = source->presentationTick;
    auto refreshed = refreshMembership(*source, job.projectionScope);
    if (!refreshed)
        return refreshed;
    if (source->presentationTick)
        return frames->expire(*source->presentationTick);
    return {};
}

[[nodiscard]] auto SessionOwner::refreshMembership(const PresentationSourceInfo& source,
                                                   std::uint64_t scope) -> core::Result<void> {
    currentSource = source;
    membershipScope = scope;
    currentMembership.clear();
    if (realtime) {
        auto members = HostCoordinationAccess::feedbackMembership(session, scope);
        if (!members)
            return core::unexpected(members.error());
        for (const auto& m : *members) {
            if (frames) {
                auto added = frames->feedback(m.id, m.created, m.expiry);
                if (!added)
                    return added;
            }
            currentMembership.push_back(m.id);
        }
    }
    return {};
}

[[nodiscard]] auto SessionOwner::layout() -> core::Result<FrameDeliveryLayout> {
    auto info = session.chartInfo();
    if (!info)
        return core::unexpected(info.error());
    const auto resources = activeCandidate ? activeCandidate->resources.size() : 0;
    auto bound = frameByteBound(info->objectCount, resources, configuration.maxFrameMembers);
    if (!bound)
        return core::unexpected(bound.error());
    return FrameDeliveryLayout{*bound, info->objectCount, configuration.maxFrameMembers};
}

[[nodiscard]] auto SessionOwner::requestFrame(const FrameRequest& request)
    -> core::Result<DeliveryInfo> try {
    if (framePath == FramePath::legacy)
        return core::unexpected(
            workerError("context_stale", "New session required for delivery migration"));
    framePath = FramePath::requested;
    auto bound = bindFrames(request.context, request.projectionScope);
    if (!bound)
        return core::unexpected(bound.error());
    auto source = session.presentationSource();
    if (!source)
        return core::unexpected(source.error());
    if (!request.frozen &&
        (!currentSource || currentSource->resultVersion != source->resultVersion ||
         membershipScope != request.projectionScope)) {
        auto refreshed = refreshMembership(*source, request.projectionScope);
        if (!refreshed)
            return core::unexpected(refreshed.error());
    }
    const auto& original = request.retained.frame();
    if (request.frozen && original &&
        (original->projectionScope != request.projectionScope ||
         original->context.sessionScope != request.context.sessionScope ||
         original->context.contentIdentity != request.context.contentIdentity ||
         original->context.resourceGeneration != request.context.resourceGeneration ||
         original->context.rendererGeneration != request.context.rendererGeneration))
        return DeliveryInfo{request.consumerId, request.requestId, 0, 0,
                            DeliveryStatus::noRetainedFrame};
    const auto& members =
        request.frozen && original ? original->membershipView() : currentMembership;
    if (std::any_of(members.begin(), members.end(), [](const auto& id) {
            return id.targetId.size() > 256 || id.bindingId.size() > 256;
        }))
        return DeliveryInfo{request.consumerId, request.requestId, 0, 0,
                            DeliveryStatus::skippedNoCredit};
    const bool reuse = request.frozen && original && original->payload &&
                       original->snapshotView().viewportWidth == request.viewport.width &&
                       original->snapshotView().viewportHeight == request.viewport.height;
    auto admitted = delivery->admit(request, source->resultVersion, now(), reuse);
    if (!admitted)
        return core::unexpected(admitted.error());
    if (!admitted->materialize)
        return admitted->info;
    FrameEnvelope frame;
    frame.attemptLease = FrameAttemptLease::create(std::move(admitted->attempt));
    frame.context = request.context;
    frame.projectionScope = request.projectionScope;
    frame.consumerId = request.consumerId;
    frame.requestId = request.requestId;
    frame.frameId = admitted->info.frameId;
    frame.source =
        request.frozen ? original->source : std::optional<PresentationSourceInfo>{*source};
    frame.presentationTick = frame.source->presentationTick;
    frame.runtimeFrame = frame.source->frame;
    frame.foldedFactCursor = frame.source->factCursor;
    frame.foldedWorkTick = frame.source->workTick;
    if (request.frozen) {
        frame.retainedSerial = request.retained.serial();
        frame.retainedFrameId = original->frameId;
    }
    if (reuse)
        frame.payload = original->payload;
    else {
        if ((request.frozen ? original->membershipView().size() : currentMembership.size()) >
            configuration.maxFrameMembers) {
            return delivery->finishRequest(request.consumerId, request.requestId,
                                           DeliveryStatus::skippedNoCredit);
        }
        auto payload = std::make_shared<FramePayload>();
        payload->storage = std::move(admitted->reservation);
        auto extracted = request.frozen ? rematerializeRetainedSnapshot(original->snapshotView(),
                                                                        request.viewport)
                                        : session.extractFrame(request.viewport);
        if (!extracted)
            return delivery->finishRequest(request.consumerId, request.requestId,
                                           DeliveryStatus::rejected);
        payload->snapshot = std::move(*extracted);
        payload->resources = request.frozen
                                 ? original->resourcesView()
                                 : (activeCandidate ? activeCandidate->resources
                                                    : std::vector<PortableResourcePtr>{});
        payload->membership = request.frozen ? original->membershipView() : currentMembership;
        if (payloadBytes(*payload) > payload->storage.bytes())
            return delivery->finishRequest(request.consumerId, request.requestId,
                                           DeliveryStatus::rejected);
        frame.payload = std::move(payload);
    }
    if (frame.frameId) {
        frame.dispatch = std::make_shared<DispatchToken>();
        auto recorded = frames->recordAttempt(frame);
        if (!recorded)
            return core::unexpected(recorded.error());
    }
    return delivery->publish(request, std::move(frame), now());
} catch (...) {
    return delivery->finishRequest(request.consumerId, request.requestId, DeliveryStatus::rejected);
}

void SessionOwner::reclaimCoherent(const AdvanceRetentionProgress& progress) noexcept {
    const bool replaced = !coherentIdentity || *coherentIdentity != progress.identityContext;
    for (auto& record : coherent)
        if (record && (replaced || (progress.reclaimedThrough &&
                                    record->request.requestId <= *progress.reclaimedThrough)))
            record.reset();
    coherentIdentity = progress.identityContext;
}

[[nodiscard]] auto SessionOwner::ledger(std::uint64_t scope) -> core::Result<void> {
    if (software)
        return {};
    auto created = OwnerStepLedger::create(scope, Participant::software, configuration.stepCapacity,
                                           configuration.stepReserve);
    if (!created)
        return core::unexpected(created.error());
    software.emplace(std::move(*created));
    return {};
}

[[nodiscard]] auto SessionOwner::fence(const ControlReceipt& proof
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                                       ,
                                       rpa::Fence* destination
#endif
                                       ) -> core::Result<CommitFence> {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    auto& observationFence = destination ? *destination : rpaFence;
#endif
    auto existing = HostCoordinationAccess::currentFence(session);
    if (existing) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        observationFence.path = rpa::FencePath::existingFence;
        observationFence.applicability = rpa::FenceApplicability::applicable;
        observationFence.existingFence = true;
        observationFence.existingIdentity = fenceIdentity(*existing);
#endif
        return existing;
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    observationFence.applicability = rpa::FenceApplicability::applicable;
    observationFence.attempted = true;
    observationFence.path =
        proof.request.kind == ControlKind::load || proof.request.kind == ControlKind::reload
            ? rpa::FencePath::candidate
            : rpa::FencePath::normalControl;
    observationFence.rejected = 0;
    auto observation = rpa::FenceScope{&observationFence};
#endif
    if (proof.request.kind == ControlKind::load || proof.request.kind == ControlKind::reload) {
        if (!candidate)
            return core::unexpected(workerError("context_stale", "Owning candidate is absent"));
        auto entered = HostCoordinationAccess::enterFence(session, *candidate);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        observationFence.succeeded = bool(entered);
#endif
        return entered;
    }
    auto entered = HostCoordinationAccess::enterControlFence(session, proof);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    observationFence.succeeded = bool(entered);
#endif
    return entered;
}

[[nodiscard]] auto SessionOwner::simpleStep(const StepRequest& request, bool retired)
    -> core::Result<StepReceipt> {
    auto started = software->start(request);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return software->query(request.key);
    auto generation = HostCoordinationAccess::generation(session);
    if (!generation)
        return core::unexpected(generation.error());
    StepReceipt result{request, StepStatus::applied, {}, {}};
    result.effects.generation = *generation;
    result.effects.callbackIsolated = retired;
    result.effects.referencesReleased = retired;
    auto published = software->complete(result);
    if (!published)
        return core::unexpected(published.error());
    return result;
}

[[nodiscard]] auto SessionOwner::now() -> std::int64_t {
    return configuration.monotonicNow ? configuration.monotonicNow() : monotonicNow();
}

} // namespace cuexis::playback::detail
