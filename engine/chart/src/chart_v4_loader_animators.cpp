#include "chart_v4_loader_helpers_internal.hpp"

#include "chart_v4_animation_reader_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::chart::loader_internal {
namespace {

void readAnimatorFromComponents(const json::Reader& componentsReader, AnimatorOwnerKind ownerKind,
                                std::string ownerId, const ChartLimits& limits,
                                core::Diagnostics& diagnostics,
                                std::vector<ParameterUse>& parameterUses,
                                std::vector<AnimatorSource>& animators) {
    const auto animatorReader = componentsReader.optionalField("cuexis.animator");
    if (!animatorReader) {
        return;
    }
    auto component =
        detail::readAnimatorComponent(*animatorReader, limits, diagnostics, parameterUses);
    if (component) {
        animators.push_back(AnimatorSource{ownerKind, std::move(ownerId), std::move(*component),
                                           std::string{animatorReader->fieldPath()}});
    }
}

void readAnimatorFromPatches(const json::Reader& patchesReader, AnimatorOwnerKind ownerKind,
                             const std::string& ownerId, const ChartLimits& limits,
                             core::Diagnostics& diagnostics,
                             std::vector<ParameterUse>& parameterUses,
                             std::vector<AnimatorSource>& animators) {
    const auto* patches = patchesReader.readArray();
    if (patches == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < patches->size(); ++index) {
        const auto patchReader = patchesReader.element(index);
        if (!patchReader || patchReader->readObject() == nullptr) {
            continue;
        }
        const auto pathReader = patchReader->optionalField("path");
        const auto operationReader = patchReader->optionalField("op");
        const auto path = pathReader ? pathReader->readString() : std::nullopt;
        const auto operation = operationReader ? operationReader->readString() : std::nullopt;
        if (!path) {
            continue;
        }
        if (path->starts_with("/components/cuexis.animator/") ||
            (*path != "/components/cuexis.animator" &&
             path->starts_with("/components/cuexis.animator"))) {
            detail::addV4Error(diagnostics, "chart.patch.path_unsupported",
                               "Animator patch must replace the entire component",
                               std::string{pathReader->fieldPath()});
            continue;
        }
        if (*path != "/components/cuexis.animator" || !operation || *operation == "remove") {
            continue;
        }
        const auto valueReader = patchReader->optionalField("value");
        if (!valueReader) {
            continue;
        }
        auto component =
            detail::readAnimatorComponent(*valueReader, limits, diagnostics, parameterUses);
        if (component) {
            animators.push_back(AnimatorSource{ownerKind, ownerId, std::move(*component),
                                               std::string{valueReader->fieldPath()}});
        }
    }
}

} // namespace

[[nodiscard]] auto readAnimators(const json::Reader& root, const ChartLimits& limits,
                                 core::Diagnostics& diagnostics,
                                 std::vector<ParameterUse>& parameterUses)
    -> std::vector<AnimatorSource> {
    std::vector<AnimatorSource> result;
    const auto readOwners = [&](std::string_view field, AnimatorOwnerKind ownerKind) {
        const auto ownersReader = root.requiredField(field);
        const auto* owners = ownersReader ? ownersReader->readArray() : nullptr;
        if (owners == nullptr) {
            return;
        }
        for (std::size_t index = 0; index < owners->size(); ++index) {
            const auto ownerReader = ownersReader->element(index);
            if (!ownerReader || ownerReader->readObject() == nullptr) {
                continue;
            }
            const auto idReader = ownerReader->optionalField("id");
            const auto id = idReader ? idReader->readString() : std::nullopt;
            if (!id) {
                continue;
            }
            if (ownerKind == AnimatorOwnerKind::Template) {
                if (const auto prototype = ownerReader->optionalField("prototype")) {
                    if (const auto components = prototype->optionalField("components")) {
                        readAnimatorFromComponents(*components, ownerKind, std::string{*id}, limits,
                                                   diagnostics, parameterUses, result);
                    }
                }
                if (const auto patches = ownerReader->optionalField("patch")) {
                    readAnimatorFromPatches(*patches, ownerKind, std::string{*id}, limits,
                                            diagnostics, parameterUses, result);
                }
            } else {
                if (const auto components = ownerReader->optionalField("components")) {
                    readAnimatorFromComponents(*components, ownerKind, std::string{*id}, limits,
                                               diagnostics, parameterUses, result);
                }
                if (const auto patches = ownerReader->optionalField("overrides")) {
                    readAnimatorFromPatches(*patches, ownerKind, std::string{*id}, limits,
                                            diagnostics, parameterUses, result);
                }
            }
        }
    };
    readOwners("templates", AnimatorOwnerKind::Template);
    readOwners("objects", AnimatorOwnerKind::Object);
    std::ranges::sort(result, [](const AnimatorSource& left, const AnimatorSource& right) {
        if (left.ownerKind != right.ownerKind) {
            return left.ownerKind < right.ownerKind;
        }
        if (left.ownerId != right.ownerId) {
            return left.ownerId < right.ownerId;
        }
        return left.fieldPath < right.fieldPath;
    });
    return result;
}

void validateAnimationReferences(const std::vector<AnimationTemplateImport>& imports,
                                 const std::vector<AnimationClip>& clips,
                                 const std::vector<AnimatorSource>& animators,
                                 core::Diagnostics& diagnostics) {
    std::set<std::string, std::less<>> importIds;
    std::set<std::string, std::less<>> clipIds;
    for (const auto& item : imports) {
        importIds.emplace(item.id);
    }
    for (const auto& clip : clips) {
        clipIds.emplace(clip.id);
    }
    for (const auto& animator : animators) {
        for (const auto& binding : animator.component.templateBindings) {
            if (!importIds.contains(binding.templateId)) {
                detail::addV4Error(diagnostics, "chart.animation.template_reference_missing",
                                   "Template binding refers to an unknown CXT import",
                                   binding.fieldPath + "/template");
            }
        }
        for (const auto& layer : animator.component.layers) {
            for (const auto& group : layer.blendGroups) {
                for (const auto& instance : group.instances) {
                    if (!clipIds.contains(instance.clipId)) {
                        detail::addV4Error(diagnostics, "chart.animation.reference_missing",
                                           "Clip instance refers to an unknown AnimationClip",
                                           instance.fieldPath + "/clip");
                    }
                }
            }
        }
    }
}

} // namespace cuexis::chart::loader_internal
