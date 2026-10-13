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
    SessionWorkerConfiguration& configuration;
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
                    auto recorded = session.queryGameplayAdvance(job.requestId);
                    if (!recorded)
                        slot->reset();
                    return reply(std::move(advanced));

                } else if constexpr (std::is_same_v<T, SessionQueryAdvance>) {
                    return reply(session.queryGameplayAdvance(job.requestId));
                } else if constexpr (std::is_same_v<T, SessionRetireAdvance>) {
                    return reply(session.retireGameplayAdvance(job.requestId));
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
                        job.proof.request.context.controlSequence != current->controlSequence)
                        return reply(
                            HostCoordinationAccess::completeClose(session, *current, job.proof));
                    return reply(HostCoordinationAccess::leaveFence(session, *current, job.proof));
                } else if constexpr (std::is_same_v<T, SessionSample>) {
                    auto valid = HostCoordinationAccess::validateFrameContext(session, job.context);
                    if (!valid)
                        return core::unexpected(valid.error());
                    if (!job.frameId || !job.projectionScope || job.frameId <= lastFrameId ||
                        (frameContext && *frameContext == job.context && lastPresentationTick &&
                         job.presentationTick < *lastPresentationTick) ||
                        (frameContext && *frameContext != job.context &&
                         job.projectionScope == projectionScope))
                        return core::unexpected(workerError(
                            "context_stale", "Frame identity or projection scope stale"));
                    auto sampled = job.frozen ? core::Result<void>{}
                                   : job.realtime
                                       ? HostCoordinationAccess::samplePresentation(
                                             session, {job.presentationTick}, job.runtimeFrame)
                                       : session.update(job.runtimeFrame);
                    if (!sampled)
                        return core::unexpected(sampled.error());
                    auto snapshot = session.extractFrame(job.viewport);
                    if (!snapshot)
                        return core::unexpected(snapshot.error());
                    FrameEnvelope frame;
                    frame.context = job.context;
                    frame.frameId = job.frameId;
                    frame.projectionScope = job.projectionScope;
                    frame.presentationTick = job.presentationTick;
                    auto actualFrame = HostCoordinationAccess::currentRuntimeFrame(session);
                    if (!actualFrame)
                        return core::unexpected(actualFrame.error());
                    frame.runtimeFrame = *actualFrame;
                    frame.snapshot = std::move(*snapshot);
                    if (activeCandidate)
                        frame.resources = activeCandidate->resources;
                    if (job.realtime) {
                        auto lag = session.gameplayExecutionLag({job.presentationTick});
                        if (!lag)
                            return core::unexpected(lag.error());
                        frame.foldedFactCursor = lag->foldedFactCursor;
                        if (lag->foldedWorkTick)
                            frame.foldedWorkTick = lag->foldedWorkTick->value;
                    }
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
                        auto activated = frames->activate(frame.context, frame.projectionScope);
                        if (!activated)
                            return core::unexpected(activated.error());
                        frameContext = frame.context;
                        projectionScope = frame.projectionScope;
                    }
                    if (job.realtime) {
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
                    auto published = frames->publish(frame);
                    if (!published)
                        return core::unexpected(published.error());
                    lastFrameId = job.frameId;
                    lastPresentationTick = job.presentationTick;
                    auto expired = frames->expire(job.presentationTick);
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
      channel_(configuration_.capacity, configuration_.controlReserve) {}
auto SessionWorker::create(SessionWorkerConfiguration configuration)
    -> core::Result<std::unique_ptr<SessionWorker>> try {
    if (!configuration.testOnly || configuration.capacity <= configuration.controlReserve ||
        !configuration.controlReserve || configuration.stepCapacity <= configuration.stepReserve ||
        !configuration.stepReserve || !configuration.frameCapacity ||
        !configuration.feedbackCapacity)
        return core::unexpected(
            workerError("profile_incomplete", "Explicit test-only worker capacities required"));
    auto worker = std::unique_ptr<SessionWorker>{new SessionWorker{std::move(configuration)}};
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
void SessionWorker::requestShutdown() noexcept {
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
        SessionOwner owner{PlaybackSession{}, {}, {}, {}, {}, {}, {}, 0, 0, {}, {}, {},
                           configuration_};
        for (;;) {
            if (auto next = channel_.take()) {
                try {
                    channel_.complete(next->first, owner.dispatch(std::move(next->second)));
                } catch (...) {
                    channel_.complete(
                        next->first, core::unexpected(workerError("capacity_exceeded",
                                                                  "Worker job allocation failed")));
                }
                continue;
            }
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
