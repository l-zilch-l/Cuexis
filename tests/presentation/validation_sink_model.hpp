#pragma once

// Canonical presentation data, resource location and summary hashing.

#include "validation_sink_canonical.hpp"

namespace cuexis::test_support {
namespace validation_detail {

struct CanonicalPresentationData final {
    playback::PresentationResourceManifest manifest;
    std::vector<playback::PortableResourcePtr> resources;
};

[[nodiscard]] inline auto
canonicalizePresentationData(const playback::PresentationResourceManifest& manifest,
                             std::span<const playback::PortableResourcePtr> resources)
    -> core::Result<CanonicalPresentationData> {
    if (manifest.version != 1) {
        return core::unexpected(referenceError("Presentation manifest version is unsupported"));
    }
    if (manifest.entries.size() > maxManifestEntries ||
        resources.size() != manifest.entries.size()) {
        return core::unexpected(referenceError("Presentation manifest/resource count is invalid"));
    }

    CanonicalPresentationData canonical{
        manifest, std::vector<playback::PortableResourcePtr>(resources.begin(), resources.end())};
    for (std::size_t index = 0; index < canonical.manifest.entries.size(); ++index) {
        const auto& entry = canonical.manifest.entries[index];
        if (!isPortableAssetId(entry.reference.assetId)) {
            return core::unexpected(
                referenceError("Presentation AssetId is not portable", &entry.reference));
        }
        if (index != 0 && !(referenceKey(canonical.manifest.entries[index - 1].reference) <
                            referenceKey(entry.reference))) {
            return core::unexpected(referenceError(
                "Presentation manifest entries are not in canonical order", &entry.reference));
        }
        if (index != 0 &&
            canonical.manifest.entries[index - 1].reference.assetId == entry.reference.assetId) {
            return core::unexpected(referenceError(
                "Presentation manifest contains a duplicate AssetId", &entry.reference));
        }
        const auto& resource = canonical.resources[index];
        if (!resource) {
            return core::unexpected(referenceError("Presentation resource pointer is null"));
        }
        if (resource->reference != entry.reference) {
            return core::unexpected(referenceError(
                "Presentation resource order does not match the manifest", &resource->reference));
        }
    }

    std::uint64_t totalEncodedBytes{};
    std::uint64_t totalResourceDecodedBytes{};
    std::uint64_t manifestBytes = 24;
    for (std::size_t index = 0; index < canonical.manifest.entries.size(); ++index) {
        const auto& entry = canonical.manifest.entries[index];
        const auto& resource = canonical.resources[index];
        if (!resource) {
            return core::unexpected(
                referenceError("Presentation manifest resource is missing", &entry.reference));
        }

        std::uint64_t decodedBytes{};
        playback::PresentationContentIdentity computedIdentity;
        switch (entry.reference.type) {
        case playback::PresentationResourceType::Mesh: {
            const auto* value = std::get_if<playback::PortableMesh>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Mesh ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateMesh(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = meshIdentity(*value);
            if (!entry.dependencies.empty()) {
                return core::unexpected(
                    referenceError("Portable Mesh has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::Texture2D: {
            const auto* value = std::get_if<playback::PortableTexture2D>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Texture2D ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateTexture(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = textureIdentity(*value);
            if (!entry.dependencies.empty()) {
                return core::unexpected(referenceError(
                    "Portable Texture2D has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::UnlitMaterial: {
            const auto* value = std::get_if<playback::PortableUnlitMaterial>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Material ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateMaterial(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = materialIdentity(*value);
            if (value->baseColorTexture) {
                if (entry.dependencies.size() != 1 ||
                    entry.dependencies.front() != *value->baseColorTexture) {
                    return core::unexpected(referenceError(
                        "Portable Material dependency does not match its value", &entry.reference));
                }
                const auto dependency = std::lower_bound(
                    canonical.manifest.entries.begin(), canonical.manifest.entries.end(),
                    *value->baseColorTexture, [](const auto& candidate, const auto& reference) {
                        return referenceKey(candidate.reference) < referenceKey(reference);
                    });
                if (dependency == canonical.manifest.entries.end() ||
                    dependency->reference != *value->baseColorTexture) {
                    return core::unexpected(referenceError(
                        "Portable Material texture dependency is missing", &entry.reference));
                }
            } else if (!entry.dependencies.empty()) {
                return core::unexpected(referenceError(
                    "Portable Material has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::Shader: {
            const auto* value = std::get_if<playback::PortableShader>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Shader ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateShader(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = computePresentationIdentity(resource->value);
            if (!entry.dependencies.empty()) {
                return core::unexpected(referenceError(
                    "Portable Shader has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::ParameterizedMaterial: {
            const auto* value =
                std::get_if<playback::PortableParameterizedMaterial>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Parameterized Material ref has an incompatible value",
                    &entry.reference));
            }
            if (auto valid = validateParameterizedMaterial(entry.reference, *value, decodedBytes);
                !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = computePresentationIdentity(resource->value);
            const auto expected = expectedParameterizedDependencies(*value);
            if (entry.dependencies != expected) {
                return core::unexpected(
                    referenceError("Parameterized Material dependency does not match its value",
                                   &entry.reference));
            }
            for (const auto& dependency : expected) {
                const auto found = std::lower_bound(
                    canonical.manifest.entries.begin(), canonical.manifest.entries.end(),
                    dependency, [](const auto& candidate, const auto& reference) {
                        return referenceKey(candidate.reference) < referenceKey(reference);
                    });
                if (found == canonical.manifest.entries.end() || found->reference != dependency) {
                    return core::unexpected(referenceError(
                        "Parameterized Material dependency is missing", &entry.reference));
                }
            }
            break;
        }
        }

        if (computedIdentity != entry.reference.identity ||
            resource->reference != entry.reference) {
            return core::unexpected(referenceError(
                "Presentation resource semantic identity does not match its reference",
                &entry.reference));
        }
        if (entry.decodedByteCount != decodedBytes || entry.encodedByteCount > maxResourceBytes ||
            decodedBytes > maxResourceBytes) {
            return core::unexpected(
                core::Error{"playback.presentation.resource.budget_exceeded",
                            "Validation Sink resource byte budget is inconsistent"}
                    .withContext("asset_id", entry.reference.assetId)
                    .withContext("resource_type",
                                 std::string{resourceTypeName(entry.reference.type)})
                    .withContext("limit", std::to_string(maxResourceBytes))
                    .withContext("actual", std::to_string(decodedBytes)));
        }
        if (!checkedAdd(totalEncodedBytes, entry.encodedByteCount, totalEncodedBytes) ||
            !checkedAdd(totalResourceDecodedBytes, decodedBytes, totalResourceDecodedBytes)) {
            return core::unexpected(core::Error{"playback.presentation.session.budget_exceeded",
                                                "Validation Sink resource totals overflowed"});
        }

        std::uint64_t entryBytes = 20;
        std::uint64_t refBytes{};
        if (!referenceByteCount(entry.reference, refBytes) ||
            !checkedAdd(entryBytes, refBytes, entryBytes)) {
            return core::unexpected(
                referenceError("Presentation manifest byte count overflowed", &entry.reference));
        }
        for (const auto& dependency : entry.dependencies) {
            if (!referenceByteCount(dependency, refBytes) ||
                !checkedAdd(entryBytes, refBytes, entryBytes)) {
                return core::unexpected(referenceError(
                    "Presentation dependency byte count overflowed", &entry.reference));
            }
        }
        if (!checkedAdd(manifestBytes, entryBytes, manifestBytes)) {
            return core::unexpected(
                referenceError("Presentation manifest total overflowed", &entry.reference));
        }
    }

    std::uint64_t totalDecodedBytes{};
    if (!checkedAdd(totalResourceDecodedBytes, manifestBytes, totalDecodedBytes) ||
        totalEncodedBytes != canonical.manifest.totalEncodedBytes ||
        totalDecodedBytes != canonical.manifest.totalDecodedBytes ||
        totalEncodedBytes > maxSessionBytes || totalDecodedBytes > maxSessionBytes) {
        return core::unexpected(core::Error{"playback.presentation.session.budget_exceeded",
                                            "Validation Sink manifest totals are inconsistent"}
                                    .withContext("limit", std::to_string(maxSessionBytes))
                                    .withContext("actual", std::to_string(totalDecodedBytes)));
    }
    return canonical;
}

struct LocatedResource final {
    const playback::PresentationManifestEntry* entry{};
    const playback::PortableResource* resource{};
};

[[nodiscard]] inline auto
locateResource(const ValidationCandidate& candidate,
               const playback::PresentationResourceRef& reference) noexcept
    -> std::optional<LocatedResource> {
    const auto& manifest = candidate.manifest();
    const auto resources = candidate.resources();
    const auto found = std::lower_bound(manifest.entries.begin(), manifest.entries.end(), reference,
                                        [](const auto& entry, const auto& candidateReference) {
                                            return referenceKey(entry.reference) <
                                                   referenceKey(candidateReference);
                                        });
    if (found == manifest.entries.end() || found->reference != reference) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(found - manifest.entries.begin());
    if (index >= resources.size() || !resources[index] ||
        resources[index]->reference != reference) {
        return std::nullopt;
    }
    return LocatedResource{&*found, resources[index].get()};
}

[[nodiscard]] inline auto finiteMatrix(const float (&matrix)[16]) noexcept -> bool {
    return std::all_of(std::begin(matrix), std::end(matrix),
                       [](float value) { return std::isfinite(value); });
}

struct Point3 final {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] inline auto transformPoint(const float (&matrix)[16], const Point3& point) noexcept
    -> Point3 {
    return Point3{
        static_cast<double>(matrix[0]) * point.x + static_cast<double>(matrix[4]) * point.y +
            static_cast<double>(matrix[8]) * point.z + static_cast<double>(matrix[12]),
        static_cast<double>(matrix[1]) * point.x + static_cast<double>(matrix[5]) * point.y +
            static_cast<double>(matrix[9]) * point.z + static_cast<double>(matrix[13]),
        static_cast<double>(matrix[2]) * point.x + static_cast<double>(matrix[6]) * point.y +
            static_cast<double>(matrix[10]) * point.z + static_cast<double>(matrix[14])};
}

[[nodiscard]] inline auto finitePoint(const Point3& point) noexcept -> bool {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

class SummaryHash final {
  public:
    SummaryHash() noexcept {
        static constexpr char domain[] = "cuexis.validation.summary.v1";
        writeBytes(std::as_bytes(std::span{domain, sizeof(domain)}));
    }

    void writeU8(std::uint8_t value) noexcept {
        value_ ^= value;
        value_ *= 1099511628211ULL;
    }

    void writeU32(std::uint32_t value) noexcept {
        for (std::size_t index = 0; index < 4; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeU64(std::uint64_t value) noexcept {
        for (std::size_t index = 0; index < 8; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeI64(std::int64_t value) noexcept {
        writeU64(std::bit_cast<std::uint64_t>(value));
    }

    void writeBool(bool value) noexcept {
        writeU8(value ? 1U : 0U);
    }

    void writeFloat(float value) noexcept {
        if (value == 0.0F) {
            value = 0.0F;
        }
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeDouble(double value) noexcept {
        if (value == 0.0) {
            value = 0.0;
        }
        writeU64(std::bit_cast<std::uint64_t>(value));
    }

    void writeBytes(std::span<const std::byte> bytes) noexcept {
        for (const auto value : bytes) {
            writeU8(std::to_integer<std::uint8_t>(value));
        }
    }

    void writeString(std::string_view value) noexcept {
        writeU32(static_cast<std::uint32_t>(value.size()));
        writeBytes(std::as_bytes(std::span{value.data(), value.size()}));
    }

    void writeReference(const playback::PresentationResourceRef& reference) noexcept {
        writeU32(static_cast<std::uint32_t>(reference.type));
        writeString(reference.assetId);
        writeBytes(std::as_bytes(std::span{reference.identity.sha256}));
    }

    [[nodiscard]] std::uint64_t value() const noexcept {
        return value_;
    }

  private:
    std::uint64_t value_{14695981039346656037ULL};
};

inline void hashCommand(SummaryHash& hash, const ValidationCommand& command) noexcept {
    hash.writeString(command.objectId);
    for (const auto value : command.worldMatrix) {
        hash.writeFloat(value);
    }
    hash.writeReference(command.mesh);
    hash.writeReference(command.material);
    for (const auto value : command.effectiveColor) {
        hash.writeDouble(value);
    }
    hash.writeU8(static_cast<std::uint8_t>(command.pass));
    hash.writeBool(command.backFaceCulling);
    hash.writeBool(command.depthTest);
    hash.writeBool(command.depthWrite);
    hash.writeBool(command.sourceOverBlend);
    hash.writeDouble(command.depthMeters);
    hash.writeI64(command.transparentDepthKey);
}

[[nodiscard]] inline auto summaryDigest(const ValidationSummary& summary) noexcept
    -> std::uint64_t {
    SummaryHash hash;
    hash.writeU32(summary.version);
    hash.writeU32(summary.viewportWidth);
    hash.writeU32(summary.viewportHeight);
    for (const auto value : summary.clearColor) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.cameraActive);
    for (const auto value : summary.viewMatrix) {
        hash.writeFloat(value);
    }
    for (const auto value : summary.projectionMatrix) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.debugPassEnabled);
    hash.writeU32(static_cast<std::uint32_t>(summary.opaque.size()));
    for (const auto& command : summary.opaque) {
        hashCommand(hash, command);
    }
    hash.writeU32(static_cast<std::uint32_t>(summary.transparent.size()));
    for (const auto& command : summary.transparent) {
        hashCommand(hash, command);
    }
    return hash.value();
}

inline void assignReference(playback::PresentationResourceRef& destination,
                            const playback::PresentationResourceRef& source) {
    destination.type = source.type;
    destination.assetId = source.assetId;
    destination.identity = source.identity;
}

[[nodiscard]] inline auto nextCommand(std::vector<ValidationCommand>& commands, std::size_t& count)
    -> ValidationCommand& {
    if (count == commands.size()) {
        commands.emplace_back();
    }
    return commands[count++];
}

} // namespace validation_detail
} // namespace cuexis::test_support
