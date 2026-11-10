// PreparedPlayback: the owning prepared session, its presentation and its candidate metadata.

#include "playback_session_internal.hpp"
#include "playback_session_prepare_internal.hpp"

namespace cuexis::playback {

using namespace prepare_detail;

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

} // namespace cuexis::playback
