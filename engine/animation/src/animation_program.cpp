#include <cuexis/animation/animation_program.hpp>

#include <string_view>

namespace cuexis::animation {

auto generatedRecordKindName(chart::GeneratedRecordKind kind) noexcept -> std::string_view {
    switch (kind) {
    case chart::GeneratedRecordKind::Clip:
        return "clip";
    case chart::GeneratedRecordKind::Layer:
        return "layer";
    case chart::GeneratedRecordKind::BlendGroup:
        return "blend_group";
    case chart::GeneratedRecordKind::ClipInstance:
        return "clip_instance";
    }
    return "unknown";
}

auto AnimationProgram::findClip(const chart::AnimationRecordIdentity& identity) const noexcept
    -> const chart::AnimationProgramClip* {
    const auto found = clipIndex_.find(identity);
    if (found == clipIndex_.end()) {
        return nullptr;
    }
    return &clips_[found->second];
}

auto AnimationProgram::findInstance(const chart::AnimationRecordIdentity& identity) const noexcept
    -> const chart::ResolvedClipInstance* {
    const auto found = instanceIndex_.find(identity);
    if (found == instanceIndex_.end()) {
        return nullptr;
    }
    const auto& location = found->second;
    return &objects_[location.objectIndex]
                .layers[location.layerIndex]
                .blendGroups[location.groupIndex]
                .instances[location.instanceIndex];
}

} // namespace cuexis::animation
