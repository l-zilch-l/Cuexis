// PlaybackSession host facade: load, absolute-time update, owning headless snapshots and reload.

#include <cuexis/playback/playback_session.hpp>

#include "playback_source_state.hpp"
#include "presentation_internal.hpp"

#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/assets/asset_database.hpp>
#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/chart_writer.hpp>
#include <cuexis/chart/prepared_semantic_identity.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::playback {
namespace {

std::atomic<std::uint64_t> nextPlaybackSessionToken{1};

[[nodiscard]] auto allocatePlaybackSessionToken() noexcept -> std::uint64_t {
    const auto token = nextPlaybackSessionToken.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        std::terminate();
    }
    return token;
}

void copyMatrix(const core::Mat4& source, float (&destination)[16]) noexcept {
    std::copy(source.values.begin(), source.values.end(), destination);
}

[[nodiscard]] auto runtimeFrame(const RuntimeFrame& frame) noexcept -> runtime::RuntimeFrame {
    return runtime::RuntimeFrame{.chartTimeMs = frame.chartTimeMs,
                                 .simulationDeltaTimeMs = frame.simulationDeltaTimeMs,
                                 .timeDiscontinuityId = frame.timeDiscontinuityId};
}

[[nodiscard]] auto operationError(std::string code, std::string message,
                                  const core::Diagnostics& diagnostics) -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    const auto firstError = std::find_if(
        diagnostics.items().begin(), diagnostics.items().end(), [](const core::Diagnostic& item) {
            return item.severity() == core::DiagnosticSeverity::Error;
        });
    if (firstError != diagnostics.items().end()) {
        error.withContext("diagnostic_code", std::string{firstError->code()});
        if (!firstError->fieldPath().empty()) {
            error.withContext("field_path", std::string{firstError->fieldPath()});
        }
    }
    return error;
}

void addErrorDiagnostic(core::Diagnostics& diagnostics, const core::Error& error) {
    core::Diagnostic diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                std::string{error.message()}};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

struct SnapshotEntity final {
    chart::ChartObjectId id;
    entt::entity entity{entt::null};
    std::optional<PresentationResourceRef> mesh;
    std::map<std::string, PresentationResourceRef, std::less<>> materials;
    std::size_t maximumMeshAssetIdSize{};
    std::size_t maximumMaterialAssetIdSize{};
};

struct SnapshotLayout final {
    std::vector<SnapshotEntity> entities;
    std::optional<entt::entity> activeCamera;
};

[[nodiscard]] auto ownerError(std::string_view operation) -> core::Error {
    return core::Error{"playback.session.not_owner_thread",
                       "PlaybackSession belongs to another thread"}
        .withContext("operation", std::string{operation});
}

[[nodiscard]] auto reentryError(std::string_view operation) -> core::Error {
    return core::Error{"playback.session.reentrant",
                       "PlaybackSession cannot be re-entered while an operation is active"}
        .withContext("operation", std::string{operation});
}

[[nodiscard]] auto prepareExceptionError(std::string_view operation, bool allocationFailure,
                                         const std::exception* exception = nullptr) -> core::Error {
    auto error = core::Error{allocationFailure ? "playback.prepare.budget_exceeded"
                                               : "playback.prepare.failed",
                             allocationFailure ? "Playback preparation could not allocate memory"
                                               : "Playback preparation failed"};
    error.withContext("operation", std::string{operation});
    if (exception != nullptr) {
        error.withContext("exception", exception->what());
    }
    return error;
}

[[nodiscard]] auto presentationResourceTypeName(PresentationResourceType type) noexcept
    -> std::string_view {
    switch (type) {
    case PresentationResourceType::Mesh:
        return "mesh";
    case PresentationResourceType::Texture2D:
        return "texture2d";
    case PresentationResourceType::UnlitMaterial:
        return "unlit_material";
    }
    return "unknown";
}

constexpr std::size_t maxPresentationDiagnostics = 1024;

[[nodiscard]] auto presentationDiagnostics() -> core::Diagnostics {
    return core::Diagnostics{
        maxPresentationDiagnostics,
        core::Diagnostic{core::DiagnosticSeverity::Error,
                         "playback.presentation.diagnostics.limit_exceeded",
                         "Presentation diagnostics reached the configured limit", "$"}
            .withContext("max_diagnostics", std::to_string(maxPresentationDiagnostics))};
}

void addPresentationDiagnostic(core::Diagnostics& diagnostics, core::DiagnosticSeverity severity,
                               std::string code, std::string message, std::string fieldPath,
                               std::string capability = {}, std::string actual = {},
                               std::string limit = {}) {
    auto diagnostic =
        core::Diagnostic{severity, std::move(code), std::move(message), std::move(fieldPath)};
    if (!capability.empty()) {
        diagnostic.withContext("capability", std::move(capability));
    }
    if (!actual.empty()) {
        diagnostic.withContext("actual", std::move(actual));
    }
    if (!limit.empty()) {
        diagnostic.withContext("limit", std::move(limit));
    }
    diagnostics.add(std::move(diagnostic));
}

void addMissingPresentationCapability(core::Diagnostics& diagnostics, std::string_view capability,
                                      std::string_view fieldPath) {
    addPresentationDiagnostic(diagnostics, core::DiagnosticSeverity::Error,
                              "playback.presentation.capability.required_missing",
                              "Portable presentation requires an unsupported adapter capability",
                              std::string{fieldPath}, std::string{capability});
}

void addInsufficientPresentationLimit(core::Diagnostics& diagnostics, std::string_view capability,
                                      std::uint64_t limit, std::uint64_t actual,
                                      std::string_view fieldPath) {
    addPresentationDiagnostic(diagnostics, core::DiagnosticSeverity::Error,
                              "playback.presentation.capability.limit_insufficient",
                              "Adapter presentation limit is below the candidate requirement",
                              std::string{fieldPath}, std::string{capability},
                              std::to_string(actual), std::to_string(limit));
}

[[nodiscard]] auto snapshotResourceMismatch(std::string_view objectId, std::string_view assetId,
                                            PresentationResourceType type) -> core::Error {
    return core::Error{"playback.presentation.frame.resource_mismatch",
                       "Snapshot presentation resource could not be resolved"}
        .withContext("object_id", std::string{objectId})
        .withContext("asset_id", std::string{assetId})
        .withContext("resource_type", std::string{presentationResourceTypeName(type)});
}

void assignPresentationReference(std::optional<PresentationResourceRef>& destination,
                                 const PresentationResourceRef& source,
                                 std::size_t assetIdCapacity) {
    if (!destination) {
        destination.emplace();
    }
    if (destination->assetId.capacity() < assetIdCapacity) {
        destination->assetId.reserve(assetIdCapacity);
    }
    destination->type = source.type;
    if (destination->assetId != source.assetId) {
        destination->assetId = source.assetId;
    }
    destination->identity = source.identity;
}

class SessionOperation final {
  public:
    explicit SessionOperation(bool& active) noexcept : active_(active) {
        active_ = true;
    }

    ~SessionOperation() noexcept {
        active_ = false;
    }

    SessionOperation(const SessionOperation&) = delete;
    auto operator=(const SessionOperation&) -> SessionOperation& = delete;

  private:
    bool& active_;
};

struct RequiredCapability final {
    std::string_view id;
    std::string_view fieldPath;
};

[[nodiscard]] auto allCapabilities() -> PlaybackCapabilitySet {
    return PlaybackCapabilitySet{
        .version = 1,
        .ids = {std::string{capabilityAnimationClipV1}, std::string{capabilityAnimationLayersV1},
                std::string{capabilityBehaviorEventV1}, std::string{capabilityChartV3},
                std::string{capabilityChartV4}, std::string{capabilityMaterialSnapshotV1},
                std::string{capabilityRenderVisibilityV1}, std::string{capabilitySourceCxcV1},
                std::string{capabilitySourceCxtV1}},
    };
}

void normalizeCapabilities(PlaybackCapabilitySet& capabilities) {
    std::sort(capabilities.ids.begin(), capabilities.ids.end());
    capabilities.ids.erase(std::unique(capabilities.ids.begin(), capabilities.ids.end()),
                           capabilities.ids.end());
}

[[nodiscard]] auto requiredCapabilities(const chart::ChartDocument& document)
    -> std::vector<RequiredCapability> {
    std::vector<RequiredCapability> required;
    if (document.version == 3) {
        required.push_back({capabilityChartV3, "$/version"});
    }
    for (std::size_t index = 0; index < document.behaviors.size(); ++index) {
        const auto& behavior = document.behaviors[index];
        if (behavior.type == "behavior.event") {
            required.push_back({capabilityBehaviorEventV1, "$/behaviors"});
        }
        for (const auto& event : behavior.events) {
            if (event.property == chart::BehaviorProperty::MaterialOpacity ||
                event.property == chart::BehaviorProperty::MaterialTint) {
                required.push_back({capabilityMaterialSnapshotV1, "$/behaviors"});
            }
        }
        for (const auto& event : behavior.stepEvents) {
            if (event.property == chart::BehaviorStepProperty::RenderVisible) {
                required.push_back({capabilityRenderVisibilityV1, "$/behaviors"});
            } else if (event.property == chart::BehaviorStepProperty::RenderMaterial) {
                required.push_back({capabilityMaterialSnapshotV1, "$/behaviors"});
            }
        }
    }
    if (std::any_of(document.objects.begin(), document.objects.end(),
                    [](const auto& object) { return object.components.renderable.has_value(); })) {
        required.push_back({capabilityMaterialSnapshotV1, "$/objects"});
    }
    std::sort(required.begin(), required.end(), [](const auto& left, const auto& right) {
        return std::tie(left.id, left.fieldPath) < std::tie(right.id, right.fieldPath);
    });
    required.erase(
        std::unique(required.begin(), required.end(),
                    [](const auto& left, const auto& right) { return left.id == right.id; }),
        required.end());
    return required;
}

[[nodiscard]] auto capabilityFieldPath(std::string_view capability) noexcept -> std::string_view {
    if (capability == capabilityChartV4) {
        return "$/version";
    }
    if (capability == capabilitySourceCxcV1) {
        return "$/source";
    }
    if (capability == capabilitySourceCxtV1) {
        return "$/animationTemplateImports";
    }
    if (capability == capabilityAnimationClipV1) {
        return "$/animationClips";
    }
    if (capability == capabilityAnimationLayersV1) {
        return "$/objects";
    }
    return "$";
}

[[nodiscard]] auto preflightCapabilities(const chart::ChartDocument& document,
                                         std::span<const std::string> additionalRequirements,
                                         const PlaybackCapabilitySet& supported,
                                         core::Diagnostics& diagnostics) -> bool {
    if (supported.version != 1) {
        diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "playback.capability.version_unsupported",
            "Playback capability set version is unsupported", "$/capabilities/version"}
                            .withContext("version", std::to_string(supported.version)));
        return false;
    }
    auto required = requiredCapabilities(document);
    required.reserve(required.size() + additionalRequirements.size());
    for (const auto& capability : additionalRequirements) {
        required.push_back({capability, capabilityFieldPath(capability)});
    }
    std::sort(required.begin(), required.end(), [](const auto& left, const auto& right) {
        return std::tie(left.id, left.fieldPath) < std::tie(right.id, right.fieldPath);
    });
    required.erase(
        std::unique(required.begin(), required.end(),
                    [](const auto& left, const auto& right) { return left.id == right.id; }),
        required.end());
    for (const auto& capability : required) {
        if (!std::binary_search(supported.ids.begin(), supported.ids.end(), capability.id)) {
            diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                             "playback.capability.unsupported",
                                             "Chart requires an unsupported playback capability",
                                             std::string{capability.fieldPath}}
                                .withContext("capability", std::string{capability.id}));
        }
    }
    diagnostics.sortDeterministically();
    return !diagnostics.hasErrors();
}

[[nodiscard]] auto compileAnimationProgram(std::optional<chart::AnimationProgramInput> input,
                                           const chart::ChartLimits& limits,
                                           core::Diagnostics& diagnostics)
    -> std::optional<animation::AnimationProgram> {
    if (!input.has_value()) {
        return animation::AnimationProgram{};
    }
    auto compiled = animation::AnimationCompiler::compile(std::move(*input), limits);
    diagnostics.append(std::move(compiled.diagnostics));
    if (!compiled.hasValue()) {
        diagnostics.sortDeterministically();
        return std::nullopt;
    }
    return std::move(*compiled.program);
}

[[nodiscard]] auto chartParameterInputs(const PlaybackPrepareOptions& options,
                                        core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::ChartParameterInput>> {
    std::vector<chart::ChartParameterInput> inputs;
    inputs.reserve(options.parameters.values.size());
    for (std::size_t index = 0; index < options.parameters.values.size(); ++index) {
        const auto& parameter = options.parameters.values[index];
        chart::ChartParameterInput input;
        input.id = parameter.id;
        if (const auto* number = std::get_if<ChartParameterNumber>(&parameter.value)) {
            input.type = chart::ChartParameterType::Number;
            input.value = number->value;
        } else if (const auto* rational = std::get_if<ChartParameterRational>(&parameter.value)) {
            auto value = chart::RationalBeat::create(rational->numerator, rational->denominator);
            if (!value) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "chart.parameter.out_of_range",
                    "Rational parameter input is outside the supported range",
                    "$/parameterInputs/" + std::to_string(index) + "/value"});
                continue;
            }
            input.type = chart::ChartParameterType::Rational;
            input.value = *value;
        } else {
            const auto& weight = std::get<ChartParameterWeight>(parameter.value);
            input.type = chart::ChartParameterType::Weight;
            input.value = weight.value;
        }
        inputs.push_back(std::move(input));
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return std::nullopt;
    }
    return inputs;
}

[[nodiscard]] auto projectDocuments(std::span<const PlaybackProjectDocument> sourceDocuments)
    -> std::vector<chart::ProjectDocument> {
    std::vector<chart::ProjectDocument> documents;
    documents.reserve(sourceDocuments.size());
    for (const auto& document : sourceDocuments) {
        documents.push_back({document.path, document.utf8Text});
    }
    return documents;
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
            .entity = **entity,
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
                    const auto* material = detail::findPresentationResource(
                        *presentation, assetId, PresentationResourceType::UnlitMaterial);
                    if (material == nullptr) {
                        return core::unexpected(snapshotResourceMismatch(
                            object.id.value, assetId, PresentationResourceType::UnlitMaterial));
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
                                const auto* resource = detail::findPresentationResource(
                                    *presentation, material->value,
                                    PresentationResourceType::UnlitMaterial);
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

    auto camera = session.withWorld(
        [&](const world::World& world) -> core::Result<std::optional<entt::entity>> {
            return world.withRegistry([&](const entt::registry& registry)
                                          -> core::Result<std::optional<entt::entity>> {
                for (std::size_t index = 0; index < chartRuntime.objects.size(); ++index) {
                    if (chartRuntime.objects[index].components.camera.has_value() &&
                        registry.all_of<render::CameraComponent>(layout.entities[index].entity)) {
                        return layout.entities[index].entity;
                    }
                }

                std::optional<entt::entity> selected;
                const auto view = registry.view<const render::CameraComponent>();
                for (const entt::entity entity : view) {
                    if (!selected.has_value() ||
                        entt::to_integral(entity) < entt::to_integral(*selected)) {
                        selected = entity;
                    }
                }
                return selected;
            });
        });
    if (!camera) {
        return core::unexpected(std::move(camera.error()));
    }
    layout.activeCamera = *camera;
    return layout;
}

} // namespace

struct PlaybackSession::State final {
    State() = default;

    core::ThreadChecker ownerThread;
    // ResourceManager must outlive RuntimeSession and its ResourceScope.
    std::shared_ptr<content::IContentProvider> contentProvider;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::string activeChartJson;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    SnapshotLayout snapshotLayout;
    std::optional<RuntimeFrame> lastFrame;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<ChartInfo> activeChartInfo;
    std::optional<PlaybackContentInfo> activeContentInfo;
    std::optional<PlaybackMode> activeMode;
    std::optional<PreparedSemanticIdentity> semanticIdentity;
    ChartParameterSet parameters;
    std::uint64_t sessionToken{allocatePlaybackSessionToken()};
    std::uint64_t generation{1};
    SessionState sessionState{SessionState::Empty};
    bool operationActive{};
    PlaybackCapabilitySet capabilities;
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t nextCandidateGeneration{1};
};

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
    std::optional<assets::AudioSourceLease> audioSourceLease;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<RuntimeFrame> targetFrame;
    SessionState committedState{SessionState::Ready};
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t candidateGeneration{};
};

static_assert(std::is_nothrow_move_assignable_v<core::Diagnostics>);
static_assert(std::is_nothrow_move_assignable_v<PlaybackContentInfo>);
static_assert(std::is_nothrow_move_assignable_v<detail::PreparedPresentation>);

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

        if (capabilities.version != 1) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.version_unsupported",
                                      "Presentation capability set version is unsupported",
                                      "$/capabilities/version", "capabilities.version",
                                      std::to_string(capabilities.version), "1");
        }
        if (request.version != 1) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.version_unsupported",
                                      "Presentation request version is unsupported",
                                      "$/request/version", "request.version",
                                      std::to_string(request.version), "1");
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
            if (!capabilities.opaquePass) {
                addMissingPresentationCapability(result.diagnostics, "opaque_pass",
                                                 "$/capabilities/opaquePass");
            }
            if (!capabilities.transparentPass) {
                addMissingPresentationCapability(result.diagnostics, "transparent_pass",
                                                 "$/capabilities/transparentPass");
            }
            if (!capabilities.linearTexture) {
                addMissingPresentationCapability(result.diagnostics, "linear_texture",
                                                 "$/capabilities/linearTexture");
            }
            if (!capabilities.srgbTexture) {
                addMissingPresentationCapability(result.diagnostics, "srgb_texture",
                                                 "$/capabilities/srgbTexture");
            }
            if (!capabilities.straightAlphaBlend) {
                addMissingPresentationCapability(result.diagnostics, "straight_alpha_blend",
                                                 "$/capabilities/straightAlphaBlend");
            }
            if (!capabilities.backFaceCulling) {
                addMissingPresentationCapability(result.diagnostics, "back_face_culling",
                                                 "$/capabilities/backFaceCulling");
            }
            if (!capabilities.doubleSided) {
                addMissingPresentationCapability(result.diagnostics, "double_sided",
                                                 "$/capabilities/doubleSided");
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

            if (capabilities.maxResourceBytes < maxResourceBytes) {
                addInsufficientPresentationLimit(result.diagnostics, "max_resource_bytes",
                                                 capabilities.maxResourceBytes, maxResourceBytes,
                                                 "$/capabilities/maxResourceBytes");
            }
            if (capabilities.maxTotalDecodedBytes <
                state_->presentation->manifest.totalDecodedBytes) {
                addInsufficientPresentationLimit(result.diagnostics, "max_total_decoded_bytes",
                                                 capabilities.maxTotalDecodedBytes,
                                                 state_->presentation->manifest.totalDecodedBytes,
                                                 "$/capabilities/maxTotalDecodedBytes");
            }
            if (capabilities.maxTextureDimension < maxTextureDimension) {
                addInsufficientPresentationLimit(
                    result.diagnostics, "max_texture_dimension", capabilities.maxTextureDimension,
                    maxTextureDimension, "$/capabilities/maxTextureDimension");
            }
            if (capabilities.maxMeshVertices < maxMeshVertices) {
                addInsufficientPresentationLimit(result.diagnostics, "max_mesh_vertices",
                                                 capabilities.maxMeshVertices, maxMeshVertices,
                                                 "$/capabilities/maxMeshVertices");
            }
            if (capabilities.maxMeshIndices < maxMeshIndices) {
                addInsufficientPresentationLimit(result.diagnostics, "max_mesh_indices",
                                                 capabilities.maxMeshIndices, maxMeshIndices,
                                                 "$/capabilities/maxMeshIndices");
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

        result.diagnostics.sortDeterministically();
        if (!result.diagnostics.hasErrors()) {
            result.settings = EffectivePresentationSettings{
                .version = 1, .portableProfileVersion = 1, .debugPassEnabled = debugPassEnabled};
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
    return state_->capabilities;
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
        return core::unexpected(reentryError("prepare_load"));
    }
    try {
        auto source = PlaybackSource::fromChartText(std::string{jsonText});
        if (!source) {
            return core::unexpected(std::move(source.error()));
        }
        return prepare(std::move(*source), mode, nullptr, ReloadPolicy::KeepChartTime, false,
                       options);
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("prepare_load", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("prepare_load", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("prepare_load", false));
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
        return core::unexpected(reentryError("prepare_load"));
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
        return core::unexpected(reentryError("prepare_reload"));
    }
    if (!state_->activeMode) {
        return core::unexpected(
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"});
    }
    try {
        auto source = PlaybackSource::fromChartText(std::string{replacementJson});
        if (!source) {
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
        return core::unexpected(prepareExceptionError("prepare_reload", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("prepare_reload", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("prepare_reload", false));
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
        return core::unexpected(reentryError("prepare_reload"));
    }
    if (!state_->activeMode) {
        return core::unexpected(
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"});
    }
    return prepare(std::move(replacement), *state_->activeMode, &targetFrame, policy, true,
                   options);
}

auto PlaybackSession::prepare(PlaybackSource&& source, PlaybackMode mode,
                              const RuntimeFrame* targetFrame, ReloadPolicy policy,
                              bool replacement, const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    try {
        if (!state_->ownerThread.isCurrent()) {
            return core::unexpected(ownerError(replacement ? "prepare_reload" : "prepare_load"));
        }
        if (state_->operationActive) {
            return core::unexpected(reentryError(replacement ? "prepare_reload" : "prepare_load"));
        }
        SessionOperation operation{state_->operationActive};
        if ((!replacement && state_->sessionState != SessionState::Empty) ||
            (replacement && state_->sessionState != SessionState::Ready &&
             state_->sessionState != SessionState::Running)) {
            return core::unexpected(core::Error{
                replacement ? "playback.session.not_ready" : "playback.session.not_empty",
                replacement ? "PlaybackSession must be active before preparing a reload"
                            : "PlaybackSession must be Empty before preparing a load"});
        }

        if (!source.state_) {
            return core::unexpected(
                core::Error{"playback.source.invalid", "PlaybackSource is empty"});
        }
        core::Diagnostics diagnostics;
        auto& sourceState = *source.state_;
        const auto* entryChart = sourceState.entryChart();
        if (entryChart == nullptr) {
            return core::unexpected(core::Error{"playback.source.invalid",
                                                "PlaybackSource entry Chart is unavailable"});
        }
        const auto& jsonText = entryChart->utf8Text;
        const chart::ChartLimits limits;
        std::optional<chart::ChartDocument> document;
        std::optional<chart::AnimationProgramInput> animationProgram;
        std::optional<chart::ChartV4ResolvedArtifact> v4Artifact;
        std::optional<chart::CanonicalContentIdentity> chartIdentity;
        std::optional<chart::CanonicalContentIdentity> parameterIdentity;
        std::vector<chart::CxtIdentityComponent> cxtIdentities;
        std::vector<chart::ChartResourceRequirement> resourceRequirements;
        std::vector<std::string> additionalCapabilities;
        if (sourceState.cxcPackageIdentity) {
            additionalCapabilities.emplace_back(capabilitySourceCxcV1);
        }

        if (chart::ChartV4Loader::isV4(jsonText, limits)) {
            auto loaded = chart::ChartV4Loader::load(jsonText, limits);
            const bool loadedValid = loaded.hasValue();
            diagnostics.append(std::move(loaded.diagnostics));
            if (!loadedValid) {
                state_->lastOperationDiagnostics = diagnostics;
                return core::unexpected(
                    operationError(replacement ? "playback.chart.reload_load_failed"
                                               : "playback.chart.load_failed",
                                   "Chart v4 loading produced errors", diagnostics));
            }

            auto inputs = chartParameterInputs(options, diagnostics);
            if (!inputs) {
                state_->lastOperationDiagnostics = diagnostics;
                return core::unexpected(
                    operationError(replacement ? "playback.chart.reload_load_failed"
                                               : "playback.chart.load_failed",
                                   "Chart parameter conversion produced errors", diagnostics));
            }
            auto documents = projectDocuments(sourceState.projectDocuments);
            auto resolved =
                chart::ChartV4Resolver::resolve(*loaded.document, *inputs, documents, {}, limits);
            const bool resolvedValid = resolved.hasValue();
            diagnostics.append(std::move(resolved.diagnostics));
            if (!resolvedValid) {
                state_->lastOperationDiagnostics = diagnostics;
                return core::unexpected(
                    operationError(replacement ? "playback.chart.reload_load_failed"
                                               : "playback.chart.load_failed",
                                   "Chart v4 resolution produced errors", diagnostics));
            }
            auto artifact = std::move(*resolved.artifact);
            additionalCapabilities.insert(additionalCapabilities.end(),
                                          artifact.capabilityRequirements.begin(),
                                          artifact.capabilityRequirements.end());
            document.emplace(artifact.document.chart);
            animationProgram = std::move(artifact.animationProgram);
            chartIdentity = artifact.chartIdentity;
            parameterIdentity = artifact.parameterIdentity;
            cxtIdentities = artifact.cxtIdentities;
            resourceRequirements = artifact.resourceRequirements;
            v4Artifact = std::move(artifact);
        } else {
            auto loaded = chart::ChartLoader::load(jsonText, limits);
            const bool loadedValid = loaded.hasValue();
            diagnostics.append(std::move(loaded.diagnostics));
            if (!loadedValid) {
                state_->lastOperationDiagnostics = diagnostics;
                return core::unexpected(
                    operationError(replacement ? "playback.chart.reload_load_failed"
                                               : "playback.chart.load_failed",
                                   "Chart loading produced errors", diagnostics));
            }
            for (std::size_t index = 0; index < options.parameters.values.size(); ++index) {
                diagnostics.add(
                    core::Diagnostic{core::DiagnosticSeverity::Error, "chart.parameter.unknown",
                                     "Host parameter input is not supported by this Chart version",
                                     "$/parameterInputs/" + std::to_string(index) + "/id"});
            }
            if (diagnostics.hasErrors()) {
                diagnostics.sortDeterministically();
                state_->lastOperationDiagnostics = diagnostics;
                return core::unexpected(
                    operationError(replacement ? "playback.chart.reload_load_failed"
                                               : "playback.chart.load_failed",
                                   "Chart parameter resolution produced errors", diagnostics));
            }
            document.emplace(std::move(*loaded.document));
            auto canonical = chart::ChartWriter::write(*document);
            if (!canonical) {
                return core::unexpected(std::move(canonical.error()));
            }
            chartIdentity = chart::canonicalBytesIdentity(*canonical);
            parameterIdentity = chart::emptyParameterIdentity();
        }

        if (!preflightCapabilities(*document, additionalCapabilities, state_->capabilities,
                                   diagnostics)) {
            state_->lastOperationDiagnostics = diagnostics;
            return core::unexpected(operationError("playback.capability.preflight_failed",
                                                   "Playback capability preflight failed",
                                                   diagnostics));
        }

        auto compiledAnimation =
            compileAnimationProgram(std::move(animationProgram), limits, diagnostics);
        if (!compiledAnimation) {
            state_->lastOperationDiagnostics = diagnostics;
            return core::unexpected(
                operationError(replacement ? "playback.animation.reload_compile_failed"
                                           : "playback.animation.compile_failed",
                               "Animation program compilation produced errors", diagnostics));
        }

        auto runtimeResult = chart::ChartCompiler::compile(*document, limits);
        const bool runtimeValid = runtimeResult.hasValue();
        diagnostics.append(std::move(runtimeResult.diagnostics));
        if (!runtimeValid) {
            state_->lastOperationDiagnostics = diagnostics;
            return core::unexpected(
                operationError(replacement ? "playback.chart.reload_compile_failed"
                                           : "playback.chart.compile_failed",
                               "Chart compilation produced errors", diagnostics));
        }
        auto& chartRuntime = *runtimeResult.runtime;
        const bool hasMainMusic = chartRuntime.mainMusic.has_value();
        if ((mode == PlaybackMode::ChartClock && hasMainMusic) ||
            (mode != PlaybackMode::ChartClock && !hasMainMusic)) {
            return core::unexpected(
                core::Error{"playback.mode.content_mismatch",
                            mode == PlaybackMode::ChartClock
                                ? "ChartClock requires a chart without main music"
                                : "HostClock and CuexisAudio require a chart with main music"});
        }

        std::unique_ptr<assets::ResourceManager> resourceManager;
        if (sourceState.database) {
            resourceManager = std::make_unique<assets::ResourceManager>(
                std::move(*sourceState.database), sourceState.provider);
        }

        std::optional<assets::AudioSourceLease> audioSourceLease;
        if (chartRuntime.mainMusic) {
            if (!resourceManager) {
                return core::unexpected(core::Error{
                    "playback.content.asset_database_missing",
                    "A chart with main music requires an AssetDatabase and ContentProvider"});
            }
            auto sourceResult = resourceManager->requestAudioSource(
                assets::AssetId{chartRuntime.mainMusic->value}, assets::ResourcePolicy::Required);
            const bool sourceValid = sourceResult.hasValue();
            diagnostics.append(std::move(sourceResult.diagnostics));
            if (!sourceValid) {
                state_->lastOperationDiagnostics = diagnostics;
                return core::unexpected(operationError(
                    "playback.content.main_music_failed",
                    "Required main music source could not be prepared", diagnostics));
            }
            audioSourceLease.emplace(std::move(*sourceResult.lease));
        }

        auto session = resourceManager ? std::make_unique<runtime::RuntimeSession>(*resourceManager)
                                       : std::make_unique<runtime::RuntimeSession>();
        auto runtimePrepared = session->prepare(chartRuntime, std::move(*compiledAnimation));
        const bool preparedValid = runtimePrepared.hasValue();
        diagnostics.append(std::move(runtimePrepared.diagnostics));
        if (!preparedValid) {
            state_->lastOperationDiagnostics = diagnostics;
            return core::unexpected(operationError("playback.session.prepare_failed",
                                                   "RuntimeSession preparation produced errors",
                                                   diagnostics));
        }
        auto presentation = detail::preparePresentation(chartRuntime, resourceManager.get());
        if (!presentation) {
            addErrorDiagnostic(diagnostics, presentation.error());
            diagnostics.sortDeterministically();
            state_->lastOperationDiagnostics = diagnostics;
            return core::unexpected(std::move(presentation.error()));
        }
        if (auto committed = session->commit(std::move(*runtimePrepared.prepared)); !committed) {
            return core::unexpected(std::move(committed.error()));
        }

        std::optional<RuntimeFrame> committedFrame;
        if (replacement && targetFrame != nullptr) {
            committedFrame = *targetFrame;
            committedFrame->simulationDeltaTimeMs = 0.0;
            if (policy == ReloadPolicy::RestartAtZero) {
                committedFrame->chartTimeMs = 0.0;
            }
            if (auto updated = session->update(runtimeFrame(*committedFrame)); !updated) {
                addErrorDiagnostic(diagnostics, updated.error());
                diagnostics.sortDeterministically();
                auto error = operationError("playback.session.reload_sample_failed",
                                            "Reload target frame sampling failed", diagnostics);
                state_->lastOperationDiagnostics = std::move(diagnostics);
                return core::unexpected(std::move(error));
            }
        }

        const auto* preparedPresentation =
            presentation->has_value() ? &presentation->value() : nullptr;
        auto snapshotLayout = buildSnapshotLayout(*session, chartRuntime, preparedPresentation);
        if (!snapshotLayout) {
            return core::unexpected(std::move(snapshotLayout.error()));
        }

        if (!v4Artifact) {
            if (audioSourceLease && audioSourceLease->valid()) {
                resourceRequirements.push_back(chart::ChartResourceRequirement{
                    .assetId = chart::AssetId{std::string{audioSourceLease->resource().id.value}},
                    .uses = {chart::ChartResourceUse::MainMusic}});
            }
            if (presentation->has_value()) {
                for (const auto& entry : presentation->value().manifest.entries) {
                    const auto existing = std::ranges::find_if(
                        resourceRequirements, [&](const chart::ChartResourceRequirement& item) {
                            return item.assetId.value == entry.reference.assetId;
                        });
                    auto use = entry.reference.type == PresentationResourceType::Mesh
                                   ? chart::ChartResourceUse::RenderableMesh
                                   : chart::ChartResourceUse::RenderableMaterial;
                    if (existing == resourceRequirements.end()) {
                        resourceRequirements.push_back(chart::ChartResourceRequirement{
                            .assetId = chart::AssetId{entry.reference.assetId}, .uses = {use}});
                    } else if (std::ranges::find(existing->uses, use) == existing->uses.end()) {
                        existing->uses.push_back(use);
                    }
                }
            }
        }
        auto resourceIdentities = assembleResourceIdentities(resourceRequirements, audioSourceLease,
                                                             *presentation, diagnostics);
        if (!resourceIdentities) {
            state_->lastOperationDiagnostics = diagnostics;
            return core::unexpected(
                operationError("playback.identity.assemble_failed",
                               "Prepared semantic identity could not be assembled", diagnostics));
        }
        const auto assembledIdentity = chart::assemblePreparedSemanticIdentity(
            *chartIdentity, cxtIdentities, *resourceIdentities, *parameterIdentity);

        diagnostics.sortDeterministically();
        auto prepared = std::make_unique<PreparedPlayback::State>();
        prepared->owner = this;
        prepared->ownerToken = state_->sessionToken;
        prepared->expectedGeneration = state_->generation;
        prepared->replacement = replacement;
        prepared->contentProvider = std::move(sourceState.provider);
        prepared->resourceManager = std::move(resourceManager);
        prepared->chartJson = jsonText;
        prepared->runtimeSession = std::move(session);
        prepared->snapshotLayout = std::move(*snapshotLayout);
        prepared->chartInfo = chartInfoFor(chartRuntime, prepared->runtimeSession->resourceCount());
        prepared->contentInfo = PlaybackContentInfo{
            chartRuntime.chartId.value, chartRuntime.version, chartRuntime.timingMap.offsetMs(),
            mode,
            chartRuntime.mainMusic ? std::optional<std::string>{chartRuntime.mainMusic->value}
                                   : std::nullopt};
        prepared->parameters = options.parameters;
        prepared->semanticIdentity = toPublicIdentity(assembledIdentity);
        prepared->audioSourceLease = std::move(audioSourceLease);
        prepared->targetFrame = committedFrame;
        prepared->committedState = replacement ? state_->sessionState : SessionState::Ready;
        prepared->presentation = std::move(*presentation);
        prepared->candidateGeneration = state_->nextCandidateGeneration++;
        if (prepared->candidateGeneration == 0) {
            prepared->candidateGeneration = state_->nextCandidateGeneration++;
        }
        prepared->diagnostics = std::move(diagnostics);
        prepared->lastOperationDiagnostics = prepared->diagnostics;
        return PreparedPlayback{std::move(prepared)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError(
            replacement ? "prepare_reload" : "prepare_load", false, &exception));
    } catch (...) {
        return core::unexpected(
            prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", false));
    }
}

auto PlaybackSession::commit(PreparedPlayback&& prepared) -> core::Result<void> {
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
        return core::unexpected(core::Error{"playback.prepared.wrong_session",
                                            "PreparedPlayback belongs to another PlaybackSession"});
    }
    if (candidate.expectedGeneration != state_->generation) {
        return core::unexpected(
            core::Error{"playback.prepared.stale", "PlaybackSession changed after preparation"});
    }
    if ((!candidate.replacement && state_->sessionState != SessionState::Empty) ||
        (candidate.replacement && state_->sessionState != SessionState::Ready &&
         state_->sessionState != SessionState::Running)) {
        return core::unexpected(core::Error{"playback.prepared.lifecycle_changed",
                                            "PlaybackSession lifecycle changed after preparation"});
    }

    state_->contentProvider = std::move(candidate.contentProvider);
    state_->resourceManager = std::move(candidate.resourceManager);
    state_->activeChartJson = std::move(candidate.chartJson);
    state_->runtimeSession = std::move(candidate.runtimeSession);
    state_->snapshotLayout = std::move(candidate.snapshotLayout);
    state_->activeChartInfo = candidate.chartInfo;
    state_->activeContentInfo = std::move(candidate.contentInfo);
    state_->activeMode = state_->activeContentInfo->mode;
    state_->semanticIdentity = candidate.semanticIdentity;
    state_->parameters = std::move(candidate.parameters);
    state_->lastFrame = candidate.targetFrame;
    state_->diagnostics = std::move(candidate.diagnostics);
    state_->lastOperationDiagnostics = std::move(candidate.lastOperationDiagnostics);
    state_->sessionState = candidate.committedState;
    state_->presentation = std::move(candidate.presentation);
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
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
    auto updated = state_->runtimeSession->update(runtimeFrame(frame));
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
    }
    state_->lastFrame = frame;
    state_->sessionState = SessionState::Running;
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
    return {};
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport) const
    -> core::Result<FrameSnapshot> {
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
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport, FrameSnapshot& snapshot) const
    -> core::Result<void> {
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

    auto extracted = state_->runtimeSession->withWorld([&](const world::World& world)
                                                           -> core::Result<void> {
        return world.withRegistry([&](const entt::registry& registry) -> core::Result<void> {
            snapshot.camera = {};
            snapshot.clearRed = 0.055F;
            snapshot.clearGreen = 0.063F;
            snapshot.clearBlue = 0.071F;
            snapshot.clearAlpha = 1.0F;
            snapshot.viewportWidth = viewport.width;
            snapshot.viewportHeight = viewport.height;
            if (snapshot.objects.size() < state_->snapshotLayout.entities.size()) {
                snapshot.objects.resize(state_->snapshotLayout.entities.size());
            }

            const core::Mat4 identity;
            for (std::size_t index = 0; index < state_->snapshotLayout.entities.size(); ++index) {
                const auto& entry = state_->snapshotLayout.entities[index];
                auto& object = snapshot.objects[index];
                if (object.materialAssetId.capacity() < entry.maximumMaterialAssetIdSize) {
                    object.materialAssetId.reserve(entry.maximumMaterialAssetIdSize);
                }
                if (entry.mesh) {
                    assignPresentationReference(object.mesh, *entry.mesh,
                                                entry.maximumMeshAssetIdSize);
                } else {
                    object.mesh.reset();
                }
                if (object.id != entry.id.value) {
                    object.id = entry.id.value;
                }
                object.hasTransform = registry.all_of<world::WorldTransformComponent>(entry.entity);
                if (object.hasTransform) {
                    copyMatrix(registry.get<world::WorldTransformComponent>(entry.entity).matrix,
                               object.worldMatrix);
                } else {
                    copyMatrix(identity, object.worldMatrix);
                }
                if (registry.all_of<render::AppearanceComponent>(entry.entity)) {
                    const auto& appearance =
                        registry.get<render::AppearanceComponent>(entry.entity);
                    object.visible = appearance.visible;
                    object.materialAssetId.assign(appearance.materialAssetId);
                    if (entry.mesh) {
                        const auto material = entry.materials.find(appearance.materialAssetId);
                        if (material == entry.materials.end()) {
                            return core::unexpected(
                                snapshotResourceMismatch(entry.id.value, appearance.materialAssetId,
                                                         PresentationResourceType::UnlitMaterial));
                        }
                        assignPresentationReference(object.material, material->second,
                                                    entry.maximumMaterialAssetIdSize);
                    } else {
                        object.material.reset();
                    }
                    object.materialOpacity = appearance.opacity;
                    object.materialTint[0] = appearance.tint.x;
                    object.materialTint[1] = appearance.tint.y;
                    object.materialTint[2] = appearance.tint.z;
                } else {
                    if (entry.mesh) {
                        return core::unexpected(snapshotResourceMismatch(
                            entry.id.value, "", PresentationResourceType::UnlitMaterial));
                    }
                    object.visible = true;
                    object.materialAssetId.clear();
                    object.material.reset();
                    object.materialOpacity = 1.0;
                    object.materialTint[0] = 1.0F;
                    object.materialTint[1] = 1.0F;
                    object.materialTint[2] = 1.0F;
                }
            }
            snapshot.objects.resize(state_->snapshotLayout.entities.size());

            if (!state_->snapshotLayout.activeCamera.has_value()) {
                return {};
            }
            const auto activeCamera = *state_->snapshotLayout.activeCamera;
            if (!registry.all_of<render::CameraComponent>(activeCamera)) {
                return core::unexpected(core::Error{"playback.snapshot.camera_missing",
                                                    "Active camera is unavailable"});
            }
            const auto& camera = registry.get<render::CameraComponent>(activeCamera);
            const auto projection =
                core::makePerspective(camera.fovY * 3.14159265358979323846 / 180.0,
                                      static_cast<double>(viewport.width) / viewport.height,
                                      camera.nearPlane, camera.farPlane);
            if (!projection) {
                return core::unexpected(core::Error{"playback.snapshot.camera_invalid",
                                                    "Active camera parameters are invalid"}
                                            .withCause(projection.error()));
            }

            snapshot.camera.active = true;
            snapshot.camera.fovY = camera.fovY;
            snapshot.camera.nearPlane = camera.nearPlane;
            snapshot.camera.farPlane = camera.farPlane;
            snapshot.camera.pitch = camera.pitch;
            snapshot.camera.yaw = camera.yaw;
            snapshot.camera.roll = camera.roll;
            copyMatrix(*projection, snapshot.camera.projectionMatrix);

            core::Mat4 viewMatrix{};
            if (registry.all_of<world::WorldTransformComponent>(activeCamera)) {
                auto inverse = core::inverse(
                    registry.get<world::WorldTransformComponent>(activeCamera).matrix);
                if (!inverse) {
                    return core::unexpected(core::Error{"playback.snapshot.camera_not_invertible",
                                                        "Active camera transform is not invertible"}
                                                .withCause(inverse.error()));
                }
                viewMatrix = *inverse;
            }
            copyMatrix(viewMatrix, snapshot.camera.viewMatrix);
            return {};
        });
    });
    if (!extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    return {};
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
    state_->parameters = {};
    state_->presentation.reset();
    state_->sessionState = SessionState::Empty;
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
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
    return *state_->activeContentInfo;
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
    return state_->diagnostics;
}

auto PlaybackSession::lastOperationDiagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("last_operation_diagnostics"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("last_operation_diagnostics"));
    }
    SessionOperation operation{state_->operationActive};
    return state_->lastOperationDiagnostics;
}

} // namespace cuexis::playback
