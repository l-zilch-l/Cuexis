#include <cuexis/animation/animation_compiler.hpp>

#include <cuexis/animation/animation_diagnostics.hpp>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace cuexis::animation {
namespace {

[[nodiscard]] auto fieldPathOrFallback(const std::string& fieldPath) -> std::string {
    if (fieldPath.empty()) {
        return std::string{fallbackFieldPath};
    }
    return fieldPath;
}

void attachIdentityContext(core::Diagnostic& diagnostic,
                           const chart::AnimationRecordIdentity& identity) {
    std::visit(
        [&](const auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, std::string>) {
                diagnostic.withContext(std::string{contextChartLocalId}, value);
            } else {
                diagnostic.withContext(std::string{contextObjectId}, value.objectId);
                diagnostic.withContext(std::string{contextBindingId}, value.bindingId);
                diagnostic.withContext(std::string{contextTemplateId}, value.templateId);
                diagnostic.withContext(std::string{contextRecordKind},
                                       std::string{generatedRecordKindName(value.recordKind)});
            }
        },
        identity);
}

void addIdentityError(core::Diagnostics& diagnostics, std::string_view code, std::string message,
                      const chart::AnimationRecordIdentity& identity, std::string fieldPath) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{code},
                                       std::move(message), std::move(fieldPath)};
    attachIdentityContext(diagnostic, identity);
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto clipFieldPath(const chart::AnimationProgramClip& clip) -> std::string {
    return fieldPathOrFallback(clip.clip.fieldPath);
}

} // namespace

AnimationCompileResult::AnimationCompileResult()
    : diagnostics(maxDiagnostics, core::Diagnostic{core::DiagnosticSeverity::Error,
                                                   std::string{diagnosticLimitExceeded},
                                                   "Animation diagnostic limit was reached",
                                                   std::string{fallbackFieldPath}}) {}

auto AnimationCompiler::compile(chart::AnimationProgramInput input) -> AnimationCompileResult {
    AnimationCompileResult result;
    AnimationProgram program;
    program.clips_.reserve(input.clips.size());
    program.objects_.reserve(input.objects.size());

    for (auto& clip : input.clips) {
        if (program.clipIndex_.contains(clip.identity)) {
            addIdentityError(result.diagnostics, diagnosticIdentityDuplicate,
                             "Animation clip identity is duplicated", clip.identity,
                             clipFieldPath(clip));
            continue;
        }
        const auto index = program.clips_.size();
        program.clipIndex_.emplace(clip.identity, index);
        program.clips_.push_back(std::move(clip));
    }

    for (auto& object : input.objects) {
        const auto objectIndex = program.objects_.size();
        for (std::size_t layerIndex = 0; layerIndex < object.layers.size(); ++layerIndex) {
            const auto& layer = object.layers[layerIndex];
            for (std::size_t groupIndex = 0; groupIndex < layer.blendGroups.size(); ++groupIndex) {
                const auto& group = layer.blendGroups[groupIndex];
                for (std::size_t instanceIndex = 0; instanceIndex < group.instances.size();
                     ++instanceIndex) {
                    const auto& instance = group.instances[instanceIndex];
                    if (program.findClip(instance.clipIdentity) == nullptr) {
                        addIdentityError(result.diagnostics, diagnosticClipMissing,
                                         "Clip instance refers to an unknown clip identity",
                                         instance.clipIdentity, std::string{fallbackFieldPath});
                    }
                    if (program.instanceIndex_.contains(instance.identity)) {
                        addIdentityError(result.diagnostics, diagnosticIdentityDuplicate,
                                         "Animation instance identity is duplicated",
                                         instance.identity, std::string{fallbackFieldPath});
                        continue;
                    }
                    program.instanceIndex_.emplace(
                        instance.identity, AnimationProgram::InstanceLocation{
                                               objectIndex, layerIndex, groupIndex, instanceIndex});
                }
            }
        }
        program.objects_.push_back(std::move(object));
    }

    result.diagnostics.sortDeterministically();
    if (!result.diagnostics.hasErrors()) {
        result.program = std::move(program);
    }
    return result;
}

} // namespace cuexis::animation
