#include "session_worker.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <type_traits>

namespace cuexis::playback::detail {
namespace {
auto workerError(std::string_view code, std::string_view message) -> core::Error {
    return {"host.rpa." + std::string{code}, std::string{message}};
}
template <typename T> auto reply(core::Result<T>&& result) -> core::Result<SessionReply> {
    if (!result)
        return core::unexpected(std::move(result.error()));
    if constexpr (std::is_void_v<T>)
        return SessionReply{std::monostate{}};
    else
        return SessionReply{std::move(*result)};
}
auto monotonicNow() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
struct CoherentAdvanceRecord final {
    SessionCoherentAdvance request;
};
struct SessionOwner final {
    PlaybackSession session;
    std::optional<PreparedPlayback> prepared;
    std::optional<PresentationCandidateInput> candidate;
    std::optional<PresentationCandidateInput> activeCandidate;
    std::optional<OwnerStepLedger> software;
    std::optional<FrameObservationLedger> frames;
    std::optional<HostContext> frameContext;
    std::uint64_t projectionScope{}, lastFrameId{};
    std::optional<std::int64_t> lastPresentationTick;
    std::optional<SessionRealtimeStart> realtime;
    std::vector<std::optional<CoherentAdvanceRecord>> coherent;
    std::optional<GameplayExecutionContext> coherentIdentity;
    SessionWorkerConfiguration& configuration;
    std::shared_ptr<FrameDelivery> delivery;
    std::optional<PresentationSourceInfo> currentSource;
    std::vector<FeedbackId> currentMembership;
    std::uint64_t membershipScope{};
    enum class FramePath { unselected, legacy, requested };
    FramePath framePath{FramePath::unselected};
    SessionOwner(SessionWorkerConfiguration& config, std::shared_ptr<FrameDelivery> frameDelivery)
        : configuration(config), delivery(std::move(frameDelivery)) {}
    [[nodiscard]] auto bindFrames(const HostContext& context, std::uint64_t scope)
        -> core::Result<void> {
        auto valid = HostCoordinationAccess::validateFrameContext(session, context);
        if (!valid)
            return valid;
        if (!scope)
            return core::unexpected(workerError("context_stale", "Projection scope absent"));
        if (!frames) {
            auto created = FrameObservationLedger::create(
                context, scope, configuration.frameCapacity, configuration.feedbackCapacity);
            if (!created)
                return core::unexpected(created.error());
            auto configured = created->configure(configuration.observation);
            if (!configured)
                return configured;
            frames.emplace(std::move(*created));
        } else if (!frameContext || *frameContext != context || projectionScope != scope) {
            auto bound = projectionScope == scope ? frames->rebind(context)
                                                  : frames->activate(context, scope);
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
    [[nodiscard]] auto updatePresentation(const SessionUpdatePresentation& job)
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
    [[nodiscard]] auto refreshMembership(const PresentationSourceInfo& source, std::uint64_t scope)
        -> core::Result<void> {
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
    [[nodiscard]] auto layout() -> core::Result<FrameDeliveryLayout> {
        auto info = session.chartInfo();
        if (!info)
            return core::unexpected(info.error());
        const auto resources = activeCandidate ? activeCandidate->resources.size() : 0;
        auto bound = frameByteBound(info->objectCount, resources, configuration.maxFrameMembers);
        if (!bound)
            return core::unexpected(bound.error());
        return FrameDeliveryLayout{*bound, info->objectCount, configuration.maxFrameMembers};
    }
    [[nodiscard]] auto requestFrame(const FrameRequest& request) -> core::Result<DeliveryInfo> try {
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
            auto extracted =
                request.frozen
                    ? rematerializeRetainedSnapshot(original->snapshotView(), request.viewport)
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
        return delivery->finishRequest(request.consumerId, request.requestId,
                                       DeliveryStatus::rejected);
    }
    void reclaimCoherent(const AdvanceRetentionProgress& progress) noexcept {
        const bool replaced = !coherentIdentity || *coherentIdentity != progress.identityContext;
        for (auto& record : coherent)
            if (record && (replaced || (progress.reclaimedThrough &&
                                        record->request.requestId <= *progress.reclaimedThrough)))
                record.reset();
        coherentIdentity = progress.identityContext;
    }
    [[nodiscard]] auto ledger(std::uint64_t scope) -> core::Result<void> {
        if (software)
            return {};
        auto created = OwnerStepLedger::create(
            scope, Participant::software, configuration.stepCapacity, configuration.stepReserve);
        if (!created)
            return core::unexpected(created.error());
        software.emplace(std::move(*created));
        return {};
    }
    [[nodiscard]] auto fence(const ControlReceipt& proof) -> core::Result<CommitFence> {
        auto existing = HostCoordinationAccess::currentFence(session);
        if (existing)
            return existing;
        if (proof.request.kind == ControlKind::load || proof.request.kind == ControlKind::reload) {
            if (!candidate)
                return core::unexpected(workerError("context_stale", "Owning candidate is absent"));
            return HostCoordinationAccess::enterFence(session, *candidate);
        }
        return HostCoordinationAccess::enterControlFence(session, proof);
    }
    [[nodiscard]] auto simpleStep(const StepRequest& request, bool retired = false)
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
    [[nodiscard]] auto dispatch(SessionCommand&& command) -> core::Result<SessionReply> {
        return std::visit(
            [&]<typename T>(T& job) -> core::Result<SessionReply> {
                if constexpr (std::is_same_v<T, SessionInspect>) {
                    auto state = session.state();
                    auto generation = HostCoordinationAccess::generation(session);
                    if (!state)
                        return core::unexpected(state.error());
                    if (!generation)
                        return core::unexpected(generation.error());
                    auto logical = HostCoordinationAccess::reconciliationOrdinal(session);
                    auto scope = HostCoordinationAccess::sessionScope(session);
                    if (!scope)
                        return core::unexpected(scope.error());
                    return SessionReply{SessionInspection{
                        *state, *generation, std::this_thread::get_id(),
                        logical ? std::optional<std::uint64_t>{*logical} : std::nullopt, *scope}};
                } else if constexpr (std::is_same_v<T, SessionGameplayResult>) {
                    auto result = session.queryGameplay();
                    if (!result)
                        return core::unexpected(result.error());
                    return SessionReply{std::move(*result)};
                } else if constexpr (std::is_same_v<T, SessionPrepareSource>) {
                    if (prepared)
                        return core::unexpected(
                            workerError("control_pending", "A Prepared candidate is retained"));
                    core::Result<PreparedPlayback> result =
                        job.gameplay
                            ? (job.reload ? session.prepareGameplayReload(
                                                std::move(job.source), *job.gameplay, *job.reload)
                                          : session.prepareGameplayLoad(std::move(job.source),
                                                                        job.mode, *job.gameplay))
                            : (job.reload ? session.prepareReload(std::move(job.source),
                                                                  job.targetFrame, *job.reload)
                                          : session.prepareLoad(std::move(job.source), job.mode));
                    if (!result)
                        return core::unexpected(std::move(result.error()));
                    auto input = HostCoordinationAccess::materialize(
                        *result, job.capabilities, job.presentation, job.rendererInstance,
                        job.rendererGeneration, job.controlSequence);
                    if (!input)
                        return core::unexpected(input.error());
                    auto reserved = ledger(input->context.sessionScope);
                    if (!reserved)
                        return core::unexpected(reserved.error());
                    prepared.emplace(std::move(*result));
                    candidate = *input;
                    return SessionReply{std::move(*input)};
                } else if constexpr (std::is_same_v<T, SessionAcceptControl>) {
                    auto reserved = ledger(job.request.context.sessionScope);
                    if (!reserved)
                        return core::unexpected(reserved.error());
                    return reply(
                        HostCoordinationAccess::acceptControlAtOwner(session, job.request));
                } else if constexpr (std::is_same_v<T, SessionRealtimeStart>) {
                    if (!job.profile.retainedMessageCapacity ||
                        *job.profile.retainedMessageCapacity > SIZE_MAX)
                        return core::unexpected(
                            workerError("profile_incomplete", "Coherent command capacity missing"));
                    std::vector<std::optional<CoherentAdvanceRecord>> reserved(
                        static_cast<std::size_t>(*job.profile.retainedMessageCapacity));
                    auto started = session.beginRealtimeGameplay(job.profile, job.mapping,
                                                                 job.context, job.monotonicDomain);
                    if (started) {
                        realtime = job;
                        coherent = std::move(reserved);
                    }
                    return reply(std::move(started));
                } else if constexpr (std::is_same_v<T, SessionCaptureDelivery>) {
                    return reply(session.deliverGameplayCapture(job.delivery, job.clock, now()));
                } else if constexpr (std::is_same_v<T, SessionAdvance>) {
                    return reply(session.requestGameplayAdvance(job.request, job.clock, now()));
                } else if constexpr (std::is_same_v<T, SessionTailStart>) {
                    return reply(
                        session.beginRealtimeTail(job.eof, job.segment, job.profile, now()));
                } else if constexpr (std::is_same_v<T, SessionCoherentAdvance>) {
                    auto progress = HostCoordinationAccess::advanceRetention(session);
                    if (!progress)
                        return core::unexpected(progress.error());
                    reclaimCoherent(*progress);
                    if (progress->reclaimedThrough && job.requestId <= *progress->reclaimedThrough)
                        return core::unexpected(
                            workerError("request_retired", "Request identity reclaimed"));
                    for (const auto& record : coherent) {
                        if (!record || record->request.requestId != job.requestId)
                            continue;
                        if (record->request != job) {
                            if (software)
                                software->reportConflict();
                            auto revoked =
                                HostCoordinationAccess::revokeRealtimePermission(session);
                            if (!revoked)
                                return core::unexpected(revoked.error());
                            return core::unexpected(workerError(
                                "message_conflict", "Coherent request payload conflict"));
                        }
                        return reply(session.queryGameplayAdvance(job.requestId));
                    }
                    auto slot = std::find_if(coherent.begin(), coherent.end(),
                                             [](const auto& entry) { return !entry; });
                    if (slot == coherent.end())
                        return core::unexpected(
                            workerError("capacity_exceeded", "Coherent identity budget exhausted"));
                    slot->emplace(CoherentAdvanceRecord{job});
                    const auto actualNow = now();
                    auto mapped =
                        HostCoordinationAccess::mapRealtimeClock(session, job.clock, actualNow);
                    if (!mapped) {
                        slot->reset();
                        return core::unexpected(mapped.error());
                    }
                    auto advanced = session.requestGameplayAdvance(
                        {job.context, job.requestId, job.commandOrdinal, {mapped->horizon}},
                        job.clock, actualNow);
                    if (!advanced ||
                        advanced->disposition == GameplayAdvanceDisposition::BusyNotAccepted) {
                        auto retained = HostCoordinationAccess::advanceRetention(session);
                        if (!retained)
                            return core::unexpected(retained.error());
                        if (retained->lastAcceptedId != job.requestId)
                            slot->reset();
                    }
                    return reply(std::move(advanced));

                } else if constexpr (std::is_same_v<T, SessionQueryAdvance>) {
                    return reply(session.queryGameplayAdvance(job.requestId));
                } else if constexpr (std::is_same_v<T, SessionRetireAdvance>) {
                    auto retired = session.retireGameplayAdvance(job.requestId);
                    if (retired) {
                        auto progress = HostCoordinationAccess::advanceRetention(session);
                        if (!progress)
                            return core::unexpected(progress.error());
                        reclaimCoherent(*progress);
                    }
                    return reply(std::move(retired));
                } else if constexpr (std::is_same_v<T, SessionConfirmControl>) {
                    auto current = HostCoordinationAccess::currentFence(session);
                    if (!current) {
                        if (job.proof.request.kind != ControlKind::close ||
                            !controlFullyApplied(job.proof) ||
                            session.state() != SessionState::Empty || !software)
                            return core::unexpected(current.error());
                        const auto& closed =
                            job.proof.steps[static_cast<unsigned>(Participant::software)]
                                           [static_cast<unsigned>(StepPhase::close)];
                        if (!closed)
                            return core::unexpected(
                                workerError("device_pending", "Software close proof absent"));
                        auto binding = HostCoordinationAccess::validateControlBinding(
                            session, closed->request);
                        if (!binding)
                            return core::unexpected(binding.error());
                        auto actual = software->query(closed->request.key);
                        if (!actual || actual->status != StepStatus::applied ||
                            actual->request != closed->request ||
                            actual->effects != closed->effects)
                            return core::unexpected(workerError(
                                "device_pending", "Software close differs from owner history"));
                        return SessionReply{std::monostate{}};
                    }
                    if (job.proof.request.kind == ControlKind::close &&
                        job.proof.request.context.controlSequence != current->controlSequence())
                        return reply(
                            HostCoordinationAccess::completeClose(session, *current, job.proof));
                    auto released =
                        HostCoordinationAccess::leaveFence(session, *current, job.proof);
                    if (!released)
                        return core::unexpected(released.error());
                    if (frames && job.projectionScope && job.proof.finalizedContext) {
                        auto bound = job.projectionScope == projectionScope
                                         ? frames->rebind(*job.proof.finalizedContext)
                                         : frames->activate(*job.proof.finalizedContext,
                                                            job.projectionScope);
                        if (!bound)
                            return core::unexpected(bound.error());
                        if (job.projectionScope != projectionScope)
                            lastPresentationTick.reset();
                        projectionScope = job.projectionScope;
                        frameContext = *job.proof.finalizedContext;
                    }
                    currentSource.reset();
                    currentMembership.clear();
                    membershipScope = 0;
                    if (realtime && frames && job.projectionScope) {
                        auto source = session.presentationSource();
                        if (source) {
                            auto refreshed = refreshMembership(*source, job.projectionScope);
                            if (!refreshed)
                                return core::unexpected(refreshed.error());
                        }
                    }
                    return SessionReply{std::monostate{}};
                } else if constexpr (std::is_same_v<T, SessionUpdatePresentation>) {
                    return reply(updatePresentation(job));
                } else if constexpr (std::is_same_v<T, SessionDeliveryLayout>) {
                    return reply(layout());
                } else if constexpr (std::is_same_v<T, SessionRegisterConsumer>) {
                    auto valid = HostCoordinationAccess::validateFrameContext(session, job.context);
                    if (!valid)
                        return core::unexpected(valid.error());
                    return reply(
                        delivery->registerConsumer(job.operationId, job.context, job.profile));
                } else if constexpr (std::is_same_v<T, SessionConfirmConsumer>) {
                    if (!job.revoke) {
                        auto valid = HostCoordinationAccess::validateFrameContext(
                            session, job.receipt.registration.context);
                        if (!valid) {
                            auto retiring =
                                delivery->beginRevoke(job.receipt.registration.consumerId);
                            if (!retiring)
                                return core::unexpected(retiring.error());
                            return core::unexpected(valid.error());
                        }
                    }
                    return reply(job.revoke ? delivery->confirmRevoke(job.receipt)
                                            : delivery->confirmRegistration(job.receipt));
                } else if constexpr (std::is_same_v<T, SessionRevokeConsumer>) {
                    return reply(delivery->beginRevoke(job.consumerId));
                } else if constexpr (std::is_same_v<T, SessionRequestFrame>) {
                    return reply(requestFrame(job.request));
                } else if constexpr (std::is_same_v<T, SessionRevokeRetained>) {
                    delivery->cancelRetained(job.consumerId, job.serial);
                    return SessionReply{std::monostate{}};
                } else if constexpr (std::is_same_v<T, SessionFeedbackQuery>) {
                    if (!frames)
                        return core::unexpected(
                            workerError("context_stale", "Observation history unavailable"));
                    return reply(frames->query(job.id));
                } else if constexpr (std::is_same_v<T, SessionObservationQuery>) {
                    if (!frames)
                        return SessionReply{ObservationInfo{}};
                    const auto coverage = frames->coverage();
                    return SessionReply{
                        ObservationInfo{frames->retainedAttempts(),
                                        frames->journalBytes(),
                                        frames->peakJournalBytes(),
                                        coverage.retainedFirst,
                                        coverage.retainedLast,
                                        coverage.lossEvents,
                                        {coverage.incomplete.begin(), coverage.incomplete.end()},
                                        coverage.exactLossCount,
                                        coverage.attemptsEvicted,
                                        coverage.feedbackEvicted,
                                        coverage.receiptLoss,
                                        coverage.historyMisses}};
                } else if constexpr (std::is_same_v<T, SessionDeliveryMetrics>) {
                    return SessionReply{delivery->storageMetrics()};
                } else if constexpr (std::is_same_v<T, SessionSample>) {
                    if (framePath == FramePath::requested)
                        return core::unexpected(workerError(
                            "context_stale", "New session required for legacy fallback"));
                    framePath = FramePath::legacy;
                    auto valid = HostCoordinationAccess::validateFrameContext(session, job.context);
                    if (!valid)
                        return core::unexpected(valid.error());
                    if (!job.frameId || !job.projectionScope || job.frameId <= lastFrameId)
                        return core::unexpected(workerError(
                            "context_stale", "Frame identity or projection scope stale"));
                    FrameEnvelope frame;
                    if (job.frozen) {
                        const auto& retained = job.retained.frame();
                        if (!retained || retained->projectionScope != job.projectionScope ||
                            retained->context.sessionScope != job.context.sessionScope ||
                            retained->context.contentIdentity != job.context.contentIdentity ||
                            retained->context.resourceGeneration !=
                                job.context.resourceGeneration ||
                            retained->context.rendererGeneration != job.context.rendererGeneration)
                            return core::unexpected(
                                workerError("context_stale", "No eligible retained presentation"));
                        frame.source = retained->source;
                        frame.presentationTick = retained->presentationTick;
                        frame.runtimeFrame = retained->runtimeFrame;
                        frame.foldedFactCursor = retained->foldedFactCursor;
                        frame.foldedWorkTick = retained->foldedWorkTick;
                        frame.membership = retained->membershipView();
                        frame.resources = retained->resourcesView();
                        const auto& snapshot = retained->snapshotView();
                        if (snapshot.viewportWidth == job.viewport.width &&
                            snapshot.viewportHeight == job.viewport.height) {
                            frame.retainedSnapshot =
                                retained->payload
                                    ? std::shared_ptr<const FrameSnapshot>{retained->payload,
                                                                           &retained->payload
                                                                                ->snapshot}
                                : retained->retainedSnapshot ? retained->retainedSnapshot
                                                             : std::shared_ptr<const FrameSnapshot>{
                                                                   retained, &retained->snapshot};
                        } else {
                            auto resized = rematerializeRetainedSnapshot(snapshot, job.viewport);
                            if (!resized)
                                return core::unexpected(resized.error());
                            frame.retainedSnapshot =
                                std::make_shared<const FrameSnapshot>(std::move(*resized));
                        }
                    } else {
                        if (frameContext && projectionScope == job.projectionScope &&
                            lastPresentationTick && job.presentationTick < *lastPresentationTick)
                            return core::unexpected(
                                workerError("context_stale", "Source time stale"));
                        auto sampled = updatePresentation({job.context, job.projectionScope,
                                                           job.presentationTick, job.runtimeFrame,
                                                           job.realtime});
                        if (!sampled)
                            return core::unexpected(sampled.error());
                        auto source = session.presentationSource();
                        if (!source)
                            return core::unexpected(source.error());
                        auto snapshot = session.extractFrame(job.viewport);
                        if (!snapshot)
                            return core::unexpected(snapshot.error());
                        frame.source = *source;
                        frame.presentationTick = source->presentationTick;
                        if (source->frame)
                            frame.runtimeFrame = *source->frame;
                        frame.foldedFactCursor = source->factCursor;
                        frame.foldedWorkTick = source->workTick;
                        frame.snapshot = std::move(*snapshot);
                        if (activeCandidate)
                            frame.resources = activeCandidate->resources;
                    }
                    frame.context = job.context;
                    frame.frameId = job.frameId;
                    frame.projectionScope = job.projectionScope;
                    if (!frames) {
                        auto created = FrameObservationLedger::create(
                            frame.context, frame.projectionScope, configuration.frameCapacity,
                            configuration.feedbackCapacity);
                        if (!created)
                            return core::unexpected(created.error());
                        frames.emplace(std::move(*created));
                        frameContext = frame.context;
                        projectionScope = frame.projectionScope;
                    } else if (*frameContext != frame.context ||
                               projectionScope != frame.projectionScope) {
                        auto bound = projectionScope == frame.projectionScope
                                         ? frames->rebind(frame.context)
                                         : frames->activate(frame.context, frame.projectionScope);
                        if (!bound)
                            return core::unexpected(bound.error());
                        frameContext = frame.context;
                        projectionScope = frame.projectionScope;
                        if (!job.frozen)
                            lastPresentationTick.reset();
                    }
                    if (job.realtime && !job.frozen) {
                        auto members = HostCoordinationAccess::feedbackMembership(
                            session, frame.projectionScope);
                        if (!members)
                            return core::unexpected(members.error());
                        for (const auto& member : *members) {
                            auto created =
                                frames->feedback(member.id, member.created, member.expiry);
                            if (!created)
                                return core::unexpected(created.error());
                            frame.membership.push_back(member.id);
                        }
                    }
                    auto published = job.frozen ? frames->publishRetained(frame, job.retained)
                                                : frames->publish(frame);
                    if (!published)
                        return core::unexpected(published.error());
                    lastFrameId = job.frameId;
                    if (!job.frozen)
                        lastPresentationTick = frame.presentationTick;
                    auto expired = job.frozen || !frame.presentationTick
                                       ? core::Result<void>{}
                                       : frames->expire(*frame.presentationTick);
                    if (!expired)
                        return core::unexpected(expired.error());
                    auto latest = frames->takeLatest();
                    if (!latest)
                        return core::unexpected(latest.error());
                    if (!*latest)
                        return core::unexpected(
                            workerError("context_stale", "Published frame missing"));
                    return SessionReply{std::move(**latest)};
                } else if constexpr (std::is_same_v<T, SessionPresentReceipt>) {
                    if (!frames)
                        return core::unexpected(
                            workerError("context_stale", "Frame ledger absent"));
                    return reply(frames->observe(job.receipt));
                } else if constexpr (std::is_same_v<T, SessionEvaluateReplay>) {
                    return reply(session.evaluateGameplayReplay(job.archive));
                } else if constexpr (std::is_same_v<T, SessionLag>) {
                    return reply(session.gameplayExecutionLag(job.horizon));
                } else if constexpr (std::is_same_v<T, SessionArchive>) {
                    return reply(session.archiveGameplay());
                } else if constexpr (std::is_same_v<T, SessionSnapshot>) {
                    return reply(session.snapshotGameplay());
                } else if constexpr (std::is_same_v<T, SessionArchiveCut>) {
                    return reply(
                        HostCoordinationAccess::archiveCut(session, job.archive, job.horizon));
                } else {
                    if (!software)
                        return core::unexpected(
                            workerError("context_stale", "Software ledger absent"));
                    if constexpr (std::is_same_v<T, SessionQueryStep>) {
                        return reply(software->query(job.key));
                    } else if constexpr (std::is_same_v<T, SessionCancelStep>) {
                        auto canceled = software->cancel(job.request);
                        if (canceled && canceled->status == StepStatus::canceledNotApplied &&
                            candidate && job.request.target.candidateId == candidate->candidateId) {
                            prepared.reset();
                            candidate.reset();
                        }
                        return reply(std::move(canceled));
                    } else if constexpr (std::is_same_v<T, SessionSoftwarePrepare>) {
                        auto valid =
                            HostCoordinationAccess::validateControlBinding(session, job.request);
                        if (!valid)
                            return core::unexpected(valid.error());
                        if (job.request.key.phase != StepPhase::prepare)
                            return core::unexpected(
                                workerError("context_stale", "Software phase mismatch"));
                        auto old = software->query(job.request.key);
                        if (old && old->status != StepStatus::pending)
                            return reply(software->apply(job.request));
                        auto generation = HostCoordinationAccess::generation(session);
                        if (!generation ||
                            *generation != generationFor(job.request.target, Participant::software))
                            return core::unexpected(
                                workerError("context_stale", "Software generation changed"));
                        if (prepared && (!candidate ||
                                         job.request.target.candidateId != candidate->candidateId ||
                                         candidate->mutationGeneration != *generation))
                            return core::unexpected(
                                workerError("context_stale", "Software candidate mismatch"));
                        return reply(simpleStep(job.request));
                    } else if constexpr (std::is_same_v<T, SessionSoftwareFence>) {
                        auto binding =
                            HostCoordinationAccess::validateControlBinding(session, job.request);
                        if (!binding)
                            return core::unexpected(binding.error());
                        if (job.request.key.phase != StepPhase::fence ||
                            job.request.target != job.proof.request.target)
                            return core::unexpected(
                                workerError("context_stale", "Software fence target mismatch"));
                        auto old = software->query(job.request.key);
                        if (old && old->status != StepStatus::pending)
                            return reply(software->apply(job.request));
                        auto entered = fence(job.proof);
                        if (!entered)
                            return core::unexpected(entered.error());
                        return reply(simpleStep(job.request));
                    } else if constexpr (std::is_same_v<T, SessionSoftwareCommit>) {
                        auto old = software->query(job.request.key);
                        if (old && old->status != StepStatus::pending)
                            return reply(software->apply(job.request));
                        auto current = HostCoordinationAccess::currentFence(session);
                        if (!current)
                            return core::unexpected(current.error());
                        if (!prepared)
                            return core::unexpected(
                                workerError("context_stale", "Prepared candidate absent"));
                        auto committed = HostCoordinationAccess::commitOnce(
                            session, *software, job.request, std::move(*prepared), *current,
                            job.proof);
                        auto actualGeneration = HostCoordinationAccess::generation(session);
                        if (candidate && actualGeneration &&
                            *actualGeneration != candidate->mutationGeneration &&
                            session.state() == SessionState::Ready)
                            activeCandidate = std::move(candidate);
                        auto actualStep = software->query(job.request.key);
                        if (!prepared->valid() ||
                            (actualStep && actualStep->status != StepStatus::pending))
                            prepared.reset();
                        return reply(std::move(committed));
                    } else if constexpr (std::is_same_v<T, SessionPause>) {
                        auto old = software->query(job.request.key);
                        if (old && old->status != StepStatus::pending)
                            return reply(HostCoordinationAccess::applyPause(session, *software,
                                                                            job.request, now()));
                        auto entered = fence(job.proof);
                        if (!entered)
                            return core::unexpected(entered.error());
                        return reply(HostCoordinationAccess::applyPause(session, *software,
                                                                        job.request, now()));
                    } else if constexpr (std::is_same_v<T, SessionResume>) {
                        auto old = software->query(job.request.key);
                        if (old && old->status != StepStatus::pending)
                            return reply(HostCoordinationAccess::applyResume(
                                session, *software, job.request, job.reconciliation, job.context,
                                job.clock, now(), &job.routeReceipt));
                        auto entered = fence(job.proof);
                        if (!entered)
                            return core::unexpected(entered.error());
                        return reply(HostCoordinationAccess::applyResume(
                            session, *software, job.request, job.reconciliation, job.context,
                            job.clock, now(), &job.routeReceipt));
                    } else if constexpr (std::is_same_v<T, SessionSeek>) {
                        auto old = software->query(job.request.key);
                        if (old && old->status != StepStatus::pending && realtime)
                            return reply(HostCoordinationAccess::applySeek(
                                session, *software, job.request, job.proof, job.archive,
                                job.horizon, job.cut, job.checkpoints, job.presentationTick,
                                job.runtimeFrame, realtime->profile, realtime->mapping,
                                realtime->monotonicDomain));
                        auto entered = fence(job.proof);
                        if (!entered)
                            return core::unexpected(entered.error());
                        if (!realtime)
                            return core::unexpected(
                                workerError("profile_incomplete", "Seek realtime profile absent"));
                        return reply(HostCoordinationAccess::applySeek(
                            session, *software, job.request, job.proof, job.archive, job.horizon,
                            job.cut, job.checkpoints, job.presentationTick, job.runtimeFrame,
                            realtime->profile, realtime->mapping, realtime->monotonicDomain));
                    } else if constexpr (std::is_same_v<T, SessionSoftwareRetire>) {
                        auto entered = fence(job.proof);
                        if (!entered)
                            return core::unexpected(entered.error());
                        auto retired = HostCoordinationAccess::retireSoftware(
                            session, *software, job.request, job.proof);
                        if (session.state() == SessionState::Empty) {
                            prepared.reset();
                            candidate.reset();
                            activeCandidate.reset();
                        }
                        return reply(std::move(retired));
                    } else if constexpr (std::is_same_v<T, SessionSoftwareClose>) {
                        auto binding =
                            HostCoordinationAccess::validateControlBinding(session, job.request);
                        if (!binding)
                            return core::unexpected(binding.error());
                        if (job.request.key.phase != StepPhase::close ||
                            job.proof.request.kind != ControlKind::close ||
                            job.request.target != job.proof.request.target ||
                            session.state() != SessionState::Empty)
                            return core::unexpected(
                                workerError("device_pending", "Software is not retired"));
                        auto retiredKey = job.request.key;
                        retiredKey.phase = StepPhase::retire;
                        auto retired = software->query(retiredKey);
                        if (!retired || retired->status != StepStatus::applied ||
                            !retired->effects.referencesReleased)
                            return core::unexpected(workerError(
                                "device_pending", "Software retirement is unconfirmed"));
                        candidate.reset();
                        prepared.reset();
                        return reply(simpleStep(job.request, true));
                    }
                }
            },
            command);
    }
    [[nodiscard]] auto now() -> std::int64_t {
        return configuration.monotonicNow ? configuration.monotonicNow() : monotonicNow();
    }
};
} // namespace
SessionWorker::SessionWorker(SessionWorkerConfiguration configuration)
    : configuration_(std::move(configuration)),
      channel_(configuration_.capacity, configuration_.controlReserve),
      receipts_(configuration_.receiptCapacity) {}
auto SessionWorker::create(SessionWorkerConfiguration configuration)
    -> core::Result<std::unique_ptr<SessionWorker>> try {
    if (!configuration.testOnly || configuration.capacity <= configuration.controlReserve ||
        !configuration.controlReserve || configuration.stepCapacity <= configuration.stepReserve ||
        !configuration.stepReserve || !configuration.frameCapacity ||
        !configuration.feedbackCapacity || !configuration.receiptCapacity)
        return core::unexpected(
            workerError("profile_incomplete", "Explicit test-only worker capacities required"));
    auto worker = std::unique_ptr<SessionWorker>{new SessionWorker{std::move(configuration)}};
    auto delivery = FrameDelivery::create(worker->configuration_.delivery);
    if (!delivery)
        return core::unexpected(delivery.error());
    worker->delivery_ = std::move(*delivery);
    worker->thread_ = std::thread{[ptr = worker.get()] { ptr->run(); }};
    return worker;
} catch (...) {
    return core::unexpected(workerError("capacity_exceeded", "Worker startup failed"));
}
SessionWorker::~SessionWorker() noexcept {
    requestShutdown();
    if (thread_.joinable())
        thread_.join();
}
auto SessionWorker::post(SessionCommand command, bool control) -> core::Result<std::uint64_t> {
    if (shutdown_.load(std::memory_order_acquire))
        return core::unexpected(workerError("owner_closed", "Worker admission is closed"));
    return channel_.post(std::move(command), control);
}
auto SessionWorker::poll(std::uint64_t id)
    -> core::Result<std::optional<core::Result<SessionReply>>> {
    return channel_.poll(id);
}
auto SessionWorker::acknowledge(std::uint64_t id) -> core::Result<void> {
    return channel_.ack(id);
}
auto SessionWorker::takeDelivery(std::uint64_t consumer, std::uint64_t request)
    -> core::Result<std::optional<FrameEnvelope>> {
    return delivery_->take(consumer, request,
                           configuration_.monotonicNow ? configuration_.monotonicNow()
                                                       : monotonicNow());
}
auto SessionWorker::pollDelivery(std::uint64_t consumer, std::uint64_t request)
    -> core::Result<DeliveryInfo> {
    return delivery_->poll(consumer, request,
                           configuration_.monotonicNow ? configuration_.monotonicNow()
                                                       : monotonicNow());
}
auto SessionWorker::cancelDelivery(std::uint64_t consumer, std::uint64_t request)
    -> core::Result<DeliveryInfo> {
    return delivery_->cancel(consumer, request);
}
void SessionWorker::noteLostReceipt(const PresentReceipt& receipt) noexcept {
    try {
        std::lock_guard lock(receiptMutex_);
        ReceiptLoss incoming{receipt.context.sessionScope,
                             receipt.context.sessionScope,
                             receipt.projectionScope,
                             receipt.projectionScope,
                             receipt.frameId,
                             receipt.frameId};
        auto slot = std::find_if(receiptLosses_.begin(), receiptLosses_.end(), [&](const auto& r) {
            return !r ||
                   (r->sessionFirst == incoming.sessionFirst &&
                    r->sessionLast == incoming.sessionLast &&
                    r->scopeFirst == incoming.scopeFirst && r->scopeLast == incoming.scopeLast);
        });
        if (slot == receiptLosses_.end())
            slot = receiptLosses_.begin();
        if (*slot) {
            auto& r = **slot;
            r.sessionFirst = std::min(r.sessionFirst, incoming.sessionFirst);
            r.sessionLast = std::max(r.sessionLast, incoming.sessionLast);
            r.scopeFirst = std::min(r.scopeFirst, incoming.scopeFirst);
            r.scopeLast = std::max(r.scopeLast, incoming.scopeLast);
            r.frameFirst = std::min(r.frameFirst, incoming.frameFirst);
            r.frameLast = std::max(r.frameLast, incoming.frameLast);
        } else
            *slot = incoming;
    } catch (...) {
    }
    auto count = lostReceipts_.load();
    while (count != UINT64_MAX && !lostReceipts_.compare_exchange_weak(count, count + 1)) {
    }
    channel_.wake();
}
void SessionWorker::postReceipt(const PresentReceipt& receipt) noexcept {
    const auto lost = [&]() noexcept { noteLostReceipt(receipt); };
    try {
        std::size_t errorBytes = 0, depth = 0;
        for (auto* cause = receipt.cause ? &*receipt.cause : nullptr; cause;
             cause = cause->cause()) {
            if (++depth > 16) {
                lost();
                return;
            }
            const auto add = [&](std::size_t bytes) {
                if (bytes > 4096 || errorBytes > 4096 - bytes)
                    return false;
                errorBytes += bytes;
                return true;
            };
            if (!add(cause->code().size()) || !add(cause->message().size())) {
                lost();
                return;
            }
            if (cause->context().size() > 64) {
                lost();
                return;
            }
            for (const auto& context : cause->context())
                if (!add(context.key.size()) || !add(context.value.size())) {
                    lost();
                    return;
                }
        }
        auto bounded = copyPresentReceipt(receipt);
        if (!bounded) {
            lost();
            return;
        }
        {
            std::lock_guard lock(receiptMutex_);
            if (receiptCount_ < receipts_.size() && !shutdown_.load()) {
                receipts_[receiptWrite_].emplace(std::move(*bounded));
                receiptWrite_ = (receiptWrite_ + 1) % receipts_.size();
                ++receiptCount_;
                channel_.wake();
                return;
            }
        }
        lost();
    } catch (...) {
        lost();
    }
}
void SessionWorker::waitFor(std::chrono::milliseconds duration) {
    channel_.waitFor(duration);
}
void SessionWorker::waitForReply(std::uint64_t id, std::chrono::steady_clock::duration duration) {
    channel_.waitForReply(id, duration);
}
void SessionWorker::requestShutdown() noexcept {
    delivery_->closeAdmission();
    channel_.closeAdmission();
    shutdown_.store(true, std::memory_order_release);
    channel_.wake();
}
auto SessionWorker::exited() const noexcept -> bool {
    return exited_.load(std::memory_order_acquire);
}
auto SessionWorker::joinExited() -> core::Result<void> {
    if (!exited())
        return core::unexpected(workerError("control_pending", "Worker still owns its session"));
    if (thread_.joinable())
        thread_.join();
    return {};
}
auto SessionWorker::fault() const noexcept -> OwnerChannel<SessionCommand, SessionReply>::Fault {
    return channel_.fault();
}
void SessionWorker::run() noexcept {
    try {
        SessionOwner owner{configuration_, delivery_};
        auto maintain = [&]() {
            const auto now = owner.now();
            delivery_->expire(now);
            if (owner.frames)
                owner.frames->maintain(now);
            if (lostReceipts_.exchange(0)) {
                std::array<std::optional<ReceiptLoss>, 8> losses;
                {
                    std::lock_guard lock(receiptMutex_);
                    losses = receiptLosses_;
                    receiptLosses_ = {};
                }
                if (owner.frames)
                    for (const auto& r : losses)
                        if (r)
                            owner.frames->receiptLost(r->sessionFirst, r->sessionLast,
                                                      r->scopeFirst, r->scopeLast, r->frameFirst,
                                                      r->frameLast);
            }
            for (std::size_t i = 0; i < 16; ++i) {
                std::optional<PresentReceipt> receipt;
                {
                    std::lock_guard lock(receiptMutex_);
                    if (!receiptCount_)
                        break;
                    receipt = std::move(receipts_[receiptRead_]);
                    receipts_[receiptRead_].reset();
                    receiptRead_ = (receiptRead_ + 1) % receipts_.size();
                    --receiptCount_;
                }
                if (owner.frames) {
                    auto observed = owner.frames->observe(*receipt);
                    if (!observed)
                        owner.frames->historyMiss();
                }
            }
        };
        for (;;) {
            auto next = channel_.takeIf([](const SessionCommand& command) {
                return !std::holds_alternative<SessionRequestFrame>(command);
            });
            if (!next)
                next = channel_.take();
            if (next) {
                try {
                    channel_.complete(next->first, owner.dispatch(std::move(next->second)));
                } catch (...) {
                    channel_.complete(
                        next->first, core::unexpected(workerError("capacity_exceeded",
                                                                  "Worker job allocation failed")));
                }
                maintain();
                continue;
            }
            maintain();
            if (shutdown_.load(std::memory_order_acquire))
                break;
            channel_.waitFor(std::chrono::milliseconds{20});
        }
    } catch (...) {
        channel_.closeAdmission();
        shutdown_.store(true, std::memory_order_release);
        while (auto next = channel_.take())
            channel_.complete(next->first, core::unexpected(workerError(
                                               "owner_closed", "Worker initialization failed")));
    }
    // Every Playback/Prepared/ledger destructor above has run on this owner.
    exited_.store(true, std::memory_order_release);
}
} // namespace cuexis::playback::detail
