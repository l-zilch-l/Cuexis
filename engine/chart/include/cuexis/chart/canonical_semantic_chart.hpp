#pragma once

// Candidate Chart v5 semantic model. This header intentionally contains no
// source-format or JSON types; CXT and Packed adapters target these values.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/rational_beat.hpp>

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cuexis::chart {

struct SemanticIdentityStep final {
    std::string nodeId;
    // Non-indexed emit steps use zero. Indexed steps use repeat index + 1.
    std::uint32_t iterationIndexPlusOne{};

    friend bool operator==(const SemanticIdentityStep&, const SemanticIdentityStep&) = default;
};

struct ExplicitEntityIdentity final {
    ChartObjectId objectId;

    friend auto operator<=>(const ExplicitEntityIdentity&, const ExplicitEntityIdentity&) = default;
};

struct GeneratedEntityIdentity final {
    ChartId chartId;
    std::string bindingId;
    std::string moduleId;
    std::string exportId;
    std::vector<SemanticIdentityStep> path;

    friend bool operator==(const GeneratedEntityIdentity&,
                           const GeneratedEntityIdentity&) = default;
};

using CanonicalEntityIdentity = std::variant<ExplicitEntityIdentity, GeneratedEntityIdentity>;

struct TypedReference final {
    std::string domain;
    std::string id;

    friend auto operator<=>(const TypedReference&, const TypedReference&) = default;
};

enum class CanonicalRequirementKind : std::uint8_t {
    Tap = 1,
};

enum class CanonicalIntervalKind : std::uint8_t {
    Point = 0,
    HalfOpenRange = 1,
};

struct CanonicalRequirementInterval final {
    CanonicalIntervalKind kind{CanonicalIntervalKind::Point};
    RationalBeat startBeat{RationalBeat::zero()};
    std::optional<RationalBeat> endBeat;

    friend bool operator==(const CanonicalRequirementInterval&,
                           const CanonicalRequirementInterval&) = default;
};

struct LaneConstraint final {
    std::uint32_t lane{};

    friend auto operator<=>(const LaneConstraint&, const LaneConstraint&) = default;
};

using CanonicalConstraint = std::variant<LaneConstraint>;

struct CanonicalRequirement final {
    std::string localId;
    CanonicalRequirementKind kind{CanonicalRequirementKind::Tap};
    CanonicalRequirementInterval interval;
    TypedReference judgementDomain;
    TypedReference requiredAction;
    std::vector<CanonicalConstraint> constraints;
    // Foundation revision 1 requires this to be empty. The vector is typed so
    // later revisions can add an explicit effect contract without opaque data.
    std::vector<TypedReference> effects;

    friend bool operator==(const CanonicalRequirement&, const CanonicalRequirement&) = default;
};

struct CanonicalTransform final {
    core::Vec3 position{};
    core::Quat rotation{};
    core::Vec3 scale{1.0F, 1.0F, 1.0F};

    friend bool operator==(const CanonicalTransform&, const CanonicalTransform&) = default;
};

struct CanonicalRenderable final {
    AssetId mesh;
    AssetId material;
    std::uint8_t alpha{255};

    friend bool operator==(const CanonicalRenderable&, const CanonicalRenderable&) = default;
};

struct CanonicalCamera final {
    std::string type{"perspective"};
    double fovY{60.0};
    double nearPlane{0.1};
    double farPlane{1000.0};

    friend bool operator==(const CanonicalCamera&, const CanonicalCamera&) = default;
};

using CanonicalComponent = std::variant<CanonicalTransform, CanonicalRenderable, CanonicalCamera>;

enum class CanonicalResourceUseKind : std::uint8_t {
    MainMusic,
    RenderableMesh,
    RenderableMaterial,
};

struct CanonicalResourceUse final {
    AssetId assetId;
    CanonicalResourceUseKind use{CanonicalResourceUseKind::MainMusic};

    friend auto operator<=>(const CanonicalResourceUse&, const CanonicalResourceUse&) = default;
};

struct CanonicalResourceClosure final {
    std::vector<CanonicalResourceUse> resources;

    [[nodiscard]] auto assetIds() const -> std::vector<AssetId> {
        std::vector<AssetId> result;
        result.reserve(resources.size());
        for (const auto& resource : resources) {
            result.push_back(resource.assetId);
        }
        return result;
    }

    friend bool operator==(const CanonicalResourceClosure&,
                           const CanonicalResourceClosure&) = default;
};

struct CanonicalEntity final {
    CanonicalEntityIdentity identity;
    std::optional<CanonicalEntityIdentity> parent;
    std::vector<CanonicalComponent> components;
    std::vector<CanonicalRequirement> requirements;

    // Packed candidate component bits: Transform=0, Renderable=1, Requirement=2,
    // Camera=4. Requirement presence is represented independently from visual components.
    [[nodiscard]] auto componentMask() const noexcept -> std::uint64_t {
        std::uint64_t mask = requirements.empty() ? 0U : (std::uint64_t{1} << 2U);
        for (const auto& component : components) {
            if (std::holds_alternative<CanonicalTransform>(component)) {
                mask |= std::uint64_t{1} << 0U;
            } else if (std::holds_alternative<CanonicalRenderable>(component)) {
                mask |= std::uint64_t{1} << 1U;
            } else if (std::holds_alternative<CanonicalCamera>(component)) {
                mask |= std::uint64_t{1} << 4U;
            }
        }
        return mask;
    }

    friend bool operator==(const CanonicalEntity&, const CanonicalEntity&) = default;
};

struct CanonicalFeature final {
    std::string id;
    std::uint32_t version{1};

    friend auto operator<=>(const CanonicalFeature&, const CanonicalFeature&) = default;
};

struct CanonicalSemanticChart final {
    ChartId chartId;
    std::optional<AssetId> mainMusic;
    std::vector<CanonicalFeature> features;
    ChartTiming timing;
    CameraData defaultCamera;
    CanonicalResourceClosure resourceClosure;
    std::vector<CanonicalEntity> entities;
};

[[nodiscard]] inline auto isGeneratedIdentity(const CanonicalEntityIdentity& identity) noexcept
    -> bool {
    return std::holds_alternative<GeneratedEntityIdentity>(identity);
}

[[nodiscard]] inline auto isExplicitIdentity(const CanonicalEntityIdentity& identity) noexcept
    -> bool {
    return std::holds_alternative<ExplicitEntityIdentity>(identity);
}

} // namespace cuexis::chart
