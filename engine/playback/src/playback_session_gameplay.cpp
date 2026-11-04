// PlaybackSession gameplay commands: the gameplay session, its projection and the publication.

#include "playback_session_internal.hpp"

#include <cmath>

namespace cuexis::playback {

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
auto validateGameplayFrame(const RuntimeFrame& frame, const std::optional<RuntimeFrame>& previous)
    -> core::Result<void> {
    // The frame has its own time domain. Never derive integer H or T from this double.
    if (!std::isfinite(frame.chartTimeMs))
        return core::unexpected(core::Error{"runtime.frame.chart_time_non_finite",
                                            "RuntimeFrame chartTimeMs must be finite"});
    if (!std::isfinite(frame.simulationDeltaTimeMs) || frame.simulationDeltaTimeMs < 0)
        return core::unexpected(
            core::Error{"runtime.frame.delta_invalid", "Invalid RuntimeFrame delta"});
    if (previous) {
        if (frame.timeDiscontinuityId == previous->timeDiscontinuityId &&
            frame.chartTimeMs < previous->chartTimeMs)
            return core::unexpected(core::Error{"runtime.frame.backward_seek_undeclared",
                                                "Backward time requires discontinuity"});
        if (frame.timeDiscontinuityId != previous->timeDiscontinuityId &&
            frame.simulationDeltaTimeMs != 0)
            return core::unexpected(core::Error{"runtime.frame.discontinuity_delta_nonzero",
                                                "Discontinuity requires zero delta"});
    }
    return {};
}
auto makeGameplaySession(const GameplayContent& c) -> core::Result<judgement::JudgementSession> {
    const auto* content = detail::GameplayAccess::content(c);
    if (!content)
        return core::unexpected(gameplayError("Empty Gameplay content"));
    auto session = judgement::JudgementSession::create();
    if (!session)
        return core::unexpected(detail::projectGameplayError(std::move(session.error())));
    if (auto r = session->configure(content->configuration); !r)
        return core::unexpected(detail::projectGameplayError(std::move(r.error())));
    if (auto r = session->prepare(content->capsule.gameplay); !r)
        return core::unexpected(detail::projectGameplayError(std::move(r.error())));
    return std::move(*session);
}
auto publicTick(std::optional<judgement::Tick> t) -> std::optional<GameplayTick> {
    return t ? std::optional{GameplayTick{t->value()}} : std::nullopt;
}
auto publicFault(std::optional<judgement::FaultStage> s) -> std::optional<GameplayFaultStage> {
    if (!s)
        return {};
    switch (*s) {
    case judgement::FaultStage::kernel:
        return GameplayFaultStage::Kernel;
    case judgement::FaultStage::fold:
        return GameplayFaultStage::Fold;
    case judgement::FaultStage::control:
        return GameplayFaultStage::Control;
    }
    return {};
}
auto PlaybackSession::gameplayWriteGuard() const -> core::Result<void> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("gameplay"));
    if (state_->operationActive)
        return core::unexpected(reentryError("gameplay"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence && !state_->hostCommitAuthorized)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks gameplay mutation"});
#endif
    if (state_->gameplayState == GameplayState::Faulted)
        return core::unexpected(
            gameplayError("Faulted Gameplay is read-only until reset or replacement"));
    return {};
}
auto PlaybackSession::attachGameplay(PreparedPlayback& p, const GameplayContent& c)
    -> core::Result<void> try {
    SessionOperation operation{state_->operationActive};
    const auto* content = detail::GameplayAccess::content(c);
    if (!content || !p.valid())
        return core::unexpected(gameplayError("Empty Gameplay content or PreparedPlayback"));
    if (p.state_->contentInfo.chartId != content->capsule.chart.chartId.value)
        return core::unexpected(gameplayError("Presentation and Gameplay Chart identities differ"));
    auto session = makeGameplaySession(c);
    if (!session)
        return core::unexpected(detail::projectGameplayError(std::move(session.error())));
    auto index = prepareProjection(*content, p.state_->snapshotLayout);
    if (!index)
        return core::unexpected(std::move(index.error()));
    p.state_->gameplay = std::make_unique<judgement::JudgementSession>(std::move(*session));
    p.state_->gameplayContent = c;
    p.state_->gameplayProjectionIndex = std::move(*index);
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Gameplay prepare allocation failed"));
}
auto PlaybackSession::prepareGameplayLoad(PlaybackSource&& source, PlaybackMode mode,
                                          const GameplayContent& c)
    -> core::Result<PreparedPlayback> {
    auto p = prepareLoad(std::move(source), mode);
    if (!p)
        return core::unexpected(detail::projectGameplayError(std::move(p.error())));
    if (auto r = attachGameplay(*p, c); !r)
        return core::unexpected(detail::projectGameplayError(std::move(r.error())));
    return std::move(*p);
}
auto PlaybackSession::prepareGameplayReload(PlaybackSource&& source, const GameplayContent& c,
                                            ReloadPolicy policy) -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("prepare_gameplay_reload"));
    const auto target = state_->lastFrame.value_or(RuntimeFrame{});
    auto p = prepareReload(std::move(source), target, policy);
    if (!p)
        return core::unexpected(detail::projectGameplayError(std::move(p.error())));
    if (auto r = attachGameplay(*p, c); !r)
        return core::unexpected(detail::projectGameplayError(std::move(r.error())));
    return std::move(*p);
}
auto PlaybackSession::submitGameplay(std::span<const GameplayInput> inputs)
    -> core::Result<void> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{state_->operationActive};
    if (!state_->gameplay || state_->gameplayState == GameplayState::Paused)
        return core::unexpected(gameplayError("Gameplay must be active and unpaused"));
    std::vector<judgement::ClockedIngress> batch;
    batch.reserve(inputs.size());
    for (const auto& input : inputs) {
        auto channel = judgement::ChannelRef::fromToken(input.channel);
        auto source = judgement::SourceClass::fromToken(input.sourceClass);
        if (!channel)
            return core::unexpected(detail::projectGameplayError(std::move(channel.error())));
        if (!source)
            return core::unexpected(detail::projectGameplayError(std::move(source.error())));
        const auto* current = detail::GameplayAccess::content(state_->gameplayContent);
        if (source->token() != current->configuration.inputMapping.view().sourceClass.token())
            return core::unexpected(
                gameplayError("Input source class does not match frozen mapping"));
        judgement::InputAction action;
        switch (input.action) {
        case GameplayInputAction::Press:
            action = judgement::InputAction::press;
            break;
        case GameplayInputAction::Release:
            action = judgement::InputAction::release;
            break;
        case GameplayInputAction::Update:
            action = judgement::InputAction::update;
            break;
        default:
            return core::unexpected(gameplayError("Unknown input action"));
        }
        std::optional<judgement::RationalBeat> amount;
        if (input.amount) {
            auto r =
                judgement::RationalBeat::create(input.amount->numerator, input.amount->denominator);
            if (!r)
                return core::unexpected(detail::projectGameplayError(std::move(r.error())));
            amount = *r;
        }
        batch.push_back({judgement::ObservationTick{judgement::Tick{input.observationTick.value}},
                         {judgement::IngressSequence{input.sequence},
                          action,
                          *channel,
                          input.domain,
                          {input.rawTimestamps.device, input.rawTimestamps.hostArrival,
                           input.rawTimestamps.audioFrame, input.rawTimestamps.renderFrame},
                          {input.crossedSamplingGap, input.reconnected, input.droppedSamples},
                          judgement::ContinuityKind::discrete,
                          amount}});
    }
    auto admitted = state_->gameplay->submit(std::move(batch));
    if (!admitted)
        return core::unexpected(detail::projectGameplayError(std::move(admitted.error())));
    if (!inputs.empty())
        state_->bumpGeneration();
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Gameplay input allocation failed"));
}
auto PlaybackSession::advanceGameplay(GameplayTick h, GameplayPresentationTick t,
                                      const RuntimeFrame& frame) -> core::Result<void> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{state_->operationActive};
    if (!state_->gameplay || state_->gameplayState == GameplayState::Paused)
        return core::unexpected(gameplayError("Gameplay must be active and unpaused"));
    if (auto valid = validateGameplayFrame(frame, state_->lastFrame); !valid)
        return valid;
    if (state_->gameplayProjection.lastTick && t.value < state_->gameplayProjection.lastTick->value)
        return core::unexpected(
            gameplayError("Presentation Tick rewind requires explicit recovery"));
    auto selected = state_->gameplay->query();
    if (selected && selected->kernelView().rpa)
        return core::unexpected(
            gameplayError("Realtime profile requires the coordinated advance entry"));
    auto previousReceipt = std::move(state_->gameplayReceipt);
    state_->gameplayReceipt =
        GameplayAdvanceReceipt{h, {}, {}, {}, 0, 0, false, {}, {}, false, {}, {}};
    auto advanced = state_->gameplay->advance(judgement::Tick{h.value});
    // A successful kernel advance is already a mutation, even if the owning query allocates.
    if (advanced) {
        state_->bumpGeneration();
        state_->gameplayState = GameplayState::Running;
        state_->gameplayReceipt->admitted = true;
    }
    auto query = state_->gameplay->query();
    if (!query)
        return core::unexpected(detail::projectGameplayError(std::move(query.error())));
    const auto& k = query->kernelView();
    if (!advanced && k.state != judgement::KernelSessionState::Faulted) {
        state_->gameplayReceipt = std::move(previousReceipt);
        return core::unexpected(detail::projectGameplayError(std::move(advanced.error())));
    }
    if (!advanced)
        state_->bumpGeneration();
    state_->gameplayReceipt =
        GameplayAdvanceReceipt{h,
                               publicTick(k.processedFrontier),
                               publicTick(k.lastAdvanceHorizon),
                               publicTick(k.failedTick),
                               k.sealedFactCursor,
                               k.fold ? k.fold->factCursor : k.sealedFactCursor,
                               false,
                               publicFault(k.faultStage),
                               {},
                               true,
                               k.fold ? publicTick(k.fold->workTick) : std::nullopt,
                               {}};
    if (!advanced) {
        state_->gameplayState = GameplayState::Faulted;
        state_->gameplayReceipt->error = detail::projectGameplayError(advanced.error());
        return core::unexpected(detail::projectGameplayError(std::move(advanced.error())));
    }
    state_->gameplayState = GameplayState::Running;
    auto failureStage = GameplayPublicationFailureStage::Projection;
    auto published = detail::GameplayPublicationAccess::publish(*this, *state_->gameplay, t, frame,
                                                                true, false, &failureStage);
    if (!published) {
        state_->gameplayReceipt->publicationFailureStage = failureStage;
        state_->gameplayReceipt->error = published.error();
        return published;
    }
    state_->gameplayReceipt->runtimeUpdated = static_cast<bool>(state_->runtimeSession);
    return {};
} catch (...) {
    auto error = gameplayError("Gameplay advance allocation failed");
    if (state_->gameplayReceipt) {
        state_->gameplayReceipt->publicationFailureStage = GameplayPublicationFailureStage::Result;
        state_->gameplayReceipt->error = error;
    }
    return core::unexpected(std::move(error));
}
void detail::GameplayPublicationAccess::finalize(PlaybackSession& session,
                                                 ProjectionCandidate&& candidate,
                                                 const RuntimeFrame& frame,
                                                 internal::presentation::Mutation& mutation,
                                                 CommittedPresentationState descriptor) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<ProjectionState>);
    static_assert(std::is_nothrow_move_assignable_v<core::Diagnostics>);
    auto& state = *session.state_;
    state.gameplayProjection = std::move(candidate.state);
    state.lastFrame = frame;
    state.sessionState = SessionState::Running;
    state.lastOperationDiagnostics = std::move(candidate.diagnostics);
    state.publishPresentation(mutation, descriptor);
}

auto detail::GameplayPublicationAccess::publish(
    PlaybackSession& session, judgement::JudgementSession& source, GameplayPresentationTick t,
    const RuntimeFrame& frame, bool tickLifetimes, bool rebuildScope,
    GameplayPublicationFailureStage* failureStage) -> core::Result<void> try {
    auto* state_ = session.state_.get();
    internal::pd::Timer measurement{internal::pd::Stage::projection};
    using internal::pd::memory::structure;
    structure("index.snapshot-layout", state_->snapshotLayout.entities.size(),
              state_->snapshotLayout.entities.capacity());
    structure("index.projection-rows", state_->gameplayProjectionIndex.rows.size(),
              state_->gameplayProjectionIndex.rows.capacity());
    structure("index.projection-sources", state_->gameplayProjectionIndex.sources.size(),
              state_->gameplayProjectionIndex.sources.size());
    structure("index.projection-targets", state_->gameplayProjectionIndex.targets.size(),
              state_->gameplayProjectionIndex.targets.size());
    structure("current.projection-active", state_->gameplayProjection.active.size(),
              state_->gameplayProjection.active.capacity());
    structure("current.projection-future", state_->gameplayProjection.future.size(),
              state_->gameplayProjection.future.size());
    if (failureStage)
        *failureStage = GameplayPublicationFailureStage::Projection;
    if (auto valid = validateGameplayFrame(frame, state_->lastFrame); !valid)
        return valid;
    auto scope = judgement::detail::PublicationAccess::scope(source);
    if (!scope)
        return core::unexpected(projectGameplayError(std::move(scope.error())));
    const ProjectionState fresh;
    const auto& projection =
        rebuildScope || !state_->gameplayProjection.scope ? fresh : state_->gameplayProjection;
    // A first publication or an explicit recovery establishes a new consumption scope.
    // Ordinary sampling must never silently rewind a mismatching scope.
    const auto expectedScope = projection.scope ? projection.scope : *scope;
    auto read =
        judgement::detail::PublicationAccess::readSince(source, expectedScope, projection.cursor);
    if (!read)
        return core::unexpected(projectGameplayError(std::move(read.error())));
    auto metadata = read->metadata();
    if (!metadata)
        return core::unexpected(std::move(metadata.error()));
    const auto& publication = metadata->get();
    if (!publication.mayProject)
        return core::unexpected(
            publication.faultDiagnostic
                ? projectGameplayError(*publication.faultDiagnostic)
                : gameplayError("Faulted Gameplay cannot publish presentation"));
    if (publication.realtime && !publication.hasFold)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "No successful Fold prefix to sample"});
    if (!state_->runtimeSession) {
        auto next = projection;
        next.scope = publication.runScope;
        next.lastTick = t;
        state_->gameplayProjection = std::move(next);
        state_->lastFrame = frame;
        state_->sessionState = SessionState::Running;
        return {};
    }
    auto mutation =
        internal::presentation::Mutation::reserve(state_->presentationClock, false, !rebuildScope);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto candidate = PresentationProjectionAdapter::project(
        state_->gameplayProjectionIndex, projection, publication, t,
        [&](std::uint64_t ordinal) { return read->fact(ordinal); },
        [&](std::size_t index, std::string_view id) -> core::Result<bool> {
            if (index >= state_->snapshotLayout.entities.size())
                return false;
            const auto& target = state_->snapshotLayout.entities[index];
            if (target.id.value != id || !target.mesh)
                return false;
            auto current =
                runtime::detail::PresentationAccess::state(std::as_const(*state_->runtimeSession));
            if (!current)
                return core::unexpected(std::move(current.error()));
            bool present{};
            auto checked = runtime::detail::PresentationAccess::read(
                std::as_const(*state_->runtimeSession),
                {state_->snapshotLayout.runtimeOwner, state_->snapshotLayout.runtimeLayout,
                 current->stamp.version},
                [&](const runtime::detail::PresentationRead& view) -> core::Result<void> {
                    auto value = view.readObject(target.runtimeIndex);
                    if (!value)
                        return core::unexpected(std::move(value.error()));
                    present = value->id == id && value->hasRenderable;
                    return {};
                });
            if (!checked)
                return core::unexpected(std::move(checked.error()));
            return present;
        });
    if (!candidate)
        return core::unexpected(std::move(candidate.error()));
    measurement.items(candidate->matchedRows);
    if (!candidate->state.sourceScope)
        candidate->state.sourceScope = allocatePlaybackSessionToken();
    std::vector<runtime::PropertyOverrideWrite> writes;
    writes.reserve(candidate->writes.size());
    for (const auto& [id, value] : candidate->writes)
        writes.push_back({chart::ChartObjectId{id}, world::PropertyId::RenderVisible, value});
    CommittedPresentationState descriptor;
    descriptor.source = rebuildScope ? internal::presentation::Source::Restore
                                     : internal::presentation::Source::RuntimeUpdate;
    descriptor.frame = frame;
    descriptor.presentationTick = t.value;
    descriptor.factCursor = publication.publishableCursor;
    descriptor.controlCursor = publication.controlCursor;
    descriptor.hasFold = publication.hasFold;
    const auto work = publication.hasFold ? publication.foldWorkTick : publication.kernelWorkTick;
    if (work)
        descriptor.workTick = work->value();
    descriptor.activationRevision = mutation->version();
    if (failureStage)
        *failureStage = GameplayPublicationFailureStage::Runtime;
    auto updated =
        state_->runtimeSession->updateGameplay(runtimeFrame(frame), writes, tickLifetimes);
    if (!updated) {
        return updated;
    }
    finalize(session, std::move(*candidate), frame, *mutation, descriptor);
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Gameplay projection allocation failed"));
}

auto PlaybackSession::gameplayState() const -> core::Result<GameplayState> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("gameplay_state"));
    if (state_->operationActive)
        return core::unexpected(reentryError("gameplay_state"));
    return state_->gameplayState;
}

auto PlaybackSession::queryGameplayPresentation() const
    -> core::Result<GameplayPresentationMap> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("gameplay_presentation_query"));
    if (state_->operationActive)
        return core::unexpected(reentryError("gameplay_presentation_query"));
    if (!state_->gameplay)
        return core::unexpected(gameplayError("Gameplay must be explicitly active"));
    const auto& projection = state_->gameplayProjection;
    std::map<std::tuple<std::uint64_t, std::uint64_t, std::string>, GameplayPresentationSource>
        sources;
    for (const auto& scheduled : projection.active) {
        const auto& b = state_->gameplayProjectionIndex.rows[scheduled.row].binding;
        if (!scheduled.published || !projection.lastTick ||
            b.start.value > projection.lastTick->value ||
            (b.end && b.end->value <= projection.lastTick->value))
            continue;
        sources.try_emplace(
            {scheduled.factId.commitId, scheduled.factId.localOrdinal, b.target},
            GameplayPresentationSource{{scheduled.factId.commitId, scheduled.factId.localOrdinal},
                                       b.target,
                                       b.visible,
                                       b.start,
                                       b.end});
    }
    GameplayPresentationMap result{projection.sourceScope, {}};
    result.sources.reserve(sources.size());
    for (auto& [unused, source] : sources)
        result.sources.push_back(std::move(source));
    return result;
} catch (...) {
    return core::unexpected(gameplayError("Gameplay source map query allocation failed"));
}

auto PlaybackSession::queryGameplayCapability(std::string_view id, std::string_view revision,
                                              const GameplayConfiguration* pending) const
    -> core::Result<GameplayCapabilityQuery> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("gameplay_capability"));
    if (state_->operationActive)
        return core::unexpected(reentryError("gameplay_capability"));
    const auto refused = [&](GameplayCapabilityState status, std::string_view code,
                             std::string_view category) {
        return GameplayCapabilityQuery{
            std::string{id}, std::string{revision}, status,
            core::Error{
                std::string{code},
                "Gameplay capability query did not find a matching validated active capability"}
                .withContext("category", std::string{category})
                .withContext("severity", "error")
                .withContext("faulted", "false")
                .withContext("capabilityId", std::string{id})
                .withContext("revision", std::string{revision})};
    };
    auto registry = gameplayCapabilities();
    if (!registry)
        return core::unexpected(std::move(registry.error()));
    const auto capability =
        std::ranges::find_if(*registry, [&](const auto& row) { return row.id == id; });
    if (capability == registry->end())
        return refused(GameplayCapabilityState::Unknown, "capability.unknown",
                       "unknown_capability");
    if (capability->revision != revision)
        return refused(GameplayCapabilityState::Unknown, "capability.revision_mismatch",
                       "unknown_capability");
    const auto* content = detail::GameplayAccess::content(state_->gameplayContent);
    const auto enabled = pending   ? std::span<const std::string>{pending->enabledCapabilities}
                         : content ? std::span<const std::string>{content->enabledCapabilities}
                                   : std::span<const std::string>{};
    if (std::ranges::find(enabled, id) == enabled.end())
        return refused(GameplayCapabilityState::Disabled, "capability.disabled",
                       "capability_disabled");
    bool matching = content && state_->gameplay;
    if (matching && pending) {
        auto config = encodeGameplayConfiguration(*pending);
        matching = config && *config == content->publicConfiguration;
    }
    if (!matching)
        return refused(GameplayCapabilityState::Insufficient, "capability.budget_insufficient",
                       "budget_exceeded");
    return GameplayCapabilityQuery{
        std::string{id}, std::string{revision}, GameplayCapabilityState::Available, {}};
} catch (...) {
    return core::unexpected(gameplayError("Gameplay capability query allocation failed"));
}
auto PlaybackSession::queryGameplay() const -> core::Result<GameplayResult> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("query_gameplay"));
    if (state_->operationActive)
        return core::unexpected(reentryError("query_gameplay"));
    if (!state_->gameplay)
        return core::unexpected(gameplayError("Gameplay inactive"));
    auto q = state_->gameplay->query();
    if (!q)
        return core::unexpected(detail::projectGameplayError(std::move(q.error())));
    return detail::GameplayAccess::result(q->kernelView(), state_->gameplayState);
} catch (...) {
    return core::unexpected(gameplayError("Gameplay query allocation failed"));
}
auto PlaybackSession::gameplayAdvanceReceipt() const -> core::Result<GameplayAdvanceReceipt> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("gameplay_receipt"));
    if (state_->operationActive)
        return core::unexpected(reentryError("gameplay_receipt"));
    if (!state_->gameplayReceipt)
        return core::unexpected(gameplayError("No admitted advance receipt"));
    return *state_->gameplayReceipt;
} catch (...) {
    return core::unexpected(gameplayError("Gameplay receipt allocation failed"));
}
auto PlaybackSession::archiveGameplay() const -> core::Result<GameplayReplay> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("archive_gameplay"));
    if (state_->operationActive)
        return core::unexpected(reentryError("archive_gameplay"));
    if (!state_->gameplay)
        return core::unexpected(gameplayError("Gameplay inactive"));
    auto a = state_->gameplay->archive();
    if (!a)
        return core::unexpected(detail::projectGameplayError(std::move(a.error())));
    return detail::GameplayAccess::replay(std::move(*a));
} catch (...) {
    return core::unexpected(gameplayError("Gameplay archive allocation failed"));
}
auto PlaybackSession::snapshotGameplay() const -> core::Result<GameplaySnapshot> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("snapshot_gameplay"));
    if (state_->operationActive)
        return core::unexpected(reentryError("snapshot_gameplay"));
    if (!state_->gameplay)
        return core::unexpected(gameplayError("Gameplay inactive"));
    auto s = state_->gameplay->snapshot();
    if (!s)
        return core::unexpected(detail::projectGameplayError(std::move(s.error())));
    return detail::GameplayAccess::snapshot(std::move(*s));
} catch (...) {
    return core::unexpected(gameplayError("Gameplay snapshot allocation failed"));
}
auto PlaybackSession::evaluateGameplayReplay(const GameplayReplay& a) const
    -> core::Result<GameplayReplayEvaluation> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("evaluate_gameplay"));
    if (state_->operationActive)
        return core::unexpected(reentryError("evaluate_gameplay"));
    const auto* archive = detail::GameplayAccess::replay(a);
    if (!archive)
        return core::unexpected(gameplayError("Empty Replay"));
    auto e = judgement::JudgementSession::evaluateReplay(archive->archive);
    if (!e)
        return core::unexpected(detail::projectGameplayError(std::move(e.error())));
    const auto s = e->result->state == judgement::KernelSessionState::Faulted
                       ? GameplayState::Faulted
                       : GameplayState::Running;
    return GameplayReplayEvaluation{e->evidenceValid,
                                    detail::GameplayAccess::result(*e->result, s)};
} catch (...) {
    return core::unexpected(gameplayError("Replay evaluation allocation failed"));
}
auto PlaybackSession::restoreGameplay(const GameplaySnapshot& s, GameplayPresentationTick t,
                                      const RuntimeFrame& frame) -> core::Result<void> try {
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{state_->operationActive};
    const auto* snapshot = detail::GameplayAccess::snapshot(s);
    const auto* content = detail::GameplayAccess::content(state_->gameplayContent);
    if (!snapshot || !content)
        return core::unexpected(gameplayError("Snapshot or current content absent"));
    auto restored = judgement::JudgementSession::recover(
        snapshot->snapshot, {content->configuration, content->capsule.gameplay});
    if (!restored)
        return core::unexpected(detail::projectGameplayError(std::move(restored.error())));
    auto candidate = std::make_unique<judgement::JudgementSession>(std::move(*restored));
    auto q = candidate->query();
    if (!q)
        return core::unexpected(detail::projectGameplayError(std::move(q.error())));
    if (auto published =
            detail::GameplayPublicationAccess::publish(*this, *candidate, t, frame, false, true);
        !published)
        return published;
    state_->gameplayState =
        q->kernelView().state == judgement::KernelSessionState::Faulted
            ? GameplayState::Faulted
            : (q->kernelView().rpa && q->kernelView().rpa->paused ? GameplayState::Paused
                                                                  : GameplayState::Running);
    if (state_->executionScheduler)
        state_->pendingRetiredRequestFloor =
            state_->executionScheduler->retentionProgress().lastAcceptedId;
    state_->executionScheduler.reset();
    state_->realtimeClock.reset();
    state_->executionContext.reset();
    state_->realtimeWasStarted = false;
    state_->gameplay.swap(candidate);
    state_->gameplayReceipt.reset();
    state_->bumpGeneration();
    return {};
} catch (...) {
    return core::unexpected(gameplayError("Gameplay restore allocation failed"));
}
auto PlaybackSession::checkpointGameplay(const GameplayReplay& a, GameplayTick h) const
    -> core::Result<GameplayCheckpoint> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("checkpoint_gameplay"));
    if (state_->operationActive)
        return core::unexpected(reentryError("checkpoint_gameplay"));
    const auto* archive = detail::GameplayAccess::replay(a);
    if (!archive)
        return core::unexpected(gameplayError("Empty Replay"));
    auto checkpoint =
        judgement::JudgementSession::checkpoint(archive->archive, judgement::Tick{h.value});
    if (!checkpoint)
        return core::unexpected(detail::projectGameplayError(std::move(checkpoint.error())));
    return detail::GameplayAccess::checkpoint(std::move(*checkpoint));
} catch (...) {
    return core::unexpected(gameplayError("Checkpoint allocation failed"));
}
#endif

} // namespace cuexis::playback
