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

struct PreparedPlayback::State final {
    PlaybackSession* owner{};
    std::uint64_t ownerToken{};
    std::uint64_t expectedGeneration{};
    bool replacement{};
    core::ThreadChecker ownerThread;
    std::shared_ptr<content::IContentProvider> contentProvider;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::string chartJson;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    SnapshotLayout snapshotLayout;
    ChartInfo chartInfo;
    PlaybackContentInfo contentInfo;
    ChartParameterSet parameters;
    PreparedSemanticIdentity semanticIdentity;
    std::optional<chart::CandidateRuntimeMetadata> candidateMetadata;
    std::optional<assets::AudioSourceLease> audioSourceLease;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<RuntimeFrame> targetFrame;
    SessionState committedState{SessionState::Ready};
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t candidateGeneration{};
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    GameplayContent gameplayContent;
    std::unique_ptr<judgement::JudgementSession> gameplay;
    detail::PreparedProjection gameplayProjectionIndex;
#endif
};

auto PreparedPlayback::materializePresentation(const PresentationCapabilities& capabilities,
                                               const PresentationRequest& request,
                                               std::uint64_t rendererInstance,
                                               std::uint64_t rendererGeneration,
                                               std::uint64_t sequence)
    -> core::Result<PresentationCandidateInput> try {
    auto& prepared = *this;
    if (!prepared.valid() || !prepared.state_->owner || !rendererInstance || !rendererGeneration ||
        !sequence)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Invalid owning candidate context"});
    auto validation = prepared.validatePresentation(capabilities, request);
    if (!validation.hasValue())
        return core::unexpected(
            core::Error{"host.rpa.device_failed", "Candidate capabilities rejected"});
    const auto* manifest = prepared.presentationManifest();
    auto token = prepared.presentationCandidateToken();
    if (!manifest || !token)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Portable candidate missing"});
    const auto& p = *prepared.state_;
    PresentationCandidateInput value;
    value.context = {p.ownerToken,      1, 1, sequence, p.candidateGeneration, rendererGeneration,
                     p.semanticIdentity};
    value.playbackOwnerToken = p.ownerToken;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (p.owner && p.owner->state_->executionContext) {
        const auto& execution = *p.owner->state_->executionContext;
        value.context.sessionScope = execution.sessionScope;
        value.context.clockSegment = execution.clockSegment;
        value.context.runEpoch = execution.runEpoch;
    }
#endif
    value.mutationGeneration = p.expectedGeneration;
    value.candidateId = p.candidateGeneration;
    value.rendererInstance = rendererInstance;
    value.token = *token;
    value.capabilities = capabilities;
    value.manifest = *manifest;
    value.settings = *validation.settings;
    value.resources.reserve(manifest->entries.size());
    for (const auto& entry : manifest->entries) {
        auto resource = prepared.acquirePresentationResource(entry.reference);
        if (!resource)
            return core::unexpected(resource.error());
        value.resources.push_back(std::move(*resource));
    }
    if (const auto source = prepared.mainMusicSource())
        value.audio = AudioCandidateInput{std::string{source->assetId},
                                          source->timingOffsetMs,
                                          source->contentRevision,
                                          {source->bytes.begin(), source->bytes.end()}};
    return value;
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Owning candidate reservation failed"});
}

static_assert(std::is_nothrow_move_assignable_v<core::Diagnostics>);
static_assert(std::is_nothrow_move_assignable_v<PlaybackContentInfo>);
static_assert(std::is_nothrow_move_assignable_v<detail::PreparedPresentation>);
static_assert(static_cast<std::uint8_t>(HostPropertyId::TransformPositionX) ==
              static_cast<std::uint8_t>(world::PropertyId::TransformPositionX));
static_assert(static_cast<std::uint8_t>(HostPropertyId::MaterialTint) ==
              static_cast<std::uint8_t>(world::PropertyId::MaterialTint));
static_assert(static_cast<std::uint8_t>(HostOverrideLifetimeKind::UntilReleased) ==
              static_cast<std::uint8_t>(world::OverrideLifetimeKind::UntilReleased));
static_assert(static_cast<std::uint8_t>(HostOverrideLifetimeKind::UntilChartTimeMs) ==
              static_cast<std::uint8_t>(world::OverrideLifetimeKind::UntilChartTimeMs));

PreparedPlayback::PreparedPlayback() noexcept = default;

PreparedPlayback::PreparedPlayback(std::unique_ptr<State> state) noexcept
    : state_(std::move(state)) {}

PreparedPlayback::~PreparedPlayback() {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

PreparedPlayback::PreparedPlayback(PreparedPlayback&& other) noexcept
    : state_(std::move(other.state_)) {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

auto PreparedPlayback::operator=(PreparedPlayback&& other) noexcept -> PreparedPlayback& {
    if ((state_ && !state_->ownerThread.isCurrent()) ||
        (other.state_ && !other.state_->ownerThread.isCurrent())) {
        std::terminate();
    }
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

bool PreparedPlayback::valid() const noexcept {
    return state_ && state_->ownerThread.isCurrent() && state_->runtimeSession != nullptr;
}

const PlaybackContentInfo* PreparedPlayback::contentInfo() const noexcept {
    return valid() ? &state_->contentInfo : nullptr;
}

auto detail::CandidateMetadataAccess::prepared(const PreparedPlayback& playback)
    -> const chart::CandidateRuntimeMetadata* {
    if (playback.state_ == nullptr || !playback.state_->candidateMetadata) {
        return nullptr;
    }
    return &*playback.state_->candidateMetadata;
}

auto detail::CandidateMetadataAccess::active(const PlaybackSession& session)
    -> const chart::CandidateRuntimeMetadata* {
    if (session.state_ == nullptr || !session.state_->candidateMetadata) {
        return nullptr;
    }
    return &*session.state_->candidateMetadata;
}

std::optional<PreparedSemanticIdentity> PreparedPlayback::semanticIdentity() const noexcept {
    if (!valid()) {
        return std::nullopt;
    }
    return state_->semanticIdentity;
}

std::optional<MainMusicSourceView> PreparedPlayback::mainMusicSource() const noexcept {
    if (!valid() || !state_->audioSourceLease || !state_->audioSourceLease->valid()) {
        return std::nullopt;
    }
    const auto& source = state_->audioSourceLease->resource();
    return MainMusicSourceView{source.id.value, state_->contentInfo.timingOffsetMs,
                               source.blob->providerRevision, source.bytes()};
}

auto PreparedPlayback::presentationCandidateToken() const
    -> core::Result<PresentationCandidateToken> {
    if (!state_ || !state_->ownerThread.isCurrent()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PreparedPlayback has no portable presentation candidate"});
    }
    PresentationCandidateToken token;
    token.sessionToken_ = state_->ownerToken;
    token.candidateGeneration_ = state_->candidateGeneration;
    return token;
}

const PresentationResourceManifest* PreparedPlayback::presentationManifest() const noexcept {
    if (!valid() || !state_->presentation) {
        return nullptr;
    }
    return &state_->presentation->manifest;
}

auto PreparedPlayback::validatePresentation(const PresentationCapabilities& capabilities,
                                            const PresentationRequest& request) const
    -> PresentationValidationResult {
    PresentationValidationResult result;
    try {
        result.diagnostics = presentationDiagnostics();
        if (!state_ || !state_->ownerThread.isCurrent() || state_->runtimeSession == nullptr) {
            addPresentationDiagnostic(
                result.diagnostics, core::DiagnosticSeverity::Error, "playback.prepared.invalid",
                "PreparedPlayback is empty or belongs to another thread", "$/prepared");
            result.diagnostics.sortDeterministically();
            return result;
        }

        if (capabilities.version != 1 && capabilities.version != 2) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.version_unsupported",
                                      "Presentation capability set version is unsupported",
                                      "$/capabilities/version", "capabilities.version",
                                      std::to_string(capabilities.version), "1 or 2");
        }
        if (request.version != 1 && request.version != 2) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.version_unsupported",
                                      "Presentation request version is unsupported",
                                      "$/request/version", "request.version",
                                      std::to_string(request.version), "1 or 2");
        }
        if (capabilities.portableProfileVersion != 1) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.profile_unsupported",
                                      "Adapter portable presentation profile is unsupported",
                                      "$/capabilities/portableProfileVersion",
                                      "capabilities.portable_profile",
                                      std::to_string(capabilities.portableProfileVersion), "1");
        }
        if (request.portableProfileVersion != 1) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.profile_unsupported",
                                      "Requested portable presentation profile is unsupported",
                                      "$/request/portableProfileVersion",
                                      "request.portable_profile",
                                      std::to_string(request.portableProfileVersion), "1");
        }

        if (state_->presentation) {
            for (const auto& rule : presentationCapabilityRules) {
                if (!(capabilities.*rule.supported)) {
                    addMissingPresentationCapability(result.diagnostics, rule.id, rule.fieldPath);
                }
            }

            std::uint64_t maxResourceBytes = 0;
            std::uint32_t maxTextureDimension = 0;
            std::uint32_t maxMeshVertices = 0;
            std::uint32_t maxMeshIndices = 0;
            for (const auto& entry : state_->presentation->manifest.entries) {
                maxResourceBytes = std::max(
                    maxResourceBytes, std::max(entry.encodedByteCount, entry.decodedByteCount));
            }
            for (const auto& resource : state_->presentation->orderedResources) {
                if (!resource) {
                    continue;
                }
                if (const auto* texture = std::get_if<PortableTexture2D>(&resource->value)) {
                    maxTextureDimension =
                        std::max(maxTextureDimension, std::max(texture->width, texture->height));
                } else if (const auto* mesh = std::get_if<PortableMesh>(&resource->value)) {
                    maxMeshVertices = std::max(
                        maxMeshVertices, static_cast<std::uint32_t>(mesh->positions.size() / 3U));
                    maxMeshIndices =
                        std::max(maxMeshIndices, static_cast<std::uint32_t>(mesh->indices.size()));
                }
            }

            const std::array<std::uint64_t, basePresentationLimitRules.size()> baseRequirements{
                maxResourceBytes, state_->presentation->manifest.totalDecodedBytes,
                maxTextureDimension, maxMeshVertices, maxMeshIndices};
            for (std::size_t index = 0; index < basePresentationLimitRules.size(); ++index) {
                const auto& rule = basePresentationLimitRules[index];
                const auto actual = presentationLimitValue(capabilities, rule.value);
                if (actual < baseRequirements[index]) {
                    addInsufficientPresentationLimit(result.diagnostics, rule.id, actual,
                                                     baseRequirements[index], rule.fieldPath);
                }
            }

            bool hasParameterizedMaterial = false;
            bool hasShader = false;
            bool hasDeclaredVariants = false;
            std::set<std::string> requiredHostExtensions;
            for (const auto& resource : state_->presentation->orderedResources) {
                if (!resource) {
                    continue;
                }
                if (const auto* shader = std::get_if<PortableShader>(&resource->value)) {
                    hasShader = true;
                    if (!shader->variantKeywords.empty()) {
                        hasDeclaredVariants = true;
                    }
                    for (const auto& extension : shader->requiredHostExtensions) {
                        requiredHostExtensions.insert(extension);
                    }
                } else if (std::holds_alternative<PortableParameterizedMaterial>(resource->value)) {
                    hasParameterizedMaterial = true;
                }
            }

            if (hasParameterizedMaterial &&
                (capabilities.version < 2 || !capabilities.parameterizedMaterial)) {
                addMissingPresentationCapability(result.diagnostics, "parameterized_material",
                                                 "$/capabilities/parameterizedMaterial");
            }

            if (capabilities.version == 2) {
                for (const auto& extension : requiredHostExtensions) {
                    if (std::find(capabilities.hostExtensionIds.begin(),
                                  capabilities.hostExtensionIds.end(),
                                  extension) == capabilities.hostExtensionIds.end()) {
                        addMissingPresentationCapability(result.diagnostics, extension,
                                                         "$/capabilities/hostExtensionIds");
                    }
                }
                if (hasDeclaredVariants && capabilities.parameterizedMaterial &&
                    !capabilities.declaredVariants) {
                    addMissingPresentationCapability(result.diagnostics, "declared_variants",
                                                     "$/capabilities/declaredVariants");
                }
                if (capabilities.parameterizedMaterial && (hasShader || hasParameterizedMaterial)) {
                    for (const auto& rule : parameterizedPresentationLimitRules) {
                        const auto actual = presentationLimitValue(capabilities, rule.value);
                        if (actual < rule.required) {
                            addInsufficientPresentationLimit(result.diagnostics, rule.id, actual,
                                                             rule.required, rule.fieldPath);
                        }
                    }
                }
            }
        }

        bool debugPassEnabled = request.enableDebugPass;
        if (request.enableDebugPass && !capabilities.debugPass) {
            debugPassEnabled = false;
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Warning,
                                      "playback.presentation.debug_unavailable",
                                      "Requested optional Debug presentation pass is unavailable",
                                      "$/request/enableDebugPass", "debug_pass", "unsupported");
        }

        bool shaderCompileEnabled = false;
        bool shaderHotReloadEnabled = false;
        if (request.version == 2) {
            const bool compileSupported = capabilities.version == 2 &&
                                          capabilities.shaderGlsl450Source &&
                                          capabilities.shaderSpirv && capabilities.shaderGlsl330;
            if (request.enableShaderCompile) {
                if (!compileSupported) {
                    addMissingPresentationCapability(result.diagnostics, "shader_compile",
                                                     "$/request/enableShaderCompile");
                } else {
                    shaderCompileEnabled = true;
                }
            }
            if (request.enableShaderHotReload) {
                if (!compileSupported) {
                    addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Warning,
                                              "playback.presentation.shader_hot_reload_unavailable",
                                              "Requested shader hot reload is unavailable",
                                              "$/request/enableShaderHotReload",
                                              "shader_hot_reload", "unsupported");
                } else {
                    shaderHotReloadEnabled = true;
                }
            }
        }

        result.diagnostics.sortDeterministically();
        if (!result.diagnostics.hasErrors()) {
            if (request.version == 2) {
                result.settings = EffectivePresentationSettings{
                    .version = 2,
                    .portableProfileVersion = 1,
                    .debugPassEnabled = debugPassEnabled,
                    .shaderCompileEnabled = shaderCompileEnabled,
                    .shaderHotReloadEnabled = shaderHotReloadEnabled,
                };
            } else {
                result.settings = EffectivePresentationSettings{
                    .version = 1,
                    .portableProfileVersion = 1,
                    .debugPassEnabled = debugPassEnabled,
                };
            }
        }
        return result;
    } catch (const std::bad_alloc&) {
        result.settings.reset();
        result.diagnostics.clear();
        result.diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "playback.presentation.diagnostics.limit_exceeded",
            "Presentation validation could not allocate its bounded diagnostics", "$/prepared"});
        return result;
    } catch (const std::exception& exception) {
        result.settings.reset();
        result.diagnostics.clear();
        result.diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                                "playback.presentation.prepare_failed",
                                                "Presentation validation failed", "$/prepared"}
                                   .withContext("exception", exception.what()));
        return result;
    } catch (...) {
        result.settings.reset();
        result.diagnostics.clear();
        result.diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                                "playback.presentation.prepare_failed",
                                                "Presentation validation failed", "$/prepared"});
        return result;
    }
}

auto PreparedPlayback::acquirePresentationResource(const PresentationResourceRef& reference) const
    -> core::Result<PortableResourcePtr> {
    if (!state_ || !state_->ownerThread.isCurrent()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PreparedPlayback has no portable presentation candidate"});
    }
    const auto* resource = detail::findPresentationResource(*state_->presentation, reference);
    if (resource == nullptr) {
        return core::unexpected(
            core::Error{"playback.presentation.reference.invalid",
                        "Portable presentation resource reference is not in the candidate manifest"}
                .withContext("asset_id", reference.assetId)
                .withContext("resource_type",
                             std::string{presentationResourceTypeName(reference.type)}));
    }
    return *resource;
}

PlaybackSession::PlaybackSession() noexcept : PlaybackSession(allCapabilities()) {}

PlaybackSession::PlaybackSession(PlaybackCapabilitySet capabilities) noexcept
    : state_(std::make_unique<State>()) {
    normalizeCapabilities(capabilities);
    state_->capabilities = std::move(capabilities);
}

PlaybackSession::~PlaybackSession() {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
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