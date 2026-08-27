#pragma once

// Typed Stage 4 animation input produced by Chart v4 resolve/lowering.
// Owning value types only; no JSON DOM, CXC archive, CXT text, or provider borrow.

#include <cuexis/chart/chart_v4_document.hpp>

#include <compare>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace cuexis::chart {

enum class GeneratedRecordKind {
    Clip,
    Layer,
    BlendGroup,
    ClipInstance,
};

struct GeneratedAnimationIdentity final {
    std::string objectId;
    std::string bindingId;
    std::string templateId;
    GeneratedRecordKind recordKind{GeneratedRecordKind::Clip};

    auto operator<=>(const GeneratedAnimationIdentity&) const = default;
};

using AnimationRecordIdentity = std::variant<std::string, GeneratedAnimationIdentity>;

struct AnimationProgramClip final {
    AnimationRecordIdentity identity;
    AnimationClip clip;
};

struct ResolvedClipInstance final {
    AnimationRecordIdentity identity;
    AnimationRecordIdentity clipIdentity;
    RationalBeat startBeat;
    RationalBeat durationScale;
    AnimationIterations iterations;
    AnimationFillMode fillMode{AnimationFillMode::None};
    double weight{1.0};
    PropertyMask propertyMask;
};

struct ResolvedBlendGroup final {
    AnimationRecordIdentity identity;
    AnimationBlendMode mode{AnimationBlendMode::Override};
    double weight{1.0};
    std::vector<ResolvedClipInstance> instances;
};

struct ResolvedAnimationLayer final {
    AnimationRecordIdentity identity;
    std::int64_t priority{};
    double weight{1.0};
    PropertyMask propertyMask;
    std::vector<ResolvedBlendGroup> blendGroups;
};

struct ObjectAnimationProgram final {
    ChartObjectId objectId;
    std::vector<ResolvedAnimationLayer> layers;
};

struct AnimationProgramInput final {
    std::vector<AnimationProgramClip> clips;
    std::vector<ObjectAnimationProgram> objects;
};

} // namespace cuexis::chart
