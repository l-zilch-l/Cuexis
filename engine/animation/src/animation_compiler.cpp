#include <cuexis/animation/animation_compiler.hpp>

#include <cuexis/animation/animation_diagnostics.hpp>
#include <cuexis/world/property.hpp>

#include <cstddef>
#include <limits>
#include <optional>
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

[[nodiscard]] auto preferredFieldPath(const std::string& preferred, const std::string& fallback)
    -> std::string {
    if (!preferred.empty()) {
        return preferred;
    }
    return fieldPathOrFallback(fallback);
}

[[nodiscard]] auto checkedSum(std::size_t left, std::size_t right) -> std::optional<std::size_t> {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

[[nodiscard]] auto checkedAccumulate(std::size_t& total, std::size_t count, std::size_t limit)
    -> bool {
    if (count > limit || total > limit - count) {
        return false;
    }
    total += count;
    return true;
}

void addLimitError(core::Diagnostics& diagnostics, std::string_view code, std::string message,
                   std::string fieldPath) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::string{code},
                                     std::move(message), fieldPathOrFallback(fieldPath)});
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

void countGenerated(core::Diagnostics& diagnostics, std::size_t& generated,
                    const chart::ChartLimits& limits,
                    const chart::AnimationRecordIdentity& identity, const std::string& fieldPath) {
    if (!std::holds_alternative<chart::GeneratedAnimationIdentity>(identity)) {
        return;
    }
    if (checkedAccumulate(generated, 1, limits.maxGeneratedAnimationRecords)) {
        return;
    }
    auto diagnostic =
        core::Diagnostic{core::DiagnosticSeverity::Error, std::string{diagnosticGeneratedLimit},
                         "Generated animation record count exceeds the configured limit",
                         fieldPathOrFallback(fieldPath)};
    attachIdentityContext(diagnostic, identity);
    diagnostics.add(std::move(diagnostic));
}

void validateCompiledBudgets(const chart::AnimationProgramInput& input,
                             const chart::ChartLimits& limits, std::size_t maxWritesPerFrame,
                             core::Diagnostics& diagnostics) {
    if (input.clips.size() > limits.maxAnimationClips) {
        addLimitError(diagnostics, diagnosticGeneratedLimit,
                      "Animation clip count exceeds the configured limit",
                      input.clips[limits.maxAnimationClips].clip.fieldPath);
    }
    if (input.objects.size() > limits.maxObjects) {
        addLimitError(diagnostics, diagnosticGeneratedLimit,
                      "Animation object count exceeds the configured limit",
                      std::string{fallbackFieldPath});
    }
    if (!world::animationWriteBudgetFits(input.objects.size(), maxWritesPerFrame)) {
        addLimitError(diagnostics, diagnosticWriteLimit,
                      "Animation program exceeds the per-frame Property Write limit",
                      std::string{fallbackFieldPath});
    }

    std::size_t totalTracks = 0;
    std::size_t totalSegmentsAndSteps = 0;
    std::size_t generated = 0;
    for (const auto& clip : input.clips) {
        countGenerated(diagnostics, generated, limits, clip.identity, clip.clip.fieldPath);
        const auto trackCount = checkedSum(clip.clip.tracks.size(), clip.clip.stepTracks.size());
        if (!trackCount || *trackCount > limits.maxAnimationTracksPerClip) {
            addLimitError(diagnostics, diagnosticGeneratedLimit,
                          "Animation track count exceeds the configured per-Clip limit",
                          clip.clip.fieldPath);
        }
        if (!trackCount ||
            !checkedAccumulate(totalTracks, *trackCount, limits.maxAnimationTracks)) {
            addLimitError(diagnostics, diagnosticGeneratedLimit,
                          "Total animation track count exceeds the configured limit",
                          clip.clip.fieldPath);
        }
        for (const auto& track : clip.clip.tracks) {
            if (track.segments.size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
                addLimitError(diagnostics, diagnosticGeneratedLimit,
                              "Animation segment count exceeds the configured per-Track limit",
                              preferredFieldPath(track.fieldPath, clip.clip.fieldPath));
            }
            if (!checkedAccumulate(totalSegmentsAndSteps, track.segments.size(),
                                   limits.maxAnimationSegmentsAndSteps)) {
                addLimitError(diagnostics, diagnosticGeneratedLimit,
                              "Total animation segment and step count exceeds the configured limit",
                              preferredFieldPath(track.fieldPath, clip.clip.fieldPath));
            }
        }
        for (const auto& track : clip.clip.stepTracks) {
            if (track.steps.size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
                addLimitError(diagnostics, diagnosticGeneratedLimit,
                              "Animation step count exceeds the configured per-Track limit",
                              preferredFieldPath(track.fieldPath, clip.clip.fieldPath));
            }
            if (!checkedAccumulate(totalSegmentsAndSteps, track.steps.size(),
                                   limits.maxAnimationSegmentsAndSteps)) {
                addLimitError(diagnostics, diagnosticGeneratedLimit,
                              "Total animation segment and step count exceeds the configured limit",
                              preferredFieldPath(track.fieldPath, clip.clip.fieldPath));
            }
        }
    }

    for (const auto& object : input.objects) {
        if (object.layers.size() > limits.maxAnimationLayersPerAnimator) {
            addLimitError(diagnostics, diagnosticGeneratedLimit,
                          "Animation layer count exceeds the configured per-Animator limit",
                          std::string{fallbackFieldPath});
        }
        for (const auto& layer : object.layers) {
            countGenerated(diagnostics, generated, limits, layer.identity, {});
            const auto layerMask = checkedSum(layer.propertyMask.properties.size(),
                                              layer.propertyMask.prefixes.size());
            if (!layerMask || *layerMask > limits.maxAnimationMaskEntries) {
                addLimitError(diagnostics, diagnosticGeneratedLimit,
                              "Animation mask entry count exceeds the configured limit",
                              std::string{fallbackFieldPath});
            }
            if (layer.blendGroups.size() > limits.maxBlendGroupsPerLayer) {
                addLimitError(diagnostics, diagnosticGeneratedLimit,
                              "Blend group count exceeds the configured per-Layer limit",
                              std::string{fallbackFieldPath});
            }
            for (const auto& group : layer.blendGroups) {
                countGenerated(diagnostics, generated, limits, group.identity, {});
                if (group.instances.size() > limits.maxClipInstancesPerBlendGroup) {
                    addLimitError(diagnostics, diagnosticGeneratedLimit,
                                  "Clip instance count exceeds the configured per-BlendGroup limit",
                                  std::string{fallbackFieldPath});
                }
                for (const auto& instance : group.instances) {
                    countGenerated(diagnostics, generated, limits, instance.identity, {});
                    const auto instanceMask = checkedSum(instance.propertyMask.properties.size(),
                                                         instance.propertyMask.prefixes.size());
                    if (!instanceMask || *instanceMask > limits.maxAnimationMaskEntries) {
                        addLimitError(diagnostics, diagnosticGeneratedLimit,
                                      "Animation mask entry count exceeds the configured limit",
                                      std::string{fallbackFieldPath});
                    }
                }
            }
        }
    }
}

} // namespace

AnimationCompileResult::AnimationCompileResult()
    : diagnostics(maxDiagnostics, core::Diagnostic{core::DiagnosticSeverity::Error,
                                                   std::string{diagnosticLimitExceeded},
                                                   "Animation diagnostic limit was reached",
                                                   std::string{fallbackFieldPath}}) {}

auto AnimationCompiler::compile(chart::AnimationProgramInput input) -> AnimationCompileResult {
    return compile(std::move(input), chart::ChartLimits{}, world::maxPropertyWritesPerFrame);
}

auto AnimationCompiler::compile(chart::AnimationProgramInput input,
                                const chart::ChartLimits& limits) -> AnimationCompileResult {
    return compile(std::move(input), limits, world::maxPropertyWritesPerFrame);
}

auto AnimationCompiler::compile(chart::AnimationProgramInput input,
                                const chart::ChartLimits& limits, std::size_t maxWritesPerFrame)
    -> AnimationCompileResult {
    AnimationCompileResult result;
    validateCompiledBudgets(input, limits, maxWritesPerFrame, result.diagnostics);
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
