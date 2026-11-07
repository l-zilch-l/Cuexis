#include "validation_sink.hpp"

#include "validation_sink_model.hpp"

// Public identity, candidate and validation entry points.

namespace cuexis::test_support {

using namespace validation_detail;

auto computePresentationIdentity(const playback::PortableResourceValue& value) noexcept
    -> playback::PresentationContentIdentity {
    if (const auto* mesh = std::get_if<playback::PortableMesh>(&value)) {
        return meshIdentity(*mesh);
    }
    if (const auto* texture = std::get_if<playback::PortableTexture2D>(&value)) {
        return textureIdentity(*texture);
    }
    if (const auto* unlit = std::get_if<playback::PortableUnlitMaterial>(&value)) {
        return materialIdentity(*unlit);
    }
    if (const auto* shader = std::get_if<playback::PortableShader>(&value)) {
        CanonicalHash hash{playback::PresentationResourceType::Shader};
        hash.writeString(shader->vertexEntry);
        hash.writeString(shader->fragmentEntry);
        auto keywords = shader->variantKeywords;
        std::sort(keywords.begin(), keywords.end());
        hash.writeU32(static_cast<std::uint32_t>(keywords.size()));
        for (const auto& keyword : keywords) {
            hash.writeString(keyword);
        }
        hash.writeU32(static_cast<std::uint32_t>(shader->parameters.size()));
        for (const auto& parameter : shader->parameters) {
            hash.writeString(parameter.name);
            hash.writeU32(static_cast<std::uint32_t>(parameter.type));
            hash.writeU32(parameter.set);
            hash.writeU32(parameter.binding);
            for (const auto lane : parameter.defaultNumeric) {
                hash.writeFloat(lane);
            }
            hash.writeU32(std::bit_cast<std::uint32_t>(parameter.defaultInt));
            hash.writeU32(parameter.defaultBool ? 1U : 0U);
        }
        hash.writeU32(static_cast<std::uint32_t>(shader->bindings.size()));
        for (const auto& binding : shader->bindings) {
            hash.writeU32(binding.set);
            hash.writeU32(binding.binding);
            hash.writeU32(static_cast<std::uint32_t>(binding.type));
            hash.writeString(binding.name);
        }
        hash.writeU32(static_cast<std::uint32_t>(shader->defaultAlphaMode));
        hash.writeU32(shader->defaultDoubleSided ? 1U : 0U);
        hash.writeString(shader->requiredRendererProfile);
        auto extensions = shader->requiredHostExtensions;
        std::sort(extensions.begin(), extensions.end());
        hash.writeU32(static_cast<std::uint32_t>(extensions.size()));
        for (const auto& extension : extensions) {
            hash.writeString(extension);
        }
        hash.writeString(shader->vertexSource);
        hash.writeString(shader->fragmentSource);
        return hash.finish();
    }
    const auto& material = std::get<playback::PortableParameterizedMaterial>(value);
    CanonicalHash hash{playback::PresentationResourceType::ParameterizedMaterial};
    hash.writeU32(static_cast<std::uint32_t>(material.alphaMode));
    hash.writeU32(material.doubleSided ? 1U : 0U);
    hash.writeString(material.shader.assetId);
    hash.writeIdentity(material.shader.identity);
    auto keywords = material.selectedKeywords;
    std::sort(keywords.begin(), keywords.end());
    hash.writeU32(static_cast<std::uint32_t>(keywords.size()));
    for (const auto& keyword : keywords) {
        hash.writeString(keyword);
    }
    hash.writeU32(static_cast<std::uint32_t>(material.parameters.size()));
    for (const auto& parameter : material.parameters) {
        hash.writeString(parameter.name);
        hash.writeU32(static_cast<std::uint32_t>(parameter.type));
        if (parameter.type == playback::ShaderParameterType::Texture2D) {
            hash.writeU32(parameter.texture ? 1U : 0U);
            if (parameter.texture) {
                hash.writeString(parameter.texture->assetId);
                hash.writeIdentity(parameter.texture->identity);
            }
        } else if (parameter.type == playback::ShaderParameterType::Int) {
            hash.writeU32(std::bit_cast<std::uint32_t>(parameter.integer));
        } else if (parameter.type == playback::ShaderParameterType::Bool) {
            hash.writeU32(parameter.boolean ? 1U : 0U);
        } else {
            for (const auto lane : parameter.numeric) {
                hash.writeFloat(lane);
            }
        }
    }
    return hash.finish();
}

void ValidationSummary::clear() noexcept {
    version = validationSummaryVersion;
    viewportWidth = 0;
    viewportHeight = 0;
    clearColor.fill(0.0F);
    cameraActive = false;
    viewMatrix.fill(0.0F);
    projectionMatrix.fill(0.0F);
    debugPassEnabled = false;
    opaque.clear();
    transparent.clear();
    digest = 0;
}

ValidationCandidate::ValidationCandidate(playback::PresentationCandidateToken token,
                                         playback::PresentationResourceManifest manifest,
                                         std::vector<playback::PortableResourcePtr> resources,
                                         playback::EffectivePresentationSettings settings) noexcept
    : token_(std::move(token)), manifest_(std::move(manifest)), resources_(std::move(resources)),
      settings_(settings) {}

auto ValidationCandidate::token() const noexcept -> const playback::PresentationCandidateToken& {
    return token_;
}

auto ValidationCandidate::manifest() const noexcept
    -> const playback::PresentationResourceManifest& {
    return manifest_;
}

auto ValidationCandidate::resources() const noexcept
    -> std::span<const playback::PortableResourcePtr> {
    return resources_;
}

auto ValidationCandidate::settings() const noexcept
    -> const playback::EffectivePresentationSettings& {
    return settings_;
}

auto validatePresentationData(const playback::PresentationResourceManifest& manifest,
                              std::span<const playback::PortableResourcePtr> resources)
    -> core::Result<void> {
    try {
        auto canonical = canonicalizePresentationData(manifest, resources);
        if (!canonical) {
            return core::unexpected(std::move(canonical.error()));
        }
        return {};
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"playback.presentation.session.budget_exceeded",
                        "Validation Sink candidate allocation could not be satisfied"}
                .withContext("limit", std::to_string(maxSessionBytes))
                .withContext("actual", "allocation_failed"));
    } catch (const std::exception& exception) {
        return core::unexpected(referenceError("Validation Sink candidate validation failed")
                                    .withContext("exception", exception.what()));
    } catch (...) {
        return core::unexpected(referenceError("Validation Sink candidate validation failed"));
    }
}

auto prepareValidationCandidate(playback::PreparedPlayback& prepared,
                                const playback::PresentationCapabilities& capabilities,
                                const playback::PresentationRequest& request)
    -> ValidationCandidateResult {
    ValidationCandidateResult result;
    try {
        auto validation = prepared.validatePresentation(capabilities, request);
        result.diagnostics = std::move(validation.diagnostics);
        if (!validation.hasValue()) {
            return result;
        }

        const auto* manifest = prepared.presentationManifest();
        if (manifest == nullptr) {
            addError(result.diagnostics,
                     core::Error{"playback.presentation.resource.missing",
                                 "Validation Sink requires a portable presentation candidate"});
            return result;
        }
        auto token = prepared.presentationCandidateToken();
        if (!token) {
            addError(result.diagnostics, token.error());
            return result;
        }

        std::vector<playback::PortableResourcePtr> resources;
        resources.reserve(manifest->entries.size());
        for (const auto& entry : manifest->entries) {
            auto resource = prepared.acquirePresentationResource(entry.reference);
            if (!resource) {
                addError(result.diagnostics, resource.error());
                return result;
            }
            resources.push_back(std::move(*resource));
        }
        auto canonical = canonicalizePresentationData(*manifest, resources);
        if (!canonical) {
            addError(result.diagnostics, canonical.error());
            return result;
        }

        result.candidate =
            ValidationCandidate{std::move(*token), std::move(canonical->manifest),
                                std::move(canonical->resources), *validation.settings};
        return result;
    } catch (const std::bad_alloc&) {
        result.candidate.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics = diagnostics();
        }
        addError(result.diagnostics,
                 core::Error{"playback.presentation.session.budget_exceeded",
                             "Validation Sink candidate allocation could not be satisfied"}
                     .withContext("limit", std::to_string(maxSessionBytes))
                     .withContext("actual", "allocation_failed"));
        return result;
    } catch (const std::exception& exception) {
        result.candidate.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics = diagnostics();
        }
        addError(result.diagnostics, referenceError("Validation Sink candidate preparation failed")
                                         .withContext("exception", exception.what()));
        return result;
    } catch (...) {
        result.candidate.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics = diagnostics();
        }
        addError(result.diagnostics,
                 referenceError("Validation Sink candidate preparation failed"));
        return result;
    }
}
} // namespace cuexis::test_support
