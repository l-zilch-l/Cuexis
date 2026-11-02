#include "gameplay_graph_render_internal.hpp"

#include "gameplay_closure.hpp"
#include "source_codes.hpp"

#include <cuexis/judgement/diagnostic.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Diagnostics
//  ---------------------------------------------------------------------------------------------

[[nodiscard]] auto rejection(const DiagnosticTokens& tokens) -> core::Error {
    const Diagnostic diagnostic{
        .code = tokens.code,
        .category = tokens.category,
        .severity = codes::kErrorSeverity,
        //  None of these rejections mutates session state, so none of them faults the session.
        .faulted = false,
        .summary = tokens.summary,
        .context =
            DiagnosticContext{
                .fieldPath = DiagnosticFieldPath{.section = tokens.section, .path = tokens.path},
                .requirement =
                    DiagnosticRequirementRef{.kind = codes::kAbsent, .identity = codes::kAbsent},
                .identity = DiagnosticIdentityComponent{.component = codes::kAbsent,
                                                        .token = codes::kAbsent},
                .budget = nullptr,
                .capabilityId = tokens.capabilityId,
                .remediation = tokens.remediation,
                .rawTime = codes::kAbsent,
            },
    };
    return toError(diagnostic);
}

//  A structurally incomplete declaration: a component its own contract requires is not declared
//  (Spec 9.3, first class).
[[nodiscard]] auto declarationInvalidError(std::string_view code, std::string_view section,
                                           std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kInvalidRelationCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path});
}

//  A declared value that left the domain its declaration requires (Spec 9.3, second class).
[[nodiscard]] auto valueError(std::string_view code, std::string_view section,
                              std::string_view path, std::string_view summary) -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kBudgetExceededCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path});
}

//  A stable capability rejection. Spec 7.2 requires an R-01 to R-11 rejection to name the
//  capability and the replacement path, so both are passed explicitly and neither has a default.
[[nodiscard]] auto capabilityRejection(std::string_view code, std::string_view section,
                                       std::string_view path, std::string_view summary,
                                       std::string_view capabilityId, std::string_view remediation)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kCapabilityDisabledCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path,
                                      .capabilityId = capabilityId,
                                      .remediation = remediation});
}

//  ---------------------------------------------------------------------------------------------
//  Rendering, for the field-level diff
//  ---------------------------------------------------------------------------------------------

[[nodiscard]] auto renderText(std::string_view text) -> std::string {
    return std::string{text};
}

[[nodiscard]] auto
renderRequirementCapacity(const std::optional<MeasuredParameter<std::uint64_t>>& value)
    -> std::string {
    const RequirementCapacityKey key = requirementCapacityKey(value);
    switch (key.state) {
    case RequirementCapacityState::absent:
        return "absent";
    case RequirementCapacityState::pending:
        return "pending";
    case RequirementCapacityState::measured:
        return "measured(" + std::to_string(key.value) + ")";
    }
    return "?";
}

[[nodiscard]] auto renderBool(bool value) -> std::string {
    return value ? std::string{"true"} : std::string{"false"};
}

//  A deterministic rendering of a token list. The separator is a single character that cannot occur
//  in a stable token, so a rendering of "a,b" can never be confused with one of "a" and "b".
[[nodiscard]] auto renderTokens(const std::vector<std::string>& tokens) -> std::string {
    std::string rendered;
    for (const auto& token : tokens) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(token);
    }
    return rendered;
}

[[nodiscard]] auto renderFeatureRefs(const std::vector<FeatureRef>& refs) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(refs.size());
    for (const auto& ref : refs) {
        tokens.push_back(ref.featureId);
    }
    return renderTokens(tokens);
}

[[nodiscard]] auto renderCapabilityRefs(const std::vector<CapabilityRef>& refs) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(refs.size());
    for (const auto& ref : refs) {
        tokens.push_back(ref.capabilityId + "@" + ref.revision);
    }
    return renderTokens(tokens);
}

[[maybe_unused, nodiscard]] auto renderResourceRefs(const std::vector<ResourceRef>& refs)
    -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(refs.size());
    for (const auto& ref : refs) {
        tokens.push_back(ref.resourceId);
    }
    return renderTokens(tokens);
}

[[nodiscard]] auto renderStableIds(const std::vector<StableDeclarationId>& ids) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(ids.size());
    for (const auto& id : ids) {
        tokens.push_back(id.sourceDocumentId + "#" + std::to_string(id.declarationOrdinal));
    }
    return renderTokens(tokens);
}

[[nodiscard]] auto renderEmissionPath(const std::vector<EmissionPathStep>& path) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(path.size());
    for (const auto& step : path) {
        tokens.push_back(step.nodeId + "#" + std::to_string(step.repeatIndex));
    }
    return renderTokens(tokens);
}

//  The canonical rendering of the six-tuple. It is the diff key of a requirement and never an
//  interchange identity: nothing outside this file reads it.
[[nodiscard]] auto renderRequirementIdentity(const RequirementIdentity& identity) -> std::string {
    return identity.chartEntryId + "/" + identity.invocationId + "/" + identity.moduleId + "/" +
           identity.exportId + "/" + renderEmissionPath(identity.emissionPath) + "/" +
           identity.requirementLocalId;
}

[[nodiscard]] auto renderPhaseKind(PhaseKind kind) -> std::string {
    switch (kind) {
    case PhaseKind::tap:
        return "tap";
    case PhaseKind::head:
        return "head";
    case PhaseKind::body:
        return "body";
    case PhaseKind::tail:
        return "tail";
    }
    return "?";
}

[[nodiscard]] auto renderGracePolicy(GraceResolutionPolicy policy) -> std::string {
    switch (policy) {
    case GraceResolutionPolicy::explicitDeclaration:
        return "explicit";
    case GraceResolutionPolicy::inheritedDeclaration:
        return "inherited";
    case GraceResolutionPolicy::defaultDeclaration:
        return "default";
    }
    return "?";
}

[[nodiscard]] auto renderPatternPrimitive(PatternPrimitive primitive) -> std::string {
    switch (primitive) {
    case PatternPrimitive::atom:
        return "atom";
    case PatternPrimitive::sequence:
        return "sequence";
    case PatternPrimitive::choice:
        return "choice";
    case PatternPrimitive::boundedRepeat:
        return "boundedRepeat";
    case PatternPrimitive::skip:
        return "skip";
    case PatternPrimitive::instant:
        return "instant";
    case PatternPrimitive::complement:
        return "complement";
    }
    return "?";
}

//  The declaration tree of a pattern, rendered deterministically. The compiled predicate comparison
//  is the second half; this rendering is the declaration-level evidence.
//
//  The rendering is an iterative pre-order over an explicit stack, so a deeply nested declaration
//  is rendered with the same host stack as a flat one and the rendering never becomes the depth
//  bound the declaration model deliberately does not have. The text is the text a recursive
//  pre-order produced: a node renders its primitive, its atom, its operands in declared order, then
//  its bounds and its unsupported form.
[[nodiscard]] auto renderPatternNode(const PatternNodeDeclaration& root) -> std::string {
    struct Frame final {
        const PatternNodeDeclaration* node{nullptr};
        std::size_t next{0};
    };
    std::string rendered;
    std::vector<Frame> stack;
    stack.push_back(Frame{.node = &root});
    while (!stack.empty()) {
        const PatternNodeDeclaration* node = stack.back().node;
        const std::size_t next = stack.back().next;
        if (next == 0U) {
            rendered.append(renderPatternPrimitive(node->primitive));
            rendered.push_back('(');
            if (!node->atomRef.empty()) {
                rendered.append("atom=");
                rendered.append(node->atomRef);
                rendered.push_back(';');
            }
            if (!node->operands.empty()) {
                rendered.push_back('[');
            }
        }
        if (next < node->operands.size()) {
            if (next != 0U) {
                rendered.push_back(',');
            }
            stack.back().next = next + 1U;
            stack.push_back(Frame{.node = &node->operands[next]});
            continue;
        }
        if (!node->operands.empty()) {
            rendered.push_back(']');
        }
        if (node->repeatBounds.has_value()) {
            rendered.append("bounds=");
            rendered.append(std::to_string(node->repeatBounds->minimum));
            rendered.push_back('.');
            rendered.append(std::to_string(node->repeatBounds->maximum));
        }
        if (node->unsupportedForm.has_value()) {
            rendered.append("unsupported=");
            rendered.append(node->unsupportedForm->declaredToken);
        }
        rendered.push_back(')');
        stack.pop_back();
    }
    return rendered;
}

//  ---------------------------------------------------------------------------------------------
//  Typed comparison of the two borrowed declarations
//  ---------------------------------------------------------------------------------------------

//  TimebaseProfile deliberately has no operator==, and this file does not add one, because the type
//  belongs to S7A-2. The semantic diff compares it member by member instead, which keeps the
//  comparison inside this batch.
[[nodiscard]] auto sameTimebase(const TimebaseProfile& left, const TimebaseProfile& right) -> bool {
    return left.profileId == right.profileId && left.unitToken == right.unitToken &&
           left.tickScale == right.tickScale && left.originBeat == right.originBeat &&
           left.initialTempo == right.initialTempo && left.tempoSections == right.tempoSections &&
           left.stopSections == right.stopSections;
}

[[nodiscard]] auto renderTimebase(const TimebaseProfile& profile) -> std::string {
    std::string rendered{profile.profileId};
    rendered.append("/");
    rendered.append(profile.unitToken);
    rendered.append("/scale=");
    rendered.append(std::to_string(profile.tickScale.numerator()));
    rendered.push_back('/');
    rendered.append(std::to_string(profile.tickScale.denominator()));
    rendered.append("/origin=");
    rendered.append(std::to_string(profile.originBeat.numerator()));
    rendered.push_back('/');
    rendered.append(std::to_string(profile.originBeat.denominator()));
    rendered.append("/tempo=");
    rendered.append(profile.initialTempo.has_value()
                        ? std::to_string(profile.initialTempo->numerator()) + "/" +
                              std::to_string(profile.initialTempo->denominator())
                        : std::string{"absent"});
    rendered.append("/tempoSections=");
    rendered.append(std::to_string(profile.tempoSections.size()));
    rendered.append("/stopSections=");
    rendered.append(std::to_string(profile.stopSections.size()));
    return rendered;
}

[[nodiscard]] auto renderLatePolicy(const LatePolicyParameters& parameters) -> std::string {
    const auto render = [](const MeasuredParameter<TickSpan>& parameter) -> std::string {
        const TickSpan* value = parameter.measuredValue();
        return value == nullptr ? std::string{"pending"} : std::to_string(value->value());
    };
    std::string rendered{"finalizationWatermark="};
    rendered.append(render(parameters.finalizationWatermark));
    rendered.append(",maxQueueHop=");
    rendered.append(render(parameters.maxQueueHop));
    rendered.append(",windowCloseThreshold=");
    rendered.append(render(parameters.windowCloseThreshold));
    rendered.append(",windowOpenThreshold=");
    rendered.append(render(parameters.windowOpenThreshold));
    rendered.append(",policy=");
    rendered.append(parameters.policy.has_value()
                        ? std::to_string(static_cast<unsigned>(*parameters.policy))
                        : std::string{"absent"});
    return rendered;
}

} // namespace cuexis::judgement
