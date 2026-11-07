// PlaybackSession host facade: load, absolute-time update, owning headless snapshots and reload.

#include "playback_session_internal.hpp"
#include "playback_session_prepare_internal.hpp"

namespace cuexis::playback {

using namespace prepare_detail;

auto detail::PresentationAccess::state(const PlaybackSession& session)
    -> core::Result<CommittedPresentationState> {
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("presentation_state"));
    if (session.state_->operationActive)
        return core::unexpected(reentryError("presentation_state"));
    if (!session.state_->committedPresentation)
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    return *session.state_->committedPresentation;
}

auto PlaybackSession::presentationSource() const -> core::Result<PresentationSourceInfo> {
    auto committed = detail::PresentationAccess::state(*this);
    if (!committed)
        return core::unexpected(committed.error());
    const auto& s = *committed;
    PresentationSource source{PresentationSource::Prepared};
    switch (s.source) {
    case internal::presentation::Source::Prepared:
        source = PresentationSource::Prepared;
        break;
    case internal::presentation::Source::RuntimeUpdate:
        source = PresentationSource::RuntimeUpdate;
        break;
    case internal::presentation::Source::Restore:
        source = PresentationSource::Restore;
        break;
    case internal::presentation::Source::Mutation:
        source = PresentationSource::Mutation;
        break;
    }
    return PresentationSourceInfo{s.sessionScope,
                                  s.resultVersion,
                                  s.contentRevision,
                                  s.layoutRevision,
                                  s.worldRevision,
                                  s.updateOrdinal,
                                  source,
                                  s.frame,
                                  s.presentationTick,
                                  s.workTick,
                                  s.factCursor,
                                  s.controlCursor,
                                  s.activationRevision ? std::optional{s.activationRevision}
                                                       : std::nullopt,
                                  s.hasFold};
}

auto PlaybackSession::state() const -> core::Result<SessionState> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("state"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("state"));
    }
    return state_->sessionState;
}

auto PlaybackSession::capabilities() const -> core::Result<PlaybackCapabilitySet> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("capabilities"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("capabilities"));
    }
    try {
        return state_->capabilities;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("capabilities", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("capabilities", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("capabilities", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("capabilities", false));
    }
}

auto PlaybackSession::loadChart(std::string_view jsonText) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("load_chart"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("load_chart"));
    }
    auto prepared = prepareLoad(jsonText, PlaybackMode::ChartClock);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::load(PlaybackSource&& source, PlaybackMode mode) -> core::Result<void> {
    return load(std::move(source), mode, PlaybackPrepareOptions{});
}

auto PlaybackSession::load(PlaybackSource&& source, PlaybackMode mode,
                           const PlaybackPrepareOptions& options) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("load"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("load"));
    }
    auto prepared = prepareLoad(std::move(source), mode, options);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::update(const RuntimeFrame& frame) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("update"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks update"});
#endif
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    if (state_->gameplayState == GameplayState::Paused)
        return core::unexpected(gameplayError("Paused Gameplay preserves the published frame"));
#endif
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("update"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("update"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running) {
        return core::unexpected(core::Error{"playback.session.not_ready",
                                            "PlaybackSession must be active to receive updates"});
    }
    if (!state_->runtimeSession)
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    auto mutation =
        internal::presentation::Mutation::reserve(state_->presentationClock, false, true);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto updated = state_->runtimeSession->update(runtimeFrame(frame));
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
    }
    detail::CommittedPresentationState descriptor;
    descriptor.source = internal::presentation::Source::RuntimeUpdate;
    descriptor.frame = frame;
    descriptor.activationRevision = state_->committedPresentation->activationRevision;
    state_->publishPresentation(*mutation, descriptor);
    state_->lastFrame = frame;
    state_->sessionState = SessionState::Running;
    state_->bumpGeneration();
    return {};
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport) const
    -> core::Result<FrameSnapshot> try {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("extract_frame"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("extract_frame"));
    }
    FrameSnapshot snapshot;
    if (auto extracted = extractFrame(viewport, snapshot); !extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    return snapshot;
} catch (const std::bad_alloc&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::length_error&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::exception& exception) {
    return core::unexpected(prepareExceptionError("extract_frame", false, &exception));
} catch (...) {
    return core::unexpected(prepareExceptionError("extract_frame", false));
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport, FrameSnapshot& snapshot) const
    -> core::Result<void> {
    return detail::PresentationAccess::materialize(*this, std::nullopt, viewport, snapshot);
}

auto detail::PresentationAccess::materialize(const PlaybackSession& session,
                                             std::optional<std::uint64_t> exactVersion,
                                             const FrameViewport& viewport, FrameSnapshot& snapshot)
    -> core::Result<void> try {
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
    // Product tag: extract-frame materialization is the per-call snapshot DTO, not session state.
    const internal::pd::memory::TagScope tag{internal::pd::memory::ownerMain,
                                             internal::pd::memory::domainSnapshot};
#endif
    auto* state_ = session.state_.get();
    internal::pd::Timer measurement{internal::pd::Stage::snapshot};
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("extract_frame"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("extract_frame"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState == SessionState::Empty || !state_->runtimeSession ||
        !state_->activeChartInfo.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    if (viewport.width == 0 || viewport.height == 0) {
        return core::unexpected(core::Error{"playback.viewport.invalid",
                                            "FrameViewport width and height must be positive"});
    }

    if (!state_->committedPresentation ||
        (exactVersion && *exactVersion != state_->committedPresentation->resultVersion))
        return core::unexpected(core::Error{"playback.presentation.version_unavailable",
                                            "Requested presentation version is unavailable"});
    measurement.items(state_->snapshotLayout.entities.size());
    const auto& committed = *state_->committedPresentation;
    auto extracted = runtime::detail::PresentationAccess::read(
        std::as_const(*state_->runtimeSession), committed.runtime,
        [&](const runtime::detail::PresentationRead& view) {
            return detail::materializeSnapshot(view, state_->snapshotLayout, viewport, snapshot);
        });
    if (!extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    return {};
} catch (const std::bad_alloc&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::length_error&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::exception& exception) {
    return core::unexpected(prepareExceptionError("extract_frame", false, &exception));
} catch (...) {
    return core::unexpected(prepareExceptionError("extract_frame", false));
}

auto PlaybackSession::reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
    return reload(replacementJson, targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy, const PlaybackPrepareOptions& options)
    -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("reload"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("reload"));
    }
    auto prepared = prepareReload(replacementJson, targetFrame, policy, options);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::reload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
    return reload(std::move(replacement), targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::reload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy, const PlaybackPrepareOptions& options)
    -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("reload"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("reload"));
    }
    auto prepared = prepareReload(std::move(replacement), targetFrame, policy, options);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::unload() -> core::Result<void> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("unload"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence && !state_->hostCommitAuthorized)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks unload"});
#endif
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("unload"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("unload"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState == SessionState::Empty) {
        return {};
    }
    auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    if (state_->runtimeSession) {
        if (auto result = state_->runtimeSession->unload(); !result) {
            return result;
        }
    }
    state_->runtimeSession.reset();
    state_->resourceManager.reset();
    state_->contentProvider.reset();
    state_->activeChartJson.clear();
    state_->snapshotLayout = {};
    state_->lastFrame.reset();
    state_->diagnostics.clear();
    state_->lastOperationDiagnostics.clear();
    state_->activeChartInfo.reset();
    state_->activeContentInfo.reset();
    state_->activeMode.reset();
    state_->semanticIdentity.reset();
    state_->candidateMetadata.reset();
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    state_->pendingRetiredRequestFloor.reset();
    state_->executionScheduler.reset();
    state_->realtimeClock.reset();
    state_->executionContext.reset();
    state_->realtimeWasStarted = false;
    state_->gameplay.reset();
    state_->gameplayContent = {};
    state_->gameplayProjectionIndex = {};
    state_->gameplayState = GameplayState::Inactive;
    state_->gameplayReceipt.reset();
    state_->gameplayProjection = {};
#endif
    state_->parameters = {};
    state_->presentation.reset();
    state_->sessionState = SessionState::Empty;
    state_->publishPresentation(*mutation, {});
    state_->bumpGeneration();
    return {};
}

auto PlaybackSession::chartInfo() const -> core::Result<ChartInfo> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("chart_info"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("chart_info"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->runtimeSession || state_->runtimeSession->empty() ||
        !state_->activeChartInfo.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed data"});
    }
    auto info = *state_->activeChartInfo;
    info.resourceCount = state_->runtimeSession->resourceCount();
    return info;
}

auto PlaybackSession::presentationManifest() const -> core::Result<PresentationResourceManifest> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("presentation_manifest"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("presentation_manifest"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PlaybackSession has no portable presentation manifest"});
    }
    try {
        return state_->presentation->manifest;
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.budget_exceeded",
                        "Portable presentation manifest copy could not be allocated"});
    } catch (...) {
        return core::unexpected(core::Error{"playback.presentation.prepare_failed",
                                            "Portable presentation manifest copy failed"});
    }
}

auto PlaybackSession::acquirePresentationResource(const PresentationResourceRef& reference) const
    -> core::Result<PortableResourcePtr> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("acquire_presentation_resource"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("acquire_presentation_resource"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PlaybackSession has no portable presentation resources"});
    }
    const auto* resource = detail::findPresentationResource(*state_->presentation, reference);
    if (resource == nullptr) {
        return core::unexpected(
            core::Error{"playback.presentation.reference.invalid",
                        "Portable presentation resource reference is not in the active manifest"}
                .withContext("asset_id", reference.assetId)
                .withContext("resource_type",
                             std::string{presentationResourceTypeName(reference.type)}));
    }
    return *resource;
}

auto PlaybackSession::contentInfo() const -> core::Result<PlaybackContentInfo> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("content_info"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("content_info"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->activeContentInfo) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed content"});
    }
    try {
        return *state_->activeContentInfo;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("content_info", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("content_info", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("content_info", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("content_info", false));
    }
}

auto PlaybackSession::semanticIdentity() const -> core::Result<PreparedSemanticIdentity> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("semantic_identity"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("semantic_identity"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->semanticIdentity) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed content"});
    }
    return *state_->semanticIdentity;
}

auto PlaybackSession::diagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("diagnostics"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("diagnostics"));
    }
    SessionOperation operation{state_->operationActive};
    try {
        return state_->diagnostics;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("diagnostics", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("diagnostics", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("diagnostics", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("diagnostics", false));
    }
}

auto PlaybackSession::lastOperationDiagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("last_operation_diagnostics"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("last_operation_diagnostics"));
    }
    SessionOperation operation{state_->operationActive};
    try {
        return state_->lastOperationDiagnostics;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("last_operation_diagnostics", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("last_operation_diagnostics", true));
    } catch (const std::exception& exception) {
        return core::unexpected(
            prepareExceptionError("last_operation_diagnostics", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("last_operation_diagnostics", false));
    }
}

auto PlaybackSession::acquireHostOverride(std::string_view ownerId, std::int64_t priority,
                                          std::uint16_t propertyMask,
                                          const HostOverrideLifetime& lifetime,
                                          std::span<const HostOverrideWrite> writes)
    -> core::Result<HostOverrideToken> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("acquire_host_override"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks override"});
#endif
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (auto guard = gameplayWriteGuard(); !guard)
        return core::unexpected(std::move(guard.error()));
#endif
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("acquire_host_override"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("acquire_host_override"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running) {
        return core::unexpected(
            core::Error{"playback.session.not_ready",
                        "PlaybackSession must be active to acquire a host override"});
    }
    if (!state_->runtimeSession) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    if (writes.empty()) {
        return core::unexpected(core::Error{"playback.override.empty",
                                            "Override tokens require at least one property write"});
    }

    try {
        std::vector<runtime::PropertyOverrideWrite> mapped;
        mapped.reserve(writes.size());
        for (const auto& write : writes) {
            mapped.push_back(runtime::PropertyOverrideWrite{
                .objectId = chart::ChartObjectId{write.objectId},
                .property = toWorldProperty(write.property),
                .value = toWorldValue(write.value),
            });
        }
        auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock);
        if (!mutation)
            return core::unexpected(std::move(mutation.error()));
        auto token = state_->runtimeSession->acquireOverride(
            world::OverrideKind::Host, std::string{ownerId}, priority, propertyMask,
            toWorldLifetime(lifetime), mapped);
        if (!token) {
            return core::unexpected(mapHostOverrideError(token.error()));
        }
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (state_->gameplay)
            state_->bumpGeneration();
#endif
        detail::CommittedPresentationState descriptor;
        descriptor.source = internal::presentation::Source::Mutation;
        descriptor.activationRevision = state_->committedPresentation->activationRevision;
        state_->publishPresentation(*mutation, descriptor);
        return HostOverrideToken{.value = token->value};
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("acquire_host_override", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("acquire_host_override", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("acquire_host_override", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("acquire_host_override", false));
    }
}

auto PlaybackSession::releaseHostOverride(HostOverrideToken token) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("release_host_override"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks override"});
#endif
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
#endif
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("release_host_override"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("release_host_override"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running) {
        return core::unexpected(
            core::Error{"playback.session.not_ready",
                        "PlaybackSession must be active to release a host override"});
    }
    if (!state_->runtimeSession) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto released =
        state_->runtimeSession->releaseOverride(world::OverrideTokenId{.value = token.value});
    if (!released) {
        return core::unexpected(mapHostOverrideError(released.error()));
    }
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->gameplay)
        state_->bumpGeneration();
#endif
    detail::CommittedPresentationState descriptor;
    descriptor.source = internal::presentation::Source::Mutation;
    descriptor.activationRevision = state_->committedPresentation->activationRevision;
    state_->publishPresentation(*mutation, descriptor);
    return {};
}

} // namespace cuexis::playback
