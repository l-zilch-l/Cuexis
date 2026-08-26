#pragma once

// Compiles owning AnimationProgramInput into an owning, seekable AnimationProgram.
// The compiled program does not borrow Chart resolver artifacts, JSON text, or CXC packages.

#include <cuexis/animation/animation_program.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <optional>

namespace cuexis::animation {

struct AnimationCompileResult final {
    AnimationCompileResult();

    std::optional<AnimationProgram> program;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return program.has_value() && !diagnostics.hasErrors();
    }
};

class AnimationCompiler final {
  public:
    [[nodiscard]] static auto compile(chart::AnimationProgramInput input) -> AnimationCompileResult;
};

} // namespace cuexis::animation
