#include <cuexis/chart/candidate_lowering.hpp>

#include "diagnostic_limit.hpp"
#include "packed_identity_internal.hpp"
#include "packed_profile_internal.hpp"
#include "sha256_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart {
namespace {

using packed::identity_detail::ByteKeyLess;
using packed::identity_detail::canonicalIdentityBytes;

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    static_cast<void>(diagnostics.add(core::Diagnostic{
        core::DiagnosticSeverity::Error, std::move(code), std::move(message), std::move(path)}));
}

void addError(core::Diagnostics& diagnostics, const core::Error& error, std::string path) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(path)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

struct OrderedEntity final {
    std::vector<std::byte> identityBytes;
    const CanonicalEntity* entity{};
};

[[nodiscard]] auto executionId(const CanonicalEntityIdentity& identity,
                               std::span<const std::byte> identityBytes) -> std::string {
    if (const auto* explicitIdentity = std::get_if<ExplicitEntityIdentity>(&identity)) {
        return explicitIdentity->objectId.value;
    }

    constexpr std::string_view domain{"cuexis.entity.execution-id.v5.candidate.1"};
    std::vector<std::byte> preimage;
    preimage.reserve(domain.size() + 1U + identityBytes.size());
    const auto domainBytes = std::as_bytes(std::span<const char>{domain.data(), domain.size()});
    preimage.insert(preimage.end(), domainBytes.begin(), domainBytes.end());
    preimage.push_back(std::byte{0});
    preimage.insert(preimage.end(), identityBytes.begin(), identityBytes.end());
    const auto digest =
        detail::sha256(std::span<const std::byte>{preimage.data(), preimage.size()});
    return "v5g1:" + detail::sha256Hex(digest);
}

[[nodiscard]] auto toRuntimeComponents(const CanonicalEntity& entity,
                                       std::optional<double>& renderableOpacity,
                                       core::Diagnostics& diagnostics, std::size_t index)
    -> ObjectComponents {
    ObjectComponents components;
    for (const auto& component : entity.components) {
        if (const auto* transform = std::get_if<CanonicalTransform>(&component)) {
            if (components.transform) {
                addError(diagnostics, "candidate.lowering.component_duplicate",
                         "Candidate entity contains duplicate transform components",
                         "$/entities[" + std::to_string(index) + "]/components");
                continue;
            }
            components.transform =
                TransformData{transform->position, transform->rotation, transform->scale};
        } else if (const auto* renderable = std::get_if<CanonicalRenderable>(&component)) {
            if (components.renderable) {
                addError(diagnostics, "candidate.lowering.component_duplicate",
                         "Candidate entity contains duplicate renderable components",
                         "$/entities[" + std::to_string(index) + "]/components");
                continue;
            }
            components.renderable = RenderableData{renderable->mesh, renderable->material};
            renderableOpacity = static_cast<double>(renderable->alpha) / 255.0;
        } else if (const auto* camera = std::get_if<CanonicalCamera>(&component)) {
            if (components.camera) {
                addError(diagnostics, "candidate.lowering.component_duplicate",
                         "Candidate entity contains duplicate camera components",
                         "$/entities[" + std::to_string(index) + "]/components");
                continue;
            }
            components.camera = CameraComponentData{camera->type, camera->fovY, camera->nearPlane,
                                                    camera->farPlane};
        }
    }
    return components;
}

} // namespace

auto lowerCandidateRuntime(const CanonicalSemanticChart& chart, const ChartLimits& limits)
    -> CandidateRuntimeArtifactResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return CandidateRuntimeArtifactResult{std::nullopt, std::move(diagnostics)};
    }

    if (chart.entities.empty()) {
        addError(diagnostics, "candidate.profile.entities",
                 "A candidate chart must contain at least one entity", "$/entities");
    }
    if (chart.entities.size() > limits.maxObjects || chart.entities.size() > 40000U) {
        addError(diagnostics, "candidate.limit.entities",
                 "Candidate entity count exceeds the Foundation limit", "$/entities");
    }
    std::size_t requirementCount = 0U;
    for (const auto& entity : chart.entities) {
        if (entity.requirements.size() > 40000U ||
            requirementCount > 40000U - entity.requirements.size()) {
            addError(diagnostics, "candidate.limit.requirements",
                     "Candidate requirement count exceeds the Foundation limit", "$/entities");
            break;
        }
        requirementCount += entity.requirements.size();
    }

    if (auto profile = packed::profile_detail::validateFoundationProfile(chart); !profile) {
        addError(diagnostics, profile.error(), "$/entities");
    }
    auto semanticIdentity = packed::semanticIdentity(chart);
    if (!semanticIdentity) {
        addError(diagnostics, semanticIdentity.error(), "$/");
    }

    auto timingMap = TimingMap::create(chart.timing.defaultBpm, chart.timing.offsetMs,
                                       chart.timing.tempoEvents, chart.timing.stops);
    if (!timingMap) {
        addError(diagnostics, timingMap.error(), "$/timing");
    }
    if (diagnostics.hasErrors() || !semanticIdentity || !timingMap) {
        diagnostics.sortDeterministically();
        return CandidateRuntimeArtifactResult{std::nullopt, std::move(diagnostics)};
    }

    std::vector<OrderedEntity> ordered;
    ordered.reserve(chart.entities.size());
    std::map<std::vector<std::byte>, std::size_t, ByteKeyLess> indicesByIdentity;
    for (const auto& entity : chart.entities) {
        auto bytes = canonicalIdentityBytes(entity.identity);
        if (!bytes) {
            addError(diagnostics, bytes.error(), "$/entities");
            continue;
        }
        const auto [unused, inserted] = indicesByIdentity.emplace(*bytes, ordered.size());
        static_cast<void>(unused);
        if (!inserted) {
            addError(diagnostics, "candidate.lowering.identity_duplicate",
                     "Candidate entity identities must be unique", "$/entities");
            continue;
        }
        ordered.push_back(OrderedEntity{std::move(*bytes), &entity});
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return CandidateRuntimeArtifactResult{std::nullopt, std::move(diagnostics)};
    }

    std::sort(ordered.begin(), ordered.end(),
              [](const OrderedEntity& left, const OrderedEntity& right) {
                  return ByteKeyLess{}(left.identityBytes, right.identityBytes);
              });
    indicesByIdentity.clear();
    std::vector<std::string> executionIds;
    executionIds.reserve(ordered.size());
    std::map<std::string, std::size_t, std::less<>> executionOrder;
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        indicesByIdentity.emplace(ordered[index].identityBytes, index);
        auto id = executionId(ordered[index].entity->identity,
                              std::span<const std::byte>{ordered[index].identityBytes.data(),
                                                         ordered[index].identityBytes.size()});
        if (!executionOrder.emplace(id, index).second) {
            addError(diagnostics, "candidate.lowering.execution_id_collision",
                     "Distinct candidate identities produced the same execution ID", "$/entities");
        }
        executionIds.push_back(std::move(id));
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return CandidateRuntimeArtifactResult{std::nullopt, std::move(diagnostics)};
    }

    std::vector<RuntimeObject> runtimeObjects;
    runtimeObjects.reserve(ordered.size());
    std::vector<CandidateRuntimeObjectMetadata> metadataObjects;
    metadataObjects.reserve(ordered.size());
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        const auto& orderedEntity = ordered[index];
        const auto& entity = *orderedEntity.entity;
        std::optional<std::size_t> parentIndex;
        if (entity.parent) {
            auto parentBytes = canonicalIdentityBytes(*entity.parent);
            if (!parentBytes) {
                addError(diagnostics, parentBytes.error(), "$/entities/parent");
                continue;
            }
            const auto parent = indicesByIdentity.find(*parentBytes);
            if (parent == indicesByIdentity.end()) {
                addError(diagnostics, "candidate.lowering.parent_missing",
                         "Candidate entity parent is not part of the chart", "$/entities/parent");
                continue;
            }
            parentIndex = parent->second;
        }

        std::optional<double> renderableOpacity;
        auto components = toRuntimeComponents(entity, renderableOpacity, diagnostics, index);
        RuntimeObject runtimeObject{ChartObjectId{executionIds[index]}, parentIndex,
                                    std::move(components)};
        if (renderableOpacity) {
            runtimeObject.renderableOpacity = *renderableOpacity;
        }
        runtimeObjects.push_back(std::move(runtimeObject));
        metadataObjects.push_back(CandidateRuntimeObjectMetadata{
            index, entity.identity, entity.parent, executionIds[index], entity.requirements,
            renderableOpacity});
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return CandidateRuntimeArtifactResult{std::nullopt, std::move(diagnostics)};
    }

    std::vector<std::uint8_t> visitState(runtimeObjects.size(), 0);
    for (std::size_t start = 0; start < runtimeObjects.size(); ++start) {
        if (visitState[start] != 0) {
            continue;
        }
        std::vector<std::size_t> chain;
        auto current = start;
        bool cycle = false;
        while (visitState[current] == 0) {
            visitState[current] = 1;
            chain.push_back(current);
            const auto parent = runtimeObjects[current].parentIndex;
            if (!parent) {
                break;
            }
            if (*parent >= runtimeObjects.size() || *parent == current ||
                visitState[*parent] == 1) {
                cycle = true;
                break;
            }
            if (visitState[*parent] == 2) {
                break;
            }
            current = *parent;
        }
        if (cycle) {
            addError(diagnostics, "candidate.lowering.parent_cycle",
                     "Candidate entity parents contain a cycle", "$/entities/parent");
            break;
        }
        for (const auto index : chain) {
            visitState[index] = 2;
        }
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return CandidateRuntimeArtifactResult{std::nullopt, std::move(diagnostics)};
    }

    std::vector<std::size_t> order(runtimeObjects.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        return runtimeObjects[left].id < runtimeObjects[right].id;
    });
    std::vector<std::size_t> remapped(order.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        remapped[order[index]] = index;
    }
    std::vector<RuntimeObject> sortedObjects;
    std::vector<CandidateRuntimeObjectMetadata> sortedMetadata;
    sortedObjects.reserve(order.size());
    sortedMetadata.reserve(order.size());
    for (const auto oldIndex : order) {
        auto object = std::move(runtimeObjects[oldIndex]);
        auto metadata = std::move(metadataObjects[oldIndex]);
        if (object.parentIndex) {
            object.parentIndex = remapped[*object.parentIndex];
        }
        metadata.runtimeIndex = sortedObjects.size();
        sortedObjects.push_back(std::move(object));
        sortedMetadata.push_back(std::move(metadata));
    }
    runtimeObjects = std::move(sortedObjects);
    metadataObjects = std::move(sortedMetadata);

    CandidateRuntimeMetadata metadata;
    metadata.semanticIdentity = *semanticIdentity;
    metadata.resourceClosure = chart.resourceClosure;
    metadata.objects = std::move(metadataObjects);

    ChartRuntime runtime{chart.chartId,
                         std::move(*timingMap),
                         chart.defaultCamera,
                         {},
                         std::move(runtimeObjects),
                         5,
                         chart.mainMusic};
    diagnostics.sortDeterministically();
    return CandidateRuntimeArtifactResult{
        CandidateRuntimeArtifact{std::move(runtime), std::move(metadata)}, std::move(diagnostics)};
}

void writeU32(detail::Sha256& hash, std::uint32_t value) noexcept {
    std::array<std::byte, 4> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(value >> (index * 8U));
    }
    hash.update(bytes);
}

auto assembleCandidatePreparedSemanticIdentity(
    const packed::PackedSemanticIdentity& semanticIdentity,
    std::span<const PreparedResourceIdentityComponent> resourceIdentities)
    -> core::Result<CanonicalContentIdentity> {
    auto sorted = std::vector<PreparedResourceIdentityComponent>{resourceIdentities.begin(),
                                                                 resourceIdentities.end()};
    std::ranges::sort(sorted, {}, [](const PreparedResourceIdentityComponent& item) {
        return item.assetId.value;
    });
    if (sorted.size() > std::numeric_limits<std::uint32_t>::max()) {
        return core::unexpected(core::Error{"candidate.identity.resource_count",
                                            "Candidate resource identity count exceeds u32"});
    }
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        if (sorted[index].assetId.value.empty() ||
            sorted[index].assetId.value.size() > std::numeric_limits<std::uint32_t>::max()) {
            return core::unexpected(
                core::Error{"candidate.identity.resource_invalid",
                            "Candidate resource asset ID is empty or too long"});
        }
        if (index > 0 && sorted[index].assetId.value == sorted[index - 1].assetId.value) {
            return core::unexpected(
                core::Error{"candidate.identity.resource_duplicate",
                            "Candidate prepared identity received duplicate resource asset IDs"});
        }
    }

    detail::Sha256 hash;
    static constexpr char domain[] = "cuexis.prepared-semantic.v5.candidate.1";
    hash.update(std::as_bytes(std::span{domain, sizeof(domain)}));
    writeU32(hash, 1U);
    hash.update(std::as_bytes(std::span{semanticIdentity}));
    writeU32(hash, static_cast<std::uint32_t>(sorted.size()));
    for (const auto& item : sorted) {
        writeU32(hash, static_cast<std::uint32_t>(item.assetId.value.size()));
        hash.update(std::as_bytes(std::span{item.assetId.value.data(), item.assetId.value.size()}));
        hash.update(std::as_bytes(std::span{item.identity.sha256}));
    }
    return CanonicalContentIdentity{hash.finish()};
}

} // namespace cuexis::chart
