#include "session_worker_internal.hpp"

// The session command dispatch table and its per-command receipts.

namespace cuexis::playback::detail {

using namespace worker_detail;

[[nodiscard]] auto SessionOwner::dispatch(SessionCommand&& command) -> core::Result<SessionReply> {
    const internal::pd::memory::ExecutionTagScope memoryScope{
        internal::pd::memory::ownerPlaybackWorker, internal::pd::memory::domainCommandSession};
    return std::visit(
        [&]<typename T>(T& job) -> core::Result<SessionReply> {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            if constexpr (std::is_same_v<T, SessionRpaObservation>) {
                auto observation = rpaClose;
                if (auto current = HostCoordinationAccess::currentFence(session))
                    observation.currentFence = fenceIdentity(*current);
                return SessionReply{std::move(observation)};
            } else if constexpr (std::is_same_v<T, SessionRpaSeekObservation>) {
                // Read-and-clear: the host merges each seek's worker samples once.
                auto ledger = rpaSeek;
                rpaSeek = {};
                return SessionReply{ledger};
            } else
#endif
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
                        ? (job.reload ? session.prepareGameplayReload(std::move(job.source),
                                                                      *job.gameplay, *job.reload)
                                      : session.prepareGameplayLoad(std::move(job.source), job.mode,
                                                                    *job.gameplay))
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
                return reply(HostCoordinationAccess::acceptControlAtOwner(session, job.request));
            } else if constexpr (std::is_same_v<T, SessionRealtimeStart>) {
                if (!job.profile.retainedMessageCapacity ||
                    *job.profile.retainedMessageCapacity > SIZE_MAX)
                    return core::unexpected(
                        workerError("profile_incomplete", "Coherent command capacity missing"));
                std::vector<std::optional<CoherentAdvanceRecord>> reserved(
                    static_cast<std::size_t>(*job.profile.retainedMessageCapacity));
                auto started = session.beginRealtimeGameplay(job.profile, job.mapping, job.context,
                                                             job.monotonicDomain);
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
                return reply(session.beginRealtimeTail(job.eof, job.segment, job.profile, now()));
            } else if constexpr (std::is_same_v<T, SessionCoherentAdvance>) {
                internal::execution_diagnostics::Trace diagnostic{"SessionCoherentAdvance"};
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
                        auto revoked = HostCoordinationAccess::revokeRealtimePermission(session);
                        if (!revoked)
                            return core::unexpected(revoked.error());
                        return core::unexpected(
                            workerError("message_conflict", "Coherent request payload conflict"));
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
                    {job.context, job.requestId, job.commandOrdinal, {mapped->horizon}}, job.clock,
                    actualNow);
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
                if (job.proof.request.kind == ControlKind::close) {
                    const auto& closed =
                        job.proof.steps[static_cast<unsigned>(Participant::software)]
                                       [static_cast<unsigned>(StepPhase::close)];
                    if (!closed)
                        return core::unexpected(
                            workerError("device_pending", "Software close proof absent"));
                    auto binding = HostCoordinationAccess::validateControlProof(
                        session, closed->request, job.proof);
                    if (!binding)
                        return core::unexpected(binding.error());
                }
                auto current = HostCoordinationAccess::currentFence(session);
                if (!current) {
                    if (job.proof.request.kind != ControlKind::close ||
                        !controlFullyApplied(job.proof) || session.state() != SessionState::Empty ||
                        !software)
                        return core::unexpected(current.error());
                    const auto& closed =
                        job.proof.steps[static_cast<unsigned>(Participant::software)]
                                       [static_cast<unsigned>(StepPhase::close)];
                    if (!closed)
                        return core::unexpected(
                            workerError("device_pending", "Software close proof absent"));
                    auto binding =
                        HostCoordinationAccess::validateControlBinding(session, closed->request);
                    if (!binding)
                        return core::unexpected(binding.error());
                    auto actual = software->query(closed->request.key);
                    if (!actual || actual->status != StepStatus::applied ||
                        actual->request != closed->request || actual->effects != closed->effects)
                        return core::unexpected(workerError(
                            "device_pending", "Software close differs from owner history"));
                    return SessionReply{std::monostate{}};
                }
                if (job.proof.request.kind == ControlKind::close &&
                    job.proof.request.context.controlSequence != current->controlSequence())
                    return reply(
                        HostCoordinationAccess::completeClose(session, *current, job.proof));
                auto released = HostCoordinationAccess::leaveFence(session, *current, job.proof);
                if (!released)
                    return core::unexpected(released.error());
                if (frames && job.projectionScope && job.proof.finalizedContext) {
                    auto bound =
                        job.projectionScope == projectionScope
                            ? frames->rebind(*job.proof.finalizedContext)
                            : frames->activate(*job.proof.finalizedContext, job.projectionScope);
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
                // Updating presentation materialises execution state; bill that work to
                // the execution domain instead of the command envelope.
                const internal::pd::memory::ExecutionDomainScope updateDomain{
                    internal::pd::memory::domainAdvance};
                return reply(updatePresentation(job));
            } else if constexpr (std::is_same_v<T, SessionDeliveryLayout>) {
                return reply(layout());
            } else if constexpr (std::is_same_v<T, SessionRegisterConsumer>) {
                auto valid = HostCoordinationAccess::validateFrameContext(session, job.context);
                if (!valid)
                    return core::unexpected(valid.error());
                return reply(delivery->registerConsumer(job.operationId, job.context, job.profile));
            } else if constexpr (std::is_same_v<T, SessionConfirmConsumer>) {
                if (!job.revoke) {
                    auto valid = HostCoordinationAccess::validateFrameContext(
                        session, job.receipt.registration.context);
                    if (!valid) {
                        auto retiring = delivery->beginRevoke(job.receipt.registration.consumerId);
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
                    return core::unexpected(
                        workerError("context_stale", "New session required for legacy fallback"));
                framePath = FramePath::legacy;
                auto valid = HostCoordinationAccess::validateFrameContext(session, job.context);
                if (!valid)
                    return core::unexpected(valid.error());
                if (!job.frameId || !job.projectionScope || job.frameId <= lastFrameId)
                    return core::unexpected(
                        workerError("context_stale", "Frame identity or projection scope stale"));
                FrameEnvelope frame;
                if (job.frozen) {
                    const auto& retained = job.retained.frame();
                    if (!retained || retained->projectionScope != job.projectionScope ||
                        retained->context.sessionScope != job.context.sessionScope ||
                        retained->context.contentIdentity != job.context.contentIdentity ||
                        retained->context.resourceGeneration != job.context.resourceGeneration ||
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
                                                                       &retained->payload->snapshot}
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
                        return core::unexpected(workerError("context_stale", "Source time stale"));
                    auto sampled =
                        updatePresentation({job.context, job.projectionScope, job.presentationTick,
                                            job.runtimeFrame, job.realtime});
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
                    auto members =
                        HostCoordinationAccess::feedbackMembership(session, frame.projectionScope);
                    if (!members)
                        return core::unexpected(members.error());
                    for (const auto& member : *members) {
                        auto created = frames->feedback(member.id, member.created, member.expiry);
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
                    return core::unexpected(workerError("context_stale", "Frame ledger absent"));
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
                return reply(HostCoordinationAccess::archiveCut(session, job.archive, job.horizon));
            } else {
                if (!software)
                    return core::unexpected(workerError("context_stale", "Software ledger absent"));
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
                    if (prepared &&
                        (!candidate || job.request.target.candidateId != candidate->candidateId ||
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
                        session, *software, job.request, std::move(*prepared), *current, job.proof);
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
                    return reply(
                        HostCoordinationAccess::applyPause(session, *software, job.request, now()));
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
                        session, *software, job.request, job.reconciliation, job.context, job.clock,
                        now(), &job.routeReceipt));
                } else if constexpr (std::is_same_v<T, SessionSeek>) {
                    // Seek re-materialises execution state; bill it to the execution
                    // domain instead of the command envelope.
                    const internal::pd::memory::ExecutionDomainScope seekDomain{
                        internal::pd::memory::domainAdvance};
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                    rpa::SeekSinkScope seekSinkScope{&rpaSeek};
                    rpa::SeekScope seekPhase{&rpaSeek, rpa::SeekPhase::s4InternalExecution};
                    rpa::seekSet(rpaSeek, rpa::SeekPhase::s4InternalExecution,
                                 &rpa::SeekWork::actualRecords,
                                 static_cast<std::uint64_t>(job.cut.completeRecords));
                // S4 work volume is recorded by the judgement seek itself, where the
                // ticks, matchers, folds and runtime state are actually touched. The host
                // only contributes the record count; overwriting the rest here would
                // replace measured values with host-side placeholders.
#endif
                    auto old = software->query(job.request.key);
                    if (old && old->status != StepStatus::pending && realtime)
                        return reply(HostCoordinationAccess::applySeek(
                            session, *software, job.request, job.proof, job.archive, job.horizon,
                            job.cut, job.checkpoints, job.presentationTick, job.runtimeFrame,
                            realtime->profile, realtime->mapping, realtime->monotonicDomain));
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
                    // A directed cleanup retire (suspended or failed owner) must not establish
                    // a normal software fence: enterExecutionFence accepts only a preparing
                    // proof, and the retire path already re-verifies the accepted request,
                    // target, participants, capture cut and audio isolation. Close cleanup
                    // therefore proceeds on the owner ledger, and the later Close either
                    // completes an older fence with a larger sequence or takes the no-fence
                    // confirm branch.
                    // Design 6.2: a suspended Close may take the explicit cleanup path, but a
                    // failed Close is not permitted in v1, so only non-Close failures qualify.
                    const bool directedCleanup = job.proof.status == ControlStatus::suspended ||
                                                 (job.proof.status == ControlStatus::failed &&
                                                  job.proof.request.kind != ControlKind::close);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                    auto& retirement = job.proof.request.kind == ControlKind::close
                                           ? rpaClose.closeRetire
                                           : rpaClose.cleanup;
                    retirement = {};
                    retirement.identity = proofIdentity(job.proof);
                    retirement.permitDirected = directedCleanup;
                    retirement.path = directedCleanup ? rpa::FencePath::cleanupPermit
                                                      : rpa::FencePath::normalControl;
                    retirement.applicability = directedCleanup
                                                   ? rpa::FenceApplicability::notApplicable
                                                   : rpa::FenceApplicability::applicable;
                    // The no-fence permit path must still record an existing fence identity:
                    // "not applicable" is not the same as "not checked".
                    if (directedCleanup) {
                        if (auto existing = HostCoordinationAccess::currentFence(session)) {
                            retirement.existingFence = true;
                            retirement.existingIdentity = fenceIdentity(*existing);
                        }
                    }
#endif
                    if (job.proof.request.kind == ControlKind::close &&
                        job.proof.status == ControlStatus::failed)
                        return core::unexpected(
                            workerError("control_pending", "Failed Close cannot retire software"));
                    if (!directedCleanup) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                        auto entered = fence(job.proof, &retirement);
#else
                        auto entered = fence(job.proof);
#endif
                        if (!entered)
                            return core::unexpected(entered.error());
                    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                    retirement.permitAttempted = true;
                    rpa::FenceScope permitScope{&retirement};
#endif
                    auto retired = HostCoordinationAccess::retireSoftware(session, *software,
                                                                          job.request, job.proof);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                    retirement.retireCalled = true;
                    retirement.retireApplied = retired && retired->status == StepStatus::applied;
                    retirement.referencesReleased = retired && retired->effects.referencesReleased;
#endif
                    if (session.state() == SessionState::Empty) {
                        prepared.reset();
                        candidate.reset();
                        activeCandidate.reset();
                    }
                    return reply(std::move(retired));
                } else if constexpr (std::is_same_v<T, SessionSoftwareClose>) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                    auto& closeStep = rpaClose.closeStep;
                    const auto identity = proofIdentity(job.proof);
                    if (closeStep.identity.scope != identity.scope ||
                        closeStep.identity.operationId != identity.operationId ||
                        closeStep.identity.decisionOrdinal != identity.decisionOrdinal)
                        closeStep = {};
                    closeStep.identity = identity;
                    closeStep.closeAttempted = true;
                    closeStep.closeApplied = false;
                    closeStep.closeState = rpa::CloseStepState::resultPending;
                    auto execute = [&]() -> core::Result<SessionReply> {
#endif
                        auto binding = HostCoordinationAccess::validateControlProof(
                            session, job.request, job.proof);
                        if (!binding)
                            return core::unexpected(binding.error());
                        if (job.proof.request.kind == ControlKind::close &&
                            job.proof.status == ControlStatus::failed)
                            return core::unexpected(workerError(
                                "control_pending", "Failed Close cannot apply software close"));
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
                        auto closed = simpleStep(job.request, true);
                        return reply(std::move(closed));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                    };
                    try {
                        auto result = execute();
                        const auto* receipt = result ? std::get_if<StepReceipt>(&*result) : nullptr;
                        closeStep.closeApplied = receipt && receipt->status == StepStatus::applied;
                        closeStep.closeState = closeStep.closeApplied
                                                   ? rpa::CloseStepState::applied
                                                   : rpa::CloseStepState::rejected;
                        if (!result && closeStep.firstErrorCode[0] == '\0')
                            rpa::copyCode(closeStep.firstErrorCode, result.error().code());
                        else if (receipt && receipt->cause && closeStep.firstErrorCode[0] == '\0')
                            rpa::copyCode(closeStep.firstErrorCode, receipt->cause->code());
                        return result;
                    } catch (...) {
                        closeStep.closeState = rpa::CloseStepState::rejected;
                        if (closeStep.firstErrorCode[0] == '\0')
                            rpa::copyCode(closeStep.firstErrorCode, "host.rpa.capacity_exceeded");
                        throw;
                    }
#endif
                }
            }
        },
        command);
}

} // namespace cuexis::playback::detail
