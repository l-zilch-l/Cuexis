#pragma once

// Owning compiled animation program. Lookup uses AnimationRecordIdentity, never AnimationClip::id.

#include <cuexis/chart/animation_program_input.hpp>

#include <cstddef>
#include <map>
#include <string_view>
#include <vector>

namespace cuexis::animation {

[[nodiscard]] auto generatedRecordKindName(chart::GeneratedRecordKind kind) noexcept
    -> std::string_view;

class AnimationProgram final {
  public:
    AnimationProgram() = default;

    [[nodiscard]] auto empty() const noexcept -> bool {
        return clips_.empty() && objects_.empty();
    }

    [[nodiscard]] auto clipCount() const noexcept -> std::size_t {
        return clips_.size();
    }

    [[nodiscard]] auto objectCount() const noexcept -> std::size_t {
        return objects_.size();
    }

    [[nodiscard]] auto clips() const noexcept -> const std::vector<chart::AnimationProgramClip>& {
        return clips_;
    }

    [[nodiscard]] auto objects() const noexcept
        -> const std::vector<chart::ObjectAnimationProgram>& {
        return objects_;
    }

    [[nodiscard]] auto findClip(const chart::AnimationRecordIdentity& identity) const noexcept
        -> const chart::AnimationProgramClip*;

    [[nodiscard]] auto findInstance(const chart::AnimationRecordIdentity& identity) const noexcept
        -> const chart::ResolvedClipInstance*;

  private:
    friend class AnimationCompiler;

    struct InstanceLocation final {
        std::size_t objectIndex{};
        std::size_t layerIndex{};
        std::size_t groupIndex{};
        std::size_t instanceIndex{};
    };

    std::vector<chart::AnimationProgramClip> clips_;
    std::vector<chart::ObjectAnimationProgram> objects_;
    std::map<chart::AnimationRecordIdentity, std::size_t> clipIndex_;
    std::map<chart::AnimationRecordIdentity, InstanceLocation> instanceIndex_;
};

} // namespace cuexis::animation
