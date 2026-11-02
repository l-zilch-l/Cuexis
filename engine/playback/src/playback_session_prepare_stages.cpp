// Prepare stages: document loading, parameter resolution, runtime and identity assembly.

#include "playback_session_internal.hpp"
#include "playback_session_prepare_internal.hpp"

namespace cuexis::playback {
namespace prepare_detail {

[[nodiscard]] auto prepareCandidateStage(const cxc::CandidateChart& candidate, bool gameplaySource,
                                         const PlaybackPrepareOptions& options, bool replacement,
                                         const chart::ChartLimits& limits,
                                         PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::ChartRuntime> {
    if (!options.parameters.values.empty()) {
        for (std::size_t index = 0; index < options.parameters.values.size(); ++index) {
            diagnostics.add(
                core::Diagnostic{core::DiagnosticSeverity::Error, "chart.parameter.unknown",
                                 "Host parameter input is not supported by this Chart version",
                                 "$/parameterInputs/" + std::to_string(index) + "/id"});
        }
        diagnostics.sortDeterministically();
        return core::unexpected(operationError(
            replacement ? "playback.chart.reload_load_failed" : "playback.chart.load_failed",
            "Chart parameter resolution produced errors", diagnostics));
    }

    auto lowered =
        candidate.behavior ? chart::CandidateRuntimeArtifactResult{*candidate.behavior, {}}
        : gameplaySource   ? chart::lowerGameplayPresentationRuntime(candidate.semantic, limits)
                           : chart::lowerCandidateRuntime(candidate.semantic, limits);
    diagnostics.append(std::move(lowered.diagnostics));
    if (!lowered.hasValue()) {
        diagnostics.sortDeterministically();
        return core::unexpected(operationError(replacement ? "playback.chart.reload_compile_failed"
                                                           : "playback.chart.compile_failed",
                                               "Candidate lowering produced errors", diagnostics));
    }

    auto requirements =
        candidateResourceRequirements(lowered.artifact->metadata.resourceClosure, diagnostics);
    if (!requirements) {
        return core::unexpected(operationError("playback.candidate.resource_closure_invalid",
                                               "Candidate resource closure could not be prepared",
                                               diagnostics));
    }
    const bool renderable = std::ranges::any_of(lowered.artifact->runtime.objects,
                                                [](const chart::RuntimeObject& object) {
                                                    return object.components.renderable.has_value();
                                                });
    if (candidate.behavior) {
        artifact.additionalCapabilities.emplace_back(capabilityBehaviorEventV2);
        for (const auto& behavior : lowered.artifact->runtime.behaviors)
            for (const auto& track : behavior.stepTracks)
                if (track.property == chart::BehaviorStepProperty::RenderVisible)
                    artifact.additionalCapabilities.emplace_back(capabilityRenderVisibilityV1);
    }
    if (renderable) {
        artifact.additionalCapabilities.emplace_back(capabilityMaterialSnapshotV1);
    }
    artifact.resourceRequirements = std::move(*requirements);
    artifact.candidateMetadata = std::move(lowered.artifact->metadata);
    return std::move(lowered.artifact->runtime);
}

[[nodiscard]] auto loadDocumentStage(const PrepareContext& context, PrepareArtifact& artifact,
                                     core::Diagnostics& diagnostics) -> core::Result<void> {
    auto loaded = [&] {
        const internal::PrepareTraceScope trace{"load_chart"};
        return chart::detail::loadChartForPlayback(context.chartJson, context.limits);
    }();
    artifact.isV4 = loaded.isV4;
    const bool valid = loaded.hasValue();
    diagnostics.append(std::move(loaded.diagnostics));
    if (!valid) {
        return core::unexpected(core::Error{"playback.prepare.stage.load_document_failed",
                                            artifact.isV4 ? "Chart v4 loading produced errors"
                                                          : "Chart loading produced errors"});
    }
    if (artifact.isV4) {
        artifact.v4Source = std::move(*loaded.v4Document);
        artifact.v4Input = std::move(loaded.v4Input);
        return {};
    }
    artifact.document.emplace(std::move(*loaded.legacyDocument));
    // Legacy ChartDocument is a runtime-facing projection. Identity must use the complete
    // canonical source bytes so legacy timing, template, and keyframe fields are not erased.
    // The dispatcher canonicalizes the tree it already parsed, so this stage does not parse the
    // document a second time. A value the canonical writer rejects has no bytes to reuse and
    // travels the text entry point instead, which reports the writer error unchanged.
    const internal::PrepareTraceScope traceCanonical{"load_canonical"};
    std::string canonicalText;
    if (loaded.legacyCanonicalText.has_value()) {
        canonicalText = std::move(*loaded.legacyCanonicalText);
    } else {
        auto canonical = chart::ChartWriter::writeCanonicalJson(context.chartJson, context.limits);
        if (!canonical) {
            addErrorDiagnostic(diagnostics, canonical.error());
            return core::unexpected(std::move(canonical.error()));
        }
        canonicalText = std::move(*canonical);
    }
    artifact.chartIdentity = chart::canonicalBytesIdentity(canonicalText);
    artifact.parameterIdentity = chart::emptyParameterIdentity();
    return {};
}

[[nodiscard]] auto resolveParametersStage(const PrepareContext& context, PrepareArtifact& artifact,
                                          std::span<const PlaybackProjectDocument> sourceDocuments,
                                          core::Diagnostics& diagnostics) -> core::Result<void> {
    if (!artifact.v4Source) {
        for (std::size_t index = 0; index < context.options.parameters.values.size(); ++index) {
            diagnostics.add(
                core::Diagnostic{core::DiagnosticSeverity::Error, "chart.parameter.unknown",
                                 "Host parameter input is not supported by this Chart version",
                                 "$/parameterInputs/" + std::to_string(index) + "/id"});
        }
        if (diagnostics.hasErrors()) {
            diagnostics.sortDeterministically();
            return core::unexpected(core::Error{"playback.prepare.stage.resolve_parameters_failed",
                                                "Chart parameter resolution produced errors"});
        }
        return {};
    }

    auto inputs = chartParameterInputs(context.options, diagnostics);
    if (!inputs) {
        return core::unexpected(core::Error{"playback.prepare.stage.resolve_parameters_failed",
                                            "Chart parameter conversion produced errors"});
    }
    auto documents = projectDocuments(sourceDocuments);
    auto resolved = artifact.v4Input
                        ? chart::detail::resolveV4Parsed(*artifact.v4Source, *artifact.v4Input,
                                                         *inputs, documents, {}, context.limits)
                        : chart::ChartV4Resolver::resolve(*artifact.v4Source, *inputs, documents,
                                                          {}, context.limits);
    artifact.v4Input.reset();
    const bool valid = resolved.hasValue();
    diagnostics.append(std::move(resolved.diagnostics));
    if (!valid) {
        return core::unexpected(core::Error{"playback.prepare.stage.resolve_parameters_failed",
                                            "Chart v4 resolution produced errors"});
    }
    auto resolvedArtifact = std::move(*resolved.artifact);
    artifact.additionalCapabilities.insert(artifact.additionalCapabilities.end(),
                                           resolvedArtifact.capabilityRequirements.begin(),
                                           resolvedArtifact.capabilityRequirements.end());
    artifact.document.emplace(resolvedArtifact.document.chart);
    artifact.animationProgram = std::move(resolvedArtifact.animationProgram);
    artifact.chartIdentity = resolvedArtifact.chartIdentity;
    artifact.parameterIdentity = resolvedArtifact.parameterIdentity;
    artifact.cxtIdentities = resolvedArtifact.cxtIdentities;
    artifact.resourceRequirements = resolvedArtifact.resourceRequirements;
    artifact.v4Artifact = std::move(resolvedArtifact);
    return {};
}

[[nodiscard]] auto toPublicIdentity(const chart::CanonicalContentIdentity& identity) noexcept
    -> PreparedSemanticIdentity {
    return PreparedSemanticIdentity{identity.sha256};
}

[[nodiscard]] auto
assembleResourceIdentities(std::span<const chart::ChartResourceRequirement> requirements,
                           const std::optional<assets::AudioSourceLease>& audioSourceLease,
                           const std::optional<detail::PreparedPresentation>& presentation,
                           core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::PreparedResourceIdentityComponent>> {
    std::vector<chart::PreparedResourceIdentityComponent> identities;
    identities.reserve(requirements.size());
    for (const auto& requirement : requirements) {
        std::optional<chart::CanonicalContentIdentity> identity;
        const bool wantsAudio =
            std::ranges::any_of(requirement.uses, [](chart::ChartResourceUse use) {
                return use == chart::ChartResourceUse::MainMusic;
            });
        const bool wantsPresentation =
            std::ranges::any_of(requirement.uses, [](chart::ChartResourceUse use) {
                return use != chart::ChartResourceUse::MainMusic;
            });
        if (wantsAudio) {
            if (!audioSourceLease || !audioSourceLease->valid() ||
                audioSourceLease->resource().id.value != requirement.assetId.value) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                    "Prepared semantic identity is missing a fetched main music resource",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            identity = chart::audioContentIdentity(audioSourceLease->resource().bytes());
        }
        if (wantsPresentation) {
            if (!presentation) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                    "Prepared semantic identity is missing a fetched presentation resource",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            const auto found = std::ranges::find_if(
                presentation->manifest.entries, [&](const PresentationManifestEntry& entry) {
                    return entry.reference.assetId == requirement.assetId.value;
                });
            if (found == presentation->manifest.entries.end()) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                    "Prepared semantic identity is missing a fetched presentation resource",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            chart::CanonicalContentIdentity presentationIdentity{found->reference.identity.sha256};
            if (identity && *identity != presentationIdentity) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_collision",
                    "The same AssetId produced different prepared resource identities",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            identity = presentationIdentity;
        }
        if (!identity) {
            diagnostics.add(core::Diagnostic{
                core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                "Prepared semantic identity could not resolve a required resource",
                "$/resources/" + requirement.assetId.value});
            continue;
        }
        identities.push_back({requirement.assetId, *identity});
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return std::nullopt;
    }
    return identities;
}

[[nodiscard]] auto chartInfoFor(const chart::ChartRuntime& chartRuntime, std::size_t resourceCount)
    -> ChartInfo {
    return ChartInfo{
        .objectCount = chartRuntime.objects.size(),
        .behaviorCount = chartRuntime.behaviors.size(),
        .renderableCount = static_cast<std::size_t>(
            std::count_if(chartRuntime.objects.begin(), chartRuntime.objects.end(),
                          [](const chart::RuntimeObject& object) {
                              return object.components.renderable.has_value();
                          })),
        .resourceCount = resourceCount,
    };
}

[[nodiscard]] auto buildSnapshotLayout(runtime::RuntimeSession& session,
                                       const chart::ChartRuntime& chartRuntime,
                                       const detail::PreparedPresentation* presentation)
    -> core::Result<SnapshotLayout> {
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
    // Product tag: the prepared snapshot layout is capped as state.candidate.
    const internal::pd::memory::TagScope tag{internal::pd::memory::ownerMain,
                                             internal::pd::memory::domainCandidate};
#endif
    SnapshotLayout layout;
    layout.entities.reserve(chartRuntime.objects.size());
    for (const auto& object : chartRuntime.objects) {
        auto entity = session.findEntity(object.id);
        if (!entity) {
            return core::unexpected(std::move(entity.error()));
        }
        if (!entity->has_value()) {
            return core::unexpected(core::Error{"playback.snapshot.object_missing",
                                                "Runtime object mapping is incomplete"}
                                        .withContext("object_id", object.id.value));
        }
        SnapshotEntity snapshotEntity{
            .id = object.id,
            .runtimeIndex = layout.entities.size(),
            .mesh = std::nullopt,
            .materials = {},
            .maximumMeshAssetIdSize = 0,
            .maximumMaterialAssetIdSize = 0,
        };
        if (object.components.renderable) {
            snapshotEntity.maximumMeshAssetIdSize = object.components.renderable->mesh.value.size();
            snapshotEntity.maximumMaterialAssetIdSize =
                object.components.renderable->material.value.size();
            if (presentation != nullptr) {
                const auto* mesh = detail::findPresentationResource(
                    *presentation, object.components.renderable->mesh.value,
                    PresentationResourceType::Mesh);
                if (mesh == nullptr) {
                    return core::unexpected(snapshotResourceMismatch(
                        object.id.value, object.components.renderable->mesh.value,
                        PresentationResourceType::Mesh));
                }
                snapshotEntity.mesh = (*mesh)->reference;

                const auto addMaterial = [&](std::string_view assetId) -> core::Result<void> {
                    const auto* unlit = detail::findPresentationResource(
                        *presentation, assetId, PresentationResourceType::UnlitMaterial);
                    const auto* parameterized = detail::findPresentationResource(
                        *presentation, assetId, PresentationResourceType::ParameterizedMaterial);
                    const auto* material = unlit != nullptr ? unlit : parameterized;
                    if (material == nullptr) {
                        return core::unexpected(snapshotResourceMismatch(
                            object.id.value, assetId, PresentationResourceType::UnlitMaterial));
                    }
                    if ((*material)->reference.type == PresentationResourceType::Shader) {
                        return core::unexpected(snapshotResourceMismatch(
                            object.id.value, assetId, PresentationResourceType::Shader));
                    }
                    snapshotEntity.maximumMaterialAssetIdSize =
                        std::max(snapshotEntity.maximumMaterialAssetIdSize, assetId.size());
                    snapshotEntity.materials.emplace(std::string{assetId}, (*material)->reference);
                    return {};
                };
                if (auto added = addMaterial(object.components.renderable->material.value);
                    !added) {
                    return core::unexpected(std::move(added.error()));
                }
            }
        }
        if (object.components.behavior) {
            const auto behavior =
                std::lower_bound(chartRuntime.behaviors.begin(), chartRuntime.behaviors.end(),
                                 object.components.behavior->behavior,
                                 [](const chart::RuntimeBehavior& candidate,
                                    const chart::BehaviorId& id) { return candidate.id < id; });
            if (behavior != chartRuntime.behaviors.end() &&
                behavior->id == object.components.behavior->behavior) {
                for (const auto& track : behavior->stepTracks) {
                    if (track.property != chart::BehaviorStepProperty::RenderMaterial) {
                        continue;
                    }
                    for (const auto& event : track.events) {
                        if (const auto* material = std::get_if<chart::AssetId>(&event.value)) {
                            snapshotEntity.maximumMaterialAssetIdSize = std::max(
                                snapshotEntity.maximumMaterialAssetIdSize, material->value.size());
                            if (presentation != nullptr && object.components.renderable) {
                                const auto* unlit = detail::findPresentationResource(
                                    *presentation, material->value,
                                    PresentationResourceType::UnlitMaterial);
                                const auto* parameterized = detail::findPresentationResource(
                                    *presentation, material->value,
                                    PresentationResourceType::ParameterizedMaterial);
                                const auto* resource = unlit != nullptr ? unlit : parameterized;
                                if (resource == nullptr) {
                                    return core::unexpected(snapshotResourceMismatch(
                                        object.id.value, material->value,
                                        PresentationResourceType::UnlitMaterial));
                                }
                                snapshotEntity.materials.emplace(material->value,
                                                                 (*resource)->reference);
                            }
                        }
                    }
                }
            }
        }
        layout.entities.push_back(std::move(snapshotEntity));
    }

    auto revision = runtime::detail::PresentationAccess::state(session);
    if (!revision)
        return core::unexpected(std::move(revision.error()));
    layout.runtimeOwner = revision->stamp.owner;
    layout.runtimeLayout = revision->stamp.layout;
    return layout;
}

[[nodiscard]] auto acquireAudioStage(PrepareArtifact& artifact,
                                     const chart::ChartRuntime& chartRuntime,
                                     core::Diagnostics& diagnostics) -> core::Result<void> {
    if (!chartRuntime.mainMusic) {
        return {};
    }
    if (!artifact.resourceManager) {
        return core::unexpected(
            core::Error{"playback.content.asset_database_missing",
                        "A chart with main music requires an AssetDatabase and ContentProvider"});
    }
    auto sourceResult = artifact.resourceManager->requestAudioSource(
        assets::AssetId{chartRuntime.mainMusic->value}, assets::ResourcePolicy::Required);
    const bool sourceValid = sourceResult.hasValue();
    diagnostics.append(std::move(sourceResult.diagnostics));
    if (!sourceValid) {
        return core::unexpected(operationError("playback.content.main_music_failed",
                                               "Required main music source could not be prepared",
                                               diagnostics));
    }
    artifact.audioSourceLease.emplace(std::move(*sourceResult.lease));
    return {};
}

[[nodiscard]] auto prepareRuntimeStage(PrepareArtifact& artifact,
                                       const chart::ChartRuntime& chartRuntime,
                                       animation::AnimationProgram animationProgram,
                                       core::Diagnostics& diagnostics) -> core::Result<void> {
    artifact.runtimeSession =
        artifact.resourceManager
            ? std::make_unique<runtime::RuntimeSession>(*artifact.resourceManager)
            : std::make_unique<runtime::RuntimeSession>();
    auto runtimePrepared =
        artifact.runtimeSession->prepare(chartRuntime, std::move(animationProgram));
    const bool preparedValid = runtimePrepared.hasValue();
    diagnostics.append(std::move(runtimePrepared.diagnostics));
    if (!preparedValid) {
        return core::unexpected(operationError("playback.session.prepare_failed",
                                               "RuntimeSession preparation produced errors",
                                               diagnostics));
    }
    artifact.preparedRuntime = std::move(*runtimePrepared.prepared);
    return {};
}

[[nodiscard]] auto commitRuntimeStage(PrepareArtifact& artifact) -> core::Result<void> {
    if (!artifact.runtimeSession || !artifact.preparedRuntime) {
        return core::unexpected(core::Error{"playback.prepare.stage.commit_runtime_missing",
                                            "Runtime preparation artifact is unavailable"});
    }
    if (auto committed = artifact.runtimeSession->commit(std::move(*artifact.preparedRuntime));
        !committed) {
        return core::unexpected(std::move(committed.error()));
    }
    artifact.preparedRuntime.reset();
    return {};
}

[[nodiscard]] auto preparePresentationStage(const PrepareContext& context,
                                            PrepareArtifact& artifact,
                                            const chart::ChartRuntime& chartRuntime,
                                            core::Diagnostics& diagnostics,
                                            bool checkHostCapabilities) -> core::Result<void> {
    auto preparedPresentation = detail::preparePresentation(
        chartRuntime, artifact.resourceManager.get(), artifact.v4Artifact.has_value());
    if (!preparedPresentation) {
        addErrorDiagnostic(diagnostics, preparedPresentation.error());
        diagnostics.sortDeterministically();
        return core::unexpected(std::move(preparedPresentation.error()));
    }
    if (preparedPresentation->has_value() && checkHostCapabilities) {
        std::vector<std::string> presentationCapabilities;
        bool requiresShader = false;
        bool requiresParameterized = false;
        for (const auto& entry : preparedPresentation->value().manifest.entries) {
            if (entry.reference.type == PresentationResourceType::Shader) {
                requiresShader = true;
            } else if (entry.reference.type == PresentationResourceType::ParameterizedMaterial) {
                requiresParameterized = true;
                requiresShader = true;
            }
        }
        if (requiresShader) {
            presentationCapabilities.emplace_back(capabilityShaderAssetV1);
        }
        if (requiresParameterized) {
            presentationCapabilities.emplace_back(capabilityMaterialParameterizedV1);
        }
        const bool capabilitiesOk =
            artifact.document
                ? preflightCapabilities(*artifact.document, presentationCapabilities,
                                        context.capabilities, diagnostics)
                : preflightCapabilities(chart::ChartDocument{}, presentationCapabilities,
                                        context.capabilities, diagnostics);
        if (!presentationCapabilities.empty() && !capabilitiesOk) {
            return core::unexpected(operationError("playback.capability.preflight_failed",
                                                   "Playback capability preflight failed",
                                                   diagnostics));
        }
    }
    artifact.presentation = std::move(*preparedPresentation);
    return {};
}

[[nodiscard]] auto commitFrameStage(const PrepareContext& context, PrepareArtifact& artifact,
                                    core::Diagnostics& diagnostics) -> core::Result<void> {
    if (!context.replacement || context.targetFrame == nullptr) {
        return {};
    }
    artifact.committedFrame = *context.targetFrame;
    artifact.committedFrame->simulationDeltaTimeMs = 0.0;
    if (context.policy == ReloadPolicy::RestartAtZero) {
        artifact.committedFrame->chartTimeMs = 0.0;
    }
    if (auto updated = artifact.runtimeSession->update(runtimeFrame(*artifact.committedFrame));
        !updated) {
        addErrorDiagnostic(diagnostics, updated.error());
        diagnostics.sortDeterministically();
        return core::unexpected(operationError("playback.session.reload_sample_failed",
                                               "Reload target frame sampling failed", diagnostics));
    }
    return {};
}

[[nodiscard]] auto assembleIdentityStage(PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::CanonicalContentIdentity> {
    if (artifact.candidateMetadata) {
        auto resourceIdentities =
            assembleResourceIdentities(artifact.resourceRequirements, artifact.audioSourceLease,
                                       artifact.presentation, diagnostics);
        if (!resourceIdentities) {
            return core::unexpected(
                operationError("playback.identity.assemble_failed",
                               "Prepared semantic identity could not be assembled", diagnostics));
        }
        auto identity = chart::assembleCandidatePreparedSemanticIdentity(
            artifact.candidateMetadata->semanticIdentity, *resourceIdentities);
        if (!identity) {
            addErrorDiagnostic(diagnostics, identity.error());
            diagnostics.sortDeterministically();
            return core::unexpected(operationError(
                "playback.identity.assemble_failed",
                "Candidate prepared semantic identity could not be assembled", diagnostics));
        }
        return std::move(*identity);
    }
    if (!artifact.v4Artifact) {
        if (artifact.audioSourceLease && artifact.audioSourceLease->valid()) {
            artifact.resourceRequirements.push_back(chart::ChartResourceRequirement{
                .assetId =
                    chart::AssetId{std::string{artifact.audioSourceLease->resource().id.value}},
                .uses = {chart::ChartResourceUse::MainMusic}});
        }
        if (artifact.presentation.has_value()) {
            for (const auto& entry : artifact.presentation->manifest.entries) {
                const auto existing =
                    std::ranges::find_if(artifact.resourceRequirements,
                                         [&](const chart::ChartResourceRequirement& item) {
                                             return item.assetId.value == entry.reference.assetId;
                                         });
                auto use = entry.reference.type == PresentationResourceType::Mesh
                               ? chart::ChartResourceUse::RenderableMesh
                               : chart::ChartResourceUse::RenderableMaterial;
                if (existing == artifact.resourceRequirements.end()) {
                    artifact.resourceRequirements.push_back(chart::ChartResourceRequirement{
                        .assetId = chart::AssetId{entry.reference.assetId}, .uses = {use}});
                } else if (std::ranges::find(existing->uses, use) == existing->uses.end()) {
                    existing->uses.push_back(use);
                }
            }
        }
    }
    auto resourceIdentities =
        assembleResourceIdentities(artifact.resourceRequirements, artifact.audioSourceLease,
                                   artifact.presentation, diagnostics);
    if (!resourceIdentities) {
        return core::unexpected(operationError("playback.identity.assemble_failed",
                                               "Prepared semantic identity could not be assembled",
                                               diagnostics));
    }
    return chart::assemblePreparedSemanticIdentity(*artifact.chartIdentity, artifact.cxtIdentities,
                                                   *resourceIdentities,
                                                   *artifact.parameterIdentity);
}

} // namespace prepare_detail
} // namespace cuexis::playback
