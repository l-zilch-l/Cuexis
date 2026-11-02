// PlaybackSession prepare and commit: the source preparation and the atomic commit.

#include "playback_session_internal.hpp"
#include "playback_session_prepare_internal.hpp"

namespace cuexis::playback {

using namespace prepare_detail;

auto PlaybackSession::prepareLoad(std::string_view jsonText, PlaybackMode mode)
    -> core::Result<PreparedPlayback> {
    return prepareLoad(jsonText, mode, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareLoad(std::string_view jsonText, PlaybackMode mode,
                                  const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_load"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_load");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    try {
        auto source = [&] {
            const internal::PrepareTraceScope trace{"source_parse"};
            return PlaybackSource::fromChartText(std::string{jsonText});
        }();
        if (!source) {
            publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, source.error());
            return core::unexpected(std::move(source.error()));
        }
        return prepare(std::move(*source), mode, nullptr, ReloadPolicy::KeepChartTime, false,
                       options);
    } catch (const std::bad_alloc&) {
        auto error = prepareExceptionError("prepare_load", true);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (const std::exception& exception) {
        auto error = prepareExceptionError("prepare_load", false, &exception);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (...) {
        auto error = prepareExceptionError("prepare_load", false);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
}

auto PlaybackSession::prepareLoad(PlaybackSource&& source, PlaybackMode mode)
    -> core::Result<PreparedPlayback> {
    return prepareLoad(std::move(source), mode, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareLoad(PlaybackSource&& source, PlaybackMode mode,
                                  const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_load"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_load");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    return prepare(std::move(source), mode, nullptr, ReloadPolicy::KeepChartTime, false, options);
}

auto PlaybackSession::prepareReload(std::string_view replacementJson,
                                    const RuntimeFrame& targetFrame, ReloadPolicy policy)
    -> core::Result<PreparedPlayback> {
    return prepareReload(replacementJson, targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareReload(std::string_view replacementJson,
                                    const RuntimeFrame& targetFrame, ReloadPolicy policy,
                                    const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_reload"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_reload");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if (!state_->activeMode) {
        auto error =
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    try {
        auto source = PlaybackSource::fromChartText(std::string{replacementJson});
        if (!source) {
            publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, source.error());
            return core::unexpected(std::move(source.error()));
        }
        if (state_->resourceManager) {
            auto sourceState = std::move(source->state_);
            sourceState->provider = state_->contentProvider;
            sourceState->database.emplace(state_->resourceManager->database());
            source->state_ = std::move(sourceState);
        }
        return prepare(std::move(*source), *state_->activeMode, &targetFrame, policy, true,
                       options);
    } catch (const std::bad_alloc&) {
        auto error = prepareExceptionError("prepare_reload", true);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (const std::exception& exception) {
        auto error = prepareExceptionError("prepare_reload", false, &exception);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (...) {
        auto error = prepareExceptionError("prepare_reload", false);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
}

auto PlaybackSession::prepareReload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                                    ReloadPolicy policy) -> core::Result<PreparedPlayback> {
    return prepareReload(std::move(replacement), targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareReload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                                    ReloadPolicy policy, const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_reload"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_reload");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if (!state_->activeMode) {
        auto error =
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    return prepare(std::move(replacement), *state_->activeMode, &targetFrame, policy, true,
                   options);
}

auto PlaybackSession::prepare(PlaybackSource&& source, PlaybackMode mode,
                              const RuntimeFrame* targetFrame, ReloadPolicy policy,
                              bool replacement, const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    core::Diagnostics diagnostics;
    std::optional<PrepareDiagnosticsRecorder> diagnosticsRecorder;
    try {
        if (!state_->ownerThread.isCurrent()) {
            return core::unexpected(ownerError(replacement ? "prepare_reload" : "prepare_load"));
        }
        diagnosticsRecorder.emplace(state_->lastOperationDiagnostics, diagnostics);
        if (state_->operationActive) {
            auto error = reentryError(replacement ? "prepare_reload" : "prepare_load");
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }
        SessionOperation operation{state_->operationActive};
        if ((!replacement && state_->sessionState != SessionState::Empty) ||
            (replacement && state_->sessionState != SessionState::Ready &&
             state_->sessionState != SessionState::Running)) {
            auto error = core::Error{
                replacement ? "playback.session.not_ready" : "playback.session.not_empty",
                replacement ? "PlaybackSession must be active before preparing a reload"
                            : "PlaybackSession must be Empty before preparing a load"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }

        if (!source.state_) {
            auto error = core::Error{"playback.source.invalid", "PlaybackSource is empty"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }
        auto& sourceState = *source.state_;
        const auto* entryChart = sourceState.entryChart();
        const bool candidateSource = sourceState.candidate.has_value();
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        const bool gameplayOnly = sourceState.gameplayContent.valid() &&
                                  sourceState.gameplayIntent == GameplayPrepareIntent::GameplayOnly;
#else
        const bool gameplayOnly = false;
#endif

        if (!candidateSource && entryChart == nullptr) {
            auto error =
                core::Error{"playback.source.invalid", "PlaybackSource entry Chart is unavailable"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }
        const std::string_view jsonText =
            entryChart != nullptr ? entryChart->utf8Text : std::string_view{};
        const chart::ChartLimits limits;
        PrepareContext context{*this,  source,      jsonText, mode,   targetFrame,
                               policy, replacement, options,  limits, state_->capabilities};
        PrepareArtifact artifact;
        auto& document = artifact.document;
        auto& animationProgram = artifact.animationProgram;
        auto& additionalCapabilities = artifact.additionalCapabilities;
        auto& resourceManager = artifact.resourceManager;
        auto& audioSourceLease = artifact.audioSourceLease;
        auto& committedFrame = artifact.committedFrame;
        if (sourceState.cxcPackageIdentity) {
            additionalCapabilities.emplace_back(capabilitySourceCxcV1);
        }

        std::optional<chart::ChartRuntime> chartRuntimeStorage;
        std::optional<animation::AnimationProgram> compiledAnimation;
        if (candidateSource) {
            auto candidateRuntime =
                prepareCandidateStage(*sourceState.candidate,
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
                                      sourceState.gameplayContent.valid(),
#else
                                      false,
#endif
                                      options, replacement, limits, artifact, diagnostics);
            if (!candidateRuntime) {
                return core::unexpected(std::move(candidateRuntime.error()));
            }
            if (!gameplayOnly &&
                !preflightCapabilities(chart::ChartDocument{}, additionalCapabilities,
                                       context.capabilities, diagnostics)) {
                return core::unexpected(operationError("playback.capability.preflight_failed",
                                                       "Playback capability preflight failed",
                                                       diagnostics));
            }
            compiledAnimation = animation::AnimationProgram{};
            chartRuntimeStorage = std::move(*candidateRuntime);
        } else {
            auto loaded = [&] {
                const internal::PrepareTraceScope trace{"load_document"};
                return loadDocumentStage(context, artifact, diagnostics);
            }();
            if (!loaded) {
                const auto message = artifact.isV4 ? "Chart v4 loading produced errors"
                                                   : "Chart loading produced errors";
                return core::unexpected(operationError(context.replacement
                                                           ? "playback.chart.reload_load_failed"
                                                           : "playback.chart.load_failed",
                                                       message, diagnostics));
            }
            auto resolved = [&] {
                const internal::PrepareTraceScope trace{"resolve_parameters"};
                return resolveParametersStage(context, artifact, sourceState.projectDocuments,
                                              diagnostics);
            }();
            if (!resolved) {
                return core::unexpected(
                    operationError(context.replacement ? "playback.chart.reload_load_failed"
                                                       : "playback.chart.load_failed",
                                   artifact.v4Source ? "Chart v4 resolution produced errors"
                                                     : "Chart parameter resolution produced errors",
                                   diagnostics));
            }

            const bool preflightOk = [&] {
                const internal::PrepareTraceScope trace{"preflight"};
                return preflightCapabilities(*document, additionalCapabilities,
                                             context.capabilities, diagnostics);
            }();
            if (!preflightOk) {
                return core::unexpected(operationError("playback.capability.preflight_failed",
                                                       "Playback capability preflight failed",
                                                       diagnostics));
            }

            compiledAnimation = [&] {
                const internal::PrepareTraceScope trace{"animation_compile"};
                return compileAnimationProgram(std::move(animationProgram), limits, diagnostics);
            }();
            if (!compiledAnimation) {
                return core::unexpected(
                    operationError(replacement ? "playback.animation.reload_compile_failed"
                                               : "playback.animation.compile_failed",
                                   "Animation program compilation produced errors", diagnostics));
            }

            auto runtimeResult = [&] {
                const internal::PrepareTraceScope trace{"chart_compile"};
                return chart::ChartCompiler::compile(*document, limits);
            }();
            const bool runtimeValid = runtimeResult.hasValue();
            diagnostics.append(std::move(runtimeResult.diagnostics));
            if (!runtimeValid) {
                return core::unexpected(
                    operationError(replacement ? "playback.chart.reload_compile_failed"
                                               : "playback.chart.compile_failed",
                                   "Chart compilation produced errors", diagnostics));
            }
            chartRuntimeStorage = std::move(*runtimeResult.runtime);
        }
        auto& chartRuntime = *chartRuntimeStorage;
        const bool hasMainMusic = chartRuntime.mainMusic.has_value();
        if ((context.mode == PlaybackMode::ChartClock && hasMainMusic) ||
            (context.mode != PlaybackMode::ChartClock && !hasMainMusic)) {
            auto error =
                core::Error{"playback.mode.content_mismatch",
                            context.mode == PlaybackMode::ChartClock
                                ? "ChartClock requires a chart without main music"
                                : "HostClock and CuexisAudio require a chart with main music"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }

        {
            const internal::PrepareTraceScope trace{"resources"};
            if (sourceState.database) {
                resourceManager = std::make_unique<assets::ResourceManager>(
                    std::move(*sourceState.database), sourceState.provider);
            }
        }

        auto acquired = [&] {
            const internal::PrepareTraceScope trace{"audio"};
            return acquireAudioStage(artifact, chartRuntime, diagnostics);
        }();
        if (!acquired) {
            if (diagnostics.empty()) {
                addErrorDiagnostic(diagnostics, acquired.error());
            }
            return core::unexpected(std::move(acquired.error()));
        }
        if (gameplayOnly) {
            if (auto presentation =
                    preparePresentationStage(context, artifact, chartRuntime, diagnostics, false);
                !presentation)
                return core::unexpected(std::move(presentation.error()));
            artifact.snapshotLayout.emplace();
        } else {
            auto runtime = [&] {
                const internal::PrepareTraceScope trace{"runtime"};
                return prepareRuntimeStage(artifact, chartRuntime, std::move(*compiledAnimation),
                                           diagnostics);
            }();
            if (!runtime) {
                return core::unexpected(std::move(runtime.error()));
            }
            auto presentation = [&] {
                const internal::PrepareTraceScope trace{"presentation"};
                return preparePresentationStage(context, artifact, chartRuntime, diagnostics);
            }();
            if (!presentation) {
                return core::unexpected(std::move(presentation.error()));
            }
            auto committed = [&] {
                const internal::PrepareTraceScope trace{"commit_runtime"};
                return commitRuntimeStage(artifact);
            }();
            if (!committed) {
                addErrorDiagnostic(diagnostics, committed.error());
                return core::unexpected(std::move(committed.error()));
            }
            auto sampled = [&] {
                const internal::PrepareTraceScope trace{"commit_frame"};
                return commitFrameStage(context, artifact, diagnostics);
            }();
            if (!sampled) {
                return core::unexpected(std::move(sampled.error()));
            }

            const auto* presentationCandidate =
                artifact.presentation ? &*artifact.presentation : nullptr;
            auto layout = [&] {
                const internal::PrepareTraceScope trace{"snapshot_layout"};
                return buildSnapshotLayout(*artifact.runtimeSession, chartRuntime,
                                           presentationCandidate);
            }();
            if (!layout) {
                addErrorDiagnostic(diagnostics, layout.error());
                return core::unexpected(std::move(layout.error()));
            }
            artifact.snapshotLayout = std::move(*layout);
        }
        auto assembledIdentity = [&] {
            const internal::PrepareTraceScope trace{"identity"};
            return assembleIdentityStage(artifact, diagnostics);
        }();
        if (!assembledIdentity) {
            return core::unexpected(std::move(assembledIdentity.error()));
        }

        const internal::PrepareTraceScope traceAssemble{"assemble"};
        diagnostics.sortDeterministically();
        auto prepared = std::make_unique<PreparedPlayback::State>();
        prepared->owner = &context.owner;
        prepared->ownerToken = state_->sessionToken;
        prepared->expectedGeneration = state_->generation;
        prepared->replacement = context.replacement;
        prepared->contentProvider = std::move(sourceState.provider);
        prepared->resourceManager = std::move(resourceManager);
        prepared->chartJson = candidateSource ? std::string{} : std::string{jsonText};
        prepared->runtimeSession = std::move(artifact.runtimeSession);
        prepared->snapshotLayout = std::move(*artifact.snapshotLayout);
        prepared->chartInfo = chartInfoFor(
            chartRuntime, prepared->runtimeSession ? prepared->runtimeSession->resourceCount() : 0);
        prepared->contentInfo = PlaybackContentInfo{
            chartRuntime.chartId.value, chartRuntime.version, chartRuntime.timingMap.offsetMs(),
            context.mode,
            chartRuntime.mainMusic ? std::optional<std::string>{chartRuntime.mainMusic->value}
                                   : std::nullopt};
        prepared->parameters = context.options.parameters;
        prepared->semanticIdentity = toPublicIdentity(*assembledIdentity);
        prepared->candidateMetadata = std::move(artifact.candidateMetadata);
        prepared->audioSourceLease = std::move(audioSourceLease);
        prepared->targetFrame = committedFrame;
        prepared->committedState = replacement ? state_->sessionState : SessionState::Ready;
        prepared->presentation = std::move(artifact.presentation);
        prepared->candidateGeneration = state_->nextCandidateGeneration++;
        if (prepared->candidateGeneration == 0) {
            prepared->candidateGeneration = state_->nextCandidateGeneration++;
        }
        // Keep the candidate snapshot independent; the operation recorder still publishes the
        // same warnings (or an empty set) to lastOperationDiagnostics on scope exit.
        prepared->diagnostics = diagnostics;
        prepared->lastOperationDiagnostics = prepared->diagnostics;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (sourceState.gameplayContent.valid()) {
            const auto* content = detail::GameplayAccess::content(sourceState.gameplayContent);
            auto index = prepareProjection(*content, prepared->snapshotLayout);
            if (!index)
                return core::unexpected(std::move(index.error()));
            auto gameplay = judgement::JudgementSession::create();
            if (!gameplay)
                return core::unexpected(std::move(gameplay.error()));
            if (auto r = gameplay->configure(content->configuration); !r)
                return core::unexpected(std::move(r.error()));
            if (auto r = gameplay->prepare(content->capsule.gameplay); !r)
                return core::unexpected(std::move(r.error()));
            prepared->gameplay =
                std::make_unique<judgement::JudgementSession>(std::move(*gameplay));
            prepared->gameplayContent = sourceState.gameplayContent;
            prepared->gameplayProjectionIndex = std::move(*index);
        }
#endif

        return PreparedPlayback{std::move(prepared)};
    } catch (const std::bad_alloc&) {
        auto error = prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", true);
        try {
            addErrorDiagnostic(diagnostics, error);
        } catch (...) {
            diagnostics.clear();
        }
        return core::unexpected(std::move(error));
    } catch (const std::exception& exception) {
        auto error = prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", false,
                                           &exception);
        try {
            addErrorDiagnostic(diagnostics, error);
        } catch (...) {
            diagnostics.clear();
        }
        return core::unexpected(std::move(error));
    } catch (...) {
        auto error = prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", false);
        try {
            addErrorDiagnostic(diagnostics, error);
        } catch (...) {
            diagnostics.clear();
        }
        return core::unexpected(std::move(error));
    }
}

auto PlaybackSession::commit(PreparedPlayback&& prepared) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("commit"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence && !state_->hostCommitAuthorized)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence owns mutation"});
#endif
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("commit"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("commit"));
    }
    SessionOperation operation{state_->operationActive};
    if (!prepared.state_ || !prepared.state_->ownerThread.isCurrent()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    auto& candidate = *prepared.state_;
    if (candidate.owner != this || candidate.ownerToken != state_->sessionToken) {
        auto error = core::Error{"playback.prepared.wrong_session",
                                 "PreparedPlayback belongs to another PlaybackSession"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if (candidate.expectedGeneration != state_->generation) {
        auto error =
            core::Error{"playback.prepared.stale", "PlaybackSession changed after preparation"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if ((!candidate.replacement && state_->sessionState != SessionState::Empty) ||
        (candidate.replacement && state_->sessionState != SessionState::Ready &&
         state_->sessionState != SessionState::Running)) {
        auto error = core::Error{"playback.prepared.lifecycle_changed",
                                 "PlaybackSession lifecycle changed after preparation"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }

    auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock, true);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    detail::CommittedPresentationState descriptor;
    descriptor.source = candidate.replacement ? internal::presentation::Source::Restore
                                              : internal::presentation::Source::Prepared;
    descriptor.frame = candidate.targetFrame;
    state_->contentProvider = std::move(candidate.contentProvider);
    state_->resourceManager = std::move(candidate.resourceManager);
    state_->activeChartJson = std::move(candidate.chartJson);
    state_->runtimeSession = std::move(candidate.runtimeSession);
    state_->snapshotLayout = std::move(candidate.snapshotLayout);
    state_->activeChartInfo = candidate.chartInfo;
    state_->activeContentInfo = std::move(candidate.contentInfo);
    state_->activeMode = state_->activeContentInfo->mode;
    state_->semanticIdentity = candidate.semanticIdentity;
    state_->candidateMetadata = std::move(candidate.candidateMetadata);
    state_->parameters = std::move(candidate.parameters);
    state_->lastFrame = candidate.targetFrame;
    state_->diagnostics = std::move(candidate.diagnostics);
    state_->lastOperationDiagnostics = std::move(candidate.lastOperationDiagnostics);
    state_->sessionState = candidate.committedState;
    state_->presentation = std::move(candidate.presentation);
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    state_->gameplayContent = std::move(candidate.gameplayContent);
    state_->gameplayProjectionIndex = std::move(candidate.gameplayProjectionIndex);
    state_->pendingRetiredRequestFloor.reset();
    state_->executionScheduler.reset();
    state_->realtimeClock.reset();
    state_->executionContext.reset();
    state_->realtimeWasStarted = false;
    state_->gameplay = std::move(candidate.gameplay);
    state_->gameplayState = state_->gameplay ? GameplayState::Prepared : GameplayState::Inactive;
    state_->gameplayReceipt.reset();
    state_->gameplayProjection = {};
#endif
    state_->bumpGeneration();
    state_->publishPresentation(*mutation, descriptor);
    prepared.state_.reset();
    return {};
}

} // namespace cuexis::playback
