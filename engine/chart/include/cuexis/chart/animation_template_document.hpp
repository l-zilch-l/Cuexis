#pragma once

#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cuexis::chart {

struct AnimationTemplateApplication final {
    AnimationBlendMode blendMode{AnimationBlendMode::Override};
    AnimationIterations iterations;
    AnimationFillMode fillMode{AnimationFillMode::None};
};

struct AnimationTemplateDocument final {
    std::string templateId;
    std::optional<std::string> name;
    AnimationTemplateApplication application;
    AnimationClip clip;
    std::vector<RequiredExtension> requiredExtensions;
    OpaqueJson extensions;
    OpaqueJson canonicalSource;
};

struct AnimationTemplateResult final {
    std::optional<AnimationTemplateDocument> document;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return document.has_value() && !diagnostics.hasErrors();
    }
};

} // namespace cuexis::chart
